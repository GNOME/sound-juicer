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
#include <gnome.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include "sj-about.h"
#include "sj-musicbrainz.h"
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"
#include "sj-prefs.h"

GladeXML *glade;

SjExtractor *extractor;

GConfClient *gconf_client;

GtkWidget *main_window;
static GtkWidget *title_entry, *artist_entry, *duration_label;
static GtkWidget *track_listview, *reread_button, *extract_button;
static GtkWidget *extract_menuitem, *select_all_menuitem, *deselect_all_menuitem;
GtkListStore *track_store;

const char *base_path, *path_pattern, *file_pattern;
static const char *device;
gboolean strip_chars;
gboolean extracting = FALSE;

static AlbumDetails *current_album;

#define DEFAULT_PARANOIA 4

/**
 * Clicked Quit
 */
void on_quit_activate (GtkMenuItem *item, gpointer user_data)
{
  gtk_main_quit ();
}

/**
 * Clicked Eject
 */
void on_eject_activate (GtkMenuItem *item, gpointer user_data)
{
  eject_cdrom (device, GTK_WINDOW (main_window));
}

gboolean on_destory_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  if (extracting) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_NONE,
                                     _("You are currently extracting a CD. Do you want to quit now or contine?"));
    gtk_dialog_add_button (GTK_DIALOG (dialog), "gtk-quit", GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("Continue"), GTK_RESPONSE_REJECT);
    g_signal_connect_swapped (GTK_OBJECT (dialog), 
                              "response", 
                              G_CALLBACK (gtk_widget_destroy),
                              GTK_OBJECT (dialog));
    gtk_widget_show_all (dialog);
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    return response != GTK_RESPONSE_ACCEPT;
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
}

void on_deselect_all_activate (GtkMenuItem *item, gpointer user_data)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), select_all_foreach_cb, GINT_TO_POINTER (FALSE));
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

/**
 * Utility function to update the UI for a given Album
 */
static void update_ui_for_album (AlbumDetails *album)
{
  GList *l;
  int album_duration = 0;
  char* duration_text;

  if (album == NULL) {
    gtk_entry_set_text (GTK_ENTRY (title_entry), "");
    gtk_entry_set_text (GTK_ENTRY (artist_entry), "");
    gtk_label_set_text (GTK_LABEL (duration_label), "");
    gtk_list_store_clear (track_store);
    gtk_widget_set_sensitive (title_entry, FALSE);
    gtk_widget_set_sensitive (artist_entry, FALSE);
    gtk_widget_set_sensitive (extract_button, FALSE);
    gtk_widget_set_sensitive (extract_menuitem, FALSE);
    gtk_widget_set_sensitive (select_all_menuitem, FALSE);
    gtk_widget_set_sensitive (deselect_all_menuitem, FALSE);
  } else {
    gtk_entry_set_text (GTK_ENTRY (title_entry), album->title);
    gtk_entry_set_text (GTK_ENTRY (artist_entry), album->artist);
    gtk_widget_set_sensitive (title_entry, TRUE);
    gtk_widget_set_sensitive (artist_entry, TRUE);
    gtk_widget_set_sensitive (extract_button, TRUE);
    gtk_widget_set_sensitive (extract_menuitem, TRUE);
    gtk_widget_set_sensitive (select_all_menuitem, TRUE);
    gtk_widget_set_sensitive (deselect_all_menuitem, TRUE);
    
    gtk_list_store_clear (track_store);
    for (l = album->tracks; l; l=g_list_next (l)) {
      GtkTreeIter iter;
      TrackDetails *track = (TrackDetails*)l->data;
      album_duration += track->duration;
      gtk_list_store_append (track_store, &iter);
      gtk_list_store_set (track_store, &iter,
                          COLUMN_EXTRACT, TRUE,
                          COLUMN_NUMBER, track->number,
                          COLUMN_TITLE, track->title,
                          COLUMN_ARTIST, track->artist,
                          COLUMN_DURATION, track->duration,
                          COLUMN_DETAILS, track,
                          -1);
    }
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
void basepath_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_BASEPATH) == 0);
  if (entry->value == NULL) {
    base_path = g_strdup (g_get_home_dir ());
  } else {
    base_path = gconf_value_get_string (entry->value);
  }
  /* TODO: sanity check the path somewhat */
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
 * Utility function to reread a CD
 */
