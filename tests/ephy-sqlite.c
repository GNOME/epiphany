/*
 * ephy-sqlite-statement.c
 * This file is part of Epiphany
 *
 * Copyright © 2011 Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "ephy-sqlite-connection.h"
#include "ephy-sqlite-statement.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

static EphySQLiteConnection *
ensure_empty_database (const char* filename)
{
  EphySQLiteConnection *connection = ephy_sqlite_connection_new ();
  GError *error = NULL;

  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    g_unlink (filename);

  g_assert (ephy_sqlite_connection_open (connection, filename, &error));
  g_assert (!error);
  return connection;
}

static void
test_create_connection (void)
{
  GError *error = NULL;
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);

  EphySQLiteConnection *connection = ensure_empty_database (temporary_file);
  ephy_sqlite_connection_close (connection);

  g_assert ( g_file_test (temporary_file, G_FILE_TEST_IS_REGULAR));
  g_unlink (temporary_file);
  g_assert ( !g_file_test (temporary_file, G_FILE_TEST_IS_REGULAR));
  g_free (temporary_file);

  temporary_file = g_build_filename (g_get_tmp_dir (), "directory-that-does-not-exist", "epiphany_sqlite_test.db", NULL);
  g_assert (!ephy_sqlite_connection_open (connection, temporary_file, &error));
  g_assert (error);
  g_assert (!g_file_test (temporary_file, G_FILE_TEST_IS_REGULAR));
  g_object_unref (connection);
}


static void
test_create_statement (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  EphySQLiteConnection* connection = ensure_empty_database (temporary_file);
  GError *error = NULL;
  EphySQLiteStatement *statement = NULL;

  statement = ephy_sqlite_connection_create_statement (connection, "CREATE TABLE TEST (id INTEGER)", &error);
  g_assert (statement);
  g_assert (!error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "BLAHBLAHBLAHBA", &error);
  g_assert (!statement);
  g_assert (error);

  ephy_sqlite_connection_close (connection);
  g_unlink (temporary_file);
  g_free (temporary_file);

  g_object_unref (connection);
}

static void
create_table_and_insert_row (EphySQLiteConnection *connection)
{
  GError *error = NULL;
  EphySQLiteStatement *statement = ephy_sqlite_connection_create_statement (connection, "CREATE TABLE test (id INTEGER, text LONGVARCHAR)", &error);
  g_assert (statement);
  g_assert (!error);
  ephy_sqlite_statement_step (statement, &error);
  g_assert (!error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "SELECT * FROM test", &error);
  g_assert (statement);
  g_assert (!error);
  g_assert (!ephy_sqlite_statement_step (statement, &error));
  g_assert (!error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "INSERT INTO test (id, text) VALUES (3, \"test\")", &error);
  g_assert (statement);
  g_assert (!error);
  ephy_sqlite_statement_step (statement, &error);
  g_assert (!error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "SELECT * FROM test", &error);
  g_assert (statement);
  g_assert (!error);

  g_assert (ephy_sqlite_statement_step (statement, &error));
  g_assert (!error);

  g_assert_cmpint (ephy_sqlite_connection_get_last_insert_id (connection), ==, 1);
  g_assert_cmpint (ephy_sqlite_statement_get_column_count (statement), ==, 2);
  g_assert_cmpint (ephy_sqlite_statement_get_column_type (statement, 0), ==, EPHY_SQLITE_COLUMN_TYPE_INT);
  g_assert_cmpint (ephy_sqlite_statement_get_column_type (statement, 1), ==, EPHY_SQLITE_COLUMN_TYPE_STRING);

  /* Step will return false here since there is only one row. */
  g_assert (!ephy_sqlite_statement_step (statement, &error));
  g_object_unref (statement);
}

static void
test_create_table_and_insert_row (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  EphySQLiteConnection* connection = ensure_empty_database (temporary_file);

  create_table_and_insert_row (connection);

  g_object_unref (connection);
  g_unlink (temporary_file);
  g_free (temporary_file);
}

static void
test_bind_data (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  EphySQLiteConnection* connection = ensure_empty_database (temporary_file);
  GError *error = NULL;
  EphySQLiteStatement *statement = NULL;

  ephy_sqlite_connection_execute (connection, "CREATE TABLE test (id INTEGER, text LONGVARCHAR)", &error);

  statement = ephy_sqlite_connection_create_statement (connection, "INSERT INTO test (id, text) VALUES (?, ?)", &error);
  g_assert (statement);
  g_assert (!error);

  g_assert (ephy_sqlite_statement_bind_int (statement, 0, 3, &error));
  g_assert (!error);
  g_assert (ephy_sqlite_statement_bind_string (statement, 1, "foo", &error));
  g_assert (!error);

  /* Will return false since there are no resulting rows. */
  g_assert (!ephy_sqlite_statement_step (statement, &error));
  g_assert (!error);
  g_object_unref (statement);

  statement = ephy_sqlite_connection_create_statement (connection, "SELECT * FROM test", &error);
  g_assert (statement);
  g_assert (!error);
  g_assert (ephy_sqlite_statement_step (statement, &error));
  g_assert (!error);
  g_assert_cmpint (ephy_sqlite_statement_get_column_count (statement), ==, 2);
  g_assert_cmpint (ephy_sqlite_statement_get_column_as_int (statement, 0), ==, 3);
  g_assert_cmpstr (ephy_sqlite_statement_get_column_as_string (statement, 1), ==, "foo");

  g_object_unref (connection);
  g_unlink (temporary_file);
  g_free (temporary_file);
}

static void
test_table_exists (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-sqlite-test.db", NULL);
  EphySQLiteConnection* connection = ensure_empty_database (temporary_file);

  g_assert (!ephy_sqlite_connection_table_exists (connection, "test"));
  g_assert (!ephy_sqlite_connection_table_exists (connection, "something_fakey"));
  create_table_and_insert_row (connection);
  g_assert (ephy_sqlite_connection_table_exists (connection, "test"));
  g_assert (!ephy_sqlite_connection_table_exists (connection, "something_fakey"));

  g_object_unref (connection);
  g_unlink (temporary_file);
  g_free (temporary_file);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/lib/sqlite/ephy-sqlite/create_connection", test_create_connection);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/create_statement", test_create_statement);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/create_table_and_insert_row", test_create_table_and_insert_row);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/bind_data", test_bind_data);
  g_test_add_func ("/lib/sqlite/ephy-sqlite/table_exists", test_table_exists);

  return g_test_run ();
}
