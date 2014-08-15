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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gtk/gtk.h>

#include "sound-juicer.h"
#include "sj-play.h"
#include "sj-main.h"

static GstElement *pipeline = NULL;
static guint id = 0, button_change_id = 0;
static gint seek_to_track = -1, current_track = -1;
static gboolean seeking = FALSE, internal_update = FALSE;
static guint64 slen = GST_CLOCK_TIME_NONE;
static gfloat vol = 1.0;
static void set_gst_ui_and_play (void);

static GtkTreeIter current_iter;

static GtkWidget *play_button, *seek_scale, *volume_button, *statusbar,  *track_listview;

/**
 * Select track number.
 */
static gboolean
select_track (void)
{
  GstStateChangeReturn ret;

  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (track_store),
                                     &current_iter, NULL, seek_to_track)) {
    g_warning (G_STRLOC ": cannot get nth child");
    return FALSE;
  }

  ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_ASYNC) {
	while (ret == GST_STATE_CHANGE_ASYNC)
		ret = gst_element_get_state (pipeline, NULL, NULL, GST_MSECOND);
  }		

  if (ret == GST_STATE_CHANGE_FAILURE) {
    return FALSE;
  } else if (ret == GST_STATE_CHANGE_SUCCESS) {
    /* state change was instant, we can seek right away */
    if (gst_element_seek (pipeline, 1.0, gst_format_get_by_nick ("track"),
                          GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, seek_to_track,
                          GST_SEEK_TYPE_NONE, -1)) {
      current_track = seek_to_track;
    } else {
      /* seek failed - what now? */
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Start playing.
 */
static void
_play (void)
{
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  set_action_enabled ("next-track", TRUE);
  set_action_enabled ("previous-track", TRUE);
}

/**
 * Pause
 */
static void
_pause (void)
{
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
}

/**
 * Stop and reset UI.
 */
static void
_stop (void)
{
  if (pipeline != NULL)
    gst_element_set_state (pipeline, GST_STATE_NULL);

  /* TODO: this should be centralised into the state change logic really */
  set_action_enabled ("next-track", FALSE);
  set_action_enabled ("previous-track", FALSE);

  set_action_enabled ("re-read", TRUE);

  gtk_widget_hide (seek_scale);
  gtk_widget_hide (volume_button);
  sj_main_set_title (NULL);
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, "");
  gtk_button_set_label (GTK_BUTTON (play_button), _("_Play"));
  slen = GST_CLOCK_TIME_NONE;

  if (current_track != -1 &&
      gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (track_store), &current_iter, NULL, current_track))
    gtk_list_store_set (track_store, &current_iter, COLUMN_STATE, STATE_IDLE, -1);

  current_track = -1;
}

/**
 * Are we playing?
 */
static gboolean
is_playing (void)
{
  return (pipeline != NULL && GST_STATE (pipeline) == GST_STATE_PLAYING);
}

/**
 * Are we paused?
 */
static gboolean
is_paused (void)
{
  return (pipeline != NULL && GST_STATE (pipeline) == GST_STATE_PAUSED);
}

/*
 * callbacks.
 */

static void
cb_hop_track (GstBus *bus, GstMessage *message, gpointer user_data)
{
  GtkTreeModel *model;
  gint tracks, next_track = current_track + 1;
  GtkTreeIter next_iter;

  model = GTK_TREE_MODEL (track_store);
  tracks = gtk_tree_model_iter_n_children (model, NULL);

  while (next_track < tracks) {
    gboolean do_play;

    if (!gtk_tree_model_iter_nth_child (model, &next_iter, NULL, next_track))
      return;
    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &next_iter,
        COLUMN_EXTRACT, &do_play, -1);
    if (do_play)
      break;
    next_track++;
  }

  if (next_track >= tracks) {
    _stop ();
    seek_to_track = 0;
  } else {
    seek_to_track = next_track;
    set_gst_ui_and_play ();
  }
}

