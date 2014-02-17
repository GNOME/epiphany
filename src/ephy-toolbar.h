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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_TOOLBAR_H
#define EPHY_TOOLBAR_H

#include <gtk/gtk.h>

#include "ephy-title-box.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TOOLBAR            (ephy_toolbar_get_type())
#define EPHY_TOOLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_TOOLBAR, EphyToolbar))
#define EPHY_TOOLBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  EPHY_TYPE_TOOLBAR, EphyToolbarClass))
#define EPHY_IS_TOOLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_TOOLBAR))
#define EPHY_IS_TOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  EPHY_TYPE_TOOLBAR))
#define EPHY_TOOLBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  EPHY_TYPE_TOOLBAR, EphyToolbarClass))

typedef struct _EphyToolbar EphyToolbar;
typedef struct _EphyToolbarClass EphyToolbarClass;
typedef struct _EphyToolbarPrivate EphyToolbarPrivate;

struct _EphyToolbar {
  GtkHeaderBar parent;

  /*< private >*/
  EphyToolbarPrivate *priv;
};

struct _EphyToolbarClass {
  GtkHeaderBarClass parent_class;
};

GType      ephy_toolbar_get_type (void) G_GNUC_CONST;

GtkWidget *ephy_toolbar_new      (EphyWindow *window);

GtkWidget *ephy_toolbar_get_location_entry (EphyToolbar *toolbar);

EphyTitleBox *ephy_toolbar_get_title_box (EphyToolbar *toolbar);

G_END_DECLS

#endif /* EPHY_TOOLBAR_H */