void reread_cd (void)
{
  GList *albums;
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
  
  albums = sj_musicbrainz_list_albums(&error);

  if (realized)
    gdk_window_set_cursor (main_window->window, NULL);
  
  if (error) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new (realized ? GTK_WINDOW (main_window) : NULL, 0,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     g_strdup_printf ("<b>%s</b>\n\n%s\n%s: %s",
                                                     _("Could not read the CD"),
                                                     _("Sound Juicer could not read the track listing on this CD."),
                                                     _("Reason"),
                                                     error->message));
    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_error_free (error);
    update_ui_for_album (NULL);
    return;
  }

  /* If there is more than one album... */
  if (g_list_next (albums)) {
    current_album = multiple_album_dialog (albums);
  } else {
    current_album = albums ? albums->data : NULL;
  }
  update_ui_for_album (current_album);
}

/**
 * The GConf key for the device changed
 */
void device_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_DEVICE) == 0);

  if (entry->value == NULL) {
    device = prefs_get_default_device();
    if (device == NULL) {
#ifndef IGNORE_MISSING_CD
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       g_strdup_printf ("<b>%s</b>\n\n%s",
                                                        _("No CD-ROM drives found"),
                                                        _("Sound Juicer could not find any CD-ROM drives to read.")));
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      exit (1);
#endif
    }
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
                                       g_strdup_printf ("<b>%s</b>\n\n%s\n%s: %s",
                                                        _("Could not read the CD"),
                                                        message,
                                                        _("Reason"),
                                                        strerror (errno)));
      g_free (message);
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      /* Set a null device */
      device = NULL;
    }
  }
  sj_musicbrainz_set_cdrom (device);
  sj_extractor_set_device (extractor, device);

  reread_cd();
}

/**
 * The /apps/sound-juicer/format key has changed.
 */
void format_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  char* value;
  GEnumValue* enumvalue;
  g_assert (strcmp (entry->key, GCONF_FORMAT) == 0);
  if (!entry->value) return;
  value = g_ascii_strup (gconf_value_get_string (entry->value), -1);
  /* TODO: this line is pretty convoluted */
  enumvalue = g_enum_get_value_by_name (g_type_class_peek (encoding_format_get_type()), value);
  if (enumvalue == NULL) {
    g_warning (_("Unknown format %s"), value);
    g_free (value);
    return;
  }
  g_object_set (extractor, "format", enumvalue->value, NULL);
  g_free (value);
}

/**
 * Configure the http proxy
 */
void
http_proxy_setup (GConfClient *client)
{
  if (!gconf_client_get_bool (client, GCONF_HTTP_PROXY_ENABLE, NULL))
  {
    sj_musicbrainz_set_proxy (NULL);
  } else {
    const char *host;
    int port;

    host = gconf_client_get_string (client, GCONF_HTTP_PROXY, NULL);
    sj_musicbrainz_set_proxy (host);
    port = gconf_client_get_int (client, GCONF_HTTP_PROXY_PORT, NULL);
    sj_musicbrainz_set_proxy_port (port);
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
  reread_cd ();
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
  gtk_list_store_set (track_store, &iter, COLUMN_EXTRACT, !extract, -1);
}

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

  /* Set the artist field in each tree row */
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (track_store), &iter)) return;
  
  do {
    gtk_tree_model_get (GTK_TREE_MODEL (track_store), &iter, COLUMN_DETAILS, &track, -1);
    g_free (track->artist);
    track->artist = g_strdup (current_album->artist);
    gtk_list_store_set (track_store, &iter, COLUMN_ARTIST, current_album->artist, -1);
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(track_store), &iter));
}

void on_contents_activate(GtkWidget *button, gpointer user_data) {
  GError *error = NULL;

  gnome_help_display("sound-juicer", NULL, &error);
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

static void error_on_start (GError *error)
{
  GtkWidget *dialog;
  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   g_strdup_printf ("<b>%s</b>\n\n%s: %s.\n%s",
                                                    _("Could not start Sound Juicer"),
                                                    _("Reason"),
                                                    error->message,
                                                    _("Please consult the documentation for assistance.")));
  gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
  gtk_dialog_run (GTK_DIALOG (dialog));
}

