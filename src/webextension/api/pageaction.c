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

#include "config.h"
#include "pageaction.h"

#include "ephy-pixbuf-utils.h"
#include "ephy-shell.h"
#include "ephy-web-extension.h"
#include "ephy-window.h"

static GtkWidget *
get_action_for_tab_id (EphyWebExtension *extension,
                       gint64            tab_id)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebView *web_view;

  if (tab_id <= 0)
    return NULL;

  web_view = ephy_shell_get_web_view (shell, tab_id);
  if (!web_view)
    return NULL;

  return ephy_web_extension_manager_get_page_action (manager, extension, web_view);
}

static void
pageaction_handler_seticon (EphyWebExtensionSender *sender,
                            const char             *method_name,
                            JsonArray              *args,
                            GTask                  *task)
{
  JsonObject *details = ephy_json_array_get_object (args, 0);
  gint64 tab_id;
  const char *path;
  GtkWidget *action, *child;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GdkTexture) texture = NULL;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.setIcon(): Missing details object");
    return;
  }

  tab_id = ephy_json_object_get_int (details, "tabId");
  action = get_action_for_tab_id (sender->extension, tab_id);
  if (!action) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.setIcon(): Failed to find action by tabId");
    return;
  }

  if (ephy_json_object_get_object (details, "path")) {
    JsonObject *path_obj = ephy_json_object_get_object (details, "path");

    path = ephy_json_object_get_string (path_obj, "16");
    if (!path)
      path = ephy_json_object_get_string (path_obj, "32");
    if (!path)
      path = ephy_json_object_get_string (path_obj, "48");
    if (!path)
      path = ephy_json_object_get_string (path_obj, "128");
    if (!path) {
      GList *members = json_object_get_members (path_obj);

      if (members)
        path = json_object_get_string_member (path_obj, (const char *)members->data);
      g_list_free (members);
    }
  } else {
    path = ephy_json_object_get_string (details, "path");
  }

  if (path && *path)
    pixbuf = ephy_web_extension_load_pixbuf (sender->extension, path, -1);

  if (!pixbuf) {
    const char *default_icon = ephy_web_extension_get_page_action_icon (sender->extension);

    if (default_icon)
      pixbuf = ephy_web_extension_load_pixbuf (sender->extension, default_icon, 16);
  }

  if (!pixbuf)
    pixbuf = ephy_web_extension_get_icon (sender->extension, 16);

  if (pixbuf)
    texture = ephy_texture_new_for_pixbuf (pixbuf);

  /* action can be a GtkButton or GtkMenuButton. They both have a "child" property */
  g_object_get (action, "child", &child, NULL);

  gtk_image_set_from_paintable (GTK_IMAGE (child), GDK_PAINTABLE (texture));
  g_task_return_pointer (task, NULL, NULL);
}

static void
pageaction_handler_settitle (EphyWebExtensionSender *sender,
                             const char             *method_name,
                             JsonArray              *args,
                             GTask                  *task)
{
  JsonObject *details = ephy_json_array_get_object (args, 0);
  gint64 tab_id;
  const char *title;
  GtkWidget *action;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.setTitle(): Missing details object");
    return;
  }

  tab_id = ephy_json_object_get_int (details, "tabId");
  action = get_action_for_tab_id (sender->extension, tab_id);
  if (!action) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.setTitle(): Failed to find action by tabId");
    return;
  }

  title = ephy_json_object_get_string (details, "title");
  if (!title)
    title = ephy_web_extension_get_page_action_title (sender->extension);

  gtk_widget_set_tooltip_text (action, title);
  g_task_return_pointer (task, NULL, NULL);
}

static void
pageaction_handler_gettitle (EphyWebExtensionSender *sender,
                             const char             *method_name,
                             JsonArray              *args,
                             GTask                  *task)
{
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  GtkWidget *action;
  const char *title = NULL;

  action = get_action_for_tab_id (sender->extension, tab_id);
  if (!action) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.getTitle(): Failed to find action by tabId");
    return;
  }

  title = gtk_widget_get_tooltip_text (action);
  if (!title)
    title = ephy_web_extension_get_page_action_title (sender->extension);

  g_task_return_pointer (task, g_strdup_printf ("\"%s\"", title ? title : ""), g_free);
}

static void
pageaction_handler_show (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  GtkWidget *action;

  action = get_action_for_tab_id (sender->extension, tab_id);
  if (!action) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.show(): Failed to find action by tabId");
    return;
  }

  gtk_widget_set_visible (action, TRUE);
  g_task_return_pointer (task, NULL, NULL);
}

static void
pageaction_handler_hide (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  gint64 tab_id = ephy_json_array_get_int (args, 0);
  GtkWidget *action;

  action = get_action_for_tab_id (sender->extension, tab_id);
  if (!action) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.hide(): Failed to find action by tabId");
    return;
  }

  gtk_widget_set_visible (action, FALSE);
  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler pageaction_handlers[] = {
  {"setIcon", pageaction_handler_seticon},
  {"setTitle", pageaction_handler_settitle},
  {"getTitle", pageaction_handler_gettitle},
  {"show", pageaction_handler_show},
  {"hide", pageaction_handler_hide},
};

void
ephy_web_extension_api_pageaction_handler (EphyWebExtensionSender *sender,
                                           const char             *method_name,
                                           JsonArray              *args,
                                           GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (pageaction_handlers); idx++) {
    EphyWebExtensionApiHandler handler = pageaction_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
