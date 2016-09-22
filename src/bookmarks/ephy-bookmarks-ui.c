/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "ephy-bookmarks.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-header-bar.h"
#include "ephy-link.h"
#include "ephy-node-common.h"
#include "ephy-open-tabs-action.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-topic-action-group.h"
#include "ephy-topic-action.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define BM_WINDOW_DATA_KEY "bookmarks-window-data"

typedef struct {
  guint toolbar_menu;
} BookmarksWindowData;

enum {
  RESPONSE_SHOW_PROPERTIES = 1,
  RESPONSE_NEW_BOOKMARK = 2
};

static GHashTable *properties_dialogs = 0;

void
ephy_bookmarks_ui_attach_window (EphyWindow *window)
{
  EphyBookmarks *eb;
  EphyNode *bookmarks;
  EphyNode *topics;
  BookmarksWindowData *data;
  GtkUIManager *manager;
  GtkActionGroup *actions;

  eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());
  bookmarks = ephy_bookmarks_get_bookmarks (eb);
  topics = ephy_bookmarks_get_keywords (eb);
  data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
  g_return_if_fail (data == NULL);

  manager = gtk_ui_manager_new ();

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
}

void
ephy_bookmarks_ui_detach_window (EphyWindow *window)
{
  BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);

  g_return_if_fail (data != 0);

  g_object_set_data (G_OBJECT (window), BM_WINDOW_DATA_KEY, 0);
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
