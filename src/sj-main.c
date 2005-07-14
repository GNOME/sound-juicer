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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 *          Mike Hearn  <mike@theoretic.com>
 */

#include "sound-juicer.h"

#include <string.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <nautilus-burn-drive.h>
#include <libgnome/gnome-help.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <profiles/gnome-media-profiles.h>
#include <gst/gst.h>

#include "bacon-message-connection.h"
#include "sj-about.h"
#include "sj-metadata.h"
#include "sj-metadata-musicbrainz.h"
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"
#include "sj-prefs.h"
#include "sj-play.h"
#include "sj-volume.h"

gboolean on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data);

static void reread_cd (gboolean ignore_no_media);
static void update_ui_for_album (AlbumDetails *album);

/* Prototypes for the signal blocking/unblocking in update_ui_for_album */
void on_title_edit_changed(GtkEditable *widget, gpointer user_data);
void on_artist_edit_changed(GtkEditable *widget, gpointer user_data);

GladeXML *glade;

SjMetadata *metadata;
SjExtractor *extractor;

GConfClient *gconf_client;

GtkWidget *main_window;
static GtkWidget *title_entry, *artist_entry, *duration_label, *genre_entry;
static GtkWidget *track_listview, *extract_button, *play_button;
static GtkWidget *status_bar;
static GtkWidget *extract_menuitem, *play_menuitem, *select_all_menuitem, *deselect_all_menuitem;
GtkListStore *track_store;
static BaconMessageConnection *connection;
GtkCellRenderer *toggle_renderer, *title_renderer, *artist_renderer;

const char *base_uri, *path_pattern, *file_pattern;
NautilusBurnDrive *drive = NULL;
gboolean strip_chars;
gboolean eject_finished;
gboolean extracting = FALSE;
static gboolean tray_opened = FALSE;
static guint poll_id = 0;
static gint total_no_of_tracks;
static gint no_of_tracks_selected;
static AlbumDetails *current_album;

gboolean autostart = FALSE, autoplay = FALSE;

#define DEFAULT_PARANOIA 4
#define RAISE_WINDOW "raise-window"
#define SOURCE_GLADE "../data/sound-juicer.glade"
#define INSTALLED_GLADE DATADIR"/sound-juicer/sound-juicer.glade"

static void
add_stock_icon (GtkIconFactory *sj_icon_factory,
                const gchar *stock_id,
                GtkIconSize  size,
                const gchar *file_name)
{
  GtkIconSource *source;
  GtkIconSet    *set;
  GdkPixbuf     *pixbuf;

  source = gtk_icon_source_new ();

  gtk_icon_source_set_size (source, size);
  gtk_icon_source_set_size_wildcarded (source, FALSE);

  pixbuf = gdk_pixbuf_new_from_file (file_name, NULL);

  gtk_icon_source_set_pixbuf (source, pixbuf);

  g_object_unref (pixbuf);

  set = gtk_icon_set_new ();

  gtk_icon_set_add_source (set, source);
  gtk_icon_source_free (source);

  gtk_icon_factory_add (sj_icon_factory, stock_id, set);

  gtk_icon_set_unref (set);
}

void
sj_stock_init (void)
{
  static gboolean initialized = FALSE;
  static GtkIconFactory *sj_icon_factory = NULL;

  static GtkStockItem sj_stock_items[] =
  {
    { SJ_STOCK_PLAYING, NULL, 0, 0, NULL },
    { SJ_STOCK_RECORDING, NULL, 0, 0, NULL },
    { SJ_STOCK_EXTRACT, "E_xtract", GDK_CONTROL_MASK, GDK_Return, NULL }
  };

  if (initialized)
    return;

  sj_icon_factory = gtk_icon_factory_new ();

  add_stock_icon (sj_icon_factory, SJ_STOCK_PLAYING, GTK_ICON_SIZE_MENU, PKGDATADIR"/sj-play.png");
  add_stock_icon (sj_icon_factory, SJ_STOCK_RECORDING, GTK_ICON_SIZE_MENU, PKGDATADIR"/sj-record.png");
  gtk_icon_factory_add (sj_icon_factory, SJ_STOCK_EXTRACT, gtk_icon_factory_lookup_default (GTK_STOCK_CDROM));

  gtk_icon_factory_add_default (sj_icon_factory);

  gtk_stock_add_static (sj_stock_items, G_N_ELEMENTS (sj_stock_items));

  initialized = TRUE;
}

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

