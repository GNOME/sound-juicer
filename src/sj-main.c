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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <brasero-medium-selection.h>
#include <brasero-volume.h>
#include <profiles/gnome-media-profiles.h>
#include <gst/gst.h>

#include "bacon-message-connection.h"
#include "gconf-bridge.h"
#include "sj-about.h"
#include "sj-metadata-getter.h"
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"
#include "sj-main.h"
#include "sj-prefs.h"
#include "sj-play.h"
#include "sj-genres.h"
#include "gedit-message-area.h"

gboolean on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data);

static void reread_cd (gboolean ignore_no_media);
static void update_ui_for_album (AlbumDetails *album);
static void set_duplication (gboolean enable);

/* Prototypes for the signal blocking/unblocking in update_ui_for_album */
void on_title_edit_changed(GtkEditable *widget, gpointer user_data);
void on_artist_edit_changed(GtkEditable *widget, gpointer user_data);
void on_year_edit_changed(GtkEditable *widget, gpointer user_data);
void on_disc_number_edit_changed(GtkEditable *widget, gpointer user_data);

GladeXML *glade;

SjMetadataGetter *metadata;
SjExtractor *extractor;

GConfClient *gconf_client;

GtkWidget *main_window;
static GtkWidget *message_area_eventbox;
static GtkWidget *title_entry, *artist_entry, *duration_label, *genre_entry, *year_entry, *disc_number_entry;
static GtkWidget *track_listview, *extract_button, *play_button;
static GtkWidget *status_bar;
static GtkWidget *extract_menuitem, *play_menuitem, *next_menuitem, *prev_menuitem, *select_all_menuitem, *deselect_all_menuitem;
static GtkWidget *submit_menuitem;
static GtkWidget *duplicate, *eject;
GtkListStore *track_store;
static BaconMessageConnection *connection;
GtkCellRenderer *toggle_renderer, *title_renderer, *artist_renderer;

GtkWidget *current_message_area;

const char *path_pattern, *file_pattern;
GFile *base_uri;
BraseroDrive *drive = NULL;
gboolean strip_chars;
gboolean eject_finished;
gboolean open_finished;
gboolean extracting = FALSE;
static gboolean duplication_enabled;

static gint total_no_of_tracks;
static gint no_of_tracks_selected;
static AlbumDetails *current_album;
static char *current_submit_url = NULL;

gboolean autostart = FALSE, autoplay = FALSE;

static guint debug_flags = 0;

#define DEFAULT_PARANOIA 4
#define RAISE_WINDOW "raise-window"
#define SOURCE_GLADE "../data/sound-juicer.glade"
#define INSTALLED_GLADE DATADIR"/sound-juicer/sound-juicer.glade"

