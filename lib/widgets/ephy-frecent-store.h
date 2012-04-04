/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
 *  Author: Claudio Saavedra  <csaavedra@igalia.com>
 */

#ifndef _EPHY_FRECENT_STORE_H
#define _EPHY_FRECENT_STORE_H

#include <glib-object.h>

#include "ephy-overview-store.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FRECENT_STORE            (ephy_frecent_store_get_type())
#define EPHY_FRECENT_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_FRECENT_STORE, EphyFrecentStore))
#define EPHY_FRECENT_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_FRECENT_STORE, EphyFrecentStoreClass))
#define EPHY_IS_FRECENT_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_FRECENT_STORE))
#define EPHY_IS_FRECENT_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_FRECENT_STORE))
#define EPHY_FRECENT_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_FRECENT_STORE, EphyFrecentStoreClass))

typedef struct _EphyFrecentStore        EphyFrecentStore;
typedef struct _EphyFrecentStoreClass   EphyFrecentStoreClass;
typedef struct _EphyFrecentStorePrivate EphyFrecentStorePrivate;

struct _EphyFrecentStore
{
  EphyOverviewStore parent;
  EphyFrecentStorePrivate *priv;
};

struct _EphyFrecentStoreClass
{
  EphyOverviewStoreClass parent_class;
};

GType ephy_frecent_store_get_type (void) G_GNUC_CONST;

EphyFrecentStore* ephy_frecent_store_new                (void);

void              ephy_frecent_store_set_hidden         (EphyFrecentStore *store,
                                                         GtkTreeIter      *iter);

void              ephy_frecent_store_set_history_length (EphyFrecentStore *store,
                                                         gint              length);

int               ephy_frecent_store_get_history_length (EphyFrecentStore *store);

G_END_DECLS

#endif /* _EPHY_FRECENT_STORE_H */
