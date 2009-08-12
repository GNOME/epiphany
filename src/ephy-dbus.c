/*
 *  Copyright © 2004, 2005 Jean-François Rameau
 *  Copyright © 2006 Christian Persch
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

#include "config.h"

#include "ephy-dbus.h"
#include "ephy-type-builtins.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"
#include "ephy-activation.h"
#include "ephy-dbus-server-bindings.h"

#include <string.h>
#include <dbus/dbus-glib-bindings.h>

/* dbus 0.6 API change */
#ifndef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
#define DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT 0
#endif

/* dbus < 0.6 compat */
#ifndef DBUS_NAME_FLAG_DO_NOT_QUEUE
#define DBUS_NAME_FLAG_DO_NOT_QUEUE 0
#endif

/* Epiphany's DBUS ids */
#define DBUS_EPHY_SERVICE	"org.gnome.Epiphany"
#define DBUS_EPHY_PATH		"/org/gnome/Epiphany"
#define DBUS_EPHY_INTERFACE	"org.gnome.Epiphany"

#define RECONNECT_DELAY	3 /* seconds */

#define EPHY_DBUS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DBUS, EphyDbusPrivate))

struct _EphyDbusPrivate
{
	DBusGConnection *session_bus;
	DBusGConnection *system_bus;
	guint session_reconnect_timeout_id;
	guint system_reconnect_timeout_id;
	guint is_session_service_owner : 1;
	guint register_name : 1;
};

enum
{
	CONNECTED,
	DISCONNECTED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_CLAIM_NAME
};

static EphyDbus *ephy_dbus_instance;

static guint signals[LAST_SIGNAL];
GQuark ephy_dbus_error_quark;

/* Filter signals form session bus */
static DBusHandlerResult session_filter_func (DBusConnection *connection,
				              DBusMessage *message,
				              void *user_data);
/* Filter signals from system bus */
static DBusHandlerResult system_filter_func (DBusConnection *connection,
				             DBusMessage *message,
				             void *user_data);

/* Both  connect to their respective bus */
static gboolean ephy_dbus_connect_to_session_bus (EphyDbus*, GError**);
static gboolean ephy_dbus_connect_to_system_bus  (EphyDbus*, GError**);

/* implementation of the DBUS helpers */

static gboolean
ephy_dbus_connect_to_session_bus_cb (gpointer user_data)
{
	EphyDbus *dbus = EPHY_DBUS (user_data);

	if (!ephy_dbus_connect_to_session_bus (dbus, NULL))
	{
		/* try again */
		return TRUE;
	}

	dbus->priv->session_reconnect_timeout_id = 0;

	/* we're done */
	return FALSE;
}

static gboolean
ephy_dbus_connect_to_system_bus_cb (gpointer user_data)
{
	EphyDbus *dbus = EPHY_DBUS (user_data);

	if (!ephy_dbus_connect_to_system_bus (dbus, NULL))
	{
		/* try again */
		return TRUE;
	}

	dbus->priv->system_reconnect_timeout_id = 0;

	/* we're done */
	return FALSE;
}

