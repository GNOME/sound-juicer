/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-about.c
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

#include <libgnomeui/gnome-about.h>

void on_about_activate (void)
{
  static GtkWidget *win = NULL;
  GdkPixbuf *pixbuf;
  
  const gchar *authors[] = {
    "Ross Burton <ross@burtonini.com>",
    NULL
  };
  const gchar *documentors[] = {
    "Mike Hearn <mike@theoretic.com>",
    NULL
  };
  const gchar *translator_credits = NULL;
  
  if (win != NULL) {
    gtk_window_present (GTK_WINDOW (win));
    return;
  }
  
  /* TODO: I think this is leaking */
  /* TODO: pass a GError */
  pixbuf = gdk_pixbuf_new_from_file (DATADIR"/orange-slice.png", NULL);
  win = gnome_about_new (_("Sound Juicer"), VERSION,
                         "Copyright \xc2\xa9 2003 Ross Burton",
                         _("A CD Ripper"),
                         authors, documentors, translator_credits, pixbuf);
  gtk_window_set_icon_from_file (GTK_WINDOW (win), PIXMAPDIR"/sound-juicer.png", NULL);
  gtk_window_set_transient_for (GTK_WINDOW (win), GTK_WINDOW (main_window));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (win), TRUE);
  
  g_object_add_weak_pointer (G_OBJECT (win), (gpointer) & win);
  
  gtk_window_present (GTK_WINDOW (win));
}
