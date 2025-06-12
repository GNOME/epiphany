/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2006 Christian Persch
 *  Copyright © 2011 Igalia S.L.
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

#pragma once

#include "ephy-bookmarks-manager.h"
#include "ephy-embed-shell.h"
#include "ephy-embed.h"
#include "ephy-history-manager.h"
#include "ephy-open-tabs-manager.h"
#include "ephy-password-manager.h"
#include "ephy-session.h"
#include "ephy-sync-service.h"
#include "ephy-web-extension-manager.h"
#include "ephy-web-app-utils.h"
#include "ephy-window.h"

#include <webkit/webkit.h>
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
 *        tab bar.
 * @EPHY_NEW_TAB_APPEND_AFTER: appends the new tab right after the
 *        current one in the tab bar.
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
  EPHY_STARTUP_NEW_TAB,
  EPHY_STARTUP_NEW_WINDOW
} EphyStartupMode;

typedef struct {
  EphyStartupMode startup_mode;
  char *session_filename;
  char **arguments;
} EphyShellStartupContext;

EphyShell               *ephy_shell_get_default             (void);

EphyEmbed               *ephy_shell_new_tab                 (EphyShell        *shell,
                                                             EphyWindow       *parent_window,
                                                             EphyEmbed        *previous_embed,
                                                             EphyNewTabFlags   flags);

EphyEmbed               *ephy_shell_new_tab_full            (EphyShell        *shell,
                                                             const char       *title,
                                                             WebKitWebView    *related_view,
                                                             EphyWindow       *parent_window,
                                                             EphyEmbed        *previous_embed,
                                                             EphyNewTabFlags   flags);

EphySession             *ephy_shell_get_session             (EphyShell        *shell);
GNetworkMonitor         *ephy_shell_get_net_monitor         (EphyShell        *shell);
EphyBookmarksManager    *ephy_shell_get_bookmarks_manager   (EphyShell        *shell);
EphyHistoryManager      *ephy_shell_get_history_manager     (EphyShell        *shell);
EphyOpenTabsManager     *ephy_shell_get_open_tabs_manager   (EphyShell        *shell);
EphySyncService         *ephy_shell_get_sync_service        (EphyShell        *shell);

GtkWidget               *ephy_shell_get_history_dialog      (EphyShell        *shell);
GtkWidget               *ephy_shell_get_firefox_sync_dialog (EphyShell        *shell);
GObject                 *ephy_shell_get_prefs_dialog        (EphyShell        *shell);

gboolean                 ephy_shell_get_checking_modified_forms     (EphyShell        *shell);
void                     ephy_shell_set_checking_modified_forms     (EphyShell        *shell,
                                                                     gboolean          is_checking);
int                      ephy_shell_get_num_windows_with_modified_forms (EphyShell        *shell);
void                     ephy_shell_set_num_windows_with_modified_forms (EphyShell        *shell,
                                                                         int               windows);

guint                    ephy_shell_get_n_windows           (EphyShell        *shell);
gboolean                 ephy_shell_close_all_windows       (EphyShell        *shell);

void                     ephy_shell_try_quit                (EphyShell        *shell);

void                     ephy_shell_open_uris               (EphyShell        *shell,
                                                             const char      **uris,
                                                             EphyStartupMode   startup_mode);

void                     ephy_shell_set_startup_context     (EphyShell                *shell,
                                                             EphyShellStartupContext  *ctx);
EphyShellStartupContext *ephy_shell_startup_context_new     (EphyStartupMode           startup_mode,
                                                             char                     *session_filename,
                                                             char                    **arguments);

void                     _ephy_shell_create_instance        (EphyEmbedShellMode mode);

void                     ephy_shell_send_notification       (EphyShell        *shell,
                                                             gchar            *id,
                                                             GNotification    *notification);

gboolean                 ephy_shell_startup_finished        (EphyShell *shell);

EphyWebView              *ephy_shell_get_web_view              (EphyShell        *shell,
                                                                guint64           id);

EphyWebView              *ephy_shell_get_active_web_view       (EphyShell        *shell);

EphyWebApplication       *ephy_shell_get_webapp                (EphyShell        *shell);

void                      ephy_shell_resync_title_boxes        (EphyShell        *shell,
                                                                const char       *title,
                                                                const char       *address);

void                      ephy_shell_register_window           (EphyShell        *shell,
                                                                EphyWindow       *window);
void                      ephy_shell_unregister_window         (EphyShell        *shell,
                                                                EphyWindow       *window);

G_END_DECLS
