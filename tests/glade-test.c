#include "config.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-file-entry.h>
#include "bacon-cd-selection.h"
#include "sj-musicbrainz.h"
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"

GladeXML *glade;

SjExtractor *extractor;

GConfClient *gconf_client;

GtkWidget *main_window, *progress_dialog = NULL;
GtkWidget *title_label, *artist_label, *duration_label, *basepath_label;
GtkWidget *track_listview, *cd_option, *reread_button, *extract_button;
GtkWidget *extract_menuitem, *select_all_menuitem;
GtkWidget *progress_label, *track_progress, *album_progress;
GtkListStore *track_store;

EncoderFormat encoding_format = VORBIS;

const char *base_path;
const char *device;
int track_duration; /* duration of current track for progress dialog */
gboolean ripping = FALSE;

GList *pending = NULL; /* a GList of TrackDetails* to rip */
int total_ripping; /* the number of tracks to rip, not decremented */

#define GCONF_ROOT "/apps/sound-juicer"
#define GCONF_DEVICE GCONF_ROOT "/device"
#define GCONF_BASEPATH GCONF_ROOT "/base_path"
#define GCONF_FORMAT GCONF_ROOT "/format"

enum {
  COLUMN_EXTRACT,
  COLUMN_NUMBER,
  COLUMN_TITLE,
  COLUMN_ARTIST,
  COLUMN_DURATION,
  COLUMN_DETAILS,
  COLUMN_TOTAL
};

/**
 * Clicked About in the menus
 */
void on_about_activate (GtkMenuItem *item, gpointer user_data)
{
  GtkWidget *dialog;
  const char* authors[] = {"Ross Burton <ross@burtonini.com>", NULL};
  dialog = gnome_about_new (_("Sound Juicer"),
                            VERSION,
                            _("Copyright (C) 2003 Ross Burton"),
                            _("A CD ripper"),
                            authors,
                            NULL, NULL, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

/**
 * Clicked Quit
 */
void on_quit_activate (GtkMenuItem *item, gpointer user_data)
{
  g_object_unref (extractor);
  gtk_main_quit ();
}

gboolean on_destory_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  if (ripping) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_window), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_NONE,
                                     _("You are currently ripping a CD. Do you want to quit now or contine ripping?"));
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
  gtk_list_store_set (track_store, iter, COLUMN_EXTRACT, TRUE, -1);
  return FALSE;
}

void on_select_all_activate (GtkMenuItem *item, gpointer user_data)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), select_all_foreach_cb, NULL);  
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
  GValue v = {0};
  int duration;

  gtk_tree_model_get_value (tree_model, iter, COLUMN_DURATION, &v);
  duration = g_value_get_int (&v);

  g_value_unset (&v);
  g_value_init(&v, G_TYPE_STRING);  
  if (duration) {
    g_value_set_string (&v, g_strdup_printf("%d:%02d", duration / 60, duration % 60));
  } else {
    g_value_set_string (&v, _("(unknown)"));
  }
  g_object_set_property (G_OBJECT (cell), "text", &v);
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
    gtk_label_set_text (GTK_LABEL (title_label), "");
    gtk_label_set_text (GTK_LABEL (artist_label), "");
    gtk_label_set_text (GTK_LABEL (duration_label), "");
    gtk_list_store_clear (track_store);
    gtk_widget_set_sensitive (extract_button, FALSE);
    gtk_widget_set_sensitive (extract_menuitem, FALSE);
    gtk_widget_set_sensitive (select_all_menuitem, FALSE);
  } else {
    gtk_label_set_text (GTK_LABEL (title_label), album->title);
    gtk_label_set_text (GTK_LABEL (artist_label), album->artist);
    gtk_widget_set_sensitive (extract_button, TRUE);
    gtk_widget_set_sensitive (extract_menuitem, TRUE);
    gtk_widget_set_sensitive (select_all_menuitem, TRUE);
    
    gtk_list_store_clear (track_store);
    for (l = album->tracks; l; l=g_list_next (l)) {
      GtkTreeIter iter;
      TrackDetails *track = (TrackDetails*)l->data;
      album_duration += track->duration;
      gtk_list_store_append (track_store, &iter);
      gtk_list_store_set (track_store, &iter,
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
  gtk_label_set_text (GTK_LABEL (basepath_label), base_path);
}

/**
 * Utility function to reread a CD
 */
void reread_cd (void)
{
  GList *albums;
  GError *error = NULL;

  albums = sj_musicbrainz_list_albums(&error);
  if (error) {
    g_print (_("Cannot list albums: %s\n"), error->message);
    g_error_free (error);
    return;
  }

  /* If there is more than one album... */
  if (g_list_next (albums)) {
    update_ui_for_album(multiple_album_dialog (albums));
  } else {
    update_ui_for_album (albums ? albums->data : NULL);
  }
}

/**
 * The GConf key for the device changed
 */
void device_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_DEVICE) == 0);

  if (entry->value == NULL) {
    device = bacon_cd_selection_get_device (BACON_CD_SELECTION (cd_option));
    if (device == NULL) {
#if 0
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (NULL, 0,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("<b>No CD-ROMs found</b>\n\n"
                                       "Sound Juicer could not find any CD-ROM drives to read."));
      gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
      gtk_dialog_run (GTK_DIALOG (dialog));
      exit(1); /* TODO: fix */
#endif
    }
  } else {
    device = gconf_value_get_string (entry->value);
    /* TODO: sanity check device */
  }
  sj_musicbrainz_set_cdrom (device);
  sj_extractor_set_device (extractor, device);

  reread_cd();
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
  return FALSE;
}

