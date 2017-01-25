/*
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-main.c
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
 *          Mike Hearn  <mike@theoretic.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sound-juicer.h"
#include "sj-album-chooser-dialog.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <brasero-medium-selection.h>
#include <brasero-volume.h>
#include <gst/gst.h>
#include <gst/pbutils/encoding-profile.h>

#include "sj-cell-renderer-text.h"
#include "rb-gst-media-types.h"
#include "sj-about.h"
#include "sj-metadata.h"
#include "sj-metadata-getter.h"
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"
#include "sj-prefs.h"
#include "sj-play.h"
#include "sj-genres.h"
#include "sj-window-state.h"

gboolean on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data);

static void reread_cd (void);
static void update_ui_for_album (AlbumDetails *album);
static gboolean get_action_state_bool (const char *name);
static void set_action_state (const char *detailed_name);

/* Prototypes for the signal blocking/unblocking in update_ui_for_album */
G_MODULE_EXPORT void on_title_edit_changed(GtkEditable *widget, gpointer user_data);
G_MODULE_EXPORT void on_person_edit_changed(GtkEditable *widget, gpointer user_data);
G_MODULE_EXPORT void on_genre_edit_changed(GtkEditable *widget, gpointer user_data);
G_MODULE_EXPORT void on_year_edit_changed(GtkEditable *widget, gpointer user_data);
G_MODULE_EXPORT void on_disc_number_edit_changed(GtkEditable *widget, gpointer user_data);
G_MODULE_EXPORT void submit_bar_response_cb (GtkInfoBar *infobar, int response_id, gpointer user_data);
G_MODULE_EXPORT void on_activate_move_focus (GtkWidget *widget, gpointer data);

GtkBuilder *builder;

SjMetadataGetter *metadata;
SjExtractor *sj_extractor;

GSettings *sj_settings;

GtkWidget *main_window;
static GtkWidget *submit_bar, *submit_label;
static GtkWidget *title_entry, *artist_entry, *composer_label, *composer_entry, *duration_label, *genre_entry, *year_entry, *disc_number_entry;
static GtkWidget *entry_grid; /* GtkGrid containing album title, artist etc. */
static GtkTreeViewColumn *composer_column; /* Treeview column containing composers */
static GtkWidget *track_listview, *extract_button, *play_button, *select_button;
static GtkWidget *multiple_album_dialog, *status_bar, *submit_button, *reload_button;
GtkListStore *track_store;
GtkCellRenderer *toggle_renderer, *title_renderer, *artist_renderer, *composer_renderer;

char *sj_path_pattern, *sj_file_pattern;
GFile *sj_base_uri;
BraseroDrive *sj_drive = NULL;
gboolean strip_chars;
gboolean eject_finished;
gboolean open_finished;
gboolean extracting = FALSE;
static gboolean duplication_enabled;

static gboolean eject_activated;
static gint total_no_of_tracks;
static gint no_of_tracks_selected;
static AlbumDetails *current_album;
static char *current_submit_url = NULL;
static GCancellable *reread_cancellable; /* weak reference */

gboolean autostart = FALSE, autoplay = FALSE;
static char *sj_device = NULL, **uris = NULL;

static guint debug_flags = 0;

/**
 * Shared state for cell editing callbacks
 */
typedef struct {
  GtkTreePath* editable_path;
  char* text;
  ViewColumn column;
} CellCbContext;

static CellCbContext cell_editing_context;

#define DEFAULT_PARANOIA 15
#define RAISE_WINDOW "raise-window"

void
sj_main_set_title (const char* detail)
{
  if (detail == NULL) {
    gtk_window_set_title (GTK_WINDOW (main_window), _("Sound Juicer"));
  } else {
    char *s = g_strdup_printf ("%s - %s", detail, _("Sound Juicer"));
    gtk_window_set_title (GTK_WINDOW (main_window), s);
    g_free (s);
  }
}

void sj_debug (SjDebugDomain domain, const gchar* format, ...)
{
  va_list args;
  gchar *string;

  if (debug_flags & domain) {
    va_start (args, format);
    string = g_strdup_vprintf (format, args);
    va_end (args);
    g_printerr ("%s", string);
    g_free (string);
  }
}

static void sj_debug_init (void)
{
  const char *str;
  const GDebugKey debug_keys[] = {
    { "cd", DEBUG_CD },
    { "metadata", DEBUG_METADATA },
    { "playing", DEBUG_PLAYING },
    { "extracting", DEBUG_EXTRACTING }
  };

  str = g_getenv ("SJ_DEBUG");
  if (str) {
    debug_flags = g_parse_debug_string (str, debug_keys, G_N_ELEMENTS (debug_keys));
  }
}

static void error_on_start (GError *error)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new_with_markup (NULL, 0,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "<b>%s</b>\n\n%s: %s.\n%s",
                                               _("Could not start Sound Juicer"),
                                               _("Reason"),
                                               error->message,
                                               _("Please consult the documentation for assistance."));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

/**
 * Clicked Quit
 */
static void on_quit_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  if (on_delete_event (NULL, NULL, NULL) == FALSE) {
    gtk_widget_destroy (GTK_WIDGET (main_window));
  }
}

enum {
  SJ_RESPONSE_EJECTED = 1
};

static void
disc_ejected_cb (void)
{
  /* first make sure we're not playing */
  stop_playback ();
  stop_ui_hack ();
  update_ui_for_album (NULL);
  if (multiple_album_dialog != NULL)
    gtk_dialog_response (GTK_DIALOG (multiple_album_dialog),
                         SJ_RESPONSE_EJECTED);
  if (reread_cancellable != NULL) {
    g_cancellable_cancel (reread_cancellable);
    reread_cancellable = NULL;
  }
  set_action_state ("re-read(false)");
  set_action_enabled ("re-read", FALSE);
  set_action_enabled ("submit-tracks", FALSE);
}

/**
 * Clicked Eject
 */
static void on_eject_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  disc_ejected_cb ();
  eject_activated = TRUE;
  brasero_drive_eject (sj_drive, FALSE, NULL);
}

G_MODULE_EXPORT gboolean on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  if (extracting) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_NONE,
                                     _("You are currently extracting a CD. Do you want to quit now or continue?"));
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Quit"), GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Continue"), GTK_RESPONSE_REJECT);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    if (response == GTK_RESPONSE_ACCEPT) {
      return FALSE;
    }
    return TRUE;
  }
  return FALSE;
}

static gboolean select_all_foreach_cb (GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
  gboolean b = (gboolean)GPOINTER_TO_INT (data);
  gtk_list_store_set (track_store, iter, COLUMN_EXTRACT, b, -1);
  return FALSE;
}

static void on_play_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  toggle_play ();
}

static void on_next_track_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  play_next_track ();
}

static void on_previous_track_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  play_previous_track ();
}

static void on_select_all_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), select_all_foreach_cb, GINT_TO_POINTER (TRUE));
  gtk_widget_set_sensitive (extract_button, TRUE);

  set_action_enabled ("select-all", FALSE);
  set_action_enabled ("deselect-all", TRUE);

  gtk_actionable_set_action_name(GTK_ACTIONABLE(select_button), "win.deselect-all");
  gtk_button_set_label(GTK_BUTTON(select_button), _("Select None"));

  no_of_tracks_selected = total_no_of_tracks;
}

static void on_deselect_all_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), select_all_foreach_cb, GINT_TO_POINTER (FALSE));
  gtk_widget_set_sensitive (extract_button, FALSE);

  set_action_enabled ("deselect-all", FALSE);
  set_action_enabled ("select-all", TRUE);

  gtk_actionable_set_action_name(GTK_ACTIONABLE(select_button), "win.select-all");
  gtk_button_set_label(GTK_BUTTON(select_button), _("Select All"));

  no_of_tracks_selected = 0;
}

/**
 * GtkTreeView cell renderer callback to render durations
 */
static void duration_cell_data_cb (GtkTreeViewColumn *tree_column,
                                GtkCellRenderer *cell,
                                GtkTreeModel *tree_model,
                                GtkTreeIter *iter,
                                gpointer data)
{
  int duration;
  char *string;

  gtk_tree_model_get (tree_model, iter, COLUMN_DURATION, &duration, -1);
  if (duration != 0) {
    string = g_strdup_printf("%d:%02d", duration / 60, duration % 60);
  } else {
    string = g_strdup(_("(unknown)"));
  }
  g_object_set (G_OBJECT (cell), "text", string, NULL);
  g_free (string);
}

