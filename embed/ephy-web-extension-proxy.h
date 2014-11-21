/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_WEB_EXTENSION_PROXY_H
#define EPHY_WEB_EXTENSION_PROXY_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_EXTENSION_PROXY         (ephy_web_extension_proxy_get_type ())
#define EPHY_WEB_EXTENSION_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_WEB_EXTENSION_PROXY, EphyWebExtensionProxy))
#define EPHY_IS_WEB_EXTENSION_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_WEB_EXTENSION_PROXY))
#define EPHY_WEB_EXTENSION_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_WEB_EXTENSION_PROXY, EphyWebExtensionProxyClass))
#define EPHY_IS_WEB_EXTENSION_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_WEB_EXTENSION_PROXY))
#define EPHY_WEB_EXTENSION_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_WEB_EXTENSION_PROXY, EphyWebExtensionProxyClass))

typedef struct _EphyWebExtensionProxyClass   EphyWebExtensionProxyClass;
typedef struct _EphyWebExtensionProxy        EphyWebExtensionProxy;
typedef struct _EphyWebExtensionProxyPrivate EphyWebExtensionProxyPrivate;

struct _EphyWebExtensionProxy
{
  GObject parent;

  /*< private >*/
  EphyWebExtensionProxyPrivate *priv;
};

struct _EphyWebExtensionProxyClass
{
  GObjectClass parent_class;
};

GType                  ephy_web_extension_proxy_get_type                                  (void);

EphyWebExtensionProxy *ephy_web_extension_proxy_new                                       (GDBusConnection       *bus,
                                                                                           const char            *service_name);
const char *           ephy_web_extension_proxy_get_name_owner                            (EphyWebExtensionProxy *web_extension);
void                   ephy_web_extension_proxy_form_auth_data_save_confirmation_response (EphyWebExtensionProxy *web_extension,
                                                                                           guint                  request_id,
                                                                                           gboolean               response);
void                   ephy_web_extension_proxy_web_page_has_modified_forms               (EphyWebExtensionProxy *web_extension,
                                                                                           guint64                page_id,
                                                                                           GCancellable          *cancellable,
                                                                                           GAsyncReadyCallback    callback,
                                                                                           gpointer               user_data);
gboolean               ephy_web_extension_proxy_web_page_has_modified_forms_finish        (EphyWebExtensionProxy *web_extension,
                                                                                           GAsyncResult          *result,
                                                                                           GError               **error);
void                   ephy_web_extension_proxy_get_best_web_app_icon                     (EphyWebExtensionProxy *web_extension,
                                                                                           guint64                page_id,
                                                                                           const char            *base_uri,
                                                                                           GCancellable          *cancellable,
                                                                                           GAsyncReadyCallback    callback,
                                                                                           gpointer               user_data);
gboolean               ephy_web_extension_proxy_get_best_web_app_icon_finish              (EphyWebExtensionProxy *web_extension,
                                                                                           GAsyncResult          *result,
                                                                                           gboolean              *icon_result,
                                                                                           char                 **icon_uri,
                                                                                           char                 **icon_color,
                                                                                           GError               **error);
void                   ephy_web_extension_proxy_get_web_app_title                         (EphyWebExtensionProxy *web_extension,
                                                                                           guint64                page_id,
                                                                                           GCancellable          *cancellable,
                                                                                           GAsyncReadyCallback    callback,
                                                                                           gpointer               user_data);
char                  *ephy_web_extension_proxy_get_web_app_title_finish                  (EphyWebExtensionProxy *web_extension,
                                                                                           GAsyncResult          *result,
                                                                                           GError               **error);
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

G_END_DECLS

#endif /* !EPHY_WEB_EXTENSION_PROXY_H */
