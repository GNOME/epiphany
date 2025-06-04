/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#include "ephy-sqlite-connection.h"
#include "ephy-sqlite-statement.h"
#include <glib.h>
#include <gtk/gtk.h>

static void
test_create_connection (void)
{
  gchar *temporary_file;
  EphySQLiteConnection *connection;
  GError *error = NULL;

  temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, temporary_file);
  g_assert_true (ephy_sqlite_connection_open (connection, &error));
  g_assert_no_error (error);
  g_assert_true (g_file_test (temporary_file, G_FILE_TEST_IS_REGULAR));

  ephy_sqlite_connection_close (connection);
  ephy_sqlite_connection_delete_database (connection);
  g_assert_false (g_file_test (temporary_file, G_FILE_TEST_IS_REGULAR));

  g_free (temporary_file);
  g_object_unref (connection);

  temporary_file = g_build_filename (g_get_tmp_dir (), "directory-that-does-not-exist", "epiphany_sqlite_test.db", NULL);
  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, temporary_file);
  g_assert_false (ephy_sqlite_connection_open (connection, &error));
  g_assert_nonnull (error);
  g_assert_false (g_file_test (temporary_file, G_FILE_TEST_IS_REGULAR));

  g_free (temporary_file);
  g_object_unref (connection);
  g_error_free (error);
}

static void
test_create_statement (void)
{
  gchar *temporary_file;
  EphySQLiteConnection *connection;
  GError *error = NULL;
  EphySQLiteStatement *statement = NULL;

  temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, temporary_file);
  g_assert_true (ephy_sqlite_connection_open (connection, &error));
  g_assert_no_error (error);

  statement = ephy_sqlite_connection_create_statement (connection, "CREATE TABLE TEST (id INTEGER)", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "BLAHBLAHBLAHBA", &error);
  g_assert_null (statement);
  g_assert_nonnull (error);
  g_clear_error (&error);

  ephy_sqlite_connection_close (connection);
  ephy_sqlite_connection_delete_database (connection);

  g_free (temporary_file);
  g_object_unref (connection);
}

static void
create_table_and_insert_row (EphySQLiteConnection *connection)
{
  GError *error = NULL;
  EphySQLiteStatement *statement = ephy_sqlite_connection_create_statement (connection, "CREATE TABLE test (id INTEGER, text LONGVARCHAR)", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);
  ephy_sqlite_statement_step (statement, &error);
  g_assert_no_error (error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "SELECT * FROM test", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);
  g_assert_false (ephy_sqlite_statement_step (statement, &error));
  g_assert_no_error (error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "INSERT INTO test (id, text) VALUES (3, \"test\")", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);
  ephy_sqlite_statement_step (statement, &error);
  g_assert_no_error (error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "SELECT * FROM test", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);

  g_assert_true (ephy_sqlite_statement_step (statement, &error));
  g_assert_no_error (error);

  g_assert_cmpint (ephy_sqlite_connection_get_last_insert_id (connection), ==, 1);
  g_assert_cmpint (ephy_sqlite_statement_get_column_count (statement), ==, 2);
  g_assert_cmpint (ephy_sqlite_statement_get_column_type (statement, 0), ==, EPHY_SQLITE_COLUMN_TYPE_INT);
  g_assert_cmpint (ephy_sqlite_statement_get_column_type (statement, 1), ==, EPHY_SQLITE_COLUMN_TYPE_STRING);

  /* Step will return false here since there is only one row. */
  g_assert_false (ephy_sqlite_statement_step (statement, &error));
  g_assert_no_error (error);
  g_object_unref (statement);
}

static void
test_create_table_and_insert_row (void)
{
  gchar *temporary_file;
  EphySQLiteConnection *connection;
  GError *error = NULL;

  temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, temporary_file);
  g_assert_true (ephy_sqlite_connection_open (connection, &error));
  g_assert_no_error (error);

  create_table_and_insert_row (connection);

  ephy_sqlite_connection_close (connection);
  ephy_sqlite_connection_delete_database (connection);

  g_free (temporary_file);
  g_object_unref (connection);
}

static void
test_bind_data (void)
{
  gchar *temporary_file;
  EphySQLiteConnection *connection;
  GError *error = NULL;
  EphySQLiteStatement *statement = NULL;

  temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, temporary_file);
  g_assert_true (ephy_sqlite_connection_open (connection, &error));
  g_assert_no_error (error);

  ephy_sqlite_connection_execute (connection, "CREATE TABLE test (id INTEGER, text LONGVARCHAR)", &error);

  statement = ephy_sqlite_connection_create_statement (connection, "INSERT INTO test (id, text) VALUES (?, ?)", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);

  g_assert_true (ephy_sqlite_statement_bind_int (statement, 0, 3, &error));
  g_assert_no_error (error);
  g_assert_true (ephy_sqlite_statement_bind_string (statement, 1, "foo", &error));
  g_assert_no_error (error);

  /* Will return false since there are no resulting rows. */
  g_assert_false (ephy_sqlite_statement_step (statement, &error));
  g_assert_no_error (error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "SELECT * FROM test", &error);
  g_assert_nonnull (statement);
  g_assert_no_error (error);
  g_assert_true (ephy_sqlite_statement_step (statement, &error));
  g_assert_no_error (error);
  g_assert_cmpint (ephy_sqlite_statement_get_column_count (statement), ==, 2);
  g_assert_cmpint (ephy_sqlite_statement_get_column_as_int (statement, 0), ==, 3);
  g_assert_cmpstr (ephy_sqlite_statement_get_column_as_string (statement, 1), ==, "foo");
  g_object_unref (statement);

  ephy_sqlite_connection_close (connection);
  ephy_sqlite_connection_delete_database (connection);

  g_object_unref (connection);
  g_free (temporary_file);
}

static void
test_table_exists (void)
{
  gchar *temporary_file;
  EphySQLiteConnection *connection;
  GError *error = NULL;

  temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_READWRITE, temporary_file);
  g_assert_true (ephy_sqlite_connection_open (connection, &error));
  g_assert_no_error (error);

  g_assert_false (ephy_sqlite_connection_table_exists (connection, "test"));
  g_assert_false (ephy_sqlite_connection_table_exists (connection, "something_fakey"));
  create_table_and_insert_row (connection);
  g_assert_true (ephy_sqlite_connection_table_exists (connection, "test"));
  g_assert_false (ephy_sqlite_connection_table_exists (connection, "something_fakey"));

  ephy_sqlite_connection_close (connection);
  ephy_sqlite_connection_delete_database (connection);

  g_object_unref (connection);
  g_free (temporary_file);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/lib/sqlite/ephy-sqlite/create_connection", test_create_connection);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/create_statement", test_create_statement);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/create_table_and_insert_row", test_create_table_and_insert_row);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/bind_data", test_bind_data);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/table_exists", test_table_exists);

  return g_test_run ();
}