static void number_cell_icon_data_cb (GtkTreeViewColumn *tree_column,
                                      GtkCellRenderer *cell,
                                      GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
  TrackState state;
  gtk_tree_model_get (tree_model, iter, COLUMN_STATE, &state, -1);
  switch (state) {
  case STATE_IDLE:
    g_object_set (G_OBJECT (cell), "icon-name", NULL, NULL);
    break;
  case STATE_PLAYING:
    g_object_set (G_OBJECT (cell), "icon-name", "media-playback-start", NULL);
    break;
  case STATE_PAUSED:
    g_object_set (G_OBJECT (cell), "icon-name", "media-playback-pause", NULL);
    break;
  case STATE_EXTRACTING:
    g_object_set (G_OBJECT (cell), "icon-name", "media-record", NULL);
    break;
  default:
    g_warning("Unhandled track state %d\n", state);
  }
}

static void
set_submit_text (const AlbumDetails *album)
{
  gchar *text;

  if (g_str_equal (album->title, _("Unknown Title"))) {
    text = g_strdup (_("This album is not in the MusicBrainz database, please click ‘Edit Album’ to open your browser and edit it in MusicBrainz."));
  } else {
    text = g_strdup_printf (_("Could not find %s by %s on MusicBrainz, please click ‘Edit Album’ to open your browser and edit it in MusicBrainz."),
                                    album->title, album->artist);
  }
  gtk_label_set_text (GTK_LABEL (submit_label), text);
  g_free (text);
  gtk_widget_show (submit_button);
  gtk_widget_hide (reload_button);
}

static void
submit_tracks_enabled_changed_cb (GActionGroup *action_group,
                                  gchar        *action_name,
                                  gboolean      enabled,
                                  gpointer      user_data)
{
  if (!enabled)
    gtk_widget_hide (submit_bar);
}

enum {
  SJ_RESPONSE_SUBMIT = 1,
  SJ_RESPONSE_REREAD
};

G_MODULE_EXPORT void
submit_bar_response_cb (GtkInfoBar *infobar,
                        int         response_id,
                        gpointer    user_data)
{
  if (!gtk_widget_get_visible (submit_bar))
    return;

  if (response_id == SJ_RESPONSE_SUBMIT) {
    gtk_widget_hide (submit_button);
    gtk_widget_show (reload_button);
    gtk_label_set_text (GTK_LABEL (submit_label),
                        _("Click ‘Reload’ to load the edited album details from MusicBrainz"));
  } else {
    gtk_widget_hide (GTK_WIDGET (infobar));
  }
}

/**
 * Clicked the Submit menu item in the UI
 */
static void on_submit_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  GError *error = NULL;

  if (current_submit_url) {
      if (!gtk_show_uri (NULL, current_submit_url, GDK_CURRENT_TIME, &error)) {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (main_window),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "<b>%s</b>\n\n%s\n%s: %s",
                                                   _("Could not open URL"),
                                                   _("Sound Juicer could not open the submission URL"),
                                                   _("Reason"),
                                                   error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
  }
  gtk_info_bar_response (GTK_INFO_BAR (submit_bar), SJ_RESPONSE_SUBMIT);
}

static void on_preferences_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  show_preferences_dialog ();
}

static void on_about_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  show_about_dialog ();
}

/**
 * Clicked on duplicate in the UI (button/menu)
 */
static void on_duplicate_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  GError *error = NULL;
  const gchar* device;

  device = brasero_drive_get_device (sj_drive);
  if (!g_spawn_command_line_async (g_strconcat ("brasero -c ", device, NULL), &error)) {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (main_window),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "<b>%s</b>\n\n%s\n%s: %s",
                                                   _("Could not duplicate disc"),
                                                   _("Sound Juicer could not duplicate the disc"),
                                                   _("Reason"),
                                                   error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
  }
}


/*
 * Show composer entry and composer column in track listview
 */
static void
show_composer_fields (void)
{
  if (!gtk_widget_get_visible (GTK_WIDGET (composer_label))) {
    gtk_widget_show (GTK_WIDGET (composer_entry));
    gtk_widget_show (GTK_WIDGET (composer_label));
    gtk_tree_view_column_set_visible (composer_column, TRUE);
  }
}

#undef TABLE_ROW_SPACING

/*
 * Hide composer entry and composer column in track listview
 */
static void
hide_composer_fields (void)
{
  if (gtk_widget_get_visible (GTK_WIDGET (composer_label))) {
    gtk_widget_hide (GTK_WIDGET (composer_entry));
    gtk_widget_hide (GTK_WIDGET (composer_label));
    gtk_tree_view_column_set_visible (composer_column, FALSE);
  }
}

/*
 * Determine if the composer fields should be shown based on genre,
 * always show composer fields if they are non-empty.
 */
static void
composer_show_hide (const char* genre)
{
  static const char *composer_genres[] = {
    N_("Classical"), N_("Lieder"), N_("Opera"), N_("Chamber"), N_("Musical")
  };  /* Genres for which the composer fields should be shown. */
#define COUNT (G_N_ELEMENTS (composer_genres))
  static char *genres[COUNT]; /* store localized genre names */
  static gboolean init = FALSE; /* TRUE if localized genre names initalized*/
  gboolean composer_show = FALSE;
  gsize i;
  GList* l;
  char *folded_genre;

  if (composer_column == NULL)
    return;

  if (!init) {
    for (i = 0; i < COUNT; i++)
      genres[i] = g_utf8_casefold (gettext (composer_genres[i]), -1);

    init = TRUE;
  }

  composer_show = !sj_str_is_empty (current_album->composer);
  for (l = current_album->tracks; l; l = g_list_next (l)) {
    if (!sj_str_is_empty (((TrackDetails*) (l->data))->composer)) {
      composer_show = TRUE;
      break;
    }
  }

  folded_genre = g_utf8_casefold (genre, -1);
  for (i = 0; i < COUNT; i++) {
    if (g_str_equal (folded_genre, genres[i])) {
      composer_show = TRUE;
      break;
    }
  }
  g_free (folded_genre);

  if (composer_show)
    show_composer_fields ();
  else
    hide_composer_fields ();
  return;
}
#undef COUNT

/**
 * Utility function to update the UI for a given Album
 */
