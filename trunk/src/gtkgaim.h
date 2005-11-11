/**
 * @file gtkgaim.h UI definitions and includes
 * @ingroup gtkui
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
#ifndef _GAIM_GTKGAIM_H_
#define _GAIM_GTKGAIM_H_

#ifndef _WIN32
# include <gdk/gdkx.h>
#endif

#include <gtk/gtk.h>

#ifndef _WIN32
# define GAIM_ALERT_TITLE ""
#else
# define GAIM_ALERT_TITLE "Gaim"
#endif

/*
 * This is backwards-compatibility code for older versions of GTK+ (< 2.4.x)
 * It defines the new wrap behavior (unknown in earlier versions)
 * as the old (slightly buggy) wrap behavior.
 */
#if (!GTK_CHECK_VERSION(2,4,0))
#define GTK_WRAP_WORD_CHAR GTK_WRAP_WORD
#endif

#ifdef GAIM_SMALL_SCREEN
    #define CONTAINER_BORDER_WIDTH 6
    #define BOX_SPACING 12
#ifndef ENABLE_HILDON
    #define ENTRY_SIZE 14
#else
    #define ENTRY_SIZE 20
#endif
#else
    #define CONTAINER_BORDER_WIDTH 1
    #define BOX_SPACING 2
#endif

#endif /* _GAIM_GTKGAIM_H_ */

