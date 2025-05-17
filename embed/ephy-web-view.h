/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2008 Gustavo Noronha Silva
 *  Copyright © 2012 Igalia S.L.
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

#include <webkit/webkit.h>

#include "ephy-autofill-fill-choice.h"
#include "ephy-embed-shell.h"
#include "ephy-history-types.h"
#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_VIEW (ephy_web_view_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebView, ephy_web_view, EPHY, WEB_VIEW, WebKitWebView)

#define EPHY_WEB_VIEW_NON_SEARCH_REGEX  "(" \
                                        "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9](:[0-9]+)?.*$|" \
                                        "^::[0-9a-f:]*$|" \
                                        "^[0-9a-f:]+:[0-9a-f:]*$|" \
                                        "^https?://[^/\\.[:space:]]+.*$|" \
                                        "^about:.*$|" \
                                        "^data:.*$|" \
                                        "^file:.*$|" \
                                        "^inspector://.*$|" \
                                        "^webkit://.*$|" \
                                        "^ephy-resource://.*$|" \
                                        "^view-source:.*$|" \
                                        "^ephy-reader:.*$" \
                                        ")"

#define EPHY_WEB_VIEW_DOMAIN_REGEX "^localhost(\\.[^[:space:]]+)?(:\\d+)?(:[0-9]+)?(/.*)?$|" \
                                   "^[^\\.[:space:]]+\\.[^\\.[:space:]]+.*$|"

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
  EPHY_WEB_VIEW_ERROR_PAGE_NONE,
  EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR,
  EPHY_WEB_VIEW_ERROR_PAGE_CRASH,
  EPHY_WEB_VIEW_ERROR_PROCESS_CRASH,
  EPHY_WEB_VIEW_ERROR_UNRESPONSIVE_PROCESS,
  EPHY_WEB_VIEW_ERROR_INVALID_TLS_CERTIFICATE,
  EPHY_WEB_VIEW_ERROR_NO_SUCH_FILE,
} EphyWebViewErrorPage;

typedef enum {
  EPHY_WEB_VIEW_TLS_ERROR_PAGE_MESSAGE_HANDLER = 1 << 0,
  EPHY_WEB_VIEW_RELOAD_PAGE_MESSAGE_HANDLER = 1 << 1,
  EPHY_WEB_VIEW_ABOUT_APPS_MESSAGE_HANDLER = 1 << 2,
} EphyWebViewMessageHandler;

typedef enum {
  EPHY_WEB_VIEW_REGISTER_MESSAGE_HANDLER_FOR_CURRENT_PAGE,
  EPHY_WEB_VIEW_REGISTER_MESSAGE_HANDLER_FOR_NEXT_LOAD
} EphyWebViewMessageHandlerScope;

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
GIcon *                    ephy_web_view_get_icon                 (EphyWebView               *view);
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
gboolean                   ephy_web_view_is_newtab                (EphyWebView               *view);
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
                                                                   const char               **address,
                                                                   GTlsCertificate          **certificate,
                                                                   GTlsCertificateFlags      *errors);
void                       ephy_web_view_print                    (EphyWebView               *view);
const char *               ephy_web_view_get_address              (EphyWebView               *view);
const char *               ephy_web_view_get_display_address      (EphyWebView               *view);
void                       ephy_web_view_set_placeholder          (EphyWebView               *view,
                                                                   const char                *uri,
                                                                   const char                *title);
EphyWebViewErrorPage       ephy_web_view_get_error_page           (EphyWebView               *view);
void                       ephy_web_view_load_error_page          (EphyWebView               *view,
                                                                   const char                *uri,
                                                                   EphyWebViewErrorPage       page,
                                                                   GError                    *error,
                                                                   gpointer                   user_data);
void                       ephy_web_view_get_best_web_app_icon    (EphyWebView               *view,
                                                                   GCancellable              *cancellable,
                                                                   GAsyncReadyCallback        callback,
                                                                   gpointer                   user_data);
gboolean               ephy_web_view_get_best_web_app_icon_finish (EphyWebView               *view,
                                                                   GAsyncResult              *result,
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
void                       ephy_web_view_get_web_app_mobile_capable        (EphyWebView         *view,
                                                                            GCancellable        *cancellable,
                                                                            GAsyncReadyCallback  callback,
                                                                            gpointer             user_data);
gboolean                   ephy_web_view_get_web_app_mobile_capable_finish (EphyWebView   *view,
                                                                            GAsyncResult  *result,
                                                                            GError       **error);

void                       ephy_web_view_set_visit_type           (EphyWebView *view, 
                                                                   EphyHistoryPageVisitType visit_type);
EphyHistoryPageVisitType   ephy_web_view_get_visit_type           (EphyWebView *view);

void                       ephy_web_view_save                     (EphyWebView               *view,
                                                                   const char                *uri);
void                       ephy_web_view_load_homepage            (EphyWebView               *view);
void                       ephy_web_view_load_new_tab_page        (EphyWebView               *view);

char *                     ephy_web_view_create_web_application   (EphyWebView               *view,
                                                                   const char                *title,
                                                                   GdkPixbuf                 *icon);

void                       ephy_web_view_toggle_reader_mode       (EphyWebView               *view,
                                                                   gboolean                   active);

gboolean                   ephy_web_view_is_reader_mode_available (EphyWebView               *view);

gboolean                   ephy_web_view_get_reader_mode_state    (EphyWebView               *view);

gboolean                   ephy_web_view_is_in_auth_dialog        (EphyWebView               *view);

GtkWidget                 *ephy_web_view_new_with_user_content_manager (WebKitUserContentManager *ucm);

guint64                    ephy_web_view_get_uid                       (EphyWebView *web_view);

void                       ephy_web_view_get_web_app_manifest_url (EphyWebView         *view,
                                                                   GCancellable        *cancellable,
                                                                   GAsyncReadyCallback  callback,
                                                                   gpointer             user_data);
char                      *ephy_web_view_get_web_app_manifest_url_finish (EphyWebView   *view,
                                                                          GAsyncResult  *result,
                                                                          GError       **error);

void                       ephy_web_view_register_message_handler (EphyWebView                    *view,
                                                                   EphyWebViewMessageHandler       handler,
                                                                   EphyWebViewMessageHandlerScope  scope);

void ephy_web_view_autofill                                       (EphyWebView               *view,
                                                                   const char                *selector,
                                                                   EphyAutofillFillChoice     fill_choice);

gboolean ephy_web_view_autofill_popup_enabled                     (EphyWebView               *web_view);

void ephy_web_view_autofill_disable_popup                         (EphyWebView               *web_view);

GListModel *               ephy_web_view_get_opensearch_engines   (EphyWebView               *view);

void ephy_web_view_set_location_entry_position                    (EphyWebView *self,
                                                                   int          position);

int ephy_web_view_get_location_entry_position                     (EphyWebView *self);

void ephy_web_view_set_location_entry_has_focus                   (EphyWebView *self,
                                                                   gboolean     focus);

gboolean ephy_web_view_get_location_entry_has_focus               (EphyWebView *self);

G_END_DECLS
