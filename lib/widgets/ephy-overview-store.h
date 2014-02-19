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
 */

#ifndef _EPHY_OVERVIEW_STORE_H
#define _EPHY_OVERVIEW_STORE_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_OVERVIEW_STORE            (ephy_overview_store_get_type())
#define EPHY_OVERVIEW_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_OVERVIEW_STORE, EphyOverviewStore))
#define EPHY_OVERVIEW_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_OVERVIEW_STORE, EphyOverviewStoreClass))
#define EPHY_IS_OVERVIEW_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_OVERVIEW_STORE))
#define EPHY_IS_OVERVIEW_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_OVERVIEW_STORE))
#define EPHY_OVERVIEW_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_OVERVIEW_STORE, EphyOverviewStoreClass))

typedef struct _EphyOverviewStore        EphyOverviewStore;
typedef struct _EphyOverviewStoreClass   EphyOverviewStoreClass;
typedef struct _EphyOverviewStorePrivate EphyOverviewStorePrivate;

struct _EphyOverviewStore
{
  GtkListStore parent;

  EphyOverviewStorePrivate *priv;
};

struct _EphyOverviewStoreClass
{
  GtkListStoreClass parent_class;
};

enum {
  EPHY_OVERVIEW_STORE_ID,
  EPHY_OVERVIEW_STORE_URI,
  EPHY_OVERVIEW_STORE_TITLE,
  EPHY_OVERVIEW_STORE_AUTHOR,
  EPHY_OVERVIEW_STORE_SNAPSHOT,
  EPHY_OVERVIEW_STORE_LAST_VISIT,
  EPHY_OVERVIEW_STORE_SELECTED,
  EPHY_OVERVIEW_STORE_SNAPSHOT_CANCELLABLE,
  EPHY_OVERVIEW_STORE_SNAPSHOT_MTIME,
  EPHY_OVERVIEW_STORE_SNAPSHOT_PATH,
  EPHY_OVERVIEW_STORE_NCOLS
};

GType    ephy_overview_store_get_type             (void) G_GNUC_CONST;

void     ephy_overview_store_peek_snapshot        (EphyOverviewStore *self,
                                                   WebKitWebView *webview,
                                                   GtkTreeIter *iter);

void     ephy_overview_store_set_default_icon     (EphyOverviewStore *store,
                                                   GdkPixbuf         *default_icon);

gboolean ephy_overview_store_needs_snapshot       (EphyOverviewStore *store,
                                                   GtkTreeIter       *iter);

gboolean ephy_overview_store_remove               (EphyOverviewStore *store,
                                                   GtkTreeIter       *iter);


typedef  void (* EphyOverviewStoreAnimRemoveFunc) (EphyOverviewStore *store,
                                                   GtkTreeIter *iter,
                                                   gboolean valid,
                                                   gpointer user_data);

void     ephy_overview_store_animated_remove      (EphyOverviewStore *store,
                                                   GtkTreeRowReference *ref,
                                                   EphyOverviewStoreAnimRemoveFunc func,
                                                   gpointer user_data);


gboolean ephy_overview_store_find_url             (EphyOverviewStore *store,
                                                   const char        *url,
                                                   GtkTreeIter       *iter);

void     ephy_overview_store_set_snapshot         (EphyOverviewStore *store,
                                                   GtkTreeRowReference *ref,
                                                   cairo_surface_t   *snapshot,
                                                   cairo_surface_t   *favicon);

G_END_DECLS

#endif /* _EPHY_OVERVIEW_STORE_H */
