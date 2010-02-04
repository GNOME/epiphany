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

#define LIBSOUP_I_HAVE_READ_BUG_594377_AND_KNOW_SOUP_PASSWORD_MANAGER_MIGHT_GO_AWAY
#define NSPLUGINWRAPPER_SETUP "/usr/bin/mozilla-plugin-config"

#include "eel-gconf-extensions.h"
#include "ephy-embed-single.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-marshal.h"
#include "ephy-signal-accumulator.h"
#include "ephy-permission-manager.h"
#include "ephy-profile-migration.h"

#ifdef ENABLE_CERTIFICATE_MANAGER
#include "ephy-certificate-manager.h"
#endif

#include <webkit/webkit.h>
#include <libsoup/soup-gnome.h>
#include <gnome-keyring.h>

#define EPHY_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSinglePrivate))

struct _EphyEmbedSinglePrivate {
  guint online : 1;

  GHashTable *form_auth_data;
};

enum {
  PROP_0,
  PROP_NETWORK_STATUS
};

static void ephy_embed_single_init (EphyEmbedSingle *single);
static void ephy_embed_single_class_init (EphyEmbedSingleClass *klass);
static void ephy_permission_manager_iface_init (EphyPermissionManagerIface *iface);
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
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_CERTIFICATE_MANAGER,
                                                ephy_certificate_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))
#else
G_DEFINE_TYPE_WITH_CODE (EphyEmbedSingle, ephy_embed_single, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))
#endif

static void
form_auth_data_free (EphyEmbedSingleFormAuthData *data)
{
  g_free (data->form_username);
  g_free (data->form_password);
  g_free (data->username);

  g_slice_free (EphyEmbedSingleFormAuthData, data);
}

static EphyEmbedSingleFormAuthData*
form_auth_data_new (const char *form_username,
                    const char *form_password,
                    const char *username)
{
  EphyEmbedSingleFormAuthData *data;

  data = g_slice_new (EphyEmbedSingleFormAuthData);
  data->form_username = g_strdup (form_username);
  data->form_password = g_strdup (form_password);
  data->username = g_strdup (username);

  return data;
}

static void
get_attr_cb (GnomeKeyringResult result,
             GnomeKeyringAttributeList *attributes,
             EphyEmbedSingle *single)
{
  int i = 0;
  GnomeKeyringAttribute *attribute;
  char *server = NULL, *username = NULL;

  if (result != GNOME_KEYRING_RESULT_OK)
    return;

  attribute = (GnomeKeyringAttribute*)attributes->data;
  for (i = 0; i < attributes->len; i++) {
    if (server && username)
      break;

    if (attribute[i].type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
      if (g_str_equal (attribute[i].name, "server")) {
        server = g_strdup (attribute[i].value.string);
      } else if (g_str_equal (attribute[i].name, "user")) {
        username = g_strdup (attribute[i].value.string);
      }
    }
  }

  if (server && username &&
      g_strstr_len (server, -1, "form%5Fusername") &&
      g_strstr_len (server, -1, "form%5Fpassword")) {
    /* This is a stored login/password from a form, cache the form
     * names locally so we don't need to hit the keyring daemon all
     * the time */
    const char *form_username, *form_password;
    GHashTable *t;
    SoupURI *uri = soup_uri_new (server);
    t = soup_form_decode (uri->query);
    form_username = g_hash_table_lookup (t, FORM_USERNAME_KEY);
    form_password = g_hash_table_lookup (t, FORM_PASSWORD_KEY);
    ephy_embed_single_add_form_auth (single, uri->host, form_username, form_password, username);
    soup_uri_free (uri);
    g_hash_table_destroy (t);
  }

  g_free (server);
  g_free (username);
}

static void
store_form_data_cb (GnomeKeyringResult result, GList *l, EphyEmbedSingle *single)
{
  GList *p;

  if (result != GNOME_KEYRING_RESULT_OK)
    return;

  for (p = l; p; p = p->next) {
    guint key_id = GPOINTER_TO_UINT (p->data);
    gnome_keyring_item_get_attributes (GNOME_KEYRING_DEFAULT,
                                       key_id,
                                       (GnomeKeyringOperationGetAttributesCallback) get_attr_cb,
                                       single,
                                       NULL);
  }
}

static void
cache_keyring_form_data (EphyEmbedSingle *single)
{
  gnome_keyring_list_item_ids (GNOME_KEYRING_DEFAULT,
                               (GnomeKeyringOperationGetListCallback)store_form_data_cb,
                               single,
                               NULL);
}

static void
free_form_auth_data_list (gpointer data)
{
  GSList *p, *l = (GSList*)data;

  for (p = l; p; p = p->next)
    form_auth_data_free ((EphyEmbedSingleFormAuthData*)p->data);

  g_slist_free (l);
}

