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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sound-juicer.h"

#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <canberra-gtk.h>

#include "sj-error.h"
#include "sj-extracting.h"
#include "sj-util.h"
#include "sj-play.h"
#include "sj-genres.h"
#include "egg-play-preview.h"

typedef struct {
  int seconds;
  struct timeval time;
  int ripped;
  int taken;
} Progress;


typedef enum {
  OVERWRITE_ALL = 1,
  SKIP_ALL = 2,
  NORMAL = 3
} OverWriteModes;

int sj_overwrite_mode;

typedef enum {
  BUTTON_SKIP = 1,
  BUTTON_SKIP_ALL = 2,
  BUTTON_OVERWRITE = 3,
  BUTTON_OVERWRITE_ALL = 4,
  BUTTON_DELETE_EVENT = GTK_RESPONSE_DELETE_EVENT,
} OverwriteDialogResponse;

/* files smaller than this are assumed to be corrupt */
#define MIN_FILE_SIZE 100000


/** If this module has been initialised yet. */
static gboolean initialised = FALSE;

/** If a track has been successfully extracted */
static gboolean successful_extract = FALSE;

/** The progress bar and Status bar */
static GtkWidget *progress_bar, *status_bar;

/** The widgets in the main UI */
static GtkWidget *extract_button, *play_button, *title_entry, *artist_entry, *composer_entry, *genre_entry, *year_entry, *disc_number_entry, *track_listview;

static GtkTreeIter current;

/**
 * A list of paths we have extracted music into. Contains allocated items, free
 * the data and the list when finished.
 */
static GList *paths = NULL;

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
 * Snapshots of the progress used to calculate the speed and the ETA
 */
static Progress before;

/**
* The cookie returned from PowerManagement
*/
static guint cookie;

/**
 * Build the absolute filename for the specified track.
 *
 * The base path is the extern variable 'base_uri', the formats to use are the
 * extern variables 'sj_path_pattern' and 'sj_file_pattern'. Free the result when you
 * have finished with it.
 */
static GFile *
build_filename (const TrackDetails *track, gboolean temp_filename, GError **error)
{
  GFile *uri, *new;
  gchar *realfile, *realpath, *filename, *scheme;
  const gchar *extension;
  int len_extension;
  int max_realfile = INT_MAX;
  GstEncodingProfile *profile;

  g_object_get (sj_extractor, "profile", &profile, NULL);

  realpath = filepath_parse_pattern (sj_path_pattern, track);
  new = g_file_get_child (sj_base_uri, realpath);
  uri = new;
  g_free (realpath);

  if (profile == NULL) {
    g_set_error (error, 0, 0, _("Failed to get output format"));
    return NULL;
  } else {
      gchar *media_type;
      media_type = rb_gst_encoding_profile_get_media_type (profile);
      extension = rb_gst_media_type_to_extension (media_type);
      g_free (media_type);
      gst_encoding_profile_unref (profile);
  }

  len_extension = 1 + (int) strlen (extension);
#if defined(NAME_MAX) && NAME_MAX > 0
  max_realfile = NAME_MAX - len_extension;
#endif /* NAME_MAX */
#if defined(PATH_MAX) && PATH_MAX > 0
  scheme = g_file_get_uri_scheme (uri);
  if (scheme && !strcmp (scheme, "file")) {
    gchar *path = g_file_get_path (uri);
    int len_path = (int) strlen (path) + 1;
    max_realfile = MIN (max_realfile, PATH_MAX - len_path - len_extension);
    g_free (path);
  }
  g_free (scheme);
#endif /* PATH_MAX */
  if (max_realfile <= 0) {
    g_set_error_literal (error, SJ_ERROR, SJ_ERROR_INTERNAL_ERROR, _("Name too long"));
    return NULL;
  }
  realfile = filepath_parse_pattern (sj_file_pattern, track);
  if (temp_filename) {
    filename = g_strdup_printf (".%.*s.%s", max_realfile-1, realfile, extension);
  } else {
    filename = g_strdup_printf ("%.*s.%s", max_realfile, realfile, extension);
  }
  new = g_file_get_child (uri, filename);
  g_object_unref (uri); uri = new;
  g_free (filename); g_free (realfile);

  return uri;
}

