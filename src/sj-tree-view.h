/*
 * Copyright (C) 2014 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-tree-view.h
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

#ifndef _SJ_TREE_VIEW_H_
#define _SJ_TREE_VIEW_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SJ_TYPE_TREE_VIEW             (sj_tree_view_get_type ())
#define SJ_TREE_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SJ_TYPE_TREE_VIEW, SjTreeView))
#define SJ_TREE_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SJ_TYPE_TREE_VIEW, SjTreeViewClass))
#define SJ_IS_TREE_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SJ_TYPE_TREE_VIEW))
#define SJ_IS_TREE_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SJ_TYPE_TREE_VIEW))
#define SJ_TREE_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SJ_TYPE_TREE_VIEW, SjTreeViewClass))

typedef struct _SjTreeViewClass SjTreeViewClass;
typedef struct _SjTreeView SjTreeView;

struct _SjTreeViewClass
{
  GtkTreeViewClass parent_class;
  void             (*play_row)(SjTreeView*, GtkTreePath*);
  void             (*play_cursor_row)(SjTreeView*);
};

struct _SjTreeView
{
  GtkTreeView parent_instance;
};

GType sj_tree_view_get_type (void) G_GNUC_CONST;
GtkWidget *sj_tree_view_new (void);

G_END_DECLS

#endif /* _SJ_TREE_VIEW_H_ */
