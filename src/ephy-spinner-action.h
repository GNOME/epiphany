/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EPHY_SPINNER_ACTION_H
#define EPHY_SPINNER_ACTION_H

#include <gtk/gtk.h>
#include <egg-action.h>

#define EPHY_TYPE_SPINNER_ACTION            (ephy_spinner_action_get_type ())
#define EPHY_SPINNER_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_SPINNER_ACTION, EphySpinnerAction))
#define EPHY_SPINNER_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_SPINNER_ACTION, EphySpinnerActionClass))
#define EPHY_IS_SPINNER_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_SPINNER_ACTION))
#define EPHY_IS_SPINNER_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_SPINNER_ACTION))
#define EPHY_SPINNER_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_SPINNER_ACTION, EphySpinnerActionClass))

typedef struct _EphySpinnerAction      EphySpinnerAction;
typedef struct _EphySpinnerActionClass EphySpinnerActionClass;
typedef struct EphySpinnerActionPrivate EphySpinnerActionPrivate;

struct _EphySpinnerAction
{
	EggAction parent;
	EphySpinnerActionPrivate *priv;
};

struct _EphySpinnerActionClass
{
	EggActionClass parent_class;
};

GType    ephy_spinner_action_get_type   (void);

#endif
