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

#ifndef EPHY_SQLITE_CONNECTION_H
#define EPHY_SQLITE_CONNECTION_H

#include <glib-object.h>
#include "ephy-sqlite-statement.h"

G_BEGIN_DECLS

/* convenience macros */
#define EPHY_TYPE_SQLITE_CONNECTION             (ephy_sqlite_connection_get_type())
#define EPHY_SQLITE_CONNECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),EPHY_TYPE_SQLITE_CONNECTION,EphySQLiteConnection))
#define EPHY_SQLITE_CONNECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),EPHY_TYPE_SQLITE_CONNECTION,EphySQLiteConnectionClass))
#define EPHY_IS_SQLITE_CONNECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),EPHY_TYPE_SQLITE_CONNECTION))
#define EPHY_IS_SQLITE_CONNECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),EPHY_TYPE_SQLITE_CONNECTION))
#define EPHY_SQLITE_CONNECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),EPHY_TYPE_SQLITE_CONNECTION,EphySQLiteConnectionClass))

typedef struct _EphySQLiteConnection                EphySQLiteConnection;
typedef struct _EphySQLiteConnectionClass           EphySQLiteConnectionClass;
typedef struct _EphySQLiteConnectionPrivate         EphySQLiteConnectionPrivate;

struct _EphySQLiteConnection {
     GObject parent;

    /* private */
    EphySQLiteConnectionPrivate *priv;
};

struct _EphySQLiteConnectionClass {
    GObjectClass parent_class;
};

GType                   ephy_sqlite_connection_get_type                (void);

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

#endif /* EPHY_SQLITE_CONNECTION_H */

