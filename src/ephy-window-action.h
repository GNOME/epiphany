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

#ifndef EPHY_WINDOW_ACTION_H
#define EPHY_WINDOW_ACTION_H

#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WINDOW_ACTION            (ephy_window_action_get_type ())
#define EPHY_WINDOW_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_WINDOW_ACTION, EphyWindowAction))
#define EPHY_WINDOW_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_WINDOW_ACTION, EphyWindowActionClass))
#define EPHY_IS_WINDOW_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_WINDOW_ACTION))
#define EPHY_IS_WINDOW_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_WINDOW_ACTION))
#define EPHY_WINDOW_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_WINDOW_ACTION, EphyWindowActionClass))

typedef struct _EphyWindowAction                EphyWindowAction;
typedef struct _EphyWindowActionPrivate EphyWindowActionPrivate;
typedef struct _EphyWindowActionClass   EphyWindowActionClass;

struct _EphyWindowAction {
  GtkAction parent;

  /*< private >*/
  EphyWindowActionPrivate *priv;
};

struct _EphyWindowActionClass {
  GtkActionClass parent_class;
};

GType ephy_window_action_get_type (void);

EphyWindow     *ephy_window_action_get_window       (EphyWindowAction *action);

G_END_DECLS

#endif