/**
 * Create a directory and all of its parents.
 */
static void mkdirs (const char* directory, mode_t mode, GError **error)
{
  /* TODO: this is vile */
  gboolean last = FALSE;
  char* dir = strdup (directory);
  char* p = strchr (dir+1, '/');

  if (p == NULL) {
    if (mkdir (dir, mode) == -1) {
      if (errno == EEXIST) return;
      g_set_error (error,
                   SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                   _("Could not create directory %s: %s"), directory, strerror (errno));
      return;
    }
  }
  
  while (p && !last) {
    *p = '\0';
    g_message(p);
    if (mkdir (dir, mode) == -1) {
      if (errno == EEXIST) goto next;
      g_set_error (error,
                   SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                   _("Could not create directory %s: %s"), directory, strerror (errno));
      return;
    }
  next:
    *p = '/';
    p = strchr (p+1, '/');
    if (p == NULL) {
      p = dir;
      last = TRUE;
    }
  }
}

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
    mkdirs (directory, 0750, &error);
    if (error) {
      g_warning (_("mkdir() failed: %s"), error->message);
      g_error_free (error);
      return;
    }
  }
  g_free (directory);
  ripping = TRUE;
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
  gtk_tree_model_foreach (GTK_TREE_MODEL (track_store), rip_track_foreach_cb, NULL);
  pop_and_rip();
}

/**
 * Clicked on Reread in the UI (button/menu)
 */
void on_reread_activate (GtkWidget *button, gpointer user_data)
{
  reread_cd ();
}

/**
 * Changed the CD-ROM device in the prefs dialog
 */
void prefs_cdrom_changed_cb (GtkWidget *widget, const char* device)
{
  gconf_client_set_string (gconf_client, "/apps/sound-juicer/device", device, NULL);
}

/**
 * Clicked on Browse in the Prefs dialog
 */
void prefs_browse_clicked (GtkButton *button, gpointer user_data)
{
  g_print ("%s: TODO\n", __FUNCTION__);
}

/**
 * Cancel in the progress dialog clicked
 */
void on_progress_cancel_clicked (GtkWidget *button, gpointer user_data)
{
  sj_extractor_cancel_extract(extractor);
  gtk_widget_hide (progress_dialog);
}

static GtkWidget *format_vorbis, *format_mpeg, *format_flac, *format_wave;

/**
 * Clicked on Preferences in the UI
 */
void on_edit_preferences_cb (GtkMenuItem *item, gpointer user_data)
{
  static GtkWidget *dialog;
  if (dialog == NULL) {
    dialog = glade_xml_get_widget (glade, "prefs_dialog");
    g_assert (dialog != NULL);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));
    /* Maybe connect a GConf notify to the base_path key here to update the label... */
    format_vorbis = glade_xml_get_widget (glade, "format_vorbis");
    format_mpeg = glade_xml_get_widget (glade, "format_mpeg");
    format_flac = glade_xml_get_widget (glade, "format_flac");
    format_wave = glade_xml_get_widget (glade, "format_wave");
  }
  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);
}

/**
 * The /apps/sound-juicer/format key has changed. Argh where to put
 * this!
 */
