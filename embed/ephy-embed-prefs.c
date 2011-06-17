/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
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

#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-langs.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"

typedef struct
{
  char *schema;
  char *key;
  char *webkit_pref;
  void (*callback) (GSettings *settings, char *key, gpointer data);
} PrefData;

static WebKitWebSettings *webkit_settings = NULL;

static void
webkit_pref_callback_user_stylesheet (GSettings *settings,
                                      char *key,
                                      gpointer data)
{
  gboolean value = FALSE;
  char *uri = NULL;
  char *webkit_pref = data;

  value = g_settings_get_boolean (settings, key);

  if (value)
    /* We need the leading file://, so use g_strconcat instead
     * of g_build_filename */
    uri = g_strconcat ("file://",
                       ephy_dot_dir (),
                       G_DIR_SEPARATOR_S,
                       USER_STYLESHEET_FILENAME,
                       NULL);

  g_object_set (webkit_settings, webkit_pref, uri, NULL);
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

  g_object_get (webkit_settings, "user-agent", &webkit_user_agent, NULL);

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
webkit_pref_callback_user_agent (GSettings *settings,
                                 char *key,
                                 gpointer data)
{
  char *value = NULL;
  static char *internal_user_agent = NULL;
  char *webkit_pref = data;

  value = g_settings_get_string (settings, key);

  if (value == NULL || value[0] == '\0') {
    if (internal_user_agent == NULL)
      internal_user_agent = webkit_pref_get_internal_user_agent ();

    g_object_set (webkit_settings, webkit_pref, internal_user_agent, NULL);
  } else
    g_object_set (webkit_settings, webkit_pref, value, NULL);

  g_free (value);
}

static void
webkit_pref_callback_font_size (GSettings *settings,
                                char *key,
                                gpointer data)
{
  char *webkit_pref = data;
  char *value = NULL;
  int size = 9; /* FIXME: What to use here? */

  char *schema = NULL;
  g_object_get (settings, "schema", &schema, NULL);

  /* If we are changing a GNOME font value and we are not using GNOME fonts in
   * Epiphany, return. */
  if (g_strcmp0 (schema, EPHY_PREFS_WEB_SCHEMA) != 0 &&
      g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_USE_GNOME_FONTS) != TRUE)
  {
    g_free (schema);
    return;
  }
  g_free (schema);

  value = g_settings_get_string (settings, key);

  if (value) {
    PangoFontDescription* desc;

    desc = pango_font_description_from_string (value);
    size = pango_font_description_get_size (desc);
    if (pango_font_description_get_size_is_absolute (desc) == FALSE)
      size /= PANGO_SCALE;
    pango_font_description_free (desc);
  }

  g_object_set (webkit_settings, webkit_pref, size, NULL);
  g_free (value);
}

static void
webkit_pref_callback_font_family (GSettings *settings,
                                  char *key,
                                  gpointer data)
{
  char *webkit_pref = data;
  char *value = NULL;

  char *schema = NULL;
  g_object_get (settings, "schema", &schema, NULL);

  /* If we are changing a GNOME font value and we are not using GNOME fonts in
   * Epiphany, return. */
  if (g_strcmp0 (schema, EPHY_PREFS_WEB_SCHEMA) != 0 &&
      g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_USE_GNOME_FONTS) != TRUE)
  {
    g_free (schema);
    return;
  }
  g_free (schema);

  value = g_settings_get_string (settings, key);

  if (value) {
    PangoFontDescription* desc;
    const char *family = NULL;

    desc = pango_font_description_from_string (value);
    family = pango_font_description_get_family (desc);
    g_object_set (webkit_settings, webkit_pref, family, NULL);
    pango_font_description_free (desc);
  }

  g_free (value);
}