static void error_on_start (GError *error)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "<b>%s</b>\n\n%s: %s.\n%s",
                                   _("Could not start Sound Juicer"),
                                   _("Reason"),
                                   error->message,
                                   _("Please consult the documentation for assistance."));
  gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
  gtk_dialog_run (GTK_DIALOG (dialog));
}

/**
 * Clicked Quit
 */
void on_quit_activate (GtkMenuItem *item, gpointer user_data)
{
  if (on_delete_event (NULL, NULL, NULL) == FALSE) {
    gtk_main_quit ();
  }
}

/**
 * Destroy signal Callback
 */
void on_destroy_signal (GtkMenuItem *item, gpointer user_data)
{
   gtk_main_quit ();
}

/**
 * Clicked Eject
 */
void on_eject_activate (GtkMenuItem *item, gpointer user_data)
{
  /* first make sure we're not playing */
  stop_playback ();

  nautilus_burn_drive_eject (drive);
}

gboolean poll_tray_opened (gpointer data)
{
  gboolean new_status;

  if (extracting == TRUE)
    return TRUE;

  new_status = nautilus_burn_drive_door_is_open (drive);
  if (new_status != tray_opened && new_status == FALSE) {
    reread_cd (TRUE);
  } else if (new_status != tray_opened && new_status == TRUE) {
    update_ui_for_album (NULL);
  }
  tray_opened = new_status;

  return TRUE;
}

gboolean on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  if (extracting) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_NONE,
                                     _("You are currently extracting a CD. Do you want to quit now or continue?"));
    gtk_dialog_add_button (GTK_DIALOG (dialog), "gtk-quit", GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Continue"), GTK_RESPONSE_REJECT);
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    
    if (response == GTK_RESPONSE_ACCEPT) {
      if (poll_id > 0) {
        g_source_remove (poll_id);
      }
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

void on_select_all_activate (GtkMenuItem *item, gpointer user_data)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), select_all_foreach_cb, GINT_TO_POINTER (TRUE));
  gtk_widget_set_sensitive (extract_button, TRUE);
  gtk_widget_set_sensitive (extract_menuitem, TRUE);
  gtk_widget_set_sensitive (select_all_menuitem, FALSE);
  gtk_widget_set_sensitive (deselect_all_menuitem, TRUE);
  no_of_tracks_selected = total_no_of_tracks;
}

void on_deselect_all_activate (GtkMenuItem *item, gpointer user_data)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), select_all_foreach_cb, GINT_TO_POINTER (FALSE));
  gtk_widget_set_sensitive (extract_button, FALSE);
  gtk_widget_set_sensitive (extract_menuitem, FALSE);
  gtk_widget_set_sensitive (deselect_all_menuitem, FALSE);
  gtk_widget_set_sensitive (select_all_menuitem,TRUE);
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
    g_object_set (G_OBJECT (cell), "stock-id", "", NULL);
    break;
  case STATE_PLAYING:
    g_object_set (G_OBJECT (cell), "stock-id", SJ_STOCK_PLAYING, NULL);
    break;
  case STATE_EXTRACTING:
    g_object_set (G_OBJECT (cell), "stock-id", SJ_STOCK_RECORDING, NULL);
    break;
  default:
    g_warning("Unhandled track state %d\n", state);
  }
}

/**
 * Utility function to update the UI for a given Album
 */
