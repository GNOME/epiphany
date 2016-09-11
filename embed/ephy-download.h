/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-download.h
 * This file is part of Epiphany
 *
 * Copyright © 2011 - Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <webkit2/webkit2.h>

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

void          ephy_download_cancel                (EphyDownload *download);
gboolean      ephy_download_is_active             (EphyDownload *download);
gboolean      ephy_download_succeeded             (EphyDownload *download);
gboolean      ephy_download_failed                (EphyDownload *download,
                                                   GError      **error);

void          ephy_download_set_destination_uri   (EphyDownload *download,
                                                   const char *destination);

WebKitDownload *ephy_download_get_webkit_download (EphyDownload *download);

const char   *ephy_download_get_destination_uri   (EphyDownload *download);
const char   *ephy_download_get_content_type      (EphyDownload *download);

guint32       ephy_download_get_start_time        (EphyDownload *download);

EphyDownloadActionType ephy_download_get_action   (EphyDownload *download);
void          ephy_download_set_action            (EphyDownload *download,
                                                   EphyDownloadActionType action);
gboolean      ephy_download_do_download_action    (EphyDownload *download,
                                                   EphyDownloadActionType action);

G_END_DECLS
