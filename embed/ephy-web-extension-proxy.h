/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_EXTENSION_PROXY (ephy_web_extension_proxy_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebExtensionProxy, ephy_web_extension_proxy, EPHY, WEB_EXTENSION_PROXY, GObject)

EphyWebExtensionProxy *ephy_web_extension_proxy_new                                       (GDBusConnection       *connection);
void                   ephy_web_extension_proxy_history_set_urls                          (EphyWebExtensionProxy *web_extension,
                                                                                           GList                 *urls);
void                   ephy_web_extension_proxy_history_set_url_thumbnail                 (EphyWebExtensionProxy *web_extension,
                                                                                           const char            *url,
                                                                                           const char            *path);
void                   ephy_web_extension_proxy_history_set_url_title                     (EphyWebExtensionProxy *web_extension,
                                                                                           const char            *url,
                                                                                           const char            *title);
void                   ephy_web_extension_proxy_history_delete_url                        (EphyWebExtensionProxy *web_extension,
                                                                                           const char            *url);
void                   ephy_web_extension_proxy_history_delete_host                       (EphyWebExtensionProxy *web_extension,
                                                                                           const char            *host);
void                   ephy_web_extension_proxy_history_clear                             (EphyWebExtensionProxy *web_extension);
void                   ephy_web_extension_proxy_password_cached_users_response            (EphyWebExtensionProxy *web_extension,
                                                                                           GList                 *users,
                                                                                           gint32                 id);
void                   ephy_web_extension_proxy_password_query_response                   (EphyWebExtensionProxy *web_extension,
                                                                                           const char            *username,
                                                                                           const char            *password,
                                                                                           gint32                 id);
G_END_DECLS
