/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

/**
 * - Load a web_extension as described at https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/
 * - Prepare the internal structure so that they can be easily applied to its destination (webview/browser) with the help of extension manager.
 */

#include "config.h"

#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-json-utils.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-web-extension.h"
#include "ephy-window.h"

#include "tabs.h"

#include <archive.h>
#include <archive_entry.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

typedef struct {
  gint64 size;
  GdkPixbuf *pixbuf;
} WebExtensionIcon;

typedef struct  {
  GPtrArray *allow_list;
  GPtrArray *block_list;
  GPtrArray *js;

  WebKitUserContentInjectedFrames injected_frames;
  WebKitUserScriptInjectionTime injection_time;
  GList *user_scripts;
} WebExtensionContentScript;

typedef struct {
  GList *default_icons;
  GtkWidget *widget;
} WebExtensionPageAction;

typedef struct {
  char *title;
  GList *default_icons;
  char *popup;
} WebExtensionBrowserAction;

typedef struct {
  char *page;
} WebExtensionOptionsUI;

typedef struct {
  char *code;
  WebKitUserStyleSheet *style;
} WebExtensionCustomCSS;

struct _EphyWebExtension {
  GObject parent_instance;

  gboolean xpi;
  char *base_location;
  char *manifest;

  char *description;
  gint64 manifest_version;
  char *guid;
  char *author;
  char *name;
  char *short_name;
  char *version;
  char *homepage_url;
  char *content_security_policy;
  GList *icons;
  GList *content_scripts;
  char *background_page;
  GHashTable *page_action_map;
  WebExtensionPageAction *page_action;
  WebExtensionBrowserAction *browser_action;
  WebExtensionOptionsUI *options_ui;
  GHashTable *resources;
  GList *custom_css;
  GHashTable *permissions;
  GPtrArray *host_permissions;
  GCancellable *cancellable;
  char *local_storage_path;
  JsonNode *local_storage;
  GHashTable *web_accessible_resources;
  GHashTable *commands;
};

G_DEFINE_QUARK (web - extension - error - quark, web_extension_error)

G_DEFINE_FINAL_TYPE (EphyWebExtension, ephy_web_extension, G_TYPE_OBJECT)

static gboolean is_supported_scheme (const char *scheme);

gboolean
ephy_web_extension_has_resource (EphyWebExtension *self,
                                 const char       *name)
{
  return g_hash_table_contains (self->resources, name);
}

gconstpointer
ephy_web_extension_get_resource (EphyWebExtension *self,
                                 const char       *name,
                                 gsize            *length)
{
  GBytes *resource;
  if (length)
    *length = 0;

  resource = g_hash_table_lookup (self->resources, name);
  if (!resource) {
    g_debug ("Could not find web_extension resource: %s\n", name);
    return NULL;
  }

  return g_bytes_get_data (resource, length);
}

char *
ephy_web_extension_get_resource_as_string (EphyWebExtension *self,
                                           const char       *name)
{
  gsize len;
  gconstpointer data = ephy_web_extension_get_resource (self, name, &len);
  g_autofree char *out = NULL;

  if (data && len) {
    out = g_malloc0 (len + 1);
    memcpy (out, data, len);
  }

  return g_steal_pointer (&out);
}

static WebExtensionIcon *
web_extension_icon_new (EphyWebExtension *self,
                        const char       *resource_path,
                        gint64            size)
{
  WebExtensionIcon *icon = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  pixbuf = ephy_web_extension_load_pixbuf (self, resource_path, size);
  if (!pixbuf)
    return NULL;

  icon = g_new (WebExtensionIcon, 1);
  icon->size = size;
  icon->pixbuf = g_steal_pointer (&pixbuf);

  return icon;
}

static void
web_extension_icon_free (WebExtensionIcon *icon)
{
  g_clear_object (&icon->pixbuf);
  g_free (icon);
}

static WebExtensionContentScript *
web_extension_content_script_new (WebKitUserContentInjectedFrames injected_frames,
                                  WebKitUserScriptInjectionTime   injection_time)
{
  WebExtensionContentScript *content_script = g_malloc0 (sizeof (WebExtensionContentScript));

  content_script->injected_frames = injected_frames;
  content_script->injection_time = injection_time;
  content_script->allow_list = g_ptr_array_new_full (1, g_free);
  content_script->block_list = g_ptr_array_new_full (1, g_free);
  content_script->js = g_ptr_array_new_full (1, g_free);

  return content_script;
}

static void
web_extension_content_script_free (WebExtensionContentScript *content_script)
{
  g_clear_pointer (&content_script->allow_list, g_ptr_array_unref);
  g_clear_pointer (&content_script->block_list, g_ptr_array_unref);
  g_clear_pointer (&content_script->js, g_ptr_array_unref);
  g_clear_list (&content_script->user_scripts, (GDestroyNotify)webkit_user_script_unref);
  g_free (content_script);
}

static WebExtensionOptionsUI *
web_extension_options_ui_new (const char *page)
{
  WebExtensionOptionsUI *options_ui = g_malloc0 (sizeof (WebExtensionOptionsUI));

  options_ui->page = g_strdup (page);

  return options_ui;
}

static void
web_extension_options_ui_free (WebExtensionOptionsUI *options_ui)
{
  g_clear_pointer (&options_ui->page, g_free);
  g_free (options_ui);
}

static void
web_extension_add_icon (JsonObject *object,
                        const char *member_name,
                        JsonNode   *member_node,
                        gpointer    user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  WebExtensionIcon *icon;
  const char *path;
  gint64 size;

  path = ephy_json_node_to_string (member_node);
  if (!path) {
    LOG ("Skipping icon as value is invalid");
    return;
  }

  size = g_ascii_strtoll (member_name, NULL, 0);
  if (size == 0) {
    LOG ("Skipping %s as web extension icon as size is 0", path);
    return;
  }

  icon = web_extension_icon_new (self, path, size);
  if (icon)
    self->icons = g_list_append (self->icons, icon);
}

static void
web_extension_add_browser_icons (JsonObject *object,
                                 const char *member_name,
                                 JsonNode   *member_node,
                                 gpointer    user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  WebExtensionIcon *icon;
  const char *file;
  gint64 size;

  file = ephy_json_node_to_string (member_node);
  if (!file) {
    LOG ("Skipping browser icon as value is invalid");
    return;
  }

  size = g_ascii_strtoll (member_name, NULL, 0);
  if (size == 0) {
    LOG ("Skipping %s as web extension browser icon as size is 0", file);
    return;
  }

  icon = web_extension_icon_new (self, file, size);
  if (icon)
    self->browser_action->default_icons = g_list_append (self->browser_action->default_icons, icon);
}