static void update_ui_for_album (AlbumDetails *album)
{
  GList *l;
  int album_duration = 0;
  char* duration_text;
  total_no_of_tracks=0;

  if (album == NULL) {
    gtk_list_store_clear (track_store);
    gtk_entry_set_text (GTK_ENTRY (title_entry), "");
    gtk_entry_set_text (GTK_ENTRY (artist_entry), "");
    gtk_entry_set_text (GTK_ENTRY (genre_entry), "");
    gtk_label_set_text (GTK_LABEL (duration_label), "");
    gtk_widget_set_sensitive (title_entry, FALSE);
    gtk_widget_set_sensitive (artist_entry, FALSE);
    gtk_widget_set_sensitive (genre_entry, FALSE);
    gtk_widget_set_sensitive (play_button, FALSE);
    gtk_widget_set_sensitive (play_menuitem, FALSE);
    gtk_widget_set_sensitive (extract_button, FALSE);
    gtk_widget_set_sensitive (extract_menuitem, FALSE);
    gtk_widget_set_sensitive (select_all_menuitem, FALSE);
    gtk_widget_set_sensitive (deselect_all_menuitem, TRUE);
  } else {
    gtk_list_store_clear (track_store);

    g_signal_handlers_block_by_func (title_entry, on_title_edit_changed, NULL);
    g_signal_handlers_block_by_func (artist_entry, on_artist_edit_changed, NULL);
    gtk_entry_set_text (GTK_ENTRY (title_entry), album->title);
    gtk_entry_set_text (GTK_ENTRY (artist_entry), album->artist);
    g_signal_handlers_unblock_by_func (title_entry, on_title_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (artist_entry, on_artist_edit_changed, NULL);

    gtk_widget_set_sensitive (title_entry, TRUE);
    gtk_widget_set_sensitive (artist_entry, TRUE);
    gtk_widget_set_sensitive (genre_entry, TRUE);
    gtk_widget_set_sensitive (play_button, TRUE);
    gtk_widget_set_sensitive (play_menuitem, TRUE);
    gtk_widget_set_sensitive (extract_button, TRUE);
    gtk_widget_set_sensitive (extract_menuitem, TRUE);
    gtk_widget_set_sensitive (select_all_menuitem, FALSE);
    gtk_widget_set_sensitive (deselect_all_menuitem, TRUE);
    
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
                          COLUMN_DURATION, track->duration,
                          COLUMN_DETAILS, track,
                          -1);
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
  }
}

/**
 * Called by the Multiple Album dialog when the user hits return in
 * the list view
 */
static void album_row_activated (GtkTreeView *treeview,
                                 GtkTreePath *arg1,
                                 GtkTreeViewColumn *arg2,
                                 gpointer user_data)
{
  GtkDialog *dialog = GTK_DIALOG (user_data);
  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

/**
 * Utility function for when there are more than one albums available
 */
AlbumDetails* multiple_album_dialog(GList *albums)
{
  static GtkWidget *dialog = NULL, *albums_listview;
  static GtkListStore *albums_store;
  static GtkTreeSelection *selection;
  AlbumDetails *album;
  GtkTreeIter iter;
  int response;

  if (dialog == NULL) {
    GtkTreeViewColumn *column;
    GtkCellRenderer *text_renderer = text_renderer = gtk_cell_renderer_text_new ();

    dialog = glade_xml_get_widget (glade, "multiple_dialog");
    g_assert (dialog != NULL);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
    albums_listview = glade_xml_get_widget (glade, "albums_listview");
    /* TODO: A little hacky */
    g_signal_connect (albums_listview, "row-activated", G_CALLBACK (album_row_activated), dialog);

    albums_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                       text_renderer,
                                                       "text", 0,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (albums_listview), column);

    column = gtk_tree_view_column_new_with_attributes (_("Artist"),
                                                       text_renderer,
                                                       "text", 1,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (albums_listview), column);
    gtk_tree_view_set_model (GTK_TREE_VIEW (albums_listview), GTK_TREE_MODEL (albums_store));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW (albums_listview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
  }

  gtk_list_store_clear (albums_store);
  for (; albums ; albums = g_list_next (albums)) {
    GtkTreeIter iter;
    AlbumDetails *album = (AlbumDetails*)(albums->data);
    gtk_list_store_append (albums_store, &iter);
    gtk_list_store_set (albums_store, &iter,
                        0, album->title,
                        1, album->artist,
                        2, album,
                        -1);
  }
  /*
   * TODO: focus is a little broken here. The first row should be
   * selected, so just hitting return works.
   */
  gtk_widget_grab_focus (albums_listview);
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_DELETE_EVENT) {
    return NULL;
  }
  gtk_tree_selection_get_selected (selection, NULL, &iter);
  gtk_tree_model_get (GTK_TREE_MODEL (albums_store), &iter, 2, &album, -1);
  return album;
}

/**
 * The GConf key for the base path changed
 */
void baseuri_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_BASEURI) == 0);
  if (entry->value == NULL) {
    base_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
  } else {
    base_uri = gconf_value_get_string (entry->value);
  }
  /* TODO: sanity check the URI somewhat */
}

/**
 * The GConf key for the directory pattern changed
 */
void path_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_PATH_PATTERN) == 0);
  if (entry->value == NULL) {
    /* TODO: this value and the value in sj-prefs need to be in one place */
    path_pattern = g_strdup ("%aa/%at");
  } else {
    path_pattern = gconf_value_get_string (entry->value);
  }
  /* TODO: sanity check the pattern */
}

/**
 * The GConf key for the filename pattern changed
 */
