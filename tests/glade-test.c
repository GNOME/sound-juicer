#include "sound-juicer.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>
#include <gconf/gconf-client.h>
#include "sj-about.h"
#include "sj-musicbrainz.h"
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"
#include "prefs-test.h"

GladeXML *glade;

SjExtractor *extractor;

static GConfClient *gconf_client;

GtkWidget *main_window;
static GtkWidget *title_label, *artist_label, *duration_label;
static GtkWidget *track_listview, *reread_button, *extract_button;
static GtkWidget *extract_menuitem, *select_all_menuitem;
GtkListStore *track_store;

static EncoderFormat encoding_format = VORBIS;

const char *base_path;
static const char *device;
gboolean extracting = FALSE;

#define GCONF_ROOT "/apps/sound-juicer"
#define GCONF_DEVICE GCONF_ROOT "/device"
#define GCONF_BASEPATH GCONF_ROOT "/base_path"
#define GCONF_FORMAT GCONF_ROOT "/format"

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
  if (extracting) {
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
  GValue v = {0,};
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
    update_ui_for_album (NULL);
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
    device = prefs_get_default_device();
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
      gtk_main_quit ();
      return;
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
 * Clicked on Reread in the UI (button/menu)
 */
void on_reread_activate (GtkWidget *button, gpointer user_data)
{
  reread_cd ();
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

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);
  sj_musicbrainz_init ();

  extractor = SJ_EXTRACTOR (sj_extractor_new());

  gconf_client = gconf_client_get_default();
  g_assert (gconf_client != NULL);
  gconf_client_add_dir (gconf_client, GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_BASEPATH, basepath_changed_cb, NULL, NULL, NULL);
  gconf_client_notify_add (gconf_client, GCONF_FORMAT, format_changed_cb, NULL, NULL, NULL);

  glade = glade_xml_new ("../data/sound-juicer.glade", NULL, NULL);
  g_assert (glade != NULL);
  glade_xml_signal_autoconnect (glade);

  main_window = glade_xml_get_widget (glade, "main_window");
  gtk_window_set_icon_from_file (GTK_WINDOW (main_window), "../data/sound-juicer.png", NULL);

  select_all_menuitem = glade_xml_get_widget (glade, "select_all");
  extract_menuitem = glade_xml_get_widget (glade, "extract_menuitem");
  title_label = glade_xml_get_widget (glade, "title_label");
  artist_label = glade_xml_get_widget (glade, "artist_label");
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
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    text_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Track"),
                                                       text_renderer,
                                                       "text", COLUMN_NUMBER,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    /* TODO: Do I need to create these every time or will a single one do? */
    text_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                       text_renderer,
                                                       "text", COLUMN_TITLE,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);

    text_renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Artist"),
                                                       text_renderer,
                                                       "text", COLUMN_ARTIST,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (track_listview), column);
    
    text_renderer = gtk_cell_renderer_text_new ();
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
    
  gtk_widget_show_all (main_window);
  gtk_main ();
  return 0;
}
