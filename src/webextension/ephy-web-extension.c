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
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-web-extension.h"
#include "ephy-window.h"

#include <archive.h>
#include <archive_entry.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

typedef struct {
  gint64 size;
  char *file;
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
  GPtrArray *scripts;
  char *page;
} WebExtensionBackground;

typedef struct {
  char *page;
} WebExtensionOptionsUI;

typedef struct {
  char *name;
  GBytes *bytes;
} WebExtensionResource;

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
  char *version;
  char *homepage_url;
  GList *icons;
  GList *content_scripts;
  WebExtensionBackground *background;
  GHashTable *page_action_map;
  WebExtensionPageAction *page_action;
  WebExtensionBrowserAction *browser_action;
  WebExtensionOptionsUI *options_ui;
  GList *resources;
  GList *custom_css;
  GPtrArray *permissions;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (EphyWebExtension, ephy_web_extension, G_TYPE_OBJECT)

gboolean
ephy_web_extension_has_resource (EphyWebExtension *self,
                                 const char       *name)
{
  for (GList *list = self->resources; list && list->data; list = list->next) {
    WebExtensionResource *resource = list->data;

    if (g_strcmp0 (resource->name, name) == 0)
      return TRUE;
  }

  return FALSE;
}

gconstpointer
ephy_web_extension_get_resource (EphyWebExtension *self,
                                 const char       *name,
                                 gsize            *length)
{
  if (length)
    *length = 0;

  for (GList *list = self->resources; list && list->data; list = list->next) {
    WebExtensionResource *resource = list->data;

    if (g_strcmp0 (resource->name, name) == 0)
      return g_bytes_get_data (resource->bytes, length);
  }

  g_debug ("Could not find web_extension resource: %s\n", name);
  return NULL;
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
                        const char       *file,
                        gint64            size)
{
  WebExtensionIcon *icon = NULL;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  const unsigned char *data = NULL;
  gsize length;

  data = ephy_web_extension_get_resource (self, file, &length);
  if (!data) {
    if (!self->xpi) {
      g_autofree char *path = NULL;
      path = g_build_filename (self->base_location, file, NULL);
      pixbuf = gdk_pixbuf_new_from_file (path, NULL);
    }
  } else {
    stream = g_memory_input_stream_new_from_data (data, length, NULL);
    pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
  }

  if (!pixbuf) {
    g_warning ("Could not read web_extension icon: %s", error ? error->message : "");
    return NULL;
  }

  icon = g_malloc0 (sizeof (WebExtensionIcon));
  icon->file = g_strdup (file);
  icon->size = size;
  icon->pixbuf = g_steal_pointer (&pixbuf);

  return icon;
}

static void
web_extension_icon_free (WebExtensionIcon *icon)
{
  g_clear_pointer (&icon->file, g_free);
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

static WebExtensionBackground *
web_extension_background_new (void)
{
  WebExtensionBackground *background = g_malloc0 (sizeof (WebExtensionBackground));

  background->scripts = g_ptr_array_new_full (1, g_free);

  return background;
}

static void
web_extension_background_free (WebExtensionBackground *background)
{
  g_clear_pointer (&background->scripts, g_ptr_array_unref);
  g_clear_pointer (&background->page, g_free);
  g_free (background);
}

static void
web_extension_add_icon (JsonObject *object,
                        const char *member_name,
                        JsonNode   *member_node,
                        gpointer    user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  WebExtensionIcon *icon;
  const char *file = json_node_get_string (member_node);
  gint64 size;

  size = g_ascii_strtoll (member_name, NULL, 0);
  if (size == 0) {
    LOG ("Skipping %s as web extension icon as size is 0", file);
    return;
  }

  icon = web_extension_icon_new (self, file, size);

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
  const char *file = json_node_get_string (member_node);
  gint64 size;

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
      return gdk_pixbuf_scale_simple (icon->pixbuf, size, size, GDK_INTERP_BILINEAR);

    if (!icon_fallback || icon->size > icon_fallback->size)
      icon_fallback = icon;
  }

  /* Fallback */
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

  g_ptr_array_add (content_script->allow_list, g_strdup (json_node_get_string (element_node)));
}