static void update_ui_for_album (AlbumDetails *album)
{
  GList *l;
  int album_duration = 0;
  char* duration_text;
  total_no_of_tracks=0;

  /* Really really make sure we don't have a playing title */
  sj_main_set_title (NULL);

  hide_composer_fields ();

  if (album == NULL) {
    gtk_list_store_clear (track_store);
    gtk_entry_set_text (GTK_ENTRY (title_entry), "");
    gtk_entry_set_text (GTK_ENTRY (artist_entry), "");
    gtk_entry_set_text (GTK_ENTRY (composer_entry), "");
    gtk_entry_set_text (GTK_ENTRY (genre_entry), "");
    gtk_entry_set_text (GTK_ENTRY (year_entry), "");
    gtk_entry_set_text (GTK_ENTRY (disc_number_entry), "");
    gtk_label_set_text (GTK_LABEL (duration_label), "");
    gtk_widget_set_sensitive (title_entry, FALSE);
    gtk_widget_set_sensitive (artist_entry, FALSE);
    gtk_widget_set_sensitive (composer_entry, FALSE);
    gtk_widget_set_sensitive (genre_entry, FALSE);
    gtk_widget_set_sensitive (year_entry, FALSE);
    gtk_widget_set_sensitive (disc_number_entry, FALSE);
    gtk_widget_set_sensitive (play_button, FALSE);
    gtk_widget_set_sensitive (extract_button, FALSE);
    set_action_enabled ("play", FALSE);
    set_action_enabled ("select-all", FALSE);
    set_action_enabled ("deselect-all", FALSE);
    set_action_enabled ("previous-track", FALSE);
    set_action_enabled ("next-track", FALSE);
    set_action_enabled ("duplicate", FALSE);
  } else {
    gtk_list_store_clear (track_store);

    g_signal_handlers_block_by_func (title_entry, on_title_edit_changed, NULL);
    g_signal_handlers_block_by_func (artist_entry, on_person_edit_changed, NULL);
    g_signal_handlers_block_by_func (composer_entry, on_person_edit_changed, NULL);
    g_signal_handlers_block_by_func (year_entry, on_year_edit_changed, NULL);
    g_signal_handlers_block_by_func (disc_number_entry, on_disc_number_edit_changed, NULL);
    gtk_entry_set_text (GTK_ENTRY (title_entry), album->title);
    gtk_entry_set_text (GTK_ENTRY (artist_entry), album->artist);
    if (!sj_str_is_empty (album->composer)) {
      gtk_entry_set_text (GTK_ENTRY (composer_entry), album->composer);
      show_composer_fields ();
    } else {
      gtk_entry_set_text (GTK_ENTRY (composer_entry), "");
    }
    if (album->disc_number) {
      gchar *disc_number = g_strdup_printf ("%d", album->disc_number);
      gtk_entry_set_text (GTK_ENTRY (disc_number_entry), disc_number);
      g_free (disc_number);
    }
    if (album->release_date && gst_date_time_has_year (album->release_date)) {
      gchar *release_date =  g_strdup_printf ("%d", gst_date_time_get_year (album->release_date));
      gtk_entry_set_text (GTK_ENTRY (year_entry), release_date);
      g_free (release_date);
    }
    g_signal_handlers_unblock_by_func (title_entry, on_title_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (artist_entry, on_person_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (composer_entry, on_person_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (year_entry, on_year_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (disc_number_entry, on_disc_number_edit_changed, NULL);
    /* Clear the genre field, it's from the user */
    gtk_entry_set_text (GTK_ENTRY (genre_entry), "");

    gtk_widget_set_sensitive (title_entry, TRUE);
    gtk_widget_set_sensitive (artist_entry, TRUE);
    gtk_widget_set_sensitive (composer_entry, TRUE);
    gtk_widget_set_sensitive (genre_entry, TRUE);
    gtk_widget_set_sensitive (year_entry, TRUE);
    gtk_widget_set_sensitive (disc_number_entry, TRUE);
    gtk_widget_set_sensitive (play_button, TRUE);
    gtk_widget_set_sensitive (extract_button, TRUE);
    set_action_enabled ("play", TRUE);
    set_action_enabled ("select-all", FALSE);
    set_action_enabled ("deselect-all", TRUE);
    set_action_enabled ("previous-track", FALSE);
    set_action_enabled ("next-track", FALSE);
    set_action_enabled ("duplicate", TRUE);

    for (l = album->tracks; l; l=g_list_next (l)) {
      GtkTreeIter iter;
      TrackDetails *track = (TrackDetails*)l->data;
      album_duration += track->duration;
      gtk_list_store_append (track_store, &iter);
      gtk_list_store_set (track_store, &iter,
                          COLUMN_STATE, STATE_IDLE,
                          COLUMN_EXTRACT, TRUE,
                          COLUMN_NUMBER, track->number,
                          COLUMN_TITLE, track->title,
                          COLUMN_ARTIST, track->artist,
                          COLUMN_COMPOSER, track->composer,
                          COLUMN_DURATION, track->duration,
                          COLUMN_DETAILS, track,
                          -1);
      if (!sj_str_is_empty (track->composer))
        show_composer_fields ();
      total_no_of_tracks++;
    }
    no_of_tracks_selected=total_no_of_tracks;

    /* Some albums don't have track durations :( */
    if (album_duration) {
      duration_text = g_strdup_printf("%d:%02d", album_duration / 60, album_duration % 60);
      gtk_label_set_text (GTK_LABEL (duration_label), duration_text);
      g_free (duration_text);
    } else {
      gtk_label_set_text (GTK_LABEL (duration_label), _("(unknown)"));
    }
    /* If album details don't come from MusicBrainz ask user to add them */
    if (album->metadata_source != SOURCE_MUSICBRAINZ) {
      set_submit_text (album);
      gtk_widget_show (submit_bar);
    }
  }
}

/**
 * Utility function for when there are more than one album available
 */
static AlbumDetails*
choose_album(GList *albums)
{
  AlbumDetails *album;
  gint response;

  multiple_album_dialog = sj_album_chooser_dialog_new (GTK_WINDOW (main_window),
                                                       albums);
  response = gtk_dialog_run (GTK_DIALOG (multiple_album_dialog));
  if (response == SJ_RESPONSE_EJECTED || response == GTK_RESPONSE_NONE)
    album = NULL;
  else
    album = sj_album_chooser_dialog_get_selected_album (SJ_ALBUM_CHOOSER_DIALOG (multiple_album_dialog));
  if (response != GTK_RESPONSE_NONE)
    gtk_widget_destroy (GTK_WIDGET (multiple_album_dialog));
  multiple_album_dialog = NULL;

  return album;
}

/**
 * The GSettings key for the base path changed
 */
static void baseuri_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  gchar *value;
  g_assert (strcmp (key, SJ_SETTINGS_BASEURI) == 0);
  if (sj_base_uri) {
    g_object_unref (sj_base_uri);
  }
  value = g_settings_get_string (settings, key);
  if (sj_str_is_empty (value)) {
    sj_base_uri = sj_get_default_music_directory ();
  } else {
    GFileType file_type;
    sj_base_uri = g_file_new_for_uri (value);
    file_type = g_file_query_file_type (sj_base_uri, G_FILE_QUERY_INFO_NONE, NULL);
    if (file_type != G_FILE_TYPE_DIRECTORY) {
      g_object_unref (sj_base_uri);
      sj_base_uri = sj_get_default_music_directory ();
    }
  }
  g_free (value);
}

/**
 * The GSettings key for the directory pattern changed
 */
static void path_pattern_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  g_assert (strcmp (key, SJ_SETTINGS_PATH_PATTERN) == 0);
  g_free (sj_path_pattern);
  sj_path_pattern = g_settings_get_string (settings, key);
  if (sj_str_is_empty (sj_path_pattern)) {
    g_free (sj_path_pattern);
    sj_path_pattern = g_strdup (sj_get_default_path_pattern ());
  }
  /* TODO: sanity check the pattern */
}

/**
 * The GSettings key for the filename pattern changed
 */
static void file_pattern_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  g_assert (strcmp (key, SJ_SETTINGS_FILE_PATTERN) == 0);
  g_free (sj_file_pattern);
  sj_file_pattern = g_settings_get_string (settings, key);
  if (sj_str_is_empty (sj_file_pattern)) {
    g_free (sj_file_pattern);
    sj_file_pattern = g_strdup (sj_get_default_file_pattern ());
  }
  /* TODO: sanity check the pattern */
}

/**
 * The GSettings key for the paranoia mode has changed
 */
static void paranoia_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  int value;
  g_assert (strcmp (key, SJ_SETTINGS_PARANOIA) == 0);
  value = g_settings_get_flags (settings, key);
  if (value >= 0) {
    if (value < 32) {
      sj_extractor_set_paranoia (sj_extractor, value);
    } else {
      sj_extractor_set_paranoia (sj_extractor, DEFAULT_PARANOIA);
    }
  }
}

/**
 * The GSettings key for the strip characters option changed
 */
static void strip_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  g_assert (strcmp (key, SJ_SETTINGS_STRIP) == 0);
  strip_chars = g_settings_get_boolean (settings, key);
}

/**
 * The GSettings key for the eject when finished option changed
 */
static void eject_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  g_assert (strcmp (key, SJ_SETTINGS_EJECT) == 0);
  eject_finished = g_settings_get_boolean (settings, key);
}

/**
 * The GSettings key for the open when finished option changed
 */
static void open_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  g_assert (strcmp (key, SJ_SETTINGS_OPEN) == 0);
  open_finished = g_settings_get_boolean (settings, key);
}

/**
 * The GSettings key for audio volume changes
 */
static void audio_volume_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  GtkWidget *volb;

  g_assert (strcmp (key, SJ_SETTINGS_AUDIO_VOLUME) == 0);

  volb = GET_WIDGET ("volume_button");
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (volb), g_settings_get_double (settings, key));
}

