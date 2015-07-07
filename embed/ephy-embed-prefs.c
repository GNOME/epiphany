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
#include "ephy-embed-prefs.h"

#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-langs.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"

#include <glib.h>
#include <math.h>
#include <webkit2/webkit2.h>

typedef struct
{
  char *schema;
  char *key;
  char *webkit_pref;
  void (*callback) (GSettings *settings, char *key, gpointer data);
} PrefData;

#define ENABLE_SCRIPTS_SETTING "enable-javascript"
#define DEFAULT_ENCODING_SETTING "default-charset"
static WebKitSettings *webkit_settings = NULL;

static void
user_style_sheet_output_stream_splice_cb (GOutputStream *output_stream,
                                          GAsyncResult *result,
                                          gpointer user_data)
{
  gssize bytes;

  bytes = g_output_stream_splice_finish (output_stream, result, NULL);
  if (bytes > 0) {
    WebKitUserStyleSheet *style_sheet;

    style_sheet = webkit_user_style_sheet_new (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream)),
                                               WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER,
                                               NULL, NULL);
    webkit_user_content_manager_add_style_sheet (WEBKIT_USER_CONTENT_MANAGER (ephy_embed_shell_get_user_content_manager (ephy_embed_shell_get_default ())),
                                                 style_sheet);
    webkit_user_style_sheet_unref (style_sheet);
  }
}

static void
user_style_seet_read_cb (GFile *file,
                         GAsyncResult *result,
                         gpointer user_data)
{
  GFileInputStream *input_stream;
  GOutputStream *output_stream;

  input_stream = g_file_read_finish (file, result, NULL);
  if (!input_stream)
    return;

  output_stream = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (output_stream, G_INPUT_STREAM (input_stream),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                (GAsyncReadyCallback)user_style_sheet_output_stream_splice_cb,
                                NULL);
  g_object_unref (input_stream);
  g_object_unref (output_stream);
}

static void
webkit_pref_callback_user_stylesheet (GSettings *settings,
                                      char *key,
                                      gpointer data)
{
  gboolean value;

  value = g_settings_get_boolean (settings, key);

  if (!value)
    webkit_user_content_manager_remove_all_style_sheets (WEBKIT_USER_CONTENT_MANAGER (ephy_embed_shell_get_user_content_manager (ephy_embed_shell_get_default ())));
  else {
    GFile *file;
    char *filename;

    filename = g_build_filename (ephy_dot_dir (), USER_STYLESHEET_FILENAME, NULL);
    file = g_file_new_for_path (filename);
    g_free (filename);

    g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
                       (GAsyncReadyCallback)user_style_seet_read_cb, NULL);
    g_object_unref (file);
  }
}

static char *
webkit_pref_get_vendor_user_agent (void)
{
  char *vendor_user_agent = NULL;
  GKeyFile *branding_keyfile;

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

  return vendor_user_agent;
}

static const char *
webkit_pref_get_internal_user_agent (void)
{
  static char *user_agent = NULL;
  static gboolean initialized = FALSE;
  char *vendor_user_agent;
  const char *webkit_user_agent;

  if (initialized)
    return user_agent;

  initialized = TRUE;

  vendor_user_agent = webkit_pref_get_vendor_user_agent ();
  if (!vendor_user_agent)
    return NULL;

  webkit_user_agent = webkit_settings_get_user_agent (webkit_settings);
  user_agent = g_strdup_printf ("%s %s Epiphany/%s", webkit_user_agent,
                                vendor_user_agent, VERSION);
  g_free (vendor_user_agent);

  return user_agent;
}

static void
webkit_pref_callback_user_agent (GSettings *settings,
                                 char *key,
                                 gpointer data)
{
  char *value;
  const char *internal_user_agent;

  value = g_settings_get_string (settings, key);
  if (value != NULL && value[0] != '\0') {
    webkit_settings_set_user_agent (webkit_settings, value);
    g_free (value);
    return;
  }
  g_free (value);

  internal_user_agent = webkit_pref_get_internal_user_agent ();
  if (internal_user_agent)
    webkit_settings_set_user_agent (webkit_settings, internal_user_agent);
  else
    webkit_settings_set_user_agent_with_application_details (webkit_settings,
                                                             "Epiphany", VERSION);
}

