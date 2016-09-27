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

#include "ephy-bookmarks.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-header-bar.h"
#include "ephy-link.h"
#include "ephy-node-common.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"

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

void
ephy_bookmarks_ui_attach_window (EphyWindow *window)
{
  EphyBookmarks *eb;
  BookmarksWindowData *data;
  GtkUIManager *manager;
  GtkActionGroup *actions;

  eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());
  data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);
  g_return_if_fail (data == NULL);

  data = g_new0 (BookmarksWindowData, 1);
  g_object_set_data_full (G_OBJECT (window), BM_WINDOW_DATA_KEY, data, g_free);
}

void
ephy_bookmarks_ui_detach_window (EphyWindow *window)
{
  BookmarksWindowData *data = g_object_get_data (G_OBJECT (window), BM_WINDOW_DATA_KEY);

  g_return_if_fail (data != 0);

  g_object_set_data (G_OBJECT (window), BM_WINDOW_DATA_KEY, 0);
}

void
ephy_bookmarks_ui_add_bookmark (GtkWindow  *parent,
                                const char *location,
                                const char *title)
{
  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING))
    return;
}

void
ephy_bookmarks_ui_show_bookmark (GtkWindow *parent, EphyNode *bookmark)
{
  EphyBookmarks *bookmarks;

  bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

  g_return_if_fail (EPHY_IS_BOOKMARKS (bookmarks));
  g_return_if_fail (EPHY_IS_NODE (bookmark));
}
