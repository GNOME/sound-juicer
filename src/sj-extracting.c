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
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreemodel.h>

#include "sj-extracting.h"
#include "sj-util.h"

/** If this module has been initialised yet. */
static gboolean initialised = FALSE;

/** The progress dialog. */
static GtkWidget *progress_dialog;

/** The track and album progress bars. */
static GtkWidget *track_progress, *album_progress;

/** The progress label. */
static GtkWidget *progress_label;

/** The extract and rereda buttons in the UI */
static GtkWidget *extract_button, *reread_button;

/** The current TrackDetails being extracted. */
static TrackDetails *track;

/**
 * The list of pending TrackDetails to extract. ->data points to existing
 * entries, so do not free the data, only the list.
 */
static GList *pending;

/**
 * A list of paths we have extracted music into. Contains allocated items, free
 * the data and the list when finished.
 */
static GList *paths;

/**
 * The total number of tracks we are extracting.
 */
static int total_extracting;

/**
 * The duration of the extracted tracks, only used so the album progress
 * displays inter-track progress.
 */

static int current_duration;
/**
 * The total duration of the tracks we are ripping.
 */
static int total_duration;

/**
 * Build the absolute filename for the specified track.
 *
 * The base path is the extern variable 'base_path', the format to use
 * is the extern variable 'file_pattern'. Free the result when you
 * have finished with it.
 */
static char*
build_filename (const TrackDetails *track)
{
  char *realfile, *realpath, *filename, *extension, *path;
  EncoderFormat format;
  realpath = filepath_parse_pattern (path_pattern, track);
  realfile = filepath_parse_pattern (file_pattern, track);
  g_object_get (extractor, "format", &format, NULL);
  switch (format) {
  case SJ_FORMAT_VORBIS:
    extension = ".ogg";
    break;
  case SJ_FORMAT_MPEG:
    extension = ".mp3";
    break;
  case SJ_FORMAT_FLAC:
    extension = ".flac";
    break;
  case SJ_FORMAT_WAVE:
    extension = ".wav";
    break;
  default:
    g_free (realpath);
    g_free (realfile);
    g_return_val_if_reached (NULL);
  }
  filename = g_strconcat (realfile, extension, NULL);
  path = g_build_filename (base_path, realpath, filename, NULL);
  g_free (realpath);
  realpath = g_filename_from_utf8 (path, -1, NULL, NULL, NULL);
  g_free (path);
  g_free (realfile);
  g_free (filename);
  return realpath;
}

/**
 * Cleanup the data used, and even enable the Extract button again.
 */
static void
cleanup (void)
{
  /* Free the used data */
  if (paths) {
    g_list_deep_free (paths, NULL);
  }
  g_list_free (pending);
  pending = NULL;
  track = NULL;
  gtk_widget_set_sensitive (extract_button, TRUE);
  gtk_widget_set_sensitive (reread_button, TRUE);
}

/**
 * Check if a file exists, can be written to, etc.
 * Return true on continue, false on skip.
 */
static gboolean
check_for_file (const char* filename)
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
  if (stats.st_size < (100000)) {
    /* The file exists but is small, assume overwriting */
    return TRUE;
  }
  /* Otherwise the file exists and is large, ask user if they really
     want to overwrite it */
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("A file called '%s' exists, size %luKB.\nDo you want to skip this track or overwrite it?"),
                                   filename, (unsigned long)(stats.st_size / 1000));
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Skip"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Overwrite"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_widget_show_all (dialog);
  ret = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  return ret == GTK_RESPONSE_ACCEPT;
}

/**
 * Find the name of the directory this file is in, create it, and return the
 * directory.
 */
static char*
create_directory_for (const char* filename, GError **error)
{
  char *directory;
  g_return_val_if_fail (filename != NULL, NULL);

  directory = g_path_get_dirname (filename);
  if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
    mkdir_recursive (directory, 0750, error);
    if (error) {
      return NULL;
    }
  }
  return directory;
}

/**
 * The work horse of this file.  Take the first entry from the pending list,
 * update the UI, and start the extractor.
 */
