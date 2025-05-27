/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include <adwaita.h>
#include <webkit/webkit.h>

#include "ephy-downloads-manager.h"
#include "ephy-encodings.h"
#include "ephy-history-service.h"
#include "ephy-password-manager.h"
#include "ephy-permissions-manager.h"
#include "ephy-search-engine-manager.h"

G_BEGIN_DECLS

typedef struct _EphyFiltersManager EphyFiltersManager;

#define EPHY_TYPE_EMBED_SHELL (ephy_embed_shell_get_type ())

G_DECLARE_DERIVABLE_TYPE (EphyEmbedShell, ephy_embed_shell, EPHY, EMBED_SHELL, AdwApplication)

typedef enum
{
  EPHY_EMBED_SHELL_MODE_BROWSER,
  EPHY_EMBED_SHELL_MODE_STANDALONE,
  EPHY_EMBED_SHELL_MODE_PRIVATE,
  EPHY_EMBED_SHELL_MODE_INCOGNITO,
  EPHY_EMBED_SHELL_MODE_APPLICATION,
  EPHY_EMBED_SHELL_MODE_TEST,
  EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER,
  EPHY_EMBED_SHELL_MODE_AUTOMATION,
  EPHY_EMBED_SHELL_MODE_KIOSK
} EphyEmbedShellMode;

struct _EphyEmbedShellClass
{
  AdwApplicationClass parent_class;
};

EphyEmbedShell    *ephy_embed_shell_get_default                (void);
const char        *ephy_embed_shell_get_guid                   (EphyEmbedShell   *shell);
WebKitWebContext  *ephy_embed_shell_get_web_context            (EphyEmbedShell   *shell);
WebKitNetworkSession *ephy_embed_shell_get_network_session     (EphyEmbedShell   *shell);
EphyHistoryService
                  *ephy_embed_shell_get_global_history_service (EphyEmbedShell   *shell);
EphyEncodings     *ephy_embed_shell_get_encodings              (EphyEmbedShell   *shell);
void               ephy_embed_shell_restored_window            (EphyEmbedShell   *shell);
void               ephy_embed_shell_set_page_setup             (EphyEmbedShell   *shell,
                                                                GtkPageSetup     *page_setup);
GtkPageSetup      *ephy_embed_shell_get_page_setup             (EphyEmbedShell   *shell);
void               ephy_embed_shell_set_print_settings         (EphyEmbedShell   *shell,
                                                                GtkPrintSettings *settings);
GtkPrintSettings  *ephy_embed_shell_get_print_settings         (EphyEmbedShell   *shell);
EphyEmbedShellMode ephy_embed_shell_get_mode                   (EphyEmbedShell   *shell);
void               ephy_embed_shell_set_thumbnail_path         (EphyEmbedShell   *shell,
                                                                const char       *url,
                                                                const char       *path);
void               ephy_embed_shell_schedule_thumbnail_update  (EphyEmbedShell   *shell,
                                                                EphyHistoryURL   *url);
EphyFiltersManager       *ephy_embed_shell_get_filters_manager      (EphyEmbedShell *shell);
EphyDownloadsManager     *ephy_embed_shell_get_downloads_manager    (EphyEmbedShell *shell);
EphyPermissionsManager   *ephy_embed_shell_get_permissions_manager  (EphyEmbedShell *shell);
EphySearchEngineManager  *ephy_embed_shell_get_search_engine_manager (EphyEmbedShell *shell);
EphyPasswordManager      *ephy_embed_shell_get_password_manager      (EphyEmbedShell *shell);
WebKitFaviconDatabase    *ephy_embed_shell_get_favicon_database      (EphyEmbedShell *shell);

void                     ephy_embed_shell_register_ucm (EphyEmbedShell           *shell,
                                                        WebKitUserContentManager *ucm);
void                     ephy_embed_shell_unregister_ucm (EphyEmbedShell           *shell,
                                                          WebKitUserContentManager *ucm);

gboolean                 ephy_embed_shell_should_remember_passwords (EphyEmbedShell *shell);

void                     ephy_embed_shell_set_web_extension_initialization_data (EphyEmbedShell *shell,
                                                                                 GVariant       *data);

G_END_DECLS
