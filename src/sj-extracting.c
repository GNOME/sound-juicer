/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-extracting.c
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

#include <glib/glist.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtklabel.h>
#include <glade/glade-xml.h>
#include "sj-util.h"

static GList *pending = NULL; /* a GList of TrackDetails* to rip */
static int total_ripping; /* the number of tracks to rip, not decremented */
static int total_duration, current_duration;
static int track_duration; /* duration of current track for progress dialog */

static GtkWidget *progress_dialog;
static GtkWidget *track_progress, *album_progress;
static GtkWidget *progress_label;

static void pop_and_rip (void)
{
  TrackDetails *track;
  char *file_path, *directory;
  GError *error = NULL;
  int left;

  if (pending == NULL) {
    gtk_widget_hide (progress_dialog);
    return;
  }

  track = pending->data;
  pending = g_list_next (pending);

  left = total_ripping - (g_list_length (pending) + 1); /* +1 as we've popped already */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (album_progress), (float)left/(float)total_ripping);

  track_duration = track->duration;

  gtk_label_set_text (GTK_LABEL (progress_label), g_strdup_printf (_("Currently extracting '%s'"), track->title));
  file_path = g_strdup_printf("%s/%s/%s.ogg", base_path, track->album->title, track->title); /* TODO: CRAP */
  directory = g_path_get_dirname (file_path);
  if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
    GError *error = NULL;
    mkdir_recursive (directory, 0750, &error);
    if (error) {
      g_warning (_("mkdir() failed: %s"), error->message);
      g_error_free (error);
      return;
    }
  }
  g_free (directory);
  extracting = TRUE;
  sj_extractor_extract_track (extractor, track, file_path, &error);
  if (error) {
    g_print ("Error extracting: %s\n", error->message);
    g_error_free (error);
    return;
  }
  g_free (file_path);
  return;
}

/**
 * Cancel in the progress dialog clicked
 */
void on_progress_cancel_clicked (GtkWidget *button, gpointer user_data)
{
  sj_extractor_cancel_extract(extractor);
  gtk_widget_hide (progress_dialog);
}

/**
 * Called for every selected track when ripping
 */
static gboolean rip_track_foreach_cb (GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
  gboolean extract;
  TrackDetails *track;

  gtk_tree_model_get (model, iter,
                      COLUMN_EXTRACT, &extract,
                      COLUMN_DETAILS, &track,
                      -1);
  if (!extract) return FALSE;
  pending = g_list_append (pending, track);
  ++total_ripping;
  total_duration += track->duration;
  return FALSE;
}

/**
 * Callback from SjExtractor to report progress
 */
static void on_progress_cb (SjExtractor *extractor, const int seconds, gpointer data)
{
  if (track_duration != 0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (track_progress), (float)seconds/(float)track_duration);
  } else {
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (track_progress));
  }

  if (total_duration != 0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (album_progress), (float)(current_duration + seconds)/(float)total_duration);
  }

  return;
}

/**
 * Callback from SjExtractor to report completion.
 */
static void on_completion_cb (SjExtractor *extractor, gpointer data)
{
  extracting = FALSE;
  /* TODO: uncheck the Extract check box */
  current_duration += sj_extractor_get_track_details (extractor)->duration;
  pop_and_rip ();
  return;
}

/**
 * Clicked on Extract in the UI
 */
void on_extract_activate (GtkWidget *button, gpointer user_data)
{
  if (progress_dialog == NULL) {
    progress_dialog = glade_xml_get_widget (glade, "progress_dialog");
    gtk_window_set_transient_for (GTK_WINDOW (progress_dialog), GTK_WINDOW (main_window));
    track_progress = glade_xml_get_widget (glade, "track_progress");
    album_progress = glade_xml_get_widget (glade, "album_progress");
    progress_label = glade_xml_get_widget (glade, "progress_label");
    g_assert (progress_dialog != NULL);
  }
  gtk_widget_show_all (progress_dialog);

  /* Fill pending with a list of all tracks to rip */
  g_list_free (pending);
  total_ripping = 0;
  total_duration = 0;

  g_signal_connect (extractor, "progress", G_CALLBACK(on_progress_cb), NULL);
  g_signal_connect (extractor, "completion", G_CALLBACK(on_completion_cb), NULL);

  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), rip_track_foreach_cb, NULL);
  pop_and_rip();
}
