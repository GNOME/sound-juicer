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

#include <sys/time.h>
#include <time.h>

#include <glib/glist.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreemodel.h>

#include "sj-error.h"
#include "sj-extracting.h"
#include "sj-util.h"
#include "sj-play.h"

typedef struct {
  int seconds;
  struct timeval time;
  int ripped;
  int taken;
} Progress;

/** If this module has been initialised yet. */
static gboolean initialised = FALSE;

/** The progress bar and Status bar */
static GtkWidget *progress_bar, *status_bar;

/** The widgets in the main UI */
static GtkWidget *extract_button, *play_button, *title_entry, *artist_entry, *genre_entry, *track_listview;

/** The menuitem in the main menu */
static GtkWidget *extract_menuitem, *play_menuitem, *reread_menuitem, *select_all_menuitem, *deselect_all_menuitem;

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
 * Build the absolute filename for the specified track.
 *
 * The base path is the extern variable 'base_path', the format to use
 * is the extern variable 'file_pattern'. Free the result when you
 * have finished with it.
 */
static char*
build_filename (const TrackDetails *track)
{
  GnomeVFSURI *uri, *new;
  char *realfile, *realpath, *filename, *string;
  GMAudioProfile *profile;

  g_object_get (extractor, "profile", &profile, NULL);

  uri = gnome_vfs_uri_new (base_uri);

  realpath = filepath_parse_pattern (path_pattern, track);
  new = gnome_vfs_uri_append_path (uri, realpath);
  gnome_vfs_uri_unref (uri); uri = new;
  g_free (realpath);

  realfile = filepath_parse_pattern (file_pattern, track);
  filename = g_strdup_printf("%s.%s", realfile, gm_audio_profile_get_extension(profile));
  new = gnome_vfs_uri_append_file_name (uri, filename);
  gnome_vfs_uri_unref (uri); uri = new;
  g_free (filename); g_free (realfile);

  string = gnome_vfs_uri_to_string (uri, 0);
  gnome_vfs_uri_unref (uri);
  return string;
}

/**
 * Cleanup the data used, and even enable the Extract button again.
 */
static void
cleanup (void)
{
  /* We're not extracting any more */
  extracting = FALSE;

  /* Remove any state icons from the model */
  gtk_list_store_set (track_store, &track->iter,
                 COLUMN_STATE, STATE_IDLE, -1);
  
  /* Free the used data */
  if (paths) {
    g_list_deep_free (paths, NULL);
    paths = NULL;
  }
  g_list_free (pending);
  pending = NULL;
  track = NULL;
  gtk_button_set_label (GTK_BUTTON (extract_button), SJ_STOCK_EXTRACT);
  
  /* Clear the Status bar */
  gtk_statusbar_push (GTK_STATUSBAR (status_bar), 0, "");
  /* Clear the progress bar */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);
  
  gtk_widget_set_sensitive (play_button, TRUE);
  gtk_widget_set_sensitive (title_entry, TRUE);
  gtk_widget_set_sensitive (artist_entry, TRUE);
  gtk_widget_set_sensitive (genre_entry, TRUE);
  /* Enabling the Menuitem */ 
  gtk_widget_set_sensitive (play_menuitem, TRUE);
  gtk_widget_set_sensitive (extract_menuitem, TRUE);
  gtk_widget_set_sensitive (reread_menuitem, TRUE);
  gtk_widget_set_sensitive (select_all_menuitem, TRUE);
  gtk_widget_set_sensitive (deselect_all_menuitem, TRUE);
  
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
static gboolean
check_for_file (const char* uri)
{
  GnomeVFSFileInfo info;
  GnomeVFSResult res;
  GtkWidget *dialog;
  char *filename, *size;
  int ret;
  
  res = gnome_vfs_get_file_info (uri, &info, GNOME_VFS_FILE_INFO_DEFAULT);
  if (res == GNOME_VFS_ERROR_NOT_FOUND)
    return TRUE;
  if (res != GNOME_VFS_OK) {
    /* TODO: display an error dialog */
    g_warning ("Cannot get file info: %s", gnome_vfs_result_to_string (res));
    return FALSE;
  }
  if (info.size < (100000)) {
    /* The file exists but is small, assume overwriting */
    return TRUE;
  }
  /* Otherwise the file exists and is large, ask user if they really
     want to overwrite it */
  filename = gnome_vfs_format_uri_for_display (uri);
  size = gnome_vfs_format_file_size_for_display (info.size);
  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("A file called '%s' exists, size %s.\nDo you want to skip this track or overwrite it?"),
                                   filename, size);
  g_free (filename);
  g_free (size);
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
create_directory_for (const char* url, GError **error)
{
  GnomeVFSResult res;
  GnomeVFSURI *uri, *parent;
  char *string;

  g_return_val_if_fail (url != NULL, NULL);

  uri = gnome_vfs_uri_new (url);
  parent = gnome_vfs_uri_get_parent (uri);
  gnome_vfs_uri_unref (uri);

  res = make_directory_with_parents_for_uri (parent, 0777);
  if (res != GNOME_VFS_OK && res != GNOME_VFS_ERROR_FILE_EXISTS) {
    g_set_error (error, SJ_ERROR, SJ_ERROR_CD_PERMISSION_ERROR, g_strdup (gnome_vfs_result_to_string (res)));
    return NULL;
  }

  string = gnome_vfs_uri_to_string (parent, 0);
  gnome_vfs_uri_unref (parent);
  return string;
}

