/*
 * Gaim buddy notification plugin.
 *
 * Copyright (C) 2000-2001, Eric Warmenhoven (original code)
 * Copyright (C) 2002, Etan Reisner <deryni@eden.rutgers.edu> (rewritten code)
 * Copyright (C) 2003, Christian Hammond (update for changed API)
 * Copyright (C) 2003, Brian Tarricone <bjt23@cornell.edu> (mostly rewritten)
 * Copyright (C) 2003, Mark Doliner (minor cleanup)
 * Copyright (C) 2003, Etan Reisner (largely rewritten again)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * From Etan, 2002:
 *  -Added config dialog
 *  -Added control over notification method
 *  -Added control over when to release notification
 *
 *  -Added option to get notification for chats also
 *  -Cleaned up code
 *  -Added option to notify on click as it's own option
 *   rather then as what happens when on focus isn't clicked
 *  -Added apply button to change the denotification methods for
 *   open conversation windows
 *  -Fixed apply to conversations, count now keeps count across applies
 *  -Fixed(?) memory leak, and in the process fixed some stupidities
 *  -Hit enter when done editing the title string entry box to save it
 *
 * Thanks to Carles Pina i Estany <carles@pinux.info>
 *   for count of new messages option
 *
 * From Brian, 20 July 2003:
 *  -Use new xml prefs
 *  -Better handling of notification states tracking
 *  -Better pref change handling
 *  -Fixed a possible memleak and possible crash (rare)
 *  -Use gtk_window_get_title() rather than gtkwin->title
 *  -Other random fixes and cleanups
 *
 * Etan again, 12 August 2003:
 *  -Better use of the new xml prefs
 *  -Removed all bitmask stuff
 *  -Even better pref change handling
 *  -Removed unnecessary functions
 *  -Reworking of notification/unnotification stuff
 *  -Header file include cleanup
 *  -General code cleanup
 *
 * Etan yet again, 04 April 2004:
 *  -Re-added Urgent option
 *  -Re-added unnotify on focus option (still needs work, as it will only
 *  react to focus-in events when the entry or history widgets are focused)
 */

#include "internal.h"
#include "gtkgaim.h"

#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"

#include "gtkplugin.h"
#include "gtkutils.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define NOTIFY_PLUGIN_ID "gtk-x11-notify"

static GaimPlugin *my_plugin = NULL;

/* notification set/unset */
static int notify(GaimConversation *conv, gboolean increment);
static void notify_win(GaimConvWindow *gaimwin);
static void unnotify(GaimConversation *conv, gboolean reset);
static int unnotify_cb(GtkWidget *widget, gpointer data, GaimConversation *conv);

/* gtk widget callbacks for prefs panel */
static void type_toggle_cb(GtkWidget *widget, gpointer data);
static void method_toggle_cb(GtkWidget *widget, gpointer data);
static void notify_toggle_cb(GtkWidget *widget, gpointer data);
static gboolean options_entry_cb(GtkWidget *widget, GdkEventFocus *event, gpointer data);
static void apply_method();
static void apply_notify();

/* string function */
static void handle_string(GaimConvWindow *window);

/* count function */
static void handle_count(GaimConvWindow *window);

/* urgent function */
static void handle_urgent(GaimConvWindow *window, gboolean add);

/****************************************/
/* Begin doing stuff below this line... */
/****************************************/
static int
count_messages(GaimConvWindow *gaimwin)
{
	gint count = 0;
	GList *convs = NULL;

	for (convs = gaim_conv_window_get_conversations(gaimwin);
	     convs != NULL; convs = convs->next) {
		GaimConversation *conv = convs->data;
		count += GPOINTER_TO_INT(gaim_conversation_get_data(conv, "notify-message-count"));
	}

	return count;
}

