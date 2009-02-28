/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*  Copyright © 2008 Xan Lopez <xan@gnome.org>
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

#include <config.h>

#include <glib.h>
#include <webkit/webkit.h>

#include "webkit-embed-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"

typedef struct
{
  char *gconf_key;
  char *webkit_pref;
  GConfClientNotifyFunc func;
  guint cnxn_id;
} PrefData;

static WebKitWebSettings *settings = NULL;
static guint *connections = NULL;

static void
webkit_pref_callback_int (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry,
                          gpointer data)
{
  GConfValue *gcvalue;
  gint value = 0;
  char *webkit_pref = data;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_INT) {
      value = gconf_value_get_int (gcvalue);
      value = MAX (value, 0);
  }

  g_object_set (settings, webkit_pref, value, NULL);
}

static void
webkit_pref_callback_boolean (GConfClient *client,
                              guint cnxn_id,
                              GConfEntry *entry,
                              gpointer data)
{
  GConfValue *gcvalue;
  gboolean value = FALSE;
  char *webkit_pref = data;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_BOOL) {
      value = gconf_value_get_bool (gcvalue);
  }

  g_object_set (settings, webkit_pref, value, NULL);
}

static void
webkit_pref_callback_string (GConfClient *client,
                             guint cnxn_id,
                             GConfEntry *entry,
                             gpointer data)
{
  GConfValue *gcvalue;
  const char *value = NULL;
  char *webkit_pref = data;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_STRING) {
      value = gconf_value_get_string (gcvalue);
  }

  g_object_set (settings, webkit_pref, value, NULL);
}

static void
webkit_pref_callback_user_stylesheet (GConfClient *client,
                                      guint cnxn_id,
                                      GConfEntry *entry,
                                      gpointer data)
{
  GConfValue *gcvalue;
  gboolean value = FALSE;
  char *uri = NULL;
  char *webkit_pref = data;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_BOOL) {
      value = gconf_value_get_bool (gcvalue);
  }

  if (value)
    /* We need the leading file://, so use g_strconcat instead
     * of g_build_filename */
    uri = g_strconcat ("file://",
                       ephy_dot_dir (),
                       G_DIR_SEPARATOR_S,
                       USER_STYLESHEET_FILENAME,
                       NULL);
  g_object_set (settings, webkit_pref, uri, NULL);
  g_free (uri);
}

static const PrefData webkit_pref_entries[] =
  {
    { CONF_RENDERING_FONT_MIN_SIZE,
      "minimum-font-size",
      webkit_pref_callback_int },
    { CONF_SECURITY_JAVASCRIPT_ENABLED,
      "enable-scripts",
      webkit_pref_callback_boolean },
    { CONF_LANGUAGE_DEFAULT_ENCODING,
      "default-encoding",
      webkit_pref_callback_string },
    { CONF_WEB_INSPECTOR_ENABLED,
      "enable-developer-extras",
      webkit_pref_callback_boolean },
    { CONF_USER_CSS_ENABLED,
      "user-stylesheet-uri",
      webkit_pref_callback_user_stylesheet }
  };

static void
webkit_embed_prefs_apply (WebKitEmbed *embed, WebKitWebSettings *settings)
{
  webkit_web_view_set_settings (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed),
                                settings);
}

void
webkit_embed_prefs_init (void)
{
  int i;

  eel_gconf_monitor_add ("/apps/epiphany/web");

  settings = webkit_web_settings_new ();

  connections = g_malloc (sizeof (guint) * G_N_ELEMENTS (webkit_pref_entries));

  for (i = 0; i < G_N_ELEMENTS (webkit_pref_entries); i++) {
    connections[i] = eel_gconf_notification_add (webkit_pref_entries[i].gconf_key,
                                                 webkit_pref_entries[i].func,
                                                 webkit_pref_entries[i].webkit_pref);

    eel_gconf_notify (webkit_pref_entries[i].gconf_key);
  }
}

void
webkit_embed_prefs_shutdown (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (webkit_pref_entries); i++)
    eel_gconf_notification_remove (connections[i]);

  g_free (connections);
  g_object_unref (settings);
}

void
webkit_embed_prefs_add_embed (WebKitEmbed *embed)
{
  webkit_embed_prefs_apply (embed, settings);
}

