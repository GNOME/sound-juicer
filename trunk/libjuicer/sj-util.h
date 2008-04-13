/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-util.h
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifndef SJ_UTIL_H
#define SJ_UTIL_H

#include <sys/types.h>
#include <gtk/gtkfilechooser.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-result.h>

GnomeVFSResult make_directory_with_parents_for_uri (GnomeVFSURI * uri, guint perm);
GnomeVFSResult make_directory_with_parents (const gchar * text_uri, guint perm);

void g_list_deep_free (GList *l, GFunc free_func);

void sj_add_default_dirs (GtkFileChooser *dialog);
char *sj_get_default_music_directory (void);

#endif /* SJ_UTIL_H */