static gdouble
get_screen_dpi (GdkScreen *screen)
{
  gdouble dpi;
  gdouble dp, di;

  dpi = gdk_screen_get_resolution (screen);
  if (dpi != -1)
    return dpi;

  dp = hypot (gdk_screen_get_width (screen), gdk_screen_get_height (screen));
  di = hypot (gdk_screen_get_width_mm (screen), gdk_screen_get_height_mm (screen)) / 25.4;

  return dp / di;
}

static guint
normalize_font_size (gdouble font_size)
{
  /* WebKit2 uses font sizes in pixels. */
  GdkScreen *screen;
  gdouble dpi;

  /* FIXME: We should use the view screen instead of the detault one
   * but we don't have access to the view here.
   */
  screen = gdk_screen_get_default ();
  dpi = screen ? get_screen_dpi (screen) : 96;

  return font_size / 72.0 * dpi;
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
  g_object_get (settings, "schema-id", &schema, NULL);

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

  g_object_set (webkit_settings, webkit_pref, normalize_font_size (size), NULL);
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
  g_object_get (settings, "schema-id", &schema, NULL);

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

static char **
normalize_languages (char **languages)
{
  int i;
  GPtrArray *langs;

  langs = g_ptr_array_new ();

  for (i = 0; languages && languages[i]; i++) {
    if (g_str_equal (languages[i], "system")) {
      char **sys_langs = ephy_langs_get_languages ();
      int j;

      for (j = 0; sys_langs && sys_langs[j]; j++)
        g_ptr_array_add (langs, g_strdelimit (g_strdup (sys_langs[i]), "-", '_'));

      g_strfreev (sys_langs);
    } else
      g_ptr_array_add (langs, g_strdelimit (g_strdup (languages[i]), "-", '_'));
  }

  g_ptr_array_add (langs, NULL);

  return (char **)g_ptr_array_free (langs, FALSE);
}

/* Based on Christian Persch's code from gecko backend of epiphany
   (old transform_accept_languages_list() function) */
static void
webkit_pref_callback_accept_languages (GSettings *settings,
                                       char *key,
                                       gpointer data)
{
  GArray *array;
  char **languages;
  int i;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *web_context = ephy_embed_shell_get_web_context (shell);

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

  webkit_web_context_set_preferred_languages (web_context, (const char * const *)(void *)array->data);

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING)) {
    char **normalized = normalize_languages ((char **)(void *)array->data);
    webkit_web_context_set_spell_checking_languages (web_context, (const char * const *)normalized);
    g_strfreev (normalized);
  }

  g_strfreev (languages);
  g_array_free (array, TRUE);
}


void
ephy_embed_prefs_set_cookie_accept_policy (WebKitCookieManager *cookie_manager,
                                           const char *settings_policy)
{
  WebKitCookieAcceptPolicy policy;

  if (g_str_equal (settings_policy, "never"))
    policy = WEBKIT_COOKIE_POLICY_ACCEPT_NEVER;
  else if (g_str_equal (settings_policy, "always"))
    policy = WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
  else if (g_str_equal (settings_policy, "no-third-party"))
    policy = WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;
  else {
    g_warn_if_reached ();
    return;
  }

  webkit_cookie_manager_set_accept_policy (cookie_manager, policy);
}

static void
webkit_pref_callback_cookie_accept_policy (GSettings *settings,
                                           char *key,
                                           gpointer data)
{
  WebKitCookieManager *cookie_manager;
  char *value;
  EphyEmbedShell *shell;

  value = g_settings_get_string (settings, key);
  if (!value)
    return;

  shell = ephy_embed_shell_get_default ();
  cookie_manager = webkit_web_context_get_cookie_manager (ephy_embed_shell_get_web_context (shell));
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, value);
  g_free (value);
}