void format_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  const char* value;
  g_assert (strcmp (entry->key, GCONF_FORMAT) == 0);
  if (!entry->value) return;
  value = gconf_value_get_string (entry->value);
  if (strcmp ("vorbis", value) == 0) {
    encoding_format = VORBIS;
  } else if (strcmp ("mpeg", value) == 0) {
    encoding_format = MPEG;
  } else if (strcmp ("flac", value) == 0) {
    encoding_format = FLAC;
  } else if (strcmp ("wave", value) == 0) {
    encoding_format = WAVE;
  } else {
    g_warning (_("Unknown format '%s'"), value);
  }
}

/**
 * One of the format toggle buttons in the prefs dialog has been
 * toggled.
 */
void on_format_toggled (GtkToggleButton *togglebutton,
                               gpointer user_data)
{
  const char* format;
  if (!gtk_toggle_button_get_active (togglebutton)) {
    return;
  }
  if (GTK_WIDGET (togglebutton) == format_vorbis) {
    format = "vorbis";
  } else if (GTK_WIDGET(togglebutton) == format_mpeg) {
    format = "mpeg";
  } else if (GTK_WIDGET(togglebutton) == format_flac) {
    format = "flac";
  } else if (GTK_WIDGET(togglebutton) == format_wave) {
    format = "wave";
  } else {
    return;
  }
  gconf_client_set_string (gconf_client, GCONF_FORMAT, format, NULL); /* TODO: GError */
}

/**
 * Called when the user clicked on the Extract? column check boxes
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

/**
 * Called by sj-gstreamer to report progress
 */
static void on_progress_cb (SjExtractor *extractor, int seconds)
{
  if (track_duration != 0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (track_progress), (float)seconds/(float)track_duration);
  } else {
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (track_progress));
  }
  return;
}

static void on_completion_cb (SjExtractor *extractor)
{
  ripping = FALSE;
  /* TODO: uncheck the Extract? check box */
  pop_and_rip ();
  return;
}

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);
  sj_musicbrainz_init ();

  extractor = SJ_EXTRACTOR (sj_extractor_new());
  g_signal_connect (extractor, "progress", G_CALLBACK(on_progress_cb), NULL);
  g_signal_connect (extractor, "completion", G_CALLBACK(on_completion_cb), NULL);

  gconf_client = gconf_client_get_default();
  g_assert (gconf_client != NULL);
  /* TODO: move paths and key names to defines */
  gconf_client_add_dir (gconf_client, GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_BASEPATH, basepath_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_FORMAT, format_changed_cb, NULL, NULL, NULL);

  glade = glade_xml_new ("../data/sound-juicer.glade", NULL, NULL);
  g_assert (glade != NULL);
  glade_xml_signal_autoconnect (glade);

  main_window = glade_xml_get_widget (glade, "main_window");
  select_all_menuitem = glade_xml_get_widget (glade, "select_all");
  extract_menuitem = glade_xml_get_widget (glade, "extract_menuitem");
  title_label = glade_xml_get_widget (glade, "title_label");
  artist_label = glade_xml_get_widget (glade, "artist_label");
  duration_label = glade_xml_get_widget (glade, "duration_label");
  basepath_label = glade_xml_get_widget (glade, "path_label"); /* TODO: from prefs, urgh. gconf it separately */
  track_listview = glade_xml_get_widget (glade, "track_listview");
  cd_option = glade_xml_get_widget (glade, "cd_option"); /* also from prefs, urgh */
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
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    text_renderer = gtk_cell_renderer_text_new (); /* TODO: is this cheating? */
    column = gtk_tree_view_column_new_with_attributes (_("Track"),
                                                       text_renderer,
                                                       "text", COLUMN_NUMBER,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                       text_renderer,
                                                       "text", COLUMN_TITLE,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    column = gtk_tree_view_column_new_with_attributes (_("Artist"),
                                                       text_renderer,
                                                       "text", COLUMN_ARTIST,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
    
    column = gtk_tree_view_column_new_with_attributes (_("Duration"),
                                                       text_renderer,
                                                       NULL);
    gtk_tree_view_column_set_cell_data_func (column, text_renderer, duration_cell_data_cb, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

  }
  
  /* YUCK. As bad as Baldrick's trousers */
  basepath_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEPATH, NULL, TRUE, NULL), NULL);
  device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);
  format_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FORMAT, NULL, TRUE, NULL), NULL);
    
  gtk_widget_show_all(main_window);
  gtk_main();
  return 0;
}
