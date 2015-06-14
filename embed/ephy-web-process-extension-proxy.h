/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
 *  Copyright © 2019 Abdullah Alansari
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

#define EPHY_TYPE_WEB_PROCESS_EXTENSION_PROXY (ephy_web_process_extension_proxy_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebProcessExtensionProxy, ephy_web_process_extension_proxy, EPHY, WEB_PROCESS_EXTENSION_PROXY, GObject)

EphyWebProcessExtensionProxy *ephy_web_process_extension_proxy_new                               (GDBusConnection              *connection);
void                          ephy_web_process_extension_proxy_history_set_urls                  (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  GList                        *urls);
void                          ephy_web_process_extension_proxy_history_set_url_thumbnail         (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  const char                   *url,
                                                                                                  const char                   *path);
void                          ephy_web_process_extension_proxy_history_set_url_title             (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  const char                   *url,
                                                                                                  const char                   *title);
void                          ephy_web_process_extension_proxy_history_delete_url                (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  const char                   *url);
void                          ephy_web_process_extension_proxy_history_delete_host               (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  const char                   *host);
void                          ephy_web_process_extension_proxy_history_clear                     (EphyWebProcessExtensionProxy *web_process_extension);
void                          ephy_web_process_extension_proxy_password_query_usernames_response (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  GList                        *users,
                                                                                                  gint32                        promise_id,
                                                                                                  guint64                       frame_id);
void                          ephy_web_process_extension_proxy_password_query_response           (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  const char                   *username,
                                                                                                  const char                   *password,
                                                                                                  gint32                        promise_id,
                                                                                                  guint64                       frame_id);

void                          ephy_web_process_extension_proxy_autofill                          (EphyWebProcessExtensionProxy *web_process_extension,
                                                                                                  guint64                       page_id,
                                                                                                  const char                   *selector,
                                                                                                  int                           fill_choice);

G_END_DECLS