GdkPixbuf *
ephy_web_extension_get_icon (EphyWebExtension *self,
                             gint64            size)
{
  WebExtensionIcon *icon_fallback = NULL;

  for (GList *list = self->icons; list && list->data; list = list->next) {
    WebExtensionIcon *icon = list->data;

    if (icon->size == size)
      return gdk_pixbuf_copy (icon->pixbuf);

    if (!icon_fallback || icon->size > icon_fallback->size)
      icon_fallback = icon;
  }

  /* Fallback */
  if (!icon_fallback && self->browser_action && self->browser_action->default_icons)
    icon_fallback = self->browser_action->default_icons->data;

  if (icon_fallback && icon_fallback->pixbuf)
    return gdk_pixbuf_scale_simple (icon_fallback->pixbuf, size, size, GDK_INTERP_BILINEAR);

  return NULL;
}

const char *
ephy_web_extension_get_name (EphyWebExtension *self)
{
  return self->name;
}

const char *
ephy_web_extension_get_short_name (EphyWebExtension *self)
{
  return self->short_name ? self->short_name : self->name;
}

const char *
ephy_web_extension_get_version (EphyWebExtension *self)
{
  return self->version;
}

const char *
ephy_web_extension_get_description (EphyWebExtension *self)
{
  return self->description;
}

const char *
ephy_web_extension_get_homepage_url (EphyWebExtension *self)
{
  return self->homepage_url;
}

const char *
ephy_web_extension_get_author (EphyWebExtension *self)
{
  return self->author;
}

const char *
ephy_web_extension_get_content_security_policy (EphyWebExtension *self)
{
  return self->content_security_policy;
}

const char *
ephy_web_extension_get_manifest (EphyWebExtension *self)
{
  return self->manifest;
}

const char *
ephy_web_extension_get_base_location (EphyWebExtension *self)
{
  return self->base_location;
}

static void
web_extension_add_allow_list (JsonArray *array,
                              guint      index,
                              JsonNode  *element_node,
                              gpointer   user_data)
{
  WebExtensionContentScript *content_script = user_data;
  const char *match = ephy_json_node_to_string (element_node);

  if (!match) {
    LOG ("Skipping invalid content_script match rule");
    return;
  }

  if (g_strcmp0 (match, "<all_urls>") == 0) {
    g_ptr_array_add (content_script->allow_list, g_strdup ("https://*/*"));
    g_ptr_array_add (content_script->allow_list, g_strdup ("http://*/*"));
    return;
  }

  g_ptr_array_add (content_script->allow_list, g_strdup (match));
}

static void
web_extension_add_block_list (JsonArray *array,
                              guint      index,
                              JsonNode  *element_node,
                              gpointer   user_data)
{
  WebExtensionContentScript *content_script = user_data;
  const char *exclude = ephy_json_node_to_string (element_node);

  if (!exclude) {
    LOG ("Skipping invalid content_script exclude rule");
    return;
  }

  g_ptr_array_add (content_script->block_list, g_strdup (exclude));
}

static void
web_extension_add_js (JsonArray *array,
                      guint      index_,
                      JsonNode  *element_node,
                      gpointer   user_data)
{
  WebExtensionContentScript *content_script = user_data;
  const char *file = ephy_json_node_to_string (element_node);

  if (!file) {
    LOG ("Skipping invalid content_script js file");
    return;
  }

  g_ptr_array_add (content_script->js, g_strdup (file));
}

static void
web_extension_content_script_build (EphyWebExtension          *self,
                                    WebExtensionContentScript *content_script)
{
  if (!content_script->js)
    return;

  for (guint i = 0; i < content_script->js->len; i++) {
    WebKitUserScript *user_script;
    char *js_data;

    js_data = ephy_web_extension_get_resource_as_string (self, g_ptr_array_index (content_script->js, i));
    if (!js_data)
      continue;

    user_script = webkit_user_script_new_for_world (js_data,
                                                    content_script->injected_frames,
                                                    content_script->injection_time,
                                                    ephy_web_extension_get_guid (self),
                                                    (const char * const *)content_script->allow_list->pdata,
                                                    (const char * const *)content_script->block_list->pdata);

    content_script->user_scripts = g_list_append (content_script->user_scripts, user_script);
    g_free (js_data);
  }
}

static void
web_extension_add_content_script (JsonArray *array,
                                  guint      index_,
                                  JsonNode  *element_node,
                                  gpointer   user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  WebKitUserContentInjectedFrames injected_frames = WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
  WebKitUserScriptInjectionTime injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
  WebExtensionContentScript *content_script;
  JsonObject *object = ephy_json_node_get_object (element_node);
  JsonArray *child_array;
  const char *run_at;
  gboolean all_frames;

  if (!object) {
    LOG ("Skipping content script as invalid object");
    return;
  }

  /* TODO: The default value is "document_idle", which in WebKit term is document_end */
  run_at = json_object_get_string_member_with_default (object, "run_at", "document_idle");
  if (strcmp (run_at, "document_start") == 0) {
    injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;
  } else if (strcmp (run_at, "document_end") == 0) {
    injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
  } else if (strcmp (run_at, "document_idle") == 0) {
    LOG ("run_at: document_idle not supported by WebKit, falling back to document_end");
    injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
  } else {
    LOG ("Unhandled run_at '%s' in web_extension, ignoring.", run_at);
    return;
  }

  /* all_frames */
  all_frames = ephy_json_object_get_boolean (object, "all_frames", FALSE);
  injected_frames = all_frames ? WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES : WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;

  content_script = web_extension_content_script_new (injected_frames, injection_time);

  if ((child_array = ephy_json_object_get_array (object, "matches")))
    json_array_foreach_element (child_array, web_extension_add_allow_list, content_script);
  g_ptr_array_add (content_script->allow_list, NULL); /* This is a GStrv later. */

  if ((child_array = ephy_json_object_get_array (object, "exclude_matches")))
    json_array_foreach_element (child_array, web_extension_add_block_list, content_script);
  g_ptr_array_add (content_script->block_list, NULL); /* This is a GStrv later. */


  if ((child_array = ephy_json_object_get_array (object, "js")))
    json_array_foreach_element (child_array, web_extension_add_js, content_script);

  /* Create user scripts so that we can unload them if necessary */
  web_extension_content_script_build (self, content_script);

  self->content_scripts = g_list_append (self->content_scripts, content_script);
}

