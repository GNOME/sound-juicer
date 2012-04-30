/*
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-about.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sound-juicer.h"

#include <string.h>
#include <gst/pbutils/encoding-profile.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <brasero-drive-selection.h>

#include "rb-gst-media-types.h"
#include "sj-util.h"
#include "gconf-bridge.h"
#include "sj-extracting.h"
#include "sj-prefs.h"

extern GtkBuilder *builder;
extern GtkWidget *main_window;

static GtkWidget *audio_profile;
static GtkWidget *cd_option, *path_option, *file_option, *basepath_fcb, *check_strip, *check_eject, *check_open;
static GtkWidget *path_example_label;

typedef struct {
  char* name;
  char* pattern;
} FilePattern;

static const FilePattern path_patterns[] = {
  {N_("Album Artist, Album Title"), "%aa/%at"},
  {N_("Album Artist (sortable), Album Title"), "%as/%at"},
  {N_("Track Artist, Album Title"), "%ta/%at"},
  {N_("Track Artist (sortable), Album Title"), "%ts/%at"},
  {N_("Album Title"), "%at"},
  {N_("Album Artist"), "%aa"},
  {N_("Album Artist (sortable)"), "%as"},
  {N_("Album Artist - Album Title"), "%aa - %at"},
  {N_("Album Artist (sortable) - Album Title"), "%as - %at"},
  {N_("[none]"), "./"},
  {NULL, NULL}
};

static const FilePattern file_patterns[] = {
  {N_("Number - Title"), "%dn - %tt"},
  {N_("Track Title"), "%tt"},
  {N_("Track Artist - Track Title"), "%ta - %tt"},
  {N_("Track Artist (sortable) - Track Title"), "%ts - %tt"},
  {N_("Number. Track Artist - Track Title"), "%dN. %ta - %tt"},
  /* {N_("Number. Track Artist (sortable) - Track Title"), "%tN. %ts - %tt"}, */
  {N_("Number-Track Artist-Track Title (lowercase)"), "%dN-%tA-%tT"},
  /* {N_("Number-Track Artist (sortable)-Track Title (lowercase)"), "%tN-%tS-%tT"}, */
  {NULL, NULL}
};

void prefs_profile_changed (GtkWidget *widget, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  /* Handle the change being to unselect a profile */
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
    char *media_type;
    gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
                        0, &media_type, -1);
    gconf_client_set_string (gconf_client, GCONF_AUDIO_PROFILE_MEDIA_TYPE,
                             media_type, NULL);
    g_free (media_type);
  }
}

/**
 * Show the gnome help browser
 */
void show_help (GtkWindow *parent)
{
  GError *error = NULL;

  gtk_show_uri (NULL, "ghelp:sound-juicer?preferences", GDK_CURRENT_TIME, &error);
  if (error) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (parent,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Could not display help for Sound Juicer\n"
                                       "%s"),
                                     error->message);

    gtk_widget_show_all (dialog);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);
    g_error_free (error);
  }
}

/**
 * Changed folder in the Prefs dialog
 */
G_MODULE_EXPORT void prefs_base_folder_changed (GtkWidget *chooser, gpointer user_data)
{
  char *new_uri, *current_uri;

  current_uri = gconf_client_get_string (gconf_client, GCONF_BASEURI, NULL);
  new_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));

  if (current_uri == NULL || strcmp(current_uri, new_uri) != 0) {
      gconf_client_set_string (gconf_client, GCONF_BASEURI, new_uri, NULL);
  }

  g_free (new_uri);
  g_free (current_uri);
}

void prefs_path_option_changed (GtkComboBox *combo, gpointer user_data)
{
  gint active;
  const char* pattern;
  active = gtk_combo_box_get_active (combo);
  if (active == -1)
    return;

  pattern = path_patterns[active].pattern;
  if (pattern) {
    gconf_client_set_string (gconf_client, GCONF_PATH_PATTERN, pattern, NULL);
  }
}

G_MODULE_EXPORT void prefs_file_option_changed (GtkComboBox *combo, gpointer user_data)
{
  gint active;
  const char* pattern;
  active = gtk_combo_box_get_active (combo);
  if (active == -1)
    return;

  pattern = file_patterns[active].pattern;
  if (pattern) {
    gconf_client_set_string (gconf_client, GCONF_FILE_PATTERN, pattern, NULL);
  }
}

