/*
 * Copyright (C) 2014 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-cell-renderer-text.h
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

#ifndef _SJ_CELL_RENDERER_TEXT_H_
#define _SJ_CELL_RENDERER_TEXT_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SJ_TYPE_CELL_RENDERER_TEXT             (sj_cell_renderer_text_get_type ())
#define SJ_CELL_RENDERER_TEXT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SJ_TYPE_CELL_RENDERER_TEXT, SjCellRendererText))
#define SJ_CELL_RENDERER_TEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SJ_TYPE_CELL_RENDERER_TEXT, SjCellRendererTextClass))
#define SJ_IS_CELL_RENDERER_TEXT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SJ_TYPE_CELL_RENDERER_TEXT))
#define SJ_IS_CELL_RENDERER_TEXT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SJ_TYPE_CELL_RENDERER_TEXT))
#define SJ_CELL_RENDERER_TEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SJ_TYPE_CELL_RENDERER_TEXT, SjCellRendererTextClass))

typedef struct _SjCellRendererTextClass SjCellRendererTextClass;
typedef struct _SjCellRendererText SjCellRendererText;

struct _SjCellRendererTextClass
{
  GtkCellRendererTextClass parent_class;
};

struct _SjCellRendererText
{
  GtkCellRendererText parent_instance;
};

GType sj_cell_renderer_text_get_type (void) G_GNUC_CONST;
GtkCellRenderer *sj_cell_renderer_text_new (void);

G_END_DECLS

#endif /* _SJ_CELL_RENDERER_TEXT_H_ */
