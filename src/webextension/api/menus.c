/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright 2022 Igalia S.L.
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

#include "config.h"

#include "ephy-json-utils.h"
#include "ephy-web-view.h"
#include "ephy-shell.h"

#include "tabs.h"
#include "menus.h"

typedef enum {
  MENU_CONTEXT_AUDIO = 1 << 0,
  MENU_CONTEXT_BOOKMARK = 1 << 1,
  MENU_CONTEXT_BROWSER_ACTION = 1 << 2,
  MENU_CONTEXT_EDITABLE = 1 << 3,
  MENU_CONTEXT_FRAME = 1 << 4,
  MENU_CONTEXT_IMAGE = 1 << 5,
  MENU_CONTEXT_LINK = 1 << 6,
  MENU_CONTEXT_PAGE = 1 << 7,
  MENU_CONTEXT_PASSWORD = 1 << 8,
  MENU_CONTEXT_SELECTION = 1 << 9,
  MENU_CONTEXT_TAB = 1 << 10,
  MENU_CONTEXT_TOOLS_MENU = 1 << 11,
  MENU_CONTEXT_VIDEO = 1 << 12,
  MENU_CONTEXT_PAGE_ACTION = 1 << 13,
} MenuContext;

typedef enum {
  MENU_TYPE_NORMAL,
  MENU_TYPE_CHECKBOX,
  MENU_TYPE_RADIO,
  MENU_TYPE_SEPARATOR,
} MenuType;

typedef enum {
  VIEW_TYPE_ANY = 0,
  VIEW_TYPE_TAB = 1 << 0,
  VIEW_TYPE_POPUP = 1 << 1,
  VIEW_TYPE_SIDEBAR = 1 << 2,
} ViewType;

typedef enum {
  COMMAND_NONE,
  COMMAND_BROWSER_ACTION,
  COMMAND_PAGE_ACTION,
} Command;

typedef struct {
  char *id;
  char *parent_id;
  char *title;
  GHashTable *children;
  GStrv document_url_patterns;
  GStrv target_url_patterns;
  MenuType menu_type;
  ViewType view_type;
  Command command;
  MenuContext contexts;
  gboolean checked;
  gboolean enabled;
  gboolean visible;
} MenuItem;

static GStrv
get_strv_property (JsonObject *object,
                   const char *name)
{
  JsonNode *node = json_object_get_member (object, name);
  JsonArray *json_array;
  GPtrArray *array;
  GStrv ret;

  if (!node || json_node_get_node_type (node) != JSON_NODE_ARRAY)
    return NULL;

  json_array = json_node_get_array (node);
  array = g_ptr_array_new ();

  for (guint i = 0; i < json_array_get_length (json_array); i++) {
    const char *string = ephy_json_array_get_string (json_array, i);
    if (!string)
      continue;

    g_ptr_array_add (array, g_strdup (string));
  }

  if (array->len == 0) {
    g_ptr_array_free (array, TRUE);
    return NULL;
  }

  g_ptr_array_add (array, NULL);
  ret = (GStrv)array->pdata;
  g_ptr_array_free (array, FALSE);
  return ret;
}

static Command
get_command_property (JsonObject *object)
{
  JsonNode *node = json_object_get_member (object, "command");
  const char *command;

  command = ephy_json_node_to_string (node);
  if (!command)
    return COMMAND_NONE;
  if (strcmp (command, "_execute_browser_action") == 0)
    return COMMAND_BROWSER_ACTION;
  if (strcmp (command, "_execute_page_action") == 0)
    return COMMAND_PAGE_ACTION;

  return COMMAND_NONE;
}

static MenuType
get_menu_type_property (JsonObject *object)
{
  JsonNode *node = json_object_get_member (object, "type");
  const char *menu_type;

  menu_type = ephy_json_node_to_string (node);
  if (!menu_type)
    return MENU_TYPE_NORMAL;
  if (strcmp (menu_type, "normal") == 0)
    return MENU_TYPE_NORMAL;
  if (strcmp (menu_type, "checkbox") == 0)
    return MENU_TYPE_CHECKBOX;
  if (strcmp (menu_type, "radio") == 0)
    return MENU_TYPE_RADIO;
  if (strcmp (menu_type, "separator") == 0)
    return MENU_TYPE_SEPARATOR;

  return MENU_TYPE_NORMAL;
}

