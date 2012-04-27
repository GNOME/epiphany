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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef EPHY_SQLITE_STATEMENT_H
#define EPHY_SQLITE_STATEMENT_H

#include <glib-object.h>
#include "ephy-sqlite.h"

G_BEGIN_DECLS

/* convenience macros */
#define EPHY_TYPE_SQLITE_STATEMENT             (ephy_sqlite_statement_get_type())
#define EPHY_SQLITE_STATEMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),EPHY_TYPE_SQLITE_STATEMENT,EphySQLiteStatement))
#define EPHY_SQLITE_STATEMENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),EPHY_TYPE_SQLITE_STATEMENT,EphySQLiteStatementClass))
#define EPHY_IS_SQLITE_STATEMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),EPHY_TYPE_SQLITE_STATEMENT))
#define EPHY_IS_SQLITE_STATEMENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),EPHY_TYPE_SQLITE_STATEMENT))
#define EPHY_SQLITE_STATEMENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),EPHY_TYPE_SQLITE_STATEMENT,EphySQLiteStatementClass))

typedef struct _EphySQLiteStatement                EphySQLiteStatement;
typedef struct _EphySQLiteStatementClass           EphySQLiteStatementClass;
typedef struct _EphySQLiteStatementPrivate         EphySQLiteStatementPrivate;

struct _EphySQLiteStatement {
     GObject parent;

    /* private */
    EphySQLiteStatementPrivate *priv;
};

struct _EphySQLiteStatementClass {
    GObjectClass parent_class;
};

GType                    ephy_sqlite_statement_get_type              (void);

gboolean                 ephy_sqlite_statement_bind_null             (EphySQLiteStatement *statement, int column, GError **error);
gboolean                 ephy_sqlite_statement_bind_boolean          (EphySQLiteStatement *statement, int column, gboolean value, GError **error);
gboolean                 ephy_sqlite_statement_bind_int              (EphySQLiteStatement *statement, int column, int value, GError **error);
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
double                   ephy_sqlite_statement_get_column_as_double  (EphySQLiteStatement *statement, int column);
const char*              ephy_sqlite_statement_get_column_as_string  (EphySQLiteStatement *statement, int column);
const void*              ephy_sqlite_statement_get_column_as_blob    (EphySQLiteStatement *statement, int column);

char*                    ephy_sqlite_create_match_pattern (const char *match_string);

G_END_DECLS

#endif /* EPHY_SQLITE_STATEMENT_H */

