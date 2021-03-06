/**
 * @file gtkdialogs.h Assorted GTK+ dialogs
 * @defgroup gtkui GTK+ User Interface
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */
#ifndef _GAIM_GTKDIALOGS_H_
#define _GAIM_GTKDIALOGS_H_

#include "gtkgaim.h"

#include "account.h"
#include "conversation.h"

/* Functions in gtkdialogs.c (these should actually stay in this file) */
void gaim_gtkdialogs_destroy_all();
void gaim_gtkdialogs_about();
void gaim_gtkdialogs_im();
void gaim_gtkdialogs_im_with_user(GaimAccount *, const char *);
void gaim_gtkdialogs_info();
void gaim_gtkdialogs_log();
void gaim_gtkdialogs_warn(GaimConnection *, const char *);
void gaim_gtkdialogs_alias_contact(GaimContact *);
void gaim_gtkdialogs_alias_buddy(GaimBuddy *);
void gaim_gtkdialogs_alias_chat(GaimChat *);

void gaim_gtkdialogs_remove_buddy(GaimBuddy *);
void gaim_gtkdialogs_remove_group(GaimGroup *);
void gaim_gtkdialogs_remove_chat(GaimChat *);
void gaim_gtkdialogs_remove_contact(GaimContact *);

/* Everything after this should probably be moved elsewhere */

/**
 * Our UI's identifier.
 */
#define GAIM_GTK_UI "gtk-gaim"


#define GAIM_DIALOG(x)	x = gtk_window_new(GTK_WINDOW_TOPLEVEL); \
			gtk_window_set_type_hint(GTK_WINDOW(x), GDK_WINDOW_TYPE_HINT_DIALOG)
#define GAIM_WINDOW_ICONIFIED(x) (gdk_window_get_state(GTK_WIDGET(x)->window) & GDK_WINDOW_STATE_ICONIFIED)

#define DEFAULT_FONT_FACE "Helvetica"

/* this is used for queuing messages received while away. This is really a UI function
 * which is why the struct is here. */

struct queued_message {
	char name[80];
	char alias[80];
	char *message;
	time_t tm;
	GaimAccount *account;
	GaimMessageFlags flags;
};

struct smiley_theme {
	char *path;
	char *name;
	char *desc;
	char *icon;
	char *author;
	
	struct smiley_list *list;
};

/* Globals in main.c */

extern GtkWidget *mainwindow;
extern int docklet_count;

/* Globals in themes.c */
extern struct smiley_theme *current_smiley_theme;
extern GSList *smiley_themes;

/* Functions in main.c */
extern void show_login();
extern void gaim_setup(GaimConnection *);

/* Functions in session.c */
extern void session_init(gchar *, gchar *, gchar *);
extern void session_end();

/* Functions in themes.c */
extern void smiley_themeize(GtkWidget *);
extern void smiley_theme_probe();
extern void load_smiley_theme(const char *file, gboolean load);
extern GSList *get_proto_smileys(const char *id);

#endif /* _GAIM_GTKDIALOGS_H_ */
