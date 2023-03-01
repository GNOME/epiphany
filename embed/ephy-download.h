/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include <glib-object.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOAD (ephy_download_get_type ())

G_DECLARE_FINAL_TYPE (EphyDownload, ephy_download, EPHY, DOWNLOAD, GObject)

typedef enum
{
  EPHY_DOWNLOAD_ACTION_NONE,
  EPHY_DOWNLOAD_ACTION_BROWSE_TO,
  EPHY_DOWNLOAD_ACTION_OPEN
} EphyDownloadActionType;

EphyDownload *ephy_download_new                   (WebKitDownload *download);
EphyDownload *ephy_download_new_for_uri           (const char     *uri);
EphyDownload *ephy_download_new_for_uri_internal  (const char     *uri);

void          ephy_download_cancel                (EphyDownload *download);
gboolean      ephy_download_is_active             (EphyDownload *download);
gboolean      ephy_download_succeeded             (EphyDownload *download);
gboolean      ephy_download_failed                (EphyDownload *download,
                                                   GError      **error);

void          ephy_download_set_destination       (EphyDownload *download,
                                                   const char *destination);

WebKitDownload *ephy_download_get_webkit_download (EphyDownload *download);

const char   *ephy_download_get_destination       (EphyDownload *download);
const char   *ephy_download_get_content_type      (EphyDownload *download);

EphyDownloadActionType ephy_download_get_action   (EphyDownload *download);
void          ephy_download_set_action            (EphyDownload *download,
                                                   EphyDownloadActionType action);
gboolean      ephy_download_do_download_action    (EphyDownload          *download,
                                                   EphyDownloadActionType action);
void          ephy_download_disable_desktop_notification
                                                  (EphyDownload *download);
guint64       ephy_download_get_uid               (EphyDownload *download);

void          ephy_download_set_always_ask_destination
                                                  (EphyDownload *download,
                                                   gboolean      always_ask);
void          ephy_download_set_choose_filename   (EphyDownload *download,
                                                   gboolean      choose_filename);
void          ephy_download_set_suggested_destination
                                                  (EphyDownload *download,
                                                   const char   *suggested_directory,
                                                   const char   *suggested_filename);
void          ephy_download_set_allow_overwrite   (EphyDownload *download,
                                                   gboolean      allow_overwrite);
gboolean      ephy_download_get_was_moved         (EphyDownload *download);
GDateTime    *ephy_download_get_start_time        (EphyDownload *download);
GDateTime    *ephy_download_get_end_time          (EphyDownload *download);
gboolean      ephy_download_get_initiating_web_extension_info
                                                  (EphyDownload  *download,
                                                   const char   **extension_id_out,
                                                   const char   **extension_name_out);
void          ephy_download_set_initiating_web_extension_info
                                                  (EphyDownload *download,
                                                   const char   *extension_id,
                                                   const char   *extension_name);
G_END_DECLS