void
sj_stock_init (void)
{
  static gboolean initialized = FALSE;
  static GtkIconFactory *sj_icon_factory = NULL;

  static const GtkStockItem sj_stock_items[] =
  {
    { SJ_STOCK_EXTRACT, N_("E_xtract"), GDK_CONTROL_MASK, GDK_Return, NULL }
  };

  if (initialized)
    return;

  sj_icon_factory = gtk_icon_factory_new ();

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

void sj_debug (SjDebugDomain domain, const gchar* format, ...)
{
  va_list args;
  gchar *string;

  if (debug_flags & domain) {
    va_start (args, format);
    string = g_strdup_vprintf (format, args);
    va_end (args);
    g_printerr (string);
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

  brasero_drive_eject (drive, FALSE, NULL);
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
    g_object_set (G_OBJECT (cell), "stock-id", GTK_STOCK_MEDIA_PLAY, NULL);
    break;
  case STATE_PAUSED:
    g_object_set (G_OBJECT (cell), "stock-id", GTK_STOCK_MEDIA_PAUSE, NULL);
    break;
  case STATE_EXTRACTING:
    g_object_set (G_OBJECT (cell), "stock-id", GTK_STOCK_MEDIA_RECORD, NULL);
    break;
  default:
    g_warning("Unhandled track state %d\n", state);
  }
}

/* Taken from gedit */
static void
set_message_area_text_and_icon (GeditMessageArea *message_area,
                                const gchar   *icon_stock_id,
                                const gchar   *primary_text,
                                const gchar   *secondary_text,
                                GtkWidget *button)
{
  GtkWidget *hbox_content;
  GtkWidget *image;
  GtkWidget *vbox;
  gchar *primary_markup;
  gchar *secondary_markup;
  GtkWidget *primary_label;
  GtkWidget *secondary_label;
  AtkObject *ally_target;

  ally_target = gtk_widget_get_accessible (button);

  hbox_content = gtk_hbox_new (FALSE, 8);
  gtk_widget_show (hbox_content);

  image = gtk_image_new_from_stock (icon_stock_id, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (hbox_content), image, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (hbox_content), vbox, TRUE, TRUE, 0);

  primary_markup = g_markup_printf_escaped ("<b>%s</b>", primary_text);
  primary_label = gtk_label_new (primary_markup);
  g_free (primary_markup);
  gtk_widget_show (primary_label);
  gtk_box_pack_start (GTK_BOX (vbox), primary_label, TRUE, TRUE, 0);
  gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (primary_label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (primary_label), 0, 0.5);
  atk_object_add_relationship (ally_target,
                               ATK_RELATION_LABELLED_BY,
                               gtk_widget_get_accessible (primary_label));

  if (secondary_text != NULL) {
    secondary_markup = g_markup_printf_escaped ("<small>%s</small>",
                                                secondary_text);
    secondary_label = gtk_label_new (secondary_markup);
    g_free (secondary_markup);
    gtk_widget_show (secondary_label);
    gtk_box_pack_start (GTK_BOX (vbox), secondary_label, TRUE, TRUE, 0);
    gtk_label_set_use_markup (GTK_LABEL (secondary_label), TRUE);
    gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (secondary_label), 0, 0.5);
    atk_object_add_relationship (ally_target,
                                 ATK_RELATION_LABELLED_BY,
                                 gtk_widget_get_accessible (secondary_label));
  }

  gedit_message_area_set_contents (GEDIT_MESSAGE_AREA (message_area),
                                   hbox_content);
}

/* Taken from gedit */
static void
set_message_area (GtkWidget *container,
                  GtkWidget *message_area)
{
  if (current_message_area == message_area)
    return;
  
  if (current_message_area != NULL)
    gtk_widget_destroy (current_message_area);
  
  current_message_area = message_area;
  
  if (message_area == NULL)
    return;

  gtk_container_add (GTK_CONTAINER (container), message_area);

  g_object_add_weak_pointer (G_OBJECT (current_message_area),
                             (gpointer)&current_message_area);
}

static GtkWidget*
musicbrainz_submit_message_area_new (char *title, char *artist)
{
  GtkWidget *message_area, *button;
  char *primary_text;

  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (artist != NULL, NULL);

  message_area = gedit_message_area_new ();

  button = gedit_message_area_add_button (GEDIT_MESSAGE_AREA (message_area),
                                          _("S_ubmit Album"),
                                          GTK_RESPONSE_OK);
  gedit_message_area_add_button (GEDIT_MESSAGE_AREA (message_area),
                                 GTK_STOCK_CANCEL,
                                 GTK_RESPONSE_CANCEL);

  /* Translators: title, artist */
  primary_text = g_strdup_printf (_("Could not find %s by %s on MusicBrainz."), title, artist);
  
  set_message_area_text_and_icon (GEDIT_MESSAGE_AREA (message_area),
                                  "gtk-dialog-info",
                                  primary_text,
                                  _("You can improve the MusicBrainz database by adding this album."),
                                  button);
  
  g_free (primary_text);
  
  return message_area;
}

