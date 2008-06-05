/* 
 * Copyright (C) 2003-2005 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-util.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "sj-util.h"

/**
 * Stolen from gnome-vfs/programs/gnomevfs-mkdir.c (v1.3)
 */
gboolean
make_directory_with_parents (GFile * uri, GError **error_out)
{
	gboolean result;
	GFile *parent, *work_uri;
	GList *list = NULL;
	GError *error = NULL;

	result = g_file_make_directory (uri, NULL, &error);
	if (result || error->code != G_IO_ERROR_NOT_FOUND) {
	  if (error_out)
	    *error_out = error;
		return result;
  }

	work_uri = uri;

	while (!result && error->code == G_IO_ERROR_NOT_FOUND) {
	  g_clear_error (&error);
	  
		parent = g_file_get_parent (work_uri);
		result = g_file_make_directory (parent, NULL, &error);

		if (!result && error->code == G_IO_ERROR_NOT_FOUND)
			list = g_list_prepend (list, parent);
		work_uri = parent;
	}

	if (!result) {
		/* Clean up */
		while (list != NULL) {
			g_object_unref ((GFile *) list->data);
			list = g_list_remove (list, list->data);
		}

    if (error_out)
      *error_out = error;
		return result;
	}

	while (result && list != NULL) {
		result = g_file_make_directory ((GFile *) list->data, NULL, NULL);

		g_object_unref ((GFile *) list->data);
		list = g_list_remove (list, list->data);
	}

	result = g_file_make_directory (uri, NULL, error_out);
	return result;
}

/* Pass NULL to use g_free */
void
g_list_deep_free (GList *l, GFunc free_func)
{
  g_return_if_fail (l != NULL);
  if (free_func == NULL) free_func = (GFunc)g_free;
  g_list_foreach (l, free_func, NULL);
  g_list_free (l);
}

GFile *
sj_get_default_music_directory (void)
{
	const char *dir;
	GFile *file;

	dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
	if (dir == NULL) {
		dir = g_get_home_dir ();
	}
	file = g_file_new_for_path (dir);
	return file;
}

void
sj_add_default_dirs (GtkFileChooser *dialog)
{
	const char *dir;

	dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
	if (dir) {
		gtk_file_chooser_add_shortcut_folder (dialog, dir, NULL);
	}
}