int main (int argc, char **argv)
{
  GError *error = NULL;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  
  gnome_program_init ("sound-juicer",
		      VERSION, LIBGNOMEUI_MODULE,
		      argc, argv,
		      GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
  g_set_application_name (_("Sound Juicer"));

  sj_musicbrainz_init (&error);
  if (error) {
    error_on_start (error);
    exit (1);
  }

  extractor = SJ_EXTRACTOR (sj_extractor_new());
  error = sj_extractor_get_new_error (extractor);
  if (error) {
    error_on_start (error);
    exit (1);
  }

  gconf_client = gconf_client_get_default ();
  if (gconf_client == NULL) {
    g_print (_("Could not create GConf client.\n"));
    exit (1);
  }

  gconf_client_add_dir (gconf_client, GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_BASEPATH, basepath_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_FORMAT, format_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_PARANOIA, paranoia_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
  gconf_client_add_dir (gconf_client, GCONF_PROXY_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  gconf_client_notify_add (gconf_client, GCONF_HTTP_PROXY_ENABLE, http_proxy_enable_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_HTTP_PROXY, http_proxy_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_HTTP_PROXY_PORT, http_proxy_port_changed_cb, NULL, NULL, NULL);

  glade_init ();
  glade = glade_xml_new (DATADIR"/sound-juicer/sound-juicer.glade", NULL, NULL);
  g_assert (glade != NULL);
  glade_xml_signal_autoconnect (glade);

  main_window = glade_xml_get_widget (glade, "main_window");
  gtk_window_set_icon_from_file (GTK_WINDOW (main_window), PIXMAPDIR"/sound-juicer.png", NULL);

  select_all_menuitem = glade_xml_get_widget (glade, "select_all");
  deselect_all_menuitem = glade_xml_get_widget (glade, "deselect_all");
  extract_menuitem = glade_xml_get_widget (glade, "extract_menuitem");
  title_entry = glade_xml_get_widget (glade, "title_entry");
  artist_entry = glade_xml_get_widget (glade, "artist_entry");
  duration_label = glade_xml_get_widget (glade, "duration_label");
  track_listview = glade_xml_get_widget (glade, "track_listview");
  extract_button = glade_xml_get_widget (glade, "extract_button");
  reread_button = glade_xml_get_widget (glade, "reread_button");

  track_store = gtk_list_store_new (COLUMN_TOTAL, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_POINTER);
  gtk_tree_view_set_model (GTK_TREE_VIEW (track_listview), GTK_TREE_MODEL (track_store));
  {
    GtkTreeViewColumn *column;
    GtkCellRenderer *text_renderer, *toggle_renderer;
    
    toggle_renderer = gtk_cell_renderer_toggle_new ();
    g_signal_connect (toggle_renderer, "toggled", G_CALLBACK (on_extract_toggled), NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Extract?"),
                                                       toggle_renderer,
                                                       "active", COLUMN_EXTRACT,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    text_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Track"),
                                                       text_renderer,
                                                       "text", COLUMN_NUMBER,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    /* TODO: Do I need to create these every time or will a single one do? */
    text_renderer = gtk_cell_renderer_text_new ();
    g_signal_connect (text_renderer, "edited", G_CALLBACK (on_cell_edited), GUINT_TO_POINTER (COLUMN_TITLE));
    g_object_set (G_OBJECT (text_renderer), "editable", TRUE, NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                       text_renderer,
                                                       "text", COLUMN_TITLE,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    text_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Artist"),
                                                       text_renderer,
                                                       "text", COLUMN_ARTIST,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    g_signal_connect (text_renderer, "edited", G_CALLBACK (on_cell_edited), GUINT_TO_POINTER (COLUMN_ARTIST));
    g_object_set (G_OBJECT (text_renderer), "editable", TRUE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
    
    text_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Duration"),
                                                       text_renderer,
                                                       NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_cell_data_func (column, text_renderer, duration_cell_data_cb, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

  }

  /* YUCK. As bad as Baldrick's trousers */
  http_proxy_setup (gconf_client);
  basepath_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEPATH, NULL, TRUE, NULL), NULL);
  path_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PATH_PATTERN, NULL, TRUE, NULL), NULL);
  file_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FILE_PATTERN, NULL, TRUE, NULL), NULL);
  device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);
  format_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FORMAT, NULL, TRUE, NULL), NULL);
  paranoia_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PARANOIA, NULL, TRUE, NULL), NULL);
  strip_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_STRIP, NULL, TRUE, NULL), NULL);

  gtk_widget_show_all (main_window);
  gtk_main ();
  g_object_unref (extractor);
  g_object_unref (gconf_client);
  sj_musicbrainz_destroy ();
  return 0;
}
