/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*  Copyright © 2007 Xan Lopez <xan@gnome.org>
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

#include "config.h"
#include <glib/gi18n.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>
#include <libsoup/soup-gnome.h>

#include "webkit-embed-single.h"
#include "webkit-embed-prefs.h"
#include "ephy-embed-single.h"
#include "ephy-file-helpers.h"
#include "ephy-password-manager.h"
#include "ephy-permission-manager.h"

#ifdef ENABLE_CERTIFICATE_MANAGER
#include "ephy-certificate-manager.h"
#endif

#define WEBKIT_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), WEBKIT_TYPE_EMBED_SINGLE, WebKitEmbedSinglePrivate))

struct WebKitEmbedSinglePrivate {
  guint online : 1;
};

enum {
  PROP_0,
  PROP_NETWORK_STATUS
};

static void webkit_embed_single_class_init     (WebKitEmbedSingleClass *klass);
static void webkit_embed_single_init           (WebKitEmbedSingle *wes);
static void ephy_embed_single_iface_init       (EphyEmbedSingleIface *iface);
static void ephy_password_manager_iface_init   (EphyPasswordManagerIface *iface);
static void ephy_permission_manager_iface_init (EphyPermissionManagerIface *iface);

#ifdef ENABLE_CERTIFICATE_MANAGER
static void ephy_certificate_manager_iface_init (EphyCertificateManagerIface *iface);
#endif

/* Some compilers (like gcc 2.95) don't support preprocessor directives inside macros,
   so we have to duplicate the whole thing */

#ifdef ENABLE_CERTIFICATE_MANAGER
G_DEFINE_TYPE_WITH_CODE (WebKitEmbedSingle, webkit_embed_single, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_SINGLE,
                                                ephy_embed_single_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PASSWORD_MANAGER,
                                                ephy_password_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_CERTIFICATE_MANAGER,
                                                ephy_certificate_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))
#else
G_DEFINE_TYPE_WITH_CODE (WebKitEmbedSingle, webkit_embed_single, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_SINGLE,
                                                ephy_embed_single_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PASSWORD_MANAGER,
                                                ephy_password_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))
#endif


static void
webkit_embed_single_init (WebKitEmbedSingle *wes)
{
  wes->priv = WEBKIT_EMBED_SINGLE_GET_PRIVATE (wes);

  wes->priv->online = TRUE;
}

static void
webkit_embed_single_dispose (GObject *object)
{
  G_OBJECT_CLASS (webkit_embed_single_parent_class)->dispose (object);
}

static void
webkit_embed_single_finalize (GObject *object)
{
  webkit_embed_prefs_shutdown ();

  G_OBJECT_CLASS (webkit_embed_single_parent_class)->finalize (object);
}

static void
impl_clear_cache (EphyEmbedSingle *shell)
{
}

static void
impl_clear_auth_cache (EphyEmbedSingle *shell)
{
}

static void
impl_set_network_status (EphyEmbedSingle *single,
                         gboolean online)
{
  WebKitEmbedSinglePrivate *priv = WEBKIT_EMBED_SINGLE (single)->priv;

  if (online != priv->online) {
    priv->online = online;
  }
}

static gboolean
impl_get_network_status (EphyEmbedSingle *esingle)
{
  return WEBKIT_EMBED_SINGLE (esingle)->priv->online;
}

static const char *
impl_get_backend_name (EphyEmbedSingle *esingle)
{
  /* If you alter the return values here, remember to update
   * the docs in ephy-embed-single.c */
  return "WebKit";
}

static GList *
impl_get_font_list (EphyEmbedSingle *shell,
                    const char *langGroup)
{
  return NULL;
}

static GList *
impl_list_passwords (EphyPasswordManager *manager)
{
  return NULL;
}

static void
impl_remove_password (EphyPasswordManager *manager,
                      EphyPasswordInfo *info)
{
}

static void
impl_remove_all_passwords (EphyPasswordManager *manager)
{
}

static void
impl_add_password (EphyPasswordManager *manager,
                   EphyPasswordInfo *info)
{
}

static void
impl_permission_manager_add (EphyPermissionManager *manager,
                             const char *host,
                             const char *type,
                             EphyPermission permission)
{
}

static void
impl_permission_manager_remove (EphyPermissionManager *manager,
                                const char *host,
                                const char *type)
{
}

