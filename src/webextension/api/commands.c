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
command_destroy (Command *cmd)
{
  /* g_clear_pointer (&cmd->name); */
  g_free (cmd);
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
  command->name = g_strdup (shortcut);
  command->accelerator = g_strdup (suggested_key);
  command->description = g_strdup (description);

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

char *
create_accelerator (char *orig_string)
{
  char **accelerator_keys = NULL;
  char *accelerator = "";

  if (strchr (orig_string, '<') != NULL || strchr (orig_string, '>') != NULL)
    return orig_string;

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

static char *
commands_handler_get_all (EphyWebExtension  *self,
                          char              *name,
                          JSCValue          *args,
                          WebKitWebView     *web_view,
                          GError           **error)
{
  GHashTable *commands = get_commands (self);
  g_autoptr (JsonNode) node = json_node_init_array (json_node_alloc (), json_array_new ());
  JsonArray *rel = json_node_get_array (node);
  GHashTableIter iter;
  Command *cmd;

  g_hash_table_iter_init (&iter, commands);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cmd))
    json_array_add_element (rel, command_to_node (cmd));

  return json_to_string (node, FALSE);
}

static char *
commands_handler_reset (EphyWebExtension  *self,
                        char              *name,
                        JSCValue          *args,
                        WebKitWebView     *web_view,
                        GError           **error)
{
  GHashTable *commands = get_commands (self);
  g_autoptr (JSCValue) name_value = jsc_value_object_get_property_at_index (args, 0);
  g_autofree char *name_str = NULL;
  Command *cmd = NULL;
  char *shortcut;
  char *suggested_key;
  char *description;

  if (!jsc_value_is_string (name_value))
    name_str = g_strdup ("");
  else
    name_str = jsc_value_to_string (name_value);

  if (g_hash_table_lookup (commands, name_str)) {
    cmd = g_hash_table_lookup (commands, name_str);
    ephy_web_extension_get_command_data_from_name (self,
                                                   name_str,
                                                   &shortcut,
                                                   &suggested_key,
                                                   &description);

    cmd->name = g_strdup (shortcut);
    cmd->accelerator = g_strdup (suggested_key);
    cmd->description = g_strdup (description);
  }

  return NULL;
}

static char *
commands_handler_update (EphyWebExtension  *self,
                         char              *name,
                         JSCValue          *args,
                         WebKitWebView     *web_view,
                         GError           **error)
{
  GHashTable *commands = get_commands (self);
  g_autoptr (JSCValue) obj = jsc_value_object_get_property_at_index (args, 0);
  Command *cmd = NULL;
  g_autofree char *name_str = NULL;
  g_autofree char *desc_str = NULL;
  g_autofree char *shortcut_str = NULL;

  if (!jsc_value_is_object (obj))
    return NULL;
  else {
    if (!jsc_value_object_has_property (obj, "name"))
      return NULL;
    else
      name_str = jsc_value_to_string (jsc_value_object_get_property (obj, "name"));

    if (jsc_value_object_has_property (obj, "description"))
      desc_str = jsc_value_to_string (jsc_value_object_get_property (obj, "description"));

    if (jsc_value_object_has_property (obj, "shortcut")) {
      shortcut_str = jsc_value_to_string (jsc_value_object_get_property (obj, "shortcut"));
      shortcut_str = create_accelerator (shortcut_str);
    }
  }

  if (g_hash_table_lookup (commands, name_str)) {
    cmd = g_hash_table_lookup (commands, name_str);
    cmd->name = g_strdup (name_str);
    cmd->accelerator = g_strdup (shortcut_str);
    cmd->description = g_strdup (desc_str);
    gtk_application_set_accels_for_action (GTK_APPLICATION (ephy_shell_get_default ()),
                                           g_strdup_printf ("app.%s", cmd->name),
                                           (const char *[]) {
      cmd->accelerator,
      NULL,
    });
  }

  return NULL;
}

static EphyWebExtensionSyncApiHandler commands_handlers[] = {
  {"getAll", commands_handler_get_all},
  {"reset", commands_handler_reset},
  {"update", commands_handler_update}
};

void
ephy_web_extension_api_commands_handler (EphyWebExtension *self,
                                         char             *name,
                                         JSCValue         *args,
                                         WebKitWebView    *web_view,
                                         GTask            *task)
{
  g_autoptr (GError) error = NULL;

  for (guint idx = 0; idx < G_N_ELEMENTS (commands_handlers); idx++) {
    EphyWebExtensionSyncApiHandler handler = commands_handlers[idx];
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