static char *
generate_background_page (EphyWebExtension *self,
                          JsonArray        *scripts)
{
  /* The entry point is always an HTML file, if they list scripts we just generate one.
   * This behavior exactly matches Firefox. */
  GString *background_page = g_string_new ("<html><head><meta charset=\"utf-8\"></head><body>");
  const char *path = "_generated_background_page.html";
  GBytes *bytes;

  for (unsigned int i = 0; i < json_array_get_length (scripts); i++) {
    const char *script_file = json_array_get_string_element (scripts, i);
    g_autofree char *escaped_script_file = g_uri_escape_string (script_file, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);

    g_string_append_printf (background_page,
                            "<script type=\"text/javascript\" src=\"ephy-webextension://%s/%s\"></script>",
                            ephy_web_extension_get_guid (self),
                            escaped_script_file);
  }
  g_string_append (background_page, "</body>");

  bytes = g_bytes_new_take (background_page->str, background_page->len);
  g_string_free (background_page, FALSE);

  g_hash_table_insert (self->resources, g_strdup (path), bytes);

  return g_strdup (path);
}

static void
web_extension_parse_background (EphyWebExtension *self,
                                JsonObject       *object)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/background
   * Limitations:
   *  - persistent with false is not supported yet.
   */
  const char *page;
  JsonArray *scripts;

  if ((page = ephy_json_object_get_string (object, "page")))
    self->background_page = g_strdup (page);

  if ((scripts = ephy_json_object_get_array (object, "scripts"))) {
    if (self->background_page)
      LOG ("background already has page set, ignoring scripts");
    else
      self->background_page = generate_background_page (self, scripts);
  }

  if (json_object_has_member (object, "persistent"))
    LOG ("persistent background setting is not handled in Epiphany");

  if (!self->background_page)
    LOG ("Invalid background object. Missing either scripts or page");
}

static void
web_extension_parse_page_action (EphyWebExtension *self,
                                 JsonObject       *object)
{
  const char *default_icon = ephy_json_object_get_string (object, "default_icon");
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  WebExtensionIcon *icon;

  if (!default_icon) {
    LOG ("We only support page_action's default_icon as a string currently.");
    return;
  }

  pixbuf = ephy_web_extension_load_pixbuf (self, default_icon, -1);
  if (!pixbuf)
    return;

  icon = g_new (WebExtensionIcon, 1);
  icon->size = -1;
  icon->pixbuf = g_steal_pointer (&pixbuf);

  self->page_action = g_new0 (WebExtensionPageAction, 1);
  self->page_action->default_icons = g_list_append (self->page_action->default_icons, icon);
}

static void
web_extension_page_action_free (WebExtensionPageAction *page_action)
{
  g_clear_list (&page_action->default_icons, (GDestroyNotify)web_extension_icon_free);
  g_free (page_action);
}

/* TODO: Load translation for current locale during init */
static char *
web_extension_get_translation (EphyWebExtension *self,
                               const char       *locale,
                               const char       *key)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *path = g_strdup_printf ("_locales/%s/messages.json", locale);
  JsonNode *root = NULL;
  JsonObject *root_object = NULL;
  JsonObject *name = NULL;
  const unsigned char *data = NULL;
  gsize length;

  if (!ephy_web_extension_has_resource (self, path))
    return NULL;

  data = ephy_web_extension_get_resource (self, path, &length);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, (char *)data, length, &error)) {
    g_warning ("Could not load WebExtension translation: %s", error->message);
    return NULL;
  }

  root = json_parser_get_root (parser);
  if (!root) {
    g_warning ("WebExtension translation root is NULL, return NULL.");
    return NULL;
  }

  root_object = json_node_get_object (root);
  if (!root_object) {
    g_warning ("WebExtension translation root object is NULL, return NULL.");
    return NULL;
  }

  name = json_object_get_object_member (root_object, key);
  if (name)
    return g_strdup (json_object_get_string_member (name, "message"));

  return NULL;
}

char *
ephy_web_extension_manifest_get_localized_string (EphyWebExtension *self,
                                                  JsonObject       *object,
                                                  char             *name)
{
  g_autofree char *value = g_strdup (ephy_json_object_get_string (object, name));
  if (!value)
    return g_strdup ("");

  /* Translation are requested with a unique string, e.g.:
   * __MSG_unique_name__ but stored as unique_name in messages.json.
   * Let's check for this prefix and suffix and extract the unique name
   */
  if (g_str_has_prefix (value, "__MSG_") && g_str_has_suffix (value, "__")) {
    /* FIXME: Set current locale */
    g_autofree char *locale = g_strdup ("en");
    g_autofree char *translated_string = NULL;
    char *message_name;

    /* Strip prefix and remove trailing __ */
    message_name = value + strlen ("__MSG_");
    value[strlen (value) - 2] = '\0';

    translated_string = web_extension_get_translation (self, locale, message_name);

    if (!translated_string) {
      g_debug ("Failed to find '%s' translation for message '%s'", locale, message_name);
      return g_strdup ("");
    }

    return g_steal_pointer (&translated_string);
  }

  return g_steal_pointer (&value);
}

static void
web_extension_parse_browser_action (EphyWebExtension *self,
                                    JsonObject       *object)
{
  WebExtensionBrowserAction *browser_action = g_malloc0 (sizeof (WebExtensionBrowserAction));

  self->browser_action = browser_action;
  self->browser_action->title = ephy_web_extension_manifest_get_localized_string (self, object, "default_title");
  self->browser_action->popup = g_strdup (ephy_json_object_get_string (object, "default_popup"));

  if (json_object_has_member (object, "default_icon")) {
    JsonNode *icon_node = json_object_get_member (object, "default_icon");

    /* default_icon can be Object or String */
    if (json_node_get_node_type (icon_node) == JSON_NODE_OBJECT) {
      JsonObject *icon_object = json_object_get_object_member (object, "default_icon");
      json_object_foreach_member (icon_object, web_extension_add_browser_icons, self);
    } else if (JSON_NODE_HOLDS_VALUE (icon_node) && json_node_get_value_type (icon_node) == G_TYPE_STRING) {
      const char *default_icon = json_object_get_string_member (object, "default_icon");
      WebExtensionIcon *icon = web_extension_icon_new (self, default_icon, -1);
      if (icon)
        self->browser_action->default_icons = g_list_append (self->browser_action->default_icons, icon);
    } else {
      LOG ("browser_action's default_icon is invalid");
    }
  }
}

static void
web_extension_browser_action_free (WebExtensionBrowserAction *browser_action)
{
  g_clear_pointer (&browser_action->title, g_free);
  g_clear_pointer (&browser_action->popup, g_free);
  g_clear_list (&browser_action->default_icons, (GDestroyNotify)web_extension_icon_free);
  g_free (browser_action);
}

static void
web_extension_parse_options_ui (EphyWebExtension *self,
                                JsonObject       *object)
{
  const char *page = ephy_json_object_get_string (object, "page");

  if (!page) {
    LOG ("Skipping options_ui without page");
    return;
  }

  self->options_ui = web_extension_options_ui_new (page);
}

