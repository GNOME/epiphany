/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2005 Peter Harvey
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-bookmarks-ui.h"

#include "ephy-bookmark-action-group.h"
#include "ephy-bookmark-action.h"
#include "ephy-bookmark-properties.h"
#include "ephy-bookmarks-menu.h"
#include "ephy-bookmarks.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-link.h"
#include "ephy-node-common.h"
#include "ephy-open-tabs-action.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-toolbar.h"
#include "ephy-topic-action-group.h"
#include "ephy-topic-action.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define BM_WINDOW_DATA_KEY "bookmarks-window-data"

typedef struct {
  GMenu *bookmarks_menu;
  guint toolbar_menu;
} BookmarksWindowData;

enum {
  RESPONSE_SHOW_PROPERTIES = 1,
  RESPONSE_NEW_BOOKMARK = 2
};

static GHashTable *properties_dialogs = 0;

static GMenu *
find_bookmarks_menu (EphyWindow *window)
{
  GtkWidget *page_menu_button;
  GMenuModel *page_menu;
  gint n_items, i;

  /* Page menu */
  page_menu_button = ephy_toolbar_get_page_menu_button (EPHY_TOOLBAR (ephy_window_get_toolbar (window)));
  page_menu = gtk_menu_button_get_menu_model (GTK_MENU_BUTTON (page_menu_button));

  /* Number of sections in the model */
  n_items = g_menu_model_get_n_items (page_menu);

  for (i = 0; i < n_items; i++) {
    GVariant *section_label;

    /* Looking for the bookmarks section */
    section_label = g_menu_model_get_item_attribute_value (page_menu, i, "id", G_VARIANT_TYPE_STRING);
    if (section_label != NULL && g_strcmp0 (g_variant_get_string (section_label, NULL), "bookmarks-section") == 0) {
      GMenuModel *bookmarks_section_model;
      GMenuModel *bookmarks_menu_model;

      /* Bookmarks section should contain the bookmarks menu */
      bookmarks_section_model = g_menu_model_get_item_link (page_menu, i, G_MENU_LINK_SECTION);
      bookmarks_menu_model = g_menu_model_get_item_link (bookmarks_section_model, 0, G_MENU_LINK_SUBMENU);

      return G_MENU (bookmarks_menu_model);
    }
  }

  return NULL;
}

static bool
activate_bookmarks_menu (GSimpleAction *action,
                         GdkEvent      *event,
                         gpointer       user_data)
{
  GMenu *menu;
  BookmarksWindowData *data = g_object_get_data (G_OBJECT (user_data), BM_WINDOW_DATA_KEY);

  if (event->type != GDK_BUTTON_PRESS)
    return G_SOURCE_REMOVE;

  if (data && !data->bookmarks_menu) {
    menu = g_menu_new ();
    ephy_bookmarks_menu_build (menu, 0);

    data->bookmarks_menu = G_MENU (find_bookmarks_menu (EPHY_WINDOW (user_data)));
    if (data->bookmarks_menu == NULL)
      return G_SOURCE_REMOVE;

    g_menu_append_section (data->bookmarks_menu, NULL, G_MENU_MODEL (menu));
  }

  return G_SOURCE_REMOVE;
}

static void
erase_bookmarks_menu (EphyWindow *window)
{
  BookmarksWindowData *data;

  data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);

  if (data != NULL && data->bookmarks_menu != NULL) {
    g_menu_remove_all (data->bookmarks_menu);
    g_clear_object (&data->bookmarks_menu);
  }
}

static void
tree_changed_cb (EphyBookmarks *bookmarks,
                 EphyWindow    *window)
{
  erase_bookmarks_menu (window);
}

static void
node_added_cb (EphyNode   *parent,
               EphyNode   *child,
               EphyWindow *window)
{
  erase_bookmarks_menu (window);
}

static void
node_changed_cb (EphyNode   *parent,
                 EphyNode   *child,
                 guint       property_id,
                 EphyWindow *window)
{
  if (property_id == EPHY_NODE_KEYWORD_PROP_NAME ||
      property_id == EPHY_NODE_BMK_PROP_TITLE) {
    erase_bookmarks_menu (window);
  }
}

static void
node_removed_cb (EphyNode   *parent,
                 EphyNode   *child,
                 guint       index,
                 EphyWindow *window)
{
  erase_bookmarks_menu (window);
}

