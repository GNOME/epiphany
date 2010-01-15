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

#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-langs.h"

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

static char *
webkit_pref_get_internal_user_agent (void)
{
  char *user_agent;
  char *webkit_user_agent;
  char *vendor_user_agent;
  GKeyFile *branding_keyfile;

  vendor_user_agent = NULL;
  branding_keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (branding_keyfile, SHARE_DIR"/branding.conf",
                                 G_KEY_FILE_NONE, NULL)) {
    char *vendor;
    char *vendor_sub;
    char *vendor_comment;

    vendor = g_key_file_get_string (branding_keyfile,
                                    "User Agent", "Vendor", NULL);
    vendor_sub = g_key_file_get_string (branding_keyfile,
                                        "User Agent", "VendorSub", NULL);
    vendor_comment = g_key_file_get_string (branding_keyfile,
                                            "User Agent", "VendorComment", NULL);

    if (vendor) {
      vendor_user_agent = g_strconcat (vendor,
                                       vendor_sub ? "/" : "",
                                       vendor_sub ? vendor_sub : "",
                                       vendor_comment ? " (" : "",
                                       vendor_comment ? vendor_comment : "",
                                       vendor_comment ? ")" : "",
                                       NULL);
    }

    g_free (vendor);
    g_free (vendor_sub);
    g_free (vendor_comment);
  }

  g_key_file_free (branding_keyfile);

  g_object_get (settings, "user-agent", &webkit_user_agent, NULL);

  user_agent = g_strconcat (webkit_user_agent, " ",
                            vendor_user_agent ? vendor_user_agent : "",
                            vendor_user_agent ? " " : "",
                            "Epiphany/"VERSION,
                            NULL);

  g_free (vendor_user_agent);
  g_free (webkit_user_agent);

  return user_agent;
}

static void
webkit_pref_callback_user_agent (GConfClient *client,
                                 guint cnxn_id,
                                 GConfEntry *entry,
                                 gpointer data)
{
  GConfValue *gcvalue;
  const char *value = NULL;
  static char *internal_user_agent = NULL;
  char *webkit_pref = data;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_STRING) {
      value = gconf_value_get_string (gcvalue);
  }

  if (value == NULL || value[0] == '\0') {
    if (internal_user_agent == NULL)
      internal_user_agent = webkit_pref_get_internal_user_agent ();

    g_object_set (settings, webkit_pref, internal_user_agent, NULL);
  } else
    g_object_set (settings, webkit_pref, value, NULL);
}

static void
webkit_pref_callback_font_size (GConfClient *client,
                                guint cnxn_id,
                                GConfEntry *entry,
                                gpointer data)
{
  GConfValue *gcvalue;
  char *webkit_pref = data;
  const char *value = NULL;
  int size = 9; /* FIXME: What to use here? */

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_STRING) {
      value = gconf_value_get_string (gcvalue);
  }

  if (value) {
    PangoFontDescription* desc;

    desc = pango_font_description_from_string (value);
    size = pango_font_description_get_size (desc);
    if (pango_font_description_get_size_is_absolute (desc) == FALSE)
      size /= PANGO_SCALE;
    pango_font_description_free (desc);
  }

  g_object_set (settings, webkit_pref, size, NULL);
}

static void
webkit_pref_callback_font_family (GConfClient *client,
                                  guint cnxn_id,
                                  GConfEntry *entry,
                                  gpointer data)
{
  GConfValue *gcvalue;
  char *webkit_pref = data;
  const char *value = NULL;

  gcvalue = gconf_entry_get_value (entry);

  /* happens on initial notify if the key doesn't exist */
  if (gcvalue != NULL &&
      gcvalue->type == GCONF_VALUE_STRING) {
      value = gconf_value_get_string (gcvalue);
  }

  if (value) {
    PangoFontDescription* desc;
    const char *family = NULL;

    desc = pango_font_description_from_string (value);
    family = pango_font_description_get_family (desc);
    g_object_set (settings, webkit_pref, family, NULL);
    pango_font_description_free (desc);
  }
}

/* Based on Christian Persch's code from gecko backend of epiphany
   (old transform_accept_languages_list() function) */