static void
web_extension_add_permission (JsonArray *array,
                              guint      index_,
                              JsonNode  *element_node,
                              gpointer   user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  const char *permission = ephy_json_node_to_string (element_node);

  if (!permission) {
    LOG ("Skipping invalid permission");
    return;
  }

  if (strstr (permission, "://")) {
    if (!g_str_has_prefix (permission, "*://") &&
        !is_supported_scheme (g_uri_peek_scheme (permission))) {
      LOG ("Unsupported host permission: %s", permission);
      return;
    }
    g_ptr_array_insert (self->host_permissions, 0, g_strdup (permission));
    return;
  }

  if (strcmp (permission, "<all_urls>") == 0) {
    g_ptr_array_insert (self->host_permissions, 0, g_strdup ("http://*/*"));
    g_ptr_array_insert (self->host_permissions, 0, g_strdup ("https://*/*"));
    return;
  }

  g_hash_table_add (self->permissions, g_strdup (permission));
}

static void
web_extension_add_web_accessible_resource (JsonArray *array,
                                           guint      index_,
                                           JsonNode  *element_node,
                                           gpointer   user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  const char *web_accessible_resource = ephy_json_node_to_string (element_node);

  if (!web_accessible_resource)
    return;

  if (!self->web_accessible_resources)
    self->web_accessible_resources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_hash_table_add (self->web_accessible_resources, g_strdup (web_accessible_resource));
}

static gboolean
is_valid_key (const char *key)
{
  static const char * const allowed_keys[] = { "Comma", "Period", "Home", "End", "PageUp", "PageDown", "Space", "Insert", "Delete", "Up", "Down", "Left", "Right", NULL};

  if (strlen (key) == 1 && ((*key >= 'A' && *key <= 'Z') || (*key >= '0' && *key <= '9')))
    return TRUE;

  if (strlen (key) == 2 && key[0] == 'F' && (key[1] >= '0' && key[1] <= '9'))
    return TRUE;

  return g_strv_contains (allowed_keys, key);
}

/*
 * ephy_web_extension_parse_command_key:
 * @key: A command key
 *
 * This parses a key in the format specified on [MDN](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/commands).
 *
 * It returns a string in Gtk's accelerator format.
 *
 * Returns: (transfer full): An accelerator or %NULL if invalid
 */
char *
ephy_web_extension_parse_command_key (const char *suggested_key)
{
  g_autoptr (GString) accelerator = g_string_sized_new (strlen (suggested_key) + 5);
  g_auto (GStrv) keys = NULL;
  guint i;
  gboolean has_modifier = FALSE;
  gboolean has_key = FALSE;

  if (strcmp (suggested_key, "MediaNextTrack") == 0)
    return g_strdup ("XF86AudioNext");
  else if (strcmp (suggested_key, "MediaPlayPause") == 0)
    return g_strdup ("XF86AudioPlay");
  else if (strcmp (suggested_key, "MediaPrevTrack") == 0)
    return g_strdup ("XF86AudioPrev");
  else if (strcmp (suggested_key, "MediaStop") == 0)
    return g_strdup ("XF86AudioStop");

  keys = g_strsplit (suggested_key, "+", 3);
  for (i = 0; keys[i]; i++) {
    const char *key = keys[i];
    /* First two are potentially modifiers. We should check for duplicates but its probably harmless. */
    if (i == 0 || i == 1) {
      if (strcmp (key, "Ctrl") == 0 || strcmp (key, "Alt") == 0 || (i == 1 && strcmp (key, "Shift") == 0)) {
        g_string_append_printf (accelerator, "<%s>", key);
        has_modifier = TRUE;
        continue;
      } else if (strcmp (key, "Command") == 0 || strcmp (key, "MacCtrl") == 0) {
        g_string_append (accelerator, "<Ctrl>");
        has_modifier = TRUE;
        continue;
      } else if (i == 0 || !is_valid_key (key)) {
        g_debug ("Invalid modifier at index %u: %s", i, key);
        return NULL;
      }
    }

    /* Second two are potentially keys. */
    if (i == 1 || i == 2) {
      if (has_key) {
        g_debug ("Command key has two keys: %s", suggested_key);
        return NULL;
      }
      if (!is_valid_key (key)) {
        g_debug ("Command key has invalid_key: %s", key);
        return NULL;
      }
      if (strcmp (key, "Space") == 0)
        g_string_append (accelerator, "space");
      else if (strcmp (key, "Period") == 0)
        g_string_append (accelerator, "period");
      else if (strcmp (key, "Comma") == 0)
        g_string_append (accelerator, "comma");
      else if (strcmp (key, "PageUp") == 0)
        g_string_append (accelerator, "Page_Up");
      else if (strcmp (key, "PageDown") == 0)
        g_string_append (accelerator, "Page_Down");
      else
        g_string_append (accelerator, key);
      has_key = TRUE;
    }
  }

  if (!has_modifier && !has_key) {
    g_debug ("Command key requires a modifier and a key: %s", suggested_key);
    return NULL;
  }

  if (!gtk_accelerator_parse (accelerator->str, NULL, NULL)) {
    g_warning ("Transformed WebExtensions accelerator %s into %s, but this is not a valid GTK accelerator", suggested_key, accelerator->str);
    return NULL;
  }

  return g_steal_pointer (&accelerator->str);
}

GHashTable *
ephy_web_extension_get_commands (EphyWebExtension *self)
{
  return self->commands;
}

static WebExtensionCommand *
web_extension_command_new (const char *name,
                           const char *description,
                           const char *accelerator,
                           const char *shortcut)
{
  WebExtensionCommand *command = g_new (WebExtensionCommand, 1);

  command->name = g_strdup (name);
  command->description = g_strdup (description);
  command->accelerator = g_strdup (accelerator);
  command->shortcut = g_strdup (shortcut);

  return command;
}

void
web_extension_command_free (WebExtensionCommand *command)
{
  g_clear_pointer (&command->name, g_free);
  g_clear_pointer (&command->description, g_free);
  g_clear_pointer (&command->accelerator, g_free);
  g_clear_pointer (&command->shortcut, g_free);
  g_free (command);
}

WebExtensionCommand *
web_extension_command_copy (WebExtensionCommand *command)
{
  return web_extension_command_new (command->name, command->description, command->accelerator, command->shortcut);
}