static void
pop_and_extract (void)
{
  if (pending == NULL) {
    g_assert_not_reached ();
  } else {
    char *file_path, *directory;
    GError *error = NULL;
    
    /* Pop the next track to extract */
    track = pending->data;
    pending = g_list_remove (pending, track);
    /* Build the filename for this track */
    file_path = build_filename (track);
    /* And create the directory it lives in */
    directory = create_directory_for (file_path, &error);
    if (error) {
      GtkWidget *dialog;
      gtk_widget_hide (progress_dialog);
      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       g_strdup_printf ("Sound Juicer could not extract this CD.\nReason: %s", error->message));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
      cleanup ();
      return;
    }
    /* Save the directory name for later */
    paths = g_list_append (paths, directory);
    /* See if the file exists */
    if (!check_for_file (file_path)) {
      current_duration += track->duration;
      pop_and_extract ();
      return;
    }

    /* Update the progress bars */
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (track_progress), 0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (album_progress),
                                   CLAMP (1.0 - ((g_list_length (pending) + 1)  / (float)total_extracting), 0.0, 1.0));
    gtk_label_set_text (GTK_LABEL (progress_label), g_strdup_printf (_("Extracting '%s'"), track->title));

    /* Now actually do the extraction */
    sj_extractor_extract_track (extractor, track, file_path, &error);
    if (error) {
      GtkWidget *dialog;
      gtk_widget_hide (progress_dialog);
      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       g_strdup_printf ("Sound Juicer could not extract this CD.\nReason: %s", error->message));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
      cleanup ();
      return;
    }
    g_free (file_path);
  }
}

/**
 * Foreach callback to populate pending with the list of TrackDetails to
 * extract.
 */
static gboolean
extract_track_foreach_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  gboolean extract;
  TrackDetails *track;

  gtk_tree_model_get (model, iter,
                      COLUMN_EXTRACT, &extract,
                      COLUMN_DETAILS, &track,
                      -1);
  if (extract) {
    pending = g_list_append (pending, track);
    ++total_extracting;
    total_duration += track->duration;
  }
  return FALSE;
}

/**
 * Callback from SjExtractor to report progress.
 */
static void
on_progress_cb (SjExtractor *extractor, const int seconds, gpointer data)
{
  /* Track progress */
  if (track->duration != 0) {
    float percent;
    percent = CLAMP ((float)seconds / (float)track->duration, 0, 1);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (track_progress), percent);
  } else {
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (track_progress));
  }

  /* Album progress */
  if (total_duration != 0) {
    float percent;
    percent = CLAMP ((float)(current_duration + seconds) / (float)total_duration, 0, 1);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (album_progress), percent);
  }
}

/**
 * A list foreach function which will find the deepest common directory in a
 * list of filenames.
 * @param path the path in this iteration
 * @param ret a char** to the deepest common path
 */
static void
base_finder (char *path, char **ret)
{
  if (*ret == NULL) {
    /* If no common directory so far, this must be it. */
    *ret = g_strdup (path);
    return;
  } else {
    /* Urgh */
    char *i, *j, *marker;
    i = marker = path;
    j = *ret;
    while (*i == *j) {
      if (*i == G_DIR_SEPARATOR) marker = i;
      if (*i == 0) {
        marker = i;
        break;
      }
      i = g_utf8_next_char (i);
      j = g_utf8_next_char (j);
    }
    g_free (*ret);
    *ret = g_strndup (path, marker - path + 1);
  }
}

/**
 * Show the "Finished!" dialog, allowing the user to open Nautilus if he wants.
 */
static void
show_finished_dialog (void)
{
  GtkWidget *dialog;
  int result;
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_NONE,
                                   /* TODO: need to have a better message here */
                                   _("The tracks have been copied successfully."));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_OPEN, 1,
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                          NULL);
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  if (result == 1) {
    char *base, *command;
    base = NULL;
    /* Find the deepest common directory. */
    g_list_foreach (paths, (GFunc)base_finder, &base);
    /* Construct a Nautilus command line */
    /* TODO: don't think I need --no-desktop here */
    command = g_strdup_printf ("nautilus --no-desktop \'%s\'", base);
    g_spawn_command_line_async (command, NULL);
    g_free (base);
    g_free (command);
  }
}

/**
 * Callback from SjExtractor to report completion.
 */
static void
on_completion_cb (SjExtractor *extractor, gpointer data)
{
  /* TODO: uncheck the relevant check box */
  if (pending == NULL) {
    gtk_widget_hide (progress_dialog);
    show_finished_dialog ();
    cleanup ();
    extracting = FALSE;
    if (autostart) {
      gtk_main_quit ();
    }
  } else {
    /* Increment the duration */
    current_duration += track->duration;
    /* And go and do it all again */
    pop_and_extract ();
  }
}

/**
 * Callback from SjExtractor to report errors.
 */
