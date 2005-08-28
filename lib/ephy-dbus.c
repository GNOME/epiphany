/*
 *  Copyright (C) 2004, 2005 Jean-François Rameau
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

#include "config.h"

#include "ephy-dbus.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

#define EPHY_DBUS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DBUS, EphyDbusPrivate))

struct _EphyDbusPrivate
{
	DBusConnection *session_bus;
	DBusConnection *system_bus;
	guint reconnect_timeout_id;
};

enum
{
	CONNECTED,
	DISCONNECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

/* Epiphany's DBUS identification */
static const char* epiphany_dbus_service     = "org.gnome.Epiphany";
static const char* epiphany_dbus_object_path = "/org/gnome/Epiphany";

/* This function is called by DBUS when a message directed at the
 * Epiphany's object path arrives (provided we're the registered instance!)
 * it routes the message to the correct handler
 */
static DBusHandlerResult path_message_func (DBusConnection *connection,
					    DBusMessage *message,
					    gpointer data);

/* Filter signals form session bus */
static DBusHandlerResult session_filter_func (DBusConnection *connection,
				              DBusMessage *message,
				              void *user_data);
/* Filter signals from system bus */
static DBusHandlerResult system_filter_func (DBusConnection *connection,
				             DBusMessage *message,
				             void *user_data);

/* Both  connect to their respective bus */
static void ephy_dbus_connect_to_session_bus (EphyDbus *dbus);
static void ephy_dbus_connect_to_system_bus  (EphyDbus *dbus);

static DBusObjectPathVTable call_vtable = {
  NULL,
  path_message_func,
  NULL,
};

/* implementation of the DBUS helpers */

static gboolean
ephy_dbus_connect_to_session_bus_cb (gpointer user_data)
{
	EphyDbus *dbus = EPHY_DBUS (user_data);
	gboolean success;

	ephy_dbus_connect_to_session_bus (dbus);

	success = (dbus->priv->session_bus != NULL);
	if (success)
	{
		dbus->priv->reconnect_timeout_id = 0;
	}

	return !success;
}

static gboolean
ephy_dbus_connect_to_system_bus_cb (gpointer user_data)
{
	EphyDbus *dbus = EPHY_DBUS (user_data);

	ephy_dbus_connect_to_system_bus (dbus);

	return dbus->priv->system_bus == NULL;
}

