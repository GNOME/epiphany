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

#include "ephy-topic-action.h"
#include "ephy-bookmarks.h"
#include "ephy-shell.h"
#include "eggtoolitem.h"
#include "ephy-debug.h"

static void ephy_topic_action_init       (EphyTopicAction *action);
static void ephy_topic_action_class_init (EphyTopicActionClass *class);

struct EphyTopicActionPrivate
{
	int bookmark_id;
};

enum
{
	PROP_0,
	PROP_BOOKMARK_ID
};

enum
{
	GO_LOCATION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_topic_action_signals[LAST_SIGNAL] = { 0 };

GType
ephy_topic_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyTopicActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_topic_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyTopicAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_topic_action_init,
		};

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphyTopicAction",
					       &type_info, 0);
	}
	return type;
}

static GtkWidget *
create_tool_item (EggAction *action)
{
	GtkWidget *item;
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *label;

	item = (* EGG_ACTION_CLASS (parent_class)->create_tool_item) (action);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	label = gtk_label_new (NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "label", label);

	return item;
}

static void
ephy_topic_action_sync_label (EggAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkLabel *label;

	LOG ("Set bookmark action proxy label to %s", action->label)

	label = GTK_LABEL (g_object_get_data (G_OBJECT (proxy), "label"));
	g_return_if_fail (label != NULL);

	gtk_label_set_label (label, action->label);
}

static void
button_press_cb (GtkWidget *button,
		 GdkEventButton *event,
		 EphyTopicAction *action)
{
/*
	if (event->button == 3)
	{
		GtkWidget *menu;

		menu = build_topics_menu (action);
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 1,
				gtk_get_current_event_time ());
	}
*/
}

static void
connect_proxy (EggAction *action, GtkWidget *proxy)
{
	GtkWidget *button;

	(* EGG_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);

	ephy_topic_action_sync_label (action, NULL, proxy);
	g_signal_connect_object (action, "notify::label",
			         G_CALLBACK (ephy_topic_action_sync_label), proxy, 0);

	button = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "button"));
	g_signal_connect (button, "button_press_event",
			  G_CALLBACK (button_press_cb), action);
}

static void
ephy_topic_action_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	EphyTopicAction *bmk;

	bmk = EPHY_TOPIC_ACTION (object);

	switch (prop_id)
	{
		case PROP_BOOKMARK_ID:
			bmk->priv->bookmark_id = g_value_get_int (value);
			break;
	}
}

static void
ephy_topic_action_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	EphyTopicAction *bmk;

	bmk = EPHY_TOPIC_ACTION (object);

	switch (prop_id)
	{
		case PROP_BOOKMARK_ID:
			g_value_set_boolean (value, bmk->priv->bookmark_id);
			break;
	}
}

static void
ephy_topic_action_finalize (GObject *object)
{
        EphyTopicAction *eba;

	g_return_if_fail (EPHY_IS_TOPIC_ACTION (object));

	eba = EPHY_TOPIC_ACTION (object);

        g_return_if_fail (eba->priv != NULL);

        g_free (eba->priv);

	LOG ("Bookmark action finalized")

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_topic_action_class_init (EphyTopicActionClass *class)
{
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->toolbar_item_type = EGG_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	object_class->finalize = ephy_topic_action_finalize;
	object_class->set_property = ephy_topic_action_set_property;
	object_class->get_property = ephy_topic_action_get_property;

	ephy_topic_action_signals[GO_LOCATION] =
                g_signal_new ("go_location",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyTopicActionClass, go_location),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);

	g_object_class_install_property (object_class,
                                         PROP_BOOKMARK_ID,
                                         g_param_spec_int ("bookmark_id",
                                                           "bookmark_id",
                                                           "bookmark_id",
							   0,
							   G_MAXINT,
                                                           0,
                                                           G_PARAM_READWRITE));
}

static void
ephy_topic_action_init (EphyTopicAction *action)
{
	action->priv = g_new0 (EphyTopicActionPrivate, 1);
}

EggAction *
ephy_topic_action_new (const char *name, guint id)
{
	EphyNode *bmk;
	const char *title;
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	bmk = ephy_node_get_from_id (id);
	g_return_val_if_fail (bmk != NULL, NULL);

	title = ephy_node_get_property_string
		(bmk, EPHY_NODE_KEYWORD_PROP_NAME);

	return EGG_ACTION (g_object_new (EPHY_TYPE_TOPIC_ACTION,
					 "name", name,
					 "label", title,
					 NULL));
}

