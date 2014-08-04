/*
 * Copyright (C) 2014 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-tree-view.c
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

#include "sj-tree-view.h"

static GtkTreeViewClass *parent_class;

enum {
  PLAY_ROW,
  PLAY_CURSOR_ROW,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

G_DEFINE_TYPE (SjTreeView, sj_tree_view, GTK_TYPE_TREE_VIEW);

static void
sj_tree_view_play_curent_row (SjTreeView *self)
{
  GtkTreePath *path;

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (self), &path, NULL);
  if (path != NULL)
    g_signal_emit (self, signals[PLAY_ROW], 0, path);
  gtk_tree_path_free (path);
}

/**
 * Find out if the focused cell is being edited
 */
static gboolean
sj_tree_view_is_editing (GtkTreeView *self)
{
  GtkTreeViewColumn *column;
  GtkCellArea *cell_area;
  GtkCellRenderer *cell_renderer;
  gboolean editing;

  gtk_tree_view_get_cursor (self, NULL, &column);
  if (column == NULL)
    return FALSE;

  cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
  cell_renderer = gtk_cell_area_get_focus_cell (cell_area);
  if (cell_renderer == NULL)
    return FALSE;

  g_object_get (cell_renderer, "editing", &editing, NULL);
  return editing;
}

/**
 * Stop editing the focused cell if it is being edited. Ensure that
 * self has the keyboard focus whether or not we were edititng
 */
static void
sj_tree_view_stop_editing (GtkTreeView *self)
{
  GtkTreeViewColumn *column;
  GtkCellArea *cell_area;
  GtkCellRenderer *cell_renderer;
  gboolean editing;

  gtk_tree_view_get_cursor (self, NULL, &column);
  if (column == NULL)
    return;

  cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
  cell_renderer = gtk_cell_area_get_focus_cell (cell_area);
  if (cell_renderer == NULL)
    return;

  g_object_get (cell_renderer, "editing", &editing, NULL);
  if (editing)
    gtk_cell_renderer_stop_editing (cell_renderer, FALSE);

  gtk_widget_grab_focus (GTK_WIDGET (self));
}

/**
 * Start editing the focused cell if it is editable
 */
static void
sj_tree_view_start_editing (GtkTreeView *self)
{
  GtkTreeViewColumn *column;
  GtkCellArea *cell_area;
  GtkCellRenderer *cell_renderer;
  GtkCellRendererMode mode;
  gboolean ret;

  gtk_tree_view_get_cursor (self, NULL, &column);
  if (column == NULL)
    return;

  cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
  cell_renderer = gtk_cell_area_get_focus_cell (cell_area);
  if (cell_renderer == NULL)
    return;

  g_object_get (cell_renderer, "mode", &mode, NULL);
  if (mode == GTK_CELL_RENDERER_MODE_EDITABLE)
    g_signal_emit_by_name (self, "select-cursor-row", TRUE, &ret);
}

/**
 * Focus the default cell if no cell has the focus
 */
static void
sj_tree_view_focus_default_cell (GtkTreeView *self)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GtkTreeModel *model;

  gtk_tree_view_get_cursor (self, &path, &column);
  model = gtk_tree_view_get_model (self);

  if (path == NULL) {
    if (model != NULL && gtk_tree_model_get_iter_first (model, &iter))
      path = gtk_tree_model_get_path (model, &iter);
    else
      return;
  }
  if (column == NULL) {
    GList *list = gtk_tree_view_get_columns (self);
    column = list->data;
    g_list_free (list);
    list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
    renderer = list->data;
    g_list_free (list);
  } else {
    GtkCellArea *area;
    area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
    renderer = gtk_cell_area_get_focus_cell (area);
    if (renderer == NULL) {
      GList *list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
      renderer = list->data;
      g_list_free (list);
    }
  }

  gtk_tree_view_set_cursor_on_cell (self, path, column, renderer, FALSE);
  gtk_tree_path_free (path);
}

/**
 * Stop editing, move cursor up/down.
 */
static gboolean
move_vertical (GtkTreeView     *self,
               GtkMovementStep  step,
               int              count)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;

  g_return_val_if_fail (count != 0, FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_DISPLAY_LINES ||
                        step == GTK_MOVEMENT_PAGES ||
                        step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  model = gtk_tree_view_get_model (self);
  gtk_tree_view_get_cursor (self, &path, NULL);
  if (path == NULL || !gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_path_free (path);
    return FALSE;
  }
  gtk_tree_path_free (path);

  if (count < 0) {
    if (!gtk_tree_model_iter_previous (model, &iter)) {
      return FALSE; /* Do nothing if we are already at the top */
    }
  } else {
    if (!gtk_tree_model_iter_next (model, &iter)) {
      return FALSE; /* Do nothing if we are already at the bottom */
    }
  }

  /* We have to explicitly stop editing to ensure that self has the
     keyboard focus before calling parent_class->move_cursor */
  sj_tree_view_stop_editing (self);
  parent_class->move_cursor (self, step, count);
  return TRUE;
}