static gboolean
find_next (void)
{
  do {
    gboolean extract = FALSE;

    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &current, COLUMN_EXTRACT, &extract, -1);

    if (extract)
      return TRUE;

  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (track_store), &current));
  return FALSE;
}

/**
 * Cleanup the data used, and even enable the Extract button again.
 */
static void
cleanup (void)
{
  /* We're not extracting any more */
  extracting = FALSE;


  gtk_application_uninhibit (GTK_APPLICATION (g_application_get_default ()),
                             cookie);

  /* Remove any state icons from the model */
  if (current.stamp) {
    /* TODO: has to be a better way to do that test */
    gtk_list_store_set (track_store, &current,
                        COLUMN_STATE, STATE_IDLE, -1);
  }

  /* Free the used data */
  if (paths) {
    g_list_free_full (paths, g_free);
    paths = NULL;
  }
  /* Forcibly invalidate the iterator */
  current.stamp = 0;

  gtk_button_set_label (GTK_BUTTON (extract_button), _("E_xtract"));

  /* Clear the Status bar */
  gtk_statusbar_remove_all (GTK_STATUSBAR (status_bar), 2);
  /* Clear the progress bar */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);
  gtk_widget_hide (progress_bar);

  gtk_widget_set_sensitive (play_button, TRUE);
  gtk_widget_set_sensitive (title_entry, TRUE);
  gtk_widget_set_sensitive (artist_entry, TRUE);
  gtk_widget_set_sensitive (composer_entry, TRUE);
  gtk_widget_set_sensitive (genre_entry, TRUE);
  gtk_widget_set_sensitive (year_entry, TRUE);
  gtk_widget_set_sensitive (disc_number_entry, TRUE);
  /* Enabling the Menuitem */
  set_action_enabled ("play", TRUE);
  set_action_enabled ("select-all", TRUE);
  set_action_enabled ("deselect-all", TRUE);
  set_action_enabled ("re-read", TRUE);

  /*Enable the Extract column and Make the Title and Artist column Editable*/
  g_object_set (G_OBJECT (toggle_renderer), "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
  g_object_set (G_OBJECT (title_renderer), "editable", TRUE, NULL);
  g_object_set (G_OBJECT (artist_renderer), "editable", TRUE, NULL);
  g_signal_handlers_unblock_by_func (track_listview, on_tracklist_row_activate, NULL);
}


/**
 * Check if a file exists, can be written to, etc.
 * Return true on continue, false on skip.
 */
static goffset
check_file_size (GFile *uri)
{
  GFileInfo *gfile_info;
  GError *error = NULL;
  goffset size;

  gfile_info = g_file_query_info (uri, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0,
                                  NULL, &error);

  /* No existing file */
  if (!gfile_info && error->code == G_IO_ERROR_NOT_FOUND) {
    g_error_free (error);
    return 0;
  }

  /* unexpected error condition - bad news */
  if (!gfile_info) {
    /* TODO: display an error dialog */
    g_warning ("Cannot get file info: %s", error->message);
    g_error_free (error);
    return -1;
  }

  /* A file with that name does exist. Report the size. */
  size = g_file_info_get_size (gfile_info);
  g_object_unref (gfile_info);
  return size;
}

static gboolean
confirm_overwrite_existing_file (GFile *uri, int *overwrite_mode, goffset info_size)
{
  OverwriteDialogResponse ret;
  GtkWidget *dialog;
  GtkWidget *play_preview;
  char *display_name, *filename, *size;

  display_name = g_file_get_parse_name (uri);
  size = g_format_size (info_size);
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("A file with the same name exists"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("A file called ‘%s’ exists, size %s.\nDo you want to skip this track or overwrite it?"),
                                            display_name, size);
  g_free (display_name);
  g_free (size);

  filename = g_file_get_uri (uri);
  play_preview = egg_play_preview_new_with_uri (filename);
  g_object_set (play_preview,
                "halign", GTK_ALIGN_FILL,
                "margin-start", 30,
                "margin-end", 30, NULL);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), play_preview);
  g_free (filename);

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Skip"), BUTTON_SKIP);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("S_kip All"), BUTTON_SKIP_ALL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Overwrite"), BUTTON_OVERWRITE);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Overwrite _All"), BUTTON_OVERWRITE_ALL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), BUTTON_SKIP);
  gtk_widget_show_all (dialog);
  ret = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  switch (ret) {
    case BUTTON_OVERWRITE_ALL:
      *overwrite_mode = OVERWRITE_ALL;
      return TRUE;
      break;
    case BUTTON_OVERWRITE:
      return TRUE;
      break;
    case BUTTON_SKIP_ALL:
      *overwrite_mode = SKIP_ALL;
      return FALSE;
      break;
    case BUTTON_SKIP:
    case BUTTON_DELETE_EVENT:
    default:
      return FALSE;
      break;
  }
  return ret;
}