static void
metadata_cb (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
  SjMetadataGetter *mdg = SJ_METADATA_GETTER (source);
  GError *error = NULL;
  GList *albums;
  gchar *url;

  albums = sj_metadata_getter_list_albums_finish (mdg,
                                                  result,
                                                  &url,
                                                  &error);
  /* Check if this request was cancelled */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_error_free (error);
    return;
  }

  reread_cancellable = NULL;
  set_action_state ("re-read(false)");

  if (error != NULL) {
    gboolean realized;
    GtkWidget *dialog;

    realized = gtk_widget_get_realized (main_window);
    dialog = gtk_message_dialog_new_with_markup (realized ? GTK_WINDOW (main_window) : NULL, 0,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "<b>%s</b>\n\n%s\n%s: %s",
                                                 _("Could not read the CD"),
                                                 _("Sound Juicer could not read the track listing on this CD."),
                                                 _("Reason"),
                                                 error->message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    update_ui_for_album (NULL);
    g_error_free (error);
    set_action_enabled ("re-read", TRUE);
    return;
  }

  /* Free old album details */
  g_clear_pointer (&current_album, (GDestroyNotify) album_details_free);
  /* Set the new current album pointer */
  if (albums != NULL) {
    if (albums->next != NULL) {
      current_album = choose_album (albums);
      /* Concentrate here. We remove the album we want from the list, and then
         deep-free the list. */
      albums = g_list_remove (albums, current_album);
      g_list_free_full (albums, (GDestroyNotify)album_details_free);
      albums = NULL;
    } else {
      current_album = albums->data;
      /* current_album now owns ->data, so just free the list */
      g_clear_pointer (&albums, (GDestroyNotify) g_list_free);
    }
  }
  update_ui_for_album (current_album);
  set_action_enabled ("re-read", TRUE);

  g_free (current_submit_url);
  current_submit_url = url;
  if (current_submit_url) {
    set_action_enabled ("submit-tracks", TRUE);
  }

  if (autostart) {
    g_signal_emit_by_name (extract_button, "activate", NULL);
    autostart = FALSE;
  }
  if (autoplay) {
    g_signal_emit_by_name (play_button, "activate", NULL);
    autoplay = FALSE;
  }
}

static gboolean
is_audio_cd (BraseroDrive *drive)
{
  BraseroMedium *medium;
  BraseroMedia type;

  medium = brasero_drive_get_medium (drive);
  if (medium == NULL) return FALSE;
  type = brasero_medium_get_status (medium);
  if (type == BRASERO_MEDIUM_UNSUPPORTED) {
    g_warning ("Error getting media type\n");
  }
  if (type == BRASERO_MEDIUM_BUSY) {
    g_warning ("BUSY getting media type, should re-check\n");
  }
  return BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_CD);
}

/**
 * Utility function to reread a CD
 */
static void reread_cd (void)
{
  set_action_state ("re-read(true)");
  set_action_enabled ("re-read", FALSE);

  /* Make sure nothing is playing */
  stop_playback ();

  if (!sj_drive)
    sj_debug (DEBUG_CD, "Attempting to re-read NULL drive\n");

  g_free (current_submit_url);
  current_submit_url = NULL;
  set_action_enabled ("submit-tracks", FALSE);

  if (!is_audio_cd (sj_drive)) {
    sj_debug (DEBUG_CD, "Media is not an audio CD\n");
    set_action_state ("re-read(false)");
    update_ui_for_album (NULL);
    return;
  }

  if (reread_cancellable != NULL)
    g_cancellable_cancel (reread_cancellable);

  reread_cancellable = g_cancellable_new ();
  sj_metadata_getter_list_albums_async (metadata,
                                        reread_cancellable,
                                        metadata_cb,
                                        NULL);
  /* reread_cancellable holds a weak reference */
  g_object_unref (reread_cancellable);
}

static void
media_added_cb (BraseroMediumMonitor	*drive,
		BraseroMedium		*medium,
                gpointer           	 data)
{
  if (extracting == TRUE) {
    /* FIXME: recover? */
  }

  sj_debug (DEBUG_CD, "Media added to device %s\n", brasero_drive_get_device (brasero_medium_get_drive (medium)));
  /* Don't call re-read if metadata is already being retreived */
  if (!get_action_state_bool ("re-read"))
    reread_cd ();
}

static void
media_removed_cb (BraseroMediumMonitor	*drive,
		  BraseroMedium		*medium,
                  gpointer           data)
{
  if (extracting == TRUE) {
    /* FIXME: recover? */
  }

  if (!eject_activated)
    disc_ejected_cb ();
  else
    eject_activated = FALSE;

  sj_debug (DEBUG_CD, "Media removed from device %s\n", brasero_drive_get_device (brasero_medium_get_drive (medium)));
}

static void
set_drive_from_device (const char *device)
{
  BraseroMediumMonitor *monitor;

  if (sj_drive) {
    g_object_unref (sj_drive);
    sj_drive = NULL;
  }

  if (! device)
    return;

  monitor = brasero_medium_monitor_get_default ();
  sj_drive = brasero_medium_monitor_get_drive (monitor, device);
  if (!sj_drive) {
    GtkWidget *dialog;
    char *message;
    message = g_strdup_printf (_("Sound Juicer could not use the CD-ROM device ‘%s’"), device);
    dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (main_window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "<b>%s</b>\n\n%s",
                                                 message,
                                                 _("HAL daemon may not be running."));
    g_free (message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return;
  }

  g_signal_connect (monitor, "medium-added", G_CALLBACK (media_added_cb), NULL);
  g_signal_connect (monitor, "medium-removed", G_CALLBACK (media_removed_cb), NULL);
}

static void
set_device (const char* device)
{
  gboolean tray_opened;

  if (device == NULL) {
    set_drive_from_device (device);
  } else if (access (device, R_OK) != 0) {
    GtkWidget *dialog;
    char *message;
    const char *error;

    error = g_strerror (errno);
    message = g_strdup_printf (_("Sound Juicer could not access the CD-ROM device ‘%s’"), device);

    dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (main_window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "<b>%s</b>\n\n%s\n%s: %s",
                                                 _("Could not read the CD"),
                                                 message,
                                                 _("Reason"),
                                                 error);
    g_free (message);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    /* Set a null device */
    set_drive_from_device (NULL);
  } else {
    set_drive_from_device (device);
  }

  sj_metadata_getter_set_cdrom (metadata, device);
  sj_extractor_set_device (sj_extractor, device);

  if (sj_drive != NULL) {
    tray_opened = brasero_drive_is_door_open (sj_drive);
    if (tray_opened == FALSE) {
      reread_cd ();
    }

    /* Enable/disable the eject options based on wether the drive supports ejection */
    set_action_enabled ("eject", brasero_drive_can_eject (sj_drive));
  }
}

gboolean cd_drive_exists (const char *device)
{
  BraseroMediumMonitor *monitor;
  BraseroDrive *drive;
  gboolean exists;

  if (device == NULL)
    return FALSE;

  monitor = brasero_medium_monitor_get_default ();
  drive = brasero_medium_monitor_get_drive (monitor, device);
  exists = (drive != NULL);
  if (exists)
    g_object_unref (drive);

  return exists;
}

const char *
prefs_get_default_device (void)
{
  static const char * default_device = NULL;

  if (default_device == NULL) {
    BraseroMediumMonitor *monitor;

    BraseroDrive *drive;
    GSList *drives;

    monitor = brasero_medium_monitor_get_default ();
    drives = brasero_medium_monitor_get_drives (monitor, BRASERO_DRIVE_TYPE_ALL);
    if (drives == NULL)
      return NULL;

    drive = drives->data;
    default_device = brasero_drive_get_device (drive);

    g_slist_free_full (drives, g_object_unref);
  }
  return default_device;
}

/**
 * The GSettings key for the device changed
 */
static void device_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  const char *device;
  char *value;
  g_assert (strcmp (key, SJ_SETTINGS_DEVICE) == 0);

  value = g_settings_get_string (settings, key);
  if (!cd_drive_exists (value)) {
    device = prefs_get_default_device();
    if (device == NULL) {
#ifndef IGNORE_MISSING_CD
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (main_window),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "<b>%s</b>\n\n%s",
                                                   _("No CD-ROM drives found"),
                                                   _("Sound Juicer could not find any CD-ROM drives to read."));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      exit (1);
#endif
    }
  } else {
    device = value;
  }
  set_device (device);
  g_free (value);
}