static DBusHandlerResult
session_filter_func (DBusConnection *connection,
	     	     DBusMessage *message,
	     	     void *user_data)
{
	EphyDbus *dbus = EPHY_DBUS (user_data);

	if (dbus_message_is_signal (message,
				    DBUS_INTERFACE_LOCAL,
				    "Disconnected"))
	{
		LOG ("EphyDbus disconnected from session bus");

		dbus_connection_unref (dbus->priv->session_bus);
		dbus->priv->session_bus = NULL;

		g_signal_emit (dbus, signals[DISCONNECTED], 0, EPHY_DBUS_SESSION);

		/* try to reconnect later ... */
		dbus->priv->reconnect_timeout_id =
			g_timeout_add (3000, (GSourceFunc) ephy_dbus_connect_to_session_bus_cb, dbus);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
system_filter_func (DBusConnection *connection,
	     	    DBusMessage *message,
	     	    void *user_data)
{
	EphyDbus *dbus = EPHY_DBUS (user_data);

	LOG ("EphyDbus filtering message from system bus");

	if (dbus_message_is_signal (message,
				    DBUS_INTERFACE_LOCAL,
				    "Disconnected"))
	{
		LOG ("EphyDbus disconnected from system bus");

		dbus_connection_unref (dbus->priv->system_bus);
		dbus->priv->system_bus = NULL;

		g_signal_emit (dbus, signals[DISCONNECTED], 0, EPHY_DBUS_SYSTEM);

		/* try to reconnect later ... */
		g_timeout_add (3000, ephy_dbus_connect_to_system_bus_cb, (gpointer)dbus);

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
ephy_dbus_connect_to_system_bus (EphyDbus *dbus)
{
	DBusConnection *bus;
	DBusError	error;

	LOG ("EphyDbus connecting to system DBUS");

	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (bus == NULL)
	{
		g_warning ("Failed to connect to the system D-BUS: %s", error.message);
		dbus_error_free (&error);
		return;
	}
	dbus_connection_set_exit_on_disconnect (bus, FALSE);
	dbus_connection_setup_with_g_main (bus, NULL);

	dbus_connection_add_filter (bus, system_filter_func, dbus, NULL);

	dbus_bus_add_match (bus, 
                            "type='signal',interface='org.freedesktop.NetworkManager'", 
                            &error);
	if (dbus_error_is_set(&error)) {
		g_warning ("Couldn't register signal handler (system bus): %s: %s", 
                           error.name, error.message);
		return;
	}

	dbus->priv->system_bus = bus;

	g_signal_emit (dbus, signals[CONNECTED], 0, EPHY_DBUS_SYSTEM);
}

static void
ephy_dbus_connect_to_session_bus (EphyDbus *dbus)
{
	DBusError       error;
	DBusConnection *bus;

	LOG ("EphyDbus connecting to session DBUS");

	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SESSION, &error);

	if (!bus) {
		g_warning ("EphyDbus failed to connect to the session D-BUS: %s", error.message);
		dbus_error_free (&error);
		return;
	}
	dbus_connection_set_exit_on_disconnect (bus, FALSE);
	dbus_connection_setup_with_g_main (bus, NULL);

	dbus_connection_add_filter (bus, session_filter_func, dbus, NULL);

	dbus_bus_request_name (bus, epiphany_dbus_service, 0, NULL);

	if (dbus_error_is_set (&error)) {
		g_warning ("EphyDbus failed to acquire epiphany service");
		dbus_error_free (&error);
		return;
	}

	dbus_connection_register_object_path (bus,
					      epiphany_dbus_object_path,
					      &call_vtable, dbus);

	dbus->priv->session_bus = bus;

	g_signal_emit (dbus, signals[CONNECTED], 0, EPHY_DBUS_SESSION);
}

static void
ephy_dbus_disconnect_bus (DBusConnection *bus)
{
	if (bus != NULL) {
		dbus_connection_close (bus);
		dbus_connection_unref (bus);
	}
}

static DBusHandlerResult
path_message_func (DBusConnection *connection,
                   DBusMessage *message,
                   gpointer data)
{
	DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	LOG ("EphyDbus filtering path messagefrom session bus");

	if (dbus_message_is_method_call (message, epiphany_dbus_service, "load"))
	{
		result = DBUS_HANDLER_RESULT_HANDLED;
	}

	return result;
}


/* Public methods */

void
ephy_dbus_startup (EphyDbus *dbus)
{
	g_return_if_fail (EPHY_IS_DBUS (dbus));

	LOG ("EphyDbus startup");

	ephy_dbus_connect_to_session_bus (dbus);
	ephy_dbus_connect_to_system_bus (dbus);
}

void
ephy_dbus_shutdown (EphyDbus *dbus)
{
	g_return_if_fail (EPHY_IS_DBUS (dbus));

	LOG ("EphyDbus shutdown");

	if (dbus->priv->reconnect_timeout_id != 0)
	{
		g_source_remove (dbus->priv->reconnect_timeout_id);
		dbus->priv->reconnect_timeout_id = 0;
	}

	ephy_dbus_disconnect_bus (dbus->priv->session_bus);
	ephy_dbus_disconnect_bus (dbus->priv->system_bus);
}

DBusConnection *
ephy_dbus_get_bus (EphyDbus *dbus,
		   EphyDbusBus kind)
{
	DBusConnection *bus = NULL;

	g_return_val_if_fail (EPHY_IS_DBUS (dbus), NULL);

	switch (kind)
	{
		case EPHY_DBUS_SYSTEM:
			bus = dbus->priv->system_bus;
			break;
		case EPHY_DBUS_SESSION:
			bus = dbus->priv->session_bus;
			break;
		default:
			bus = dbus->priv->session_bus;
	}
	return bus;
}

/* Class implementation */

static void
ephy_dbus_init (EphyDbus *dbus)
{
	dbus->priv = EPHY_DBUS_GET_PRIVATE (dbus);

	LOG ("EphyDbus initialising");
}

static void
ephy_dbus_finalize (GObject *object)
{
	EphyDbus *dbus = EPHY_DBUS (object);

	ephy_dbus_shutdown (dbus);

	LOG ("EphyDbus finalised");

	parent_class->finalize (object);
}

static void
ephy_dbus_class_init (EphyDbusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_dbus_finalize;

	signals[CONNECTED] =
		g_signal_new ("connected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyDbusClass, connected),
			      NULL, NULL,
			      ephy_marshal_VOID__ENUM,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_DBUS_BUS);

	signals[DISCONNECTED] =
		g_signal_new ("disconnected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyDbusClass, disconnected),
			      NULL, NULL,
			      ephy_marshal_VOID__ENUM,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_DBUS_BUS);

	g_type_class_add_private (object_class, sizeof(EphyDbusPrivate));
}

GType
ephy_dbus_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyDbusClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_dbus_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyDbus),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_dbus_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyDbus",
					       &our_info, 0);
	}

	return type;
}