static void
musicbrainz_submit_message_area_response (GeditMessageArea *message_area,
                                          int response_id,
                                          gpointer user_data)
{
  if (response_id == GTK_RESPONSE_OK) {
    on_submit_activate (NULL, NULL);
  }
  
  set_message_area (message_area_eventbox, NULL);
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

  /* Really really make sure we don't have a playing title */
  sj_main_set_title (NULL);

  if (album == NULL) {
    gtk_list_store_clear (track_store);
    gtk_entry_set_text (GTK_ENTRY (title_entry), "");
    gtk_entry_set_text (GTK_ENTRY (artist_entry), "");
    gtk_entry_set_text (GTK_ENTRY (genre_entry), "");
    gtk_entry_set_text (GTK_ENTRY (year_entry), "");
    gtk_entry_set_text (GTK_ENTRY (disc_number_entry), "");
    gtk_label_set_text (GTK_LABEL (duration_label), "");
    gtk_widget_set_sensitive (title_entry, FALSE);
    gtk_widget_set_sensitive (artist_entry, FALSE);
    gtk_widget_set_sensitive (genre_entry, FALSE);
    gtk_widget_set_sensitive (year_entry, FALSE);
    gtk_widget_set_sensitive (disc_number_entry, FALSE);
    gtk_widget_set_sensitive (play_button, FALSE);
    gtk_widget_set_sensitive (play_menuitem, FALSE);
    gtk_widget_set_sensitive (extract_button, FALSE);
    gtk_widget_set_sensitive (extract_menuitem, FALSE);
    gtk_widget_set_sensitive (select_all_menuitem, FALSE);
    gtk_widget_set_sensitive (deselect_all_menuitem, FALSE);
    gtk_widget_set_sensitive (prev_menuitem, FALSE);
    gtk_widget_set_sensitive (next_menuitem, FALSE);
    set_duplication (FALSE);

    set_message_area (message_area_eventbox, NULL);
  } else {
    gtk_list_store_clear (track_store);

    g_signal_handlers_block_by_func (title_entry, on_title_edit_changed, NULL);
    g_signal_handlers_block_by_func (artist_entry, on_artist_edit_changed, NULL);
    g_signal_handlers_block_by_func (year_entry, on_year_edit_changed, NULL);
    g_signal_handlers_block_by_func (disc_number_entry, on_disc_number_edit_changed, NULL);
    gtk_entry_set_text (GTK_ENTRY (title_entry), album->title);
    gtk_entry_set_text (GTK_ENTRY (artist_entry), album->artist);
    if (album->disc_number) {
      gtk_entry_set_text (GTK_ENTRY (disc_number_entry), g_strdup_printf ("%d", album->disc_number));
    }
    if (album->release_date && g_date_valid (album->release_date)) {
      gtk_entry_set_text (GTK_ENTRY (year_entry), g_strdup_printf ("%d", g_date_get_year (album->release_date)));
    }
    g_signal_handlers_unblock_by_func (title_entry, on_title_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (artist_entry, on_artist_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (year_entry, on_year_edit_changed, NULL);
    g_signal_handlers_unblock_by_func (disc_number_entry, on_disc_number_edit_changed, NULL);
    /* Clear the genre field, it's from the user */
    gtk_entry_set_text (GTK_ENTRY (genre_entry), "");

    gtk_widget_set_sensitive (title_entry, TRUE);
    gtk_widget_set_sensitive (artist_entry, TRUE);
    gtk_widget_set_sensitive (genre_entry, TRUE);
    gtk_widget_set_sensitive (year_entry, TRUE);
    gtk_widget_set_sensitive (disc_number_entry, TRUE);
    gtk_widget_set_sensitive (play_button, TRUE);
    gtk_widget_set_sensitive (play_menuitem, TRUE);
    gtk_widget_set_sensitive (extract_button, TRUE);
    gtk_widget_set_sensitive (extract_menuitem, TRUE);
    gtk_widget_set_sensitive (select_all_menuitem, FALSE);
    gtk_widget_set_sensitive (deselect_all_menuitem, TRUE);
    gtk_widget_set_sensitive (prev_menuitem, FALSE);
    gtk_widget_set_sensitive (next_menuitem, FALSE);
    set_duplication (TRUE);
    
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

    /* If album details don't come from MusicBrainz ask user to add them */
    if (album->metadata_source != SOURCE_MUSICBRAINZ) {
      GtkWidget *message_area;

      message_area = musicbrainz_submit_message_area_new (album->title, album->artist);

      set_message_area (message_area_eventbox, message_area);

      g_signal_connect (message_area,
                        "response",
                        G_CALLBACK (musicbrainz_submit_message_area_response),
                        NULL);
      
      gedit_message_area_set_default_response (GEDIT_MESSAGE_AREA (message_area),
                                               GTK_RESPONSE_CANCEL);
      
      gtk_widget_show (message_area);
    }
  }
}

/**
 * Callback that gets fired when a user double clicks on a row
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
 * Callback that gets fired when an the selection changes. We use this to
 * change the sensitivity of the continue button
 */
static void selected_album_changed (GtkTreeSelection *selection, 
                                    gpointer *user_data)
{
  GtkWidget *ok_button = GTK_WIDGET (user_data);

  if (gtk_tree_selection_get_selected (selection, NULL, NULL)) {
    gtk_widget_set_sensitive (ok_button, TRUE);
  } else {
    gtk_widget_set_sensitive (ok_button, FALSE);
  }
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
  GtkWidget *ok_button = NULL;

  if (dialog == NULL) {
    GtkTreeViewColumn *column;
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new ();

    dialog = glade_xml_get_widget (glade, "multiple_dialog");
    g_assert (dialog != NULL);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
    albums_listview = glade_xml_get_widget (glade, "albums_listview");
    ok_button = glade_xml_get_widget (glade, "ok_button");

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
    gtk_widget_set_sensitive (ok_button, FALSE);
    g_signal_connect (selection, "changed", (GCallback)selected_album_changed, ok_button);
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

  /* Select the first album */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (albums_store), &iter))
  {
    gtk_tree_selection_select_iter (selection, &iter);
  }

  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_DELETE_EVENT) {
    return NULL;
  }

  if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
    gtk_tree_model_get (GTK_TREE_MODEL (albums_store), &iter, 2, &album, -1);
    return album;
  } else {
    return NULL;
  }
}

