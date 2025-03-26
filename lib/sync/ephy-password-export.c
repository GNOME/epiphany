/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Harshavardhan Navalli
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

#include "ephy-password-manager.h"

#include <glib/gi18n.h>

typedef char *(*GetColumnFieldFunction)(EphyPasswordRecord *);

typedef struct {
  const char *name;
  GetColumnFieldFunction get_column_field_from_password_record;
} PasswordExportCSVColumn;

static char *
get_name_from_password_record (EphyPasswordRecord *record)
{
  const char *origin;
  g_autoptr (GUri) uri = NULL;

  origin = ephy_password_record_get_origin (record);
  uri = g_uri_parse (origin, G_URI_FLAGS_NONE, NULL);
  if (!uri)
    return NULL;

  return g_strdup (g_uri_get_host (uri));
}

static char *
get_url_from_password_record (EphyPasswordRecord *record)
{
  return g_strdup (ephy_password_record_get_target_origin (record));
}

static char *
get_username_from_password_record (EphyPasswordRecord *record)
{
  return g_strdup (ephy_password_record_get_username (record));
}

static char *
get_password_from_password_record (EphyPasswordRecord *record)
{
  return g_strdup (ephy_password_record_get_password (record));
}

static char *
get_note_from_password_record (EphyPasswordRecord *record)
{
  return NULL;
}

static PasswordExportCSVColumn password_export_csv_columns[] = {
  { "name", get_name_from_password_record },
  { "url", get_url_from_password_record },
  { "username", get_username_from_password_record },
  { "password", get_password_from_password_record },
  { "note", get_note_from_password_record }
};

static char *
escape_csv_field (const char *field)
{
  g_autoptr (GString) escaped_field = NULL;
  gboolean must_escape = FALSE;
  char *result;

  escaped_field = g_string_new ("");

  for (int i = 0; field[i] != '\0'; i++) {
    if (field[i] == ' ' || field[i] == ',' || field[i] == '\"')
      must_escape = TRUE;
  }

  for (int i = 0; field[i] != '\0'; i++) {
    g_string_append_c (escaped_field, field[i]);

    if (field[i] == '\"' && must_escape)
      g_string_append_c (escaped_field, '\"');
  }

  if (must_escape) {
    g_string_prepend_c (escaped_field, '\"');
    g_string_append_c (escaped_field, '\"');
  }

  result = g_string_free_and_steal (escaped_field);
  escaped_field = NULL;

  return g_steal_pointer (&result);
}

static void
file_contents_replaced_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  g_autoptr (GTask) task;
  GFile *file = G_FILE (source_object);
  GError *error = NULL;

  task = G_TASK (user_data);
  g_file_replace_contents_finish (file, res, NULL, &error);

  if (error) {
    g_prefix_error (&error, _("Error in exporting passwords to a CSV file"));
    g_task_return_error (g_steal_pointer (&task), error);
    return;
  }

  g_task_return_boolean (g_steal_pointer (&task), TRUE);
}

static void
ephy_password_manager_query_cb (GList    *records,
                                gpointer  user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GString) csv = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GFile) password_export_file = NULL;
  GCancellable *cancellable;

  int number_of_columns = sizeof (password_export_csv_columns) / sizeof (PasswordExportCSVColumn);

  task = G_TASK (user_data);
  cancellable = g_task_get_cancellable (task);

  if (g_task_return_error_if_cancelled (task))
    return;

  password_export_file = (GFile *)g_task_get_task_data (task);
  csv = g_string_new ("");

  for (int i = 0; i < number_of_columns; i++) {
    const char *column_name;
    column_name = password_export_csv_columns[i].name;

    g_string_append (csv, column_name);

    if (i < (number_of_columns - 1))
      g_string_append (csv, ",");
  }

  g_string_append (csv, "\n");

  for (GList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record;
    record = EPHY_PASSWORD_RECORD (l->data);

    for (int i = 0; i < number_of_columns; i++) {
      g_autofree char *column_field = NULL;

      column_field = password_export_csv_columns[i].get_column_field_from_password_record (record);
      if (column_field) {
        g_autofree char *escaped_column_field = escape_csv_field (column_field);
        g_string_append (csv, escaped_column_field);
      }

      if (i < (number_of_columns - 1))
        g_string_append (csv, ",");
    }

    g_string_append (csv, "\n");
  }

  bytes = g_bytes_new (csv->str, csv->len);

  g_file_replace_contents_bytes_async (g_steal_pointer (&password_export_file), bytes, NULL, FALSE,
                                       G_FILE_CREATE_NONE, cancellable, file_contents_replaced_cb, g_steal_pointer (&task));
}

void
ephy_password_export (EphyPasswordManager *manager,
                      const char          *filename,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  GFile *password_export_file;

  task = g_task_new (manager, cancellable, callback, user_data);
  password_export_file = g_file_new_for_path (filename);

  g_task_set_task_data (task, password_export_file, (GDestroyNotify)g_object_unref);

  ephy_password_manager_query (manager, NULL, NULL, NULL, NULL, NULL, NULL,
                               ephy_password_manager_query_cb, g_steal_pointer (&task));
}

gboolean
ephy_passwords_export_finish (EphyPasswordManager  *manager,
                              GAsyncResult         *result,
                              GError              **error)
{
  g_autoptr (GTask) task = NULL;

  g_assert (g_task_is_valid (result, manager));

  task = G_TASK (result);

  return g_task_propagate_boolean (task, error);
}
