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

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>

/**
 * Stolen from gnome-vfs/programs/gnomevfs-mkdir.c (v1.3)
 */
GnomeVFSResult
make_directory_with_parents_for_uri (GnomeVFSURI * uri, guint perm)
{
	GnomeVFSResult result;
	GnomeVFSURI *parent, *work_uri;
	GList *list = NULL;

	result = gnome_vfs_make_directory_for_uri (uri, perm);
	if (result == GNOME_VFS_OK || result != GNOME_VFS_ERROR_NOT_FOUND)
		return result;

	work_uri = uri;

	while (result == GNOME_VFS_ERROR_NOT_FOUND) {
		parent = gnome_vfs_uri_get_parent (work_uri);
		result = gnome_vfs_make_directory_for_uri (parent, perm);

		if (result == GNOME_VFS_ERROR_NOT_FOUND)
			list = g_list_prepend (list, parent);
		work_uri = parent;
	}

	if (result != GNOME_VFS_OK) {
		/* Clean up */
		while (list != NULL) {
			gnome_vfs_uri_unref ((GnomeVFSURI *) list->data);
			list = g_list_remove (list, list->data);
		}

		return result;
	}

	while (result == GNOME_VFS_OK && list != NULL) {
		result = gnome_vfs_make_directory_for_uri
		    ((GnomeVFSURI *) list->data, perm);

		gnome_vfs_uri_unref ((GnomeVFSURI *) list->data);
		list = g_list_remove (list, list->data);
	}

	result = gnome_vfs_make_directory_for_uri (uri, perm);
	return result;
}

GnomeVFSResult
make_directory_with_parents (const gchar * text_uri, guint perm)
{
	GnomeVFSURI *uri;
	GnomeVFSResult result;

	uri = gnome_vfs_uri_new (text_uri);
	result = make_directory_with_parents_for_uri (uri, perm);
	gnome_vfs_uri_unref (uri);

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

/* Copied from xdg-user-dir-lookup.c */
#include <stdlib.h>
#include <string.h>

static char *
xdg_user_dir_lookup (const char *type)
{
  FILE *file;
  char *home_dir, *config_home, *config_file;
  char buffer[512];
  char *user_dir;
  char *p, *d;
  int len;
  int relative;
  
  home_dir = getenv ("HOME");

  if (home_dir == NULL)
    return strdup ("/tmp");

  config_home = getenv ("XDG_CONFIG_HOME");
  if (config_home == NULL || config_home[0] == 0)
    {
      config_file = malloc (strlen (home_dir) + strlen ("/.config/user-dirs.dirs") + 1);
      strcpy (config_file, home_dir);
      strcat (config_file, "/.config/user-dirs.dirs");
    }
  else
    {
      config_file = malloc (strlen (config_home) + strlen ("/user-dirs.dirs") + 1);
      strcpy (config_file, config_home);
      strcat (config_file, "/user-dirs.dirs");
    }

  file = fopen (config_file, "r");
  free (config_file);
  if (file == NULL)
    goto error;

  user_dir = NULL;
  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;
      
      p = buffer;
      while (*p == ' ' || *p == '\t')
	p++;
      
      if (strncmp (p, "XDG_", 4) != 0)
	continue;
      p += 4;
      if (strncmp (p, type, strlen (type)) != 0)
	continue;
      p += strlen (type);
      if (strncmp (p, "_DIR", 4) != 0)
	continue;
      p += 4;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '=')
	continue;
      p++;
      
      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '"')
	continue;
      p++;
      
      relative = 0;
      if (strncmp (p, "$HOME/", 6) == 0)
	{
	  p += 6;
	  relative = 1;
	}
      else if (*p != '/')
	continue;
      
      if (relative)
	{
	  user_dir = malloc (strlen (home_dir) + 1 + strlen (p) + 1);
	  strcpy (user_dir, home_dir);
	  strcat (user_dir, "/");
	}
      else
	{
	  user_dir = malloc (strlen (p) + 1);
	  *user_dir = 0;
	}
      
      d = user_dir + strlen (user_dir);
      while (*p && *p != '"')
	{
	  if ((*p == '\\') && (*(p+1) != 0))
	    p++;
	  *d++ = *p++;
	}
      *d = 0;
    }  
  fclose (file);

  if (user_dir)
    return user_dir;

 error:
  /* Special case desktop for historical compatibility */
  if (strcmp (type, "DESKTOP") == 0)
    {
      user_dir = malloc (strlen (home_dir) + strlen ("/Desktop") + 1);
      strcpy (user_dir, home_dir);
      strcat (user_dir, "/Desktop");
      return user_dir;
    }
  else
    return strdup (home_dir);
}

char *
sj_get_default_music_directory (void)
{
	char *uri, *dir;

	dir = xdg_user_dir_lookup ("MUSIC");
	if (dir == NULL) {
		return gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	}
	uri = gnome_vfs_get_uri_from_local_path (dir);
	g_free (dir);
	return uri;
}

void
sj_add_default_dirs (GtkFileChooser *dialog)
{
	char *dir;

	dir = xdg_user_dir_lookup ("MUSIC");
	if (dir == NULL)
		return;
	gtk_file_chooser_add_shortcut_folder (dialog, dir, NULL);
	g_free (dir);
}