static void profile_changed_cb (GSettings *settings, const gchar *key, gpointer user_data)
{
  GstEncodingProfile *profile;
  char *media_type;

  g_assert (strcmp (key, SJ_SETTINGS_AUDIO_PROFILE) == 0);
  media_type = g_settings_get_string (settings, key);
  profile = rb_gst_get_encoding_profile (media_type);
  g_free (media_type);
  if (profile != NULL)
    g_object_set (sj_extractor, "profile", profile, NULL);

  if (profile == NULL || !sj_extractor_supports_profile(profile)) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_NONE,
                                     _("The currently selected audio profile is not available on your installation."));
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Quit"), GTK_RESPONSE_REJECT);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Change Profile"), GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response != GTK_RESPONSE_REJECT) {
      gtk_widget_destroy (dialog);
      show_preferences_dialog ();
    } else {
      /* Can't use gtk_main_quit here, we may be outside the main loop */
      exit(0);
    }
  }

  if (profile != NULL)
    gst_encoding_profile_unref (profile);
}

/**
 * Clicked on Reread in the UI (button/menu)
 */
static void on_reread_activate (GSimpleAction *action, GVariant *parameter, gpointer data)
{
  gtk_info_bar_response (GTK_INFO_BAR (submit_bar), SJ_RESPONSE_REREAD);
  reread_cd ();
}

/**
 * Move focus to next widget when GtkEntry is activated
 */
G_MODULE_EXPORT void on_activate_move_focus (GtkWidget *widget,
                                             gpointer   data)
{
  gboolean ret_val;

  g_signal_emit_by_name (widget, "move-focus", GTK_DIR_TAB_FORWARD, &ret_val);
}

/**
 * If path is selected call func on all selected rows, if not call
 * func only on path.
 */
static void
tree_path_or_selection_foreach (GtkTreeView                 *tree_view,
                                GtkTreePath                 *path,
                                GtkTreeSelectionForeachFunc  func,
                                gpointer                     data)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (tree_view);
  if (gtk_tree_selection_path_is_selected (selection, path)) {
    gtk_tree_selection_selected_foreach (selection,
                                         func,
                                         data);
  } else {
    GtkTreeModel *model;
    GtkTreeIter iter;
    model = gtk_tree_view_get_model (tree_view);
    if (gtk_tree_model_get_iter (model, &iter, path))
      func (model, path, &iter, data);
  }
}

/**
 * Called by on_extract_toggled to update the Extract column for all
 * the selected rows
 */
static void
on_extract_toggled_foreach (GtkTreeModel *model,
                            GtkTreePath  *path,
                            GtkTreeIter  *iter,
                            gpointer      data)
{
  gboolean extract = GPOINTER_TO_INT (data);
  gboolean old_extract;

  gtk_tree_model_get (model, iter,
                      COLUMN_EXTRACT, &old_extract,
                      -1);
  if (extract != old_extract) {
    gtk_list_store_set (GTK_LIST_STORE (model), iter,
                        COLUMN_EXTRACT, extract,
                        -1);
    /* Update number of selected tracks */
    if (extract) {
      no_of_tracks_selected++;
    } else {
      no_of_tracks_selected--;
    }
  }
}

/**
 * Called when the user clicked on the Extract column check boxes
 */
static void
on_extract_toggled (GtkCellRendererToggle *cellrenderertoggle,
                    gchar                 *path_str,
                    gpointer               user_data)
{
  gboolean extract;
  GtkTreePath *path;
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (track_store),
                                            &iter,
                                            path_str))
      return;
  gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter,
                      COLUMN_EXTRACT, &extract,
                      -1);
  /* extract is the old state here, so toggle */
  extract = !extract;
  path = gtk_tree_path_new_from_string (path_str);
  tree_path_or_selection_foreach (GTK_TREE_VIEW (track_listview),
                                  path,
                                  on_extract_toggled_foreach,
                                  GINT_TO_POINTER (extract));
  gtk_tree_path_free (path);
  /* Update the Extract button */
  gtk_widget_set_sensitive (extract_button, no_of_tracks_selected > 0);
  /* Enable and disable the Select/Deselect All buttons */
  if (no_of_tracks_selected == total_no_of_tracks) {
    set_action_enabled ("deselect-all", TRUE);
    set_action_enabled ("select-all", FALSE);
  } else if (no_of_tracks_selected == 0) {
    set_action_enabled ("deselect-all", FALSE);
    set_action_enabled ("select-all", TRUE);
  } else {
    set_action_enabled ("select-all", TRUE);
    set_action_enabled ("deselect-all", TRUE);
  }
}

static void
on_cell_changed_foreach (GtkTreeModel *model,
                         GtkTreePath  *path,
                         GtkTreeIter  *iter,
                         gpointer      user_data)
{
  CellCbContext *context = user_data;

  /* Updating the model for the cell being edited aborts the edit so
     avoid it and set it in on_cell_editing_done instead */
  if (gtk_tree_path_compare (path, context->editable_path) != 0)
    gtk_list_store_set (GTK_LIST_STORE (model), iter,
                        context->column, context->text,
                        -1);
}

/**
 * Update the selected rows as the user types
 */
static void
on_cell_changed (GtkEditable *editable,
                 gpointer     user_data)
{
  CellCbContext *context = user_data;

  context->text = gtk_editable_get_chars (editable, 0, -1);
  tree_path_or_selection_foreach (GTK_TREE_VIEW (track_listview),
                                  context->editable_path,
                                  on_cell_changed_foreach,
                                  context);
  g_free (context->text);
}

static void
on_cell_editing_done_foreach (GtkTreeModel *model,
                              GtkTreePath  *path,
                              GtkTreeIter  *iter,
                              gpointer      user_data)
{
  CellCbContext *context = user_data;
  TrackDetails *track;

  /* COLUMN_DETAILS isn't updated during editing so update it now */
  gtk_tree_model_get (GTK_TREE_MODEL (track_store), iter,
                      COLUMN_DETAILS, &track,
                      -1);
  SJ_BEGIN_IGNORE_SWITCH_ENUM
  switch (context->column) {
  case COLUMN_TITLE:
    g_free (track->title);
    track->title = g_strdup (context->text);
    break;
  case COLUMN_ARTIST:
    g_free (track->artist);
    track->artist = g_strdup (context->text);
    if (track->artist_sortname) {
      g_free (track->artist_sortname);
      track->artist_sortname = NULL;
    }
    if (track->artist_id) {
      g_free (track->artist_id);
      track->artist_id = NULL;
    }
    break;
  case COLUMN_COMPOSER:
    g_free (track->composer);
    track->composer = g_strdup (context->text);
    if (track->composer_sortname) {
      g_free (track->composer_sortname);
      track->composer_sortname = NULL;
    }
    break;
  default:
    g_warning ("Unknown column %d in on_cell_editing_done_foreach",
               context->column);
  }
  SJ_END_IGNORE_SWITCH_ENUM
}

static void
on_cell_editing_canceled_foreach (GtkTreeModel *model,
                                  GtkTreePath  *path,
                                  GtkTreeIter  *iter,
                                  gpointer      user_data)
{
  CellCbContext *context = user_data;
  TrackDetails *track;
  char *text;

  /* COLUMN_DETAILS isn't updated during editing so get original
     values back from it. */
  gtk_tree_model_get (model, iter, COLUMN_DETAILS, &track, -1);
  SJ_BEGIN_IGNORE_SWITCH_ENUM
  switch (context->column) {
  case COLUMN_TITLE:
    text = track->title;
    break;
  case COLUMN_ARTIST:
    text = track->artist;
    break;
  case COLUMN_COMPOSER:
    text = track->composer;
    break;
  default:
    g_warning ("Unknown column %d in on_cell_editing_canceled_foreach",
               context->column);
    return;
  }
 SJ_END_IGNORE_SWITCH_ENUM
  gtk_list_store_set (track_store, iter, context->column, text, -1);
}

/**
 *  Called when the user finishes or cancels editing a track. Store
 *  changes/restore original values & disconnect callbacks on the
 *  GtkCellEditable.
 */