static ViewType
get_view_type_property (JsonObject *object)
{
  JsonNode *node = json_object_get_member (object, "viewTypes");
  JsonArray *view_type_array;
  ViewType view_type_flags = 0;

  if (!node || json_node_get_node_type (node) != JSON_NODE_ARRAY)
    return VIEW_TYPE_ANY;

  view_type_array = json_node_get_array (node);
  for (guint i = 0; i < json_array_get_length (view_type_array); i++) {
    const char *view_type = ephy_json_array_get_string (view_type_array, i);
    if (!view_type)
      continue;

    if (strcmp (view_type, "tab") == 0)
      view_type_flags |= VIEW_TYPE_TAB;
    else if (strcmp (view_type, "popup") == 0)
      view_type_flags |= VIEW_TYPE_POPUP;
    else if (strcmp (view_type, "sidebar") == 0)
      view_type_flags |= VIEW_TYPE_SIDEBAR;
  }

  if (view_type_flags == 0)
    return VIEW_TYPE_ANY;

  return view_type_flags;
}

typedef struct {
  const char *name;
  MenuContext value;
} ContextMap;

static const ContextMap context_map[] = {
  {
    "all", MENU_CONTEXT_AUDIO | MENU_CONTEXT_BROWSER_ACTION | MENU_CONTEXT_EDITABLE | MENU_CONTEXT_FRAME |
    MENU_CONTEXT_IMAGE | MENU_CONTEXT_LINK | MENU_CONTEXT_PAGE | MENU_CONTEXT_PASSWORD |
    MENU_CONTEXT_SELECTION | MENU_CONTEXT_VIDEO | MENU_CONTEXT_PAGE_ACTION
  },
  { "audio", MENU_CONTEXT_AUDIO },
  { "bookmark", MENU_CONTEXT_BOOKMARK },
  { "browser_action", MENU_CONTEXT_BROWSER_ACTION },
  { "editable", MENU_CONTEXT_EDITABLE },
  { "frame", MENU_CONTEXT_FRAME },
  { "image", MENU_CONTEXT_IMAGE },
  { "link", MENU_CONTEXT_LINK },
  { "page", MENU_CONTEXT_PAGE },
  { "password", MENU_CONTEXT_PASSWORD },
  { "selection", MENU_CONTEXT_SELECTION },
  { "tab", MENU_CONTEXT_TAB },
  { "tools_menu", MENU_CONTEXT_TOOLS_MENU },
  { "video", MENU_CONTEXT_VIDEO },
  { "page_action", MENU_CONTEXT_PAGE_ACTION },
};

static MenuContext
get_contexts_property (JsonObject *object)
{
  JsonNode *node = json_object_get_member (object, "contexts");
  JsonArray *contexts_array;
  MenuContext context_flags = 0;

  if (!node || !JSON_NODE_HOLDS_ARRAY (node))
    return MENU_CONTEXT_PAGE;

  contexts_array = json_node_get_array (node);
  for (guint i = 0; i < json_array_get_length (contexts_array); i++) {
    const char *context = ephy_json_array_get_string (contexts_array, i);

    if (!context)
      continue;

    for (guint j = 0; j < G_N_ELEMENTS (context_map); j++) {
      if (strcmp (context, context_map[j].name) == 0) {
        context_flags |= context_map[j].value;
        break;
      }
    }
  }

  if (context_flags == 0)
    return MENU_CONTEXT_PAGE;

  return context_flags;
}

static void
menu_item_free (MenuItem *item)
{
  if (item) {
    g_hash_table_unref (item->children);
    g_free (item->id);
    g_free (item->parent_id);
    g_free (item->title);
    g_strfreev (item->document_url_patterns);
    g_strfreev (item->target_url_patterns);
    g_free (item);
  }
}

