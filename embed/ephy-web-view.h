/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2008 Gustavo Noronha Silva
 *  Copyright © 2012 Igalia S.L.
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

#ifndef EPHY_WEB_VIEW_H
#define EPHY_WEB_VIEW_H

#include <webkit2/webkit2.h>

#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_VIEW (ephy_web_view_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebView, ephy_web_view, EPHY, WEB_VIEW, WebKitWebView)

typedef enum
{
  EPHY_WEB_VIEW_NAV_BACK    = 1 << 0,
  EPHY_WEB_VIEW_NAV_FORWARD = 1 << 1
} EphyWebViewNavigationFlags;

typedef enum
{
  EPHY_WEB_VIEW_DOCUMENT_HTML,
  EPHY_WEB_VIEW_DOCUMENT_XML,
  EPHY_WEB_VIEW_DOCUMENT_IMAGE,
  EPHY_WEB_VIEW_DOCUMENT_OTHER
} EphyWebViewDocumentType;

typedef enum {
  EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR,
  EPHY_WEB_VIEW_ERROR_PAGE_CRASH,
  EPHY_WEB_VIEW_ERROR_PROCESS_CRASH,
  EPHY_WEB_VIEW_ERROR_INVALID_TLS_CERTIFICATE
} EphyWebViewErrorPage;

GType                      ephy_web_view_chrome_get_type          (void);
GType                      ephy_web_view_security_level_get_type  (void);
GtkWidget *                ephy_web_view_new                      (void);
GtkWidget                 *ephy_web_view_new_with_related_view    (WebKitWebView             *related_view);
void                       ephy_web_view_load_request             (EphyWebView               *view,
                                                                   WebKitURIRequest          *request);
void                       ephy_web_view_load_url                 (EphyWebView               *view,
                                                                   const char                *url);
gboolean                   ephy_web_view_is_loading               (EphyWebView               *view);
gboolean                   ephy_web_view_load_failed              (EphyWebView               *view);
GdkPixbuf *                ephy_web_view_get_icon                 (EphyWebView               *view);
EphyWebViewDocumentType    ephy_web_view_get_document_type        (EphyWebView               *view);
EphyWebViewNavigationFlags ephy_web_view_get_navigation_flags     (EphyWebView               *view);
const char *               ephy_web_view_get_status_message       (EphyWebView               *view);
const char *               ephy_web_view_get_link_message         (EphyWebView               *view);
void                       ephy_web_view_set_link_message         (EphyWebView               *view,
                                                                   const char                *address);
void                       ephy_web_view_set_security_level       (EphyWebView               *view,
                                                                   EphySecurityLevel          level);
const char *               ephy_web_view_get_typed_address        (EphyWebView               *view);
void                       ephy_web_view_set_typed_address        (EphyWebView               *view,
                                                                   const char                *address);
gboolean                   ephy_web_view_get_is_blank             (EphyWebView               *view);
gboolean                   ephy_web_view_is_overview              (EphyWebView               *view);
void                       ephy_web_view_has_modified_forms       (EphyWebView               *view,
                                                                   GCancellable              *cancellable,
                                                                   GAsyncReadyCallback        callback,
                                                                   gpointer                   user_data);
gboolean                  ephy_web_view_has_modified_forms_finish (EphyWebView               *view,
                                                                   GAsyncResult              *result,
                                                                   GError                   **error);
void                       ephy_web_view_get_security_level       (EphyWebView               *view,
                                                                   EphySecurityLevel         *level,
                                                                   GTlsCertificate          **certificate,
                                                                   GTlsCertificateFlags      *errors);
void                       ephy_web_view_print                    (EphyWebView               *view);
const char *               ephy_web_view_get_address              (EphyWebView               *view);
const char *               ephy_web_view_get_display_address      (EphyWebView               *view);
void                       ephy_web_view_set_placeholder          (EphyWebView               *view,
                                                                   const char                *uri,
                                                                   const char                *title);

void                       ephy_web_view_load_error_page          (EphyWebView               *view,
                                                                   const char                *uri,
                                                                   EphyWebViewErrorPage       page,
                                                                   GError                    *error);
void                       ephy_web_view_get_best_web_app_icon    (EphyWebView               *view,
                                                                   GCancellable              *cancellable,
                                                                   GAsyncReadyCallback        callback,
                                                                   gpointer                   user_data);
gboolean               ephy_web_view_get_best_web_app_icon_finish (EphyWebView               *view,
                                                                   GAsyncResult              *result,
                                                                   gboolean                  *icon_result,
                                                                   char                     **icon_uri,
                                                                   GdkRGBA                   *icon_color,
                                                                   GError                   **error);
void                       ephy_web_view_get_web_app_title        (EphyWebView               *view,
                                                                   GCancellable              *cancellable,
                                                                   GAsyncReadyCallback        callback,
                                                                   gpointer                   user_data);
char                      *ephy_web_view_get_web_app_title_finish (EphyWebView               *view,
                                                                   GAsyncResult              *result,
                                                                   GError                   **error);

G_END_DECLS

#endif