/**
 * Find the name of the directory this file is in, create it, and return the
 * directory.
 */
static char*
create_directory_for (GFile *uri, GError **error)
{
  gboolean res;
  GFile *parent;
  char *string;
  GError *io_error = NULL;

  g_return_val_if_fail (uri != NULL, NULL);

  parent = g_file_get_parent (uri);

  res = g_file_make_directory_with_parents (parent, NULL, &io_error);
  if (!res) {
    if (io_error->code != G_IO_ERROR_EXISTS) {
      g_set_error (error, SJ_ERROR, SJ_ERROR_CD_PERMISSION_ERROR,
                   _("Failed to create output directory: %s"),
                   io_error->message);
      g_error_free (io_error);
      return NULL;
    }
    g_error_free (io_error);
  }

  string = g_file_get_uri (parent);
  g_object_unref (parent);
  return string;
}

/* Prototypes for pop_and_extract */
static void on_completion_cb (SjExtractor *extractor, gpointer data);
static void on_error_cb (SjExtractor *extractor, GError *error, gpointer data);

/**
 * The work horse of this file.  Take the first entry from the pending list,
 * update the UI, and start the extractor.
 */
static void
pop_and_extract (int *overwrite_mode)
{
  if (current.stamp == 0) {
    /* TODO: remove this test? */
    g_assert_not_reached ();
  } else {
    TrackDetails *track = NULL;
    char *directory;
    GFile *file = NULL, *temp_file = NULL;
    GError *error = NULL;
    goffset file_size;
    GtkTreePath* path;

    /* Pop the next track to extract */
    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &current, COLUMN_DETAILS, &track, -1);
    /* Build the filename for this track */
    file = build_filename (track, FALSE, &error);
    if (error) {
      goto error;
    }
    temp_file = build_filename (track, TRUE, &error);
    if (error) {
      goto error;
    }
    /* Delete the temporary file as giosink won't overwrite existing files */
    g_file_delete (temp_file, NULL, NULL);

    /* Create the directory it lives in */
    directory = create_directory_for (file, &error);
    if (error) {
      goto error;
    }
    /* Save the directory name for later */
    paths = g_list_append (paths, directory);

    file_size = check_file_size (file);

    /* Skip if destination file can't be accessed (unexpected error). */
    /* Skip existing files if "skip all" is selected. */
    if ((file_size == -1) ||
        ((file_size > MIN_FILE_SIZE) && (*overwrite_mode == SKIP_ALL))) {
      successful_extract = FALSE;
      on_completion_cb (NULL, overwrite_mode);
      return;
    }

    /* What if the file already exists? */
    if ((file_size > MIN_FILE_SIZE) &&
        (*overwrite_mode != OVERWRITE_ALL) &&
        (confirm_overwrite_existing_file (file, overwrite_mode, file_size) == FALSE)) {
      successful_extract = FALSE;
      on_completion_cb (NULL, overwrite_mode);
      return;
    }

    /* OK, we can write/overwrite the file */


    /* Update the state image */
    gtk_list_store_set (track_store, &current,
                   COLUMN_STATE, STATE_EXTRACTING, -1);

    /* Update the progress bars */
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar),
                                   CLAMP ((float)current_duration / (float)total_duration, 0.0, 1.0));

    /* Set the Treelist focus to the item to be extracted */
    path = gtk_tree_model_get_path(GTK_TREE_MODEL (track_store), &current);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW (track_listview), path, NULL, TRUE);
    gtk_tree_path_free(path);

    /* Now actually do the extraction */
    sj_extractor_extract_track (sj_extractor, track, temp_file, &error);
    if (error) {
      goto error;
    } else
        successful_extract = TRUE;
    goto local_cleanup;
