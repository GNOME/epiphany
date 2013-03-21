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

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-request-about.h"
#include "ephy-settings.h"
#include "ephy-signal-accumulator.h"
#include "ephy-string.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <webkit2/webkit2.h>

#define NSPLUGINWRAPPER_SETUP "/usr/bin/mozilla-plugin-config"

G_DEFINE_TYPE (EphyEmbedSingle, ephy_embed_single, G_TYPE_OBJECT)

static void
ephy_embed_single_init (EphyEmbedSingle *single)
{
}

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass)
{
}


static void
complete_about_request_for_contents (WebKitURISchemeRequest *request,
                                     gchar *data,
                                     gsize data_length)
{
  GInputStream *stream;

  stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  webkit_uri_scheme_request_finish (request, stream, data_length, "text/html");
  g_object_unref (stream);
}

static void
get_plugins_cb (WebKitWebContext *web_context,
                GAsyncResult *result,
                WebKitURISchemeRequest *request)
{
  GList *plugins;
  GString *data_str;
  gsize data_length;

  data_str = g_string_new("<html>");
  plugins = webkit_web_context_get_plugins_finish (web_context, result, NULL);
  _ephy_about_handler_handle_plugins (data_str, plugins);
  g_string_append (data_str, "</html>");

  data_length = data_str->len;
  complete_about_request_for_contents (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static void
about_request_cb (WebKitURISchemeRequest *request,
                  gpointer user_data)
{
  const gchar *path;

  path = webkit_uri_scheme_request_get_path (request);
  if (!g_strcmp0 (path, "plugins")) {
    /* Plugins API is async in WebKit2 */
    webkit_web_context_get_plugins (webkit_web_context_get_default (),
                                    NULL,
                                    (GAsyncReadyCallback) get_plugins_cb,
                                    g_object_ref (request));
  } else {
    GString *contents;
    gsize data_length;

    contents = ephy_about_handler_handle (path);
    data_length = contents->len;
    complete_about_request_for_contents (request, g_string_free (contents, FALSE), data_length);
  }
}

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
  WebKitWebContext *web_context;
  WebKitCookieManager *cookie_manager;
  char *filename;
  char *cookie_policy;

  /* Initialise nspluginwrapper's plugins if available. */
  if (g_file_test (NSPLUGINWRAPPER_SETUP, G_FILE_TEST_EXISTS) != FALSE)
    g_spawn_command_line_sync (NSPLUGINWRAPPER_SETUP, NULL, NULL, NULL, NULL);

  web_context = webkit_web_context_get_default ();

  /* Store cookies in moz-compatible SQLite format */
  cookie_manager = webkit_web_context_get_cookie_manager (web_context);
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
  g_free (filename);

  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, cookie_policy);
  g_free (cookie_policy);

  /* about: URIs handler */
  webkit_web_context_register_uri_scheme (web_context,
                                          EPHY_ABOUT_SCHEME,
                                          about_request_cb,
                                          NULL, NULL);
  return TRUE;
}