/* Part of this code taken from libsoup (soup-session.c) */
static gchar *
build_accept_languages_header (GArray *languages)
{
  gchar **langs = NULL;
  gchar *langs_str = NULL;
  gint delta;
  gint i;

  g_return_val_if_fail (languages != NULL, NULL);

  /* Calculate deltas for the quality values */
  if (languages->len < 10)
    delta = 10;
  else if (languages->len < 20)
    delta = 5;
  else
    delta = 1;

  /* Set quality values for each language */
  langs = (gchar **) languages->data;
  for (i = 0; langs[i] != NULL; i++) {
    gchar *lang = (gchar *) langs[i];
    gint quality = 100 - i * delta;

    if (quality > 0 && quality < 100) {
      gchar buf[8];
      g_ascii_formatd (buf, 8, "%.2f", quality/100.0);
      langs[i] = g_strdup_printf ("%s;q=%s", lang, buf);
    } else {
      /* Just dup the string in this case */
      langs[i] = g_strdup (lang);
    }
    g_free (lang);
  }

  /* Get the result string */
  if (languages->len > 0)
    langs_str = g_strjoinv (", ", langs);

  return langs_str;
}

/* Based on Christian Persch's code from gecko backend of epiphany
   (old transform_accept_languages_list() function) */
static void
webkit_pref_callback_accept_languages (GSettings *settings,
                                       char *key,
                                       gpointer data)
{
  SoupSession *session;
  GArray *array;
  char **languages;
  char *langs_str;
  char *webkit_pref;
  int i;

  webkit_pref = data;

  languages = g_settings_get_strv (settings, key);

  array = g_array_new (TRUE, FALSE, sizeof (char *));

  for (i = 0; languages[i]; i++) {
      if (!g_strcmp0 (languages[i], "system")) {
        ephy_langs_append_languages (array);
      } else if (languages[i][0] != '\0') {
        char *str = g_ascii_strdown (languages[i], -1);
        g_array_append_val (array, str);
      }
  }

  ephy_langs_sanitise (array);

  langs_str = build_accept_languages_header (array);

  /* Update Soup session */
  session = webkit_get_default_session ();
  g_object_set (G_OBJECT (session), webkit_pref, langs_str, NULL);

  g_strfreev (languages);
  g_free (langs_str);
  g_array_free (array, TRUE);
}

void
ephy_embed_prefs_set_cookie_jar_policy (SoupCookieJar *jar,
                                        const char *settings_policy)
{
  SoupCookieJarAcceptPolicy policy;

  g_return_if_fail (SOUP_IS_COOKIE_JAR (jar));
  g_return_if_fail (settings_policy != NULL);

  if (g_str_equal (settings_policy, "never"))
    policy = SOUP_COOKIE_JAR_ACCEPT_NEVER;
  else if (g_str_equal (settings_policy, "always"))
    policy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
  else if (g_str_equal (settings_policy, "no-third-party"))
    policy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
  else {
    g_warn_if_reached ();
    return;
  }

  g_object_set (G_OBJECT (jar), SOUP_COOKIE_JAR_ACCEPT_POLICY, policy, NULL);
}

static void
webkit_pref_callback_cookie_accept_policy (GSettings *settings,
                                           char *key,
                                           gpointer data)
{
  SoupSession *session;
  char *value = NULL;

  value = g_settings_get_string (settings, key);

  if (value) {
    SoupSessionFeature *jar;

    session = webkit_get_default_session ();
    jar = soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
    if (!jar) {
      g_free (value);
      return;
    }
    
    ephy_embed_prefs_set_cookie_jar_policy (SOUP_COOKIE_JAR (jar), value);
  }

  g_free (value);
}

static void
webkit_pref_callback_gnome_fonts (GSettings *ephy_settings,
                                  char *key,
                                  gpointer data)
{
  GSettings *settings;

  if (g_settings_get_boolean (ephy_settings, key)) {
    g_object_set (webkit_settings,
                  "default-font-family", "serif",
                  "sans-serif-font-family", "sans-serif",
                  "monospace-font-family", "monospace",
                  "default-font-size", 12,
                  "default-monospace-font-size", 10,
                  NULL);
  } else {
    /* Sync with Epiphany values */
    settings = ephy_settings;

    webkit_pref_callback_font_size (settings, EPHY_PREFS_WEB_SERIF_FONT,
                                    "default-font-size");
    webkit_pref_callback_font_size (settings, EPHY_PREFS_WEB_MONOSPACE_FONT,
                                    "default-monospace-font-size");

    webkit_pref_callback_font_family (settings, EPHY_PREFS_WEB_SERIF_FONT,
                                      "default-font-family");
    webkit_pref_callback_font_family (settings, EPHY_PREFS_WEB_SANS_SERIF_FONT,
                                      "sans-serif-font-family");
    webkit_pref_callback_font_family (settings, EPHY_PREFS_WEB_MONOSPACE_FONT,
                                      "monospace-font-family");
    webkit_pref_callback_font_family (settings, EPHY_PREFS_WEB_SERIF_FONT,
                                      "serif-font-family");
  }
}