void file_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_FILE_PATTERN) == 0);
  if (entry->value == NULL) {
    /* TODO: this value and the value in sj-prefs need to be in one place */
    file_pattern = g_strdup ("%tN-%tt");
  } else {
    file_pattern = gconf_value_get_string (entry->value);
  }
  /* TODO: sanity check the pattern */
}

/**
 * The GConf key for the paranoia mode has changed
 */
void paranoia_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_PARANOIA) == 0);
  if (entry->value == NULL) {
    sj_extractor_set_paranoia (extractor, DEFAULT_PARANOIA);
  } else {
    int value = gconf_value_get_int (entry->value);
    if (value == 0 || value == 4 || value == 255) {
      sj_extractor_set_paranoia (extractor, value);
    }
  }
}

/**
 * The GConf key for the strip characters option changed
 */
void strip_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_STRIP) == 0);
  if (entry->value == NULL) {
    strip_chars = FALSE;
  } else {
    strip_chars = gconf_value_get_bool (entry->value);
  }
}

/**
 * The GConf key for the eject when finished option changed
 */
void eject_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_EJECT) == 0);
  if (entry->value == NULL) {
    eject_finished = FALSE;
  } else {
    eject_finished = gconf_value_get_bool (entry->value);
  }
}

static void
metadata_cb (SjMetadata *m, GList *albums, GError *error)
{
  gboolean realized = GTK_WIDGET_REALIZED (main_window);

  if (realized)
    gdk_window_set_cursor (main_window->window, NULL);
    /* Clear the statusbar message */
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), 0);
  
  if (error && !(error->code == SJ_ERROR_CD_NO_MEDIA)) {
    GtkWidget *dialog;
    
    dialog = gtk_message_dialog_new (realized ? GTK_WINDOW (main_window) : NULL, 0,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "<b>%s</b>\n\n%s\n%s: %s",
                                     _("Could not read the CD"),
                                     _("Sound Juicer could not read the track listing on this CD."),
                                     _("Reason"),
                                     error->message);
    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_error_free (error);
    update_ui_for_album (NULL);
    return;
  }

  /* Free old album details */
  if (current_album != NULL) {
    album_details_free (current_album);
    current_album = NULL;
  }
  /* Set the new current album pointer */
  if (albums == NULL) {
    current_album = NULL;
  } else if (g_list_next (albums)) {
    current_album = multiple_album_dialog (albums);
    /* Concentrate here. We remove the album we want from the list, and then
       deep-free the list. */
    albums = g_list_remove (albums, current_album);
    g_list_deep_free (albums, (GFunc)album_details_free);
    albums = NULL;
  } else {
    current_album = albums->data;
    /* current_album now owns ->data, so just free the list */
    g_list_free (albums);
    albums = NULL;
  }
  update_ui_for_album (current_album);
  
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
is_audio_cd (NautilusBurnDrive *drive)
{
  gboolean audio;
  if (drive == NULL) return FALSE;
  nautilus_burn_drive_get_media_type_full (drive, NULL, NULL, NULL, &audio);
  return audio;
}

/**
 * Utility function to reread a CD
 */
static void reread_cd (gboolean ignore_no_media)
{
  /* TODO: remove ignore_no_media? */
  GError *error = NULL;
  GdkCursor *cursor;
  gboolean realized = GTK_WIDGET_REALIZED (main_window);
  
  /* Set watch cursor */
  if (realized) {
    cursor = gdk_cursor_new_for_display (gdk_drawable_get_display (main_window->window),
					 GDK_WATCH);
    gdk_window_set_cursor (main_window->window, cursor);
    gdk_cursor_unref (cursor);
    gdk_display_sync (gdk_drawable_get_display (main_window->window));
  }
  
  /* Set statusbar message */
  gtk_statusbar_push(GTK_STATUSBAR(status_bar), 0, _("Retrieving track listing...please wait."));

  if (!is_audio_cd (drive)) {
    update_ui_for_album (NULL);
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), 0);
    if (realized)
      gdk_window_set_cursor (main_window->window, NULL);
    return;
  }
  
  sj_metadata_list_albums (metadata, &error);

  if (error && !(error->code == SJ_ERROR_CD_NO_MEDIA && ignore_no_media)) {
    GtkWidget *dialog;
    char *text = g_strdup_printf ("<b>%s</b>\n\n%s\n%s: %s",
                                  _("Could not read the CD"),
                                  _("Sound Juicer could not read the track listing on this CD."),
                                  _("Reason"),
                                  error->message);

    dialog = gtk_message_dialog_new (realized ? GTK_WINDOW (main_window) : NULL, 0,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     text);

    g_free (text);

    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_error_free (error);
    update_ui_for_album (NULL);
    return;
  }
}

