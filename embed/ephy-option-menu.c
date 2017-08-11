/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-option-menu.h"

#include "ephy-debug.h"

struct _EphyOptionMenu {
  GtkWindow parent;

  EphyWebView *view;
  WebKitOptionMenu *menu;

  GtkWidget *tree_view;
  GtkTreePath *selected_path;

  GdkDevice *device;

  /* Typeahead find */
  GString *search_string;
  gunichar repeating_char;
  guint32 previous_event_time;
  gint search_index;
};

G_DEFINE_TYPE (EphyOptionMenu, ephy_option_menu, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_VIEW,
  PROP_MENU,
  LAST_PROP
};

enum {
  COLUMN_LABEL,
  COLUMN_TOOLTIP,
  COLUMN_IS_GROUP,
  COLUMN_IS_SELECTED,
  COLUMN_IS_ENABLED,
  COLUMN_INDEX,

  N_COLUMNS
};

static GParamSpec *obj_properties[LAST_PROP];

#define TYPEAHEAD_TIMOUT_MS 1000

static gboolean
ephy_option_menu_activate_item_at_path (EphyOptionMenu *menu,
                                        GtkTreePath    *path)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean enabled, is_group;
  guint index;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (menu->tree_view));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      COLUMN_IS_GROUP, &is_group,
                      COLUMN_IS_ENABLED, &enabled,
                      COLUMN_INDEX, &index,
                      -1);
  if (is_group || !enabled)
    return FALSE;

  webkit_option_menu_activate_item (menu->menu, index);
  ephy_option_menu_popdown (menu);

  return TRUE;
}

static void
tree_view_row_activated_cb (GtkTreeView       *tree_view,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            EphyOptionMenu    *menu)
{
  ephy_option_menu_activate_item_at_path (menu, path);
}

static gboolean
tree_view_button_release_event (GtkWidget      *tree_view,
                                GdkEventButton *event,
                                EphyOptionMenu *menu)
{
  GtkTreePath *path;
  gboolean retval;

  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (menu->tree_view), event->x, event->y, &path, NULL, NULL, NULL))
    return FALSE;

  retval = ephy_option_menu_activate_item_at_path (menu, path);
  gtk_tree_path_free (path);

  return retval;
}

static gboolean
select_item (GtkTreeSelection *selection,
             GtkTreeModel     *model,
             GtkTreePath      *path,
             gboolean          path_currently_selected,
             gpointer          user_data)
{
  GtkTreeIter iter;
  gboolean enabled, is_group;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      COLUMN_IS_GROUP, &is_group,
                      COLUMN_IS_ENABLED, &enabled,
                      -1);
  return !is_group && enabled;
}

static void
cell_data_func (GtkTreeViewColumn *column,
                GtkCellRenderer   *renderer,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           user_data)
{
  gchar *text;
  gboolean enabled, is_group;

  gtk_tree_model_get (model, iter,
                      COLUMN_LABEL, &text,
                      COLUMN_IS_GROUP, &is_group,
                      COLUMN_IS_ENABLED, &enabled,
                      -1);
  if (is_group) {
    gchar *markup;

    markup = g_strdup_printf ("<b>%s</b>", text);
    g_object_set (renderer, "markup", markup, NULL);
    g_free (markup);
  } else {
    g_object_set (renderer, "text", text, "sensitive", enabled, NULL);
  }

  g_free (text);
}

static void
ephy_option_menu_dispose (GObject *object)
{
  EphyOptionMenu *menu = EPHY_OPTION_MENU (object);

  if (menu->menu) {
    g_signal_handlers_disconnect_matched (menu->menu, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, menu);
    webkit_option_menu_close (menu->menu);
    g_object_unref (menu->menu);
    menu->menu = NULL;
  }

  ephy_option_menu_popdown (menu);

  g_clear_pointer (&menu->selected_path, gtk_tree_path_free);

  if (menu->search_string) {
    g_string_free (menu->search_string, TRUE);
    menu->search_string = NULL;
  }

  G_OBJECT_CLASS (ephy_option_menu_parent_class)->dispose (object);
}

