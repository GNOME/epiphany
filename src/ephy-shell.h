/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2006 Christian Persch
 *  Copyright © 2011 Igalia S.L.
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

#ifndef EPHY_SHELL_H
#define EPHY_SHELL_H

#include "ephy-bookmarks.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-embed-shell.h"
#include "ephy-embed.h"
#include "ephy-session.h"
#include "ephy-window.h"

#include <webkit2/webkit2.h>
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SHELL (ephy_shell_get_type ())

G_DECLARE_FINAL_TYPE (EphyShell, ephy_shell, EPHY, SHELL, EphyEmbedShell)

/**
 * EphyNewTabFlags:
 * @EPHY_NEW_TAB_DONT_SHOW_WINDOW: do not show the window where the new
 *        tab is attached.
 * @EPHY_NEW_TAB_APPEND_LAST: appends the new tab at the end of the
 *        notebook.
 * @EPHY_NEW_TAB_APPEND_AFTER: appends the new tab right after the
 *        current one in the notebook.
 * @EPHY_NEW_TAB_FROM_EXTERNAL: tries to open the new tab in the current
 *        active tab if it is currently not loading anything and is
 *        blank.
 *
 * Controls how new tabs/windows are created and handled.
 */
typedef enum {
  /* Page mode */
  EPHY_NEW_TAB_DONT_SHOW_WINDOW = 1 << 0,

  /* Tabs */
  EPHY_NEW_TAB_FIRST        = 1 << 1,
  EPHY_NEW_TAB_APPEND_LAST  = 1 << 2,
  EPHY_NEW_TAB_APPEND_AFTER = 1 << 3,
  EPHY_NEW_TAB_JUMP   = 1 << 4,
} EphyNewTabFlags;

typedef enum {
  EPHY_STARTUP_NEW_TAB          = 1 << 0,
  EPHY_STARTUP_NEW_WINDOW       = 1 << 1,
} EphyStartupFlags;

typedef struct {
  EphyStartupFlags startup_flags;
  
  char *bookmarks_filename;
  char *session_filename;
  char *bookmark_url;
  
  char **arguments;
  
  guint32 user_time;
} EphyShellStartupContext;

EphyShell      *ephy_shell_get_default                  (void);

EphyEmbed      *ephy_shell_new_tab                      (EphyShell *shell,
                                                         EphyWindow *parent_window,
                                                         EphyEmbed *previous_embed,
                                                         EphyNewTabFlags flags);

EphyEmbed      *ephy_shell_new_tab_full                 (EphyShell *shell,
                                                         const char *title,
                                                         WebKitWebView *related_view,
                                                         EphyWindow *parent_window,
                                                         EphyEmbed *previous_embed,
                                                         EphyNewTabFlags flags,
                                                         guint32 user_time);

EphySession     *ephy_shell_get_session                  (EphyShell *shell);

GNetworkMonitor *ephy_shell_get_net_monitor              (EphyShell *shell);

EphyBookmarks   *ephy_shell_get_bookmarks                (EphyShell *shell);

GtkWidget       *ephy_shell_get_bookmarks_editor         (EphyShell *shell);

EphyBookmarksManager *ephy_shell_get_bookmarks_manager   (EphyShell *shell);

GtkWidget       *ephy_shell_get_history_window           (EphyShell *shell);

GObject         *ephy_shell_get_prefs_dialog             (EphyShell *shell);

guint           ephy_shell_get_n_windows                (EphyShell *shell);

gboolean        ephy_shell_close_all_windows            (EphyShell *shell);

void            ephy_shell_open_uris                    (EphyShell *shell,
                                                         const char **uris,
                                                         EphyStartupFlags startup_flags,
                                                         guint32 user_time);
G_END_DECLS

#endif