static void
web_extension_parse_commands (EphyWebExtension *self,
                              JsonObject       *object)
{
  for (GList *list = json_object_get_members (object); list; list = list->next) {
    JsonObject *command_object;
    JsonObject *key_object;
    const char *suggested_key = NULL;
    WebExtensionCommand *command;
    g_autofree char *accelerator = NULL;
    g_autofree char *description = NULL;
    const char *name = list->data;

    command_object = ephy_json_object_get_object (object, name);
    if (!command_object) {
      LOG ("Skipping command as value is invalid");
      continue;
    }

    description = ephy_web_extension_manifest_get_localized_string (self, command_object, "description");
    key_object = ephy_json_object_get_object (command_object, "suggested_key");

    if (key_object) {
      if ((suggested_key = ephy_json_object_get_string (key_object, "linux")))
        accelerator = ephy_web_extension_parse_command_key (suggested_key);
      else if ((suggested_key = ephy_json_object_get_string (key_object, "default")))
        accelerator = ephy_web_extension_parse_command_key (suggested_key);
    }

    command = web_extension_command_new (name, description, accelerator, suggested_key);
    g_debug ("Inserting command %s (%s)", command->name, command->accelerator);
    g_hash_table_replace (self->commands, command->name, command);
  }
}

static void
ephy_web_extension_dispose (GObject *object)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (object);

  g_clear_pointer (&self->base_location, g_free);
  g_clear_pointer (&self->manifest, g_free);
  g_clear_pointer (&self->guid, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->author, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->short_name, g_free);
  g_clear_pointer (&self->version, g_free);
  g_clear_pointer (&self->homepage_url, g_free);
  g_clear_pointer (&self->local_storage_path, g_free);
  g_clear_pointer (&self->content_security_policy, g_free);

  g_clear_list (&self->icons, (GDestroyNotify)web_extension_icon_free);
  g_clear_list (&self->content_scripts, (GDestroyNotify)web_extension_content_script_free);
  g_clear_pointer (&self->resources, g_hash_table_unref);
  g_clear_pointer (&self->background_page, g_free);
  g_clear_pointer (&self->options_ui, web_extension_options_ui_free);
  g_clear_pointer (&self->permissions, g_hash_table_unref);
  g_clear_pointer (&self->host_permissions, g_ptr_array_unref);
  g_clear_pointer (&self->local_storage, json_node_unref);
  g_clear_pointer (&self->web_accessible_resources, g_hash_table_unref);
  g_clear_pointer (&self->commands, g_hash_table_unref);

  g_clear_pointer (&self->page_action, web_extension_page_action_free);
  g_clear_pointer (&self->browser_action, web_extension_browser_action_free);
  g_clear_list (&self->custom_css, (GDestroyNotify)webkit_user_style_sheet_unref);

  g_hash_table_destroy (self->page_action_map);

  G_OBJECT_CLASS (ephy_web_extension_parent_class)->dispose (object);
}

static void
ephy_web_extension_class_init (EphyWebExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_extension_dispose;
}

static void
ephy_web_extension_init (EphyWebExtension *self)
{
  self->page_action_map = g_hash_table_new (NULL, NULL);
  self->permissions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->host_permissions = g_ptr_array_new_full (2, g_free);
  self->commands = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)web_extension_command_free);

  self->guid = g_uuid_string_random ();

  g_ptr_array_add (self->host_permissions, g_strdup_printf ("ephy-webextension://%s/*", self->guid));
  /* This has to be NULL terminated as we pass it to webkit_web_view_set_cors_allowlist() directly. */
  g_ptr_array_add (self->host_permissions, NULL);
}

static gboolean
ephy_web_extension_parse_manifest (EphyWebExtension  *self,
                                   GError           **error)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *root;
  JsonObject *root_object;
  JsonObject *child_object;
  JsonArray *child_array;
  gsize length = 0;
  const guchar *manifest;
  g_autofree char *extension_basename = NULL;
  g_autofree char *local_storage_contents = NULL;

  manifest = ephy_web_extension_get_resource (self, "manifest.json", &length);
  if (!manifest) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_MANIFEST, _("manifest.json not found"));
    return FALSE;
  }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, (const char *)manifest, length, &local_error)) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_MANIFEST, _("Failed to parse manifest.json: %s"), local_error->message);
    return FALSE;
  }

  root = json_parser_get_root (parser);
  if (!root || json_node_get_node_type (root) != JSON_NODE_OBJECT) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_MANIFEST, _("manifest.json invalid"));
    return FALSE;
  }

  root_object = json_node_get_object (root);
  if (!root_object) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_MANIFEST, _("manifest.json invalid"));
    return FALSE;
  }

  self->manifest = g_strndup ((char *)manifest, length);
  self->manifest_version = ephy_json_object_get_int (root_object, "manifest_version");
  if (self->manifest_version != 2) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_MANIFEST, _("Only manifest_version 2 is supported"));
    return FALSE;
  }

  self->description = ephy_web_extension_manifest_get_localized_string (self, root_object, "description");
  self->name = ephy_web_extension_manifest_get_localized_string (self, root_object, "name");
  self->short_name = ephy_web_extension_manifest_get_localized_string (self, root_object, "short_name");
  self->version = ephy_web_extension_manifest_get_localized_string (self, root_object, "version");
  self->homepage_url = ephy_web_extension_manifest_get_localized_string (self, root_object, "homepage_url");
  self->author = ephy_web_extension_manifest_get_localized_string (self, root_object, "author");

  /* Default matches Firefox: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Content_Security_Policy#default_content_security_policy */
  self->content_security_policy = g_strdup (ephy_json_object_get_string (root_object, "content_security_policy"));
  if (!self->content_security_policy)
    self->content_security_policy = g_strdup ("script-src 'self'; object-src 'self';");

  if (!*self->version || !*self->name) {
    g_set_error (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_MANIFEST, _("Missing name or version"));
    return FALSE;
  }

  extension_basename = g_path_get_basename (self->base_location);
  self->local_storage_path = g_build_filename (ephy_config_dir (), "web_extensions",
                                               extension_basename, "local-storage.json", NULL);

  if (g_file_get_contents (self->local_storage_path, &local_storage_contents, NULL, NULL)) {
    self->local_storage = json_from_string (local_storage_contents, &local_error);
    if (local_error) {
      g_warning ("Failed to load extension's local storage JSON: %s", local_error->message);
      g_clear_error (&local_error);
    }
  }

  if (!self->local_storage)
    self->local_storage = json_node_init_object (json_node_alloc (), json_object_new ());

  if ((child_object = ephy_json_object_get_object (root_object, "icons")))
    json_object_foreach_member (child_object, web_extension_add_icon, self);

  if ((child_array = ephy_json_object_get_array (root_object, "content_scripts")))
    json_array_foreach_element (child_array, web_extension_add_content_script, self);

  if ((child_object = ephy_json_object_get_object (root_object, "background")))
    web_extension_parse_background (self, child_object);

  if ((child_object = ephy_json_object_get_object (root_object, "page_action")))
    web_extension_parse_page_action (self, child_object);

  if ((child_object = ephy_json_object_get_object (root_object, "browser_action")))
    web_extension_parse_browser_action (self, child_object);

  if ((child_object = ephy_json_object_get_object (root_object, "options_ui")))
    web_extension_parse_options_ui (self, child_object);

  if ((child_array = ephy_json_object_get_array (root_object, "permissions")))
    json_array_foreach_element (child_array, web_extension_add_permission, self);

  if ((child_array = ephy_json_object_get_array (root_object, "web_accessible_resources")))
    json_array_foreach_element (child_array, web_extension_add_web_accessible_resource, self);

  if ((child_object = ephy_json_object_get_object (root_object, "commands")))
    web_extension_parse_commands (self, child_object);

  return TRUE;
}