static void
sj_audio_profile_chooser_set_active (GtkWidget *chooser, const char *profile)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean done;

  done = FALSE;
  model = gtk_combo_box_get_model(GTK_COMBO_BOX(chooser));
  if (gtk_tree_model_get_iter_first (model, &iter)) {
    do {
      char *media_type;

      gtk_tree_model_get (model, &iter, 0, &media_type, -1);
      if (g_strcmp0 (media_type, profile) == 0) {
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser), &iter);
        done = TRUE;
      }
      g_free (media_type);
    } while (done == FALSE && gtk_tree_model_iter_next (model, &iter));
  }

  if (done == FALSE) {
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser), NULL);
  }
}

static void audio_profile_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  const char *value;
  g_return_if_fail (strcmp (entry->key, GCONF_AUDIO_PROFILE_MEDIA_TYPE) == 0);
  if (!entry->value) return;
  value = gconf_value_get_string (entry->value);

  sj_audio_profile_chooser_set_active (audio_profile, value);
}

static void baseuri_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  /*
   * The conflict between separation of the prefs and the main window,
   * and the problem of duplication, is very clear here. This code is
   * identical to the code in the main key listener, but modifying the
   * prefs window from the main is ugly. Maybe put the validation code
   * into sj-utils?
   */
  const char* base_uri, *current_uri;
  g_return_if_fail (strcmp (entry->key, GCONF_BASEURI) == 0);

  base_uri = NULL;
  if (entry->value != NULL)
    base_uri = gconf_value_get_string (entry->value);

  if (base_uri == NULL || base_uri[0] == '\0') {
    GFile *dir;
    char *dir_uri;

    dir = sj_get_default_music_directory ();
    dir_uri = g_file_get_uri (dir);
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (basepath_fcb), dir_uri);
    g_free (dir_uri);
    g_object_unref (dir);
  } else {
    g_return_if_fail (entry->value->type == GCONF_VALUE_STRING);
    current_uri = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (basepath_fcb));
    if (current_uri == NULL || strcmp (current_uri, base_uri) != 0)
      gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (basepath_fcb), base_uri);

  }
}

static void pattern_label_update (void)
{
  char *file_pattern, *path_pattern;
  char *file_value, *path_value, *example, *format;
  char *media_type;
  GstEncodingProfile *profile;

  static const AlbumDetails sample_album = {
    "Help!", /* title */
    "The Beatles", /* artist */
    "Beatles, The", /* sortname */
    NULL, /* genre */
    0, /* number of tracks*/
    1, /* disc number */
    NULL, /* track list */
    NULL, /* release date */
    NULL, /* album ID */
    NULL /* artist ID */
  };
  static const TrackDetails sample_track = {
    (AlbumDetails*)&sample_album,  /*album */
    7, /* track number */
    "Ticket To Ride", /* title */
    "The Beatles", /* artist */
    "Beatles, The", /* sortname */
    0, /* duration */
    NULL, /* track ID */
    NULL, /* artist ID */
  };

  g_object_get (extractor, "profile", &profile, NULL);
  /* It's possible the profile isn't set, in which case do nothing */
  if (!profile) {
    return;
  }
  media_type = rb_gst_encoding_profile_get_media_type (profile);
  gst_encoding_profile_unref (profile);

  /* TODO: sucky. Replace with get-gconf-key-with-default mojo */
  file_pattern = gconf_client_get_string (gconf_client, GCONF_FILE_PATTERN, NULL);
  if (file_pattern == NULL) {
    file_pattern = g_strdup(file_patterns[0].pattern);
  }
  path_pattern = gconf_client_get_string (gconf_client, GCONF_PATH_PATTERN, NULL);
  if (path_pattern == NULL) {
    path_pattern = g_strdup(path_patterns[0].pattern);
  }

  file_value = filepath_parse_pattern (file_pattern, &sample_track);
  path_value = filepath_parse_pattern (path_pattern, &sample_track);

  example = g_build_filename (G_DIR_SEPARATOR_S, path_value, file_value, NULL);
  g_free (file_value);
  g_free (file_pattern);
  g_free (path_value);
  g_free (path_pattern);

  format = g_strconcat ("<small><i><b>",
                        _("Example Path"),
                        ":</b> ",
                        example,
                        ".",
                        rb_gst_media_type_to_extension (media_type),
                        "</i></small>", NULL);
  g_free (example);
  g_free (media_type);

  gtk_label_set_markup (GTK_LABEL (path_example_label), format);
  g_free (format);
}

