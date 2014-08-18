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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "sj-util.h"

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

/*
 * True if string is NULL, empty or contains only ascii space
 */
gboolean
sj_str_is_empty (const char *s)
{
  int i = 0;

  if (s == NULL)
    return TRUE;
  while (s[i]) {
    if (!g_ascii_isspace (s[i++]))
      return FALSE;
  }
  return TRUE;
}