/**
 * Find the next activatable column in the given direction and return
 * the column index and row offset. Wrap up/down. Assume activatable
 * columns have only one renderer.
 */
static gboolean
find_next_activatable_column (GtkTreeView      *self,
                              gint             *target_column,
                              int              *move_row,
                              GtkDirectionType  direction)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkCellArea *area;
  int offset, inc;
  gboolean wrap = FALSE;
  GList *current_column, *wrap_column;
  gboolean (*move_row_func)(GtkTreeModel*, GtkTreeIter*);
  GtkTreeModel *model;
  GList *column_list;

  gtk_tree_view_get_cursor (self, &path, &column);
  model = gtk_tree_view_get_model (self);
  if (column == NULL || path == NULL ||
      !gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_path_free (path);
    return FALSE;
  }
  gtk_tree_path_free (path);

  column_list = gtk_tree_view_get_columns (self);
  current_column = g_list_find (column_list, column);
  switch (direction) {
  case GTK_DIR_TAB_FORWARD:
    wrap = TRUE;
    /* Fall through */
  case GTK_DIR_RIGHT:
    offset = G_STRUCT_OFFSET (GList, next);
    wrap_column = column_list;
    move_row_func = gtk_tree_model_iter_next;
    inc = 1;
    break;
  case GTK_DIR_TAB_BACKWARD:
    wrap = TRUE;
    /* Fall through */
  case GTK_DIR_LEFT:
    offset = G_STRUCT_OFFSET (GList, prev);
    wrap_column = g_list_last (column_list);
    move_row_func = gtk_tree_model_iter_previous;
    inc = -1;
    break;
  default:
    g_warning ("Unknown direction %d in find_next_activatable_column",
               direction);
    return FALSE;
  }

  *move_row = 0;
  do {
    current_column = G_STRUCT_MEMBER (GList*, current_column, offset);
    if (current_column == NULL) { /* End of a row */
      /* Move vertically if wrap set */
      if (wrap  && move_row_func (model, &iter)) {
        *move_row = inc;
        current_column = wrap_column;
      } else {
        g_list_free (column_list);
        return FALSE;
      }
    }
    area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (current_column->data));
  } while (!gtk_tree_view_column_get_visible (current_column->data) ||
           !gtk_cell_area_is_activatable (area));

  *target_column = g_list_index (column_list, current_column->data);

  g_list_free (column_list);
  return TRUE;
}

/**
 * Move the focus in the listview to the next activatable
 * column.
 */
static gboolean
move_horizontal (GtkTreeView      *self,
                 GtkDirectionType  direction)
{
  gint move_row, current, target;
  GtkTreeViewColumn *column;
  GList *column_list;
  GtkTextDirection text_dir = gtk_widget_get_direction (GTK_WIDGET (self));
  int dir = (text_dir == GTK_TEXT_DIR_LTR) ? 1 : -1;

  if (!find_next_activatable_column (self, &target, &move_row, direction))
    return FALSE;

  sj_tree_view_stop_editing (self);

  /* Unfortunately move-cursor sometimes takes three calls rather than
     two to move from column 2 to column 0. Occasionally it also jumps
     straight from column 3 to column 5. Because of this we have to
     check where we are and which direction to go to get there each
     time. Use GTK_MOVEMENT_VISUAL_POSITIONS and change the sign of
     direction if text direction is rtl as
     GTK_MOVEMENT_LOGICAL_POSITIONS does not work on rtl locales. */

  column_list = gtk_tree_view_get_columns (self);
  gtk_tree_view_get_cursor (self, NULL, &column);
  current = g_list_index (column_list, column);
  while (target != current) {
    parent_class->move_cursor (self,
                               GTK_MOVEMENT_VISUAL_POSITIONS,
                               dir * ((target > current) - (target < current)));
    gtk_tree_view_get_cursor (self, NULL, &column);
    current = g_list_index (column_list, column);
  }

  if (move_row) {
    parent_class->move_cursor (self,
                               GTK_MOVEMENT_DISPLAY_LINES,
                               move_row);
  }

  g_list_free (column_list);
  return TRUE;
}

