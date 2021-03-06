/*
 * GtkIMHtmlToolbar
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
#ifndef _GAIM_GTKIMHTMLTOOLBAR_H_
#define _GAIM_GTKIMHTMLTOOLBAR_H_

#include <gtk/gtkvbox.h>
#include "gtkimhtml.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_IMHTMLTOOLBAR_DEFAULT_FONT_FACE "Helvetica 12"

#define GTK_TYPE_IMHTMLTOOLBAR            (gtk_imhtmltoolbar_get_type ())
#define GTK_IMHTMLTOOLBAR(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_IMHTMLTOOLBAR, GtkIMHtmlToolbar))
#define GTK_IMHTMLTOOLBAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMHTMLTOOLBAR, GtkIMHtmlToolbarClass))
#define GTK_IS_IMHTMLTOOLBAR(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_IMHTMLTOOLBAR))
#define GTK_IS_IMHTMLTOOLBAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMHTMLTOOLBAR))

typedef struct _GtkIMHtmlToolbar		GtkIMHtmlToolbar;
typedef struct _GtkIMHtmlToolbarClass		GtkIMHtmlToolbarClass;

struct _GtkIMHtmlToolbar {
	GtkVBox box;

	GtkWidget *imhtml;

	GtkTooltips *tooltips;

	GtkWidget *bold;
	GtkWidget *italic;
	GtkWidget *underline;

	GtkWidget *larger_size;
	GtkWidget *normal_size;
	GtkWidget *smaller_size;

	GtkWidget *font;
	GtkWidget *fgcolor;
	GtkWidget *bgcolor;

	GtkWidget *image;
	GtkWidget *link;
	GtkWidget *smiley;
	GtkWidget *log;

	GtkWidget *font_dialog;
	GtkWidget *fgcolor_dialog;
	GtkWidget *bgcolor_dialog;
	GtkWidget *link_dialog;
	GtkWidget *smiley_dialog;
	GtkWidget *image_dialog;

	char *sml;
};

struct _GtkIMHtmlToolbarClass {
	GtkVBoxClass parent_class;

};

GType      gtk_imhtmltoolbar_get_type         (void);
GtkWidget* gtk_imhtmltoolbar_new              (void);

void gtk_imhtmltoolbar_attach    (GtkIMHtmlToolbar *toolbar, GtkWidget *imhtml);
void gtk_imhtmltoolbar_associate_smileys (GtkIMHtmlToolbar *toolbar, const char *proto_id);


#ifdef __cplusplus
}
#endif

#endif /* _GAIM_GTKIMHTMLTOOLBAR_H_ */
