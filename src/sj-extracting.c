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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> 
#include <errno.h>
#include <stdlib.h>

#include <glib/glist.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtklabel.h>
#include <glade/glade-xml.h>
#include "sj-extracting.h"
#include "sj-util.h"

static GList *pending = NULL; /* a GList of TrackDetails* to rip */
static int total_ripping; /* the number of tracks to rip, not decremented */
static int total_duration, current_duration;
static int track_duration; /* duration of current track for progress dialog */

static GtkWidget *progress_dialog;
static GtkWidget *track_progress, *album_progress;
static GtkWidget *progress_label;
static GtkWidget *extract_button;

/**
 * Build the absolute filename for the specified track.
 *
 * The base path is the extern variable 'base_path', the format to use
 * is the extern variable 'file_pattern'. Free the result when you
 * have finished with it.
 */
static char* build_filename(const TrackDetails *track)
{
  char *realfile, *realpath, *filename, *extension, *path;
  EncoderFormat format;
  realpath = filepath_parse_pattern (path_pattern, track);
  realfile = filepath_parse_pattern (file_pattern, track);
  g_object_get (extractor, "format", &format, NULL);
  switch (format) {
  case VORBIS:
    extension = ".ogg";
    break;
  case MPEG:
    extension = ".mp3";
    break;
  case FLAC:
    extension = ".flac";
    break;
  case WAVE:
    extension = ".wav";
    break;
  default:
    g_free (realpath);
    g_free (realfile);
    g_return_val_if_reached(NULL);
  }
  filename = g_strconcat (realfile, extension, NULL);
  path = g_build_filename (base_path, realpath, filename, NULL);
  g_free (realpath);
  g_free (realfile);
  g_free (filename);
  return path;
}

/**
 * Check if a file exists, can be written to, etc.
 * Return true on continue, false on skip.
 */
static gboolean check_for_file (const char* filename)
{
  struct stat stats;
  int ret;
  GtkWidget *dialog;
  ret = stat (filename, &stats);
  if (ret == -1) {
    if (errno == ENOENT) {
      return TRUE;
    }
    g_warning ("stat failed: %s", g_strerror (errno));
    return FALSE;
  }
  if (stats.st_size < (100*1024)) {
    /* The file exists but is small, assume overwriting */
    return TRUE;
  }
  /* Otherwise the file exists and is large, ask user if they really
     want to overwrite it */
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("A file called '%s' exists, size %ldK.\nDo you want to skip this track or overwrite it?"),
                                   filename, (unsigned long)(stats.st_size / 1024));
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Skip"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("Overwrite"), GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_widget_show_all (dialog);
    ret = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return ret == GTK_RESPONSE_ACCEPT;
}

static void pop_and_rip (void)
{
  TrackDetails *track;
  char *file_path, *directory;
  GError *error = NULL;
  int left;

  if (pending == NULL) {
    GtkWidget *finished_dialog;
    int result;
    
    gtk_widget_hide (progress_dialog);
    
    finished_dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_INFO,
                                              GTK_BUTTONS_NONE,
                                              /* TODO: need to have a better message here */
                                              _("The tracks have been copied successfully."));
    gtk_dialog_add_buttons (GTK_DIALOG (finished_dialog), GTK_STOCK_OPEN, 1, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    result = gtk_dialog_run (GTK_DIALOG (finished_dialog));
    gtk_widget_destroy (finished_dialog);
    if (result == 1) {
      char *command = g_strdup_printf ("nautilus --no-desktop %s", base_path);
      g_spawn_command_line_async (command, NULL);
      g_free (command);
    }
    gtk_widget_set_sensitive (extract_button, TRUE);
    return;
  }

  track = pending->data;
  pending = g_list_remove (pending, track);

  left = total_ripping - (g_list_length (pending) + 1); /* +1 as we've popped already */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (album_progress), (float)left/(float)total_ripping);

  track_duration = track->duration;

  gtk_label_set_text (GTK_LABEL (progress_label), g_strdup_printf (_("Extracting '%s'"), track->title));

  file_path = build_filename (track);

  /* Build the directory structure */
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

  /* See if the file exists */
  if (!check_for_file(file_path)) {
    current_duration += track->duration;
    pop_and_rip ();
    return;
  }

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
  /* Clean up the pending list */
  g_list_free (pending);
  gtk_widget_hide (progress_dialog);
  gtk_widget_set_sensitive (extract_button, TRUE);
  extracting = FALSE;
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
  extract_button = button;
  gtk_widget_set_sensitive (extract_button, FALSE);

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
  total_ripping = 0;
  total_duration = 0;
  current_duration = 0;

  g_signal_connect (extractor, "progress", G_CALLBACK(on_progress_cb), NULL);
  g_signal_connect (extractor, "completion", G_CALLBACK(on_completion_cb), NULL);

  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), rip_track_foreach_cb, NULL);
  pop_and_rip();
}

/*
 * Perform magic on a path to make it safe.
 *
 * This will always replace '/' with ' ', and optionally make the file
 * name shell-friendly. This involves removing [?*\ ] and replacing
 * with '_'.
 *
 * This function manipulates the string in-place, so g_strdup()
 * anything you want to keep safe!
 */
static char* sanitize_path (char* s)
{
  /* Replace path seperators with whitespace */
  g_strdelimit  (s, "/", ' ');
  if (strip_chars) {
    g_strdelimit (s, "\\*?&!:", ' ');
    g_strdelimit (s, " ", '_'); /* TODO: use a different function */
  }
  return s;
}

/**
 * Parse a filename pattern and replace markers with values from a TrackDetails structure.
 *
 * Valid markers so far are:
 * %at -- album title
 * %aa -- album artist
 * %tn -- track number (i.e 8)
 * %tN -- track number, zero padded (i.e 08)
 * %tt -- track title
 * %ta -- track artist
 */
char*
filepath_parse_pattern (const char* pattern, const TrackDetails *track)
{
  /* p is the pattern iterator, i is a general purpose iterator */
  const char *p, *i;
  /* string is the output string, s is the iterator, t is a general string */
  char *string, *s, *temp;
  s = string = g_new0(char, 256); /* TODO: bad hardcoding */

  p = pattern;
  while (*p) {
    /* If not a % marker, copy and continue */
    if (*p != '%') {
      *s++ = *p++;
      /* Explicit increment as we continue past the increment */
      continue;
    }
    /* Is a % marker, go to next and see what to do */
    switch (*++p) {
    case '%':
      /*
       * Literal %
       */
      *s++ = '%';
      break;
    case 'a':
      /*
       * Album tag
       */
      switch (*++p) {
      case 't':
        i = temp = sanitize_path (g_strdup (track->album->title));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'a':
        i = temp = sanitize_path (g_strdup (track->album->artist));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      default:
        *s++ = '%'; *s++ = 'a'; *s++ = *p;
      }
      break;
    case 't':
      /*
       * Track tag
       */
      switch (*++p) {
      case 't':
        i = temp = sanitize_path (g_strdup (track->title));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'a':
        i = temp = sanitize_path (g_strdup (track->artist));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'n':
        /* Track number */
        i = temp = g_strdup_printf ("%d", track->number);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'N':
        /* Track number, zero-padded */
        i = temp = g_strdup_printf ("%02d", track->number);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      default:
        *s++ = '%'; *s++ = 't'; *s++ = *p;
      }
      break;
    default:
      *s++ = '%';
      *s++ = *p;
    }
    ++p;
  }
  *s = '\0';
  return string;
}