EphyWebExtension *
ephy_web_extension_load_finished (GObject       *source_object,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
load_directory_or_xpi_ready_cb (GFile        *target,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr (EphyWebExtension) web_extension = NULL;
  GTask *load_task = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GHashTable) resources = NULL;
  gboolean was_xpi = GPOINTER_TO_UINT (g_task_get_task_data (G_TASK (result)));

  resources = g_task_propagate_pointer (G_TASK (result), &error);
  if (error) {
    g_task_return_error (load_task, g_steal_pointer (&error));
    return;
  }

  web_extension = g_object_new (EPHY_TYPE_WEB_EXTENSION, NULL);
  web_extension->xpi = was_xpi;
  web_extension->base_location = g_file_get_path (target);
  web_extension->resources = g_steal_pointer (&resources);

  if (!ephy_web_extension_parse_manifest (web_extension, &error)) {
    g_task_return_error (load_task, g_steal_pointer (&error));
    return;
  }

  g_task_return_pointer (load_task, g_steal_pointer (&web_extension), g_object_unref);
}

static gboolean
load_directory_resources_thread (GFile         *directory,
                                 GFile         *base_directory,
                                 GHashTable    *resources,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  g_autoptr (GFileEnumerator) enumerator = NULL;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable,
                                          error);
  if (!enumerator)
    return FALSE;

  while (TRUE) {
    GFileInfo *info;
    GFile *child;

    if (!g_file_enumerator_iterate (enumerator, &info, &child, cancellable, error))
      return FALSE;

    if (!info)
      break;

    if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
      if (!load_directory_resources_thread (child, base_directory, resources, cancellable, error))
        return FALSE;
    } else {
      char *contents;
      gsize size;

      if (!g_file_get_contents (g_file_peek_path (child), &contents, &size, error))
        return FALSE;

      g_hash_table_insert (resources,
                           g_file_get_relative_path (base_directory, child),
                           g_bytes_new_take (contents, size));
    }
  }

  return TRUE;
}

static void
load_directory_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  GFile *target = source_object;
  g_autoptr (GHashTable) resources = NULL;
  g_autoptr (GError) error = NULL;

  resources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_bytes_unref);

  if (!load_directory_resources_thread (target, target, resources, g_task_get_cancellable (task), &error)) {
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  g_task_return_pointer (task, g_steal_pointer (&resources), (GDestroyNotify)g_hash_table_unref);
}

static void
ephy_web_extension_load_directory_async (GFile *target,
                                         GTask *load_task)
{
  GTask *directory_task = g_task_new (target, g_task_get_cancellable (load_task), (GAsyncReadyCallback)load_directory_or_xpi_ready_cb, load_task);
  g_task_set_task_data (directory_task, GUINT_TO_POINTER (FALSE), NULL);
  g_task_set_return_on_cancel (directory_task, TRUE);
  g_task_run_in_thread (directory_task, load_directory_thread);
}

static void
load_xpi_thread (GTask        *task,
                 gpointer      source_object,
                 gpointer      task_data,
                 GCancellable *cancellable)
{
  GFile *target = source_object;
  GHashTable *resources;
  struct archive *pkg;
  struct archive_entry *entry;
  int res;

  pkg = archive_read_new ();
  archive_read_support_format_zip (pkg);

  res = archive_read_open_filename (pkg, g_file_peek_path (target), 10240);
  if (res != ARCHIVE_OK) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_XPI, "Invalid XPI archive: %s", archive_error_string (pkg));

    res = archive_read_free (pkg);
    if (res != ARCHIVE_OK)
      g_warning ("Error freeing archive: %s", archive_error_string (pkg));
    return;
  }

  resources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_bytes_unref);

  while (archive_read_next_header (pkg, &entry) == ARCHIVE_OK) {
    int64_t size = archive_entry_size (entry);
    gsize total_len = 0;
    g_autofree char *data = NULL;

    data = g_malloc0 (size);
    total_len = archive_read_data (pkg, data, size);

    if (total_len > 0) {
      g_hash_table_insert (resources,
                           g_strdup (archive_entry_pathname (entry)),
                           g_bytes_new_take (g_steal_pointer (&data), total_len));
    }
  }

  res = archive_read_free (pkg);
  if (res != ARCHIVE_OK)
    g_warning ("Error freeing archive: %s", archive_error_string (pkg));

  g_task_return_pointer (task, resources, (GDestroyNotify)g_hash_table_unref);
}

static void
ephy_web_extension_load_xpi_async (GFile *target,
                                   GTask *load_task)
{
  GTask *xpi_task = g_task_new (target, g_task_get_cancellable (load_task), (GAsyncReadyCallback)load_directory_or_xpi_ready_cb, load_task);
  g_task_set_task_data (xpi_task, GUINT_TO_POINTER (TRUE), NULL);
  g_task_set_return_on_cancel (xpi_task, TRUE);
  g_task_run_in_thread (xpi_task, load_xpi_thread);
}

void
ephy_web_extension_load_async (GFile               *target,
                               GFileInfo           *info,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GTask *task;

  g_assert (target);
  g_assert (info);

  task = g_task_new (target, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);

  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    ephy_web_extension_load_directory_async (target, task);
  else
    ephy_web_extension_load_xpi_async (target, task);
}

GdkPixbuf *
ephy_web_extension_load_pixbuf (EphyWebExtension *self,
                                const char       *resource_path,
                                int               size)
{
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GError) error = NULL;
  GdkPixbuf *pixbuf;
  const unsigned char *data = NULL;
  gsize length;

  if (*resource_path == '/')
    resource_path++;

  data = ephy_web_extension_get_resource (self, resource_path, &length);
  if (!data) {
    g_warning ("Failed to find web extension icon %s", resource_path);
    return NULL;
  }

  stream = g_memory_input_stream_new_from_data (data, length, NULL);
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, size, size, TRUE, NULL, &error);
  if (!pixbuf) {
    g_warning ("Could not load web extension icon: %s", error->message);
    return NULL;
  }

  return pixbuf;
}

void
ephy_web_extension_remove (EphyWebExtension *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->xpi) {
    if (!ephy_file_delete_dir_recursively (self->base_location, &error))
      g_warning ("Could not delete web_extension from %s: %s", self->base_location, error->message);
  } else {
    g_autoptr (GFile) file = g_file_new_for_path (self->base_location);
    if (!g_file_delete (file, NULL, &error))
      g_warning ("Could not delete web_extension %s: %s", self->base_location, error->message);
  }
}

