/*
 * Copyright (C) 2014 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-cell-renderer-text.c
 *
 * This is based on gtkcellrenderertext.c from GTK
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
 */

#include "sj-cell-renderer-text.h"

#define SJ_CELL_RENDERER_TEXT_PATH "sj-cell-renderer-text-path"

typedef struct _SjCellRendererTextPrivate SjCellRendererTextPrivate;

struct _SjCellRendererTextPrivate
{
  GtkWidget *entry;
  gulong focus_out_id;
  gulong populate_popup_id;
  guint entry_menu_popdown_timeout;
  gboolean in_entry_menu;
};

G_DEFINE_TYPE_WITH_PRIVATE (SjCellRendererText, sj_cell_renderer_text, GTK_TYPE_CELL_RENDERER_TEXT);

static void
sj_cell_renderer_text_editing_done (GtkCellEditable *entry,
                                    gpointer         data)
{
  SjCellRendererTextPrivate *priv;
  const gchar *path;
  const gchar *new_text;
  gboolean canceled;

  priv = sj_cell_renderer_text_get_instance_private (data);

  g_clear_object (&priv->entry);

  if (priv->focus_out_id > 0) {
    g_signal_handler_disconnect (entry, priv->focus_out_id);
    priv->focus_out_id = 0;
  }

  if (priv->populate_popup_id > 0) {
    g_signal_handler_disconnect (entry, priv->populate_popup_id);
    priv->populate_popup_id = 0;
  }

  if (priv->entry_menu_popdown_timeout) {
    g_source_remove (priv->entry_menu_popdown_timeout);
    priv->entry_menu_popdown_timeout = 0;
  }

  g_object_get (entry,
                "editing-canceled", &canceled,
                NULL);
  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (data), canceled);

  if (canceled)
    return;

  path = g_object_get_data (G_OBJECT (entry), SJ_CELL_RENDERER_TEXT_PATH);
  new_text = gtk_entry_get_text (GTK_ENTRY (entry));

  g_signal_emit_by_name (data, "edited", 0, path, new_text);
}

static gboolean
popdown_timeout (gpointer data)
{
  SjCellRendererTextPrivate *priv;

  priv = sj_cell_renderer_text_get_instance_private (data);

  priv->entry_menu_popdown_timeout = 0;

  if (!gtk_widget_is_focus (GTK_WIDGET (priv->entry)))
    sj_cell_renderer_text_editing_done (GTK_CELL_EDITABLE (priv->entry), data);

  return FALSE;
}

static void
sj_cell_renderer_text_popup_unmap (GtkMenu  *menu,
                                   gpointer  data)
{
  SjCellRendererTextPrivate *priv;

  priv = sj_cell_renderer_text_get_instance_private (data);

  priv->in_entry_menu = FALSE;

  if (priv->entry_menu_popdown_timeout)
    return;

  priv->entry_menu_popdown_timeout = g_timeout_add (500, popdown_timeout,
                                                    data);
  g_source_set_name_by_id (priv->entry_menu_popdown_timeout,
                           "[sjcellrendertext] popdown_timeout");
}

static void
sj_cell_renderer_text_populate_popup (GtkEntry *entry,
                                      GtkMenu  *menu,
                                      gpointer  data)
{
  SjCellRendererTextPrivate *priv;

  priv = sj_cell_renderer_text_get_instance_private (data);

  if (priv->entry_menu_popdown_timeout) {
    g_source_remove (priv->entry_menu_popdown_timeout);
    priv->entry_menu_popdown_timeout = 0;
  }

  priv->in_entry_menu = TRUE;

  g_signal_connect (menu, "unmap",
                    G_CALLBACK (sj_cell_renderer_text_popup_unmap), data);
}

static gboolean
sj_cell_renderer_text_focus_out_event (GtkWidget *entry,
                                       GdkEvent  *event,
                                       gpointer   data)
{
  SjCellRendererTextPrivate *priv;

  priv = sj_cell_renderer_text_get_instance_private (data);

  if (priv->in_entry_menu || gtk_widget_is_focus (entry))
    return FALSE;

  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

  /* entry needs focus-out-event */
  return FALSE;
}

static GtkCellEditable *
sj_cell_renderer_text_start_editing (GtkCellRenderer      *cell,
                                     GdkEvent             *event,
                                     GtkWidget            *widget,
                                     const gchar          *path,
                                     const GdkRectangle   *background_area,
                                     const GdkRectangle   *cell_area,
                                     GtkCellRendererState  flags)
{
  SjCellRendererText *celltext;
  SjCellRendererTextPrivate *priv;
  gfloat xalign, yalign;
  GtkCellRendererMode mode;
  gchar *text = NULL;

  celltext = SJ_CELL_RENDERER_TEXT (cell);
  priv = sj_cell_renderer_text_get_instance_private (celltext);

  /* If the cell isn't editable we return NULL. */
  g_object_get (cell, "mode", &mode, NULL);
  if (mode != GTK_CELL_RENDERER_MODE_EDITABLE)
    return NULL;

  gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);

  priv->entry = gtk_entry_new ();
  g_object_ref_sink (G_OBJECT (priv->entry));

  gtk_entry_set_has_frame (GTK_ENTRY (priv->entry), FALSE);
  gtk_entry_set_alignment (GTK_ENTRY (priv->entry), xalign);

  g_object_get (cell, "text", &text, NULL);
  gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
  g_free (text);

  g_object_set_data_full (G_OBJECT (priv->entry), SJ_CELL_RENDERER_TEXT_PATH,
                          g_strdup (path), g_free);

  gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

  priv->in_entry_menu = FALSE;
  if (priv->entry_menu_popdown_timeout) {
    g_source_remove (priv->entry_menu_popdown_timeout);
    priv->entry_menu_popdown_timeout = 0;
  }

  g_signal_connect (priv->entry,
                    "editing-done",
                    G_CALLBACK (sj_cell_renderer_text_editing_done),
                    celltext);
  priv->focus_out_id = g_signal_connect_after (priv->entry, "focus-out-event",
                                               G_CALLBACK (sj_cell_renderer_text_focus_out_event),
                                               celltext);
  priv->populate_popup_id = g_signal_connect (priv->entry, "populate-popup",
                                              G_CALLBACK (sj_cell_renderer_text_populate_popup),
                                              celltext);

  gtk_widget_show (priv->entry);

  return GTK_CELL_EDITABLE (priv->entry);
}

static void
sj_cell_renderer_text_init (SjCellRendererText *sj_cell_renderer_text)
{
  /* No Op */
}

static void
sj_cell_renderer_text_finalize (GObject *object)
{
  SjCellRendererTextPrivate *priv;

  priv = sj_cell_renderer_text_get_instance_private (SJ_CELL_RENDERER_TEXT (object));
  g_clear_object (&priv->entry);
  G_OBJECT_CLASS (sj_cell_renderer_text_parent_class)->finalize (object);
}

static void
sj_cell_renderer_text_class_init (SjCellRendererTextClass *class)
{
  GObjectClass* object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->finalize = sj_cell_renderer_text_finalize;
  renderer_class->start_editing = sj_cell_renderer_text_start_editing;
}


GtkCellRenderer*
sj_cell_renderer_text_new (void)
{
  return g_object_new (SJ_TYPE_CELL_RENDERER_TEXT, NULL);
}