static void
web_extension_add_block_list (JsonArray *array,
                              guint      index,
                              JsonNode  *element_node,
                              gpointer   user_data)
{
  WebExtensionContentScript *content_script = user_data;

  g_ptr_array_add (content_script->block_list, g_strdup (json_node_get_string (element_node)));
}

static void
web_extension_add_js (JsonArray *array,
                      guint      index_,
                      JsonNode  *element_node,
                      gpointer   user_data)
{
  WebExtensionContentScript *content_script = user_data;

  g_ptr_array_add (content_script->js, g_strdup (json_node_get_string (element_node)));
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
  JsonObject *object = json_node_get_object (element_node);
  JsonArray *child_array;
  const char *run_at;
  gboolean all_frames;

  /* TODO: The default value is "document_idle", which in WebKit term is document_end */
  run_at = json_object_get_string_member_with_default (object, "run_at", "document_idle");
  if (strcmp (run_at, "document_start") == 0) {
    injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;
  } else if (strcmp (run_at, "document_end") == 0) {
    injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
  } else if (strcmp (run_at, "document_idle") == 0) {
    g_warning ("run_at: document_idle not supported by WebKit, falling back to document_end");
    injection_time = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
  } else {
    g_warning ("Unhandled run_at '%s' in web_extension, ignoring.", run_at);
    return;
  }

  /* all_frames */
  all_frames = json_object_get_boolean_member_with_default (object, "all_frames", FALSE);
  injected_frames = all_frames ? WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES : WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;

  content_script = web_extension_content_script_new (injected_frames, injection_time);
  if (json_object_has_member (object, "matches")) {
    child_array = json_object_get_array_member (object, "matches");
    json_array_foreach_element (child_array, web_extension_add_allow_list, content_script);
  }
  g_ptr_array_add (content_script->allow_list, NULL);

  if (json_object_has_member (object, "exclude_matches")) {
    child_array = json_object_get_array_member (object, "exclude_matches");
    json_array_foreach_element (child_array, web_extension_add_block_list, content_script);
  }
  g_ptr_array_add (content_script->block_list, NULL);

  if (json_object_has_member (object, "js")) {
    child_array = json_object_get_array_member (object, "js");
    if (child_array)
      json_array_foreach_element (child_array, web_extension_add_js, content_script);
  }
  g_ptr_array_add (content_script->js, NULL);

  /* Create user scripts so that we can unload them if necessary */
  web_extension_content_script_build (self, content_script);

  self->content_scripts = g_list_append (self->content_scripts, content_script);
}

static void
web_extension_add_scripts (JsonArray *array,
                           guint      index_,
                           JsonNode  *element_node,
                           gpointer   user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);

  g_ptr_array_add (self->background->scripts, g_strdup (json_node_get_string (element_node)));
}

static void
web_extension_add_background (JsonObject *object,
                              const char *member_name,
                              JsonNode   *member_node,
                              gpointer    user_data)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/background
   * Limitations:
   *  - persistent with false is not supported yet.
   */
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  JsonArray *child_array;

  if (!json_object_has_member (object, "scripts") && !json_object_has_member (object, "page") && !json_object_has_member (object, "persistent")) {
    g_warning ("Invalid background section, it must be either scripts, page or persistent entry.");
    return;
  }

  if (!self->background)
    self->background = web_extension_background_new ();

  if (json_object_has_member (object, "scripts")) {
    child_array = json_object_get_array_member (object, "scripts");
    json_array_foreach_element (child_array, web_extension_add_scripts, self);
  } else if (!self->background->page && json_object_has_member (object, "page")) {
    self->background->page = g_strdup (json_object_get_string_member (object, "page"));
  } else if (json_object_has_member (object, "persistent")) {
    LOG ("persistent background setting is not handled in Epiphany");
  }
}

