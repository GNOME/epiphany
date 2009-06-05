/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include "ephy-embed-single.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-file-helpers.h"
#include "ephy-marshal.h"
#include "ephy-signal-accumulator.h"
#include "ephy-password-manager.h"
#include "ephy-permission-manager.h"

#ifdef ENABLE_CERTIFICATE_MANAGER
#include "ephy-certificate-manager.h"
#endif

#include <webkit/webkit.h>
#include <libsoup/soup-gnome.h>

#define EPHY_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSinglePrivate))

struct _EphyEmbedSinglePrivate {
  guint online : 1;
};

enum {
  PROP_0,
  PROP_NETWORK_STATUS
};

static void ephy_embed_single_init (EphyEmbedSingle *single);
static void ephy_embed_single_class_init (EphyEmbedSingleClass *klass);
static void ephy_permission_manager_iface_init (EphyPermissionManagerIface *iface);
static void ephy_password_manager_iface_init (EphyPasswordManagerIface *iface);
#ifdef ENABLE_CERTIFICATE_MANAGER
static void ephy_certificate_manager_iface_init (EphyCertificateManagerIface *iface);
#endif

static void
ephy_embed_single_get_property (GObject *object,
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
ephy_embed_single_set_property (GObject *object,
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

/* Some compilers (like gcc 2.95) don't support preprocessor directives inside macros,
   so we have to duplicate the whole thing */

#ifdef ENABLE_CERTIFICATE_MANAGER
G_DEFINE_TYPE_WITH_CODE (EphyEmbedSingle, ephy_embed_single, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PASSWORD_MANAGER,
                                                ephy_password_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_CERTIFICATE_MANAGER,
                                                ephy_certificate_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))
#else
G_DEFINE_TYPE_WITH_CODE (EphyEmbedSingle, ephy_embed_single, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PASSWORD_MANAGER,
                                                ephy_password_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))
#endif

static void
ephy_embed_single_finalize (GObject *object)
{
  ephy_embed_prefs_shutdown ();

  G_OBJECT_CLASS (ephy_embed_single_parent_class)->finalize (object);
}

