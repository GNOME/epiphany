/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <time.h>

#include "ephy-embed-utils.h"
#include "ephy-shell.h"
#include "ephy-json-utils.h"
#include "ephy-window.h"

#include "browseraction.h"

static void
browseraction_handler_set_badge_text (EphyWebExtensionSender *sender,
                                      const char             *method_name,
                                      JsonArray              *args,
                                      GTask                  *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  JsonObject *details;
  const char *badge_text = NULL;
  gint64 tab_id = -1;
  gint64 window_id = -1;

  details = ephy_json_array_get_object (args, 0);
  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setBadgeText(): Missing details");
    return;
  }

  badge_text = ephy_json_object_get_string (details, "text");
  if (!badge_text) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeText(): Missing text");
    return;
  }

  tab_id = ephy_json_object_get_int (details, "tabId");
  window_id = ephy_json_object_get_int (details, "windowId");
  if (tab_id != -1 && window_id != -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeText(): tabId and windowId defined. Not supported");
    return;
  }

  /* Currently only Firefox can handle them, skip it for the moment */
  if (tab_id != -1 || window_id != -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeText(): tabId or windowId defined. Not supported");
    return;
  }

  ephy_web_extension_manager_browseraction_set_badge_text (manager, sender->extension, badge_text);
  g_task_return_pointer (task, NULL, NULL);
}

static void
browseraction_handler_set_badge_background_color (EphyWebExtensionSender *sender,
                                                  const char             *method_name,
                                                  JsonArray              *args,
                                                  GTask                  *task)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  JsonObject *details;
  const char *badge_color = NULL;
  gint64 tab_id = -1;
  gint64 window_id = -1;
  GdkRGBA color;

  details = ephy_json_array_get_object (args, 0);
  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "tabs.setBadgeBackgroundColor(): Missing details");
    return;
  }

  badge_color = ephy_json_object_get_string (details, "color");
  if (!badge_color) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeBackgroundColor(): Missing badge color");
    return;
  }

  tab_id = ephy_json_object_get_int (details, "tabId");
  window_id = ephy_json_object_get_int (details, "windowId");
  if (tab_id != -1 && window_id != -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeBackgroundColor(): tabId and windowId defined. Not supported");
    return;
  }

  /* Currently only Firefox can handle them, skip it for the moment */
  if (tab_id != -1 || window_id != -1) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeBackgroundColor(): tabId or windowId defined. Not supported");
    return;
  }

  if (!gdk_rgba_parse (&color, badge_color)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "browserAction.setBadgeBackgroundColor(): Failed to parse color");
    return;
  }

  ephy_web_extension_manager_browseraction_set_badge_background_color (manager, sender->extension, &color);
  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler browseraction_handlers[] = {
  {"setBadgeText", browseraction_handler_set_badge_text},
  {"setBadgeBackgroundColor", browseraction_handler_set_badge_background_color},
};

void
ephy_web_extension_api_browseraction_handler (EphyWebExtensionSender *sender,
                                              const char             *method_name,
                                              JsonArray              *args,
                                              GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (browseraction_handlers); idx++) {
    EphyWebExtensionApiHandler handler = browseraction_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, method_name);
  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}
