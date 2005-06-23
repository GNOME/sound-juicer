/*
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-play.c
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
 * Authors: Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gconf/gconf.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkrange.h>
#include <gtk/gtkstatusbar.h>

#include "sound-juicer.h"

static GstElement *pipeline = NULL;
static guint id = 0;
static gint seek_to_track = -1, current_track = -1;
static gboolean seeking = FALSE, internal_update = FALSE;
static guint64 slen = GST_CLOCK_TIME_NONE;

/**
 * Select track number.
 */

static void
select_track (void)
{
  GstElement *cd;

  if (seek_to_track == -1)
    return;

  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  cd = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "cd-source");
  gst_pad_send_event (gst_element_get_pad (cd, "src"),
      gst_event_new_segment_seek (GST_SEEK_FLAG_FLUSH |
          gst_format_get_by_nick ("track") | GST_SEEK_METHOD_SET,
          seek_to_track, seek_to_track + 1));
  current_track = seek_to_track;
  seek_to_track = -1;
}

/**
 * Start playing.
 */

static void
play (void)
{
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

/**
 * Pause
 */

static void
pause (void)
{
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
}

/**
 * Stop.
 */

static void
stop (void)
{
  gst_element_set_state (pipeline, GST_STATE_NULL);
}

/**
 * Are we playing?
 */

static gboolean
is_playing (void)
{
  return (pipeline != NULL && GST_STATE (pipeline) == GST_STATE_PLAYING);
}

/*
 * callbacks.
 */

static gboolean
cb_hop_track (gpointer data)
{
  GtkTreeModel *model;
  gint tracks;

  model = GTK_TREE_MODEL (track_store);
  tracks = gtk_tree_model_iter_n_children (model, NULL);

  if (current_track + 2 == tracks) {
    stop ();
    seek_to_track = 0;
  } else {
    seek_to_track = current_track + 1;
    select_track ();
    play ();
  }

  /* FIXME: notify treelist so we can add a 'playing' icon */

  /* once */
  return FALSE;
}

static void
cb_eos (GstElement * p, gpointer data)
{
  g_idle_add ((GSourceFunc) cb_hop_track, NULL);
}

static void
cb_error (GstElement * p, GstElement * source, GError * err, gchar * debug,
    gpointer data)
{
  /* FIXME: nice dialog */
  g_warning (err->message);
}

static gchar *
get_label_for_time (gint sec)
{
  return g_strdup_printf ("%d:%02d", sec / 60, sec % 60);
}

static void
set_statusbar_pos (gint pos, gint len)
{
  GtkWidget *s = glade_xml_get_widget (glade, "status_bar");
  static gint prev_pos = 0, prev_len = 0;
  gchar *x, *y, *r;

  if (prev_pos == pos && prev_len == len)
    return;
  prev_pos = pos;
  prev_len = len;

  x = get_label_for_time (pos);
  y = get_label_for_time (len);
  r = g_strdup_printf ("%s / %s", x, y);
  g_free (x);
  g_free (y);

  gtk_statusbar_push (GTK_STATUSBAR (s), 0, r);
  g_free (r);
}

static gboolean
cb_set_time (gpointer data)
{
  GtkWidget *scale;
  GstElement *cd;
  GstPad *pad;
  GstFormat fmt = GST_FORMAT_TIME;
  gint64 pos, len;

  if (seeking)
    return TRUE;

  scale = glade_xml_get_widget (glade, "seek_scale");
  cd = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "cd-source");
  pad = gst_element_get_pad (cd, "src");

  if (gst_pad_query (pad, GST_QUERY_TOTAL, &fmt, &len) &&
      gst_pad_query (pad, GST_QUERY_POSITION, &fmt, &pos)) {
    internal_update = TRUE;
    gtk_range_set_value (GTK_RANGE (scale), (gdouble) pos / len);
    set_statusbar_pos (pos / GST_SECOND, len / GST_SECOND);
    slen = len;
    internal_update = FALSE;
  }

  /* repeat */
  return TRUE;
}