static void
ephy_embed_single_init (EphyEmbedSingle *single)
{
  EphyEmbedSinglePrivate *priv;

  single->priv = priv = EPHY_EMBED_SINGLE_GET_PRIVATE (single);
  priv->online = TRUE;
}

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_embed_single_finalize;
  object_class->get_property = ephy_embed_single_get_property;
  object_class->set_property = ephy_embed_single_set_property;

  /**
   * EphyEmbedSingle::new-window:
   * @single:
   * @parent_embed: the #EphyEmbed requesting the new window, or %NULL
   * @mask: a #EphyEmbedChrome
   *
   * The ::new_window signal is emitted when a new window needs to be opened.
   * For example, when a JavaScript popup window was opened.
   *
   * Return a new #EphyEmbed.
   **/
  g_signal_new ("new-window",
                EPHY_TYPE_EMBED_SINGLE,
                G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyEmbedSingleClass, new_window),
                ephy_signal_accumulator_object, ephy_embed_get_type,
                ephy_marshal_OBJECT__OBJECT_FLAGS,
                GTK_TYPE_WIDGET,
                2,
                GTK_TYPE_WIDGET,
                EPHY_TYPE_WEB_VIEW_CHROME);

  /**
   * EphyEmbedSingle::handle_content:
   * @single:
   * @mime_type: the MIME type of the content
   * @address: the URL to the content
   *
   * The ::handle_content signal is emitted when encountering content of a mime
   * type Epiphany is unable to handle itself.
   *
   * If a connected callback returns %TRUE, the signal will stop propagating. For
   * example, this could be used by a download manager to prevent other
   * ::handle_content listeners from being called.
   **/
  g_signal_new ("handle_content",
                EPHY_TYPE_EMBED_SINGLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyEmbedSingleClass, handle_content),
                g_signal_accumulator_true_handled, NULL,
                ephy_marshal_BOOLEAN__STRING_STRING,
                G_TYPE_BOOLEAN,
                2,
                G_TYPE_STRING,
                G_TYPE_STRING);

  /**
   * EphyEmbedSingle::add-sidebar:
   * @single:
   * @url: The url of the sidebar to be added
   * @title: The title of the sidebar to be added
   *
   * The ::add-sidebar signal is emitted when the user clicks a javascript link that
   * requests adding a url to the sidebar.
   **/
  g_signal_new ("add-sidebar",
                EPHY_TYPE_EMBED_SINGLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyEmbedSingleClass, add_sidebar),
                g_signal_accumulator_true_handled, NULL,
                ephy_marshal_BOOLEAN__STRING_STRING,
                G_TYPE_BOOLEAN,
                2,
                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * EphyEmbedSingle::add-search-engine
   * @single:
   * @url: The url of the search engine definition file
   * @icon_url: The url of the icon to use for this engine
   * @title: The title of the search engine to be added
   *
   * The ::add-search-engine signal is emitted when the user clicks a javascript link that
   * requests adding a search engine to the sidebar.
   **/
  g_signal_new ("add-search-engine",
                EPHY_TYPE_EMBED_SINGLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyEmbedSingleClass, add_search_engine),
                g_signal_accumulator_true_handled, NULL,
                ephy_marshal_BOOLEAN__STRING_STRING_STRING,
                G_TYPE_BOOLEAN,
                3,
                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * EphyEmbedSingle::network-status:
   * 
   * Whether the network is on-line.
   */
  g_object_class_install_property
    (object_class,
     PROP_NETWORK_STATUS,
     g_param_spec_boolean ("network-status",
                           "network-status",
                           "network-status",
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (EphyEmbedSinglePrivate));
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

static GList *
impl_permission_manager_list (EphyPermissionManager *manager,
                              const char *type)
{
  GList *list = NULL;
  return list;
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
ephy_password_manager_iface_init (EphyPasswordManagerIface *iface)
{
  iface->add = impl_add_password;
  iface->remove = impl_remove_password;
  iface->remove_all = impl_remove_all_passwords;
  iface->list = impl_list_passwords;
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

static void
ephy_certificate_manager_iface_init (EphyCertificateManagerIface *iface)
{
  iface->get_certificates = impl_get_certificates;
  iface->remove_certificate = impl_remove_certificate;
  iface->import = impl_import;
}

#endif /* ENABLE_CERTIFICATE_MANAGER */

/**
 * ephy_embed_single_initialize:
 * @single: the #EphyEmbedSingle
 * 
 * Performs startup initialisations. Must be called before calling
 * any other methods.
 **/
gboolean
ephy_embed_single_initialize (EphyEmbedSingle *single)
{
  SoupSession *session;
  SoupCookieJar *jar;
  char *filename;

  ephy_embed_prefs_init ();

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

/**
 * ephy_embed_single_clear_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the Mozilla cache (temporarily saved web pages).
 **/
void
ephy_embed_single_clear_cache (EphyEmbedSingle *single)
{
}

/**
 * ephy_embed_single_clear_auth_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the Mozilla HTTP authentication cache.
 *
 * This does not clear regular website passwords; it only clears the HTTP
 * authentication cache. Websites which use HTTP authentication require the
 * browser to send a password along with every HTTP request; the browser will
 * ask the user for the password once and then cache the password for subsequent
 * HTTP requests. This function will clear the HTTP authentication cache,
 * meaning the user will have to re-enter a username and password the next time
 * Epiphany requests a web page secured with HTTP authentication.
 **/
void
ephy_embed_single_clear_auth_cache (EphyEmbedSingle *single)
{
}

/**
 * ephy_embed_single_get_nework_status:
 * @single: the #EphyEmbedSingle
 * @offline: %TRUE if the network is on-line
 * 
 * Sets the state of the network connection.
 **/
void
ephy_embed_single_set_network_status (EphyEmbedSingle *single,
                                      gboolean status)
{
  if (status != single->priv->online)
    single->priv->online = status;
}

/**
 * ephy_embed_single_get_network_status:
 * @single: the #EphyEmbedSingle
 * 
 * Gets the state of the network connection.
 * 
 * Returns: %TRUE iff the network is on-line.
 **/
gboolean
ephy_embed_single_get_network_status (EphyEmbedSingle *single)
{
  return single->priv->online;
}

/**
 * ephy_embed_single_get_font_list:
 * @single: the #EphyEmbedSingle
 * @lang_group: a mozilla font language group name, or %NULL
 * 
 * Returns the list of fonts matching @lang_group, or all fonts if @lang_group
 * is %NULL.
 *
 * The available @lang_group arguments are listed in Epiphany's Fonts and Colors
 * preferences.
 * 
 * Return value: a list of font names
 **/
GList *
ephy_embed_single_get_font_list (EphyEmbedSingle *single,
                                 const char *lang_group)
{
  return NULL;
}

/**
 * ephy_embed_single_open_window:
 * @single: the #EphyEmbedSingle
 * @parent: the requested window's parent #EphyEmbed
 * @address: the URL to load
 * @name: a name for the window
 * @features: a Javascript features string
 *
 * Opens a new window, as if it were opened in @parent using the Javascript
 * method and arguments: <code>window.open(&quot;@address&quot;,
 * &quot;_blank&quot;, &quot;@features&quot;);</code>.
 * 
 * Returns: the new embed. This is either a #EphyEmbed, or, when @features specified
 * "chrome", a #GtkMozEmbed.
 *
 * NOTE: Use ephy_shell_new_tab() unless this handling of the @features string is
 * required.
 */
GtkWidget *
ephy_embed_single_open_window (EphyEmbedSingle *single,
                               EphyEmbed *parent,
                               const char *address,
                               const char *name,
                               const char *features)
{
  return NULL;
}