error:
    successful_extract = FALSE;
    on_error_cb (NULL, error, NULL);
    g_error_free (error);
local_cleanup:
    g_object_unref (file);
    g_object_unref (temp_file);
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
    ++total_extracting;
    total_duration += track->duration;
  }
  return FALSE;
}

/**
 * Update the ETA and Speed labels
 */
static void
update_speed_progress (SjExtractor *extractor, float speed, int eta)
{
  char *eta_str;

  if (eta >= 0) {
    eta_str = g_strdup_printf (_("Estimated time left: %d:%02d (at %0.1f\303\227)"), eta / 60, eta % 60, speed);
  } else {
    eta_str = g_strdup (_("Estimated time left: unknown"));
  }

  gtk_statusbar_push (GTK_STATUSBAR (status_bar), 2, eta_str);
  g_free (eta_str);
}

/**
 * Callback from SjExtractor to report progress.
 */
static void
on_progress_cb (SjExtractor *extractor, const int seconds, gpointer data)
{
  /* Album progress */
  if (total_duration != 0) {
    float percent;
    percent = CLAMP ((float)(current_duration + seconds) / (float)total_duration, 0, 1);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), percent);

    if (before.seconds == -1) {
      before.seconds = current_duration + seconds;
      gettimeofday(&before.time, NULL);
    } else {
      struct timeval time;
      int taken;
      float speed;

      gettimeofday(&time, NULL);
      taken = time.tv_sec + (time.tv_usec / 1000000.0)
        - (before.time.tv_sec + (before.time.tv_usec / 1000000.0));
      if (taken >= 4) {
        before.taken += taken;
        before.ripped += current_duration + seconds - before.seconds;
        speed = (float) before.ripped / (float) before.taken;
        update_speed_progress (extractor, speed, (int) ((total_duration - current_duration - seconds) / speed));
        before.seconds = current_duration + seconds;
        gettimeofday(&before.time, NULL);
      }
    }
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

static gboolean
on_main_window_focus_in (GtkWidget * widget, GdkEventFocus * event, gpointer data)
{
  gtk_window_set_urgency_hint (GTK_WINDOW (main_window), FALSE);
  return FALSE;
}

/**
 * Handle any post-rip actions
 */
static void
finished_actions (void)
{
  /* Trigger a sound effect */
  ca_gtk_play_for_widget (main_window, 0,
    CA_PROP_EVENT_ID, "complete-media-rip",
    CA_PROP_EVENT_DESCRIPTION, _("CD rip complete"),
    NULL);

  /* Trigger glowing effect after copy */
  g_signal_connect (G_OBJECT (main_window), "focus-in-event",
                    G_CALLBACK (on_main_window_focus_in),  NULL);
  gtk_window_set_urgency_hint (GTK_WINDOW (main_window), TRUE);

  /* Maybe eject */
  if (eject_finished && successful_extract) {
    udisks_drive_call_eject_sync (sj_drive, NULL, NULL, NULL);
  }

  /* Maybe open the target directory */
  if (open_finished) {
    char *base = NULL;

    /* Find the deepest common directory. */
    g_list_foreach (paths, (GFunc)base_finder, &base);

    gtk_show_uri_on_window (NULL, base, GDK_CURRENT_TIME, NULL);

    g_free (base);
  }
}

/**
 * Callback from SjExtractor to report completion.
 */
static void
on_completion_cb (SjExtractor *extractor, gpointer data)
{
  TrackDetails *track = NULL;
  GFile *temp_file, *new_file;
  GError *error = NULL;

  /* Only manipulate the track state if we have an album, as we might be here if
     the disk was ejected mid-rip. */
  if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (track_store), NULL) > 0) {
    /* Remove the track state */
    gtk_list_store_set (track_store, &current, COLUMN_STATE, STATE_IDLE, -1);
    /* Uncheck the Extract check box */
    gtk_list_store_set (track_store, &current, COLUMN_EXTRACT, FALSE, -1);
  }

  gtk_tree_model_get (GTK_TREE_MODEL (track_store), &current,
                      COLUMN_DETAILS, &track, -1);



    temp_file = build_filename (track, TRUE, NULL);
    new_file = build_filename (track, FALSE, NULL);
    /* We could be here because the user skipped an overwrite, in which case temp_file won't exist */
    if (g_file_query_exists (temp_file, NULL))
        g_file_move (temp_file, new_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);

    g_object_unref (temp_file);
    g_object_unref (new_file);

  if (error) {
    on_error_cb (NULL, error, NULL);
    g_error_free (error);
  } else if (find_next ()) {
    /* Increment the duration */
    current_duration += track->duration;
    /* And go and do it all again */
    pop_and_extract ((int*)data);
  } else {
    /* If we got here then the track state has been set to IDLE already, so
       unset the current iterator */
    current.stamp = 0;
    finished_actions ();
    cleanup ();

    if (autostart) {
      gtk_main_quit ();
    }
  }
}

