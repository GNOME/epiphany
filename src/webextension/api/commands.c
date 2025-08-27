/*
 *  Copyright © 2022 Igalia S.L.
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

#include "api-utils.h"
#include "commands.h"

static void
on_command_activated (GAction  *action,
                      GVariant *parameter,
                      gpointer  user_data)
{
  EphyWebExtension *self = user_data;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebView *view = EPHY_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  GtkWidget *button;
  const char *command_name = g_object_get_data (G_OBJECT (action), "command-name-json");

  if (strcmp (command_name, "\"_execute_browser_action\"") == 0) {
    ephy_web_extension_manager_show_browser_action (manager, self);
    return;
  } else if (strcmp (command_name, "\"_execute_page_action\"") == 0) {
    button = ephy_web_extension_manager_get_page_action (manager, self, view);
    gtk_widget_mnemonic_activate (button, false);
    return;
  }

  ephy_web_extension_manager_emit_in_extension_views (manager,
                                                      self,
                                                      "commands.onCommand",
                                                      command_name);
}

char *
get_accel_action_name (EphyWebExtension    *self,
                       WebExtensionCommand *command)
{
  return g_strdup_printf ("app.webextension-command-%s-%s",
                          ephy_web_extension_get_guid (self),
                          command->name);
}

char *
get_action_name (EphyWebExtension    *self,
                 WebExtensionCommand *command)
{
  return g_strdup_printf ("webextension-command-%s-%s",
                          ephy_web_extension_get_guid (self),
                          command->name);
}

static void
destroy_action (EphyWebExtension    *self,
                WebExtensionCommand *command)
{
  g_autofree char *action_name = get_action_name (self, command);
  g_autofree char *accel_action_name = get_accel_action_name (self, command);
  const char * const empty_accels[] = { NULL };
  gtk_application_set_accels_for_action (GTK_APPLICATION (ephy_shell_get_default ()),
                                         accel_action_name,
                                         empty_accels);
  g_action_map_remove_action (G_ACTION_MAP (ephy_shell_get_default ()), action_name);
}

void
set_accel_for_action (EphyWebExtension    *self,
                      WebExtensionCommand *command)
{
  g_auto (GStrv) current_actions = NULL;
  g_autofree char *action_name = NULL;

  if (!command->accelerator) {
    g_debug ("commands: Command has no accelerator, skipping");
    return;
  }

  current_actions = gtk_application_get_actions_for_accel (GTK_APPLICATION (ephy_shell_get_default ()),
                                                           command->accelerator);
  action_name = get_accel_action_name (self, command);

  if (current_actions[0]) {
    g_debug ("commands: Accelerator %s already set, not overriding", command->accelerator);
    return;
  }

  gtk_application_set_accels_for_action (GTK_APPLICATION (ephy_shell_get_default ()),
                                         action_name,
                                         (const char *[]) { command->accelerator, NULL });
}

static void
setup_action (EphyWebExtension    *self,
              WebExtensionCommand *command)
{
  g_autofree char *action_name = get_action_name (self, command);
  g_autoptr (GSimpleAction) action = g_simple_action_new (action_name, NULL);

  g_action_map_add_action (G_ACTION_MAP (ephy_shell_get_default ()), G_ACTION (action));

  set_accel_for_action (self, command);

  g_signal_connect (action, "activate", G_CALLBACK (on_command_activated), self);
  /* Lazy way to pass this info to on_command_activated(). */
  g_object_set_data_full (G_OBJECT (action), "command-name-json", g_strdup_printf ("\"%s\"", command->name), g_free);
}

static GHashTable *
get_commands (EphyWebExtension *self)
{
  return g_object_get_data (G_OBJECT (self), "commands");
}

static JsonNode *
command_to_node (WebExtensionCommand *command)
{
  JsonNode *node;
  JsonObject *obj;

  node = json_node_init_object (json_node_alloc (), json_object_new ());
  obj = json_node_get_object (node);
  json_object_set_string_member (obj, "name", command->name);
  json_object_set_string_member (obj, "shortcut", command->shortcut);
  json_object_set_string_member (obj, "description", command->description);

  return node;
}

static void
commands_handler_get_all (EphyWebExtensionSender *sender,
                          const char             *method_name,
                          JsonArray              *args,
                          GTask                  *task)
{
  GHashTable *commands = get_commands (sender->extension);
  g_autoptr (JsonNode) node = json_node_init_array (json_node_alloc (), json_array_new ());
  JsonArray *rel = json_node_get_array (node);
  GHashTableIter iter;
  WebExtensionCommand *command;

  g_hash_table_iter_init (&iter, commands);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&command))
    json_array_add_element (rel, command_to_node (command));

  g_task_return_pointer (task, json_to_string (node, FALSE), g_free);
}