static void
cb_error (GstBus *bus, GstMessage *message, gpointer user_data)
{
  GtkWidget *dialog;
  GError *error;

  gst_message_parse_error (message, &error, NULL);
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
				   GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				   _("Error playing CD.\n\nReason: %s"),
				   error->message);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  g_error_free (error);

  /* There may be other (more generic) error messages on the bus; set pipeline
   * to NULL state so these messages are flushed from the bus and we don't get
   * called again for those */
  _stop ();
}

static gchar *
get_label_for_time (gint sec)
{
  return g_strdup_printf ("%d:%02d", sec / 60, sec % 60);
}

static void
set_statusbar_pos (gint pos, gint len)
{
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

  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, r);
  g_free (r);
}

static gboolean
cb_set_time (gpointer data)
{
  gint64 pos, len;

  if (seeking)
    return TRUE;

  if (gst_element_query_duration (pipeline, GST_FORMAT_TIME, &len) &&
      gst_element_query_position (pipeline, GST_FORMAT_TIME, &pos)) {
    internal_update = TRUE;
    gtk_range_set_value (GTK_RANGE (seek_scale), (gdouble) pos / len);
    set_statusbar_pos (pos / GST_SECOND, len / GST_SECOND);
    slen = len;
    internal_update = FALSE;
  }

  /* repeat */
  return TRUE;
}

static gboolean
cb_change_button (gpointer data)
{
  button_change_id = 0;

  gtk_button_set_label (GTK_BUTTON (data), _("_Play"));

  /* once */
  return FALSE;
}

static void
cb_state (GstBus *bus, GstMessage *message, gpointer user_data)
{
  GstState old_state, new_state;
  GstStateChange transition;

  /* all pipe elements will receive state transition messages,
   * so we filter those out. This won't be neccessary after
   * the playbin migration */
  if ((GstElement*)GST_MESSAGE_SRC (message) != pipeline)
    return;

  gst_message_parse_state_changed (message, &old_state, &new_state, NULL);
  transition = GST_STATE_TRANSITION (old_state, new_state);

  if (transition == GST_STATE_CHANGE_READY_TO_PAUSED) {
    char *title;
    gtk_widget_show (seek_scale);
    gtk_widget_show (volume_button);
    gtk_list_store_set (track_store, &current_iter,
        COLUMN_STATE, STATE_PLAYING, -1);
    gtk_tree_model_get (GTK_TREE_MODEL (track_store),
        &current_iter, COLUMN_TITLE, &title, -1);
    sj_main_set_title (title);
    g_free (title);
  } else if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {
    gtk_widget_hide (seek_scale);
    gtk_widget_hide (volume_button);
    gtk_statusbar_pop (GTK_STATUSBAR (statusbar), 0);
    slen = GST_CLOCK_TIME_NONE;
    gtk_list_store_set (track_store, &current_iter,
        COLUMN_STATE, STATE_IDLE, -1);
    sj_main_set_title (NULL);
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, "");
    current_track = -1;
  } else if (transition == GST_STATE_CHANGE_PAUSED_TO_PLAYING) {
    gtk_button_set_label (GTK_BUTTON (play_button), _("_Pause"));
    if (id)
      g_source_remove (id);
    id = g_timeout_add (100, (GSourceFunc) cb_set_time, NULL);
    g_source_set_name_by_id (id, "[sound-juicer] cb_set_time");
    if (button_change_id) {
      g_source_remove (button_change_id);
      button_change_id = 0;
    }
  } else if (transition == GST_STATE_CHANGE_PLAYING_TO_PAUSED) {
    if (id) {
      g_source_remove (id);
      id = 0;
    }
    /* otherwise button flickers on track-switch */
    if (button_change_id)
      g_source_remove (button_change_id);
    button_change_id =
        g_timeout_add (500, (GSourceFunc) cb_change_button, play_button);
    g_source_set_name_by_id (button_change_id, "[sound-juicer] cb_change_button");
  }
}

static void
cb_source_setup (GstElement *playbin, GstElement *source, gpointer user_data)
{
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (source), "read-speed") != NULL) {
    	g_object_set (G_OBJECT (source),
    	              "read-speed", 2,
                      NULL);
    }
    /* Disable paranoia in playback mode */
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (source), "paranoia-mode"))
      g_object_set (source, "paranoia-mode", 0, NULL);

    g_object_set (G_OBJECT (source),
                  "device", brasero_drive_get_device (drive),
                  NULL);
}