static void
on_error_cb (SjExtractor *extractor, GError *error, gpointer data)
{
  GtkWidget *dialog;
  /* We've come to a screeching halt... */
  gtk_widget_hide (progress_dialog);
  /* Display a nice dialog */
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   g_strdup_printf ("Sound Juicer could not extract this CD.\nReason: %s", error->message));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  
  g_error_free (error);
  cleanup ();
  extracting = FALSE;
}

/**
 * Cancel in the progress dialog clicked or progress dialog has been closed.
 */
void
on_progress_cancel_clicked (GtkWidget *button, gpointer user_data)
{
  sj_extractor_cancel_extract (extractor);
  cleanup ();
  gtk_widget_hide (progress_dialog);
  extracting = FALSE;
}

/**
 * Entry point from the interface.
 */
void
on_extract_activate (GtkWidget *button, gpointer user_data)
{
  /* Populate the pending list */
  pending = NULL;
  total_extracting = 0;
  current_duration = total_duration = 0;
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), extract_track_foreach_cb, NULL);
  /* If the pending list is still empty, return */
  if (pending == NULL) {
    /* Should never reach here */
    g_warning ("No tracks selected for extracting\n");
    return;
  }

  /* Initialise ourself */
  if (!initialised) {
    /* Connect to the SjExtractor signals */
    g_signal_connect (extractor, "progress", G_CALLBACK (on_progress_cb), NULL);
    g_signal_connect (extractor, "completion", G_CALLBACK (on_completion_cb), NULL);
    g_signal_connect (extractor, "error", G_CALLBACK (on_error_cb), NULL);

    /* Get the progress dialog */
    progress_dialog = glade_xml_get_widget (glade, "progress_dialog");
    g_assert (progress_dialog != NULL);
    gtk_window_set_transient_for (GTK_WINDOW (progress_dialog), GTK_WINDOW (main_window));
    track_progress = glade_xml_get_widget (glade, "track_progress");
    album_progress = glade_xml_get_widget (glade, "album_progress");
    progress_label = glade_xml_get_widget (glade, "progress_label");
    extract_button = glade_xml_get_widget (glade, "extract_button");
    reread_button = glade_xml_get_widget (glade, "reread_button");
    /* TODO : this callback should be in the glade file */
    g_signal_connect (progress_dialog, "delete-event", G_CALLBACK (on_progress_cancel_clicked), NULL);

    initialised = TRUE;
  }

  /* Reset the progress dialog */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (track_progress), 0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (album_progress), 0);
  gtk_widget_show_all (progress_dialog);

  /* Disable some buttons */
  gtk_widget_set_sensitive (extract_button, FALSE);
  gtk_widget_set_sensitive (reread_button, FALSE);

  /* Start the extracting */
  extracting = TRUE;
  pop_and_extract ();
}

/*
 * TODO: These should be moved somewhere else, probably with build_pattern at
 * the top, into sj-patterns.[ch] or something.
 */

/**
 * Perform magic on a path to make it safe.
 *
 * This will always replace '/' with ' ', and optionally make the file
 * name shell-friendly. This involves removing [?*\ ] and replacing
 * with '_'.
 *
 * This function manipulates the string in-place, so g_strdup()
 * anything you want to keep safe!
 */
static char*
sanitize_path (char* s)
{
  /* Replace path seperators with whitespace */
  g_strdelimit (s, "/", ' ');
  if (strip_chars) {
    /* Mangle all weird characters to whitespace */
    g_strdelimit (s, "\\*?&!:", ' ');
    /* Replace all whitespace with underscores */
    /* TODO: I'd like this to compress whitespace aswell */
    g_strdelimit (s, "\t ", '_');
  }
  return s;
}

/**
 * Parse a filename pattern and replace markers with values from a TrackDetails
 * structure.
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
  s = string = g_new0 (char, 256); /* TODO: bad hardcoding */

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
      case 'T':
        i = temp = sanitize_path (g_utf8_strdown (track->album->title, -1));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'a':
        i = temp = sanitize_path (g_strdup (track->album->artist));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'A':
        i = temp = sanitize_path (g_utf8_strdown (track->album->artist, -1));
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
      case 'T':
        i = temp = sanitize_path (g_utf8_strdown (track->title, -1));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'a':
        i = temp = sanitize_path (g_strdup (track->artist));
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'A':
        i = temp = sanitize_path (g_utf8_strdown (track->artist, -1));
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