static void
web_extension_add_page_action (JsonObject *object,
                               gpointer    user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  WebExtensionPageAction *page_action = g_malloc0 (sizeof (WebExtensionPageAction));

  self->page_action = page_action;

  if (json_object_has_member (object, "default_icon")) {
    WebExtensionIcon *icon = g_malloc (sizeof (WebExtensionIcon));
    const char *default_icon = json_object_get_string_member (object, "default_icon");
    g_autofree char *path = NULL;

    icon->size = -1;
    icon->file = g_strdup (default_icon);

    path = g_build_filename (self->base_location, icon->file, NULL);
    icon->pixbuf = gdk_pixbuf_new_from_file (path, NULL);

    self->page_action->default_icons = g_list_append (self->page_action->default_icons, icon);
  }
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
ephy_web_extension_manifest_get_key (EphyWebExtension *self,
                                     JsonObject       *object,
                                     char             *key)
{
  char *value = NULL;

  if (json_object_has_member (object, key)) {
    g_autofree char *ret = g_strdup (json_object_get_string_member (object, key));

    /* Translation are requested with a unique string, e.g.:
     * __MSG_unique_name__ but stored as unique_name in messages.json.
     * Let's check for this prefix and suffix and extract the unique name
     */
    if (g_str_has_prefix (ret, "__MSG_") && g_str_has_suffix (ret, "__")) {
      /* FIXME: Set current locale */
      g_autofree char *locale = g_strdup ("en");

      /* Remove trailing __ */
      ret[strlen (ret) - 2] = '\0';
      value = web_extension_get_translation (self, locale, ret + strlen ("__MSG_"));
    } else {
      value = g_strdup (ret);
    }
  }

  return value;
}

static void
web_extension_add_browser_action (JsonObject *object,
                                  gpointer    user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  WebExtensionBrowserAction *browser_action = g_malloc0 (sizeof (WebExtensionBrowserAction));

  g_clear_object (&self->browser_action);
  self->browser_action = browser_action;

  if (json_object_has_member (object, "default_title")) {
    self->browser_action->title = ephy_web_extension_manifest_get_key (self, object, "default_title");
  }

  if (json_object_has_member (object, "default_icon")) {
    /* defaullt_icon can be Object or String */
    JsonNode *icon_node = json_object_get_member (object, "default_icon");

    if (json_node_get_node_type (icon_node) == JSON_NODE_OBJECT) {
      JsonObject *icon_object = json_object_get_object_member (object, "default_icon");
      json_object_foreach_member (icon_object, web_extension_add_browser_icons, self);
    } else {
      const char *default_icon = json_object_get_string_member (object, "default_icon");
      WebExtensionIcon *icon = web_extension_icon_new (self, default_icon, -1);

      self->browser_action->default_icons = g_list_append (self->browser_action->default_icons, icon);
    }
  }

  if (json_object_has_member (object, "default_popup"))
    self->browser_action->popup = g_strdup (json_object_get_string_member (object, "default_popup"));
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
web_extension_add_options_ui (JsonObject *object,
                              gpointer    user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);
  const char *page = json_object_get_string_member (object, "page");
  WebExtensionOptionsUI *options_ui = web_extension_options_ui_new (page);

  g_clear_pointer (&self->options_ui, web_extension_options_ui_free);
  self->options_ui = options_ui;
}

static void
web_extension_add_permission (JsonArray *array,
                              guint      index_,
                              JsonNode  *element_node,
                              gpointer   user_data)
{
  EphyWebExtension *self = EPHY_WEB_EXTENSION (user_data);

  g_ptr_array_add (self->permissions, g_strdup (json_node_get_string (element_node)));
}

static void
web_extension_resource_free (WebExtensionResource *resource)
{
  g_clear_pointer (&resource->bytes, g_bytes_unref);
  g_clear_pointer (&resource->name, g_free);
  g_free (resource);
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
  g_clear_pointer (&self->version, g_free);
  g_clear_pointer (&self->homepage_url, g_free);

  g_clear_list (&self->icons, (GDestroyNotify)web_extension_icon_free);
  g_clear_list (&self->content_scripts, (GDestroyNotify)web_extension_content_script_free);
  g_clear_list (&self->resources, (GDestroyNotify)web_extension_resource_free);
  g_clear_pointer (&self->background, web_extension_background_free);
  g_clear_pointer (&self->options_ui, web_extension_options_ui_free);
  g_clear_pointer (&self->permissions, g_ptr_array_unref);

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
  self->permissions = g_ptr_array_new_full (1, g_free);

  self->guid = g_uuid_string_random ();
}

static EphyWebExtension *
ephy_web_extension_new (void)
{
  return g_object_new (EPHY_TYPE_WEB_EXTENSION, NULL);
}

static void
web_extension_add_resource (EphyWebExtension *self,
                            const char       *name,
                            gpointer          data,
                            guint             len)
{
  WebExtensionResource *resource = g_malloc0 (sizeof (WebExtensionResource));

  resource->name = g_strdup (name);
  resource->bytes = g_bytes_new (data, len);

  self->resources = g_list_append (self->resources, resource);
}

static gboolean
web_extension_read_directory (EphyWebExtension *self,
                              char             *base,
                              char             *path)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GDir) dir = NULL;
  const char *dirent;
  gboolean ret = TRUE;

  dir = g_dir_open (path, 0, &error);
  if (!dir) {
    g_warning ("Could not open web_extension directory: %s", error->message);
    return FALSE;
  }

  while ((dirent = g_dir_read_name (dir))) {
    GFileType type;
    g_autofree gchar *filename = g_build_filename (path, dirent, NULL);
    g_autoptr (GFile) file = g_file_new_for_path (filename);

    type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);
    if (type == G_FILE_TYPE_DIRECTORY) {
      web_extension_read_directory (self, base, filename);
    } else {
      g_autofree char *data = NULL;
      gsize len;

      if (g_file_get_contents (filename, &data, &len, NULL))
        web_extension_add_resource (self, filename + strlen (base) + 1, data, len);
    }
  }

  return ret;
}

