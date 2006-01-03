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
 */

#include "config.h"

#include "ephy-dbus.h"
#include "ephy-type-builtins.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"
#include "ephy-activation.h"
#include "ephy-dbus-server-bindings.h"

#include <string.h>
#include <dbus/dbus-glib-bindings.h>

#define EPHY_DBUS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DBUS, EphyDbusPrivate))

struct _EphyDbusPrivate
{
	DBusGConnection *session_bus;
	DBusGConnection *system_bus;
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

/* Filter signals form session bus */
static DBusHandlerResult session_filter_func (DBusConnection *connection,
				              DBusMessage *message,
				              void *user_data);
/* Filter signals from system bus */
static DBusHandlerResult system_filter_func (DBusConnection *connection,
				             DBusMessage *message,
				             void *user_data);

/* Handler for NetworkManager's "DevicesChanged" signals */
static void ephy_dbus_nm_devices_changed_cb (DBusGProxy *proxy,
					     const char *string,
					     EphyDbus *ephy_dbus);

/* Both  connect to their respective bus */
static void ephy_dbus_connect_to_session_bus (EphyDbus *dbus);
static void ephy_dbus_connect_to_system_bus  (EphyDbus *dbus);

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
	EphyDbus *ephy_dbus = EPHY_DBUS (user_data);