/**
 * Create and update pipeline for playback of a particular track.
 */

static gboolean
setup (GError **err)
{
  if (!pipeline) {
    GstBus *bus;

    pipeline = gst_element_factory_make ("playbin", "playbin");
    if (!pipeline) {
      gst_object_unref (GST_OBJECT (pipeline));
      pipeline = NULL;
      g_set_error (err, 0, 0, _("Failed to create CD source element"));
      return FALSE;
    }

    bus = gst_element_get_bus (pipeline);
    gst_bus_add_signal_watch (bus);

    g_signal_connect (bus, "message::eos", G_CALLBACK (cb_hop_track), NULL);
    g_signal_connect (bus, "message::error", G_CALLBACK (cb_error), NULL);
    g_signal_connect (bus, "message::state-changed", G_CALLBACK (cb_state), NULL);

    g_signal_connect (pipeline, "source-setup", G_CALLBACK (cb_source_setup), NULL);

    g_object_set (G_OBJECT (pipeline), "uri", "cdda://1", NULL);

    if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (track_store), &current_iter))
      g_warning ("Cannot get first iter");
  }

  return TRUE;
}

/*
 * Public function to release device.
 */

void
stop_playback (void)
{
  _stop ();
}

/*
 * Interface entry point.
 */
void toggle_play ()
{
  GError *err = NULL;

  if (is_playing ()) {
    _pause ();
    gtk_list_store_set (track_store, &current_iter,
                        COLUMN_STATE, STATE_PAUSED, -1);
 } else if (pipeline && GST_STATE (pipeline) == GST_STATE_PAUSED &&
             current_track == seek_to_track) {
    _play ();
    gtk_list_store_set (track_store, &current_iter,
                        COLUMN_STATE, STATE_PLAYING, -1);
  } else if (pipeline && GST_STATE (pipeline) == GST_STATE_PAUSED &&
			 current_track != seek_to_track) {
    if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (track_store),
                                          &current_iter, NULL, seek_to_track))
        return;
	set_gst_ui_and_play ();
  } else if (setup (&err)) {
	current_track = -1;
	cb_hop_track (NULL,NULL,NULL);
  }	else {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
				     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				     _("Error playing CD.\n\nReason: %s"),
				     err->message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_error_free (err);
  }
}

G_MODULE_EXPORT void
on_tracklist_row_activate (GtkTreeView * treeview, GtkTreePath * path,
    GtkTreeViewColumn * col, gpointer user_data)
{
  GError *err = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint track;

  model = gtk_tree_view_get_model (treeview);
  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;
  gtk_tree_model_get (model, &iter, COLUMN_NUMBER, &track, -1);
  if (track == current_track + 1) {
    if (is_playing () || is_paused ()) {
      toggle_play ();
      return;
    }
  }

  if (setup (&err)) {
    seek_to_track = track - 1;
    set_gst_ui_and_play ();
  } else {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
				     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				     _("Error playing CD.\n\nReason: %s"),
				     err->message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_error_free (err);
  }
}

void play_next_track ()
{
  cb_hop_track (NULL, NULL, NULL);
}

void play_previous_track ()
{
  GtkTreeModel *model;
  gint prev_track = current_track - 1;
  GtkTreeIter prev_iter;

  model = GTK_TREE_MODEL (track_store);

  while (prev_track >= 0) {
    gboolean do_play;

    if (!gtk_tree_model_iter_nth_child (model, &prev_iter, NULL, prev_track))
      return;
    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &prev_iter,
        COLUMN_EXTRACT, &do_play, -1);
    if (do_play)
      break;
    prev_track--;
  }

  if (prev_track < 0) {
    _stop ();
    seek_to_track = 0;
  } else {
    seek_to_track = prev_track;
    set_gst_ui_and_play ();
  }
}