static void
ephy_embed_prefs_update_font_settings (GSettings *ephy_settings, char *key)
{
  if (g_settings_get_boolean (ephy_settings, key)) {
    g_object_set (webkit_settings,
                  "default-font-family", "serif",
                  "sans-serif-font-family", "sans-serif",
                  "monospace-font-family", "monospace",
                  "default-font-size", normalize_font_size (12),
                  "default-monospace-font-size", normalize_font_size (10),
                  NULL);
  } else {
    /* Sync with Epiphany values */
    webkit_pref_callback_font_size (ephy_settings, EPHY_PREFS_WEB_SERIF_FONT,
                                    "default-font-size");
    webkit_pref_callback_font_size (ephy_settings, EPHY_PREFS_WEB_MONOSPACE_FONT,
                                    "default-monospace-font-size");

    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_SERIF_FONT,
                                      "default-font-family");
    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_SANS_SERIF_FONT,
                                      "sans-serif-font-family");
    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_MONOSPACE_FONT,
                                      "monospace-font-family");
    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_SERIF_FONT,
                                      "serif-font-family");
  }
}

static void
webkit_pref_callback_gnome_fonts (GSettings *ephy_settings,
                                  char *key,
                                  gpointer data)
{
  ephy_embed_prefs_update_font_settings (ephy_settings, key);
}

static void
webkit_pref_callback_enable_spell_checking (GSettings *settings,
                                            char *key,
                                            gpointer data)
{
  WebKitWebContext *web_context;
  gboolean value = FALSE;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  web_context = ephy_embed_shell_get_web_context (shell);
  value = g_settings_get_boolean (settings, key);

  webkit_web_context_set_spell_checking_enabled (web_context, value);

  if (value) {
    char **languages = g_settings_get_strv (settings, EPHY_PREFS_WEB_LANGUAGE);
    char **normalized = normalize_languages (languages);

    webkit_web_context_set_spell_checking_languages (web_context, (const char* const *)normalized);

    g_strfreev (languages);
    g_strfreev (normalized);
  }
}

static void
gtk_settings_xft_dpi_changed_cb (GtkSettings *gtk_settings,
                                 GParamSpec *pspec,
                                 gpointer data)
{
  GSettings *gsettings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  ephy_embed_prefs_update_font_settings (gsettings, EPHY_PREFS_WEB_USE_GNOME_FONTS);
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

static gpointer
ephy_embed_prefs_init (gpointer user_data)
{
  GtkSettings *gtk_settings;
  int i;

  webkit_settings = webkit_settings_new_with_settings ("enable-developer-extras", TRUE,
                                                       "enable-fullscreen", TRUE,
                                                       "enable-site-specific-quirks", TRUE,
                                                       "enable-dns-prefetching", TRUE,
                                                       "javascript-can-open-windows-automatically", TRUE,
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

  /* Connect to the "notify::gtk-xft-dpi" signal for GtkSettings, so that
   * we can update the font size in real time if the screen's resolution
   * for font handling changes (e.g. enabled "Large Text" a11y mode).
   */
  gtk_settings = gtk_settings_get_default ();
  if (gtk_settings) {
    g_signal_connect (gtk_settings, "notify::gtk-xft-dpi",
                      G_CALLBACK (gtk_settings_xft_dpi_changed_cb), NULL);
  }

  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_JAVASCRIPT,
                   webkit_settings, ENABLE_SCRIPTS_SETTING,
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_MAIN,
                   EPHY_PREFS_ENABLE_CARET_BROWSING,
                   webkit_settings, "enable-caret-browsing",
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
                   webkit_settings, DEFAULT_ENCODING_SETTING,
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_WEBGL,
                   webkit_settings, "enable-webgl",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_WEBAUDIO,
                   webkit_settings, "enable-webaudio",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_MAIN,
                   EPHY_PREFS_ENABLE_SMOOTH_SCROLLING,
                   webkit_settings, "enable-smooth-scrolling",
                   G_SETTINGS_BIND_GET);
  return webkit_settings;
}

WebKitSettings *
ephy_embed_prefs_get_settings (void)
{
  static GOnce once_init = G_ONCE_INIT;

  return g_once (&once_init, ephy_embed_prefs_init, NULL);
}
