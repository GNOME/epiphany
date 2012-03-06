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

#ifndef _EPHY_URLS_STORE_H
#define _EPHY_URLS_STORE_H

#include "ephy-history-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_URLS_STORE            (ephy_urls_store_get_type())
#define EPHY_URLS_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_URLS_STORE, EphyURLsStore))
#define EPHY_URLS_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_URLS_STORE, EphyURLsStoreClass))
#define EPHY_IS_URLS_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_URLS_STORE))
#define EPHY_IS_URLS_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_URLS_STORE))
#define EPHY_URLS_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_URLS_STORE, EphyURLsStoreClass))

typedef struct _EphyURLsStore           EphyURLsStore;
typedef struct _EphyURLsStoreClass      EphyURLsStoreClass;
typedef struct _EphyURLsStorePrivate    EphyURLsStorePrivate;

typedef enum {
  EPHY_URLS_STORE_COLUMN_TITLE = 0,
  EPHY_URLS_STORE_COLUMN_ADDRESS,
  EPHY_URLS_STORE_COLUMN_DATE,
  EPHY_URLS_STORE_N_COLUMNS
} EphyURLsStoreColumn;

struct _EphyURLsStore {
  GtkListStore parent;
};

struct _EphyURLsStoreClass {
  GtkListStoreClass parent_class;
};

GType             ephy_urls_store_get_type          (void) G_GNUC_CONST;
EphyURLsStore*    ephy_urls_store_new               (void);
void              ephy_urls_store_add_urls          (EphyURLsStore *store, GList *urls);
void              ephy_urls_store_add_url           (EphyURLsStore *store, EphyHistoryURL *url);
void              ephy_urls_store_add_visits        (EphyURLsStore *store, GList *visits);
EphyHistoryURL*   ephy_urls_store_get_url_from_path (EphyURLsStore *store, GtkTreePath *path);

G_END_DECLS

#endif /* _EPHY_URLS_STORE_H */
