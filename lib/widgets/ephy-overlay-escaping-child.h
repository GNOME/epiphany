/*
 *
 * Copyright © 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __EPHY_OVERLAY_ESCAPING_CHILD_H__
#define __EPHY_OVERLAY_ESCAPING_CHILD_H__

#include "gedit-overlay-child.h"

G_BEGIN_DECLS

#define EPHY_TYPE_OVERLAY_ESCAPING_CHILD		(ephy_overlay_escaping_child_get_type())
#define EPHY_OVERLAY_ESCAPING_CHILD(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_OVERLAY_ESCAPING_CHILD, EphyOverlayEscapingChild))
#define EPHY_OVERLAY_ESCAPING_CHILD_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_OVERLAY_ESCAPING_CHILD, EphyOverlayEscapingChildClass))
#define IS_EPHY_OVERLAY_ESCAPING_CHILD(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_OVERLAY_ESCAPING_CHILD))
#define IS_EPHY_OVERLAY_ESCAPING_CHILD_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_OVERLAY_ESCAPING_CHILD))
#define EPHY_OVERLAY_ESCAPING_CHILD_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_OVERLAY_ESCAPING_CHILD, EphyOverlayEscapingChildClass))
typedef struct _EphyOverlayEscapingChild		EphyOverlayEscapingChild;

typedef struct _EphyOverlayEscapingChildClass   EphyOverlayEscapingChildClass;
typedef struct _EphyOverlayEscapingChildPrivate	EphyOverlayEscapingChildPrivate;

struct _EphyOverlayEscapingChildClass
{
  GeditOverlayChildClass parent_class;
};

struct _EphyOverlayEscapingChild
{
  GeditOverlayChild parent;
  EphyOverlayEscapingChildPrivate *priv;
};

GType                      ephy_overlay_escaping_child_get_type          (void) G_GNUC_CONST;

EphyOverlayEscapingChild  *ephy_overlay_escaping_child_new               (GtkWidget *widget);
EphyOverlayEscapingChild  *ephy_overlay_escaping_child_new_with_distance (GtkWidget *widget,
                                                                          guint escaping_distance);
G_END_DECLS

#endif /* __EPHY_OVERLAY_ESCAPING_CHILD_H__ */
