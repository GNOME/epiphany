/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2008 Xan Lopez <xan@gnome.org>
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

#include <config.h>
#include "ephy-embed-prefs.h"

#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-langs.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-user-agent.h"

typedef struct {
  const char *schema;
  const char *key;
  const char *webkit_pref;
  void (*callback)(GSettings  *settings,
                   const char *key,
                   gpointer    data);
} PrefData;

#define DEFAULT_ENCODING_SETTING "default-charset"
/* FIXME: Refactor this code to remove the need of those globals */
static WebKitSettings *webkit_settings = NULL;
static GFileMonitor *user_style_sheet_monitor = NULL;
static WebKitUserStyleSheet *style_sheet = NULL;
static GFileMonitor *user_javascript_monitor = NULL;
static WebKitUserScript *javascript = NULL;
static GList *ucm_list = NULL;

static void
update_user_style_on_all_ucm (void)
{
  GList *list = NULL;

  for (list = ucm_list; list; list = list->next) {
    WebKitUserContentManager *ucm = list->data;

    webkit_user_content_manager_remove_all_style_sheets (ucm);
    if (style_sheet)
      webkit_user_content_manager_add_style_sheet (ucm, style_sheet);
  }
}

static void
user_style_sheet_output_stream_splice_cb (GOutputStream *output_stream,
                                          GAsyncResult  *result,
                                          gpointer       user_data)
{
  gssize bytes;

  g_clear_pointer (&style_sheet, webkit_user_style_sheet_unref);

  bytes = g_output_stream_splice_finish (output_stream, result, NULL);
  if (bytes > 0) {
    style_sheet = webkit_user_style_sheet_new (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream)),
                                               WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER,
                                               NULL, NULL);
  }

  update_user_style_on_all_ucm ();

  g_object_unref (output_stream);
}

static void
user_style_sheet_read_cb (GFile        *file,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GFileInputStream) input_stream = NULL;
  g_autoptr (GOutputStream) output_stream = NULL;

  input_stream = g_file_read_finish (file, result, NULL);
  if (!input_stream)
    return;

  output_stream = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (g_steal_pointer (&output_stream),
                                G_INPUT_STREAM (input_stream),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                (GAsyncReadyCallback)user_style_sheet_output_stream_splice_cb,
                                NULL);
}

static void
user_style_sheet_file_changed (GFileMonitor      *monitor,
                               GFile             *file,
                               GFile             *other_file,
                               GFileMonitorEvent  event_type,
                               gpointer           user_data)
{
  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
    g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
                       (GAsyncReadyCallback)user_style_sheet_read_cb, NULL);
  }
}

static void
webkit_pref_callback_user_stylesheet (GSettings  *settings,
                                      const char *key,
                                      gpointer    data)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  gboolean value;
  GError *error = NULL;

  value = g_settings_get_boolean (settings, key);

  if (user_style_sheet_monitor) {
    g_signal_handlers_disconnect_by_func (user_style_sheet_monitor, user_style_sheet_file_changed, NULL);
    g_clear_object (&user_style_sheet_monitor);
  }

  g_clear_pointer (&style_sheet, webkit_user_style_sheet_unref);

  if (!value) {
    update_user_style_on_all_ucm ();
    return;
  }

  filename = g_build_filename (ephy_profile_dir (), USER_STYLESHEET_FILENAME, NULL);
  file = g_file_new_for_path (filename);

  g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
                     (GAsyncReadyCallback)user_style_sheet_read_cb, NULL);

  user_style_sheet_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (!user_style_sheet_monitor) {
    g_warning ("Could not create a file monitor for %s: %s", g_file_get_uri (file), error->message);
    g_error_free (error);
  } else {
    g_signal_connect (user_style_sheet_monitor, "changed", G_CALLBACK (user_style_sheet_file_changed), NULL);
  }
}

static void
update_user_javascript_on_all_ucm (void)
{
  GList *list = NULL;

  for (list = ucm_list; list; list = list->next) {
    WebKitUserContentManager *ucm = list->data;

    webkit_user_content_manager_remove_all_scripts (ucm);
    if (javascript)
      webkit_user_content_manager_add_script (ucm, javascript);
  }
}