static gboolean
sj_tree_view_move_cursor (GtkTreeView     *self,
                          GtkMovementStep  step,
                          gint             count)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;
  gboolean moved = FALSE, start_editing;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (self), FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
			step == GTK_MOVEMENT_VISUAL_POSITIONS ||
			step == GTK_MOVEMENT_DISPLAY_LINES ||
			step == GTK_MOVEMENT_PAGES ||
			step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  gtk_tree_view_get_cursor (self, &path, &column);
  if (path == NULL || column == NULL) {
    sj_tree_view_focus_default_cell (self);
    gtk_tree_path_free (path);
    return TRUE;
  }
  gtk_tree_path_free (path);

  start_editing = sj_tree_view_is_editing (self);

  switch (step) {
  case GTK_MOVEMENT_LOGICAL_POSITIONS:
  case GTK_MOVEMENT_VISUAL_POSITIONS:
    switch (count) {
    case -2:
      start_editing = TRUE;
      moved = move_horizontal (self, GTK_DIR_TAB_BACKWARD);
      break;
    case -1:
      moved = move_horizontal (self, GTK_DIR_LEFT);
      break;
    case 1:
      moved = move_horizontal (self, GTK_DIR_RIGHT);
      break;
    case 2:
      start_editing = TRUE;
      moved = move_horizontal (self, GTK_DIR_TAB_FORWARD);
      break;
    default:
      g_return_val_if_reached (FALSE);
    }
    break;
  case GTK_MOVEMENT_DISPLAY_LINES:
  case GTK_MOVEMENT_PAGES:
  case GTK_MOVEMENT_BUFFER_ENDS:
    moved = move_vertical (self, step, count);
    break;
  default:
    g_return_val_if_reached (FALSE);
  }

  if (!moved)
    return FALSE;

  if (start_editing)
    sj_tree_view_start_editing (self);

  return TRUE;
}

static gboolean
sj_tree_view_key_press (GtkWidget   *widget,
                        GdkEventKey *event)
{
  gboolean ret_val = FALSE;
  guint modifier = event->state & gtk_accelerator_get_default_mod_mask();
  guint key = event->keyval;

  /* Handle Tab here rather than using a keybinding signal as we need
     to fiddle the shift state to stop GtkTreeView.move_cursor()
     extending the selection when moving backwards */

  if ((modifier == GDK_SHIFT_MASK && (key == GDK_KEY_Tab ||
                                      key == GDK_KEY_KP_Tab ||
                                      key == GDK_KEY_ISO_Left_Tab ||
                                      key == GDK_KEY_3270_BackTab)) ||
      (modifier == 0 && (key == GDK_KEY_ISO_Left_Tab ||
                         key == GDK_KEY_3270_BackTab))) {
    event->state &= ~GDK_SHIFT_MASK; /* Avoid extending selection */
    ret_val = sj_tree_view_move_cursor (GTK_TREE_VIEW (widget),
                                        GTK_MOVEMENT_VISUAL_POSITIONS, -2);
    event->state |= modifier; /* Restore shift state */
  } else if (modifier == 0 && (key == GDK_KEY_Tab ||
                               key == GDK_KEY_KP_Tab)) {
    ret_val = sj_tree_view_move_cursor (GTK_TREE_VIEW (widget),
                                        GTK_MOVEMENT_VISUAL_POSITIONS, 2);
  }

  if (!ret_val)
    return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
  else
    return TRUE;
}

static void
sj_tree_view_init (SjTreeView *sj_tree_view)
{
  /* No Op */
}

static void
sj_tree_view_class_init (SjTreeViewClass *class)
{
  GtkBindingSet *binding_set;
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkTreeViewClass *tree_class = GTK_TREE_VIEW_CLASS (class);

  parent_class = GTK_TREE_VIEW_CLASS (sj_tree_view_parent_class);

  binding_set = gtk_binding_set_by_class (class);

  widget_class->key_press_event = sj_tree_view_key_press;
  tree_class->move_cursor = sj_tree_view_move_cursor;
  class->play_cursor_row = sj_tree_view_play_curent_row;

  signals[PLAY_ROW] =
    g_signal_new ("play-row",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SjTreeViewClass, play_row),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_TREE_PATH);

  signals[PLAY_CURSOR_ROW] =
    g_signal_new ("play-cursor-row",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (SjTreeViewClass, play_cursor_row),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 0);

  /* Keybindings - Ctrl-Tab moves focus, Ctrl-Spaces plays current row */

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, GDK_CONTROL_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, GDK_CONTROL_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_3270_BackTab, GDK_CONTROL_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Left_Tab, GDK_CONTROL_MASK,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK,
                                "play-cursor-row", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                "play-cursor-row", 0);
}

GtkWidget*
sj_tree_view_new (void)
{
  return g_object_new (SJ_TYPE_TREE_VIEW, NULL);
}