static void
cb_state (GstElement * p, GstElementState old_state, GstElementState new_state,
    gpointer data)
{
  /* FIXME: change play button label/icon */

  if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
    GtkWidget *w = glade_xml_get_widget (glade, "seek_scale");
    gtk_widget_show (w);
  } else if (new_state == GST_STATE_READY && old_state == GST_STATE_PAUSED) {
    GtkWidget *w = glade_xml_get_widget (glade, "seek_scale"),
        *s = glade_xml_get_widget (glade, "status_bar");
    gtk_widget_hide (w);
    gtk_statusbar_pop (GTK_STATUSBAR (s), 0);
    slen = GST_CLOCK_TIME_NONE;
  } else if (new_state == GST_STATE_PLAYING) {
    GtkWidget *b = glade_xml_get_widget (glade, "play_button");
    gtk_button_set_label (GTK_BUTTON (b), GTK_STOCK_MEDIA_PAUSE);
    id = g_timeout_add (100, (GSourceFunc) cb_set_time, NULL);
  } else if (old_state == GST_STATE_PLAYING) {
     GtkWidget *b = glade_xml_get_widget (glade, "play_button");
    gtk_button_set_label (GTK_BUTTON (b), GTK_STOCK_MEDIA_PLAY);
    if (id) {
      g_source_remove (id);
      id = 0;
    }
  }
}

/**
 * Create and update pipeline for playback of a particular track.
 */

static gboolean
setup (GError ** err)
{
  if (!pipeline) {
    GstElement *internal_t, *out, *conv, *scale, *cdp, *queue;

    pipeline = gst_thread_new ("playback");
    g_signal_connect (pipeline, "eos", G_CALLBACK (cb_eos), NULL);
    g_signal_connect (pipeline, "error", G_CALLBACK (cb_error), NULL);
    g_signal_connect (pipeline, "state-change", G_CALLBACK (cb_state), NULL);

    cdp = gst_element_factory_make ("cdparanoia", "cd-source");
    if (!cdp) {
      gst_object_unref (GST_OBJECT (pipeline));
      pipeline = NULL;
      g_set_error (err, 0, 0, 
          _("Failed to create CD source element"));
      return FALSE;
    }
    /* do not set to 1, that messes up buffering. 2 is enough to keep
     * noise from the drive down. */
    g_object_set (G_OBJECT (cdp), "read-speed", 2,
        "device", drive->device, NULL);

    internal_t = gst_thread_new ("output");
    queue = gst_element_factory_make ("queue", "queue");
    conv = gst_element_factory_make ("audioconvert", "conv");
    scale = gst_element_factory_make ("audioscale", "scale");
    out = gst_gconf_get_default_audio_sink ();

    gst_element_link_many (cdp, queue, conv, scale, out, NULL);
    gst_bin_add_many (GST_BIN (internal_t), conv, scale, out, NULL);
    gst_bin_add_many (GST_BIN (pipeline), cdp, queue, internal_t, NULL);

    /* if something went wrong, cleanup here is easier... */
    if (!out) {
      gst_object_unref (GST_OBJECT (pipeline));
      pipeline = NULL;
      g_set_error (err, 0, 0,
          _("Failed to create audio output"));
      return FALSE;
    }

    seek_to_track = 0;
  }

  return TRUE;
}

/**
 * Interface entry point.
 */

void
on_play_activate (GtkWidget *button, gpointer user_data)
{
  if (is_playing ()) {
    pause ();
  } else if (setup (NULL)) {
    select_track ();
    play ();
  }
}

void
on_tracklist_row_activate (GtkTreeView * treeview, GtkTreePath * path,
    GtkTreeViewColumn * col, gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint track;

  model = gtk_tree_view_get_model (treeview);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, COLUMN_NUMBER, &track, -1);

  if (setup (NULL)) {
    seek_to_track = track - 1;
    select_track ();
    play ();
  }
}

/**
 * Seeking.
 */

gboolean
on_seek_press (GtkWidget * scale, GdkEventButton * event, gpointer user_data)
{
  seeking = TRUE;

  return FALSE;
}

void
on_seek_moved (GtkWidget * scale, gpointer user_data)
{
  GtkWidget *s = glade_xml_get_widget (glade, "status_bar");
  gdouble val = gtk_range_get_value (GTK_RANGE (scale));
  gchar *x, *m;

  if (internal_update || !GST_CLOCK_TIME_IS_VALID (slen))
    return;

  x = get_label_for_time (slen * val / GST_SECOND);
  m = g_strdup_printf (_("Seeking to %s"), x);
  g_free (x);
  gtk_statusbar_push (GTK_STATUSBAR (s), 0, m);
  g_free (m);
}

gboolean
on_seek_release (GtkWidget * scale, GdkEventButton * event, gpointer user_data)
{
  gdouble val = gtk_range_get_value (GTK_RANGE (scale));
  GstElement *cd;

  cd = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "cd-source");
  seeking = FALSE;

  gst_pad_send_event (gst_element_get_pad (cd, "src"),
      gst_event_new_seek (GST_SEEK_FLAG_FLUSH | GST_FORMAT_TIME |
          GST_SEEK_METHOD_SET, slen * val));

  return FALSE;
}