static void
set_gst_ui_and_play (void)
{
  char *title;

  gtk_list_store_set (track_store, &current_iter,
      COLUMN_STATE, STATE_IDLE, -1);
  if (select_track ()) {
    gtk_list_store_set (track_store, &current_iter,
        COLUMN_STATE, STATE_PLAYING, -1);
    gtk_tree_model_get (GTK_TREE_MODEL (track_store),
        &current_iter, COLUMN_TITLE, &title, -1);
    sj_main_set_title (title);
    g_free (title);
    _play ();
  } else {
    g_warning (G_STRLOC ": failed to select track");
    _stop ();
  }
}

static void
select_track_foreach (GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      data)
{
  gint track;

  gtk_tree_model_get (model, iter, COLUMN_NUMBER, &track, -1);
  seek_to_track = track - 1;
}

G_MODULE_EXPORT void
on_tracklist_row_selected (GtkTreeView *treeview,
               gpointer user_data)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

  if (is_playing () || is_paused ())
    return;

  if (gtk_tree_selection_count_selected_rows (selection) == 1)
    gtk_tree_selection_selected_foreach (selection, select_track_foreach, NULL);
}

/*
 * Volume.
 */

G_MODULE_EXPORT void
on_volume_changed (GtkWidget * volb, gdouble value, gpointer data)
{
  vol = value;
  if (pipeline)
    g_object_set (G_OBJECT (pipeline), "volume", vol, NULL);
  g_settings_set_double (sj_settings, SJ_SETTINGS_AUDIO_VOLUME, vol);
}

static gboolean
is_non_seek_key (GdkEvent * event)
{
  guint key;

  return gdk_event_get_keyval (event, &key) &&
    key != GDK_KEY_Left  && key != GDK_KEY_KP_Left  &&
    key != GDK_KEY_Right && key != GDK_KEY_KP_Right &&
    key != GDK_KEY_Up    && key != GDK_KEY_KP_Up    &&
    key != GDK_KEY_Down  && key != GDK_KEY_KP_Down  &&
    key != GDK_KEY_End   && key != GDK_KEY_KP_End   &&
    key != GDK_KEY_Home  && key != GDK_KEY_KP_Home;
}

/*
 * Seeking.
 */

G_MODULE_EXPORT gboolean
on_seek_press (GtkWidget * scale, GdkEvent * event, gpointer user_data)
{
  if (is_non_seek_key (event))
    return FALSE;

  seeking = TRUE;

  return FALSE;
}

G_MODULE_EXPORT void
on_seek_moved (GtkWidget * scale, gpointer user_data)
{
  gdouble val = gtk_range_get_value (GTK_RANGE (scale));
  gchar *x, *m;

  if (internal_update || !GST_CLOCK_TIME_IS_VALID (slen))
    return;

  x = get_label_for_time (slen * val / GST_SECOND);
  m = g_strdup_printf (_("Seeking to %s"), x);
  g_free (x);
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, m);
  g_free (m);
}

G_MODULE_EXPORT gboolean
on_seek_release (GtkWidget * scale, GdkEvent * event, gpointer user_data)
{
  gdouble val = gtk_range_get_value (GTK_RANGE (scale));

  /* If gst_element_seek is called when non-seeking key is released it
     causes a glitch in playback*/
  if (is_non_seek_key (event))
    return FALSE;

  seeking = FALSE;

  gst_element_seek (pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, slen * val, GST_SEEK_TYPE_NONE, -1);

  return FALSE;
}

/*
 * Grim. Very Grim.
 */
void
stop_ui_hack (void)
{
  gtk_button_set_label (GTK_BUTTON (play_button), _("_Play"));
  gtk_widget_hide (seek_scale);
  gtk_widget_hide (volume_button);
  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), 0);
  slen = GST_CLOCK_TIME_NONE;
  if (gtk_list_store_iter_is_valid (track_store, &current_iter))
    gtk_list_store_set (track_store, &current_iter, COLUMN_STATE, STATE_IDLE, -1);
  sj_main_set_title (NULL);
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, "");
  current_track = -1;
}

/**
 * Init
 */
void
sj_play_init (void)
{
  play_button     = GET_WIDGET ("play_button");
  seek_scale      = GET_WIDGET ("seek_scale");
  volume_button   = GET_WIDGET ("volume_button");
  statusbar       = GET_WIDGET ("status_bar");
  track_listview  = GET_WIDGET ("track_listview");
}