/**
 * The GConf key for the base path changed
 */
static void baseuri_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_BASEURI) == 0);
  if (base_uri) {
    g_object_unref (base_uri);
  }
  if (entry->value == NULL) {
    base_uri = sj_get_default_music_directory ();
  } else {
    base_uri = g_file_new_for_uri (gconf_value_get_string (entry->value));
  }
  /* TODO: sanity check the URI somewhat */
}

/**
 * The GConf key for the directory pattern changed
 */
static void path_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
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
static void file_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
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
static void paranoia_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_PARANOIA) == 0);
  if (entry->value == NULL) {
    sj_extractor_set_paranoia (extractor, DEFAULT_PARANOIA);
  } else {
    int value = gconf_value_get_int (entry->value);
    if (value == 0 || value == 2 || value == 4 || value == 8 || value == 16 || value == 255) {
      sj_extractor_set_paranoia (extractor, value);
    }
  }
}

/**
 * The GConf key for the strip characters option changed
 */
static void strip_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
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
static void eject_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_EJECT) == 0);
  if (entry->value == NULL) {
    eject_finished = FALSE;
  } else {
    eject_finished = gconf_value_get_bool (entry->value);
  }
}

/**
 * The GConf key for the eject when finished option changed
 */
static void open_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_OPEN) == 0);
  if (entry->value == NULL) {
    open_finished = FALSE;
  } else {
    open_finished = gconf_value_get_bool (entry->value);
  }
}

/**
 * The GConf key for audio volume changes
 */
static void audio_volume_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_AUDIO_VOLUME) == 0);
  
  GtkWidget *volb = glade_xml_get_widget (glade, "volume_button");
  if (entry->value == NULL) {
    gtk_scale_button_set_value (GTK_SCALE_BUTTON (volb), 1.0);
  } else {
    gtk_scale_button_set_value (GTK_SCALE_BUTTON (volb), gconf_value_get_float (entry->value));
  }
}

static void
metadata_cb (SjMetadataGetter *m, GList *albums, GError *error)
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

  current_submit_url = sj_metadata_getter_get_submit_url (metadata);
  if (current_submit_url) {
    gtk_widget_set_sensitive (submit_menuitem, TRUE);
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
static void reread_cd (gboolean ignore_no_media)
{
  /* TODO: remove ignore_no_media? */
  GError *error = NULL;
  GdkCursor *cursor;
  gboolean realized = GTK_WIDGET_REALIZED (main_window);
  
  /* Make sure nothing is playing */
  stop_playback ();
  
  /* Set watch cursor */
  if (realized) {
    cursor = gdk_cursor_new_for_display (gdk_drawable_get_display (main_window->window), GDK_WATCH);
    gdk_window_set_cursor (main_window->window, cursor);
    gdk_cursor_unref (cursor);
    gdk_display_sync (gdk_drawable_get_display (main_window->window));
  }
  
  /* Set statusbar message */
  gtk_statusbar_push(GTK_STATUSBAR(status_bar), 0, _("Retrieving track listing...please wait."));

  if (!drive)
    sj_debug (DEBUG_CD, "Attempting to re-read NULL drive\n");

  g_free (current_submit_url);
  current_submit_url = NULL;
  gtk_widget_set_sensitive (submit_menuitem, FALSE);

  if (!is_audio_cd (drive)) {
    sj_debug (DEBUG_CD, "Media is not an audio CD\n");
    update_ui_for_album (NULL);
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), 0);
    if (realized)
      gdk_window_set_cursor (main_window->window, NULL);
    return;
  }

  sj_metadata_getter_list_albums (metadata, &error);

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