/* Prototype for pop_and_extract */
static void on_completion_cb (SjExtractor *extractor, gpointer data);

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
    char *text;
    
    /* Pop the next track to extract */
    track = pending->data;
    pending = g_list_remove (pending, track);
    /* Build the filename for this track */
    file_path = build_filename (track);
    /* And create the directory it lives in */
    directory = create_directory_for (file_path, &error);
    if (error) {
      GtkWidget *dialog;
      text = g_strdup_printf (_("Sound Juicer could not extract this CD.\nReason: %s"), error->message);

      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       text);
      g_free (text);

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
      on_completion_cb (NULL, NULL);
      return;
    }
   
    /* Update the state stock image */
    gtk_list_store_set (track_store, &track->iter,
                   COLUMN_STATE, STATE_EXTRACTING, -1);
		
    /* Update the progress bars */
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar),
                                   CLAMP (1.0 - ((g_list_length (pending) + 1)  / (float)total_extracting), 0.0, 1.0));

    /* Now actually do the extraction */
    sj_extractor_extract_track (extractor, track, file_path, &error);
    if (error) {
      GtkWidget *dialog;

      text = g_strdup_printf (_("Sound Juicer could not extract this CD.\nReason: %s"), error->message);

      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       text);
      g_free (text);

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
    track->iter = *iter;
    pending = g_list_append (pending, track);
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
  
  gtk_statusbar_push (GTK_STATUSBAR (status_bar), 0, eta_str);
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

/**
 * Show the "Finished!" dialog, allowing the user to open Nautilus if he wants.
 */
static void
show_finished_dialog (void)
{
  GtkWidget *dialog;
  int result;
  char *base;

  base = NULL;
  /* Find the deepest common directory. */
  g_list_foreach (paths, (GFunc)base_finder, &base);

  cleanup ();

  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_NONE,
                                   /* TODO: need to have a better message here */
                                   _("The tracks have been copied successfully."));
  /* If we eject when finished, eject now, otherwise add a button */
  if (eject_finished) {
    nautilus_burn_drive_eject (drive);
  } else {
    gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Eject"), 2, NULL);
  }
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_OPEN, 1,
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  if (result == 1) {
    char *command;
    command = g_strdup_printf ("gnome-open \"%s\"", base);
    g_spawn_command_line_async (command, NULL);
    g_free (command);
  } else if (result == 2) {
    nautilus_burn_drive_eject (drive);
  }
  g_free (base);
}

/**
 * Callback from SjExtractor to report completion.
 */
static void
on_completion_cb (SjExtractor *extractor, gpointer data)
{
  gtk_list_store_set (track_store, &track->iter,
                    COLUMN_STATE, STATE_IDLE, -1);
  
  /* Uncheck the Extract check box */
  gtk_list_store_set (track_store, &track->iter, COLUMN_EXTRACT, FALSE, -1);

  if (pending == NULL) {
    show_finished_dialog ();
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
  char *text;
  /* Display a nice dialog */
  text = g_strdup_printf (_("Sound Juicer could not extract this CD.\nReason: %s"), error->message);

  dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), 0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   text);
  g_free (text);

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
  sj_extractor_cancel_extract (extractor);
  cleanup ();
}

/**
 * Entry point from the interface.
 */
