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

/* Local command struct. */
typedef struct {
  EphyWebExtension *web_extension; /* Parent object */
  char *name;
  char *accelerator;
  char *description;
} Command;

static void
command_destroy (Command *command)
{
  g_free (command->name);
  g_free (command->accelerator);
  g_free (command->description);
  g_free (command);
}

static void
on_command_activated (GAction  *action,
                      GVariant *parameter,
                      gpointer  user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  Command *cmd = user_data;
  JsonNode *node;
  node = json_node_init_string (json_node_alloc (), cmd->name);

  ephy_web_extension_manager_emit_in_extension_views (manager,
                                                      cmd->web_extension,
                                                      "commands.onCommand",
                                                      json_to_string (node, FALSE));
}

static void
setup_actions (Command *cmd)
{
  GSimpleAction *action = g_simple_action_new (cmd->name, NULL);
  g_signal_connect (action, "activate", G_CALLBACK (on_command_activated), cmd);
  g_action_map_add_action (G_ACTION_MAP (ephy_shell_get_default ()), (GAction *)action);
  gtk_application_set_accels_for_action (GTK_APPLICATION (ephy_shell_get_default ()),
                                         g_strdup_printf ("app.%s", cmd->name),
                                         (const char *[]) {
    cmd->accelerator,
    NULL,
  });
}

static Command *
create_command (EphyWebExtension *self,
                guint             cmd)
{
  char *shortcut;
  char *suggested_key;
  char *description;
  Command *command = g_new0 (Command, 1);

  ephy_web_extension_get_command_data_from_index (self,
                                                  cmd,
                                                  &shortcut,
                                                  &suggested_key,
                                                  &description);

  command->web_extension = self;
  command->name = shortcut;
  command->accelerator = suggested_key;
  command->description = description;

  setup_actions (command);

  return command;
}

static GHashTable *
get_commands (EphyWebExtension *self)
{
  GList *commands = ephy_web_extension_get_commands (self);
  Command *cmd = NULL;

  GHashTable *cmds = g_object_get_data (G_OBJECT (self), "commands");
  if (cmds)
    return cmds;

  cmds = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)command_destroy);
  g_object_set_data_full (G_OBJECT (self), "commands", cmds, (GDestroyNotify)g_hash_table_destroy);

  for (GList *list = commands; list && list->data; list = list->next) {
    cmd = create_command (self, g_list_index (list, list->data));

    g_hash_table_replace (cmds, cmd->name, cmd);
  }
  return cmds;
}

static JsonNode *
command_to_node (Command *cmd)
{
  JsonNode *node;
  JsonObject *obj;

  if (!cmd)
    return NULL;

  node = json_node_init_object (json_node_alloc (), json_object_new ());
  obj = json_node_get_object (node);
  json_object_set_string_member (obj, "name", cmd->name);
  json_object_set_string_member (obj, "shortcut", cmd->accelerator);
  json_object_set_string_member (obj, "description", cmd->description);

  return node;
}

static char *
create_accelerator (const char *orig_string)
{
  char **accelerator_keys = NULL;
  char *accelerator = "";

  /* FIXME: Stricter validation. */
  if (strchr (orig_string, '<') != NULL || strchr (orig_string, '>') != NULL)
    return NULL;

  accelerator_keys = g_strsplit ((const gchar *)orig_string, "+", 0);

  for (int i = 0; accelerator_keys[i]; i++) {
    /* We have to use 2 here, as F# keys are treated like normal keys. */
    if (strlen (accelerator_keys[i]) > 3) {
      accelerator = g_strdup_printf ("%s<%s>", accelerator, accelerator_keys[i]);
    } else {
      accelerator = g_strdup_printf ("%s%s", accelerator, accelerator_keys[i]);
    }
  }

  return accelerator;
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
  Command *cmd;

  g_hash_table_iter_init (&iter, commands);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cmd))
    json_array_add_element (rel, command_to_node (cmd));

  g_task_return_pointer (task, json_to_string (node, FALSE), g_free);
}

static void
commands_handler_reset (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  GHashTable *commands = get_commands (sender->extension);
  const char *name = ephy_json_array_get_string (args, 0);
  Command *command;
  g_autofree char *action_name = NULL;
  g_autofree char *shortcut = NULL;
  g_autofree char *suggested_key = NULL;
  g_autofree char *description = NULL;

  if (!name) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.reset(): Missing name argument");
    return;
  }

  command = g_hash_table_lookup (commands, name);
  if (!command) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.reset(): Did not find command by name %s", name);
    return;
  }

  ephy_web_extension_get_command_data_from_name (sender->extension,
                                                 name,
                                                 &shortcut,
                                                 &suggested_key,
                                                 &description);

  g_free (command->accelerator);
  g_free (command->description);
  command->description = g_steal_pointer (&description);
  command->accelerator = create_accelerator (suggested_key);

  action_name = g_strdup_printf ("app.%s", command->name);
  gtk_application_set_accels_for_action (GTK_APPLICATION (ephy_shell_get_default ()),
                                         action_name,
                                         (const char *[]) { command->accelerator, NULL });

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
  Command *command;
  g_autofree char *action_name = NULL;
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
    command->accelerator = NULL;
  } else if (shortcut) {
    g_autofree char *new_accelerator = create_accelerator (shortcut);

    if (!new_accelerator) {
      g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "commands.update(): Shortcut was invalid: '%s'", shortcut);
      return;
    }

    g_free (command->accelerator);
    command->accelerator = g_steal_pointer (&new_accelerator);
  }

  if (shortcut) {
    action_name = g_strdup_printf ("app.%s", command->name);
    gtk_application_set_accels_for_action (GTK_APPLICATION (ephy_shell_get_default ()),
                                           action_name,
                                           (const char *[]) { command->accelerator, NULL });
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