static MenuItem *
menu_item_new (JsonObject *object)
{
  MenuItem *item = g_new0 (MenuItem, 1);

  item->id = g_strdup (ephy_json_object_get_string (object, "id"));
  item->parent_id = g_strdup (ephy_json_object_get_string (object, "parentId"));
  item->title = g_strdup (ephy_json_object_get_string (object, "title"));
  item->command = get_command_property (object);
  item->contexts = get_contexts_property (object);
  item->menu_type = get_menu_type_property (object);
  item->view_type = get_view_type_property (object);
  item->document_url_patterns = get_strv_property (object, "documentUrlPatterns");
  item->target_url_patterns = get_strv_property (object, "targetUrlPatterns");
  item->checked = json_object_get_boolean_member_with_default (object, "checked", FALSE);
  item->enabled = json_object_get_boolean_member_with_default (object, "enabled", TRUE);
  item->visible = json_object_get_boolean_member_with_default (object, "visible", TRUE);
  item->children = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)menu_item_free);

  return item;
}

static gboolean
insert_menu_item (GHashTable *menus,
                  MenuItem   *item)
{
  MenuItem *parent;
  MenuItem *next_item;
  GHashTableIter iter;

  if (!item->parent_id) {
    g_hash_table_replace (menus, item->id, item);
    return TRUE;
  }

  parent = g_hash_table_lookup (menus, item->parent_id);
  if (parent) {
    g_hash_table_replace (parent->children, item->id, item);
    return TRUE;
  }

  g_hash_table_iter_init (&iter, menus);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&next_item)) {
    if (insert_menu_item (next_item->children, item))
      return TRUE;
  }

  return FALSE;
}

static GHashTable *
get_menus (EphyWebExtension *extension)
{
  GHashTable *menus = g_object_get_data (G_OBJECT (extension), "menus");
  if (menus)
    return menus;

  menus = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)menu_item_free);
  g_object_set_data_full (G_OBJECT (extension), "menus", menus, (GDestroyNotify)g_hash_table_destroy);
  return menus;
}

static void
menus_handler_create (EphyWebExtensionSender *sender,
                      const char             *method_name,
                      JsonArray              *args,
                      GTask                  *task)
{
  JsonObject *create_properties = ephy_json_array_get_object (args, 0);
  MenuItem *item;
  GHashTable *menus = get_menus (sender->extension);

  if (!create_properties) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "menus.create(): Missing createProperties");
    return;
  }

  item = menu_item_new (create_properties);

  if (!item->id || (!item->title && item->menu_type != MENU_TYPE_SEPARATOR)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "menus.create(): createProperties is missing an id or title");
    menu_item_free (item);
    return;
  }

  if (!insert_menu_item (menus, item)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "menus.create(): parentId not found");
    menu_item_free (item);
    return;
  }

  g_task_return_pointer (task, g_strdup_printf ("\"%s\"", item->id), g_free);
}

static gboolean
menus_remove_by_id (GHashTable *menus,
                    const char *menu_id)
{
  GHashTableIter iter;
  MenuItem *next_item;

  if (!menus)
    return FALSE;

  if (g_hash_table_remove (menus, menu_id))
    return TRUE;

  g_hash_table_iter_init (&iter, menus);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&next_item)) {
    if (menus_remove_by_id (next_item->children, menu_id))
      return TRUE;
  }

  return FALSE;
}

static void
menus_handler_remove (EphyWebExtensionSender *sender,
                      const char             *method_name,
                      JsonArray              *args,
                      GTask                  *task)
{
  const char *menu_id = ephy_json_array_get_string (args, 0);

  if (!menu_id) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "menus.remove(): Missing menuId");
    return;
  }

  if (!menus_remove_by_id (get_menus (sender->extension), menu_id)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "menus.remove(): Failed to find menuId '%s'", menu_id);
    return;
  }

  g_task_return_pointer (task, NULL, NULL);
}