static int
notify(GaimConversation *conv, gboolean increment)
{
	GaimConvWindow *gaimwin = NULL;
	gint count;
	gboolean has_focus;

	if (conv == NULL)
		return 0;

	/* We want to remove the notifications, but not reset the counter */
	unnotify(conv, FALSE);

	gaimwin = gaim_conversation_get_window(conv);

	/* If we aren't doing notifications for this type of conversation, return */
	if (((gaim_conversation_get_type(conv) == GAIM_CONV_IM) &&
	     !gaim_prefs_get_bool("/plugins/gtk/X11/notify/type_im")) ||
	    ((gaim_conversation_get_type(conv) == GAIM_CONV_CHAT) &&
	     !gaim_prefs_get_bool("/plugins/gtk/X11/notify/type_chat")))
		return 0;

	g_object_get(G_OBJECT(GAIM_GTK_WINDOW(gaimwin)->window),
	             "has-toplevel-focus", &has_focus, NULL);

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/type_focused") ||
	    !has_focus) {
		if (increment) {
			count = GPOINTER_TO_INT(gaim_conversation_get_data(conv, "notify-message-count"));
			count++;
			gaim_conversation_set_data(conv, "notify-message-count", GINT_TO_POINTER(count));
		}

		notify_win(gaimwin);
	}

	return 0;
}

static void
notify_win(GaimConvWindow *gaimwin)
{
	if (count_messages(gaimwin) <= 0)
		return;

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_count"))
		handle_count(gaimwin);
	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_string"))
		handle_string(gaimwin);
	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_urgent"))
		handle_urgent(gaimwin, TRUE);
}

static void
unnotify(GaimConversation *conv, gboolean reset)
{
	GaimConversation *active_conv = NULL;
	GaimConvWindow *gaimwin = NULL;

	g_return_if_fail(conv != NULL);

	gaimwin = gaim_conversation_get_window(conv);
	active_conv = gaim_conv_window_get_active_conversation(gaimwin);

	/* reset the conversation window title */
	gaim_conversation_autoset_title(active_conv);

	if (reset) {
		/* Only need to actually remove the urgent hinting here, since removing it
		 * just to have it readded in re-notify is an unnecessary couple extra RTs
		 * to the server */
		handle_urgent(gaimwin, FALSE);
		gaim_conversation_set_data(conv, "notify-message-count", GINT_TO_POINTER(0));
	}

	return;
}

static int
unnotify_cb(GtkWidget *widget, gpointer data, GaimConversation *conv)
{
	if (GPOINTER_TO_INT(gaim_conversation_get_data(conv, "notify-message-count")) != 0)
		unnotify(conv, TRUE);

	return 0;
}

static gboolean
im_recv_im(GaimAccount *account, char *sender, char *message,
           int *flags)
{
	GaimConversation *conv = gaim_find_conversation_with_account(sender, account);

	notify(conv, TRUE);

	return FALSE;
}

static gboolean
chat_recv_im(GaimAccount *account, char *sender, char *message,
             GaimConversation *conv)
{
	if (gaim_conv_chat_is_user_ignored(GAIM_CONV_CHAT(conv), sender))
		return FALSE;

	notify(conv, TRUE);

	return FALSE;
}

static void
im_sent_im(GaimAccount *account, char *receiver, const char *message) {
	GaimConversation *conv = NULL;

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_send")) {
		conv = gaim_find_conversation_with_account(receiver, account);
		unnotify(conv, TRUE);
	}
}

static void
chat_sent_im(GaimAccount *account, const char *message, int id)
{
	GaimConversation *conv = NULL;

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_send")) {
		conv = gaim_find_chat(gaim_account_get_connection(account), id);
		unnotify(conv, TRUE);
	}
}

