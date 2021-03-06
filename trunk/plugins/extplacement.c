/*
 * Extra conversation placement options for Gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "internal.h"
#include "gtkgaim.h"
#include "conversation.h"
#include "version.h"
#include "gtkplugin.h"

static void
conv_placement_by_number(GaimConversation *conv)
{
	GaimConvWindow *win = NULL;

	if (gaim_prefs_get_bool("/plugins/gtk/extplacement/placement_number_separate"))
		win = gaim_get_last_window_with_type(gaim_conversation_get_type(conv));
	else
		win = g_list_last(gaim_get_windows())->data;

	if (win == NULL) {
		win = gaim_conv_window_new();

		gaim_conv_window_add_conversation(win, conv);
		gaim_conv_window_show(win);
	} else {
		int max_count = gaim_prefs_get_int("/plugins/gtk/extplacement/placement_number");
		int count = gaim_conv_window_get_conversation_count(win);

		if (count < max_count)
			gaim_conv_window_add_conversation(win, conv);
		else {
			GList *l = NULL;

			for (l = gaim_get_windows(); l != NULL; l = l->next) {
				win = (GaimConvWindow *)l->data;

				if (gaim_prefs_get_bool("/plugins/gtk/extplacement/placement_number_separate") &&
					gaim_conversation_get_type(gaim_conv_window_get_active_conversation(win)) != gaim_conversation_get_type(conv))
					continue;

				count = gaim_conv_window_get_conversation_count(win);
				if (count < max_count) {
					gaim_conv_window_add_conversation(win, conv);
					return;
				}
			}
			win = gaim_conv_window_new();

			gaim_conv_window_add_conversation(win, conv);
			gaim_conv_window_show(win);
		}
	}
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_conv_placement_add_fnc("number", _("By conversation count"),
							   &conv_placement_by_number);
	gaim_prefs_trigger_callback("/gaim/gtk/conversations/placement");
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_conv_placement_remove_fnc("number");
	gaim_prefs_trigger_callback("/gaim/gtk/conversations/placement");
	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label(_("Conversation Placement"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
							"/plugins/gtk/extplacement/placement_number",
							_("Number of conversations per window"));
	gaim_plugin_pref_set_bounds(ppref, 1, 50);
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
							"/plugins/gtk/extplacement/placement_number_separate",
							_("Separate IM and Chat windows when placing by number"));
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,							/**< type			*/
	GAIM_GTK_PLUGIN_TYPE,							/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	GAIM_PRIORITY_DEFAULT,							/**< priority		*/
	"gtk-extplacement",								/**< id				*/
	N_("ExtPlacement"),								/**< name			*/
	VERSION,										/**< version		*/
	N_("Extra conversation placement options."),	/**< summary		*/
													/**  description	*/
	N_("Restrict the number of conversations per windows,"
	   " optionally separating IMs and Chats"),
	"Stu Tomlinson <stu@nosnilmot.com>",			/**< author			*/
	GAIM_WEBSITE,									/**< homepage		*/
	plugin_load,									/**< load			*/
	plugin_unload,									/**< unload			*/
	NULL,											/**< destroy		*/
	NULL,											/**< ui_info		*/
	NULL,											/**< extra_info		*/
	&prefs_info,									/**< prefs_info		*/
	NULL											/**< actions		*/
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk/extplacement");
	gaim_prefs_add_int("/plugins/gtk/extplacement/placement_number", 4);
	gaim_prefs_add_bool("/plugins/gtk/extplacement/placement_number_separate", FALSE);
}

GAIM_INIT_PLUGIN(extplacement, init_plugin, info)
