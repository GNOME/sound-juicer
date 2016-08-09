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
#include "sj-util.h"
#include <glib/gi18n.h>

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


/**
 * NULL safe utility to collate utf8 strings
 */
static gint
collate (const char *a, const char *b)
{
  gint ret_val = 0;

  if (a) {
    if (b) {
      ret_val = g_utf8_collate (a, b);
    } else {
      ret_val = 1;
    }
  } else if (b) {
    ret_val = -1;
  }
  return ret_val;
}

static gint
sj_gst_date_time_compare_field (const GstDateTime *lhs,
                                const GstDateTime *rhs,
                                gboolean (*has_field) (const GstDateTime *datetime),
                                gint     (*get_field) (const GstDateTime *datetime))
{
  gint field_lhs = -1;
  gint field_rhs = -1;

  if (has_field (lhs)) {
    field_lhs = get_field (lhs);
  }
  if (has_field (rhs)) {
    field_rhs = get_field (rhs);
  }

  return (field_lhs - field_rhs);
}

static gint
sj_gst_date_time_compare (const GstDateTime *lhs,
                          const GstDateTime *rhs)
{
  int comparison;

  comparison = sj_gst_date_time_compare_field (lhs, rhs,
                                               gst_date_time_has_year,
                                               gst_date_time_get_year);
  if (comparison != 0) {
      return comparison;
  }

  comparison = sj_gst_date_time_compare_field (lhs, rhs,
                                               gst_date_time_has_month,
                                               gst_date_time_get_month);
  if (comparison != 0) {
      return comparison;
  }

  comparison = sj_gst_date_time_compare_field (lhs, rhs,
                                               gst_date_time_has_day,
                                               gst_date_time_get_day);

  return comparison;
}

/**
 * Utility function to sort albums in multiple_album_dialog
 */
static gint
sort_release_info (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  AlbumDetails *album_a, *album_b;
  GList *label_a, *label_b;
  gint ret_val = 0;

  album_a = g_object_get_qdata (G_OBJECT (row1), album_quark);
  album_b = g_object_get_qdata (G_OBJECT (row2), album_quark);

  ret_val = collate (album_a->title, album_b->title);
  if (ret_val)
    return ret_val;

  ret_val = collate (album_a->artist_sortname, album_b->artist_sortname);
  if (ret_val)
    return ret_val;

  ret_val = collate (album_a->country, album_b->country);
  if (ret_val)
    return ret_val;

  if (album_a->release_date) {
    if (album_b->release_date) {
      ret_val = sj_gst_date_time_compare (album_a->release_date, album_b->release_date);
      if (ret_val)
        return ret_val;
    } else {
      return -1;
    }
  } else if (album_b->release_date) {
    return 1;
  }

  label_a = album_a->labels;
  label_b = album_b->labels;
  while (label_a != NULL && label_b != NULL) {
    LabelDetails *a;
    LabelDetails *b;

    a = label_a->data;
    b = label_b->data;
    ret_val = collate (a->sortname,b->sortname);
    if (ret_val)
      return ret_val;

    label_a = label_a->next;
    label_b = label_b->next;
  }

  if (label_a && !label_b)
    return -1;
  if (!label_a && label_b)
    return 1;

  ret_val = (album_a->disc_number < album_b->disc_number) ? -1 :
    ((album_a->disc_number > album_b->disc_number) ? 1 : 0);
  if (ret_val)
    return ret_val;

  return (album_a->disc_count < album_b->disc_count) ? -1 :
    ((album_a->disc_count > album_b->disc_count) ? 1 : 0);
}

/**
 * Utility function to format label string for multiple_album_dialog
 */
static GString*
format_label_text (GList* labels)
{
  int count;
  GString *label_text;

  if (labels == NULL)
    return NULL;

  label_text = g_string_new (NULL);
  count = g_list_length (labels);
  while (count > 2) {
    g_string_append (label_text, ((LabelDetails*)labels->data)->name);
    g_string_append (label_text, ", ");
    labels = labels->next;
    count--;
  }

  if (count > 1) {
    g_string_append (label_text, ((LabelDetails*)labels->data)->name);
    g_string_append (label_text, " & ");
    labels = labels->next;
  }

  g_string_append (label_text, ((LabelDetails*)labels->data)->name);

  return label_text;
}

/**
 * Utility function for multiple_album_dialog to format the
 * catalog number and barcode of a release.
 */
static gchar*
format_catalog_number_text (GList* labels)
{
   int count;
   GString *catalog_text;
   GList *l;

   if (labels == NULL)
     return NULL;

   /* Count how may label entries actually have a catalog number entry */
   count = 0;
   for (l = labels; l != NULL; l = l->next) {
     if (((LabelDetails*)l->data)->catalog_number != NULL)
         count++;
   }

   if (count == 0)
      return NULL;

   /* Translators: this string is a list of catalog number(s) used by
      the label(s) to identify the release */
   catalog_text = g_string_new (ngettext("Catalog No.: ",
                                         "Catalog Nos.: ",
                                         count));
   for(l = labels; l != NULL; l = l->next) {
     char *catalog_number = ((LabelDetails*)l->data)->catalog_number;
     if (catalog_number != NULL) {
       if (count > 1) {
           g_string_append_printf (catalog_text, "%s, ", catalog_number);
       } else {
           g_string_append (catalog_text, catalog_number);
       }
       count--;
     }
   }

   return g_string_free (catalog_text, FALSE);
}