static int
attach_signals(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv = NULL;
	GaimGtkWindow *gtkwin = NULL;
	GSList *window_ids = NULL, *imhtml_ids = NULL, *entry_ids = NULL;
	guint id;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(conv));

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_focus")) {
		GtkWidget *child = NULL;
		gint page_num;

		page_num = gaim_conversation_get_index(conv);
		child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(gtkwin->notebook), page_num);

		/* TODO should really find a way to make this work no matter where the
		 * focus is inside the conv window, without having to bind to
		 * focus-in-event on the g(d|t)kwindow */
		/* try setting the signal on the focus-in-event for
		 * gtkwin->notebook->container? */
		id = g_signal_connect(G_OBJECT(gtkconv->entry), "focus-in-event",
		                      G_CALLBACK(unnotify_cb), conv);
		window_ids = g_slist_append(window_ids, GUINT_TO_POINTER(id));

		id = g_signal_connect(G_OBJECT(gtkconv->imhtml), "focus-in-event",
		                      G_CALLBACK(unnotify_cb), conv);
		window_ids = g_slist_append(window_ids, GUINT_TO_POINTER(id));
	}

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_click")) {
		/* TODO similarly should really find a way to allow for clicking in other
		 * places of the window */
		id = g_signal_connect(G_OBJECT(gtkconv->imhtml), "button-press-event",
		                               G_CALLBACK(unnotify_cb), conv);
		imhtml_ids = g_slist_append(imhtml_ids, GUINT_TO_POINTER(id));

		id = g_signal_connect(G_OBJECT(gtkconv->entry), "button-press-event",
		                      G_CALLBACK(unnotify_cb), conv);
		entry_ids = g_slist_append(entry_ids, GUINT_TO_POINTER(id));
	}

	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_type")) {
		id = g_signal_connect(G_OBJECT(gtkconv->entry), "key-press-event",
		                      G_CALLBACK(unnotify_cb), conv);
		entry_ids = g_slist_append(entry_ids, GUINT_TO_POINTER(id));
	}

	gaim_conversation_set_data(conv, "notify-window-signals", window_ids);
	gaim_conversation_set_data(conv, "notify-imhtml-signals", imhtml_ids);
	gaim_conversation_set_data(conv, "notify-entry-signals", entry_ids);

	return 0;
}

static void
detach_signals(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv = NULL;
	GaimGtkWindow *gtkwin = NULL;
	GSList *ids = NULL;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(conv));

	ids = gaim_conversation_get_data(conv, "notify-window-signals");
	for (; ids != NULL; ids = ids->next)
		g_signal_handler_disconnect(gtkwin->window, GPOINTER_TO_INT(ids->data));

	ids = gaim_conversation_get_data(conv, "notify-imhtml-signals");
	for (; ids != NULL; ids = ids->next)
		g_signal_handler_disconnect(gtkconv->imhtml, GPOINTER_TO_INT(ids->data));

	ids = gaim_conversation_get_data(conv, "notify-entry-signals");
	for (; ids != NULL; ids = ids->next)
		g_signal_handler_disconnect(gtkconv->entry, GPOINTER_TO_INT(ids->data));

	gaim_conversation_set_data(conv, "notify-message-count", GINT_TO_POINTER(0));

	gaim_conversation_set_data(conv, "notify-window-signals", NULL);
	gaim_conversation_set_data(conv, "notify-imhtml-signals", NULL);
	gaim_conversation_set_data(conv, "notify-entry-signals", NULL);
}

static void
conv_created(GaimConversation *conv)
{
	GaimConvWindow *gaimwin = NULL;
	GaimGtkWindow *gtkwin = NULL;

	gaimwin = gaim_conversation_get_window(conv);

	if (gaimwin == NULL)
		return;

	gtkwin = GAIM_GTK_WINDOW(gaimwin);

	gaim_conversation_set_data(conv, "notify-message-count", GINT_TO_POINTER(0));

	/* always attach the signals, notify() will take care of conversation type
	 * checking */
	attach_signals(conv);
}

