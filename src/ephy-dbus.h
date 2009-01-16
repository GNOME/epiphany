/*
 *  Copyright © 2004 Jean-François rameau
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

#ifndef EPHY_DBUS_H
#define EPHY_DBUS_H

#include <glib-object.h>

/* Yes, we know that DBUS API isn't stable yet */
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DBUS		(ephy_dbus_get_type ())
#define EPHY_DBUS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_DBUS, EphyDbus))
#define EPHY_DBUS_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_DBUS, EphyDbusClass))
#define EPHY_IS_DBUS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_DBUS))
#define EPHY_IS_DBUS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_DBUS))
#define EPHY_DBUS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_DBUS, EphyDbusClass))

extern GQuark ephy_dbus_error_quark;
#define EPHY_DBUS_ERROR_QUARK	(ephy_dbus_error_quark)

typedef struct _EphyDbus	EphyDbus;
typedef struct _EphyDbusPrivate	EphyDbusPrivate;
typedef struct _EphyDbusClass	EphyDbusClass;

typedef enum
{
	EPHY_DBUS_SESSION,
	EPHY_DBUS_SYSTEM
} EphyDbusBus;

struct _EphyDbus
{
	GObject parent;

	/*< private >*/
	EphyDbusPrivate *priv;
};

struct _EphyDbusClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* connected)	(EphyDbus *dbus,
				 EphyDbusBus kind);
	void (* disconnected)	(EphyDbus *dbus,
				 EphyDbusBus kind);
};

GType		ephy_dbus_get_type	(void);

EphyDbus       *ephy_dbus_get_default	(void);

DBusGConnection *ephy_dbus_get_bus	(EphyDbus *dbus,
					 EphyDbusBus kind);

DBusGProxy	*ephy_dbus_get_proxy	(EphyDbus *dbus,
					 EphyDbusBus kind);

/* private */
gboolean       _ephy_dbus_startup	(gboolean claim_name,
					 GError **error);

void	       _ephy_dbus_release	(void);

gboolean       _ephy_dbus_is_name_owner	(void);

G_END_DECLS

#endif /* !EPHY_DBUS_H */