static void
menus_handler_remove_all (EphyWebExtensionSender *sender,
                          const char             *method_name,
                          JsonArray              *args,
                          GTask                  *task)
{
  g_object_set_data (G_OBJECT (sender->extension), "menus", NULL);
  g_task_return_pointer (task, NULL, NULL);
}

static char *
format_label (const char *label,
              const char *selected_text)
{
  GString *str = g_string_new (label);
  /* FIXME: Handle & character. */
  /* Documented here: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/menus/create */
  g_string_replace (str, "%s", selected_text, 1);
  return g_string_free (str, FALSE);
}

static char *
create_tabdata (EphyWebExtension *web_extension,
                WebKitWebView    *web_view)
{
  g_autoptr (JsonNode) tab_node = NULL;

  if (!EPHY_IS_WEB_VIEW (web_view))
    return g_strdup ("undefined");

  tab_node = ephy_web_extension_api_tabs_create_tab_object (web_extension, EPHY_WEB_VIEW (web_view));
  return json_to_string (tab_node, FALSE);
}

static void
parse_context_menu_user_data (WebKitContextMenu  *context_menu,
                              const char        **selected_text,
                              gboolean           *is_editable,
                              gboolean           *is_password)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, webkit_context_menu_get_user_data (context_menu));
  g_variant_dict_lookup (&dict, "SelectedText", "&s", selected_text);
  g_variant_dict_lookup (&dict, "IsEditable", "b", is_editable);
  g_variant_dict_lookup (&dict, "IsPassword", "b", is_password);
}

static void
builder_add_modifier_array (JsonBuilder     *builder,
                            GdkModifierType  state)
{
  json_builder_begin_array (builder);
  if (state & GDK_CONTROL_MASK)
    json_builder_add_string_value (builder, "Ctrl");
  if (state & GDK_SHIFT_MASK)
    json_builder_add_string_value (builder, "Shift");
  if (state & GDK_ALT_MASK)
    json_builder_add_string_value (builder, "Alt");
  json_builder_end_array (builder);
}

static char *
create_onclickdata (EphyWebExtension    *web_extension,
                    WebKitWebView       *web_view,
                    WebKitContextMenu   *context_menu,
                    GdkModifierType      modifiers,
                    WebKitHitTestResult *hit_test_result,
                    MenuItem            *menu_item,
                    const char          *selected_text,
                    gboolean             is_editable,
                    gboolean             is_audio,
                    gboolean             is_video)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "menuItemId");
  json_builder_add_string_value (builder, menu_item->id);
  if (menu_item->parent_id) {
    json_builder_set_member_name (builder, "parentMenuItemId");
    json_builder_add_string_value (builder, menu_item->parent_id);
  }
  if (selected_text) {
    json_builder_set_member_name (builder, "selectionText");
    json_builder_add_string_value (builder, selected_text);
  }
  json_builder_set_member_name (builder, "button");
  json_builder_add_int_value (builder, GDK_BUTTON_SECONDARY);
  json_builder_set_member_name (builder, "modifiers");
  builder_add_modifier_array (builder, modifiers);
  if (webkit_hit_test_result_context_is_link (hit_test_result)) {
    json_builder_set_member_name (builder, "linkUrl");
    json_builder_add_string_value (builder, webkit_hit_test_result_get_link_uri (hit_test_result));
    if (webkit_hit_test_result_get_link_title (hit_test_result)) {
      json_builder_set_member_name (builder, "linkTitle");
      json_builder_add_string_value (builder, webkit_hit_test_result_get_link_title (hit_test_result));
    }
  }
  if (webkit_hit_test_result_context_is_image (hit_test_result)) {
    json_builder_set_member_name (builder, "mediaType");
    json_builder_add_string_value (builder, "image");
  }
  if (is_audio || is_video) {
    json_builder_set_member_name (builder, "mediaType");
    json_builder_add_string_value (builder, is_audio ? "audio" : "video");
  }
  json_builder_set_member_name (builder, "editable");
  json_builder_add_boolean_value (builder, is_editable);
  json_builder_set_member_name (builder, "pageUrl");
  json_builder_add_string_value (builder, webkit_web_view_get_uri (web_view));
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);
  return json_to_string (root, FALSE);
}