	if (dbus_message_is_signal (message,
				    DBUS_INTERFACE_LOCAL,
				    "Disconnected"))
	{
		LOG ("EphyDbus disconnected from session bus");

		dbus_g_connection_unref (ephy_dbus->priv->session_bus);
		ephy_dbus->priv->session_bus = NULL;

		g_signal_emit (ephy_dbus, signals[DISCONNECTED], 0, EPHY_DBUS_SESSION);

		/* try to reconnect later ... */
		ephy_dbus->priv->reconnect_timeout_id =
			g_timeout_add (3000, (GSourceFunc) ephy_dbus_connect_to_session_bus_cb, ephy_dbus);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
system_filter_func (DBusConnection *connection,
	     	    DBusMessage *message,
	     	    void *user_data)
{
	EphyDbus *ephy_dbus = EPHY_DBUS (user_data);

	LOG ("EphyDbus filtering message from system bus");

	if (dbus_message_is_signal (message,
				    DBUS_INTERFACE_LOCAL,
				    "Disconnected"))
	{
		LOG ("EphyDbus disconnected from system bus");

		dbus_g_connection_unref (ephy_dbus->priv->system_bus);
		ephy_dbus->priv->system_bus = NULL;

		g_signal_emit (ephy_dbus, signals[DISCONNECTED], 0, EPHY_DBUS_SYSTEM);

		/* try to reconnect later ... */
		g_timeout_add (3000, ephy_dbus_connect_to_system_bus_cb, (gpointer) ephy_dbus);

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
ephy_dbus_connect_to_system_bus (EphyDbus *ephy_dbus)
{
	DBusGProxy *proxy;
	GError *error = NULL;

	LOG ("EphyDbus connecting to system DBUS");

	ephy_dbus->priv->system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (ephy_dbus->priv->system_bus == NULL)
	{
		g_warning ("Unable to connect to system bus: %s", error->message);
		g_error_free (error);
		return;
	}

	if (dbus_g_connection_get_connection (ephy_dbus->priv->system_bus) == NULL)
	{
		g_warning ("DBus connection is null");
		return;
	}

	dbus_connection_set_exit_on_disconnect 
		(dbus_g_connection_get_connection (ephy_dbus->priv->system_bus),
		 FALSE);

	dbus_connection_add_filter
		(dbus_g_connection_get_connection (ephy_dbus->priv->system_bus),
		 system_filter_func, ephy_dbus, NULL);

	proxy = dbus_g_proxy_new_for_name (ephy_dbus->priv->system_bus,
					   DBUS_NETWORK_MANAGER_SERVICE,
					   DBUS_NETWORK_MANAGER_PATH,
					   DBUS_NETWORK_MANAGER_INTERFACE);

	if (proxy == NULL)
	{
		g_warning ("Unable to get DBus proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	dbus_g_proxy_add_signal (proxy, "DevicesChanged", G_TYPE_STRING,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "DevicesChanged",
				     G_CALLBACK (ephy_dbus_nm_devices_changed_cb),
				     ephy_dbus, NULL);

	g_object_unref (proxy);

	g_signal_emit (ephy_dbus, signals[CONNECTED], 0, EPHY_DBUS_SYSTEM);
}

static void
ephy_dbus_connect_to_session_bus (EphyDbus *ephy_dbus)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	int request_ret;
	
	LOG ("EphyDbus connecting to session DBUS");

	/* Init the DBus connection */
	ephy_dbus->priv->session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (ephy_dbus->priv->session_bus == NULL)
	{
		g_warning("Unable to connect to session bus: %s", error->message);
		g_error_free (error);
		return;
	}

	dbus_connection_set_exit_on_disconnect 
		(dbus_g_connection_get_connection (ephy_dbus->priv->session_bus),
		 FALSE);

	dbus_connection_add_filter
		(dbus_g_connection_get_connection (ephy_dbus->priv->session_bus),
		 session_filter_func, ephy_dbus, NULL);
	
	dbus_g_object_type_install_info (EPHY_TYPE_DBUS,
					 &dbus_glib_ephy_activation_object_info);

	/* Register DBUS path */
	dbus_g_connection_register_g_object (ephy_dbus->priv->session_bus,
					     DBUS_EPHY_PATH,
					     G_OBJECT (ephy_dbus));

	/* Register the service name, the constant here are defined in dbus-glib-bindings.h */
	proxy = dbus_g_proxy_new_for_name (ephy_dbus->priv->session_bus,
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

/* dbus 0.6 dependency */
#ifndef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
#define DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT 0
#endif

	org_freedesktop_DBus_request_name (proxy,
					   DBUS_EPHY_SERVICE,
					   DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT,
					   &request_ret, &error);

	if (request_ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	{
		ephy_dbus->is_session_service_owner = TRUE;
	}
	else 
	{
		/* if the bus replied that an owner already exists, we set the
		 * owner flag and proceed -- it means there's another epiphany
		 * instance running and we should simply forward the requests to
		 * it; however if it's another return code, we should (at least)
		 * print a warning */
		ephy_dbus->is_session_service_owner = FALSE;

		if ((request_ret != DBUS_REQUEST_NAME_REPLY_EXISTS) &&
		    (error != NULL))
		{
			g_warning("Unable to register service: %s", error->message);
		}

	}
	if (error != NULL)
	{
		g_error_free (error);
	}
	LOG ("Instance is %ssession bus owner.", ephy_dbus->is_session_service_owner ? "" : "NOT ");

	g_object_unref (proxy);
}

static void
ephy_dbus_disconnect_bus (DBusGConnection *bus)
{
	if (bus != NULL) {
		dbus_connection_close
			(dbus_g_connection_get_connection (bus));
		dbus_g_connection_unref (bus);
	}
}

static void
ephy_dbus_nm_devices_changed_cb (DBusGProxy *proxy,
				 const char *device,
				 EphyDbus *ephy_dbus)
{
	GError *error = NULL;
	char *status;

	/* query status from network manager */
	dbus_g_proxy_call (proxy, "status", &error, G_TYPE_INVALID,
			   G_TYPE_STRING, &status,
			   G_TYPE_INVALID);
	if (status != NULL)
	{
		g_warning ("NetworkManager's DBus \"status()\" call returned \"null\": %s",
			   error->message);
		g_error_free (error);
		return;
	}

	if (strcmp ("connected", status) == 0)
	{
		/* change ephy's status to online */
	}
	else if (strcmp ("disconnected", status) == 0)
	{
		/* change ephy's status to offline */
	}
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

DBusGConnection *
ephy_dbus_get_bus (EphyDbus *dbus,
		   EphyDbusBus kind)
{
	DBusGConnection *bus = NULL;

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

DBusGProxy *
ephy_dbus_get_proxy (EphyDbus *dbus,
		     EphyDbusBus kind)
{
	DBusGConnection *bus = NULL;

	g_return_val_if_fail (EPHY_IS_DBUS (dbus), NULL);
	
	bus = ephy_dbus_get_bus (dbus, kind);

	if (bus == NULL)
	{
		g_warning ("Unable to get proxy for DBus's s bus.");
		return NULL;
	}

	return dbus_g_proxy_new_for_name (bus,
					  DBUS_EPHY_SERVICE,
					  DBUS_EPHY_PATH,
					  DBUS_EPHY_INTERFACE);
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