gboolean
ephy_web_extension_has_page_action (EphyWebExtension *self)
{
  return !!self->page_action;
}

gboolean
ephy_web_extension_has_browser_action (EphyWebExtension *self)
{
  return !!self->browser_action;
}

gboolean
ephy_web_extension_has_background_web_view (EphyWebExtension *self)
{
  return !!self->background_page;
}

const char *
ephy_web_extension_background_web_view_get_page (EphyWebExtension *self)
{
  return self->background_page;
}

GList *
ephy_web_extension_get_content_scripts (EphyWebExtension *self)
{
  return self->content_scripts;
}

GList *
ephy_web_extension_get_content_script_js (EphyWebExtension *self,
                                          gpointer          content_script)
{
  WebExtensionContentScript *script = content_script;
  return script->user_scripts;
}

GdkPixbuf *
ephy_web_extension_browser_action_get_icon (EphyWebExtension *self,
                                            int               size)
{
  WebExtensionIcon *icon_fallback = NULL;

  if (!self->browser_action || !self->browser_action->default_icons)
    return NULL;

  for (GList *list = self->browser_action->default_icons; list && list->data; list = list->next) {
    WebExtensionIcon *icon = list->data;

    if (icon->size == size)
      return gdk_pixbuf_copy (icon->pixbuf);

    if (!icon_fallback || icon->size > icon_fallback->size)
      icon_fallback = icon;
  }

  /* Fallback */
  if (icon_fallback)
    return gdk_pixbuf_scale_simple (icon_fallback->pixbuf, size, size, GDK_INTERP_BILINEAR);

  return NULL;
}

const char *
ephy_web_extension_get_browser_popup (EphyWebExtension *self)
{
  return self->browser_action->popup;
}

const char *
ephy_web_extension_browser_action_get_tooltip (EphyWebExtension *self)
{
  return self->browser_action->title;
}

WebExtensionCustomCSS *
web_extension_custom_css_new (EphyWebExtension *self,
                              const char       *code)

{
  WebExtensionCustomCSS *css = g_malloc0 (sizeof (WebExtensionCustomCSS));

  css->code = g_strdup (code);
  css->style = webkit_user_style_sheet_new_for_world (css->code,
                                                      WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                                      WEBKIT_USER_STYLE_LEVEL_USER,
                                                      ephy_web_extension_get_guid (self),
                                                      NULL, NULL);

  self->custom_css = g_list_append (self->custom_css, css);

  return css;
}

WebKitUserStyleSheet *
ephy_web_extension_get_custom_css (EphyWebExtension *self,
                                   const char       *code)
{
  WebExtensionCustomCSS *css = NULL;

  for (GList *list = self->custom_css; list && list->data; list = list->data) {
    css = list->data;

    if (strcmp (css->code, code) == 0)
      return css->style;
  }

  return NULL;
}

WebKitUserStyleSheet *
ephy_web_extension_add_custom_css (EphyWebExtension *self,
                                   const char       *code)
{
  WebKitUserStyleSheet *style;
  WebExtensionCustomCSS *css = NULL;

  style = ephy_web_extension_get_custom_css (self, code);
  if (style)
    return style;

  css = web_extension_custom_css_new (self, code);

  return css->style;
}

GList *
ephy_web_extension_get_custom_css_list (EphyWebExtension *self)
{
  return self->custom_css;
}

WebKitUserStyleSheet *
ephy_web_extension_custom_css_style (EphyWebExtension *self,
                                     gpointer          custom_css)
{
  WebExtensionCustomCSS *css = custom_css;

  return css->style;
}

const char *
ephy_web_extension_get_option_ui_page (EphyWebExtension *self)
{
  if (!self->options_ui)
    return NULL;

  return self->options_ui->page;
}

const char *
ephy_web_extension_get_guid (EphyWebExtension *self)
{
  return self->guid;
}

const char * const *
ephy_web_extension_get_host_permissions (EphyWebExtension *self)
{
  g_assert (!self->host_permissions->pdata[self->host_permissions->len - 1]);
  return (const char * const *)self->host_permissions->pdata;
}

static gboolean
is_supported_scheme (const char *scheme)
{
  static const char * const supported_schemes[] = {
    "https", "http", "wss", "ws", "data", "file", "ephy-webextension"
  };

  g_assert (scheme);

  for (guint i = 0; i < G_N_ELEMENTS (supported_schemes); i++) {
    if (strcmp (supported_schemes[i], scheme) == 0)
      return TRUE;
  }

  return FALSE;
}

static gboolean
scheme_matches (const char *permission_scheme,
                const char *uri_scheme)
{
  /* NOTE: Firefox matches "wss" and "ws" here, Safari and Chrome do not. */
  static const char * const wildcard_allowed_schemes[] = {
    "https", "http", "wss", "ws"
  };

  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns#scheme */
  /* wildcard is a GUri workaround, see parse_uri_with_wildcard_scheme(). */
  if (strcmp (permission_scheme, "wildcard") == 0) {
    for (guint i = 0; i < G_N_ELEMENTS (wildcard_allowed_schemes); i++) {
      if (strcmp (wildcard_allowed_schemes[i], uri_scheme) == 0)
        return TRUE;
    }

    return FALSE;
  }

  return is_supported_scheme (permission_scheme) && strcmp (permission_scheme, uri_scheme) == 0;
}

static gboolean
host_matches (const char *permission_host,
              const char *uri_host)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns#host */
  if (strcmp (permission_host, "*") == 0)
    return TRUE;

  if (g_str_has_prefix (permission_host, "*."))
    return g_str_has_suffix (uri_host, permission_host + 1); /* Skip '*' but NOT '.'. */

  return strcmp (permission_host, uri_host) == 0;
}

static gboolean
path_matches (const char *permission_path,
              const char *uri_path)
{
  g_autofree char *permission_path_escaped = NULL;
  g_autoptr (GString) permission_path_regex = NULL;

  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns#path */
  if (strcmp (permission_path, "*") == 0)
    return TRUE;

  /* We need to do more complicated pattern matching so convert it to regex replacing '*' with '.*'
   * and making sure to match the entire string. */
  permission_path_escaped = g_regex_escape_string (permission_path, -1);
  permission_path_regex = g_string_new (permission_path_escaped);
  g_string_replace (permission_path_regex, "\\*", ".*", -1);

  return g_regex_match_simple (permission_path_regex->str, uri_path, G_REGEX_ANCHORED, G_REGEX_MATCH_ANCHORED | G_REGEX_MATCH_NOTEMPTY);
}