/**
 * The GConf key for the device changed
 */
void device_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  const char *device;
  gboolean ignore_no_media = GPOINTER_TO_INT (user_data);
  g_assert (strcmp (entry->key, GCONF_DEVICE) == 0);

  if (entry->value == NULL
		  || !cd_drive_exists (gconf_value_get_string (entry->value))) {
    device = prefs_get_default_device();
    if (device == NULL) {
#ifndef IGNORE_MISSING_CD
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "<b>%s</b>\n\n%s",
				       _("No CD-ROM drives found"),
				       _("Sound Juicer could not find any CD-ROM drives to read."));
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      exit (1);
#endif
    }
    drive = nautilus_burn_drive_new_from_path (device);
  } else {
    device = gconf_value_get_string (entry->value);
    if (access (device, R_OK) != 0) {
      GtkWidget *dialog;
      char *message;
      message = g_strdup_printf (_("Sound Juicer could not access the CD-ROM device '%s'"), device);
      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "<b>%s</b>\n\n%s\n%s: %s",
				       _("Could not read the CD"),
				       message,
				       _("Reason"),
				       strerror (errno));
      g_free (message);
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      /* Set a null device */
      drive = NULL;
    } else {
      drive = nautilus_burn_drive_new_from_path (device);
    }
  }
  sj_metadata_set_cdrom (metadata, device);
  sj_extractor_set_device (extractor, device);

  tray_opened = nautilus_burn_drive_door_is_open (drive);
  if (tray_opened == FALSE) {
    reread_cd (ignore_no_media);
  }
}

void profile_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  GMAudioProfile *profile;
  
  g_assert (strcmp (entry->key, GCONF_AUDIO_PROFILE) == 0);
  if (!entry->value) return;
  profile = gm_audio_profile_lookup(gconf_value_get_string (entry->value));
  if (profile != NULL)
    g_object_set(extractor, "profile", profile, NULL);

  if (profile == NULL || !sj_extractor_supports_profile(profile)) {
    GtkWidget *dialog;
    int response;
    
    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
				     GTK_DIALOG_MODAL,
				     GTK_MESSAGE_QUESTION,
				     GTK_BUTTONS_NONE,
				     _("The currently selected audio profile is not available on your installation."));
    gtk_dialog_add_button (GTK_DIALOG (dialog), "gtk-quit", GTK_RESPONSE_REJECT);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Change Profile"), GTK_RESPONSE_ACCEPT);
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response == GTK_RESPONSE_ACCEPT) {
      gtk_widget_destroy (dialog);
      on_edit_preferences_cb (NULL, NULL);
    } else {
      /* Can't use gtk_main_quit here, we may be outside the main loop */
      exit(0);
    }
  }
}

/**
 * Configure the http proxy
 */
void
http_proxy_setup (GConfClient *client)
{
  if (!gconf_client_get_bool (client, GCONF_HTTP_PROXY_ENABLE, NULL)) {
    sj_metadata_set_proxy (metadata, NULL);
  } else {
    const char *host;
    int port;

    host = gconf_client_get_string (client, GCONF_HTTP_PROXY, NULL);
    sj_metadata_set_proxy (metadata, host);
    port = gconf_client_get_int (client, GCONF_HTTP_PROXY_PORT, NULL);
    sj_metadata_set_proxy_port (metadata, port);
  }
}

/**
 * The GConf key for the HTTP proxy being enabled changed.
 */
void http_proxy_enable_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_HTTP_PROXY_ENABLE) == 0);
  if (entry->value == NULL) return;
  http_proxy_setup (client);
}

/**
 * The GConf key for the HTTP proxy changed.
 */
void http_proxy_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_HTTP_PROXY) == 0);
  if (entry->value == NULL) return;
  http_proxy_setup (client);
}

/**
 * The GConf key for the HTTP proxy port changed.
 */
void http_proxy_port_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_HTTP_PROXY_PORT) == 0);
  if (entry->value == NULL) return;
  http_proxy_setup (client);
}

/**
 * Clicked on Reread in the UI (button/menu)
 */
void on_reread_activate (GtkWidget *button, gpointer user_data)
{
  reread_cd (FALSE);
}

/**
 * Called in on_extract_toggled to see if there are selected tracks or not.
 * extracting points to the boolean to set to if there are tracks to extract,
 * and starts as false.
 */