static EphyWebExtension *
ephy_web_extension_load_directory (char *filename)
{
  EphyWebExtension *self = ephy_web_extension_new ();

  web_extension_read_directory (self, filename, filename);

  return self;
}

static EphyWebExtension *
ephy_web_extension_load_xpi (GFile *target)
{
  EphyWebExtension *self = NULL;
  struct archive *pkg;
  struct archive_entry *entry;
  int res;

  pkg = archive_read_new ();
  archive_read_support_format_zip (pkg);

  res = archive_read_open_filename (pkg, g_file_get_path (target), 10240);
  if (res == ARCHIVE_OK) {
    self = ephy_web_extension_new ();
    self->xpi = TRUE;

    while (archive_read_next_header (pkg, &entry) == ARCHIVE_OK) {
      int64_t size = archive_entry_size (entry);
      gsize total_len = 0;
      g_autofree char *data = NULL;

      data = g_malloc0 (size);
      total_len = archive_read_data (pkg, data, size);

      if (total_len > 0)
        web_extension_add_resource (self, archive_entry_pathname (entry), data, total_len);
    }

    res = archive_read_free (pkg);
    if (res != ARCHIVE_OK)
      g_warning ("Error freeing archive: %s", archive_error_string (pkg));
  } else {
    g_warning ("Could not open archive %s", archive_error_string (pkg));
  }

  return self;
}

