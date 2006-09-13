/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
 *  Copyright © 2005, 2006 Jean-François Rameau
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

#ifndef EPHY_NET_MONITOR_H
#define EPHY_NET_MONITOR_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_NET_MONITOR		(ephy_net_monitor_get_type ())
#define EPHY_NET_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NET_MONITOR, EphyNetMonitor))
#define EPHY_NET_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NET_MONITOR, EphyNetMonitorClass))
#define EPHY_IS_NET_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NET_MONITOR))
#define EPHY_IS_NET_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NET_MONITOR))
#define EPHY_NET_MONITOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NET_MONITOR, EphyNetMonitorClass))

typedef struct _EphyNetMonitor		EphyNetMonitor;
typedef struct _EphyNetMonitorClass	EphyNetMonitorClass;
typedef struct _EphyNetMonitorPrivate	EphyNetMonitorPrivate;

struct _EphyNetMonitorClass
{
	GObjectClass parent_class;
};

struct _EphyNetMonitor
{
	GObject parent_instance;

	/*< private >*/
	EphyNetMonitorPrivate *priv;
};

GType	 	ephy_net_monitor_get_type		(void);

EphyNetMonitor *ephy_net_monitor_new			(void);

gboolean	ephy_net_monitor_get_net_status	(EphyNetMonitor *monitor);

G_END_DECLS

#endif
