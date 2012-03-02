/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011-2012 Igalia S.L.
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

#ifndef _EPHY_HOSTS_STORE_H
#define _EPHY_HOSTS_STORE_H

#include "ephy-history-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_HOSTS_STORE            (ephy_hosts_store_get_type())
#define EPHY_HOSTS_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_HOSTS_STORE, EphyHostsStore))
#define EPHY_HOSTS_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_HOSTS_STORE, EphyHostsStoreClass))
#define EPHY_IS_HOSTS_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_HOSTS_STORE))
#define EPHY_IS_HOSTS_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_HOSTS_STORE))
#define EPHY_HOSTS_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_HOSTS_STORE, EphyHostsStoreClass))

typedef struct _EphyHostsStore EphyHostsStore;
typedef struct _EphyHostsStoreClass EphyHostsStoreClass;
typedef struct _EphyHostsStorePrivate EphyHostsStorePrivate;

typedef enum {
  EPHY_HOSTS_STORE_COLUMN_ID = 0,
  EPHY_HOSTS_STORE_COLUMN_TITLE,
  EPHY_HOSTS_STORE_COLUMN_ADDRESS,
  EPHY_HOSTS_STORE_COLUMN_VISIT_COUNT,
  EPHY_HOSTS_STORE_N_COLUMNS
} EphyHostsStoreColumn;

struct _EphyHostsStore
{
  GtkListStore parent;
};

struct _EphyHostsStoreClass
{
  GtkListStoreClass parent_class;
};

GType              ephy_hosts_store_get_type           (void) G_GNUC_CONST;
EphyHostsStore*    ephy_hosts_store_new                (void);
void               ephy_hosts_store_add_hosts          (EphyHostsStore *store, GList *hosts);
void               ephy_hosts_store_add_host           (EphyHostsStore *store, EphyHistoryHost *host);
void               ephy_hosts_store_add_visits         (EphyHostsStore *store, GList *visits);
EphyHistoryHost*   ephy_hosts_store_get_host_from_path (EphyHostsStore *store, GtkTreePath *path);

G_END_DECLS

#endif /* _EPHY_HOSTS_STORE_H */