EphyWebExtension *
ephy_web_extension_load (GFile *target)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) source = g_file_dup (target);
  g_autoptr (GFile) parent = NULL;
  g_autoptr (JsonObject) icons_object = NULL;
  g_autoptr (JsonArray) content_scripts_array = NULL;
  g_autoptr (JsonObject) background_object = NULL;
  JsonParser *parser = NULL;
  JsonNode *root = NULL;
  JsonObject *root_object = NULL;
  EphyWebExtension *self = NULL;
  GFileType type;
  gsize length = 0;
  const unsigned char *manifest;

  type = g_file_query_file_type (source, G_FILE_QUERY_INFO_NONE, NULL);
  if (type == G_FILE_TYPE_DIRECTORY) {
    g_autofree char *path = g_file_get_path (source);
    self = ephy_web_extension_load_directory (path);
  } else
    self = ephy_web_extension_load_xpi (source);

  if (!self)
    return NULL;

  manifest = ephy_web_extension_get_resource (self, "manifest.json", &length);
  if (!manifest)
    return NULL;

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, (char *)manifest, length, &error)) {
    g_warning ("Could not load web extension manifest: %s", error->message);
    return NULL;
  }

  root = json_parser_get_root (parser);
  if (!root) {
    g_warning ("WebExtension manifest json root is NULL, return NULL.");
    return NULL;
  }

  root_object = json_node_get_object (root);
  if (!root_object) {
    g_warning ("WebExtension manifest json root is NULL, return NULL.");
    return NULL;
  }

  /* FIXME: Implement i18n: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Internationalization#retrieving_localized_strings_in_manifests */
  self->manifest = g_strndup ((char *)manifest, length);
  self->base_location = parent ? g_file_get_path (parent) : g_file_get_path (target);
  self->description = ephy_web_extension_manifest_get_key (self, root_object, "description");
  self->manifest_version = json_object_get_int_member (root_object, "manifest_version");
  self->name = ephy_web_extension_manifest_get_key (self, root_object, "name");
  self->version = ephy_web_extension_manifest_get_key (self, root_object, "version");
  self->homepage_url = ephy_web_extension_manifest_get_key (self, root_object, "homepage_url");
  self->author = ephy_web_extension_manifest_get_key (self, root_object, "author");

  if (json_object_has_member (root_object, "icons")) {
    icons_object = json_object_get_object_member (root_object, "icons");

    json_object_foreach_member (icons_object, web_extension_add_icon, self);
  }

  if (json_object_has_member (root_object, "content_scripts")) {
    content_scripts_array = json_object_get_array_member (root_object, "content_scripts");

    json_array_foreach_element (content_scripts_array, web_extension_add_content_script, self);
  }

  if (json_object_has_member (root_object, "background")) {
    background_object = json_object_get_object_member (root_object, "background");

    json_object_foreach_member (background_object, web_extension_add_background, self);
  }
  if (self->background)
    g_ptr_array_add (self->background->scripts, NULL);

  if (json_object_has_member (root_object, "page_action")) {
    g_autoptr (JsonObject) page_action_object = json_object_get_object_member (root_object, "page_action");

    web_extension_add_page_action (page_action_object, self);
  }

  if (json_object_has_member (root_object, "browser_action")) {
    g_autoptr (JsonObject) browser_action_object = json_object_get_object_member (root_object, "browser_action");

    web_extension_add_browser_action (browser_action_object, self);
  }

  if (json_object_has_member (root_object, "options_ui")) {
    g_autoptr (JsonObject) browser_action_object = json_object_get_object_member (root_object, "options_ui");

    web_extension_add_options_ui (browser_action_object, self);
  }

  if (json_object_has_member (root_object, "permissions")) {
    g_autoptr (JsonArray) array = json_object_get_array_member (root_object, "permissions");

    json_array_foreach_element (array, web_extension_add_permission, self);
  }
  if (self->permissions)
    g_ptr_array_add (self->permissions, NULL);

  return self;
}

EphyWebExtension *
ephy_web_extension_load_finished (GObject       *unused,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (g_task_is_valid (result, unused));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
load_web_extension_thread (GTask        *task,
                           gpointer     *unused,
                           GFile        *target,
                           GCancellable *cancellable)
{
  EphyWebExtension *self = ephy_web_extension_load (target);

  g_task_return_pointer (task, self, NULL);
}

void
ephy_web_extension_load_async (GFile               *target,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GTask *task;

  g_assert (target);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_DEFAULT);
  g_task_set_task_data (task,
                        g_file_dup (target),
                        (GDestroyNotify)g_object_unref);
  g_task_run_in_thread (task, (GTaskThreadFunc)load_web_extension_thread);
  g_object_unref (task);
}


GdkPixbuf *
ephy_web_extension_load_pixbuf (EphyWebExtension *self,
                                char             *file)
{
  g_autofree gchar *path = NULL;

  path = g_build_filename (self->base_location, file, NULL);

  return gdk_pixbuf_new_from_file (path, NULL);
}

void
ephy_web_extension_remove (EphyWebExtension *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->xpi) {
    if (!ephy_file_delete_dir_recursively (self->base_location, &error))
      g_warning ("Could not delete web_extension from %s: %s", self->base_location, error->message);
  } else {
    g_unlink (self->base_location);
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
  return !!self->background;
}

const char *
ephy_web_extension_background_web_view_get_page (EphyWebExtension *self)
{
  return self->background->page;
}

GPtrArray *
ephy_web_extension_background_web_view_get_scripts (EphyWebExtension *self)
{
  return self->background->scripts;
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
  css->style = webkit_user_style_sheet_new (css->code, WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER, NULL, NULL);

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

char *
ephy_web_extension_get_option_ui_page (EphyWebExtension *self)
{
  if (!self->options_ui)
    return NULL;

  return ephy_web_extension_get_resource_as_string (self, self->options_ui->page);
}

const char *
ephy_web_extension_get_guid (EphyWebExtension *self)
{
  return self->guid;
}

GPtrArray *
ephy_web_extension_get_permissions (EphyWebExtension *self)
{
  return self->permissions;
}
