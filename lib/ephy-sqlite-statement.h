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
#include "ephy-sqlite.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SQLITE_STATEMENT (ephy_sqlite_statement_get_type ())

G_DECLARE_FINAL_TYPE (EphySQLiteStatement, ephy_sqlite_statement, EPHY, SQLITE_STATEMENT, GObject)

gboolean                 ephy_sqlite_statement_bind_null             (EphySQLiteStatement *statement, int column, GError **error);
gboolean                 ephy_sqlite_statement_bind_boolean          (EphySQLiteStatement *statement, int column, gboolean value, GError **error);
gboolean                 ephy_sqlite_statement_bind_int              (EphySQLiteStatement *statement, int column, int value, GError **error);
gboolean                 ephy_sqlite_statement_bind_int64            (EphySQLiteStatement *statement, int column, gint64 value, GError **error);
gboolean                 ephy_sqlite_statement_bind_double           (EphySQLiteStatement *statement, int column, double value, GError **error);
gboolean                 ephy_sqlite_statement_bind_string           (EphySQLiteStatement *statement, int column, const char *value, GError **error);
gboolean                 ephy_sqlite_statement_bind_blob             (EphySQLiteStatement *statement, int column, const void *value, int length, GError **error);

gboolean                 ephy_sqlite_statement_step                  (EphySQLiteStatement *statement, GError **error);
void                     ephy_sqlite_statement_reset                 (EphySQLiteStatement *statement);

int                      ephy_sqlite_statement_get_column_count      (EphySQLiteStatement *statement);
EphySQLiteColumnType     ephy_sqlite_statement_get_column_type       (EphySQLiteStatement *statement, int column);
int                      ephy_sqlite_statement_get_column_size       (EphySQLiteStatement *statement, int column);
int                      ephy_sqlite_statement_get_column_as_boolean (EphySQLiteStatement *statement, int column);
int                      ephy_sqlite_statement_get_column_as_int     (EphySQLiteStatement *statement, int column);
gint64                   ephy_sqlite_statement_get_column_as_int64   (EphySQLiteStatement *statement, int column);
double                   ephy_sqlite_statement_get_column_as_double  (EphySQLiteStatement *statement, int column);
const char*              ephy_sqlite_statement_get_column_as_string  (EphySQLiteStatement *statement, int column);
const void*              ephy_sqlite_statement_get_column_as_blob    (EphySQLiteStatement *statement, int column);

char*                    ephy_sqlite_create_match_pattern (const char *match_string);

G_END_DECLS