void
ephy_bookmarks_ui_attach_window (EphyWindow *window)
{
  EphyBookmarks *eb;
  EphyNode *bookmarks;
  EphyNode *topics;
  BookmarksWindowData *data;
  GtkUIManager *manager;
  GtkActionGroup *actions;
  GtkWidget *page_menu_button;

  eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());
  bookmarks = ephy_bookmarks_get_bookmarks (eb);
  topics = ephy_bookmarks_get_keywords (eb);
  data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
  g_return_if_fail (data == NULL);

  manager = ephy_window_get_ui_manager (window);

  data = g_new0 (BookmarksWindowData, 1);
  g_object_set_data_full (G_OBJECT (window), BM_WINDOW_DATA_KEY, data, g_free);

  /* Create the self-maintaining action groups for bookmarks and topics */
  actions = ephy_bookmark_group_new (bookmarks);
  gtk_ui_manager_insert_action_group (manager, actions, -1);
  g_signal_connect_object (actions, "open-link",
                           G_CALLBACK (ephy_link_open), G_OBJECT (window),
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  g_object_unref (actions);

  actions = ephy_topic_action_group_new (topics, manager);
  gtk_ui_manager_insert_action_group (manager, actions, -1);
  g_object_unref (actions);

  actions = ephy_open_tabs_group_new (topics);
  gtk_ui_manager_insert_action_group (manager, actions, -1);
  g_signal_connect_object (actions, "open-link",
                           G_CALLBACK (ephy_link_open), G_OBJECT (window),
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  g_object_unref (actions);

  /* Add signal handlers for the bookmark database */
  ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_ADDED,
                                   (EphyNodeCallback)node_added_cb,
                                   G_OBJECT (window));
  ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_ADDED,
                                   (EphyNodeCallback)node_added_cb,
                                   G_OBJECT (window));

  ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
                                   (EphyNodeCallback)node_removed_cb,
                                   G_OBJECT (window));
  ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_REMOVED,
                                   (EphyNodeCallback)node_removed_cb,
                                   G_OBJECT (window));

  ephy_node_signal_connect_object (bookmarks, EPHY_NODE_CHILD_CHANGED,
                                   (EphyNodeCallback)node_changed_cb,
                                   G_OBJECT (window));
  ephy_node_signal_connect_object (topics, EPHY_NODE_CHILD_CHANGED,
                                   (EphyNodeCallback)node_changed_cb,
                                   G_OBJECT (window));

  g_signal_connect_object (eb, "tree_changed",
                           G_CALLBACK (tree_changed_cb),
                           G_OBJECT (window), 0);

  page_menu_button = ephy_toolbar_get_page_menu_button (EPHY_TOOLBAR (ephy_window_get_toolbar (window)));
  g_signal_connect (GTK_WIDGET (page_menu_button), "button-press-event", G_CALLBACK (activate_bookmarks_menu), window);
}

void
ephy_bookmarks_ui_detach_window (EphyWindow *window)
{
  EphyBookmarks *eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());
  EphyNode *bookmarks = ephy_bookmarks_get_bookmarks (eb);
  EphyNode *topics = ephy_bookmarks_get_keywords (eb);

  BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
  GtkWidget *page_menu_button;

  g_return_if_fail (data != 0);

  if (data->bookmarks_menu) {
    g_menu_remove_all (data->bookmarks_menu);
    g_object_unref (data->bookmarks_menu);
    data->bookmarks_menu = NULL;
  }

  g_object_set_data (G_OBJECT (window), BM_WINDOW_DATA_KEY, 0);

  ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_ADDED,
                                      (EphyNodeCallback)node_added_cb,
                                      G_OBJECT (window));
  ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_ADDED,
                                      (EphyNodeCallback)node_added_cb,
                                      G_OBJECT (window));

  ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_REMOVED,
                                      (EphyNodeCallback)node_removed_cb,
                                      G_OBJECT (window));
  ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_REMOVED,
                                      (EphyNodeCallback)node_removed_cb,
                                      G_OBJECT (window));

  ephy_node_signal_disconnect_object (bookmarks, EPHY_NODE_CHILD_CHANGED,
                                      (EphyNodeCallback)node_changed_cb,
                                      G_OBJECT (window));
  ephy_node_signal_disconnect_object (topics, EPHY_NODE_CHILD_CHANGED,
                                      (EphyNodeCallback)node_changed_cb,
                                      G_OBJECT (window));

  g_signal_handlers_disconnect_by_func
    (G_OBJECT (eb), G_CALLBACK (tree_changed_cb), G_OBJECT (window));

  page_menu_button = ephy_toolbar_get_page_menu_button (EPHY_TOOLBAR (ephy_window_get_toolbar (window)));
  g_signal_handlers_disconnect_by_func (GTK_WIDGET (page_menu_button), G_CALLBACK (activate_bookmarks_menu), window);
}

static void
properties_dialog_destroy_cb (EphyBookmarkProperties *dialog,
                              EphyNode               *bookmark)
{
  g_hash_table_remove (properties_dialogs, bookmark);
}

void
ephy_bookmarks_ui_add_bookmark (GtkWindow  *parent,
                                const char *location,
                                const char *title)
{
  EphyBookmarks *bookmarks;
  EphyNode *bookmark;
  GtkWidget *dialog;

  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING))
    return;

  bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
  bookmark = ephy_bookmarks_add (bookmarks, title, location);

  if (properties_dialogs == 0) {
    properties_dialogs = g_hash_table_new (g_direct_hash, g_direct_equal);
  }

  dialog = ephy_bookmark_properties_new (bookmarks, bookmark, TRUE);

  g_assert (parent != NULL);

  gtk_window_group_add_window (ephy_gui_ensure_window_group (parent),
                               GTK_WINDOW (dialog));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (properties_dialog_destroy_cb), bookmark);
  g_hash_table_insert (properties_dialogs,
                       bookmark, dialog);

  gtk_window_present_with_time (GTK_WINDOW (dialog),
                                gtk_get_current_event_time ());
}

void
ephy_bookmarks_ui_show_bookmark (GtkWindow *parent, EphyNode *bookmark)
{
  EphyBookmarks *bookmarks;
  GtkWidget *dialog;

  bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

  g_return_if_fail (EPHY_IS_BOOKMARKS (bookmarks));
  g_return_if_fail (EPHY_IS_NODE (bookmark));

  if (properties_dialogs == 0) {
    properties_dialogs = g_hash_table_new (g_direct_hash, g_direct_equal);
  }

  dialog = g_hash_table_lookup (properties_dialogs, bookmark);

  if (dialog == NULL) {
    dialog = ephy_bookmark_properties_new (bookmarks, bookmark, FALSE);

    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (properties_dialog_destroy_cb), bookmark);
    g_hash_table_insert (properties_dialogs,
                         bookmark, dialog);
  }

  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  gtk_window_present_with_time (GTK_WINDOW (dialog),
                                gtk_get_current_event_time ());
}