static void
ephy_option_menu_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyOptionMenu *menu = EPHY_OPTION_MENU (object);

  switch (prop_id) {
  case PROP_VIEW:
    g_value_set_object (value, menu->view);
    break;
  case PROP_MENU:
    g_value_set_object (value, menu->menu);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_option_menu_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyOptionMenu *menu = EPHY_OPTION_MENU (object);

  switch (prop_id) {
  case PROP_VIEW:
    menu->view = g_value_get_object (value);
    break;
  case PROP_MENU:
    menu->menu = g_value_dup_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_option_menu_constructed (GObject *object)
{
  EphyOptionMenu *menu = EPHY_OPTION_MENU (object);
  GtkTreeView *tree_view;
  GtkTreeStore *model;
  GtkTreeIter parentIter;
  GtkTreeSelection *selection;
  GtkWidget *swindow;
  guint i, n_items;

  g_signal_connect_swapped (menu->menu, "close", G_CALLBACK (gtk_widget_destroy), menu);

  model = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_UINT);
  n_items = webkit_option_menu_get_n_items (menu->menu);

  for (i = 0; i < n_items; ++i) {
    WebKitOptionMenuItem *item = webkit_option_menu_get_item (menu->menu, i);

    if (webkit_option_menu_item_is_group_label (item)) {
      gtk_tree_store_insert_with_values (model, &parentIter, NULL, -1,
                                         COLUMN_LABEL, webkit_option_menu_item_get_label (item),
                                         COLUMN_IS_GROUP, TRUE,
                                         COLUMN_IS_ENABLED, TRUE,
                                         -1);
    } else {
      GtkTreeIter iter;

      gtk_tree_store_insert_with_values (model, &iter,
                                         webkit_option_menu_item_is_group_child (item) ? &parentIter : NULL, -1,
                                         COLUMN_LABEL, webkit_option_menu_item_get_label (item),
                                         COLUMN_TOOLTIP, webkit_option_menu_item_get_tooltip (item),
                                         COLUMN_IS_GROUP, FALSE,
                                         COLUMN_IS_SELECTED, webkit_option_menu_item_is_selected (item),
                                         COLUMN_IS_ENABLED, webkit_option_menu_item_is_enabled (item),
                                         COLUMN_INDEX, i,
                                         -1);
      if (webkit_option_menu_item_is_selected (item)) {
        g_assert (menu->selected_path == NULL);
        menu->selected_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      }
    }
  }

  menu->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);

  tree_view = GTK_TREE_VIEW (menu->tree_view);
  g_signal_connect (tree_view, "row-activated",
                    G_CALLBACK (tree_view_row_activated_cb),
                    menu);
  g_signal_connect_after (tree_view, "button-release-event",
                          G_CALLBACK (tree_view_button_release_event),
                          menu);
  gtk_tree_view_set_tooltip_column (tree_view, COLUMN_TOOLTIP);
  gtk_tree_view_set_show_expanders (tree_view, FALSE);
  gtk_tree_view_set_level_indentation (tree_view, 12);
  gtk_tree_view_set_enable_search(tree_view, FALSE);
  gtk_tree_view_set_activate_on_single_click (tree_view, TRUE);
  gtk_tree_view_set_hover_selection (tree_view, TRUE);
  gtk_tree_view_set_headers_visible (tree_view, FALSE);
  gtk_tree_view_insert_column_with_data_func (tree_view, 0, NULL,
                                              gtk_cell_renderer_text_new (),
                                              cell_data_func, menu, NULL);
  gtk_tree_view_expand_all (tree_view);

  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_set_select_function (selection, select_item, NULL, NULL);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (swindow), menu->tree_view);
  gtk_widget_show (menu->tree_view);

  gtk_container_add (GTK_CONTAINER (menu), swindow);
  gtk_widget_show (swindow);
}

static gboolean
ephy_option_menu_button_press_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  EphyOptionMenu *menu = EPHY_OPTION_MENU (widget);

  if (!menu->device)
    return FALSE;

  ephy_option_menu_popdown (menu);

  return TRUE;
}