static void
webkit_pref_callback_enable_spell_checking (GSettings *settings,
                                            char *key,
                                            gpointer data)
{
  gboolean value = FALSE;
  char **languages = NULL;
  char *langs = NULL;

  value = g_settings_get_boolean (settings, key);

  if (value) {
    languages = g_settings_get_strv (settings, EPHY_PREFS_WEB_LANGUAGE);
    langs = g_strjoinv (",", languages);
    g_strdelimit (langs, "-", '_');
    g_strfreev (languages);
  }

  g_object_set (webkit_settings, "enable-spell-checking", value, NULL);
  g_object_set (webkit_settings, "spell-checking-languages", langs, NULL);

  g_free (langs);
}

static const PrefData webkit_pref_entries[] =
  {
    /* Epiphany font settings */
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_SERIF_FONT,
      "default-font-size",
      webkit_pref_callback_font_size },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_MONOSPACE_FONT,
      "default-monospace-font-size",
      webkit_pref_callback_font_size },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_SERIF_FONT,
      "default-font-family",
      webkit_pref_callback_font_family },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_SANS_SERIF_FONT,
      "sans-serif-font-family",
      webkit_pref_callback_font_family },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_MONOSPACE_FONT,
      "monospace-font-family",
      webkit_pref_callback_font_family },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_SERIF_FONT,
      "serif-font-family",
      webkit_pref_callback_font_family },

    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_USE_GNOME_FONTS,
      NULL,
      webkit_pref_callback_gnome_fonts },

    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
      NULL,
      webkit_pref_callback_enable_spell_checking },

    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_ENABLE_USER_CSS,
      "user-stylesheet-uri",
      webkit_pref_callback_user_stylesheet },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_LANGUAGE,
      "accept-language",
      webkit_pref_callback_accept_languages },
    { EPHY_PREFS_SCHEMA,
      EPHY_PREFS_USER_AGENT,
      "user-agent",
      webkit_pref_callback_user_agent },
    { EPHY_PREFS_WEB_SCHEMA,
      EPHY_PREFS_WEB_COOKIES_POLICY,
      "accept-policy",
      webkit_pref_callback_cookie_accept_policy },
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

  webkit_settings = webkit_web_settings_new ();

  /* Hardcoded settings */
  g_object_set (webkit_settings,
                "enable-default-context-menu", FALSE,
                "enable-site-specific-quirks", TRUE,
                "enable-page-cache", TRUE,
                "enable-developer-extras", TRUE,
                NULL);

  for (i = 0; i < G_N_ELEMENTS (webkit_pref_entries); i++) {
    GSettings *settings;
    char *key;

    settings = ephy_settings_get (webkit_pref_entries[i].schema);
    key = g_strconcat ("changed::", webkit_pref_entries[i].key, NULL);

    webkit_pref_entries[i].callback (settings,
                                     webkit_pref_entries[i].key,
                                     webkit_pref_entries[i].webkit_pref);

    g_signal_connect (settings, key,
                      G_CALLBACK (webkit_pref_entries[i].callback),
                      webkit_pref_entries[i].webkit_pref);
    g_free (key);
  }

  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_JAVASCRIPT,
                   webkit_settings, "enable-scripts",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_MAIN,
                   EPHY_PREFS_ENABLE_CARET_BROWSING,
                   webkit_settings, "enable-caret-browsing",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   webkit_settings, "javascript-can-open-windows-automatically",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_PLUGINS,
                   webkit_settings, "enable-plugins",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_FONT_MIN_SIZE,
                   webkit_settings, "minimum-font-size",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_DEFAULT_ENCODING,
                   webkit_settings, "default-encoding",
                   G_SETTINGS_BIND_GET);
}

void
ephy_embed_prefs_shutdown (void)
{
  g_object_unref (webkit_settings);
}

void
ephy_embed_prefs_add_embed (EphyEmbed *embed)
{
  ephy_embed_prefs_apply (embed, webkit_settings);
}