static void
on_cell_editing_done (GtkCellEditable *cell_editable,
                      gpointer         user_data)
{
  gboolean canceled;
  CellCbContext *context = user_data;

  g_signal_handlers_disconnect_by_data (cell_editable, context);
  g_object_get (G_OBJECT (cell_editable), "editing-canceled", &canceled, NULL);
  if (canceled) {
    tree_path_or_selection_foreach (GTK_TREE_VIEW (track_listview),
                                    context->editable_path,
                                    on_cell_editing_canceled_foreach,
                                    context);
  } else { /* We need to update the track details in COLUMN_DETAILS */
    GtkTreeIter iter;
    context->text = gtk_editable_get_chars (GTK_EDITABLE (cell_editable), 0, -1);
    /* Update the model for the cell that has been edited */
    gtk_tree_model_get_iter (GTK_TREE_MODEL (track_store),
                             &iter, context->editable_path);
    gtk_list_store_set (track_store, &iter, context->column, context->text, -1);
    /* Update COLUMN_DETAILS */
    tree_path_or_selection_foreach (GTK_TREE_VIEW (track_listview),
                                    context->editable_path,
                                    on_cell_editing_done_foreach,
                                    context);
    g_free (context->text);
  }
  gtk_tree_path_free (context->editable_path);
}

/**
 * Called when the user starts editing a track. Setup callbacks on the
 * GtkCellEditable to support editing multiple rows at once.
 */
static void
on_cell_editing_started (GtkCellRenderer *renderer,
                         GtkCellEditable *cell_editable,
                         gchar           *path,
                         gpointer         user_data)
{
  cell_editing_context.editable_path = gtk_tree_path_new_from_string (path);
  cell_editing_context.column = GPOINTER_TO_UINT (user_data);
  g_signal_connect (cell_editable, "changed",
                    G_CALLBACK (on_cell_changed), &cell_editing_context);
  g_signal_connect (cell_editable, "editing-done",
                    G_CALLBACK (on_cell_editing_done), &cell_editing_context);
}

/*
 * Remove all of the data which is intrinsic to the album being correctly
 * detected, such as MusicBrainz IDs, ASIN, and Wikipedia links.
 */
static void
remove_musicbrainz_ids (AlbumDetails *album)
{
  GList *l;
#define UNSET(id) g_free (album->id);           \
  album->id = NULL;

  UNSET (album_id);
  UNSET (artist_id);
  UNSET (asin);
  UNSET (discogs);
  UNSET (wikipedia);
#undef UNSET

#define UNSET(id) g_free (track->id);           \
  track->id = NULL;

  for (l = album->tracks; l; l = l->next) {
    TrackDetails *track = l->data;
    UNSET (track_id);
    UNSET (artist_id);
  }
#undef UNSET
}

G_MODULE_EXPORT void on_title_edit_changed(GtkEditable *widget, gpointer user_data) {
  g_return_if_fail (current_album != NULL);

  remove_musicbrainz_ids (current_album);

  if (current_album->title) {
    g_free (current_album->title);
  }
  current_album->title = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */
}

/**
 * Return TRUE if s1 and s2 are equal according to g_utf8_casefold or
 * if they are NULL, NUL or just ascii space. NULL, NUL and space are
 * considered equal
 */
static gboolean str_case_equal (const char*s1, const char *s2)
{
  gboolean retval;
  char *t1, *t2;

  if (sj_str_is_empty (s1) && sj_str_is_empty (s2))
    return TRUE;

  /* is_empty can handle NULL pointers but g_utf8_casefold cannot */
  if (!s1 || !s2)
    return FALSE;

  t1 = g_utf8_casefold (s1, -1);
  t2 = g_utf8_casefold (s2, -1);
  retval = g_str_equal (t1, t2);
  g_free (t1);
  g_free (t2);
  return retval;
}

G_MODULE_EXPORT void on_person_edit_changed(GtkEditable *widget,
                                            gpointer user_data) {
  GtkTreeIter iter;
  gboolean ok; /* TRUE if iter is valid */
  TrackDetails *track;
  gchar *former_album_person = NULL;
  /* Album person name and sortname */
  gchar **album_person_name, **album_person_sortname;
  /* Offsets for track person name and sortname */
  int off_person_name, off_person_sortname;
  int column; /* column for person in listview */

  g_return_if_fail (current_album != NULL);

  if (widget == GTK_EDITABLE (artist_entry)) {
    column = COLUMN_ARTIST;
    album_person_name = &current_album->artist;
    album_person_sortname = &current_album->artist_sortname;
    off_person_name = G_STRUCT_OFFSET (TrackDetails, artist);
    off_person_sortname = G_STRUCT_OFFSET (TrackDetails,
                                           artist_sortname);
  } else if (widget == GTK_EDITABLE (composer_entry)) {
    column = COLUMN_COMPOSER;
    album_person_name = &current_album->composer;
    album_person_sortname = &current_album->composer_sortname;
    off_person_name = G_STRUCT_OFFSET (TrackDetails, composer);
    off_person_sortname = G_STRUCT_OFFSET (TrackDetails,
                                           composer_sortname);
  } else {
    g_warning (_("Unknown widget calling on_person_edit_changed."));
    return;
  }

  remove_musicbrainz_ids (current_album);

  /* Unset the sortname field, as we can't change it automatically */
  if (*album_person_sortname) {
    g_free (*album_person_sortname);
    *album_person_sortname = NULL;
  }

  if (*album_person_name) {
    former_album_person = *album_person_name;
  }

  /* get all the characters */
  *album_person_name = gtk_editable_get_chars (widget, 0, -1);

  /* Set the person field in each tree row */
  for (ok = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (track_store), &iter);
       ok;
       ok = gtk_tree_model_iter_next (GTK_TREE_MODEL (track_store), &iter)) {
    gchar *current_track_person;
    gchar **track_person_name, **track_person_sortname;

    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, column,
                        &current_track_person, -1);

    /* Change track person if it matched album person before the change */
    if (!str_case_equal (current_track_person, former_album_person) &&
        !str_case_equal (current_track_person, *album_person_name))
      continue;

    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, COLUMN_DETAILS,
                        &track, -1);
    track_person_name     = G_STRUCT_MEMBER_P (track, off_person_name);
    track_person_sortname = G_STRUCT_MEMBER_P (track, off_person_sortname);

    g_free (*track_person_name);
    *track_person_name = g_strdup (*album_person_name);

    /* Unset the sortname field, as we can't change it automatically */
    if (*track_person_sortname) {
      g_free (*track_person_sortname);
      *track_person_sortname = NULL;
    }

    gtk_list_store_set (track_store, &iter, column, *track_person_name, -1);
  }
  g_free (former_album_person);
}

G_MODULE_EXPORT void on_genre_edit_changed(GtkEditable *widget, gpointer user_data) {
  g_return_if_fail (current_album != NULL);
  if (current_album->genre) {
    g_free (current_album->genre);
  }
  current_album->genre = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */
  /* Toggle visibility of composer fields based on genre */
  composer_show_hide (current_album->genre);
}

G_MODULE_EXPORT void on_year_edit_changed(GtkEditable *widget, gpointer user_data) {
  const gchar* yearstr;
  int year;

  g_return_if_fail (current_album != NULL);

  yearstr = gtk_entry_get_text (GTK_ENTRY (widget));
  year = atoi (yearstr);
  if (year > 0) {
    if (current_album->release_date) {
      gst_date_time_unref (current_album->release_date);
    }
    current_album->release_date = gst_date_time_new_y (year);
  }
}

G_MODULE_EXPORT void on_disc_number_edit_changed(GtkEditable *widget, gpointer user_data) {
    const gchar* discstr;
    int disc_number;

    g_return_if_fail (current_album != NULL);
    discstr = gtk_entry_get_text (GTK_ENTRY (widget));
    disc_number = atoi(discstr);
    current_album->disc_number = disc_number;
}

static void on_contents_activate(GSimpleAction *action, GVariant *parameter, gpointer data) {
  GError *error = NULL;

  gtk_show_uri (NULL, "help:sound-juicer", GDK_CURRENT_TIME, &error);
  if (error) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Could not display help for Sound Juicer\n"
                                       "%s"),
                                     error->message);
    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy),
                              dialog);
    gtk_widget_show (dialog);
    g_error_free (error);
  }
}

/**
 * Performs various checks to ensure CD duplication is available.
 * If this is found TRUE is returned, otherwise FALSE is returned.
 */