static void
commands_handler_reset (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  GHashTable *default_commands = ephy_web_extension_get_commands (sender->extension);
  GHashTable *active_commands = get_commands (sender->extension);
  const char *name = ephy_json_array_get_string (args, 0);
  WebExtensionCommand *command;
  WebExtensionCommand *default_command;

  if (!name) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.reset(): Missing name argument");
    return;
  }

  command = g_hash_table_lookup (active_commands, name);
  if (!command) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.reset(): Did not find command by name %s", name);
    return;
  }

  g_assert (default_commands);
  default_command = g_hash_table_lookup (default_commands, name);
  g_assert (default_command);

  destroy_action (sender->extension, command);

  g_free (command->description);
  g_free (command->accelerator);
  g_free (command->shortcut);
  command->description = g_strdup (default_command->description);
  command->accelerator = g_strdup (default_command->accelerator);
  command->shortcut = g_strdup (default_command->shortcut);

  setup_action (sender->extension, command);

  g_task_return_pointer (task, NULL, NULL);
}

static void
commands_handler_update (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  GHashTable *commands = get_commands (sender->extension);
  JsonObject *details = ephy_json_array_get_object (args, 0);
  WebExtensionCommand *command;
  const char *name;
  const char *description;
  const char *shortcut;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.update(): Missing details object");
    return;
  }

  name = ephy_json_object_get_string (details, "name");
  if (!name) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.update(): Missing name");
    return;
  }

  command = g_hash_table_lookup (commands, name);
  if (!command) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.update(): Could not find command by name %s", name);
    return;
  }

  description = ephy_json_object_get_string (details, "description");
  if (description) {
    g_free (command->description);
    command->description = g_strdup (description);
  }

  shortcut = ephy_json_object_get_string (details, "shortcut");
  if (shortcut && !*shortcut) {
    /* Empty string is set to nothing. */
    g_free (command->accelerator);
    g_free (command->shortcut);
    command->accelerator = NULL;
    command->shortcut = NULL;
  } else if (shortcut) {
    g_autofree char *new_accelerator = ephy_web_extension_parse_command_key (shortcut);

    if (!new_accelerator) {
      g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.update(): Shortcut was invalid: '%s'", shortcut);
      return;
    }

    g_free (command->accelerator);
    g_free (command->shortcut);
    command->accelerator = g_steal_pointer (&new_accelerator);
    command->shortcut = g_strdup (shortcut);
  }

  if (shortcut) {
    set_accel_for_action (sender->extension, command);
  }

  g_task_return_pointer (task, NULL, NULL);
}

static EphyWebExtensionApiHandler commands_handlers[] = {
  {"getAll", commands_handler_get_all},
  {"reset", commands_handler_reset},
  {"update", commands_handler_update}
};

void
ephy_web_extension_api_commands_handler (EphyWebExtensionSender *sender,
                                         const char             *method_name,
                                         JsonArray              *args,
                                         GTask                  *task)
{
  for (guint idx = 0; idx < G_N_ELEMENTS (commands_handlers); idx++) {
    EphyWebExtensionApiHandler handler = commands_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, method_name);
  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}

void
ephy_web_extension_api_commands_init (EphyWebExtension *self)
{
  GHashTable *default_commands = ephy_web_extension_get_commands (self);
  GHashTable *active_commands = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)web_extension_command_free);
  GHashTableIter iter;
  WebExtensionCommand *command = NULL;

  /* We load the default commands from the manifest and set them up here. */
  if (default_commands) {
    g_hash_table_iter_init (&iter, default_commands);
    while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&command)) {
      WebExtensionCommand *new_command;
      new_command = web_extension_command_copy (command);
      g_hash_table_replace (active_commands, new_command->name, new_command);
      setup_action (self, new_command);
    }
  }

  g_object_set_data_full (G_OBJECT (self), "commands", active_commands, (GDestroyNotify)g_hash_table_unref);
}

void
ephy_web_extension_api_commands_dispose (EphyWebExtension *self)
{
  GHashTable *active_commands = get_commands (self);
  GHashTableIter iter;
  WebExtensionCommand *command = NULL;

  /* We load the default commands from the manifest and set them up here. */
  g_hash_table_iter_init (&iter, active_commands);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&command)) {
    destroy_action (self, command);
  }

  g_object_set_data (G_OBJECT (self), "commands", NULL);
}