/**
 * Callback from SjExtractor to report errors.
 */
static void
on_error_cb (SjExtractor *extractor, GError *error, gpointer data)
{
  GtkWidget *dialog;

  /* Display a nice dialog */
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", _("Sound Juicer could not extract this CD."));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s: %s", _("Reason"), error->message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* No need to free the error passed in */
  cleanup ();
}

/**
 * Cancel in the progress dialog clicked or progress dialog has been closed.
 */
void
on_progress_cancel_clicked (GtkWidget *button, gpointer user_data)
{
  TrackDetails *track = NULL;
  GFile *file;
  GError *error = NULL;

  sj_extractor_cancel_extract (sj_extractor);

  gtk_tree_model_get (GTK_TREE_MODEL (track_store), &current,
                      COLUMN_DETAILS, &track, -1);

  file = build_filename (track, TRUE, NULL);
  g_file_delete (file, NULL, &error);
  g_object_unref (file);

  if (error) {
    on_error_cb (NULL, error, NULL);
    g_error_free (error);
  } else {
    cleanup ();
  }
}

/**
 * Entry point from the interface.
 */
G_MODULE_EXPORT void
on_extract_activate (GtkWidget *button, gpointer user_data)
{
  /* first make sure we're not playing, we cannot share the resource */
  stop_playback ();

  /* If extracting, then cancel the extract */
  if (extracting) {
     on_progress_cancel_clicked (NULL, NULL);
     return;
  }

  /* Populate the pending list */
  current.stamp = 0;
  total_extracting = 0;
  current_duration = total_duration = 0;
  before.seconds = -1;
  sj_overwrite_mode = NORMAL;
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), extract_track_foreach_cb, NULL);
  /* If the pending list is still empty, return */
  if (total_extracting == 0) {
    /* Should never reach here */
    g_warning ("No tracks selected for extracting");
    return;
  }

  /* Initialise ourself */
  if (!initialised) {
    /* Connect to the SjExtractor signals */
    g_signal_connect (sj_extractor, "progress", G_CALLBACK (on_progress_cb), NULL);
    g_signal_connect (sj_extractor, "completion", G_CALLBACK (on_completion_cb), (gpointer)&sj_overwrite_mode);
    g_signal_connect (sj_extractor, "error", G_CALLBACK (on_error_cb), NULL);

    extract_button    = GET_WIDGET ("extract_button");
    play_button       = GET_WIDGET ("play_button");
    title_entry       = GET_WIDGET ("title_entry");
    artist_entry      = GET_WIDGET ("artist_entry");
    composer_entry    = GET_WIDGET ("composer_entry");
    genre_entry       = GET_WIDGET ("genre_entry");
    year_entry        = GET_WIDGET ("year_entry");
    disc_number_entry = GET_WIDGET ("disc_number_entry");
    track_listview    = GET_WIDGET ("track_listview");
    progress_bar      = GET_WIDGET ("progress_bar");
    status_bar        = GET_WIDGET ("status_bar");

    initialised = TRUE;
  }

  /* Change the label to Stop while extracting*/
  gtk_button_set_label (GTK_BUTTON (extract_button), _("_Stop"));
  gtk_widget_show (progress_bar);

  /* Reset the progress dialog */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);
  update_speed_progress (NULL, 0.0, -1);

  /* Disable the widgets in the main UI*/
  gtk_widget_set_sensitive (play_button, FALSE);
  gtk_widget_set_sensitive (title_entry, FALSE);
  gtk_widget_set_sensitive (artist_entry, FALSE);
  gtk_widget_set_sensitive (composer_entry, FALSE);
  gtk_widget_set_sensitive (genre_entry, FALSE);
  gtk_widget_set_sensitive (year_entry, FALSE);
  gtk_widget_set_sensitive (disc_number_entry, FALSE);

  /* Disable the menuitems in the main menu*/
  set_action_enabled ("play", FALSE);
  set_action_enabled ("select-all", FALSE);
  set_action_enabled ("deselect-all", FALSE);
  set_action_enabled ("re-read", FALSE);

  /* Disable the Extract column */
  g_object_set (G_OBJECT (toggle_renderer), "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
  g_object_set (G_OBJECT (title_renderer), "editable", FALSE, NULL);
  g_object_set (G_OBJECT (artist_renderer), "editable", FALSE, NULL);
  g_signal_handlers_block_by_func (track_listview, on_tracklist_row_activate, NULL);

  if (! udisks_drive_get_media_available (sj_drive)) {
    g_warning ("Could not lock drive: media unavailable");
  }

  cookie = gtk_application_inhibit (GTK_APPLICATION (g_application_get_default ()),
                                    GTK_WINDOW (main_window),
                                    GTK_APPLICATION_INHIBIT_SUSPEND | GTK_APPLICATION_INHIBIT_SWITCH,
                                    _("Extracting audio from CD"));


  /* Save the genre */
  save_genre (genre_entry);

  /* Start the extracting */
  extracting = TRUE;
  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (track_store), &current);
  find_next ();
  pop_and_extract (&sj_overwrite_mode);
}