static void
webkit_pref_callback_accept_languages (GConfClient *client,
                                       guint cnxn_id,
                                       GConfEntry *entry,
                                       gpointer data)
{
  SoupSession *session;
  GConfValue *gcvalue;
  GArray *array;
  GSList *languages, *l;
  char **langs;
  char *langs_str;
  char *webkit_pref;

  webkit_pref = data;
  gcvalue = gconf_entry_get_value (entry);
  if (gcvalue == NULL ||
      gcvalue->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (gcvalue) != GCONF_VALUE_STRING)
    return;

  languages = gconf_value_get_list (gcvalue);

  array = g_array_new (TRUE, FALSE, sizeof (char *));

  for (l = languages; l != NULL; l = l->next) {
      const char *lang = gconf_value_get_string ((GConfValue *) l->data);

      if ((lang != NULL) && !g_strcmp0 (lang, "system")) {
          ephy_langs_append_languages (array);
        } else if (lang != NULL && lang[0] != '\0') {
          char *str = g_ascii_strdown (lang, -1);
          g_array_append_val (array, str);
        }
    }

  ephy_langs_sanitise (array);

  langs = (char **) g_array_free (array, FALSE);
  langs_str = g_strjoinv (", ", langs);

  /* Update Soup session */
  session = webkit_get_default_session ();
  g_object_set (G_OBJECT (session), webkit_pref, langs_str, NULL);

  g_strfreev (langs);
  g_free (langs_str);
}

static const PrefData webkit_pref_entries[] =
  {
    { CONF_RENDERING_FONT_MIN_SIZE,
      "minimum-logical-font-size",
      webkit_pref_callback_int },
    { CONF_DESKTOP_FONT_VAR_NAME,
      "default-font-size",
      webkit_pref_callback_font_size },
    { CONF_DESKTOP_FONT_VAR_NAME,
      "default-font-family",
      webkit_pref_callback_font_family },
    { CONF_DESKTOP_FONT_VAR_NAME,
      "sans-serif-font-family",
      webkit_pref_callback_font_family },
    { CONF_DESKTOP_FONT_FIXED_NAME,
      "default-monospace-font-size",
      webkit_pref_callback_font_size },
    { CONF_DESKTOP_FONT_FIXED_NAME,
      "monospace-font-family",
      webkit_pref_callback_font_family },
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
      webkit_pref_callback_user_stylesheet },
    { CONF_CARET_BROWSING_ENABLED,
      "enable-caret-browsing",
      webkit_pref_callback_boolean },
    { CONF_SECURITY_ALLOW_POPUPS,
      "javascript-can-open-windows-automatically",
      webkit_pref_callback_boolean },
    { CONF_RENDERING_LANGUAGE,
      "accept-language",
      webkit_pref_callback_accept_languages },
    { CONF_USER_AGENT,
      "user-agent",
      webkit_pref_callback_user_agent }
  };

static void
ephy_embed_prefs_apply (EphyEmbed *embed, WebKitWebSettings *settings)
{
  webkit_web_view_set_settings (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed),
                                settings);
}

void
ephy_embed_prefs_init (void)
{
  int i;

  eel_gconf_monitor_add ("/apps/epiphany/web");

  settings = webkit_web_settings_new ();

  /* Hardcoded settings */
  g_object_set (settings,
                "auto-shrink-images", FALSE,
                "enable-default-context-menu", FALSE,
                "enable-site-specific-quirks", TRUE,
                NULL);

  /* Connections */
  connections = g_malloc (sizeof (guint) * G_N_ELEMENTS (webkit_pref_entries));

  for (i = 0; i < G_N_ELEMENTS (webkit_pref_entries); i++) {
    connections[i] = eel_gconf_notification_add (webkit_pref_entries[i].gconf_key,
                                                 webkit_pref_entries[i].func,
                                                 webkit_pref_entries[i].webkit_pref);

    eel_gconf_notify (webkit_pref_entries[i].gconf_key);
  }
}

void
ephy_embed_prefs_shutdown (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (webkit_pref_entries); i++)
    eel_gconf_notification_remove (connections[i]);

  g_free (connections);
  g_object_unref (settings);
}

void
ephy_embed_prefs_add_embed (EphyEmbed *embed)
{
  ephy_embed_prefs_apply (embed, settings);
}