static void
remove_form_auth_data (gpointer key, gpointer value, gpointer user_data)
{
  if (value)
    free_form_auth_data_list ((GSList*)value);
}

static void
ephy_embed_single_finalize (GObject *object)
{
  EphyEmbedSinglePrivate *priv = EPHY_EMBED_SINGLE (object)->priv;

  ephy_embed_prefs_shutdown ();

  if (priv->form_auth_data) {
    g_hash_table_foreach (priv->form_auth_data,
                          (GHFunc)remove_form_auth_data,
                          NULL);
    g_hash_table_destroy (priv->form_auth_data);
  }

  G_OBJECT_CLASS (ephy_embed_single_parent_class)->finalize (object);
}

static void
ephy_embed_single_init (EphyEmbedSingle *single)
{
  EphyEmbedSinglePrivate *priv;

  single->priv = priv = EPHY_EMBED_SINGLE_GET_PRIVATE (single);
  priv->online = TRUE;

  priv->form_auth_data = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                NULL);
  cache_keyring_form_data (single);
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
  char *cookie_policy;

  /* Initialise nspluginwrapper's plugins if available */
  if (g_file_test (NSPLUGINWRAPPER_SETUP, G_FILE_TEST_EXISTS) != FALSE)
    g_spawn_command_line_sync (NSPLUGINWRAPPER_SETUP, NULL, NULL, NULL, NULL);

  ephy_embed_prefs_init ();

  session = webkit_get_default_session ();

  /* Store cookies in moz-compatible SQLite format */
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  jar = soup_cookie_jar_sqlite_new (filename, FALSE);
  g_free (filename);
  cookie_policy = eel_gconf_get_string (CONF_SECURITY_COOKIES_ACCEPT);
  ephy_embed_prefs_set_cookie_jar_policy (jar, cookie_policy);
  g_free (cookie_policy);

  soup_session_add_feature (session, SOUP_SESSION_FEATURE (jar));
  g_object_unref (jar);

  /* Use GNOME proxy settings through libproxy */
  soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);

#ifdef SOUP_TYPE_PASSWORD_MANAGER
  /* Use GNOME keyring to store passwords. Only add the manager if we
     are not using a private session, otherwise we want any new
     password to expire when we exit *and* we don't want to use any
     existing password in the keyring */
  if (ephy_has_private_profile () == FALSE)
    soup_session_add_feature_by_type (session, SOUP_TYPE_PASSWORD_MANAGER_GNOME);
#endif

  return TRUE;
}

/**
 * ephy_embed_single_clear_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the HTTP cache (temporarily saved web pages).
 **/
void
ephy_embed_single_clear_cache (EphyEmbedSingle *single)
{
}

/**
 * ephy_embed_single_clear_auth_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the HTTP authentication cache.
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

/**
 * ephy_embed_single_get_form_auth:
 * @single: an #EphyEmbedSingle
 * @uri: the URI of a web page
 * 
 * Gets a #GSList of all stored login/passwords, in
 * #EphyEmbedSingleFormAuthData format, for any form in @uri, or %NULL
 * if we have none.
 * 
 * The #EphyEmbedSingleFormAuthData structs and the #GSList are owned
 * by @single and should not be freed by the user.
 * 
 * Returns: #GSList with the possible auto-fills for the forms in
 * @uri, or %NULL
 **/
GSList *
ephy_embed_single_get_form_auth (EphyEmbedSingle *single,
                                 const char *uri)
{
  EphyEmbedSinglePrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_SINGLE (single), NULL);
  g_return_val_if_fail (uri, NULL);

  priv = single->priv;

  return g_hash_table_lookup (priv->form_auth_data, uri);
}

/**
 * ephy_embed_single_add_form_auth:
 * @single: an #EphyEmbedSingle
 * @uri: URI of the page
 * @form_username: name of the username input field
 * @form_password: name of the password input field
 * @username: username
 * 
 * Adds a new entry to the local cache of form auth data stored in
 * @single.
 * 
 **/
void
ephy_embed_single_add_form_auth (EphyEmbedSingle *single,
                                 const char *uri,
                                 const char *form_username,
                                 const char *form_password,
                                 const char *username)
{
  EphyEmbedSingleFormAuthData *form_data;
  EphyEmbedSinglePrivate *priv;
  GSList *l;

  g_return_if_fail (EPHY_IS_EMBED_SINGLE (single));
  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);
  g_return_if_fail (username);

  priv = single->priv;

  LOG ("Appending: name field: %s / pass field: %s / username: %s / uri: %s", form_username, form_password, username, uri);

  form_data = form_auth_data_new (form_username, form_password, username);
  l = g_hash_table_lookup (priv->form_auth_data,
                           uri);
  l = g_slist_append (l, form_data);
  g_hash_table_replace (priv->form_auth_data,
                        g_strdup (uri),
                        l);
}