static void path_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  char *value;
  int i = 0;
  g_return_if_fail (strcmp (entry->key, GCONF_PATH_PATTERN) == 0);

  if (entry->value == NULL) {
    value = g_strdup (path_patterns[0].pattern);
  } else if (entry->value->type == GCONF_VALUE_STRING) {
    value = g_strdup (gconf_value_get_string (entry->value));
  } else
    return;
  while (path_patterns[i].pattern && strcmp(path_patterns[i].pattern, value) != 0) {
    i++;
  }
  g_free (value);
  gtk_combo_box_set_active (GTK_COMBO_BOX (path_option), i);
  pattern_label_update ();
}

static void file_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  char *value;
  int i = 0;

  g_return_if_fail (strcmp (entry->key, GCONF_FILE_PATTERN) == 0);

  if (entry->value == NULL) {
    value = g_strdup (file_patterns[0].pattern);
  } else if (entry->value->type == GCONF_VALUE_STRING) {
    value = g_strdup (gconf_value_get_string (entry->value));
  } else
    return;
  while (file_patterns[i].pattern && strcmp(file_patterns[i].pattern, value) != 0) {
    i++;
  }
  g_free (value);
  gtk_combo_box_set_active (GTK_COMBO_BOX (file_option), i);
  pattern_label_update ();
}

/**
 * Default device changed (either GConf key or the widget)
 */
static void device_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_return_if_fail (strcmp (entry->key, GCONF_DEVICE) == 0);

  if (entry->value == NULL)
    return;

  if (entry->value->type == GCONF_VALUE_STRING) {
    BraseroDrive *drive;
    BraseroMediumMonitor *monitor;

    monitor = brasero_medium_monitor_get_default ();
    drive = brasero_medium_monitor_get_drive (monitor, gconf_value_get_string (entry->value));
    brasero_drive_selection_set_active (BRASERO_DRIVE_SELECTION (cd_option), drive);
    g_object_unref (drive);
    g_object_unref (monitor);
  }
}

static void prefs_drive_changed (BraseroDriveSelection *selection, BraseroDrive *drive, gpointer user_data)
{
  if (drive)
    gconf_client_set_string (gconf_client, GCONF_DEVICE, brasero_drive_get_device (drive), NULL);
  else
    gconf_client_set_string (gconf_client, GCONF_DEVICE, NULL, NULL);
}

/**
 * The GConf key for the strip characters option changed
 */
static void strip_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  /* NOTE that strip_changed_cb in sj-main.c will also be called, and will also update
     the global value strip_chars - but which function will get called first?
     Make sure strip_chars is up to date BEFORE calling pattern_label_update */
  g_return_if_fail (strcmp (entry->key, GCONF_STRIP) == 0);

  if (entry->value == NULL) {
    strip_chars = FALSE;
  } else {
    strip_chars = gconf_value_get_bool (entry->value);
  }
  pattern_label_update ();
}

/**
 * Given a FilePattern array, populate the combo box.
 */
static void populate_pattern_combo (GtkComboBox *combo, const FilePattern *patterns)
{
  GtkListStore *store;
  int i;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  for (i = 0; patterns[i].pattern; ++i) {
    GtkTreeIter iter;
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0, _(patterns[i].name), 1, patterns[i].pattern, -1);
  }
  gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
}

static void
on_response (GtkDialog *dialog, gint response, gpointer user_data)
{
  if (response == GTK_RESPONSE_HELP) {
    show_help (GTK_WINDOW (dialog));
  } else {
    gtk_widget_hide (GTK_WIDGET (dialog));
  }
}

static GtkWidget *sj_audio_profile_chooser_new(void)
{
  GstEncodingTarget *target;
  const GList *p;
  GtkWidget *combo_box;
  GtkCellRenderer *renderer;
  GtkTreeModel *model;

  model = GTK_TREE_MODEL (gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER));

  target = rb_gst_get_default_encoding_target ();
  for (p = gst_encoding_target_get_profiles (target); p != NULL; p = p->next) {
    GstEncodingProfile *profile = GST_ENCODING_PROFILE (p->data);
    char *media_type;

    media_type = rb_gst_encoding_profile_get_media_type (profile);
    if (media_type == NULL) {
      continue;
    }
    gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
                                       NULL, NULL, -1,
                                       0, media_type,
                                       1, gst_encoding_profile_get_description (profile),
                                       2, profile, -1);
    g_free (media_type);
  }

  combo_box = gtk_combo_box_new_with_model (model);
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer, "text", 1, NULL);

  return GTK_WIDGET (combo_box);
}