static DBusHandlerResult
session_filter_func (DBusConnection *connection,
	     	     DBusMessage *message,
	     	     void *user_data)
{
	EphyDbus *ephy_dbus = EPHY_DBUS (user_data);
	EphyDbusPrivate *priv = ephy_dbus->priv;

	if (dbus_message_is_signal (message,
				    DBUS_INTERFACE_LOCAL,
				    "Disconnected"))
	{
		LOG ("EphyDbus disconnected from session bus");

		dbus_g_connection_unref (priv->session_bus);
		priv->session_bus = NULL;

		g_signal_emit (ephy_dbus, signals[DISCONNECTED], 0, EPHY_DBUS_SESSION);

		/* try to reconnect later ... */
		priv->session_reconnect_timeout_id =
			g_timeout_add_seconds (RECONNECT_DELAY,
				       (GSourceFunc) ephy_dbus_connect_to_session_bus_cb,
				       ephy_dbus);

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
	EphyDbusPrivate *priv = ephy_dbus->priv;

	LOG ("EphyDbus filtering message from system bus");

	if (dbus_message_is_signal (message,
				    DBUS_INTERFACE_LOCAL,
				    "Disconnected"))
	{
		LOG ("EphyDbus disconnected from system bus");

		dbus_g_connection_unref (priv->system_bus);
		priv->system_bus = NULL;

		g_signal_emit (ephy_dbus, signals[DISCONNECTED], 0, EPHY_DBUS_SYSTEM);

		/* try to reconnect later ... */
		priv->system_reconnect_timeout_id =
			g_timeout_add_seconds (RECONNECT_DELAY,
				       (GSourceFunc) ephy_dbus_connect_to_system_bus_cb,
				       ephy_dbus);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
ephy_dbus_connect_to_system_bus (EphyDbus *ephy_dbus,
				 GError **error)
{
	EphyDbusPrivate *priv = ephy_dbus->priv;

	LOG ("EphyDbus connecting to system DBUS");

	priv->system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, error);
	if (priv->system_bus == NULL)
	{
		g_warning ("Unable to connect to system bus: %s", error ? (*error)->message : "");
		return FALSE;
	}

	if (dbus_g_connection_get_connection (priv->system_bus) == NULL)
	{
		g_warning ("DBus connection is null");
		g_set_error (error,
			     EPHY_DBUS_ERROR_QUARK,
			     0,
			     "DBus connection is NULL");
		return FALSE;
	}

	dbus_connection_set_exit_on_disconnect 
		(dbus_g_connection_get_connection (priv->system_bus),
		 FALSE);

	dbus_connection_add_filter
		(dbus_g_connection_get_connection (priv->system_bus),
		 system_filter_func, ephy_dbus, NULL);

	g_signal_emit (ephy_dbus, signals[CONNECTED], 0, EPHY_DBUS_SYSTEM);

	return TRUE;
}

static gboolean
ephy_dbus_connect_to_session_bus (EphyDbus *ephy_dbus,
				  GError **error)
{
	EphyDbusPrivate *priv = ephy_dbus->priv;
	DBusGProxy *proxy;
	guint request_ret;
	
	LOG ("EphyDbus connecting to session DBUS");

	/* Init the DBus connection */
	priv->session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, error);
	if (priv->session_bus == NULL)
	{
		g_warning("Unable to connect to session bus: %s", error && *error ? (*error)->message : "");
		return FALSE;
	}

	dbus_connection_set_exit_on_disconnect 
		(dbus_g_connection_get_connection (priv->session_bus),
		 FALSE);

	dbus_connection_add_filter
		(dbus_g_connection_get_connection (priv->session_bus),
		 session_filter_func, ephy_dbus, NULL);

	if (priv->register_name == FALSE) return TRUE;

	dbus_g_object_type_install_info (EPHY_TYPE_DBUS,
					 &dbus_glib_ephy_activation_object_info);

	/* Register DBUS path */
	dbus_g_connection_register_g_object (priv->session_bus,
					     DBUS_EPHY_PATH,
					     G_OBJECT (ephy_dbus));

	/* Register the service name, the constant here are defined in dbus-glib-bindings.h */
	proxy = dbus_g_proxy_new_for_name (priv->session_bus,
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (proxy,
						DBUS_EPHY_SERVICE,
						DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT |
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&request_ret, error))
	{
		/* We have a BIG problem! */
		g_warning ("RequestName failed: %s\n", error ? (*error)->message : "");
		return FALSE;
	}

	if (request_ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
	    request_ret == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)
	{
		priv->is_session_service_owner = TRUE;
	}
	else if (request_ret == DBUS_REQUEST_NAME_REPLY_EXISTS ||
		 request_ret == DBUS_REQUEST_NAME_REPLY_IN_QUEUE)
	{
		priv->is_session_service_owner = FALSE;
	}

	LOG ("Instance is %ssession bus owner.", priv->is_session_service_owner ? "" : "NOT ");

	g_object_unref (proxy);

	return TRUE;
}

/* Public methods */

static void
ephy_dbus_shutdown (EphyDbus *dbus)
{
	EphyDbusPrivate *priv = dbus->priv;

	LOG ("EphyDbus shutdown");

	if (priv->session_reconnect_timeout_id != 0)
	{
		g_source_remove (priv->session_reconnect_timeout_id);
		priv->session_reconnect_timeout_id = 0;
	}

	if (priv->system_reconnect_timeout_id != 0)
	{
		g_source_remove (priv->system_reconnect_timeout_id);
		priv->system_reconnect_timeout_id = 0;
	}

	if (priv->session_bus)
	{
		dbus_connection_remove_filter
			(dbus_g_connection_get_connection (priv->session_bus),
			 session_filter_func, dbus);
		dbus_g_connection_unref (priv->session_bus);
		priv->session_bus = NULL;
	}

        if (priv->system_bus)
	{
		dbus_connection_remove_filter
			(dbus_g_connection_get_connection (priv->system_bus),
			 system_filter_func, dbus);
		dbus_g_connection_unref (priv->system_bus);
		priv->system_bus = NULL;
	}
}

/* Class implementation */

G_DEFINE_TYPE (EphyDbus, ephy_dbus, G_TYPE_OBJECT)

static void
ephy_dbus_get_property (GObject *object,
			guint prop_id,
		 	GValue *value,
			GParamSpec *pspec)
{
	/* no readable properties */
	g_return_if_reached ();
}

static void
ephy_dbus_set_property (GObject *object,
			guint prop_id,
			const GValue *value,
			GParamSpec *pspec)
{
	EphyDbus *dbus = EPHY_DBUS (object);
	EphyDbusPrivate *priv = dbus->priv;

	switch (prop_id)
	{
		case PROP_CLAIM_NAME:
			priv->register_name = g_value_get_boolean (value);
			break;
	}
}

static void
ephy_dbus_finalize (GObject *object)
{
	EphyDbus *dbus = EPHY_DBUS (object);

	/* Have to do this after the object's weak ref notifiers have
	 * been called, see https://bugs.freedesktop.org/show_bug.cgi?id=5688
	 */
	ephy_dbus_shutdown (dbus);

	LOG ("EphyDbus finalised");

	G_OBJECT_CLASS (ephy_dbus_parent_class)->finalize (object);
}

static void
ephy_dbus_init (EphyDbus *dbus)
{
	dbus->priv = EPHY_DBUS_GET_PRIVATE (dbus);

	LOG ("EphyDbus initialising");
}

static void
ephy_dbus_class_init (EphyDbusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = ephy_dbus_get_property;
	object_class->set_property = ephy_dbus_set_property;
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

	g_object_class_install_property
		(object_class,
		 PROP_CLAIM_NAME,
		 g_param_spec_boolean ("register-name",
				       "register-name",
				       "register-name",
				       TRUE,
				       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyDbusPrivate));
}

EphyDbus *
ephy_dbus_get_default (void)
{
	g_assert (ephy_dbus_instance != NULL);

	return ephy_dbus_instance;
}

/**
 * ephy_dbus_get_bus:
 * @dbus:
 * @kind:
 * 
 * Returns: the #DBusGConnection for the @kind DBUS, or %NULL
 * if a connection could not be established.
 */
DBusGConnection *
ephy_dbus_get_bus (EphyDbus *dbus,
		   EphyDbusBus kind)
{
	EphyDbusPrivate *priv = dbus->priv;
	DBusGConnection *bus = NULL;

	g_return_val_if_fail (EPHY_IS_DBUS (dbus), NULL);

	if (kind == EPHY_DBUS_SYSTEM)
	{
		/* We connect lazily to the system bus */
		if (priv->system_bus == NULL)
		{
			ephy_dbus_connect_to_system_bus (dbus, NULL);
		}

		bus = priv->system_bus;
	}
	else if (kind == EPHY_DBUS_SESSION)
	{
		if (priv->session_bus == NULL)
		{
			ephy_dbus_connect_to_session_bus (dbus, NULL);
		}

		bus = priv->session_bus;
	}
	else
	{
		g_assert_not_reached ();
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
		g_warning ("Unable to get proxy for the %s bus.\n",
			   kind == EPHY_DBUS_SESSION ? "session" : "system");
		return NULL;
	}

	return dbus_g_proxy_new_for_name (bus,
					  DBUS_EPHY_SERVICE,
					  DBUS_EPHY_PATH,
					  DBUS_EPHY_INTERFACE);
}

/* private API */

gboolean
_ephy_dbus_startup (gboolean connect_and_register_name,
		    GError **error)
{
	g_assert (ephy_dbus_instance == NULL);

	ephy_dbus_error_quark = g_quark_from_static_string ("ephy-dbus-error");
		
	ephy_dbus_instance = g_object_new (EPHY_TYPE_DBUS,
					   "register-name", connect_and_register_name,
					   NULL);

	if (!connect_and_register_name) return TRUE;

	/* We only connect to the session bus on startup*/
	return ephy_dbus_connect_to_session_bus (ephy_dbus_instance, error);
}

void
_ephy_dbus_release (void)
{
	g_assert (ephy_dbus_instance != NULL);

	g_object_unref (ephy_dbus_instance);
	ephy_dbus_instance = NULL;
}

gboolean
_ephy_dbus_is_name_owner (void)
{
	g_assert (ephy_dbus_instance != NULL);

	return ephy_dbus_instance->priv->is_session_service_owner;
}