static void
user_javascript_output_stream_splice_cb (GOutputStream *output_stream,
                                         GAsyncResult  *result,
                                         gpointer       user_data)
{
  gssize bytes;

  g_clear_pointer (&javascript, webkit_user_script_unref);

  bytes = g_output_stream_splice_finish (output_stream, result, NULL);
  if (bytes > 0) {
    javascript = webkit_user_script_new (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream)),
                                         WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                         NULL, NULL);
  }

  update_user_javascript_on_all_ucm ();

  g_object_unref (output_stream);
}

static void
user_javascript_read_cb (GFile        *file,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr (GFileInputStream) input_stream = NULL;
  g_autoptr (GOutputStream) output_stream = NULL;

  input_stream = g_file_read_finish (file, result, NULL);
  if (!input_stream)
    return;

  output_stream = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (g_steal_pointer (&output_stream),
                                G_INPUT_STREAM (input_stream),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                (GAsyncReadyCallback)user_javascript_output_stream_splice_cb,
                                NULL);
}

static void
user_javascript_file_changed (GFileMonitor      *monitor,
                              GFile             *file,
                              GFile             *other_file,
                              GFileMonitorEvent  event_type,
                              gpointer           user_data)
{
  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
    g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
                       (GAsyncReadyCallback)user_javascript_read_cb, NULL);
  }
}

static void
webkit_pref_callback_user_javascript (GSettings  *settings,
                                      const char *key,
                                      gpointer    data)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  gboolean value;
  g_autoptr (GError) error = NULL;

  value = g_settings_get_boolean (settings, key);

  if (user_javascript_monitor) {
    g_signal_handlers_disconnect_by_func (user_javascript_monitor, user_javascript_file_changed, NULL);
    g_clear_object (&user_style_sheet_monitor);
  }

  if (!value) {
    update_user_javascript_on_all_ucm ();
    return;
  }

  filename = g_build_filename (ephy_profile_dir (), USER_JAVASCRIPT_FILENAME, NULL);
  file = g_file_new_for_path (filename);

  g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
                     (GAsyncReadyCallback)user_javascript_read_cb, NULL);

  user_javascript_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (!user_javascript_monitor)
    g_warning ("Could not create a file monitor for %s: %s", g_file_get_uri (file), error->message);
  else
    g_signal_connect (user_javascript_monitor, "changed", G_CALLBACK (user_javascript_file_changed), NULL);
}

static void
webkit_pref_callback_user_agent (GSettings  *settings,
                                 const char *key,
                                 gpointer    data)
{
  webkit_settings_set_user_agent (webkit_settings,
                                  ephy_user_agent_get ());
}

static void
webkit_pref_callback_font_size (GSettings  *settings,
                                const char *key,
                                gpointer    data)
{
  char *webkit_pref = data;
  char *value = NULL;
  int size = 12;

  value = g_settings_get_string (settings, key);

  if (value) {
    PangoFontDescription *desc;

    desc = pango_font_description_from_string (value);
    size = pango_font_description_get_size (desc);
    if (!pango_font_description_get_size_is_absolute (desc))
      size /= PANGO_SCALE;
    pango_font_description_free (desc);
  }

  g_object_set (webkit_settings, webkit_pref, webkit_settings_font_size_to_pixels (size), NULL);
  g_free (value);
}

