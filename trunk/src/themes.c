/*
 * Themes for Gaim
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
 *
 */
#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"

struct smiley_list {
	char *sml;
	GSList *smileys;
	struct smiley_list *next;
};

GSList *smiley_themes = NULL;
struct smiley_theme *current_smiley_theme;

void smiley_themeize(GtkWidget *imhtml)
{
	struct smiley_list *list;
	if (!current_smiley_theme)
		return;
	
	gtk_imhtml_remove_smileys(GTK_IMHTML(imhtml));
	list = current_smiley_theme->list;
	while (list) {
		char *sml = !strcmp(list->sml, "default") ? NULL : list->sml;
		GSList *icons = list->smileys;
		while (icons) {
			gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), sml, icons->data);
			icons = icons->next;
		}
		list = list->next;
	}
}

void load_smiley_theme(const char *file, gboolean load)
{
	FILE *f = g_fopen(file, "r");
	char buf[256];
	char *i;
	struct smiley_theme *theme=NULL;
	struct smiley_list *list = NULL;
	GSList *lst = smiley_themes;
	char *dirname;

	if (!f)
		return;

	while (lst) {
		struct smiley_theme *thm = lst->data;
		if (!strcmp(thm->path, file)) {
			theme = thm;
			break;
		}
		lst = lst->next;
	}

	if (!theme) {
		theme = g_new0(struct smiley_theme, 1);
		theme->path = g_strdup(file);
		smiley_themes = g_slist_append(smiley_themes, theme);
	}

	dirname = g_path_get_dirname(file);
	if (load) {
		if (current_smiley_theme) {
			GSList *already_freed = NULL;
			struct smiley_list *wer = current_smiley_theme->list, *wer2;
			while (wer) {
				while (wer->smileys) {
					GtkIMHtmlSmiley *uio = wer->smileys->data;
					if (uio->icon)
						g_object_unref(uio->icon);
					if (!g_slist_find(already_freed, uio->file)) {
						g_free(uio->file);
						already_freed = g_slist_append(already_freed, uio->file);
					}
					g_free(uio->smile);
					g_free(uio);
					wer->smileys=g_slist_remove(wer->smileys, uio);
				}
				wer2 = wer->next;
				g_free(wer->sml);
				g_free(wer);
				wer = wer2;
			}
			current_smiley_theme->list = NULL;
			g_slist_free(already_freed);
		}
		current_smiley_theme = theme;
	}


	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f)) {
			break;
		}

		if (buf[0] == '#' || buf[0] == '\0')
			continue;

		i = buf;
		while (isspace(*i))
			i++;

		if (*i == '[' && strchr(i, ']') && load) {
			struct smiley_list *child = g_new0(struct smiley_list, 1);
			child->sml = g_strndup(i+1, strchr(i, ']') - i - 1);
			if (theme->list)
				list->next = child;
			else
				theme->list = child;
			list = child;
		} else if (!g_ascii_strncasecmp(i, "Name=", strlen("Name="))) {
			if(theme->name)
				g_free(theme->name);
			theme->name = g_strdup(i+ strlen("Name="));
			theme->name[strlen(theme->name)-1] = 0;
		} else if (!g_ascii_strncasecmp(i, "Description=", strlen("Description="))) {
			if(theme->desc)
				g_free(theme->desc);
			theme->desc = g_strdup(i + strlen("Description="));
			theme->desc[strlen(theme->desc)-1] = 0;
		} else if (!g_ascii_strncasecmp(i, "Icon=", strlen("Icon="))) {
			if(theme->icon)
				g_free(theme->icon);
			theme->icon = g_build_filename(dirname, i + strlen("Icon="), NULL);
			theme->icon[strlen(theme->icon)-1] = 0;
		} else if (!g_ascii_strncasecmp(i, "Author=", strlen("Author="))) {
			if(theme->author)
				g_free(theme->author);
			theme->author = g_strdup(i + strlen("Author="));
			theme->author[strlen(theme->author)-1] = 0;
		} else if (load && list) {
			gboolean hidden = FALSE;
			char *sfile = NULL;

			if (*i == '!' && *(i + 1) == ' ') {
				hidden = TRUE;
				i = i + 2;
			}
			while  (*i) {
				char l[64];
				int li = 0;
				while (!isspace(*i))
					l[li++] = *(i++);
				if (!sfile) {
					l[li] = 0;
					sfile = g_build_filename(dirname, l, NULL);
				} else {
					GtkIMHtmlSmiley *smiley = g_new0(GtkIMHtmlSmiley, 1);
					l[li] = 0;
					smiley->file = sfile;
					smiley->smile = g_strdup(l);
					smiley->hidden = hidden;
					list->smileys = g_slist_append(list->smileys, smiley);
				}
				while (isspace(*i))
					i++;

			}
		}
	}

	if (load) {
		GList *cnv;

		for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
			GaimConversation *conv = cnv->data;

			if (GAIM_IS_GTK_CONVERSATION(conv)) {
				smiley_themeize(GAIM_GTK_CONVERSATION(conv)->imhtml);
				smiley_themeize(GAIM_GTK_CONVERSATION(conv)->entry);
			}
		}
	}

	g_free(dirname);
	fclose(f);
}

void smiley_theme_probe()
{
	GDir *dir;
	const gchar *file;
	gchar *path;
	int l;

	char* probedirs[3];
	probedirs[0] = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", NULL);
	probedirs[1] = g_build_filename(gaim_user_dir(), "smileys", NULL);
	probedirs[2] = 0;
	for (l=0; probedirs[l]; l++) {
		dir = g_dir_open(probedirs[l], 0, NULL);
		if (dir) {
			while ((file = g_dir_read_name(dir))) {
				path = g_build_filename(probedirs[l], file, "theme", NULL);

				/* Here we check to see that the theme has proper syntax.
				 * We set the second argument to FALSE so that it doesn't load
				 * the theme yet.
				 */
				load_smiley_theme(path, FALSE);
				g_free(path);
			}
			g_dir_close(dir);
		} else if (l == 1) {
			g_mkdir(probedirs[l], S_IRUSR | S_IWUSR | S_IXUSR);
		}
		g_free(probedirs[l]);
	}
}

GSList *get_proto_smileys(const char *id) {
	GaimPlugin *proto = gaim_find_prpl(id);
	struct smiley_list *list, *def;

	if(!current_smiley_theme)
		return NULL;

	if(!current_smiley_theme->list)
		return NULL;

	def = list = current_smiley_theme->list;

	while(list) {
		if(!strcmp(list->sml, "default"))
			def = list;
		else if(proto && !strcmp(proto->info->name, list->sml))
			break;

		list = list->next;
	}

	return list ? list->smileys : def->smileys;
}
