/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#include "ephy-location-action.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#define EPHY_LOCATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_ACTION, EphyLocationActionPrivate))

struct _EphyLocationActionPrivate
{
	char *address;
};

static void ephy_location_action_init       (EphyLocationAction *action);
static void ephy_location_action_class_init (EphyLocationActionClass *class);
static void ephy_location_action_finalize   (GObject *object);
static void user_changed_cb		    (GtkWidget *proxy,
					     EphyLocationAction *action);
static void sync_address		    (GtkAction *action,
					     GParamSpec *pspec,
					     GtkWidget *proxy);

enum
{
	PROP_0,
	PROP_ADDRESS
};

enum
{
	GO_LOCATION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };

GType
ephy_location_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyLocationActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_location_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyLocationAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_location_action_init,
		};

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyLocationAction",
					       &type_info, 0);
	}
	return type;
}

static void
location_url_activate_cb (EphyLocationEntry *entry,
			  const char *content,
			  const char *target,
			  EphyLocationAction *action)
{
	EphyBookmarks *bookmarks;

	LOG ("Location url activated, content %s target %s", content, target)

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	if (!content)
	{
		LOG ("Go to %s", target);
		g_signal_emit (action, signals[GO_LOCATION], 0, target);
	}
	else
	{
		char *url;

		url = ephy_bookmarks_solve_smart_url
			(bookmarks, target, content);
		g_return_if_fail (url != NULL);
		LOG ("Go to %s", url);
		g_signal_emit (action, signals[GO_LOCATION], 0, url);
		g_free (url);
	}
}

static void
user_changed_cb (GtkWidget *proxy, EphyLocationAction *action)
{
	const char *address;

	address = ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (proxy));

	LOG ("user_changed_cb, new address %s", address)

	g_signal_handlers_block_by_func (action, G_CALLBACK (sync_address), proxy);
	ephy_location_action_set_address (action, address);
	g_signal_handlers_unblock_by_func (action, G_CALLBACK (sync_address), proxy);
}

static void
sync_address (GtkAction *act, GParamSpec *pspec, GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (act);

	LOG ("sync_address")

	g_return_if_fail (EPHY_IS_LOCATION_ENTRY (proxy));
	g_signal_handlers_block_by_func (proxy, G_CALLBACK (user_changed_cb), action);
	ephy_location_entry_set_location (EPHY_LOCATION_ENTRY (proxy),
					  action->priv->address);
	g_signal_handlers_unblock_by_func (proxy, G_CALLBACK (user_changed_cb), action);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	LOG ("Connect proxy")

	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		EphyAutocompletion *ac;

		ac = EPHY_AUTOCOMPLETION (ephy_shell_get_autocompletion (ephy_shell));

		ephy_location_entry_set_autocompletion (EPHY_LOCATION_ENTRY (proxy), ac);

		sync_address (action, NULL, proxy);
		g_signal_connect_object (action, "notify::address",
					 G_CALLBACK (sync_address), proxy, 0);

		g_signal_connect_object (proxy, "activated",
					 G_CALLBACK (location_url_activate_cb),
					 action, 0);
		g_signal_connect_object (proxy, "user_changed",
					 G_CALLBACK (user_changed_cb), action, 0);
	}

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
	LOG ("Disconnect proxy")

	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		g_signal_handlers_disconnect_by_func
			(action, G_CALLBACK (sync_address), proxy);

		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (location_url_activate_cb), action);

		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (user_changed_cb), action);
	}

	(* GTK_ACTION_CLASS (parent_class)->disconnect_proxy) (action, proxy);
}

static void
ephy_location_action_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ADDRESS:
			ephy_location_action_set_address (action, g_value_get_string (value));
			break;
	}
}

static void
ephy_location_action_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ADDRESS:
			g_value_set_string (value, ephy_location_action_get_address (action));
			break;
	}
}

static void
ephy_location_action_activate (GtkAction *action)
{
	GSList *proxies;

	/* Note: this makes sense only for a single proxy */
	proxies = gtk_action_get_proxies (action);

	if (proxies)
	{
		ephy_location_entry_activate (EPHY_LOCATION_ENTRY (proxies->data));
	}
}

static void
ephy_location_action_class_init (EphyLocationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = ephy_location_action_finalize;
	object_class->get_property = ephy_location_action_get_property;
	object_class->set_property = ephy_location_action_set_property;

	action_class->toolbar_item_type = EPHY_TYPE_LOCATION_ENTRY;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;
	action_class->activate = ephy_location_action_activate;

	signals[GO_LOCATION] =
                g_signal_new ("go_location",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyLocationActionClass, go_location),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_ADDRESS,
					 g_param_spec_string ("address",
							      "Address",
							      "The address",
							      "",
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EphyLocationActionPrivate));
}

static void
ephy_location_action_init (EphyLocationAction *action)
{
	action->priv = EPHY_LOCATION_ACTION_GET_PRIVATE (action);

	action->priv->address = g_strdup ("");
}

static void
ephy_location_action_finalize (GObject *object)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);

	g_free (action->priv->address);
}

const char *
ephy_location_action_get_address (EphyLocationAction *action)
{
	g_return_val_if_fail (EPHY_IS_LOCATION_ACTION (action), "");

	return action->priv->address;
}

void
ephy_location_action_set_address (EphyLocationAction *action,
				  const char *address)
{
	g_return_if_fail (EPHY_IS_LOCATION_ACTION (action));

	LOG ("set_address %s", address)

	g_free (action->priv->address);
	action->priv->address = g_strdup (address ? address : "");
	g_object_notify (G_OBJECT (action), "address");
}

static void
clear_history (GtkWidget *proxy, gpointer user_data)
{
	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		ephy_location_entry_clear_history (EPHY_LOCATION_ENTRY (proxy));
	}
}

void
ephy_location_action_clear_history (EphyLocationAction *action)
{
	GSList *proxies;

	proxies = gtk_action_get_proxies (GTK_ACTION (action));

	g_slist_foreach (proxies, (GFunc) clear_history, NULL);
}
