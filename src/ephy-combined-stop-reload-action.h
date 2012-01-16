/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011 Igalia S.L.
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

#ifndef _EPHY_COMBINED_STOP_RELOAD_ACTION_H
#define _EPHY_COMBINED_STOP_RELOAD_ACTION_H

#include <gtk/gtk.h>

#include "ephy-window-action.h"

G_BEGIN_DECLS

#define EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION            (ephy_combined_stop_reload_action_get_type())
#define EPHY_COMBINED_STOP_RELOAD_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION, EphyCombinedStopReloadAction))
#define EPHY_COMBINED_STOP_RELOAD_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION, EphyCombinedStopReloadActionClass))
#define EPHY_IS_COMBINED_STOP_RELOAD_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION))
#define EPHY_IS_COMBINED_STOP_RELOAD_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION))
#define EPHY_COMBINED_STOP_RELOAD_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION, EphyCombinedStopReloadActionClass))

typedef struct _EphyCombinedStopReloadAction EphyCombinedStopReloadAction;
typedef struct _EphyCombinedStopReloadActionClass EphyCombinedStopReloadActionClass;
typedef struct _EphyCombinedStopReloadActionPrivate EphyCombinedStopReloadActionPrivate;

struct _EphyCombinedStopReloadAction
{
  EphyWindowAction parent;

  /*< private >*/
  EphyCombinedStopReloadActionPrivate *priv;
};

struct _EphyCombinedStopReloadActionClass
{
  EphyWindowActionClass parent_class;
};

GType ephy_combined_stop_reload_action_get_type (void) G_GNUC_CONST;

void ephy_combined_stop_reload_action_set_loading (EphyCombinedStopReloadAction *action,
                                                   gboolean loading);

G_END_DECLS

#endif /* _EPHY_COMBINED_STOP_RELOAD_ACTION_H */