static void
media_added_cb (BraseroMediumMonitor	*drive,
		BraseroMedium		*medium,
                gpointer           	 data)
{
  if (extracting == TRUE) {
    /* FIXME: recover? */
  }

  sj_debug (DEBUG_CD, "Media added to device %s\n", brasero_drive_get_device (brasero_medium_get_drive (medium)));
  reread_cd (TRUE);
}

static void
media_removed_cb (BraseroMediumMonitor	*drive,
		  BraseroMedium		*medium,
                  gpointer           data)
{
  if (extracting == TRUE) {
    /* FIXME: recover? */
  }

  /* first make sure we're not playing */
  stop_playback ();

  sj_debug (DEBUG_CD, "Media removed from device %s\n", brasero_drive_get_device (brasero_medium_get_drive (medium)));
  stop_ui_hack ();
  update_ui_for_album (NULL);
}

static void
set_drive_from_device (const char *device)
{
  BraseroMediumMonitor *monitor;

  if (drive) {
    g_object_unref (drive);
    drive = NULL;
  }

  if (! device)
    return;

  monitor = brasero_medium_monitor_get_default ();
  drive = brasero_medium_monitor_get_drive (monitor, device);
  if (! drive) {
    GtkWidget *dialog;
    char *message;
    message = g_strdup_printf (_("Sound Juicer could not use the CD-ROM device '%s'"), device);
    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "<b>%s</b>\n\n%s",
                                     message,
                                     _("HAL daemon may not be running."));
    g_free (message);
    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return;
  }

  g_signal_connect (monitor, "medium-added", G_CALLBACK (media_added_cb), NULL);
  g_signal_connect (monitor, "medium-removed", G_CALLBACK (media_removed_cb), NULL);
}

static void
set_device (const char* device, gboolean ignore_no_media)
{
  gboolean tray_opened;

  if (device == NULL) {
    set_drive_from_device (device);
  } else if (access (device, R_OK) != 0) {
    GtkWidget *dialog;
    char *message;
    const char *error;
    
    error = g_strerror (errno);
    message = g_strdup_printf (_("Sound Juicer could not access the CD-ROM device '%s'"), device);
    
    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "<b>%s</b>\n\n%s\n%s: %s",
                                     _("Could not read the CD"),
                                     message,
                                     _("Reason"),
                                     error);
    g_free (message);

    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    /* Set a null device */
    set_drive_from_device (NULL);
  } else {
    set_drive_from_device (device);
  }

  sj_metadata_getter_set_cdrom (metadata, device);
  sj_extractor_set_device (extractor, device);
  
  if (drive != NULL) {
    tray_opened = brasero_drive_is_door_open (drive);
    if (tray_opened == FALSE) {
      reread_cd (ignore_no_media);
    }

    /* Enable/disable the eject options based on wether the drive supports ejection */
    gtk_widget_set_sensitive (eject, brasero_drive_can_eject (drive));
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

    g_slist_foreach (drives, (GFunc) g_object_unref, NULL);
    g_slist_free (drives);
  }
  return default_device;
}

/**
 * The GConf key for the device changed
 */
static void device_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  const char *device;
  gboolean ignore_no_media = GPOINTER_TO_INT (user_data);
  g_assert (strcmp (entry->key, GCONF_DEVICE) == 0);

  if (entry->value == NULL || !cd_drive_exists (gconf_value_get_string (entry->value))) {
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
  } else {
    device = gconf_value_get_string (entry->value);
  }
  set_device (device, ignore_no_media);
}

static void profile_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  GMAudioProfile *profile;
  
  g_assert (strcmp (entry->key, GCONF_AUDIO_PROFILE) == 0);
  if (!entry->value) return;
  profile = gm_audio_profile_lookup (gconf_value_get_string (entry->value));
  if (profile != NULL)
    g_object_set (extractor, "profile", profile, NULL);

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
static void
http_proxy_setup (GConfClient *client)
{
  if (!gconf_client_get_bool (client, GCONF_HTTP_PROXY_ENABLE, NULL)) {
    sj_metadata_getter_set_proxy (metadata, NULL);
  } else {
    char *host;
    int port;

    host = gconf_client_get_string (client, GCONF_HTTP_PROXY, NULL);
    sj_metadata_getter_set_proxy (metadata, host);
    g_free (host);
    port = gconf_client_get_int (client, GCONF_HTTP_PROXY_PORT, NULL);
    sj_metadata_getter_set_proxy_port (metadata, port);
  }
}

