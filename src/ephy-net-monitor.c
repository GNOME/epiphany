/*
 *  Copyright (C) 2005, 2006 Jean-François Rameau
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

#include "ephy-net-monitor.h"

#include "ephy-dbus.h"
#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-embed-single.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-debug.h"

#include <NetworkManager/NetworkManager.h>

#include <gmodule.h>

typedef enum 
{
	NETWORK_UP,
	NETWORK_DOWN
} NetworkStatus;

#define EPHY_NET_MONITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NET_MONITOR, EphyNetMonitorPrivate))

struct _EphyNetMonitorPrivate
{
	DBusConnection *bus;
	guint notify_id;
	guint active : 1;
	NetworkStatus status;
};

enum
{
	PROP_0,
	PROP_NETWORK_STATUS
};

static GObjectClass *parent_class;

static void
ephy_net_monitor_set_net_status (EphyNetMonitor *monitor,
				 NetworkStatus status)
{
	EphyNetMonitorPrivate *priv = monitor->priv;

	LOG ("EphyNetMonitor turning Epiphany to %s mode",
	     status != NETWORK_DOWN ? "online" : "offline");

	priv->status = status;

	g_object_notify (G_OBJECT (monitor), "network-status");
}

static NetworkStatus
ephy_net_monitor_check_for_active_device (EphyNetMonitor *monitor,
					  char *all_path[],
					  int num)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	int i;
	DBusMessage *	message = NULL;
	DBusMessage *	reply = NULL;
	const char *	iface = NULL;
	char *		op = NULL;
	dbus_uint32_t	type = 0;
	const char *	udi = NULL;
	dbus_bool_t	active = FALSE;
	const char *	ip4_address = NULL;
	const char *	broadcast = NULL;
	const char *	subnetmask = NULL;
	const char *	hw_addr = NULL;
	const char *	route = NULL;
	const char *	primary_dns = NULL;
	const char *	secondary_dns = NULL;
	dbus_uint32_t	mode = 0;
	dbus_int32_t	strength = -1;
	char *		active_network_path = NULL;
	dbus_bool_t	link_active = FALSE;
	dbus_uint32_t	caps = NM_DEVICE_CAP_NONE;
	char **		networks = NULL;
	int		num_networks = 0;
	NMActStage	act_stage = NM_ACT_STAGE_UNKNOWN;
	NetworkStatus status = NETWORK_DOWN;

	for (i = 0; i < num; i++)
	{
		const char *path = all_path [i];
		DBusError error;

		message = dbus_message_new_method_call (NM_DBUS_SERVICE, 
							path, 
							NM_DBUS_INTERFACE_DEVICES, 
							"getProperties");
		if (message == NULL)
		{
			g_warning ("EphyNetMonitor: couldn't allocate the dbus message");
			status = NETWORK_UP;
			break;
		}

		reply = dbus_connection_send_with_reply_and_block (priv->bus, message, -1, NULL);
		dbus_message_unref (message);

		if (reply == NULL)
		{
			g_warning ("EphyNetMonitor: "
				   "didn't get a reply from NetworkManager for device %s.\n", path);
			status = NETWORK_UP;
			break;
		}

		dbus_error_init (&error);
		if (dbus_message_get_args (reply, &error, DBUS_TYPE_OBJECT_PATH, &op,
					DBUS_TYPE_STRING, &iface,
					DBUS_TYPE_UINT32, &type,
					DBUS_TYPE_STRING, &udi,
					DBUS_TYPE_BOOLEAN,&active,
					DBUS_TYPE_UINT32, &act_stage,
					DBUS_TYPE_STRING, &ip4_address,
					DBUS_TYPE_STRING, &subnetmask,
					DBUS_TYPE_STRING, &broadcast,
					DBUS_TYPE_STRING, &hw_addr,
					DBUS_TYPE_STRING, &route,
					DBUS_TYPE_STRING, &primary_dns,
					DBUS_TYPE_STRING, &secondary_dns,
					DBUS_TYPE_UINT32, &mode,
					DBUS_TYPE_INT32,  &strength,
					DBUS_TYPE_BOOLEAN,&link_active,
					DBUS_TYPE_UINT32, &caps,
					DBUS_TYPE_STRING, &active_network_path,
					DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &networks, &num_networks,
					DBUS_TYPE_INVALID))
		{
			dbus_message_unref (reply);

			/* found one active device */
			if (active)
			{
				status = NETWORK_UP;
				break;
			}
		}
		else
		{
			g_warning ("EphyNetMonitor: "
				   "unexpected reply from NetworkManager for device %s.\n", path);
			if (dbus_error_is_set(&error))
			{
				g_warning ("EphyNetMonitor: %s: %s", error.name, error.message);
				dbus_error_free(&error);
			}

			status = NETWORK_UP;

			break;
		}
	}

	return active;
}

