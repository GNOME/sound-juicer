/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
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

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib/gutils.h>
#include <glib/gstrfuncs.h>
#include <libgnome/gnome-i18n.h>
#include "sj-error.h"
#include "sj-util.h"

/**
 * Stolen from gnome-vfs
 */
void
mkdir_recursive (const char *path, mode_t permission_bits, GError **error)
{
  struct stat stat_buffer;
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
      mkdir (current_path, permission_bits);
      if (stat (current_path, &stat_buffer) != 0) {
        /* we failed to create a directory and it wasn't there already;
         * bail
         */
        g_free (current_path);
        g_set_error (error,
                     SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                     _("Could not create directory %s: %s"), path, g_strerror (errno));
        return;
      }
      g_free (current_path);
    }
    if (!*dir_separator_scanner) {
      break;
    }	
  }
  return;
}

/*
 * Totally linux-centric. Non-linux people send patches! :)
 */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtklabel.h>

/**
 * Eject a CD-ROM, displaying any errors in a dialog.
 */
void
eject_cdrom (const char* device, GtkWindow *parent)
{
  int fd, result;
  g_return_if_fail (device != NULL);
  fd = open (device, O_RDONLY | O_NONBLOCK);
  if (fd == -1) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     g_strdup_printf ("<b>%s</b>\n\n%s: %s",
                                                      _("Could not eject the CD"),
                                                      _("Reason"),
                                                      g_strerror (errno)));
    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    goto done;
  }
  result = ioctl (fd, CDROMEJECT);
  if (result == -1) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     g_strdup_printf ("<b>%s</b>\n\n%s: %s",
                                                      _("Could not eject the CD"),
                                                      _("Reason"),
                                                      g_strerror (errno)));
    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    goto done;
  }
 done:
  close (fd);
  return;
}

gboolean
tray_is_opened (const char *device)
{
  int fd, status;
  
  fd = open (device, O_RDONLY | O_NONBLOCK | O_EXCL);
  if (fd < 0) {
    return FALSE;
  }
  
  status = ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
  if (status < 0) {
    close (fd);
    return FALSE;
  }
  
  close (fd);
  
  return status == CDS_TRAY_OPEN;
}
