/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-musicbrainz-fake.c
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

#include <glib/gerror.h>
#include <glib/gmessages.h>
#include <glib/gtimer.h>
#include <glib/gmain.h>
#include "sj-gstreamer.h"

void sj_gstreamer_init (int argc, char **argv, GError **error) 
{
  g_print ("gstreamer-fake: init()\n");
}

void sj_gstreamer_shutdown (void)
{
  g_print ("gstreamer-fake: shutdown()\n");
}

void sj_gstreamer_set_cdrom (const char* device)
{
  g_print ("gstreamer-fake: set_cdrom(\"%s\")\n", device == NULL ? "NULL" : device);
}

static int counter;

static gboolean idle_callback(gpointer data)
{
  g_print(".");
  g_usleep(100);
  counter++;
  if (counter > 100) {
    /* fire finish */
  } else if (counter % 10 == 0) {
    /* fire progress event */
  }
  return TRUE;
}

void sj_gstreamer_extract_track (const TrackDetails *track, const char* path, GError **error)
{
  g_print ("gstreamer-fake: extract_track()\n");
  g_idle_add (idle_callback, NULL);
}