/* This is the heart of Net Monitor monitor */
/* ATM, if there is an active device, we say that network is up: that's all ! */
static gboolean
ephy_net_monitor_network_status (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	DBusMessage *message, *reply;
	DBusError error;
	NetworkStatus net_status;

	/* ask to Network Manager if there is at least one active device */
	message = dbus_message_new_method_call (NM_DBUS_SERVICE, 
						NM_DBUS_PATH, 
						NM_DBUS_INTERFACE, 
						"getDevices");

	if (message == NULL)
	{
		g_warning ("Couldn't allocate the dbus message");
		/* fallbak: let's Epiphany roll */
		return NETWORK_UP;
	}
	dbus_error_init (&error);
	reply = dbus_connection_send_with_reply_and_block (priv->bus, 
						 	   message, 
							   -1, 
							   &error);
	if (dbus_error_is_set (&error))
	{
		if (dbus_error_has_name (&error, NM_DBUS_NO_DEVICES_ERROR))
		{
			LOG ("EphyNetMonitor: Network Manager says - no available network devices -");

			net_status = NETWORK_DOWN;
		}
		else
		{
			LOG ("EphyNetMonitor can't talk to Network Manager: %s: %s", 
			     error.name, error.message);

			/* fallback */
			net_status = NETWORK_UP;
		}

		dbus_error_free(&error);
	}
	else
	{
		if (reply == NULL)
		{
			g_warning("EphyNetMonitor got NULL reply");

			/* fallback */			
			net_status = NETWORK_UP;
		}
		else
		{
			/* check if we have at least one active device */
			char **paths = NULL;
			int num = -1;

			if (!dbus_message_get_args (reply, 
						    NULL, 
						    DBUS_TYPE_ARRAY, 
						    DBUS_TYPE_OBJECT_PATH, 
						    &paths, 
						    &num, 
						    DBUS_TYPE_INVALID))
			{
				g_warning ("EphyNetMonitor: unexpected reply from NetworkManager");
				dbus_message_unref (reply);
				net_status = NETWORK_UP;
			}
			else
			{
				net_status = ephy_net_monitor_check_for_active_device
						(monitor, paths, num);
			}
		}
	}

	if (reply)
	{
		dbus_message_unref (reply);
	}
	
	if (message)
	{
		dbus_message_unref (message);
	}

	return net_status;
}

static void
ephy_net_monitor_check_network (EphyNetMonitor *monitor)
{
	NetworkStatus net_status;
	
	net_status = ephy_net_monitor_network_status (monitor);

	LOG ("EphyNetMonitor guesses the network is %s", 
	     net_status != NETWORK_DOWN ? "up" : "down");

	ephy_net_monitor_set_net_status (monitor, net_status);
}