/**
 * The GConf key for the HTTP proxy being enabled changed.
 */
static void http_proxy_enable_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_HTTP_PROXY_ENABLE) == 0);
  if (entry->value == NULL) return;
  http_proxy_setup (client);
}

/**
 * The GConf key for the HTTP proxy changed.
 */
static void http_proxy_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_HTTP_PROXY) == 0);
  if (entry->value == NULL) return;
  http_proxy_setup (client);
}

/**
 * The GConf key for the HTTP proxy port changed.
 */
static void http_proxy_port_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
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
 * Clicked the Submit menu item in the UI
 */
void on_submit_activate (GtkWidget *menuitem, gpointer user_data)
{
  GError *error = NULL;

  if (current_submit_url) {
      if (!gtk_show_uri (NULL, current_submit_url, GDK_CURRENT_TIME, &error)) {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "<b>%s</b>\n\n%s\n%s: %s",
                                       _("Could not open URL"),
                                       _("Sound Juicer could not open the submission URL"),
                                       _("Reason"),
                                       error->message);
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
  }

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

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (track_store), &iter, path))
      return;
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

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (track_store), &iter, path))
    return;
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
  gchar *current_track_artist, *former_album_artist = NULL;

  g_return_if_fail (current_album != NULL);
  
  /* Unset the sortable artist field, as we can't change it automatically */
  if (current_album->artist_sortname) {
    g_free (current_album->artist_sortname);
    current_album->artist_sortname = NULL;
  }

  if (current_album->artist) {
    former_album_artist = current_album->artist;
  }
  current_album->artist = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (track_store), &iter)) {
    g_free (former_album_artist);
    return;
  }
  
  /* Set the artist field in each tree row */
  do {
    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, COLUMN_ARTIST, &current_track_artist, -1);
    /* Change track artist if it matched album artist before the change */
    if ((strcasecmp (current_track_artist, former_album_artist) == 0) || (strcasecmp (current_track_artist, current_album->artist) == 0)) {
      gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, COLUMN_DETAILS, &track, -1);
      
      g_free (track->artist);
      track->artist = g_strdup (current_album->artist);
      
      if (track->artist_sortname) {
        g_free (track->artist_sortname);
        track->artist_sortname = NULL;
      }
      
      gtk_list_store_set (track_store, &iter, COLUMN_ARTIST, track->artist, -1);
    }
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (track_store), &iter));
 
  g_free (former_album_artist);
}

void on_genre_edit_changed(GtkEditable *widget, gpointer user_data) {
  g_return_if_fail (current_album != NULL);
  if (current_album->genre) {
    g_free (current_album->genre);
  }
  current_album->genre = gtk_editable_get_chars (widget, 0, -1); /* get all the characters */
}

void on_year_edit_changed(GtkEditable *widget, gpointer user_data) {
  const gchar* yearstr;
  int year;
  
  g_return_if_fail (current_album != NULL);
  
  yearstr = gtk_entry_get_text (GTK_ENTRY (widget));
  year = atoi (yearstr);
  if (year > 0) {
    if (current_album->release_date) {
      g_date_set_dmy (current_album->release_date, 1, 1, year);
    } else {
      current_album->release_date = g_date_new_dmy (1, 1, year);
    }
  }
}

void on_disc_number_edit_changed(GtkEditable *widget, gpointer user_data) {
    const gchar* discstr;  
    int disc_number;

    g_return_if_fail (current_album != NULL);
    discstr = gtk_entry_get_text (GTK_ENTRY (widget));
    disc_number = atoi(discstr);
    current_album->disc_number = disc_number;
}