static gboolean
is_cd_duplication_available(void)
{
  gchar *brasero_cd_burner, *cdrdao;
  BraseroMediumMonitor *monitor;
  GSList *drives, *iter;

  /* First check the brasero tool is available in the path */
  brasero_cd_burner = g_find_program_in_path ("brasero");
  if (brasero_cd_burner == NULL) {
    return FALSE;
  }
  g_free(brasero_cd_burner);

  /* Second check the cdrdao tool is available in the path */
  cdrdao = g_find_program_in_path ("cdrdao");
  if (cdrdao == NULL) {
    return FALSE;
  }
  g_free(cdrdao);

  /* Now check that there is at least one cd recorder available */
  monitor = brasero_medium_monitor_get_default ();
  drives = brasero_medium_monitor_get_drives (monitor, BRASERO_DRIVE_TYPE_ALL);

  for (iter = drives; iter; iter = iter->next) {
    BraseroDrive *drive;

    drive = iter->data;
    if (brasero_drive_can_write (drive)) {
      g_slist_free_full (drives, g_object_unref);
      return TRUE;
    }
  }

  g_slist_free_full (drives, g_object_unref);
  return FALSE;
}

static void
ui_set_retrieving_metadata (gboolean state)
{
  if (state) {
    gtk_statusbar_push(GTK_STATUSBAR(status_bar), 1,
                       _("Retrieving track listing…please wait."));
    g_application_mark_busy (g_application_get_default ());
  } else {
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), 1);
    g_application_unmark_busy (g_application_get_default ());
  }
}

static void
reread_state_changed_cb (gchar    *group_name,
                         gchar    *action_name,
                         GVariant *value,
                         gpointer  user_data)
{
  static gboolean state = FALSE;

  if (g_variant_get_boolean (value) == state)
    return;

  state = !state;
  ui_set_retrieving_metadata (state);
}

GActionEntry app_entries[] = {
  { "re-read", on_reread_activate, NULL, "false", NULL },
  { "duplicate", on_duplicate_activate, NULL, NULL, NULL },
  { "eject", on_eject_activate, NULL, NULL, NULL },
  { "submit-tracks", on_submit_activate, NULL, NULL, NULL },
  { "preferences", on_preferences_activate, NULL, NULL, NULL },
  { "about", on_about_activate, NULL, NULL, NULL },
  { "help", on_contents_activate, NULL, NULL, NULL },
  { "quit", on_quit_activate, NULL, NULL, NULL }
};

GActionEntry win_entries[] = {
  { "play", on_play_activate, NULL, NULL, NULL },
  { "next-track", on_next_track_activate, NULL, NULL, NULL },
  { "previous-track", on_previous_track_activate, NULL, NULL, NULL },
  { "select-all", on_select_all_activate, NULL, NULL, NULL },
  { "deselect-all", on_deselect_all_activate, NULL, NULL, NULL }
};

static GtkCellRenderer*
add_editable_listview_column (const char       *title,
                              const ViewColumn  model_column)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  renderer = sj_cell_renderer_text_new ();
  g_signal_connect (renderer,
                    "editing-started",
                    G_CALLBACK (on_cell_editing_started),
                    GUINT_TO_POINTER (model_column));
  g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
  column = gtk_tree_view_column_new_with_attributes (title, renderer,
                                                     "text", model_column,
                                                     NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
  return renderer;
}

/*
 * Action accelerator definitions. For each action the first entry is
 * the detailed name of the action followed by up to 3 accelerators
 * followed by NULL.
 */
static const gchar *action_accels[][5] = {
  { "win.play", "<Primary>p", NULL },
  { "win.next-track", "<Primary>n", NULL },
  { "win.previous-track", "<Primary>b", NULL},
  { "win.select-all", "<Primary>a", NULL},
  { "win.deselect-all", "<Primary><Shift>a", NULL}
};

