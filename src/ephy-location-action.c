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

#include "ephy-location-action.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-debug.h"
#include "eggtoolitem.h"

static void ephy_location_action_init       (EphyLocationAction *action);
static void ephy_location_action_class_init (EphyLocationActionClass *class);

enum
{
	GO_LOCATION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_location_action_signals[LAST_SIGNAL] = { 0 };

GType
ephy_location_action_get_type (void)
{
	static GtkType type = 0;

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

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphyLocationAction",
					       &type_info, 0);
	}
	return type;
}

static GtkWidget *
create_tool_item (EggAction *action)
{
	GtkWidget *item;
	GtkWidget *location;

	LOG ("Create location toolitem")

	item = (* EGG_ACTION_CLASS (parent_class)->create_tool_item) (action);

	location = ephy_location_entry_new ();
	gtk_container_add (GTK_CONTAINER (item), location);
	egg_tool_item_set_expandable (EGG_TOOL_ITEM (item), TRUE);
	gtk_widget_show (location);

	LOG ("Create location toolitem: Done.")

	return item;
}

static void
location_url_activate_cb (EphyLocationEntry *entry,
			  const char *content,
			  const char *target,
			  EphyLocationAction *action)
{
	EphyBookmarks *bookmarks;
	LOG ("Location url activated")
	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	if (!content)
	{
		LOG ("Go to %s", target);
		g_signal_emit (action,
			       ephy_location_action_signals[GO_LOCATION],
			       0, target);
	}
	else
	{
		char *url;

		url = ephy_bookmarks_solve_smart_url
			(bookmarks, target, content);
		g_return_if_fail (url != NULL);
		LOG ("Go to %s", url);
		g_signal_emit (action,
			       ephy_location_action_signals[GO_LOCATION],
			       0, url);
		g_free (url);
	}
}

static void
connect_proxy (EggAction *action, GtkWidget *proxy)
{
	EphyAutocompletion *ac = ephy_shell_get_autocompletion (ephy_shell);
	EphyLocationEntry *e;

	LOG ("Connect location proxy")

	e = EPHY_LOCATION_ENTRY (GTK_BIN (proxy)->child);
	ephy_location_entry_set_autocompletion (e, ac);

	g_signal_connect (e, "activated",
			  GTK_SIGNAL_FUNC(location_url_activate_cb),
			  action);

	(* EGG_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
ephy_location_action_class_init (EphyLocationActionClass *class)
{
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->toolbar_item_type = EGG_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	ephy_location_action_signals[GO_LOCATION] =
                g_signal_new ("go_location",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyLocationActionClass, go_location),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
}

static void
ephy_location_action_init (EphyLocationAction *action)
{
}

GtkWidget *
ephy_location_action_get_widget (EphyLocationAction *action)
{
	GSList *slist;

	slist = EGG_ACTION (action)->proxies;

	if (slist)
	{
		return GTK_BIN (slist->data)->child;
	}
	else
	{
		return NULL;
	}
}