/**
 * Utility function for multiple_album_dialog to format the
 * release label, date and country.
 */
static gchar*
format_release_details (AlbumDetails *album)
{
  GString *details;
  GString *label_text = NULL;
  gchar *catalog_number_text = NULL;

  details = g_string_new(NULL);

  if (album->labels != NULL) {
    label_text = format_label_text (album->labels);
    catalog_number_text = format_catalog_number_text (album->labels);
  }

  if (!sj_str_is_empty (album->country)) {
    if (album->labels) {
      if (album->release_date && gst_date_time_has_year (album->release_date)) {
        /* Translators: this string appears when multiple CDs were
           found in musicbrainz online database, it corresponds to
           "Released: <country> in <year> on <label>" */
        g_string_append_printf (details, _("Released: %s in %d on %s"),
                                album->country,
                                gst_date_time_get_year (album->release_date),
                                label_text->str);
      } else {
        /* Translators: this string appears when multiple CDs were
           found in musicbrainz online database, it corresponds to
           "Released: <country> on <label>" */
        g_string_append_printf (details, _("Released: %s on %s"),
                                album->country, label_text->str);
      }
    } else if (album->release_date && gst_date_time_has_year (album->release_date)) {
      /* Translators: this string appears when multiple CDs were
         found in musicbrainz online database, it corresponds to
         "Released: <country> in <year>" */
      g_string_append_printf (details, _("Released: %s in %d"),
                              album->country,
                              gst_date_time_get_year (album->release_date));
    } else {
      /* Translators: this string appears when multiple CDs were
         found in musicbrainz online database, it corresponds to
         "Released: <country>" */
      g_string_append_printf (details, _("Released: %s"), album->country);
    }
  } else if (album->release_date && gst_date_time_has_year (album->release_date)) {
    if (album->labels) {
        /* Translators: this string appears when multiple CDs were
           found in musicbrainz online database, it corresponds to
           "Released in <year> on <label>" */
       g_string_append_printf (details, _("Released in %d on %s"),
                               gst_date_time_get_year (album->release_date),
                               label_text->str);
    } else {
        /* Translators: this string appears when multiple CDs were
           found in musicbrainz online database, it corresponds to
           "Released in <year>" */
        g_string_append_printf (details, _("Released in %d"),
                                gst_date_time_get_year (album->release_date));
    }
  } else if (album->labels) {
    /* Translators: this string appears when multiple CDs were
       found in musicbrainz online database, it corresponds to
       "Released on <label>" */
    g_string_append_printf (details, _("Released on %s"), label_text->str);
  } else {
    g_string_append_printf (details,
                            _("Release label, year & country unknown"));
  }

  if (catalog_number_text != NULL) {
    if (album->barcode != NULL) {
      g_string_append_printf (details,
                              "\n%s, %s %s",
                              catalog_number_text,
                              /* Translators: this string identifies
                                 the number of the barcode printed on
                                 the release */
                              _("Barcode:"),
                              album->barcode);
    } else {
      g_string_append_printf (details, "\n%s", catalog_number_text);
    }
  } else {
    if (album->barcode != NULL) {
      g_string_append_printf (details,
                              "\n%s %s",
                              _("Barcode:"),
                              album->barcode);
    }
  }

  if (label_text)
    g_string_free (label_text, TRUE);

  g_free (catalog_number_text);

  return g_string_free (details, FALSE);
}

static GtkListBoxRow*
create_row (AlbumDetails *album)
{
  GObject *label, *row;
  GString *album_title;
  gchar *release_details;
  gchar *markup;

  album_title = g_string_new (album->title);
  release_details = format_release_details (album);
  if (album->disc_number > 0 && album->disc_count > 1)
    g_string_append_printf (album_title,_(" (Disc %d/%d)"),
                            album->disc_number, album->disc_count);

  markup = g_markup_printf_escaped ("<b>%s</b>\n<b><i>%s</i></b>\n%s",
                                    album_title->str,
                                    album->artist,
                                    release_details);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", markup,
                        "margin-bottom", 6,
                        "margin-end", 6,
                        "margin-start", 6,
                        "margin-top", 6,
                        "use-markup", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0,
                        NULL);
  g_free (markup);
  g_string_free (album_title, TRUE);
  g_free (release_details);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", label,
                      "visible", TRUE,
                      NULL);
  g_object_set_qdata (row, album_quark, album);
  return GTK_LIST_BOX_ROW (row);
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
  gtk_list_box_set_sort_func (dialog->list_box,
                              sort_release_info,
                              NULL,
                              NULL);
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