/*
 * TODO: These should be moved somewhere else, probably with build_pattern at
 * the top, into sj-patterns.[ch] or something.
 */

/**
 * Perform magic on a path to make it safe.
 *
 * This will always replace '/' with ' ', and optionally make the file name
 * shell-friendly. This involves removing [?*\ ] and replacing with '_'.  Also
 * any leading periods are removed so that the files don't end up being hidden.
 *
 * This function doesn't change the input, and returns an allocated
 * string.
 */
static char*
sanitize_path (const char* str, const char* filesystem_type)
{
  gchar *res = NULL;
  gchar *s;

  g_return_val_if_fail (str != NULL, NULL);

  /* Skip leading periods, otherwise files disappear... */
  while (*str == '.')
    str++;

  s = g_strdup(str);
  /* Replace path seperators with a hyphen */
  g_strdelimit (s, "/", '-');

  /* filesystem specific sanitizing */
  if (filesystem_type) {
    if ((strcmp (filesystem_type, "vfat") == 0) ||
        (strcmp (filesystem_type, "ntfs") == 0)) {
      g_strdelimit (s, "\\:*?\"<>|", ' ');
    }
  }

  if (strip_chars) {
    /* Replace separators with a hyphen */
    g_strdelimit (s, "\\:|", '-');
    /* Replace all other weird characters to whitespace */
    g_strdelimit (s, "*?&!\'\"$()`>{}[]<>", ' ');
    /* Replace all whitespace with underscores */
    /* TODO: I'd like this to compress whitespace aswell */
    g_strdelimit (s, "\t ", '_');
  }
  res = g_filename_from_utf8(s, -1, NULL, NULL, NULL);
  g_free(s);
  return res ? res : g_strdup(str);
}

/*
 * Return sanitized name or default if name is empty
 */
static char*
sanitize_name (const char *name, const char *default_name,
               const char *filesystem_type)
{
  const char *n = (sj_str_is_empty (name)) ? default_name : name;
  return sanitize_path (n, filesystem_type);
}

/*
 * Return lowercase sanitized name or default if name is empty
 */
static char*
lower_sanitize_name (const char *name, const char *default_name,
                     const char *filesystem_type)
{
  char *tmp, *s;
  const char *n = (sj_str_is_empty (name)) ? default_name : name;
  tmp = g_utf8_strdown (n, -1);
  s = sanitize_path (tmp, filesystem_type);
  g_free (tmp);
  return s;
}

/*
 * Return sanitized sortname or name if sortname is empty or default
 * if name is empty
 */
