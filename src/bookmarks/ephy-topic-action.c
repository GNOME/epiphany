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
#include "ephy-gui.h"

static void ephy_topic_action_init       (EphyTopicAction *action);
static void ephy_topic_action_class_init (EphyTopicActionClass *class);

struct EphyTopicActionPrivate
{
	int topic_id;
};

enum
{
	PROP_0,
	PROP_TOPIC_ID
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
	GtkWidget *arrow;
	GtkWidget *hbox;
	GtkWidget *label;

	item = (* EGG_ACTION_CLASS (parent_class)->create_tool_item) (action);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_widget_show (arrow);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	label = gtk_label_new (NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "label", label);

	return item;
}

static void
menu_deactivate_cb (GtkMenuShell *ms, GtkWidget *button)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

static void
menu_activate_cb (GtkWidget *item, EggAction *action)
{
	EphyNode *node;
	const char *location;

	node = EPHY_NODE (g_object_get_data (G_OBJECT (item), "node"));
	location = ephy_node_get_property_string
		(node, EPHY_NODE_BMK_PROP_LOCATION);
	g_signal_emit (action, ephy_topic_action_signals[GO_LOCATION],
		       0, location);
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

static GtkWidget *
build_bookmarks_menu (EphyTopicAction *action, EphyNode *node)
{
	GtkWidget *menu, *item;
	GPtrArray *children;
	int i;
        EphyFaviconCache *cache;

	menu = gtk_menu_new ();

        cache = ephy_embed_shell_get_favicon_cache
               (EPHY_EMBED_SHELL (ephy_shell));

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
                const char *icon_location;
		const char *title;

		kid = g_ptr_array_index (children, i);

		icon_location = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_ICON);
		title = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_TITLE);
		if (title == NULL) continue;
		LOG ("Create menu for bookmark %s", title)

		item = gtk_image_menu_item_new_with_label (title);
		if (icon_location)
		{
			GdkPixbuf *icon;
			GtkWidget *image;

			icon = ephy_favicon_cache_get (cache, icon_location);
			if (icon != NULL)
			{
				image = gtk_image_new_from_pixbuf (icon);
				gtk_widget_show (image);
				gtk_image_menu_item_set_image
					(GTK_IMAGE_MENU_ITEM (item), image);
				g_object_unref (icon);
			}
		}

		g_object_set_data (G_OBJECT (item), "node", kid);
		g_signal_connect (item, "activate",
				  G_CALLBACK (menu_activate_cb), action);
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	ephy_node_thaw (node);

	return menu;
}

static GtkWidget *
build_topics_menu (EphyTopicAction *action, EphyNode *node)
{
	GtkWidget *menu, *item;
	GPtrArray *children;
	int i;
	EphyBookmarks *bookmarks;
	EphyNode *all;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	all = ephy_bookmarks_get_bookmarks (bookmarks);

	menu = gtk_menu_new ();

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *title;
		GtkWidget *bmk_menu;

		kid = g_ptr_array_index (children, i);
		if (kid == all) continue;

		title = ephy_node_get_property_string
			(kid, EPHY_NODE_KEYWORD_PROP_NAME);
		LOG ("Create menu for topic %s", title);

		item = gtk_image_menu_item_new_with_label (title);
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		bmk_menu = build_bookmarks_menu (action, kid);
		gtk_widget_show (bmk_menu);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), bmk_menu);
	}
	ephy_node_thaw (node);

	return menu;
}

static GtkWidget *
build_menu (EphyTopicAction *action)
{
	EphyNode *node;


	if (action->priv->topic_id == BOOKMARKS_NODE_ID)
	{
		EphyBookmarks *bookmarks;

		LOG ("Build all bookmarks crap menu")

		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		node = ephy_bookmarks_get_keywords (bookmarks);
		return build_topics_menu (action, node);
	}
	else
	{
		node = ephy_node_get_from_id (action->priv->topic_id);
		return build_bookmarks_menu (action, node);
	}
}

static void
button_toggled_cb (GtkWidget *button,
		   EphyTopicAction *action)
{
	GtkWidget *menu;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
		menu = build_menu (action);
		g_signal_connect (menu, "deactivate",
				  G_CALLBACK (menu_deactivate_cb), button);
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				ephy_gui_menu_position_under_widget,
				button, 1, gtk_get_current_event_time ());
	}

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
	g_signal_connect (button, "toggled",
			  G_CALLBACK (button_toggled_cb), action);
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
		case PROP_TOPIC_ID:
			bmk->priv->topic_id = g_value_get_int (value);
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
		case PROP_TOPIC_ID:
			g_value_set_boolean (value, bmk->priv->topic_id);
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
                                         PROP_TOPIC_ID,
                                         g_param_spec_int ("topic_id",
                                                           "topic_id",
                                                           "topic_id",
							   0,
							   G_MAXINT,
                                                           0,
                                                           G_PARAM_READWRITE));
}

static void
sync_topic_properties (EggAction *action, EphyNode *bmk)
{
	const char *title;

	title = ephy_node_get_property_string
		(bmk, EPHY_NODE_KEYWORD_PROP_NAME);

	g_object_set (action, "label", title, NULL);
}

static void
topic_child_changed_cb (EphyNode *node, EphyNode *child, EggAction *action)
{
	gulong id;

	id = EPHY_TOPIC_ACTION (action)->priv->topic_id;

	if (id == ephy_node_get_id (child))
	{
		sync_topic_properties (action, child);
	}
}

static void
ephy_topic_action_init (EphyTopicAction *action)
{
	EphyBookmarks *bookmarks;
	EphyNode *node;

	action->priv = g_new0 (EphyTopicActionPrivate, 1);

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_keywords (bookmarks);
	g_signal_connect_object (node, "child_changed",
				 G_CALLBACK (topic_child_changed_cb),
				 action, 0);
}

EggAction *
ephy_topic_action_new (const char *name, guint id)
{
	EphyNode *bmk;
	EphyBookmarks *bookmarks;
	EggAction *action;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	bmk = ephy_node_get_from_id (id);
	g_return_val_if_fail (bmk != NULL, NULL);

	action = EGG_ACTION (g_object_new (EPHY_TYPE_TOPIC_ACTION,
					   "topic_id", id,
					   "name", name,
					   NULL));

	sync_topic_properties (action, bmk);

	return action;
}