static void
conv_switched(GaimConversation *old_conv, GaimConversation *new_conv)
{
#if 0
	GaimConvWindow *gaimwin = gaim_conversation_get_window(new_conv);
#endif

	/*
	 * If the conversation was switched, then make sure we re-noitfy
	 * because Gaim will have overwritten our custom window title.
	 */
	notify(new_conv, FALSE);

#if 0
	printf("conv_switched - %p - %p\n", old_conv, new_conv);
	printf("count - %d\n", count_messages(gaimwin));
	if (gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_switch"))
		unnotify(new_conv, FALSE);
	else {
		/* if we don't have notification on the window then we don't want to
		 * re-notify it */
		if (count_messages(gaimwin))
			notify_win(gaimwin);
	}
#endif
}

static void
deleting_conv(GaimConversation *conv)
{
	GaimConvWindow *gaimwin = NULL;

	detach_signals(conv);

	unnotify(conv, TRUE);

	gaimwin = gaim_conversation_get_window(conv);
	if (count_messages(gaimwin))
		notify_win(gaimwin);
}

#if 0
static void
conversation_drag_ended(GaimConversation *active_conv,
                        GaimConvWindow *old_gaimwin,
                        GaimConvWindow *new_gaimwin)
{
	if (old_gaimwin != new_gaimwin) {
		if (old_gaimwin == NULL) {
			/*
			gaim_conversation_autoset_title(active_conv);
			handle_urgent(new_gaimwin, FALSE);
				*/

			if (count_messages(new_gaimwin))
				notify_win(new_gaimwin);
		} else {
			printf("if else count = %d\n", count_messages(new_gaimwin));
			printf("if else count = %d\n", count_messages(old_gaimwin));
			/*
			GaimConversation *old_active_conv = NULL;
			old_active_conv = gaim_conv_window_get_active_conversation(new_gaimwin);

			gaim_conversation_autoset_title(old_active_conv);
			handle_urgent(old_gaimwin, FALSE);

			if (count_messages(old_gaimwin))
				notify_win(old_gaimwin);

			gaim_conversation_autoset_title(active_conv);
			handle_urgent(new_gaimwin, FALSE);

			if (count_messages(new_gaimwin))
				notify_win(new_gaimwin);
				*/
		}
	} else {
		printf("else count = %d\n", count_messages(new_gaimwin));
		printf("else count = %d\n", count_messages(old_gaimwin));
		/*
		gaim_conversation_autoset_title(active_conv);
		handle_urgent(old_gaimwin, FALSE);

		if (count_messages(old_gaimwin))
			notify_win(old_gaimwin);
			*/
	}
}
#endif

static void
handle_string(GaimConvWindow *gaimwin)
{
	GtkWindow *window = NULL;
	gchar newtitle[256];

	g_return_if_fail(gaimwin != NULL);
#ifndef GAIM_SMALL_SCREEN
	window = GTK_WINDOW(GAIM_GTK_WINDOW(gaimwin)->window);
	g_return_if_fail(window != NULL);

	g_snprintf(newtitle, sizeof(newtitle), "%s%s",
	           gaim_prefs_get_string("/plugins/gtk/X11/notify/title_string"),
	           gtk_window_get_title(window));
	gtk_window_set_title(window, newtitle);
#endif
}

static void
handle_count(GaimConvWindow *gaimwin)
{
	GtkWindow *window;
	char newtitle[256];

	g_return_if_fail(gaimwin != NULL);

	window = GTK_WINDOW(GAIM_GTK_WINDOW(gaimwin)->window);
	g_return_if_fail(window != NULL);

	g_snprintf(newtitle, sizeof(newtitle), "[%d] %s", count_messages(gaimwin),
	           gtk_window_get_title(window));
	gtk_window_set_title(window, newtitle);
}

static void
handle_urgent(GaimConvWindow *gaimwin, gboolean add)
{
	XWMHints *hints;
	GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(gaimwin);

	g_return_if_fail(gtkwin != NULL);
	g_return_if_fail(gtkwin->window != NULL);
	g_return_if_fail(gtkwin->window->window != NULL);

	hints = XGetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window), GDK_WINDOW_XWINDOW(gtkwin->window->window));
	if (add)
		hints->flags |= XUrgencyHint;
	else
		hints->flags &= ~XUrgencyHint;
	XSetWMHints(GDK_WINDOW_XDISPLAY(gtkwin->window->window),
	            GDK_WINDOW_XWINDOW(gtkwin->window->window), hints);
	XFree(hints);
}

static void
type_toggle_cb(GtkWidget *widget, gpointer data)
{
	gboolean on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gchar pref[256];

	g_snprintf(pref, sizeof(pref), "/plugins/gtk/X11/notify/%s", (char *)data);

	gaim_prefs_set_bool(pref, on);
}