/**
 * Clicked on Preferences in the UI
 */
G_MODULE_EXPORT void on_edit_preferences_cb (GtkMenuItem *item, gpointer user_data)
{
  static GtkWidget *prefs_dialog = NULL;

  if (prefs_dialog) {
    gtk_window_present (GTK_WINDOW (prefs_dialog));
  } else {
    const char *labels[] = { "cd_label", "path_label", "folder_label", "file_label", "profile_label" };
    guint i;
    GtkSizeGroup *group;
    GtkWidget    *box;
    GConfBridge *bridge = gconf_bridge_get ();

    prefs_dialog = GET_WIDGET ("prefs_dialog");
    box          = GET_WIDGET ("hack_hbox");
    g_assert (prefs_dialog != NULL);
    g_object_add_weak_pointer (G_OBJECT (prefs_dialog), (gpointer)&prefs_dialog);

    gtk_window_set_transient_for (GTK_WINDOW (prefs_dialog), GTK_WINDOW (main_window));

    group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    for (i = 0; i < G_N_ELEMENTS (labels); i++) {
      GtkWidget *widget;
      widget = GET_WIDGET (labels[i]);
      if (widget) {
        gtk_size_group_add_widget (group, widget);
      } else {
        g_warning ("Widget %s not found", labels[i]);
      }
    }
    g_object_unref (group);

    cd_option          = GET_WIDGET ("cd_option");
    basepath_fcb       = GET_WIDGET ("path_chooser");
    path_option        = GET_WIDGET ("path_option");
    file_option        = GET_WIDGET ("file_option");
#if 0
    /* FIXME: This cannot be currently used, because aufio profile selector
     * 	      from gnome-media-profiles package is not fully qualified widget.
     * 	      Once gnome-media package is updated, this widget can be created
     * 	      using GtkBuilder. */
    audio_profile      = GET_WIDGET ("audio_profile");
#else
    audio_profile = sj_audio_profile_chooser_new();
    g_signal_connect (G_OBJECT (audio_profile), "changed",
                      G_CALLBACK (prefs_profile_changed), NULL);
    gtk_box_pack_start (GTK_BOX (box), audio_profile, TRUE, TRUE, 0);
    gtk_widget_show (audio_profile);
#endif
    check_strip        = GET_WIDGET ("check_strip");
    check_eject        = GET_WIDGET ("check_eject");
    check_open         = GET_WIDGET ("check_open");
    path_example_label = GET_WIDGET ("path_example_label");

    sj_add_default_dirs (GTK_FILE_CHOOSER (basepath_fcb));
    populate_pattern_combo (GTK_COMBO_BOX (path_option), path_patterns);
    g_signal_connect (path_option, "changed", G_CALLBACK (prefs_path_option_changed), NULL);
    populate_pattern_combo (GTK_COMBO_BOX (file_option), file_patterns);
    g_signal_connect (file_option, "changed", G_CALLBACK (prefs_file_option_changed), NULL);

    g_signal_connect (cd_option, "drive-changed", G_CALLBACK (prefs_drive_changed), NULL);

    /* Connect to GConf to update the UI */
    gconf_bridge_bind_property (bridge, GCONF_EJECT, G_OBJECT (check_eject), "active");
    gconf_bridge_bind_property (bridge, GCONF_OPEN, G_OBJECT (check_open), "active");
    gconf_bridge_bind_property (bridge, GCONF_STRIP, G_OBJECT (check_strip), "active");
    gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_BASEURI, baseuri_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_AUDIO_PROFILE_MEDIA_TYPE, audio_profile_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_STRIP, strip_changed_cb, NULL, NULL, NULL);

    g_signal_connect (extractor, "notify::profile", pattern_label_update, NULL);

    baseuri_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEURI, NULL, TRUE, NULL), NULL);
    audio_profile_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_AUDIO_PROFILE_MEDIA_TYPE, NULL, TRUE, NULL), NULL);
    file_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FILE_PATTERN, NULL, TRUE, NULL), NULL);
    path_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PATH_PATTERN, NULL, TRUE, NULL), NULL);
    device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);

    g_signal_connect (GTK_DIALOG (prefs_dialog), "response", G_CALLBACK (on_response), NULL);

    gtk_widget_show_all (prefs_dialog);
  }
}