static gboolean extract_available_foreach (GtkTreeModel *model,
                                           GtkTreePath *path,
                                           GtkTreeIter *iter,
                                           gboolean *extracting)
{
  gboolean selected;
  gtk_tree_model_get (GTK_TREE_MODEL (track_store), iter, COLUMN_EXTRACT, &selected, -1);
  if (selected) {
    *extracting = TRUE;
    /* Don't bother walking the list more, we've found a track to be extracted. */
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
 * Called when the user clicked on the Extract column check boxes
 */
static void on_extract_toggled (GtkCellRendererToggle *cellrenderertoggle,
                                gchar *path,
                                gpointer user_data)
{
  gboolean extract;
  GtkTreeIter iter;
  gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (track_store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, COLUMN_EXTRACT, &extract, -1);
  /* extract is the old state here, so toggle */
  extract = !extract;
  gtk_list_store_set (track_store, &iter, COLUMN_EXTRACT, extract, -1);
    
  /* Update the Extract buttons */
  if (extract) {
    /* If true, then we can extract */
    gtk_widget_set_sensitive (extract_button, TRUE);
    gtk_widget_set_sensitive (extract_menuitem, TRUE);
    no_of_tracks_selected++;
  } else {
    /* Reuse the boolean extract */
    extract = FALSE;
    gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), (GtkTreeModelForeachFunc)extract_available_foreach, &extract);
    gtk_widget_set_sensitive (extract_button, extract);
    gtk_widget_set_sensitive (extract_menuitem, extract);   
    no_of_tracks_selected--; 
  }
  /* Enable and disable the Select/Deselect All buttons */
  if (no_of_tracks_selected == total_no_of_tracks) {
    gtk_widget_set_sensitive(deselect_all_menuitem, TRUE);
    gtk_widget_set_sensitive(select_all_menuitem, FALSE);
  } else if (no_of_tracks_selected == 0) {
    gtk_widget_set_sensitive(deselect_all_menuitem, FALSE);
    gtk_widget_set_sensitive(select_all_menuitem, TRUE);
  } else {
    gtk_widget_set_sensitive(select_all_menuitem, TRUE);
    gtk_widget_set_sensitive(deselect_all_menuitem, TRUE);
  }
}

/**
 * Callback when the title or artist cells are edited in the list. column_data
 * contains the column number in the model which was modified.
 */
static void on_cell_edited (GtkCellRendererText *renderer,
                 gchar *path, gchar *string,
                 gpointer column_data)
{
  ViewColumn column = GPOINTER_TO_INT (column_data);
  GtkTreeIter iter;
  TrackDetails *track;
  char *artist, *title;
  gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (track_store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter,
                      COLUMN_ARTIST, &artist,
                      COLUMN_TITLE, &title,
                      COLUMN_DETAILS, &track,
                      -1);
  switch (column) {
  case COLUMN_TITLE:
    g_free (track->title);
    track->title = g_strdup (string);
    gtk_list_store_set (track_store, &iter, COLUMN_TITLE, track->title, -1);
    break;
  case COLUMN_ARTIST:
    g_free (track->artist);
    track->artist = g_strdup (string);
    gtk_list_store_set (track_store, &iter, COLUMN_ARTIST, track->artist, -1);
    break;
  default:
    g_warning (_("Unknown column %d was edited"), column);
  }
  g_free (artist);
  g_free (title);
  return;
}
 
void on_title_edit_changed(GtkEditable *widget, gpointer user_data) {
  g_return_if_fail (current_album != NULL);
  if (current_album->title) {
    g_free (current_album->title);
  }
  current_album->title = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */
}

void on_artist_edit_changed(GtkEditable *widget, gpointer user_data) {
  GtkTreeIter iter;
  TrackDetails *track;

  g_return_if_fail (current_album != NULL);
  if (current_album->artist) {
    g_free (current_album->artist);
  }
  current_album->artist = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (track_store), &iter)) return;
  
  /* Set the artist field in each tree row */
  do {
    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, COLUMN_DETAILS, &track, -1);
    g_free (track->artist);
    track->artist = g_strdup (current_album->artist);
    gtk_list_store_set (track_store, &iter, COLUMN_ARTIST, track->artist, -1);
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(track_store), &iter));
}

void on_genre_edit_changed(GtkEditable *widget, gpointer user_data) {
  g_return_if_fail (current_album != NULL);
  if (current_album->genre) {
    g_free (current_album->genre);
  }
  current_album->genre = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */
}

