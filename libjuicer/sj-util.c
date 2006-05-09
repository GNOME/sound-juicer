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
