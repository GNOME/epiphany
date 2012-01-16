/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2010, Igalia S.L.
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

#ifndef EPHY_NAVIGATION_HISTORY_ACTION_H
#define EPHY_NAVIGATION_HISTORY_ACTION_H

#include "ephy-link-action.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NAVIGATION_HISTORY_ACTION            (ephy_navigation_history_action_get_type ())
#define EPHY_NAVIGATION_HISTORY_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_NAVIGATION_HISTORY_ACTION, EphyNavigationHistoryAction))
#define EPHY_NAVIGATION_HISTORY_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_NAVIGATION_HISTORY_ACTION, EphyNavigationHistoryActionClass))
#define EPHY_IS_NAVIGATION_HISTORY_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_NAVIGATION_HISTORY_ACTION))
#define EPHY_IS_NAVIGATION_HISTORY_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_NAVIGATION_HISTORY_ACTION))
#define EPHY_NAVIGATION_HISTORY_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_NAVIGATION_HISTORY_ACTION, EphyNavigationHistoryActionClass))

typedef enum
{
  EPHY_NAVIGATION_HISTORY_DIRECTION_BACK,
  EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD
} EphyNavigationHistoryDirection;

typedef struct _EphyNavigationHistoryAction		EphyNavigationHistoryAction;
typedef struct _EphyNavigationHistoryActionPrivate	EphyNavigationHistoryActionPrivate;
typedef struct _EphyNavigationHistoryActionClass	EphyNavigationHistoryActionClass;

struct _EphyNavigationHistoryAction
{
  EphyLinkAction parent;

  /*< private >*/
  EphyNavigationHistoryActionPrivate *priv;
};

struct _EphyNavigationHistoryActionClass
{
  EphyLinkActionClass parent_class;
};

GType ephy_navigation_history_action_get_type (void);

G_END_DECLS

#endif