static void
webkit_pref_callback_font_family (GSettings  *settings,
                                  const char *key,
                                  gpointer    data)
{
  char *webkit_pref = data;
  char *value = NULL;

  value = g_settings_get_string (settings, key);

  if (value) {
    PangoFontDescription *desc;
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
    if (!strcmp (languages[i], "system")) {
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
 *  (old transform_accept_languages_list() function) */
static void
webkit_pref_callback_accept_languages (GSettings  *settings,
                                       const char *key,
                                       gpointer    data)
{
  GArray *array;
  char **languages;
  guint i;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *web_context = ephy_embed_shell_get_web_context (shell);

  languages = g_settings_get_strv (settings, key);

  array = g_array_new (TRUE, FALSE, sizeof (char *));

  for (i = 0; languages[i]; i++) {
    if (!g_strcmp0 (languages[i], "system")) {
      ephy_langs_append_languages (array);
    } else if (languages[i][0] != '\0') {
      char *str = g_strdup (languages[i]);
      g_array_append_val (array, str);
    }
  }
  g_strfreev (languages);

  ephy_langs_sanitise (array);

  webkit_web_context_set_preferred_languages (web_context, (const char * const *)(void *)array->data);
  /* Used by the Firefox Sync web view in prefs-dialog.c. */
  g_object_set_data_full (G_OBJECT (web_context), "preferred-languages",
                          g_strdupv ((char **)(void *)array->data),
                          (GDestroyNotify)g_strfreev);

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING)) {
    char **normalized = normalize_languages ((char **)(void *)array->data);
    webkit_web_context_set_spell_checking_languages (web_context, (const char * const *)normalized);
    g_strfreev (normalized);
  }

  for (i = 0; i < array->len; i++)
    g_free (g_array_index (array, char *, i));
  g_array_free (array, TRUE);
}

static void
webkit_pref_callback_gnome_fonts (GSettings  *ephy_settings,
                                  const char *key,
                                  gpointer    data)
{
  if (g_settings_get_boolean (ephy_settings, key)) {
    g_object_set (webkit_settings,
                  "default-font-family", "serif",
                  "serif-font-family", "serif",
                  "sans-serif-font-family", "sans-serif",
                  "monospace-font-family", "monospace",
                  "default-font-size", webkit_settings_font_size_to_pixels (12),
                  "default-monospace-font-size", webkit_settings_font_size_to_pixels (10),
                  NULL);
  } else {
    /* Sync with Epiphany values */
    webkit_pref_callback_font_size (ephy_settings, EPHY_PREFS_WEB_SERIF_FONT,
                                    (gpointer)"default-font-size");
    webkit_pref_callback_font_size (ephy_settings, EPHY_PREFS_WEB_MONOSPACE_FONT,
                                    (gpointer)"default-monospace-font-size");

    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_SERIF_FONT,
                                      (gpointer)"default-font-family");
    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_SANS_SERIF_FONT,
                                      (gpointer)"sans-serif-font-family");
    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_MONOSPACE_FONT,
                                      (gpointer)"monospace-font-family");
    webkit_pref_callback_font_family (ephy_settings, EPHY_PREFS_WEB_SERIF_FONT,
                                      (gpointer)"serif-font-family");
  }
}

static void
webkit_pref_callback_enable_spell_checking (GSettings  *settings,
                                            const char *key,
                                            gpointer    data)
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

    webkit_web_context_set_spell_checking_languages (web_context, (const char * const *)normalized);

    g_strfreev (languages);
    g_strfreev (normalized);
  }
}

static void
webkit_pref_callback_enable_website_data_storage (GSettings  *settings,
                                                  const char *key,
                                                  gpointer    data)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitNetworkSession *network_session;
  WebKitCookieManager *manager;
  gboolean value;

  value = g_settings_get_boolean (settings, key);
  webkit_settings_set_enable_html5_database (webkit_settings, value);
  webkit_settings_set_enable_html5_local_storage (webkit_settings, value);

  network_session = ephy_embed_shell_get_network_session (shell);
  manager = webkit_network_session_get_cookie_manager (network_session);
  webkit_cookie_manager_set_accept_policy (manager, value ? WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS : WEBKIT_COOKIE_POLICY_ACCEPT_NEVER);
}