static void
method_toggle_cb(GtkWidget *widget, gpointer data)
{
	gboolean on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gchar pref[256];

	g_snprintf(pref, sizeof(pref), "/plugins/gtk/X11/notify/%s", (char *)data);

	gaim_prefs_set_bool(pref, on);

	if (!strcmp(data, "method_string")) {
		GtkWidget *entry = g_object_get_data(G_OBJECT(widget), "title-entry");
		gtk_widget_set_sensitive(entry, on);

		gaim_prefs_set_string("/plugins/gtk/X11/notify/title_string", gtk_entry_get_text(GTK_ENTRY(entry)));
	}

	apply_method();
}

static void
notify_toggle_cb(GtkWidget *widget, gpointer data)
{
	gboolean on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gchar pref[256];

	g_snprintf(pref, sizeof(pref), "/plugins/gtk/X11/notify/%s", (char *)data);

	gaim_prefs_set_bool(pref, on);

	apply_notify();
}

static gboolean
options_entry_cb(GtkWidget *widget, GdkEventFocus *evt, gpointer data)
{
	if (data == NULL)
		return FALSE;

	if (!strcmp(data, "method_string")) {
		gaim_prefs_set_string("/plugins/gtk/X11/notify/title_string", gtk_entry_get_text(GTK_ENTRY(widget)));
	}

	apply_method();

	return FALSE;
}

static void
apply_method() {
	GList *convs = gaim_get_conversations();
	GaimConvWindow *gaimwin = NULL;

	for (convs = gaim_get_conversations(); convs != NULL; convs = convs->next) {
		GaimConversation *conv = (GaimConversation *)convs->data;

		/* remove notifications */
		unnotify(conv, FALSE);

		gaimwin = gaim_conversation_get_window(conv);
		if (GPOINTER_TO_INT(gaim_conversation_get_data(conv, "notify-message-count")) != 0)
			/* reattach appropriate notifications */
			notify(conv, FALSE);
	}
}