static void
impl_permission_manager_clear (EphyPermissionManager *manager)
{
}

static EphyPermission
impl_permission_manager_test (EphyPermissionManager *manager,
                              const char *host,
                              const char *type)
{
  g_return_val_if_fail (type != NULL && type[0] != '\0', EPHY_PERMISSION_DEFAULT);

  return (EphyPermission)0;
}

static gboolean
impl_init (EphyEmbedSingle *esingle)
{
  SoupSession *session;
  SoupCookieJar *jar;
  char *filename;

  webkit_embed_prefs_init ();

  session = webkit_get_default_session ();

  /* Store cookies in moz-compatible SQLite format */
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  jar = soup_cookie_jar_sqlite_new (filename, FALSE);
  g_free (filename);

  soup_session_add_feature (session, SOUP_SESSION_FEATURE(jar));
  g_object_unref (jar);

  /* Use GNOME proxy settings through libproxy */
  soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);

  return TRUE;
}

static GList *
impl_permission_manager_list (EphyPermissionManager *manager,
                              const char *type)
{
  GList *list = NULL;
  return list;
}

static GtkWidget *
impl_open_window (EphyEmbedSingle *single,
                  EphyEmbed *parent,
                  const char *address,
                  const char *name,
                  const char *features)
{
  return NULL;
}

#ifdef ENABLE_CERTIFICATE_MANAGER

static gboolean
impl_remove_certificate (EphyCertificateManager *manager,
                         EphyX509Cert *cert)
{
  return TRUE;
}

#define NICK_DELIMITER PRUnichar ('\001')
static GList *
impl_get_certificates (EphyCertificateManager *manager,
                       EphyX509CertType type)
{
  return NULL;
}

static gboolean
impl_import (EphyCertificateManager *manager,
             const gchar *file)
{
  return TRUE;
}

#endif /* ENABLE_CERTIFICATE_MANAGER */

static void
webkit_embed_single_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  EphyEmbedSingle *single = EPHY_EMBED_SINGLE (object);

  switch (prop_id) {
    case PROP_NETWORK_STATUS:
      g_value_set_boolean (value, ephy_embed_single_get_network_status (single));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
webkit_embed_single_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  EphyEmbedSingle *single = EPHY_EMBED_SINGLE (object);

  switch (prop_id) {
    case PROP_NETWORK_STATUS:
      ephy_embed_single_set_network_status (single, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
static void
webkit_embed_single_class_init (WebKitEmbedSingleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = webkit_embed_single_dispose;
  object_class->finalize = webkit_embed_single_finalize;
  object_class->get_property = webkit_embed_single_get_property;
  object_class->set_property = webkit_embed_single_set_property;

  g_object_class_override_property (object_class, PROP_NETWORK_STATUS, "network-status");

  g_type_class_add_private (object_class, sizeof (WebKitEmbedSinglePrivate));
}

static void
ephy_embed_single_iface_init (EphyEmbedSingleIface *iface)
{
  iface->init = impl_init;
  iface->clear_cache = impl_clear_cache;
  iface->clear_auth_cache = impl_clear_auth_cache;
  iface->set_network_status = impl_set_network_status;
  iface->get_network_status = impl_get_network_status;
  iface->get_font_list = impl_get_font_list;
  iface->open_window = impl_open_window;
  iface->get_backend_name = impl_get_backend_name;
}

static void
ephy_password_manager_iface_init (EphyPasswordManagerIface *iface)
{
  iface->add = impl_add_password;
  iface->remove = impl_remove_password;
  iface->remove_all = impl_remove_all_passwords;
  iface->list = impl_list_passwords;
}

static void
ephy_permission_manager_iface_init (EphyPermissionManagerIface *iface)
{
  iface->add = impl_permission_manager_add;
  iface->remove = impl_permission_manager_remove;
  iface->clear = impl_permission_manager_clear;
  iface->test = impl_permission_manager_test;
  iface->list = impl_permission_manager_list;
}

#ifdef ENABLE_CERTIFICATE_MANAGER

static void
ephy_certificate_manager_iface_init (EphyCertificateManagerIface *iface)
{
  iface->get_certificates = impl_get_certificates;
  iface->remove_certificate = impl_remove_certificate;
  iface->import = impl_import;
}

#endif /* ENABLE_CERTIFICATE_MANAGER */