void on_contents_activate(GtkWidget *button, gpointer user_data) {
  GError *error = NULL;

  gnome_help_display ("sound-juicer", NULL, &error);
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


static const char* genres[] = {
  "Ambient",
  "Blues",
  "Classical",
  "Country",
  "Dance",
  "Electronica",
  "Folk",
  "Jazz",
  "Latin",
  "Pop",
  "Rap",
  "Reggae",
  "Rock",
  "Soul",
  "Spoken Word",
  NULL
};

static GtkTreeModel* create_genre_list(void) {
  GtkListStore *store;
  const char **g = genres;

  store = gtk_list_store_new (1, G_TYPE_STRING);

  while (*g != NULL) {
    GtkTreeIter iter;
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0, *g++, -1);
  }

  return GTK_TREE_MODEL (store);
}

GtkWidget *
sj_make_volume_button (void)
{
  GtkWidget *w = sj_volume_button_new (0.0, 1.0, 0.02);
  sj_volume_button_set_value (SJ_VOLUME_BUTTON (w), 1.0);
  return w;
}

static void
upgrade_gconf (void)
{
  char *s;
  s = gconf_client_get_string (gconf_client, GCONF_BASEURI, NULL);
  if (s != NULL) {
    g_free (s);
  } else {
    char *uri;
    s = gconf_client_get_string (gconf_client, GCONF_BASEPATH, NULL);
    if (s == NULL)
      return;
    uri = gnome_vfs_get_uri_from_local_path (s);
    g_free (s);
    gconf_client_set_string (gconf_client, GCONF_BASEURI, uri, NULL);
    g_free (uri);
  }
}

static void
on_message_received (const char *message, gpointer user_data)
{
  if (message == NULL)
    return;
  if (strcmp (RAISE_WINDOW, message) == 0) {
    gtk_window_present (GTK_WINDOW (main_window));
  }
}

