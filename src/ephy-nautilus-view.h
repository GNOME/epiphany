/*
 *  Copyright (C) 2001, 2002 Ricardo Fernández Pascual
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
 *
 *  $Id$
 */

#ifndef EPHY_NAUTILUS_VIEW_H
#define EPHY_NAUTILUS_VIEW_H

#include <libnautilus/nautilus-view.h>
#include "ephy-shell.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NAUTILUS_VIEW		(ephy_nautilus_view_get_type ())
#define EPHY_NAUTILUS_VIEW(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NAUTILUS_VIEW, EphyNautilusView))
#define EPHY_NAUTILUS_VIEW_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NAUTILUS_VIEW, EphyNautilusViewClass))
#define EPHY_IS_NAUTILUS_VIEW(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NAUTILUS_VIEW))
#define EPHY_IS_NAUTILUS_VIEW_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NAUTILUS_VIEW))
#define EPHY_NAUTILUS_VIEW_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NAUTILUS_VIEW, EphyNautilusViewClass))

typedef struct EphyNautilusView EphyNautilusView;
typedef struct EphyNautilusViewClass EphyNautilusViewClass;
typedef struct EphyNautilusViewPrivate EphyNautilusViewPrivate;

struct EphyNautilusView
{
	NautilusView parent;

	/*< private >*/
	EphyNautilusViewPrivate *priv;
};

struct EphyNautilusViewClass
{
	NautilusViewClass parent_class;
};


GType			ephy_nautilus_view_get_type			(void);
BonoboObject *		ephy_nautilus_view_new_component		(EphyShell *gs);

/* old public methods, probably all of them are going to be killed */

void
ephy_nautilus_view_set_title			(EphyNautilusView *view,
						 const gchar *title);
void
ephy_nautilus_view_set_location			(EphyNautilusView *view,
						 const gchar *uri);
void
ephy_nautilus_view_set_statusbar		(EphyNautilusView *view,
						 const gchar *message);
void
ephy_nautilus_view_report_load_underway		(EphyNautilusView *view);

void
ephy_nautilus_view_report_load_complete		(EphyNautilusView *view);

void
ephy_nautilus_view_report_load_progress		(EphyNautilusView *view,
						 double value);
void
ephy_nautilus_view_report_zoom			(EphyNautilusView *view,
						 gint level);

void ephy_nautilus_view_open_in_new_window	(EphyNautilusView *view,
						 const gchar *url);

G_END_DECLS

#endif