static gint
ephy_option_menu_typeahead_find (EphyOptionMenu *menu,
                                 GdkEventKey    *event)
{
  gunichar keychar;
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint selected_index = 0;
  guint index, i, n_items;
  gchar *normalized_prefix, *prefix;
  gint prefix_len = -1;
  gint retval = -1;

  keychar = gdk_keyval_to_unicode (event->keyval);
  if (!g_unichar_isprint (keychar))
    return retval;

  if (event->time < menu->previous_event_time)
    return retval;

  if (event->time - menu->previous_event_time > TYPEAHEAD_TIMOUT_MS) {
    if (menu->search_string)
      g_string_truncate (menu->search_string, 0);
  }
  menu->previous_event_time = event->time;

  if (!menu->search_string)
    menu->search_string = g_string_new (NULL);
  g_string_append_unichar (menu->search_string, keychar);

  if (keychar == menu->repeating_char)
    prefix_len = 1;
  else
    menu->repeating_char = menu->search_string->len == 1 ? keychar : '\0';

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (menu->tree_view)), &model, &iter))
    gtk_tree_model_get (model, &iter, COLUMN_INDEX, &selected_index, -1);

  index = selected_index;
  if (menu->repeating_char != '\0')
    index++;

  n_items = webkit_option_menu_get_n_items (menu->menu);
  index %= n_items;

  normalized_prefix = g_utf8_normalize (menu->search_string->str, prefix_len, G_NORMALIZE_ALL);
  prefix = normalized_prefix ? g_utf8_casefold (normalized_prefix, -1) : NULL;
  g_free (normalized_prefix);
  if (!prefix)
    return retval;

  for (i = 0; i < n_items; i++, index = (index + 1) % n_items) {
    gchar *normalized_text, *text;
    WebKitOptionMenuItem *item = webkit_option_menu_get_item (menu->menu, index);

    if (webkit_option_menu_item_is_group_label (item) || !webkit_option_menu_item_is_enabled (item))
      continue;

    normalized_text = g_utf8_normalize (webkit_option_menu_item_get_label (item), -1, G_NORMALIZE_ALL);
    text = normalized_text ? g_utf8_casefold (normalized_text, -1) : NULL;
    g_free (normalized_text);
    if (!text)
      continue;

    if (strncmp (prefix, text, strlen (prefix)) == 0) {
      retval = index;
      g_free (text);
      break;
    }

    g_free (text);
  }

  g_free (prefix);

  return retval;
}

static gboolean
select_search_index (GtkTreeModel   *model,
                     GtkTreePath    *path,
                     GtkTreeIter    *iter,
                     EphyOptionMenu *menu)
{
  guint index;

  gtk_tree_model_get (model, iter, COLUMN_INDEX, &index, -1);
  if (index == menu->search_index) {
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (menu->tree_view),
                                  path, NULL, TRUE, 1, 0);
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (menu->tree_view),
                              path, NULL, FALSE);

    webkit_option_menu_select_item (menu->menu, menu->search_index);

    return TRUE;
  }

  return FALSE;
}

static gboolean
ephy_option_menu_key_press_event (GtkWidget   *widget,
                                  GdkEventKey *event)
{
  EphyOptionMenu *menu = EPHY_OPTION_MENU (widget);

  if (!menu->device)
    return FALSE;

  if (event->keyval == GDK_KEY_Escape) {
    ephy_option_menu_popdown (menu);
    return TRUE;
  }

  if (event->state == 0) {
    menu->search_index = ephy_option_menu_typeahead_find (menu, event);
    if (menu->search_index >= 0) {
      gtk_tree_model_foreach (gtk_tree_view_get_model (GTK_TREE_VIEW (menu->tree_view)),
                              (GtkTreeModelForeachFunc)select_search_index,
                              menu);
      return TRUE;
    }
  }

  /* Forward the event to the tree view */
  gtk_widget_event (menu->tree_view, (GdkEvent *)event);

  return TRUE;
}

static void
ephy_option_menu_class_init (EphyOptionMenuClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructed = ephy_option_menu_constructed;
  gobject_class->dispose = ephy_option_menu_dispose;
  gobject_class->get_property = ephy_option_menu_get_property;
  gobject_class->set_property = ephy_option_menu_set_property;

  widget_class->button_press_event = ephy_option_menu_button_press_event;
  widget_class->key_press_event = ephy_option_menu_key_press_event;

  obj_properties[PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The option menu's associated view",
                         EPHY_TYPE_WEB_VIEW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_MENU] =
    g_param_spec_object ("menu",
                         "Menu",
                         "The menu items to display",
                         WEBKIT_TYPE_OPTION_MENU,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);
}

static void
ephy_option_menu_init (EphyOptionMenu *menu)
{
}

GtkWidget *
ephy_option_menu_new (EphyWebView      *view,
                      WebKitOptionMenu *menu)
{
  g_assert (EPHY_IS_WEB_VIEW (view));
  g_assert (WEBKIT_IS_OPTION_MENU (menu));

  return g_object_new (EPHY_TYPE_OPTION_MENU,
                       "view", view,
                       "menu", menu,
                       "type", GTK_WINDOW_POPUP,
                       "type-hint", GDK_WINDOW_TYPE_HINT_COMBO,
                       "resizable", FALSE,
                       NULL);
}

static void
prepare_menu_func (GdkSeat        *seat,
                   GdkWindow      *window,
                   EphyOptionMenu *menu)
{
  if (menu->selected_path) {
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (menu->tree_view),
                                  menu->selected_path, NULL,
                                  TRUE, 0.5, 0);
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (menu->tree_view),
                              menu->selected_path, NULL, FALSE);
  }
  gtk_widget_grab_focus (menu->tree_view);
  gtk_widget_show (GTK_WIDGET (menu));
}

