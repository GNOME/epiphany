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

#include "ephy-shell.h"
#include "ephy-web-extension.h"
#include "ephy-window.h"

#include "pageaction.h"

static GtkWidget *
pageaction_get_action (EphyWebExtension *self,
                       JSCValue         *value)
{
  EphyWebView *web_view = NULL;
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autoptr (JSCValue) tab_id = NULL;
  gint32 nr;

  if (!jsc_value_is_object (value))
    return NULL;

  tab_id = jsc_value_object_get_property (value, "tabId");
  if (!jsc_value_is_number (tab_id))
    return NULL;

  nr = jsc_value_to_int32 (tab_id);
  web_view = ephy_shell_get_web_view (shell, nr);
  if (!web_view) {
    LOG ("%s(): Invalid tabId '%d', abort\n", __FUNCTION__, nr);
    return NULL;
  }

  return ephy_web_extension_manager_get_page_action (manager, self, web_view);
}

static char *
pageaction_handler_seticon (EphyWebExtension  *self,
                            char              *name,
                            JSCValue          *args,
                            WebKitWebView     *web_view,
                            GError           **error)
{
  GtkWidget *action;
  g_autofree char *path_str = NULL;
  g_autoptr (JSCValue) path_value = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);

  action = pageaction_get_action (self, value);
  if (!action) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return NULL;
  }

  path_value = jsc_value_object_get_property (value, "path");
  if (jsc_value_is_string (path_value)) {
    path_str = jsc_value_to_string (path_value);
    pixbuf = ephy_web_extension_load_pixbuf (self, path_str, -1);
  } else {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "pageAction.setIcon(): Currently only single path strings are supported.");
    return NULL;
  }

  gtk_image_set_from_pixbuf (GTK_IMAGE (gtk_bin_get_child (GTK_BIN (action))), pixbuf);
  return NULL;
}

static char *
pageaction_handler_settitle (EphyWebExtension  *self,
                             char              *name,
                             JSCValue          *args,
                             WebKitWebView     *web_view,
                             GError           **error)
{
  GtkWidget *action;
  g_autoptr (JSCValue) title = NULL;
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);

  action = pageaction_get_action (self, value);
  if (!action) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return NULL;
  }

  title = jsc_value_object_get_property (value, "title");
  gtk_widget_set_tooltip_text (action, jsc_value_to_string (title));

  return NULL;
}

static char *
pageaction_handler_gettitle (EphyWebExtension  *self,
                             char              *name,
                             JSCValue          *args,
                             WebKitWebView     *web_view,
                             GError           **error)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);
  GtkWidget *action;
  g_autofree char *title = NULL;

  action = pageaction_get_action (self, value);
  if (!action) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return NULL;
  }

  title = gtk_widget_get_tooltip_text (action);

  return g_strdup_printf ("\"%s\"", title ? title : "");
}

static char *
pageaction_handler_show (EphyWebExtension  *self,
                         char              *name,
                         JSCValue          *args,
                         WebKitWebView     *web_view,
                         GError           **error)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);
  GtkWidget *action;

  action = pageaction_get_action (self, value);
  if (!action) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return NULL;
  }

  gtk_widget_set_visible (action, TRUE);

  return NULL;
}

static char *
pageaction_handler_hide (EphyWebExtension  *self,
                         char              *name,
                         JSCValue          *args,
                         WebKitWebView     *web_view,
                         GError           **error)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property_at_index (args, 0);
  GtkWidget *action;

  action = pageaction_get_action (self, value);
  if (!action) {
    g_set_error_literal (error, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "Invalid Arguments");
    return NULL;
  }

  gtk_widget_set_visible (action, FALSE);

  return NULL;
}

static EphyWebExtensionSyncApiHandler pageaction_handlers[] = {
  {"setIcon", pageaction_handler_seticon},
  {"setTitle", pageaction_handler_settitle},
  {"getTitle", pageaction_handler_gettitle},
  {"show", pageaction_handler_show},
  {"hide", pageaction_handler_hide},
};

void
ephy_web_extension_api_pageaction_handler (EphyWebExtension *self,
                                           char             *name,
                                           JSCValue         *args,
                                           WebKitWebView    *web_view,
                                           GTask            *task)
{
  g_autoptr (GError) error = NULL;
  guint idx;

  for (idx = 0; idx < G_N_ELEMENTS (pageaction_handlers); idx++) {
    EphyWebExtensionSyncApiHandler handler = pageaction_handlers[idx];
    char *ret;

    if (g_strcmp0 (handler.name, name) == 0) {
      ret = handler.execute (self, name, args, web_view, &error);

      if (error)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_pointer (task, ret, g_free);

      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  error = g_error_new_literal (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
  g_task_return_error (task, g_steal_pointer (&error));
}