static gboolean
item_applies_to_context (MenuContext          context,
                         gboolean             is_audio,
                         gboolean             is_video,
                         gboolean             is_editable,
                         gboolean             is_password,
                         gboolean             has_selection,
                         WebKitHitTestResult *hit_test_result)
{
  /* Only pages are currently supported. */
  if ((context & MENU_CONTEXT_PAGE))
    return TRUE;
  if (is_password && (context & MENU_CONTEXT_PASSWORD))
    return TRUE;
  if (is_audio && (context & MENU_CONTEXT_AUDIO))
    return TRUE;
  if (is_editable && (context & MENU_CONTEXT_EDITABLE))
    return TRUE;
  if (webkit_hit_test_result_context_is_image (hit_test_result) && (context & MENU_CONTEXT_IMAGE))
    return TRUE;
  if (has_selection && (context & MENU_CONTEXT_SELECTION))
    return TRUE;
  if (webkit_hit_test_result_context_is_link (hit_test_result) && (context & MENU_CONTEXT_LINK))
    return TRUE;

  return FALSE;
}

static gboolean
rules_match_uri (GStrv  rules,
                 GUri  *uri)
{
  if (!rules || !*rules)
    return TRUE;

  if (!uri)
    return FALSE;

  for (guint i = 0; rules[i]; i++) {
    if (ephy_web_extension_rule_matches_uri (rules[i], uri))
      return TRUE;
  }

  return FALSE;
}

void
menu_activate_browser_action (gpointer user_data)
{
  g_autoptr (EphyWebExtension) web_extension = user_data;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  ephy_web_extension_manager_show_browser_action (manager, web_extension);
}

void
menu_activate_page_button (gpointer user_data)
{
  g_autoptr (EphyWebExtension) web_extension = user_data;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebView *view = EPHY_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  GtkWidget *button = ephy_web_extension_manager_get_page_action (manager, web_extension, view);

  gtk_widget_mnemonic_activate (button, false);
}

void
menu_activate_command_action (GAction  *action,
                              GVariant *params,
                              gpointer  user_data)
{
  EphyWebExtension *web_extension = user_data;
  Command command = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "command"));

  if (command == COMMAND_BROWSER_ACTION) {
    g_idle_add_once (menu_activate_browser_action, g_object_ref (web_extension));
  } else if (command == COMMAND_PAGE_ACTION) {
    g_idle_add_once (menu_activate_page_button, g_object_ref (web_extension));
  }
}