void on_contents_activate(GtkWidget *button, gpointer user_data) {
  GError *error = NULL;

  gtk_show_uri (NULL, "ghelp:sound-juicer", GDK_CURRENT_TIME, &error);
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

static void
upgrade_gconf (void)
{
  char *s;
  s = gconf_client_get_string (gconf_client, GCONF_BASEURI, NULL);
  if (s != NULL) {
    g_free (s);
  } else {
    GFile *gfile;
    char *uri;
    s = gconf_client_get_string (gconf_client, GCONF_BASEPATH, NULL);
    if (s == NULL)
      return;
    gfile = g_file_new_for_path (s);
    uri = g_file_get_uri (gfile);
    g_free (s);
    g_object_unref (gfile);
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

/**
 * Performs various checks to ensure CD duplication is available.
 * If this is found TRUE is returned, otherwise FALSE is returned.
 */
static gboolean
is_cd_duplication_available()
{
  /* First check the brasero tool is available in the path */
  gchar* brasero_cd_burner = g_find_program_in_path ("brasero");
  if (brasero_cd_burner == NULL) {
    return FALSE;
  } 
  g_free(brasero_cd_burner);

  /* Second check the cdrdao tool is available in the path */
  gchar* cdrdao = g_find_program_in_path ("cdrdao");
  if (cdrdao == NULL) {
    return FALSE;
  } 
  g_free(cdrdao);  

  /* Now check that there is at least one cd recorder available */
  BraseroMediumMonitor     *monitor;
  GSList		   *drives;
  GSList		   *iter;

  monitor = brasero_medium_monitor_get_default ();
  drives = brasero_medium_monitor_get_drives (monitor, BRASERO_DRIVE_TYPE_ALL);

  for (iter = drives; iter; iter = iter->next) {
    BraseroDrive *drive;

    drive = iter->data;
    if (brasero_drive_can_write (drive)) {
      g_slist_foreach (drives, (GFunc) g_object_unref, NULL);
      g_slist_free (drives);
      return TRUE;
    }
  }

  g_slist_foreach (drives, (GFunc) g_object_unref, NULL);
  g_slist_free (drives);
  return FALSE;
}

/**
 * Clicked on duplicate in the UI (button/menu)
 */
void on_duplicate_activate (GtkWidget *button, gpointer user_data)
{
  GError *error = NULL;
  const gchar* device;

  device = brasero_drive_get_device (drive);
  if (!g_spawn_command_line_async (g_strconcat ("brasero -c ", device, NULL), &error)) {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "<b>%s</b>\n\n%s\n%s: %s",
                                       _("Could not duplicate disc"),
                                       _("Sound Juicer could not duplicate the disc"),
                                       _("Reason"),
                                       error->message);
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
  }
}

/**
 * Sets the duplication buttons sensitive property if duplication is enabled.
 * This is setup in the main entry point.
 */
static void set_duplication(gboolean enabled)
{
  if (duplication_enabled) {
    gtk_widget_set_sensitive (duplicate, enabled);
  }
}

int main (int argc, char **argv)
{
  GError *error = NULL;
  GtkTreeSelection *selection;
  char *device = NULL, **uris = NULL;
  GOptionContext *ctx;
  const GOptionEntry entries[] = {
    { "auto-start", 'a', 0, G_OPTION_ARG_NONE, &autostart, N_("Start extracting immediately"), NULL },
    { "play", 'p', 0, G_OPTION_ARG_NONE, &autoplay, N_("Start playing immediately"), NULL},
    { "device", 'd', 0, G_OPTION_ARG_FILENAME, &device, N_("What CD device to read"), N_("DEVICE") },
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &uris, N_("URI to the CD device to read"), NULL },
    { NULL }
  };

  if (!g_thread_supported ()) g_thread_init (NULL);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  ctx = g_option_context_new (N_("- Extract music from your CDs"));
  g_option_context_add_main_entries (ctx, entries, GETTEXT_PACKAGE);
  g_option_context_set_translation_domain(ctx, GETTEXT_PACKAGE);
  g_option_context_add_group (ctx, gtk_get_option_group (TRUE));
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  g_option_context_add_group (ctx, brasero_media_get_option_group ());
  g_option_context_set_ignore_unknown_options (ctx, TRUE);

  g_option_context_parse (ctx, &argc, &argv, &error);
  if (error != NULL) {
      g_printerr ("Error parsing options: %s", error->message);
      exit (1);
  }

  g_set_application_name (_("Sound Juicer"));

  sj_debug_init ();

  sj_stock_init ();

  gtk_window_set_default_icon_name ("sound-juicer");
  
  connection = bacon_message_connection_new ("sound-juicer");
  if (bacon_message_connection_get_is_server (connection) == FALSE) {
    bacon_message_connection_send (connection, RAISE_WINDOW);
    bacon_message_connection_free (connection);
    exit (0);
  } else {
    bacon_message_connection_set_callback (connection, on_message_received, NULL);
  }

  brasero_media_library_start ();

  metadata = sj_metadata_getter_new ();
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
  gconf_client_notify_add (gconf_client, GCONF_OPEN, open_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_BASEURI, baseuri_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_STRIP, strip_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_AUDIO_PROFILE, profile_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_PARANOIA, paranoia_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_AUDIO_VOLUME, audio_volume_changed_cb, NULL, NULL, NULL);
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
  message_area_eventbox = glade_xml_get_widget (glade, "message_area_eventbox");
  select_all_menuitem = glade_xml_get_widget (glade, "select_all");
  deselect_all_menuitem = glade_xml_get_widget (glade, "deselect_all");
  submit_menuitem = glade_xml_get_widget (glade, "submit");
  title_entry = glade_xml_get_widget (glade, "title_entry");
  artist_entry = glade_xml_get_widget (glade, "artist_entry");
  duration_label = glade_xml_get_widget (glade, "duration_label");
  genre_entry = glade_xml_get_widget (glade, "genre_entry");
  year_entry = glade_xml_get_widget (glade, "year_entry");
  disc_number_entry = glade_xml_get_widget (glade, "disc_number_entry");
  track_listview = glade_xml_get_widget (glade, "track_listview");
  extract_button = glade_xml_get_widget (glade, "extract_button");
  extract_menuitem = glade_xml_get_widget (glade, "extract_menuitem");
  play_button = glade_xml_get_widget (glade, "play_button");
  play_menuitem = glade_xml_get_widget (glade, "play_menuitem");
  next_menuitem = glade_xml_get_widget (glade, "next_track_menuitem");
  prev_menuitem = glade_xml_get_widget (glade, "previous_track_menuitem");
  status_bar = glade_xml_get_widget (glade, "status_bar");
  duplicate = glade_xml_get_widget (glade, "duplicate_menuitem");
  eject = glade_xml_get_widget (glade, "eject");

  { /* ensure that the play/pause button's size is constant */
    GtkWidget *fake_button1, *fake_button2;
    GtkSizeGroup *size_group;

    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    fake_button1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
    gtk_size_group_add_widget (size_group, fake_button1);
    g_signal_connect_swapped (play_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button1);
    
    fake_button2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
    gtk_size_group_add_widget (size_group, fake_button2);
    g_signal_connect_swapped (play_button, "destroy",
                              G_CALLBACK (gtk_widget_destroy),
                              fake_button2);

    gtk_size_group_add_widget (size_group, play_button);
  }

  setup_genre_entry (genre_entry);

  track_store = gtk_list_store_new (COLUMN_TOTAL, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_POINTER);
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
    gtk_tree_view_column_set_resizable (column, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, renderer, duration_cell_data_cb, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
  }

  extractor = SJ_EXTRACTOR (sj_extractor_new());
  error = sj_extractor_get_new_error (extractor);
  if (error) {
    error_on_start (error);
    exit (1);
  }

  update_ui_for_album (NULL);

  sj_play_init ();

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (track_listview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  http_proxy_setup (gconf_client);
  baseuri_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEURI, NULL, TRUE, NULL), NULL);
  path_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PATH_PATTERN, NULL, TRUE, NULL), NULL);
  file_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FILE_PATTERN, NULL, TRUE, NULL), NULL);
  profile_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_AUDIO_PROFILE, NULL, TRUE, NULL), NULL);
  paranoia_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PARANOIA, NULL, TRUE, NULL), NULL);
  strip_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_STRIP, NULL, TRUE, NULL), NULL);
  eject_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_EJECT, NULL, TRUE, NULL), NULL);
  open_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_OPEN, NULL, TRUE, NULL), NULL);
  audio_volume_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_AUDIO_VOLUME, NULL, TRUE, NULL), NULL);
  if (device == NULL && uris == NULL) {
    device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), GINT_TO_POINTER (TRUE));
  } else {
    if (device)
      set_device (device, TRUE);
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
	set_device (d, TRUE);
	g_free (d);
      } else {
        device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), GINT_TO_POINTER (TRUE));
      }
    }
  }

  if (sj_extractor_supports_encoding (&error) == FALSE) {
    error_on_start (error);
    return 0;
  }

  /* Set whether duplication of a cd is available using the brasero tool */
  gtk_widget_set_sensitive (duplicate, FALSE);
  duplication_enabled = is_cd_duplication_available();

  gconf_bridge_bind_window_size(gconf_bridge_get(), GCONF_WINDOW, GTK_WINDOW (main_window));
  gtk_widget_show (main_window);
  gtk_main ();

  g_object_unref (base_uri);
  g_object_unref (metadata);
  g_object_unref (extractor);
  g_object_unref (gconf_client);
  brasero_media_library_stop ();

  return 0;
}
