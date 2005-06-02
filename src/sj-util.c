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

#include "sound-juicer.h"

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib/gutils.h>
#include <glib/gstrfuncs.h>
#include <glib/gi18n.h>
#include <nautilus-burn-drive.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtklabel.h>
#include "sj-error.h"
#include "sj-util.h"

/*
 * Totally linux-centric. Non-linux people send patches! :)
 */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __FreeBSD__
#include <sys/cdio.h>
#define CDROMEJECT CDIOCEJECT
#endif /* __FreeBSD__ */

#ifdef __linux__
#include <linux/cdrom.h>
#endif /* __linux__ */

/**
 * Stolen from gnome-vfs
 */
void
mkdir_recursive (const char *path, mode_t permission_bits, GError **error)
{
  const char *dir_separator_scanner;
  char *current_path;
  
  /* try creating a director for each level */
  for (dir_separator_scanner = path;; dir_separator_scanner++) {
    /* advance to the next directory level */
    for (;;dir_separator_scanner++) {
      if (!*dir_separator_scanner) {
        break;
      }	
      if (*dir_separator_scanner == G_DIR_SEPARATOR) {
        break;
      }
    }
    if (dir_separator_scanner - path > 0) {
      current_path = g_strndup (path, dir_separator_scanner - path);
      /* TODO: this algorithm doesn't handle "permission denied" on mkdir at all well */
      if (mkdir (current_path, permission_bits) != 0) {
        int err = errno;

	if (err != EEXIST) {
          /* We failed to create a directory and it wasn't there already; bail */
          g_free (current_path);
          g_set_error (error,
                       SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                       _("Could not create directory %s: %s"), path, g_strerror (err));
          return;
	}
      }
      g_free (current_path);
    }
    if (!*dir_separator_scanner) {
      break;
    }	
  }
  return;
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