int main (int argc, char **argv)
{
  GError *error = NULL;
  struct poptOption options[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, NULL, 0, "GStreamer", NULL },
    { "auto-start", 'a', POPT_ARG_NONE, &autostart, 0, "Start extracting immediately", NULL},
    { "play", 'p', POPT_ARG_NONE, &autoplay, 0, "Start playing immediately", NULL},
    POPT_TABLEEND
  };

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  
  options[0].arg = (void *) gst_init_get_popt_table ();
  gnome_program_init ("sound-juicer",
                      VERSION, LIBGNOMEUI_MODULE,
                      argc, argv,
                      GNOME_PROGRAM_STANDARD_PROPERTIES,
                      GNOME_PARAM_POPT_TABLE, options,
                      NULL);
  g_set_application_name (_("Sound Juicer"));

  sj_stock_init ();

  connection = bacon_message_connection_new ("sound-juicer");
  if (bacon_message_connection_get_is_server (connection) == FALSE) {
	  bacon_message_connection_send (connection, RAISE_WINDOW);
	  bacon_message_connection_free (connection);
	  exit (0);
  } else {
    bacon_message_connection_set_callback (connection, on_message_received, NULL);
  }

  metadata = SJ_METADATA (sj_metadata_musicbrainz_new ());
  error = sj_metadata_get_new_error (metadata);
  if (error) {
    error_on_start (error);
    exit (1);
  }
  g_signal_connect (metadata, "metadata", G_CALLBACK (metadata_cb), NULL);

  gconf_client = gconf_client_get_default ();
  if (gconf_client == NULL) {
    g_warning (_("Could not create GConf client.\n"));
    exit (1);
  }

  upgrade_gconf ();

  gconf_client_add_dir (gconf_client, GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_EJECT, eject_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_BASEURI, baseuri_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_STRIP, strip_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_AUDIO_PROFILE, profile_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_PARANOIA, paranoia_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
  gconf_client_add_dir (gconf_client, GCONF_PROXY_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  gconf_client_notify_add (gconf_client, GCONF_HTTP_PROXY_ENABLE, http_proxy_enable_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_HTTP_PROXY, http_proxy_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_HTTP_PROXY_PORT, http_proxy_port_changed_cb, NULL, NULL, NULL);

  /* init gnome-media-profiles */
  gnome_media_profiles_init (gconf_client);

  glade_init ();
  if (g_file_test (SOURCE_GLADE, G_FILE_TEST_EXISTS) != FALSE) {
    glade = glade_xml_new (SOURCE_GLADE, NULL, NULL);
  } else {
    glade = glade_xml_new (INSTALLED_GLADE, NULL, NULL);
  }
  if (glade == NULL) {
    g_error_new (0, 0, _("The interface file for Sound Juicer could not be read."));
    error_on_start (error);
    g_error_free (error);
    exit (1);
  }
  glade_xml_signal_autoconnect (glade);

  main_window = glade_xml_get_widget (glade, "main_window");
  gtk_window_set_icon_from_file (GTK_WINDOW (main_window), PIXMAPDIR"/sound-juicer.png", NULL);

  select_all_menuitem = glade_xml_get_widget (glade, "select_all");
  deselect_all_menuitem = glade_xml_get_widget (glade, "deselect_all");
  title_entry = glade_xml_get_widget (glade, "title_entry");
  artist_entry = glade_xml_get_widget (glade, "artist_entry");
  duration_label = glade_xml_get_widget (glade, "duration_label");
  genre_entry = glade_xml_get_widget (glade, "genre_entry");
  track_listview = glade_xml_get_widget (glade, "track_listview");
  extract_button = glade_xml_get_widget (glade, "extract_button");
  extract_menuitem = glade_xml_get_widget (glade, "extract_menuitem");
  play_button = glade_xml_get_widget (glade, "play_button");
  play_menuitem = glade_xml_get_widget (glade, "play_menuitem");
  status_bar = glade_xml_get_widget (glade, "status_bar");

  {
    GtkEntryCompletion *completion;
    completion = gtk_entry_completion_new ();
    gtk_entry_completion_set_model (completion, create_genre_list ());
    gtk_entry_completion_set_text_column (completion, 0);
    gtk_entry_completion_set_inline_completion (completion, TRUE);
    gtk_entry_set_completion (GTK_ENTRY (genre_entry), completion);
  }

  track_store = gtk_list_store_new (COLUMN_TOTAL, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_POINTER);
  gtk_tree_view_set_model (GTK_TREE_VIEW (track_listview), GTK_TREE_MODEL (track_store));
  {
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    
    toggle_renderer = gtk_cell_renderer_toggle_new ();
    g_signal_connect (toggle_renderer, "toggled", G_CALLBACK (on_extract_toggled), NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Extract"),
                                                       toggle_renderer,
                                                       "active", COLUMN_EXTRACT,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
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

    title_renderer = gtk_cell_renderer_text_new ();
    g_signal_connect (title_renderer, "edited", G_CALLBACK (on_cell_edited), GUINT_TO_POINTER (COLUMN_TITLE));
    g_object_set (G_OBJECT (title_renderer), "editable", TRUE, NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                       title_renderer,
                                                       "text", COLUMN_TITLE,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    artist_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Artist"),
                                                       artist_renderer,
                                                       "text", COLUMN_ARTIST,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_expand (column, TRUE);
    g_signal_connect (artist_renderer, "edited", G_CALLBACK (on_cell_edited), GUINT_TO_POINTER (COLUMN_ARTIST));
    g_object_set (G_OBJECT (artist_renderer), "editable", TRUE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
    
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Duration"),
                                                       renderer,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, renderer, duration_cell_data_cb, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
  }

  update_ui_for_album (NULL);
  gtk_widget_show (main_window);

  extractor = SJ_EXTRACTOR (sj_extractor_new());
  error = sj_extractor_get_new_error (extractor);
  if (error) {
    error_on_start (error);
    exit (1);
  }

  http_proxy_setup (gconf_client);
  /* TODO: port port basepath to baseuri */
  baseuri_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEURI, NULL, TRUE, NULL), NULL);
  path_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PATH_PATTERN, NULL, TRUE, NULL), NULL);
  file_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FILE_PATTERN, NULL, TRUE, NULL), NULL);
  profile_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_AUDIO_PROFILE, NULL, TRUE, NULL), NULL);
  paranoia_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PARANOIA, NULL, TRUE, NULL), NULL);
  strip_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_STRIP, NULL, TRUE, NULL), NULL);
  eject_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_EJECT, NULL, TRUE, NULL), NULL);
  device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), GINT_TO_POINTER (TRUE));

  if (sj_extractor_supports_encoding (&error) == FALSE) {
    error_on_start (error);
    return 0;
  }

  /* Poke the CD drive every now and then */
  tray_opened = nautilus_burn_drive_door_is_open (drive);
  poll_id = g_timeout_add (2000, poll_tray_opened, NULL);

  gtk_main ();
  g_object_unref (metadata);
  g_object_unref (extractor);
  g_object_unref (gconf_client);
  return 0;
}
