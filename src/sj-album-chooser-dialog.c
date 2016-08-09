/*
 * Copyright (C) 2016 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-album-chooser-dialog.c
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
 * Authors: Phillip Wood <phillip.wood@dunelm.org.uk>
 */

#include "sj-album-chooser-dialog.h"

struct _SjAlbumChooserDialogClass {
  GtkDialogClass parent_class;
};

struct _SjAlbumChooserDialog {
  GtkDialog   parent;
  GtkListBox *list_box;
  GList      *albums;
};

G_DEFINE_TYPE (SjAlbumChooserDialog, sj_album_chooser_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_ALBUMS = 1,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static const gchar* album_quark_text = "sj-album-chooser-dialog-album";
static GQuark album_quark;

static GtkListBoxRow*
create_row (AlbumDetails *album)
{
  return NULL;
}

static void
add_album (gpointer album,
           gpointer list_box)
{
  GtkListBoxRow *row;

  row = create_row (album);
  gtk_list_box_insert (list_box, GTK_WIDGET (row), -1);
}

static void
set_albums (SjAlbumChooserDialog *dialog,
            GList                *albums)
{
  GtkListBoxRow *row;

  dialog->albums = albums;
  if (dialog->albums != NULL) {
    g_list_foreach (albums, add_album, dialog->list_box);
    row = gtk_list_box_get_row_at_index (dialog->list_box, 0);
    gtk_list_box_select_row (dialog->list_box, row);
  }
}

static void
row_activated_cb (GtkListBox    *list_box,
                  GtkListBoxRow *row,
                  gpointer       dialog)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_ACCEPT);
}

static void
sj_album_chooser_dialog_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  SjAlbumChooserDialog *dialog;

  dialog = SJ_ALBUM_CHOOSER_DIALOG (object);

  switch (property_id) {
      case PROP_ALBUMS:
        g_value_set_pointer (value, dialog->albums);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sj_album_chooser_dialog_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  SjAlbumChooserDialog *dialog;

  dialog = SJ_ALBUM_CHOOSER_DIALOG (object);

  switch (property_id) {
      case PROP_ALBUMS:
        set_albums (dialog, g_value_get_pointer (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sj_album_chooser_dialog_class_init (SjAlbumChooserDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = sj_album_chooser_dialog_get_property;
  object_class->set_property = sj_album_chooser_dialog_set_property;

  properties[PROP_ALBUMS] =
    g_param_spec_pointer ("albums", "albums", "albums",
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/sound-juicer/sj-album-chooser-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class,
                                        SjAlbumChooserDialog,
                                        list_box);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);

  album_quark = g_quark_from_static_string (album_quark_text);
}

static void
sj_album_chooser_dialog_init (SjAlbumChooserDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

/**
 * sj_album_chooser_dialog_new:
 * @dialog: The parent window.
 * @albums: (element-type AlbumDetails): The list of albums to show.
 *
 * Create a new dialog showing @albums.
 *
 * Returns: The new dialog.
 */
GtkWidget*
sj_album_chooser_dialog_new (GtkWindow *parent_window,
                             GList     *albums)
{
  GtkSettings *settings;
  gboolean use_header_bar;

  settings = gtk_settings_get_default ();
  g_object_get (settings, "gtk-dialogs-use-header", &use_header_bar, NULL);
  return GTK_WIDGET (g_object_new (SJ_TYPE_ALBUM_CHOOSER_DIALOG,
                                   "albums", albums,
                                   "transient-for", parent_window,
                                   "use-header-bar", use_header_bar,
                                   NULL));
}

/**
 * sj_album_chooser_dialog_get_selected_album:
 * @dialog: The dialog to query.
 *
 * Get the currently selected album.
 *
 * Returns: The selected album
 */
AlbumDetails*
sj_album_chooser_dialog_get_selected_album (SjAlbumChooserDialog *dialog)
{
  GtkListBoxRow *row;

  g_return_val_if_fail (SJ_IS_ALBUM_CHOOSER_DIALOG (dialog), NULL);

  if (dialog->albums == NULL)
    return NULL;

  row = gtk_list_box_get_selected_row (dialog->list_box);
  return g_object_get_qdata ((GObject*) row, album_quark);
}