void
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
  pending = NULL;
  total_extracting = 0;
  current_duration = total_duration = 0;
  before.seconds = -1;
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), extract_track_foreach_cb, NULL);
  /* If the pending list is still empty, return */
  if (pending == NULL) {
    /* Should never reach here */
    g_warning ("No tracks selected for extracting");
    return;
  }

  /* Initialise ourself */
  if (!initialised) {
    /* Connect to the SjExtractor signals */
    g_signal_connect (extractor, "progress", G_CALLBACK (on_progress_cb), NULL);
    g_signal_connect (extractor, "completion", G_CALLBACK (on_completion_cb), NULL);
    g_signal_connect (extractor, "error", G_CALLBACK (on_error_cb), NULL);

    extract_button = glade_xml_get_widget (glade, "extract_button");
    play_button = glade_xml_get_widget (glade, "play_button");
    title_entry = glade_xml_get_widget (glade, "title_entry");
    artist_entry = glade_xml_get_widget (glade, "artist_entry");
    genre_entry = glade_xml_get_widget (glade, "genre_entry");
    track_listview = glade_xml_get_widget (glade, "track_listview");
    progress_bar = glade_xml_get_widget (glade, "progress_bar");
    status_bar = glade_xml_get_widget (glade, "status_bar");
	
    play_menuitem = glade_xml_get_widget (glade, "play_menuitem");
    extract_menuitem = glade_xml_get_widget (glade, "extract_menuitem");
    reread_menuitem = glade_xml_get_widget (glade, "re-read");
    select_all_menuitem = glade_xml_get_widget (glade, "select_all");
    deselect_all_menuitem = glade_xml_get_widget (glade, "deselect_all");
    
    initialised = TRUE;
  }
  
  /* Change the label to Stop while extracting*/
  gtk_button_set_label (GTK_BUTTON (extract_button), GTK_STOCK_STOP);
  
  /* Reset the progress dialog */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);
  update_speed_progress (NULL, 0.0, -1);

  /* Disable the widgets in the main UI*/
  gtk_widget_set_sensitive (play_button, FALSE);
  gtk_widget_set_sensitive (title_entry, FALSE);
  gtk_widget_set_sensitive (artist_entry, FALSE);
  gtk_widget_set_sensitive (genre_entry, FALSE);

  /* Disable the menuitems in the main menu*/	
  gtk_widget_set_sensitive (play_menuitem, FALSE);
  gtk_widget_set_sensitive (extract_menuitem, FALSE);
  gtk_widget_set_sensitive (reread_menuitem, FALSE);
  gtk_widget_set_sensitive (select_all_menuitem, FALSE);
  gtk_widget_set_sensitive (deselect_all_menuitem, FALSE);
  
  /* Disable the Extract column */
  g_object_set (G_OBJECT (toggle_renderer), "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
  g_object_set (G_OBJECT (title_renderer), "editable", FALSE, NULL);
  g_object_set (G_OBJECT (artist_renderer), "editable", FALSE, NULL);
  g_signal_handlers_block_by_func (track_listview, on_tracklist_row_activate, NULL);

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
 * This will always replace '/' with ' ', and optionally make the file name
 * shell-friendly. This involves removing [?*\ ] and replacing with '_'.  Also
 * any leading periods are removed so that the files don't end up being hidden.
 *
 * This function doesn't change the input, and returns an allocated 
 * string.
 */
static char*
sanitize_path (const char* str)
{
  gchar *res = NULL;
  gchar *s;

  /* Skip leading periods, otherwise files disappear... */
  while (*str == '.')
    str++;
  
  s = g_strdup(str);
  /* Replace path seperators with a hyphen */
  g_strdelimit (s, "/", '-');
  if (strip_chars) {
    /* Replace separators with a hyphen */
    g_strdelimit (s, "\\:|", '-');
    /* Replace all other weird characters to whitespace */
    g_strdelimit (s, "*?&!\'\"$()`>{}", ' ');
    /* Replace all whitespace with underscores */
    /* TODO: I'd like this to compress whitespace aswell */
    g_strdelimit (s, "\t ", '_');
  }
  res = g_filename_from_utf8(s, -1, NULL, NULL, NULL);
  g_free(s);
  return res ? res : g_strdup(str);
}

/**
 * Parse a filename pattern and replace markers with values from a TrackDetails
 * structure.
 *
 * Valid markers so far are:
 * %at -- album title
 * %aa -- album artist
 * %aA -- album artist (lowercase)
 * %as -- album artist sortname
 * %aS -- album artist sortname (lowercase)
 * %tn -- track number (i.e 8)
 * %tN -- track number, zero padded (i.e 08)
 * %tt -- track title
 * %ta -- track artist
 * %tA -- track artist (lowercase)
 * %ts -- track artist sortname
 * %tS -- track artist sortname (lowercase)
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
    gchar *tmp; 
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
        i = temp = sanitize_path (track->album->title);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'T':
        tmp = g_utf8_strdown (track->album->title, -1);
        i = temp = sanitize_path (tmp);
        g_free(tmp);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'a':
        i = temp = sanitize_path (track->album->artist);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'A':
        tmp = g_utf8_strdown (track->album->artist, -1);
        i = temp = sanitize_path (tmp);
        g_free(tmp);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 's':
        i = temp = sanitize_path (track->album->artist_sortname ?: track->album->artist);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'S':
        tmp = g_utf8_strdown (track->album->artist_sortname ?: track->album->artist, -1);
        i = temp = sanitize_path (tmp);
        g_free(tmp);
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
        i = temp = sanitize_path (track->title);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'T':
        tmp = g_utf8_strdown (track->title, -1);
        i = temp = sanitize_path (tmp);
        g_free(tmp);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'a':
        i = temp = sanitize_path (track->artist);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'A':
        tmp = g_utf8_strdown (track->artist, -1);
        i = temp = sanitize_path (tmp);
        g_free(tmp);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 's':
        i = temp = sanitize_path (track->artist_sortname ?: track->artist);
        while (*i) *s++ = *i++;
        g_free (temp);
        break;
      case 'S':
        tmp = g_utf8_strdown (track->artist_sortname ?: track->artist, -1);
        i = temp = sanitize_path (tmp);
        g_free(tmp);
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
