/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011, 2012 Igalia S.L.
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
 *
 */

#ifndef _EPHY_HISTORY_STORE_H
#define _EPHY_HISTORY_STORE_H

#include "ephy-history-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_STORE            (ephy_history_store_get_type())
#define EPHY_HISTORY_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_HISTORY_STORE, EphyHistoryStore))
#define EPHY_HISTORY_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_HISTORY_STORE, EphyHistoryStoreClass))
#define EPHY_IS_HISTORY_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_HISTORY_STORE))
#define EPHY_IS_HISTORY_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_HISTORY_STORE))
#define EPHY_HISTORY_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_HISTORY_STORE, EphyHistoryStoreClass))

typedef struct _EphyHistoryStore           EphyHistoryStore;
typedef struct _EphyHistoryStoreClass      EphyHistoryStoreClass;
typedef struct _EphyHistoryStorePrivate    EphyHistoryStorePrivate;

typedef enum {
  EPHY_HISTORY_STORE_COLUMN_TITLE = 0,
  EPHY_HISTORY_STORE_COLUMN_ADDRESS,
  EPHY_HISTORY_STORE_COLUMN_DATE,
  EPHY_HISTORY_STORE_N_COLUMNS
} EphyHistoryStoreColumn;

struct _EphyHistoryStore {
  GtkListStore parent;
};

struct _EphyHistoryStoreClass {
  GtkListStoreClass parent_class;
};

GType             ephy_history_store_get_type          (void) G_GNUC_CONST;
EphyHistoryStore* ephy_history_store_new               (void);
void              ephy_history_store_add_urls          (EphyHistoryStore *store, GList *urls);
void              ephy_history_store_add_url           (EphyHistoryStore *store, EphyHistoryURL *url);
void              ephy_history_store_add_visits        (EphyHistoryStore *store, GList *visits);
EphyHistoryURL*   ephy_history_store_get_url_from_path (EphyHistoryStore *store, GtkTreePath *path);

G_END_DECLS

#endif /* _EPHY_HISTORY_STORE_H */