static char*
sanitize_sortname (const char *sortname, const char *name,
                   const char *default_name, const char *filesystem_type)
{
  const char *n = (sj_str_is_empty (sortname)) ? name : sortname;
  return sanitize_name (n, default_name, filesystem_type);
}

/*
 * Return lowercase sanitized sortname or name if sortname is empty or
 * default if name is empty
 */
static char*
lower_sanitize_sortname (const char *sortname, const char *name,
                         const char *default_name, const char *filesystem_type)
{
  const char *n = (sj_str_is_empty (sortname)) ? name : sortname;
  return lower_sanitize_name (n, default_name, filesystem_type);
}

/**
 * Parse a filename pattern and replace markers with values from a TrackDetails
 * structure.
 *
 * Valid markers so far are:
 * %at -- album title
 * %ay -- album year
 * %aa -- album artist
 * %aA -- album artist (lowercase)
 * %as -- album artist sortname
 * %aS -- album artist sortname (lowercase)
 * %ac -- album composer
 * %aC -- album composer (lowercase)
 * %ap -- album composer (sortable)
 * %aP -- album composer (sortable lowercase)
 * %tn -- track number (i.e 8)
 * %tN -- track number, zero padded (i.e 08)
 * %tt -- track title
 * %tT -- track title (lowercase)
 * %ta -- track artist
 * %tA -- track artist (lowercase)
 * %ts -- track artist sortname
 * %tS -- track artist sortname (lowercase)
 * %tc -- track composer
 * %tC -- track composer (lowercase)
 * %tp -- track composer (sortable)
 * %tP -- track composer (sortable lowercase)
 * %dn -- disc and track number, track zero padded (i.e Disk 2 - 06, or 06)
 * %dN -- condensed disc and track number, zero padded (i.e d02t06, or 06)
 */
