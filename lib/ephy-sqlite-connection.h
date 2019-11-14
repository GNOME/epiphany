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

#pragma once

#include <glib-object.h>
#include "ephy-sqlite-statement.h"

#include <sqlite3.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SQLITE_CONNECTION (ephy_sqlite_connection_get_type ())

G_DECLARE_FINAL_TYPE (EphySQLiteConnection, ephy_sqlite_connection, EPHY, SQLITE_CONNECTION, GObject)

typedef enum {
  EPHY_SQLITE_CONNECTION_MODE_MEMORY,
  EPHY_SQLITE_CONNECTION_MODE_READWRITE
} EphySQLiteConnectionMode;

EphySQLiteConnection *  ephy_sqlite_connection_new                     (EphySQLiteConnectionMode  mode, const char *database_path);

gboolean                ephy_sqlite_connection_open                    (EphySQLiteConnection *self, GError **error);
void                    ephy_sqlite_connection_close                   (EphySQLiteConnection *self);
void                    ephy_sqlite_connection_delete_database         (EphySQLiteConnection *self);

void                    ephy_sqlite_connection_get_error               (EphySQLiteConnection *self, GError **error);

gboolean                ephy_sqlite_connection_execute                 (EphySQLiteConnection *self, const char *sql, GError **error);
EphySQLiteStatement *   ephy_sqlite_connection_create_statement        (EphySQLiteConnection *self, const char *sql, GError **error);
gint64                  ephy_sqlite_connection_get_last_insert_id      (EphySQLiteConnection *self);
void                    ephy_sqlite_connection_enable_foreign_keys     (EphySQLiteConnection *self);

gboolean                ephy_sqlite_connection_begin_transaction       (EphySQLiteConnection *self, GError **error);
gboolean                ephy_sqlite_connection_commit_transaction      (EphySQLiteConnection *self, GError **error);

gboolean                ephy_sqlite_connection_table_exists            (EphySQLiteConnection *self, const char *table_name);

GQuark                  ephy_sqlite_error_quark                        (void);

#define EPHY_SQLITE_ERROR ephy_sqlite_error_quark ()

G_END_DECLS
