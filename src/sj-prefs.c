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

#include "sound-juicer.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>
#include <gconf/gconf-client.h>
#include <profiles/gnome-media-profiles.h>
#include <brasero-drive-selection.h>

#include "sj-util.h"
#include "gconf-bridge.h"
#include "sj-extracting.h"
#include "sj-prefs.h"

extern GladeXML *glade;
extern GtkWidget *main_window;

static GtkWidget *audio_profile;
static GtkWidget *cd_option, *path_option, *file_option, *basepath_fcb, *check_strip, *check_eject, *check_open;
static GtkWidget *path_example_label;

#define DEFAULT_AUDIO_PROFILE_NAME "cdlossy"

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
  GMAudioProfile *profile;
  /* Handle the change being to unselect a profile */
  if (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) != -1) {
    profile = gm_audio_profile_choose_get_active (widget);
    gconf_client_set_string (gconf_client, GCONF_AUDIO_PROFILE,  
                             gm_audio_profile_get_id (profile), NULL);
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
void prefs_base_folder_changed (GtkWidget *chooser, gpointer user_data)
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

void prefs_file_option_changed (GtkComboBox *combo, gpointer user_data)
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

/**
 * The Edit Profiles button was pressed.
 */
void prefs_edit_profile_clicked (GtkButton *button, gpointer user_data)
{
  GtkWidget *dialog;
  dialog = gm_audio_profiles_edit_new (gconf_client, GTK_WINDOW (main_window));
  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
}

static void audio_profile_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  const char *value;
  g_return_if_fail (strcmp (entry->key, GCONF_AUDIO_PROFILE) == 0);
  if (!entry->value) return;
  value = gconf_value_get_string (entry->value);
  
  /* If the value is empty, unset the combo. Otherwise try and set it. */
  if (strcmp (value, "") == 0) {
    gtk_combo_box_set_active (GTK_COMBO_BOX (audio_profile), -1);
  } else {
    gm_audio_profile_choose_set_active (audio_profile, value);
  }
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
  GMAudioProfile *profile;

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
                        gm_audio_profile_get_extension (profile),
                        "</i></small>", NULL);
  g_free (example);
  
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

/**
 * Clicked on Preferences in the UI
 */
void on_edit_preferences_cb (GtkMenuItem *item, gpointer user_data)
{
  static GtkWidget *prefs_dialog = NULL;

  if (prefs_dialog) {
    gtk_window_present (GTK_WINDOW (prefs_dialog));
  } else {
    const char *labels[] = { "cd_label", "path_label", "folder_label", "file_label", "profile_label" };
    guint i;
    GtkSizeGroup *group;
    GConfBridge *bridge = gconf_bridge_get ();

    prefs_dialog = glade_xml_get_widget (glade, "prefs_dialog");
    g_assert (prefs_dialog != NULL);
    g_object_add_weak_pointer (G_OBJECT (prefs_dialog), (gpointer)&prefs_dialog);

    gtk_window_set_transient_for (GTK_WINDOW (prefs_dialog), GTK_WINDOW (main_window));

    group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    for (i = 0; i < G_N_ELEMENTS (labels); i++) {
      GtkWidget *widget;
      widget = glade_xml_get_widget (glade, labels[i]);
      if (widget) {
        gtk_size_group_add_widget (group, widget);
      } else {
        g_warning ("Widget %s not found", labels[i]);
      }
    }
    g_object_unref (group);

    cd_option = glade_xml_get_widget (glade, "cd_option");
    basepath_fcb = glade_xml_get_widget (glade, "path_chooser");
    sj_add_default_dirs (GTK_FILE_CHOOSER (basepath_fcb));
    path_option = glade_xml_get_widget (glade, "path_option");
    file_option = glade_xml_get_widget (glade, "file_option");
    audio_profile = glade_xml_get_widget (glade, "audio_profile");
    check_strip = glade_xml_get_widget (glade, "check_strip");
    check_eject = glade_xml_get_widget (glade, "check_eject");
    check_open = glade_xml_get_widget (glade, "check_open");
    path_example_label = glade_xml_get_widget (glade, "path_example_label");

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
    gconf_client_notify_add (gconf_client, GCONF_AUDIO_PROFILE, audio_profile_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_STRIP, strip_changed_cb, NULL, NULL, NULL);

    g_signal_connect (extractor, "notify::profile", pattern_label_update, NULL);

    baseuri_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEURI, NULL, TRUE, NULL), NULL);
    audio_profile_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_AUDIO_PROFILE, NULL, TRUE, NULL), NULL);
    file_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FILE_PATTERN, NULL, TRUE, NULL), NULL);
    path_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PATH_PATTERN, NULL, TRUE, NULL), NULL);
    device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);

    g_signal_connect (GTK_DIALOG (prefs_dialog), "response", G_CALLBACK (on_response), NULL);

    gtk_widget_show_all (prefs_dialog);
  }
}