char*
filepath_parse_pattern (const char* pattern, const TrackDetails *track)
{
  /* p is the pattern iterator, i is a general purpose iterator */
  const char *p;
  char *tmp, *str, *filesystem_type = NULL;
  GString *s;
  GFileInfo *fs_info;
  const char *default_album    = _("Unknown Album");
  const char *default_artist   = _("Unknown Artist");
  const char *default_composer = _("Unknown Composer");
  const char *default_track    = _("Unknown Track");

  if (pattern == NULL || pattern[0] == 0)
    return g_strdup (" ");

  fs_info = g_file_query_filesystem_info (sj_base_uri, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
                                          NULL, NULL);
  if (fs_info) {
    filesystem_type = g_file_info_get_attribute_as_string (fs_info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
    g_object_unref (fs_info);
  }

  s = g_string_new (NULL);

  p = pattern;
  while (*p) {
    char *string = NULL;
    gboolean go_next = TRUE;

    /* If not a % marker, copy and continue */
    if (*p != '%') {
      if ((*p == ' ') && (strip_chars)) {
        g_string_append_c (s, '_');
      } else {
        g_string_append_unichar (s, g_utf8_get_char (p));
      }
      p = g_utf8_next_char (p);
      /* Explicit increment as we continue past the increment */
      continue;
    }

    /* Is a % marker, go to next and see what to do */
    switch (*++p) {
    case '%':
      /*
       * Literal %
       */
      g_string_append_c (s, '%');
      break;
    case 'a':
      /*
       * Album tag
       */
      switch (*++p) {
      case 't':
        string = sanitize_name (track->album->title, default_album,
                                filesystem_type);
        break;
      case 'y':
        if (track->album->release_date && gst_date_time_has_year (track->album->release_date)) {
          tmp = g_strdup_printf ("%d", gst_date_time_get_year (track->album->release_date));
          string = sanitize_path (tmp, filesystem_type);
          g_free (tmp);
        }
        break;
      case 'T':
        string = lower_sanitize_name (track->album->title, default_album,
                                      filesystem_type);
        break;
      case 'a':
        string = sanitize_name (track->album->artist, default_artist,
                                filesystem_type);
        break;
      case 'A':
        string = lower_sanitize_name (track->album->artist, default_artist,
                                      filesystem_type);
        break;
      case 's':
        string = sanitize_sortname (track->album->artist_sortname,
                                    track->album->artist, default_artist,
                                    filesystem_type);
        break;
      case 'S':
        string = lower_sanitize_sortname (track->album->artist_sortname,
                                          track->album->artist, default_artist,
                                          filesystem_type);
        break;
      case 'c':
        string = sanitize_name (track->album->composer, default_composer,
                                filesystem_type);
        break;
      case 'C':
        string = lower_sanitize_name (track->album->composer, default_composer,
                                      filesystem_type);
        break;
      case 'p':
        string = sanitize_sortname (track->album->composer_sortname,
                                    track->album->composer, default_composer,
                                    filesystem_type);
        break;
      case 'P':
        string = lower_sanitize_sortname (track->album->composer_sortname,
                                          track->album->composer,
                                          default_composer, filesystem_type);
        break;
      default:
        /* append "%a", and then the unicode character */
        g_string_append (s, "%a");
        p += 2;

        g_string_append_unichar (s, g_utf8_get_char (p));
        p = g_utf8_next_char (p);
        go_next = FALSE;
      }
      break;
    case 't':
      /*
       * Track tag
       */
      switch (*++p) {
      case 't':
        string = sanitize_name (track->title, default_track, filesystem_type);
        break;
      case 'T':
        string = lower_sanitize_name (track->title, default_track,
                                      filesystem_type);
        break;
      case 'a':
        string = sanitize_name (track->artist, default_artist, filesystem_type);
        break;
      case 'A':
        string = lower_sanitize_name (track->artist, default_artist,
                                      filesystem_type);
        break;
      case 's':
        string = sanitize_sortname (track->artist_sortname, track->artist,
                                    default_artist, filesystem_type);
        break;
      case 'S':
        string = lower_sanitize_sortname (track->artist_sortname, track->artist,
                                          default_artist, filesystem_type);
        break;
      case 'c':
         string = sanitize_name (track->composer, default_composer,
                                 filesystem_type);
        break;
      case 'C':
        string = lower_sanitize_name (track->composer, default_composer,
                                      filesystem_type);
        break;
      case 'p':
        string = sanitize_sortname (track->composer_sortname, track->composer,
                                    default_composer, filesystem_type);
        break;
      case 'P':
        string = lower_sanitize_sortname (track->composer_sortname,
                                          track->composer, default_composer,
                                          filesystem_type);
        break;
      case 'n':
        /* Track number */
        string = g_strdup_printf ("%d", track->number);
        break;
      case 'N':
        /* Track number, zero-padded */
        string = g_strdup_printf ("%02d", track->number);
        break;
      default:
        /* append "%a", and then the unicode character */
        g_string_append (s, "%t");
        p += 2;

        g_string_append_unichar (s, g_utf8_get_char (p));
        p = g_utf8_next_char (p);
        go_next = FALSE;
      }
      break;
    case 'd':
      /*
       * Disc and track tag
       */
      switch (*++p) {
      case 'n':
        /* Disc and track number */
        if (track->album->disc_number > 0) {
          char *dtn = g_strdup_printf ("Disc %d - %02d", track->album->disc_number, track->number);
          string = sanitize_path (dtn, filesystem_type); /* strip spaces if required */
          g_free (dtn);
        } else {
          string = g_strdup_printf ("%d", track->number);
        }
        break;
      case 'N':
        /* Disc and track number, zero padded */
        if (track->album->disc_number > 0) {
          int width;

          if (track->album->disc_count < 10)
            width = 1;
          else if (track->album->disc_count < 100)
            width = 2;
          else
            width = 3;
          string = g_strdup_printf ("d%0*dt%02d", width, track->album->disc_number, track->number);
        } else {
          string = g_strdup_printf ("%02d", track->number);
        }
        break;
      default:
        g_string_append (s, "%d");
        p += 2;

        g_string_append_unichar (s, g_utf8_get_char (p));
        p = g_utf8_next_char (p);
        go_next = FALSE;
      }
      break;
    default:
      /* append "%", and then the unicode character */
      g_string_append_c (s, '%');
      p += 1;

      g_string_append_unichar (s, g_utf8_get_char (p));
      p = g_utf8_next_char (p);
    }

    if (string)
      g_string_append (s, string);
    g_free (string);

    if (go_next)
      ++p;
  }

  g_free (filesystem_type);

  str = s->str;
  g_string_free (s, FALSE);
  return str;
}
