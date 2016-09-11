/*
 *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include "ephy-sqlite-statement.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SQLITE_CONNECTION (ephy_sqlite_connection_get_type ())

G_DECLARE_FINAL_TYPE (EphySQLiteConnection, ephy_sqlite_connection, EPHY, SQLITE_CONNECTION, GObject)

EphySQLiteConnection *  ephy_sqlite_connection_new                     (void);

gboolean                ephy_sqlite_connection_open                    (EphySQLiteConnection *self, const gchar *filename, GError **error);
void                    ephy_sqlite_connection_close                   (EphySQLiteConnection *self);

void                    ephy_sqlite_connection_get_error               (EphySQLiteConnection *self, GError **error);

gboolean                ephy_sqlite_connection_execute                 (EphySQLiteConnection *self, const char *sql, GError **error);
EphySQLiteStatement *   ephy_sqlite_connection_create_statement        (EphySQLiteConnection *self, const char *sql, GError **error);
gint64                  ephy_sqlite_connection_get_last_insert_id      (EphySQLiteConnection *self);

gboolean                ephy_sqlite_connection_begin_transaction       (EphySQLiteConnection *self, GError **error);
gboolean                ephy_sqlite_connection_rollback_transaction    (EphySQLiteConnection *self, GError **error);
gboolean                ephy_sqlite_connection_commit_transaction      (EphySQLiteConnection *self, GError **error);

gboolean                ephy_sqlite_connection_table_exists            (EphySQLiteConnection *self, const char *table_name);

G_END_DECLS