void
ephy_option_menu_popup (EphyOptionMenu *menu,
                        GdkEvent       *event,
                        GdkRectangle   *rect)
{
  GdkWindow *window;
  gint x, y;
  gint width, height;
  guint n_menu_items;
  gint n_items;
  gint vertical_separator;
  GtkRequisition tree_view_req;
  GtkRequisition menu_req;
  GtkTreeViewColumn *column;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle area;
  GtkWidget *toplevel;
  GtkScrolledWindow *swindow;

  g_assert (EPHY_IS_OPTION_MENU (menu));
  g_assert (rect != NULL);

  window = gtk_widget_get_window (GTK_WIDGET (menu->view));
  gdk_window_get_origin (window, &x, &y);
  x += rect->x;
  y += rect->y;

  gtk_widget_get_preferred_size (menu->tree_view, &tree_view_req, NULL);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (menu->tree_view), 0);
  gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &height);
  gtk_widget_style_get (menu->tree_view, "vertical-separator", &vertical_separator, NULL);
  height += vertical_separator;
  if (height <= 0)
    return;

  display = gtk_widget_get_display (GTK_WIDGET (menu->view));
  monitor = gdk_display_get_monitor_at_window (display, window);
  gdk_monitor_get_workarea (monitor, &area);
  width = MIN (rect->width, area.width);
  n_menu_items = webkit_option_menu_get_n_items (menu->menu);
  n_items = MIN (n_menu_items, (area.height / 3) / height);

  swindow = GTK_SCROLLED_WINDOW (gtk_bin_get_child (GTK_BIN (menu)));
  /* Disable scrollbars when there's only one item to ensure the scrolled window
   * doesn't take them into account when calculating its minimum size.
   */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_NEVER,
                                  n_items > 1 ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
  gtk_widget_realize (menu->tree_view);
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (menu->tree_view));
  gtk_scrolled_window_set_min_content_width (swindow, width);
  gtk_widget_set_size_request (GTK_WIDGET (menu), width, -1);
  gtk_scrolled_window_set_min_content_height (swindow, n_items * height);

  gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_req, NULL);
  if (x < area.x)
    x = area.x;
  else if (x + menu_req.width > area.x + area.width)
    x = area.x + area.width - menu_req.width;

  if (y + rect->height + menu_req.height <= area.y + area.height ||
      y - area.y < (area.y + area.height) - (y + rect->height)) {
    y += rect->height;
  } else {
    y -= menu_req.height;
  }
  gtk_window_move (GTK_WINDOW (menu), x, y);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu->view));
  if (GTK_IS_WINDOW (toplevel)) {
    gtk_window_set_transient_for (GTK_WINDOW (menu), GTK_WINDOW (toplevel));
    gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                 GTK_WINDOW (menu));
  }
  gtk_window_set_attached_to (GTK_WINDOW (menu), GTK_WIDGET (menu->view));
  gtk_window_set_screen (GTK_WINDOW (menu), gtk_widget_get_screen (GTK_WIDGET (menu->view)));

  menu->device = event ? gdk_event_get_device (event) : NULL;
  if (!menu->device)
    menu->device = gtk_get_current_event_device ();
  if (menu->device && gdk_device_get_display (menu->device) != display)
    menu->device = NULL;
  if (!menu->device)
    menu->device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
  g_assert (menu->device != NULL);
  if (gdk_device_get_source (menu->device) == GDK_SOURCE_KEYBOARD)
    menu->device = gdk_device_get_associated_device (menu->device);

  gtk_grab_add (GTK_WIDGET (menu));
  gdk_seat_grab (gdk_device_get_seat (menu->device),
                 gtk_widget_get_window (GTK_WIDGET (menu)),
                 GDK_SEAT_CAPABILITY_ALL,
                 TRUE, NULL, NULL,
                 (GdkSeatGrabPrepareFunc)prepare_menu_func,
                 menu);
}

void
ephy_option_menu_popdown (EphyOptionMenu *menu)
{
  g_assert (EPHY_IS_OPTION_MENU (menu));

  if (!menu->device)
    return;

  gdk_seat_ungrab (gdk_device_get_seat (menu->device));
  gtk_grab_remove (GTK_WIDGET (menu));
  gtk_window_set_transient_for (GTK_WINDOW (menu), NULL);
  gtk_window_set_attached_to (GTK_WINDOW (menu), NULL);
  menu->device = NULL;

  if (menu->menu)
    webkit_option_menu_close (menu->menu);
}