static void
startup_cb (GApplication *app, gpointer user_data)
{
  gsize i;
  GtkTreeSelection *selection;
  GError *error = NULL;

  g_setenv ("PULSE_PROP_media.role", "music", TRUE);

  sj_debug_init ();

  gtk_window_set_default_icon_name ("sound-juicer");

  brasero_media_library_start ();

  metadata = sj_metadata_getter_new ();

  sj_settings = g_settings_new ("org.gnome.sound-juicer");

  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_DEVICE,
                    (GCallback)device_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_EJECT,
                    (GCallback)eject_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_OPEN,
                    (GCallback)open_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_BASEURI,
                    (GCallback)baseuri_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_STRIP,
                    (GCallback)strip_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_AUDIO_PROFILE,
                    (GCallback)profile_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_PARANOIA,
                    (GCallback)paranoia_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_PATH_PATTERN,
                    (GCallback)path_pattern_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_FILE_PATTERN,
                    (GCallback)file_pattern_changed_cb, NULL);
  g_signal_connect (sj_settings, "changed::"SJ_SETTINGS_AUDIO_VOLUME,
                    (GCallback)audio_volume_changed_cb, NULL);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   NULL);
  g_signal_connect (app,
                    "action-state-changed::re-read",
                    G_CALLBACK (reread_state_changed_cb),
                    NULL);

  builder = gtk_builder_new_from_resource ("/org/gnome/sound-juicer/sound-juicer.ui");

  gtk_builder_connect_signals (builder, NULL);

  main_window           = GET_WIDGET ("main_window");
  submit_bar            = GET_WIDGET ("submit_bar");
  submit_label          = GET_WIDGET ("submit_label");
  reload_button         = GET_WIDGET ("submit_bar_reload_button");
  submit_button         = GET_WIDGET ("submit_bar_submit_button");
  title_entry           = GET_WIDGET ("title_entry");
  artist_entry          = GET_WIDGET ("artist_entry");
  composer_label        = GET_WIDGET ("composer_label");
  composer_entry        = GET_WIDGET ("composer_entry");
  duration_label        = GET_WIDGET ("duration_label");
  genre_entry           = GET_WIDGET ("genre_entry");
  year_entry            = GET_WIDGET ("year_entry");
  disc_number_entry     = GET_WIDGET ("disc_number_entry");
  track_listview        = GET_WIDGET ("track_listview");
  extract_button        = GET_WIDGET ("extract_button");
  play_button           = GET_WIDGET ("play_button");
  select_button         = GET_WIDGET ("select_button");
  status_bar            = GET_WIDGET ("status_bar");
  entry_grid            = GET_WIDGET ("entry_grid");

  sj_main_window_init (GTK_WINDOW (main_window));

  g_action_map_add_action_entries (G_ACTION_MAP (main_window),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   NULL);

  g_signal_connect (app, "action-enabled-changed::submit-tracks",
                    G_CALLBACK (submit_tracks_enabled_changed_cb), NULL);

  gtk_button_set_label(GTK_BUTTON(select_button), _("Select None"));
  gtk_actionable_set_action_name(GTK_ACTIONABLE(select_button), "win.deselect-all");
  gtk_actionable_set_action_name(GTK_ACTIONABLE(play_button), "win.play");

  /* window actions are only available via shortcuts */
  for (i = 0; i < G_N_ELEMENTS (action_accels); i++)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           action_accels[i][0],
                                           &action_accels[i][1]);

  { /* ensure that the play/pause button's size is constant */
    GtkWidget *fake_button1, *fake_button2;
    GtkSizeGroup *size_group;

    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    fake_button1 = gtk_button_new_with_label (_("_Play"));
    gtk_button_set_use_underline (GTK_BUTTON (fake_button1), TRUE);
    gtk_size_group_add_widget (size_group, fake_button1);
    g_signal_connect_swapped (play_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button1);

    fake_button2 = gtk_button_new_with_label (_("_Pause"));
    gtk_button_set_use_underline (GTK_BUTTON (fake_button2), TRUE);
    gtk_size_group_add_widget (size_group, fake_button2);
    g_signal_connect_swapped (play_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button2);

    gtk_size_group_add_widget (size_group, play_button);
    g_object_unref (G_OBJECT (size_group));
  }

  { /* ensure that the extract/play button's size is constant */
    GtkWidget *fake_button1, *fake_button2;
    GtkSizeGroup *size_group;

    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    fake_button1 = gtk_button_new_with_label (_("E_xtract"));
    gtk_button_set_use_underline (GTK_BUTTON (fake_button1), TRUE);
    gtk_size_group_add_widget (size_group, fake_button1);
    g_signal_connect_swapped (extract_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button1);

    fake_button2 = gtk_button_new_with_label (_("_Stop"));
    gtk_button_set_use_underline (GTK_BUTTON (fake_button2), TRUE);
    gtk_size_group_add_widget (size_group, fake_button2);
    g_signal_connect_swapped (extract_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button2);

    gtk_size_group_add_widget (size_group, extract_button);
    g_object_unref (G_OBJECT (size_group));
  }

  { /* ensure that the select/unselect button's size is constant */
    GtkWidget *fake_button1, *fake_button2;
    GtkSizeGroup *size_group;

    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    fake_button1 = gtk_button_new_with_label (_("Select All"));
    gtk_size_group_add_widget (size_group, fake_button1);
    g_signal_connect_swapped (select_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button1);

    fake_button2 = gtk_button_new_with_label (_("Select None"));
    gtk_size_group_add_widget (size_group, fake_button2);
    g_signal_connect_swapped (select_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button2);

    gtk_size_group_add_widget (size_group, select_button);
    g_object_unref (G_OBJECT (size_group));
  }

  setup_genre_entry (genre_entry);

  track_store = gtk_list_store_new (COLUMN_TOTAL, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_POINTER);
  gtk_tree_view_set_model (GTK_TREE_VIEW (track_listview), GTK_TREE_MODEL (track_store));
  {
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    toggle_renderer = gtk_cell_renderer_toggle_new ();
    g_signal_connect (toggle_renderer, "toggled", G_CALLBACK (on_extract_toggled), NULL);
    column = gtk_tree_view_column_new_with_attributes ("",
                                                       toggle_renderer,
                                                       "active", COLUMN_EXTRACT,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, _("Track"));
    gtk_tree_view_column_set_expand (column, FALSE);
    gtk_tree_view_column_set_resizable (column, FALSE);
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_add_attribute (column, renderer, "text", COLUMN_NUMBER);
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, "xalign", 0.0, NULL);
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer, number_cell_icon_data_cb, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    title_renderer = add_editable_listview_column (_("Title"), COLUMN_TITLE);
    artist_renderer = add_editable_listview_column (_("Artist"), COLUMN_ARTIST);
    composer_renderer = add_editable_listview_column (_("Composer"), COLUMN_COMPOSER);
    composer_column = gtk_tree_view_get_column (GTK_TREE_VIEW (track_listview),
                                                gtk_tree_view_get_n_columns (GTK_TREE_VIEW (track_listview)) - 1);
    gtk_tree_view_column_set_visible (composer_column, FALSE);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Duration"),
                                                       renderer,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer, duration_cell_data_cb, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
  }

  sj_extractor = SJ_EXTRACTOR (sj_extractor_new());
  error = sj_extractor_get_new_error (sj_extractor);
  if (error) {
    error_on_start (error);
    exit (1);
  }

  update_ui_for_album (NULL);

  sj_play_init ();

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (track_listview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  baseuri_changed_cb (sj_settings, SJ_SETTINGS_BASEURI, NULL);
  path_pattern_changed_cb (sj_settings, SJ_SETTINGS_PATH_PATTERN, NULL);
  file_pattern_changed_cb (sj_settings, SJ_SETTINGS_FILE_PATTERN, NULL);
  profile_changed_cb (sj_settings, SJ_SETTINGS_AUDIO_PROFILE, NULL);
  paranoia_changed_cb (sj_settings, SJ_SETTINGS_PARANOIA, NULL);
  strip_changed_cb (sj_settings, SJ_SETTINGS_STRIP, NULL);
  eject_changed_cb (sj_settings, SJ_SETTINGS_EJECT, NULL);
  open_changed_cb (sj_settings, SJ_SETTINGS_OPEN, NULL);
  audio_volume_changed_cb (sj_settings, SJ_SETTINGS_AUDIO_VOLUME, NULL);
  if (sj_device == NULL && uris == NULL) {
    /* FIXME: this should set the device gsettings key to a meaningful
     * value if it's empty (which is the case until the user changes it in
     * the prefs)
     */
    device_changed_cb (sj_settings, SJ_SETTINGS_DEVICE, NULL);
  } else {
    if (sj_device)
      set_device (sj_device);
    else {
      char *d;

      /* Mash up the CDDA URIs into a device path */
      if (g_str_has_prefix (uris[0], "cdda://")) {
        gint len;
        d = g_strdup_printf ("/dev/%s%c", uris[0] + strlen ("cdda://"), '\0');
        /* Take last '/' out of path, or set_device thinks it is part of the device name */
      len = strlen (d);
      if (d[len - 1] == '/')
          d [len - 1] = '\0';
      set_device (d);
      g_free (d);
      } else {
        device_changed_cb (sj_settings, SJ_SETTINGS_DEVICE, NULL);
      }
    }
  }

  if (sj_extractor_supports_encoding (&error) == FALSE) {
    error_on_start (error);
    return;
  }

  /* Set whether duplication of a cd is available using the brasero tool */
  set_action_enabled ("duplicate", FALSE);
  duplication_enabled = is_cd_duplication_available();

  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (main_window));
  gtk_widget_show (main_window);
}

static void
shutdown_cb (GApplication *app, gpointer user_data)
{
  sj_metadata_helper_cleanup ();
  g_object_unref (sj_base_uri);
  g_object_unref (metadata);
  g_object_unref (sj_extractor);
  g_object_unref (sj_settings);
  brasero_media_library_stop ();
}

static void
activate_cb (GApplication *app, gpointer user_data)
{
  gtk_window_present (GTK_WINDOW (main_window));
}

static GAction*
lookup_action (const gchar* name)
{
  GActionMap *map;
  GAction *action;

  map = G_ACTION_MAP (g_application_get_default ());
  action = g_action_map_lookup_action (map, name);
  if (action == NULL)
    action = g_action_map_lookup_action (G_ACTION_MAP (main_window), name);
  if (action == NULL)
    g_warning ("Action '%s' not found", name);

  return action;
}

static gboolean
get_action_state_bool (const gchar* name)
{
  gboolean state = FALSE;
  GAction *action;

  action = lookup_action (name);
  if (action != NULL) {
    GVariant *value = g_action_get_state (action);
    state = g_variant_get_boolean (value);
    g_variant_unref (value);
  }

  return state;
}

static void
set_action_state (const gchar *detailed_name)
{
  gchar *name;
  GVariant *value;
  GError *error = NULL;

  if (g_action_parse_detailed_name (detailed_name, &name, &value, &error)) {
    GSimpleAction *action;

    action = G_SIMPLE_ACTION (lookup_action (name));
    if (action != NULL)
      g_simple_action_set_state (action, value);

    g_free (name);
    g_variant_unref (value);
  } else {
    g_warning ("Error parsing detailed action name '%s' - %s",
               detailed_name, error->message);
    g_error_free (error);
  }
}

void set_action_enabled (const char *name, gboolean enabled)
{
  GAction *action;

  action = lookup_action (name);
  if (action != NULL)
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

int main (int argc, char **argv)
{
  GtkApplication *app;
  const GOptionEntry entries[] = {
    { "auto-start", 'a', 0, G_OPTION_ARG_NONE, &autostart, N_("Start extracting immediately"), NULL },
    { "play", 'p', 0, G_OPTION_ARG_NONE, &autoplay, N_("Start playing immediately"), NULL},
    { "device", 'd', 0, G_OPTION_ARG_FILENAME, &sj_device, N_("What CD device to read"), N_("DEVICE") },
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &uris, N_("URI to the CD device to read"), NULL },
    { NULL }
  };
  guint status = 0;

  bindtextdomain (PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);

  g_set_application_name (_("Sound Juicer"));

  app = gtk_application_new ("org.gnome.sound-juicer",
                             G_APPLICATION_FLAGS_NONE);
  g_application_add_main_option_entries (G_APPLICATION (app), entries);
  g_application_add_option_group (G_APPLICATION (app), gst_init_get_option_group ());
  g_application_add_option_group (G_APPLICATION (app), brasero_media_get_option_group ());

  g_object_set (app, "register-session", TRUE, NULL);
  g_signal_connect (app, "startup", G_CALLBACK (startup_cb), NULL);
  g_signal_connect (app, "shutdown", G_CALLBACK (shutdown_cb), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate_cb), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);

  return status;
}