static void
webkit_pref_callback_hardware_acceleration_policy (GSettings  *settings,
                                                   const char *key,
                                                   gpointer    data)
{
  EphyPrefsWebHardwareAccelerationPolicy value;

  value = g_settings_get_enum (settings, key);
  switch (value) {
    case EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY_NEVER:
      webkit_settings_set_hardware_acceleration_policy (webkit_settings,
                                                        WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
      break;
    case EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY_ALWAYS:
    /* Fallthrough */
    default:
      webkit_settings_set_hardware_acceleration_policy (webkit_settings,
                                                        WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);
      break;
  }
}

static const PrefData webkit_pref_entries[] = {
  /* Epiphany font settings */
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_SERIF_FONT,
    "default-font-size",
    webkit_pref_callback_font_size
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_MONOSPACE_FONT,
    "default-monospace-font-size",
    webkit_pref_callback_font_size
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_SERIF_FONT,
    "default-font-family",
    webkit_pref_callback_font_family
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_SANS_SERIF_FONT,
    "sans-serif-font-family",
    webkit_pref_callback_font_family
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_MONOSPACE_FONT,
    "monospace-font-family",
    webkit_pref_callback_font_family
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_SERIF_FONT,
    "serif-font-family",
    webkit_pref_callback_font_family
  },

  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_USE_GNOME_FONTS,
    NULL,
    webkit_pref_callback_gnome_fonts
  },

  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
    NULL,
    webkit_pref_callback_enable_spell_checking
  },

  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_ENABLE_USER_CSS,
    "user-stylesheet-uri",
    webkit_pref_callback_user_stylesheet
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_ENABLE_USER_JS,
    "user-javascript-uri",
    webkit_pref_callback_user_javascript
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_LANGUAGE,
    "accept-language",
    webkit_pref_callback_accept_languages
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_USER_AGENT,
    "user-agent",
    webkit_pref_callback_user_agent
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_ENABLE_WEBSITE_DATA_STORAGE,
    NULL,
    webkit_pref_callback_enable_website_data_storage
  },
  {
    EPHY_PREFS_WEB_SCHEMA,
    EPHY_PREFS_WEB_HARDWARE_ACCELERATION_POLICY,
    "hardware-acceleration-policy",
    webkit_pref_callback_hardware_acceleration_policy
  },
};

static gpointer
ephy_embed_prefs_init (gpointer user_data)
{
  guint i;

  webkit_settings = webkit_settings_new_with_settings ("enable-developer-extras", TRUE,
                                                       "enable-media-stream", TRUE,
                                                       "enable-smooth-scrolling", TRUE,
                                                       "javascript-can-open-windows-automatically", TRUE,
                                                       NULL);

  for (i = 0; i < G_N_ELEMENTS (webkit_pref_entries); i++) {
    GSettings *settings;
    char *key;

    settings = ephy_settings_get (webkit_pref_entries[i].schema);
    key = g_strconcat ("changed::", webkit_pref_entries[i].key, NULL);

    webkit_pref_entries[i].callback (settings,
                                     webkit_pref_entries[i].key,
                                     (gpointer)webkit_pref_entries[i].webkit_pref);

    g_signal_connect (settings, key,
                      G_CALLBACK (webkit_pref_entries[i].callback),
                      (gpointer)webkit_pref_entries[i].webkit_pref);
    g_free (key);
  }

  g_settings_bind (EPHY_SETTINGS_MAIN,
                   EPHY_PREFS_ENABLE_CARET_BROWSING,
                   webkit_settings, "enable-caret-browsing",
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
                   EPHY_PREFS_WEB_ENABLE_SITE_SPECIFIC_QUIRKS,
                   webkit_settings, "enable-site-specific-quirks",
                   G_SETTINGS_BIND_GET);
  g_settings_bind (EPHY_SETTINGS_WEB,
                   EPHY_PREFS_WEB_ENABLE_NAVIGATION_GESTURES,
                   webkit_settings, "enable-back-forward-navigation-gestures",
                   G_SETTINGS_BIND_GET);

  return webkit_settings;
}

WebKitSettings *
ephy_embed_prefs_get_settings (void)
{
  static GOnce once_init = G_ONCE_INIT;

  return g_once (&once_init, ephy_embed_prefs_init, NULL);
}

void
ephy_embed_prefs_apply_user_style (WebKitUserContentManager *ucm)
{
  if (style_sheet)
    webkit_user_content_manager_add_style_sheet (ucm, style_sheet);
}

void
ephy_embed_prefs_apply_user_javascript (WebKitUserContentManager *ucm)
{
  if (javascript)
    webkit_user_content_manager_add_script (ucm, javascript);
}

void
ephy_embed_prefs_register_ucm (WebKitUserContentManager *ucm)
{
  ucm_list = g_list_append (ucm_list, ucm);
}

void
ephy_embed_prefs_unregister_ucm (WebKitUserContentManager *ucm)
{
  ucm_list = g_list_remove (ucm_list, ucm);
}
