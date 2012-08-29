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

#ifndef _EPHY_OVERVIEW_H
#define _EPHY_OVERVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_OVERVIEW ephy_overview_get_type()

#define EPHY_OVERVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_OVERVIEW, EphyOverview))
#define EPHY_OVERVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_OVERVIEW, EphyOverviewClass))
#define EPHY_IS_OVERVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_OVERVIEW))
#define EPHY_IS_OVERVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_OVERVIEW))
#define EPHY_OVERVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_OVERVIEW, EphyOverviewClass))

typedef struct _EphyOverview            EphyOverview;
typedef struct _EphyOverviewClass       EphyOverviewClass;
typedef struct _EphyOverviewPrivate     EphyOverviewPrivate;

struct _EphyOverview {
  GtkGrid parent;

  /*< private >*/
  EphyOverviewPrivate *priv;
};

struct _EphyOverviewClass {
  GtkGridClass parent_class;

};

#define EPHY_OVERVIEW_TITLE _("Most Visited")

GType       ephy_overview_get_type      (void) G_GNUC_CONST;
GtkWidget * ephy_overview_new           (void);

G_END_DECLS

#endif /* _EPHY_OVERVIEW_H */