static char *
join_path_and_query (GUri *uri)
{
  const char *path = g_uri_get_path (uri);
  const char *query = g_uri_get_query (uri);
  if (!query)
    return g_strdup (path);

  return g_strjoin ("?", path, query, NULL);
}

static GUri *
parse_uri_with_wildcard_scheme (const char  *uri,
                                GError     **error)
{
  g_autofree char *modified_uri = NULL;
  const char *uri_to_check = uri;

  /* GUri considers the scheme `*` invalid so we have to hackily work around that. */
  if (g_str_has_prefix (uri, "*://")) {
    modified_uri = g_strconcat ("wildcard", uri + 1, NULL);
    uri_to_check = modified_uri;
  }

  return g_uri_parse (uri_to_check, G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_SCHEME_NORMALIZE, error);
}

static gboolean
is_default_port (const char *scheme,
                 int         port)
{
  static const char * const default_port_80[] = { "http", "ws", NULL };
  static const char * const default_port_443[] = { "https", "wss", NULL };

  switch (port) {
    case 80:
      return g_strv_contains (default_port_80, scheme);
    case 443:
      return g_strv_contains (default_port_443, scheme);
  }
  return FALSE;
}

gboolean
ephy_web_extension_rule_matches_uri (const char *rule,
                                     GUri       *uri)
{
  g_autoptr (GUri) rule_uri = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *rule_path_and_query = NULL;
  g_autofree char *uri_path_and_query = NULL;
  const char *rule_scheme;
  int rule_port;

  rule_uri = parse_uri_with_wildcard_scheme (rule, &error);
  if (error) {
    g_warning ("Failed to parse rule '%s' as URI: %s", rule, error->message);
    return FALSE;
  }

  rule_scheme = g_uri_get_scheme (rule_uri);
  rule_port = g_uri_get_port (rule_uri);

  /* Ports are forbidden, however GUri normalizes these to the default. */
  if (rule_port != -1 && !is_default_port (rule_scheme, rule_port))
    return FALSE;

  /* Empty paths are forbidden. */
  if (strcmp (g_uri_get_path (rule_uri), "") == 0)
    return FALSE;

  if (!scheme_matches (rule_scheme, g_uri_get_scheme (uri)))
    return FALSE;

  if (!host_matches (g_uri_get_host (rule_uri), g_uri_get_host (uri)))
    return FALSE;

  rule_path_and_query = join_path_and_query (rule_uri);
  uri_path_and_query = join_path_and_query (uri);

  if (!path_matches (rule_path_and_query, uri_path_and_query))
    return FALSE;

  return TRUE;
}

static gboolean
ephy_web_extension_has_permission_internal (EphyWebExtension *self,
                                            EphyWebView      *web_view,
                                            gboolean          is_user_interaction,
                                            gboolean          allow_tabs)
{
  EphyWebView *active_web_view = ephy_shell_get_active_web_view (ephy_shell_get_default ());
  gboolean is_active_tab = active_web_view == web_view;
  GUri *host;

  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns */

  if (is_user_interaction && is_active_tab && g_hash_table_contains (self->permissions, "activeTab"))
    return TRUE;

  if (allow_tabs && g_hash_table_contains (self->permissions, "tabs"))
    return TRUE;

  /* Note this one is NULL terminated. */
  host = g_uri_parse (ephy_web_view_get_address (web_view), G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
  g_assert (host); /* WebKitGTK shouldn't ever expose an invalid URI. */
  for (guint i = 0; i < self->host_permissions->len - 1; i++) {
    const char *permission = g_ptr_array_index (self->host_permissions, i);
    if (ephy_web_extension_rule_matches_uri (permission, host))
      return TRUE;
  }

  return FALSE;
}

gboolean
ephy_web_extension_has_host_or_active_permission (EphyWebExtension *self,
                                                  EphyWebView      *web_view,
                                                  gboolean          is_user_interaction)
{
  return ephy_web_extension_has_permission_internal (self, web_view, is_user_interaction, FALSE);
}

gboolean
ephy_web_extension_has_tab_or_host_permission (EphyWebExtension *self,
                                               EphyWebView      *web_view,
                                               gboolean          is_user_interaction)
{
  return ephy_web_extension_has_permission_internal (self, web_view, is_user_interaction, TRUE);
}

gboolean
ephy_web_extension_has_host_permission (EphyWebExtension *self,
                                        const char       *host)
{
  GUri *uri = g_uri_parse (host, G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
  if (!uri)
    return FALSE;

  for (guint i = 0; i < self->host_permissions->len - 1; i++) {
    const char *permission = g_ptr_array_index (self->host_permissions, i);
    if (ephy_web_extension_rule_matches_uri (permission, uri))
      return TRUE;
  }

  return FALSE;
}


gboolean
ephy_web_extension_has_permission (EphyWebExtension *self,
                                   const char       *permission)
{
  return g_hash_table_contains (self->permissions, permission);
}

JsonNode *
ephy_web_extension_get_local_storage (EphyWebExtension *self)
{
  return self->local_storage;
}

void
ephy_web_extension_save_local_storage (EphyWebExtension *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *json = NULL;
  g_autofree char *parent_dir = NULL;

  parent_dir = g_path_get_dirname (self->local_storage_path);
  g_mkdir_with_parents (parent_dir, 0755);

  json = json_to_string (self->local_storage, TRUE);
  if (!g_file_set_contents (self->local_storage_path, json, -1, &error))
    g_warning ("Failed to write %s: %s", self->local_storage_path, error->message);
}

void
ephy_web_extension_clear_local_storage (EphyWebExtension *self)
{
  self->local_storage = json_node_init_object (self->local_storage, json_object_new ());
}

char *
ephy_web_extension_create_sender_object (EphyWebExtensionSender *sender)
{
  g_autoptr (JsonNode) node = json_node_init_object (json_node_alloc (), json_object_new ());
  JsonObject *obj = json_node_get_object (node);

  json_object_set_string_member (obj, "id", ephy_web_extension_get_guid (sender->extension));
  if (sender->view) {
    json_object_set_string_member (obj, "url", webkit_web_view_get_uri (sender->view));
    json_object_set_int_member (obj, "frameId", sender->frame_id);

    /* For now these are always regular views and not extension views. */
    if (EPHY_IS_WEB_VIEW (sender->view)) {
      json_object_set_member (obj, "tab", ephy_web_extension_api_tabs_create_tab_object (sender->extension, EPHY_WEB_VIEW (sender->view)));
    }
  }

  return json_to_string (node, FALSE);
}

gboolean
ephy_web_extension_has_web_accessible_resource (EphyWebExtension *self,
                                                const char       *path)
{
  if (!self->web_accessible_resources)
    return FALSE;

  return g_hash_table_contains (self->web_accessible_resources, path);
}