static void
apply_notify()
{
	GList *convs = gaim_get_conversations();

	while (convs) {
		GaimConversation *conv = (GaimConversation *)convs->data;

		/* detach signals */
		detach_signals(conv);
		/* reattach appropriate signals */
		attach_signals(conv);

		convs = convs->next;
	}
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret = NULL, *frame = NULL;
	GtkWidget *vbox = NULL, *hbox = NULL;
	GtkWidget *toggle = NULL, *entry = NULL;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER (ret), 12);

	/*---------- "Notify For" ----------*/
	frame = gaim_gtk_make_frame(ret, _("Notify For"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	toggle = gtk_check_button_new_with_mnemonic(_("_IM windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/type_im"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(type_toggle_cb), "type_im");

	toggle = gtk_check_button_new_with_mnemonic(_("C_hat windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/type_chat"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(type_toggle_cb), "type_chat");

	toggle = gtk_check_button_new_with_mnemonic(_("_Focused windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/type_focused"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(type_toggle_cb), "type_focused");

	/*---------- "Notification Methods" ----------*/
	frame = gaim_gtk_make_frame(ret, _("Notification Methods"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* String method button */
	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	toggle = gtk_check_button_new_with_mnemonic(_("Prepend _string into window title:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_string"));
	gtk_box_pack_start(GTK_BOX(hbox), toggle, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_entry_set_max_length(GTK_ENTRY(entry), 10);
	gtk_widget_set_sensitive(GTK_WIDGET(entry),
	                         gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_string"));
	gtk_entry_set_text(GTK_ENTRY(entry),
	                   gaim_prefs_get_string("/plugins/gtk/X11/notify/title_string"));
	g_object_set_data(G_OBJECT(toggle), "title-entry", entry);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(method_toggle_cb), "method_string");
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
	                 G_CALLBACK(options_entry_cb), "method_string");

	/* Count method button */
	toggle = gtk_check_button_new_with_mnemonic(_("Insert c_ount of new messages into window title"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_count"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(method_toggle_cb), "method_count");

	/* Urgent method button */
	toggle = gtk_check_button_new_with_mnemonic(_("Set window manager \"_URGENT\" hint"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/method_urgent"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(method_toggle_cb), "method_urgent");

	/*---------- "Notification Removals" ----------*/
	frame = gaim_gtk_make_frame(ret, _("Notification Removal"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* Remove on focus button */
	toggle = gtk_check_button_new_with_mnemonic(_("Remove when conversation window _gains focus"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_focus"));
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(notify_toggle_cb), "notify_focus");

	/* Remove on click button */
	toggle = gtk_check_button_new_with_mnemonic(_("Remove when conversation window _receives click"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_click"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(notify_toggle_cb), "notify_click");

	/* Remove on type button */
	toggle = gtk_check_button_new_with_mnemonic(_("Remove when _typing in conversation window"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_type"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(notify_toggle_cb), "notify_type");

	/* Remove on message send button */
	toggle = gtk_check_button_new_with_mnemonic(_("Remove when a _message gets sent"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_send"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(notify_toggle_cb), "notify_send");

#if 0
	/* Remove on conversation switch button */
	toggle = gtk_check_button_new_with_mnemonic(_("Remove on switch to conversation ta_b"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             gaim_prefs_get_bool("/plugins/gtk/X11/notify/notify_switch"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(notify_toggle_cb), "notify_switch");
#endif

	gtk_widget_show_all(ret);
	return ret;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GList *convs = gaim_get_conversations();
	void *conv_handle = gaim_conversations_get_handle();
	/*
	void *gtk_conv_handle = gaim_gtk_conversations_get_handle();
	*/

	my_plugin = plugin;

	gaim_signal_connect(conv_handle, "received-im-msg", plugin,
	                    GAIM_CALLBACK(im_recv_im), NULL);
	gaim_signal_connect(conv_handle, "received-chat-msg", plugin,
	                    GAIM_CALLBACK(chat_recv_im), NULL);
	gaim_signal_connect(conv_handle, "sent-im-msg", plugin,
	                    GAIM_CALLBACK(im_sent_im), NULL);
	gaim_signal_connect(conv_handle, "sent-chat-msg", plugin,
	                    GAIM_CALLBACK(chat_sent_im), NULL);
	gaim_signal_connect(conv_handle, "conversation-created", plugin,
	                    GAIM_CALLBACK(conv_created), NULL);
	gaim_signal_connect(conv_handle, "chat-joined", plugin,
	                    GAIM_CALLBACK(conv_created), NULL);
	gaim_signal_connect(conv_handle, "deleting-conversation", plugin,
	                    GAIM_CALLBACK(deleting_conv), NULL);
	gaim_signal_connect(conv_handle, "conversation-switched", plugin,
	                    GAIM_CALLBACK(conv_switched), NULL);
#if 0
	gaim_signal_connect(gtk_conv_handle, "conversation-drag-ended", plugin,
	                    GAIM_CALLBACK(conversation_drag_ended), NULL);
#endif

	while (convs) {
		GaimConversation *conv = (GaimConversation *)convs->data;

		/* attach signals */
		attach_signals(conv);

		convs = convs->next;
	}

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	GList *convs = gaim_get_conversations();

	while (convs) {
		GaimConversation *conv = (GaimConversation *)convs->data;

		/* kill signals */
		detach_signals(conv);

		convs = convs->next;
	}

	return TRUE;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	NOTIFY_PLUGIN_ID,                                 /**< id             */
	N_("Message Notification"),                       /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides a variety of ways of notifying you of unread messages."),
	                                                  /**  description    */
	N_("Provides a variety of ways of notifying you of unread messages."),
	"Etan Reisner <deryni@eden.rutgers.edu>\n\t\t\tBrian Tarricone <bjt23@cornell.edu>",
	                                                  /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                              /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk");
	gaim_prefs_add_none("/plugins/gtk/X11");
	gaim_prefs_add_none("/plugins/gtk/X11/notify");

	gaim_prefs_add_bool("/plugins/gtk/X11/notify/type_im", TRUE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/type_chat", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/type_focused", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_string", FALSE);
	gaim_prefs_add_string("/plugins/gtk/X11/notify/title_string", "(*)");
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_urgent", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/method_count", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_focus", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_click", FALSE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_type", TRUE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_send", TRUE);
	gaim_prefs_add_bool("/plugins/gtk/X11/notify/notify_switch", TRUE);
}

GAIM_INIT_PLUGIN(notify, init_plugin, info)