static WebKitContextMenuItem *
create_context_menu_item (GHashTable          *menus,
                          const char          *name,
                          EphyWebExtension    *self,
                          WebKitWebView       *web_view,
                          GdkModifierType      modifiers,
                          WebKitContextMenu   *context_menu,
                          WebKitHitTestResult *hit_test_result,
                          GAction             *action,
                          gboolean             is_audio,
                          gboolean             is_video,
                          gboolean             is_editable,
                          gboolean             is_password,
                          const char          *selected_text,
                          const char          *tab_data,
                          GUri                *document_uri,
                          GUri                *target_uri)
{
  GHashTableIter iter;
  MenuItem *item;
  GList *menu_items = NULL;

  g_hash_table_iter_init (&iter, menus);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&item)) {
    g_autofree char *label = NULL;
    g_autofree char *onclickdata = NULL;
    WebKitContextMenuItem *menu_item;

    if (!item->visible)
      continue;

    if (!rules_match_uri (item->document_url_patterns, document_uri) ||
        !rules_match_uri (item->target_url_patterns, target_uri))
      continue;

    if (!item_applies_to_context (item->contexts, is_audio, is_video,
                                  is_editable, is_password,
                                  selected_text && *selected_text,
                                  hit_test_result))
      continue;

    if (item->view_type != VIEW_TYPE_ANY && !(item->view_type & VIEW_TYPE_TAB))
      continue;

    /* FIXME: We don't properly support checked and radio items but we still show them for now. */
    if (item->menu_type == MENU_TYPE_SEPARATOR) {
      menu_item = webkit_context_menu_item_new_separator ();
    } else if (g_hash_table_size (item->children)) {
      label = format_label (item->title, selected_text ? selected_text : "");
      menu_item = create_context_menu_item (item->children, label, self, web_view, modifiers, context_menu, hit_test_result, action,
                                            is_audio, is_video, is_editable, is_password, selected_text, tab_data,
                                            document_uri, target_uri);

      /* If the menus root is an item with the same name as the extension we use it as the root.
       * otherwise we create a new root below. */
      if (!item->parent_id && g_hash_table_size (menus) == 1 && strcmp (item->title, name) == 0) {
        return menu_item;
      }
    } else {
      label = format_label (item->title, selected_text ? selected_text : "");
      onclickdata = create_onclickdata (self, web_view, context_menu, modifiers, hit_test_result,
                                        item, selected_text, is_editable, is_audio, is_video);
      menu_item = webkit_context_menu_item_new_from_gaction (action, label,
                                                             g_variant_new ("(sss)",
                                                                            ephy_web_extension_get_guid (self),
                                                                            onclickdata,
                                                                            tab_data));
    }

    if (item->command != COMMAND_NONE) {
      g_object_set_data (G_OBJECT (action), "command", GINT_TO_POINTER (item->command));
      g_signal_connect (action, "activate",
                        G_CALLBACK (menu_activate_command_action),
                        self);
    }

    menu_items = g_list_append (menu_items, menu_item);
  }

  return webkit_context_menu_item_new_with_submenu (name, webkit_context_menu_new_with_items (menu_items));
}

WebKitContextMenuItem *
ephy_web_extension_api_menus_create_context_menu (EphyWebExtension    *self,
                                                  WebKitWebView       *web_view,
                                                  WebKitContextMenu   *context_menu,
                                                  WebKitHitTestResult *hit_test_result,
                                                  GdkModifierType      modifiers,
                                                  gboolean             is_audio,
                                                  gboolean             is_video)
{
  GHashTable *menus = g_object_get_data (G_OBJECT (self), "menus");
  GAction *action;
  g_autofree char *tab_data = NULL;
  const char *selected_text;
  gboolean is_editable;
  gboolean is_password;
  GUri *document_uri;
  GUri *target_uri = NULL;

  if (!menus)
    return NULL;

  parse_context_menu_user_data (context_menu, &selected_text, &is_editable, &is_password);
  tab_data = create_tabdata (self, web_view);
  action = g_action_map_lookup_action (G_ACTION_MAP (ephy_shell_get_default ()), "webextension-context-menu");
  g_assert (action);

  document_uri = g_uri_parse (webkit_web_view_get_uri (web_view), G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
  if (webkit_hit_test_result_get_link_uri (hit_test_result))
    target_uri = g_uri_parse (webkit_hit_test_result_get_link_uri (hit_test_result), G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_SCHEME_NORMALIZE, NULL);

  return create_context_menu_item (menus, ephy_web_extension_get_short_name (self), self, web_view, modifiers,
                                   context_menu, hit_test_result, action, is_audio, is_video, is_editable,
                                   is_password, selected_text, tab_data, document_uri, target_uri);
}

static EphyWebExtensionApiHandler menus_handlers[] = {
  {"create", menus_handler_create},
  {"remove", menus_handler_remove},
  {"remove_all", menus_handler_remove_all},
};

void
ephy_web_extension_api_menus_handler (EphyWebExtensionSender *sender,
                                      const char             *method_name,
                                      JsonArray              *args,
                                      GTask                  *task)
{
  /* We slightly differ from Firefox here that either permission works for either API but they are identical. */
  if (!ephy_web_extension_has_permission (sender->extension, "menus") && !ephy_web_extension_has_permission (sender->extension, "contextMenus")) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (menus_handlers); idx++) {
    EphyWebExtensionApiHandler handler = menus_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