/* Filters all the messages from Network Manager */
static DBusHandlerResult
filter_func (DBusConnection *connection,
	     DBusMessage *message,
	     void *user_data)
{
	EphyNetMonitor *monitor;

	g_return_val_if_fail (user_data != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	monitor = EPHY_NET_MONITOR (user_data);

	if (dbus_message_is_signal (message,
				    NM_DBUS_INTERFACE,
				    "DeviceNoLongerActive"))
	{
		LOG ("EphyNetMonitor catches DeviceNoLongerActive signal");

		ephy_net_monitor_check_network (monitor);

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (dbus_message_is_signal (message,
				    NM_DBUS_INTERFACE,
				    "DeviceNowActive"))
	{
		LOG ("EphyNetMonitor catches DeviceNowActive signal");

		ephy_net_monitor_set_net_status (monitor, NETWORK_UP);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
ephy_net_monitor_attach_to_dbus (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	DBusError error;
	EphyDbus *dbus;
	DBusGConnection *g_connection;
	
	LOG ("EphyNetMonitor is trying to attach to SYSTEM bus");

	dbus = ephy_dbus_get_default ();

	g_connection = ephy_dbus_get_bus (dbus, EPHY_DBUS_SYSTEM);
	g_return_if_fail (g_connection != NULL);
	
	priv->bus = dbus_g_connection_get_connection (g_connection);

	if (priv->bus != NULL)
	{
		dbus_connection_add_filter (priv->bus, 
					    filter_func, 
					    monitor,
					    NULL);
		dbus_error_init (&error);
		dbus_bus_add_match (priv->bus, 
				    "type='signal',interface='" NM_DBUS_INTERFACE "'", 
				    &error);
		if (dbus_error_is_set(&error)) 
		{
			g_warning("EphyNetMonitor cannot register signal handler: %s: %s", 
				  error.name, error.message);
			dbus_error_free(&error);
		}
		LOG ("EphyNetMonitor attached to SYSTEM bus");
	}
}

static void
connect_to_system_bus_cb (EphyDbus *dbus,
			  EphyDbusBus kind,
			  EphyNetMonitor *monitor)
{
	if (kind == EPHY_DBUS_SYSTEM)
	{
		LOG ("EphyNetMonitor connecting to SYSTEM bus");

		ephy_net_monitor_attach_to_dbus (monitor);
	}
}

static void
disconnect_from_system_bus_cb (EphyDbus *dbus,
			       EphyDbusBus kind,
			       EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;

	if (kind == EPHY_DBUS_SYSTEM)
	{
		LOG ("EphyNetMonitor disconnected from SYSTEM bus");

		/* no bus anymore */
		priv->bus = NULL;
	}
}

static void
ephy_net_monitor_startup (EphyNetMonitor *monitor)
{
	EphyDbus *dbus;

	dbus = ephy_dbus_get_default ();

	LOG ("EphyNetMonitor starting up");

	ephy_net_monitor_attach_to_dbus (monitor);

	/* DBUS may disconnect us at any time. So listen carefully to it */
	g_signal_connect (dbus, "connected",  
			  G_CALLBACK (connect_to_system_bus_cb), monitor);
	g_signal_connect (dbus, "disconnected",  
			  G_CALLBACK (disconnect_from_system_bus_cb), monitor);

	/* FIXME what if the system bus isn't available right now? */
	ephy_net_monitor_check_network (monitor);
}

static void
ephy_net_monitor_shutdown (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	EphyDbus *dbus;

	dbus = ephy_dbus_get_default ();
			
	g_signal_handlers_disconnect_by_func
		(dbus, G_CALLBACK (connect_to_system_bus_cb), monitor);
	g_signal_handlers_disconnect_by_func
		(dbus,G_CALLBACK (disconnect_from_system_bus_cb), monitor);
		
	priv->bus = NULL;

	LOG ("EphyNetMonitor shutdown");
}

static void
notify_network_managed_cb (GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv = monitor->priv;
	GConfValue *value;
	gboolean active = TRUE;

	LOG (CONF_NETWORK_MANAGED " key changed");

	g_assert (entry != NULL);

	value = gconf_entry_get_value (entry);
	if (value != NULL && value->type == GCONF_VALUE_BOOL)
	{
		active = gconf_value_get_bool (value);
	}

	priv->active = active;

	g_object_notify (G_OBJECT (monitor), "network-status");
}

static void
ephy_net_monitor_init (EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv;

	priv = monitor->priv = EPHY_NET_MONITOR_GET_PRIVATE (monitor);

	LOG ("EphyNetMonitor initialising");

	priv->status = NETWORK_UP;

	priv->notify_id = eel_gconf_notification_add
		(CONF_NETWORK_MANAGED,
		 (GConfClientNotifyFunc) notify_network_managed_cb,
		 monitor);
	eel_gconf_notify (CONF_NETWORK_MANAGED);

	ephy_net_monitor_startup (monitor);
}

static void
ephy_net_monitor_dispose (GObject *object)
{
	EphyNetMonitor *monitor = EPHY_NET_MONITOR (object);
	EphyNetMonitorPrivate *priv = monitor->priv;

	LOG ("EphyNetMonitor finalising");

	ephy_net_monitor_shutdown (monitor);

	if (priv->notify_id != 0)
	{
		eel_gconf_notification_remove (priv->notify_id);
		priv->notify_id = 0;
	}

	parent_class->dispose (object);
}

static void
ephy_net_monitor_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	EphyNetMonitor *monitor = EPHY_NET_MONITOR (object);

	switch (prop_id)
	{
		case PROP_NETWORK_STATUS:
			g_value_set_boolean (value, ephy_net_monitor_get_net_status (monitor));
			break;
	}
}

static void
ephy_net_monitor_class_init (EphyNetMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ephy_net_monitor_dispose;
	object_class->get_property = ephy_net_monitor_get_property;

/**
 * EphyNetMonitor::network-status:
 * 
 * Whether the network is on-line.
 */
	g_object_class_install_property
		(object_class,
		 PROP_NETWORK_STATUS,
		 g_param_spec_boolean ("network-status",
				       "network-status",
				       "network-status",
				       FALSE,
				       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNetMonitorPrivate));
}

/* public API */

GType
ephy_net_monitor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyShellClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_net_monitor_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyNetMonitor),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_net_monitor_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyNetMonitor",
					       &our_info, 0);
	}

	return type;
}

EphyNetMonitor *
ephy_net_monitor_new (void)
{
	return g_object_new (EPHY_TYPE_NET_MONITOR, NULL);
}

gboolean
ephy_net_monitor_get_net_status	(EphyNetMonitor *monitor)
{
	EphyNetMonitorPrivate *priv;

	g_return_val_if_fail (EPHY_IS_NET_MONITOR (monitor), FALSE);

	priv = monitor->priv;

	return !priv->active || priv->status != NETWORK_DOWN;
}
