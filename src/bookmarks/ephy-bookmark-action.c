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

#include <libgnome/gnome-i18n.h>

#include "ephy-bookmark-action.h"
#include "ephy-bookmarks.h"
#include "ephy-shell.h"
#include "eggtoolitem.h"
#include "ephy-debug.h"

static void ephy_bookmark_action_init       (EphyBookmarkAction *action);
static void ephy_bookmark_action_class_init (EphyBookmarkActionClass *class);

struct EphyBookmarkActionPrivate
{
	int bookmark_id;
	char *location;
	char *smart_url;
	char *icon;
};

enum
{
	PROP_0,
	PROP_BOOKMARK_ID,
	PROP_LOCATION,
	PROP_SMART_URL,
	PROP_ICON
};

enum
{
	GO_LOCATION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_bookmark_action_signals[LAST_SIGNAL] = { 0 };

GType
ephy_bookmark_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyBookmarkActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_bookmark_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyBookmarkAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_bookmark_action_init,
		};

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphyBookmarkAction",
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
	GtkWidget *icon;

	item = (* EGG_ACTION_CLASS (parent_class)->create_tool_item) (action);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	if (EPHY_BOOKMARK_ACTION (action)->priv->smart_url)
	{
		GtkWidget *entry;

		entry = gtk_entry_new ();
		gtk_widget_show (entry);
		gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
		g_object_set_data (G_OBJECT (item), "entry", entry);
	}

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	icon = gtk_image_new ();
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (hbox), icon, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "icon", icon);

	label = gtk_label_new (NULL);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "label", label);

	return item;
}

static void
ephy_bookmark_action_sync_icon (EggAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	char *icon_location;
	GtkImage *icon;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	icon = GTK_IMAGE (g_object_get_data (G_OBJECT (proxy), "icon"));
	g_return_if_fail (icon != NULL);

	icon_location = EPHY_BOOKMARK_ACTION (action)->priv->icon;

	cache = ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell));

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	if (pixbuf)
	{
		gtk_image_set_from_pixbuf (icon, pixbuf);
		g_object_unref (pixbuf);
	}
}

static void
ephy_bookmark_action_sync_label (EggAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkLabel *label;
	gchar *toolbar_label;

	LOG ("Set bookmark action proxy label to %s", action->label)

	label = GTK_LABEL (g_object_get_data (G_OBJECT (proxy), "label"));
	g_return_if_fail (label != NULL);

	if (EPHY_BOOKMARK_ACTION (action)->priv->smart_url)
 	{
		toolbar_label = g_strdup_printf (_("%s:"), action->label);
	}
	else
	{
		toolbar_label = g_strdup (action->label);
	}

	gtk_label_set_label (label, toolbar_label);
	g_free (toolbar_label);
}

static void
entry_activated_cb (GtkWidget *entry, EggAction *action)
{
	char *text;
	char *solved;
	char *smart_url;
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (text == NULL) return;

	smart_url = EPHY_BOOKMARK_ACTION (action)->priv->smart_url;
	solved = ephy_bookmarks_solve_smart_url (bookmarks,
						 smart_url,
						 text);
	g_signal_emit (action,
		       ephy_bookmark_action_signals[GO_LOCATION],
		       0, solved);

	g_free (text);
}

static void
button_clicked_cb (GtkWidget *button, EggAction *action)
{
	g_signal_emit (action,
		       ephy_bookmark_action_signals[GO_LOCATION],
		       0, EPHY_BOOKMARK_ACTION (action)->priv->location);
}

static void
connect_proxy (EggAction *action, GtkWidget *proxy)
{
	GtkWidget *button;

	(* EGG_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);

	ephy_bookmark_action_sync_label (action, NULL, proxy);
	g_signal_connect_object (action, "notify::label",
			         G_CALLBACK (ephy_bookmark_action_sync_label), proxy, 0);

	ephy_bookmark_action_sync_icon (action, NULL, proxy);
	g_signal_connect_object (action, "notify::icon",
			         G_CALLBACK (ephy_bookmark_action_sync_icon), proxy, 0);

	button = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "button"));
	g_signal_connect (button, "clicked", G_CALLBACK (button_clicked_cb), action);

	if (EPHY_BOOKMARK_ACTION (action)->priv->smart_url)
	{
		GtkWidget *entry;

		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
		g_signal_connect (entry, "activate", G_CALLBACK (entry_activated_cb), action);
	}
}

static void
ephy_bookmark_action_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	EphyBookmarkAction *bmk;

	bmk = EPHY_BOOKMARK_ACTION (object);

	switch (prop_id)
	{
		case PROP_BOOKMARK_ID:
			bmk->priv->bookmark_id = g_value_get_int (value);
			break;
		case PROP_LOCATION:
			g_free (bmk->priv->location);
			bmk->priv->location = g_strdup (g_value_get_string (value));
			break;
		case PROP_SMART_URL:
			g_free (bmk->priv->smart_url);
			bmk->priv->smart_url = g_strdup (g_value_get_string (value));
			break;
		case PROP_ICON:
			g_free (bmk->priv->icon);
			bmk->priv->icon = g_strdup (g_value_get_string (value));
			g_object_notify(object, "icon");
			break;
	}
}

static void
ephy_bookmark_action_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	EphyBookmarkAction *bmk;

	bmk = EPHY_BOOKMARK_ACTION (object);

	switch (prop_id)
	{
		case PROP_BOOKMARK_ID:
			g_value_set_boolean (value, bmk->priv->bookmark_id);
			break;
	}
}

static void
ephy_bookmark_action_finalize (GObject *object)
{
        EphyBookmarkAction *eba;

	g_return_if_fail (EPHY_IS_BOOKMARK_ACTION (object));

	eba = EPHY_BOOKMARK_ACTION (object);

        g_return_if_fail (eba->priv != NULL);

	g_free (eba->priv->location);
	g_free (eba->priv->smart_url);
	g_free (eba->priv->icon);

        g_free (eba->priv);

	LOG ("Bookmark action finalized")

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmark_action_class_init (EphyBookmarkActionClass *class)
{
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->toolbar_item_type = EGG_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	object_class->finalize = ephy_bookmark_action_finalize;
	object_class->set_property = ephy_bookmark_action_set_property;
	object_class->get_property = ephy_bookmark_action_get_property;

	ephy_bookmark_action_signals[GO_LOCATION] =
                g_signal_new ("go_location",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyBookmarkActionClass, go_location),
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
	g_object_class_install_property (object_class,
                                         PROP_LOCATION,
                                         g_param_spec_string  ("location",
                                                               "Location",
                                                               "Location",
                                                               NULL,
                                                               G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_SMART_URL,
                                         g_param_spec_string  ("smart_url",
                                                               "Smart url",
                                                               "Smart url",
                                                               NULL,
                                                               G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_ICON,
                                         g_param_spec_string  ("icon",
                                                               "Icon",
                                                               "Icon",
                                                               NULL,
                                                               G_PARAM_READWRITE));
}

static void
sync_bookmark_properties (EggAction *action, EphyNode *bmk)
{
	const char *title, *location, *smart_url, *icon;

	icon = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_ICON);
	title = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_TITLE);
	location = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_LOCATION);
	smart_url = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_SMART_LOCATION);
	if (smart_url && *smart_url == '\0') smart_url = NULL;

	g_object_set (action,
		      "label", title,
		      "location", location,
		      "smart_url", smart_url,
		      "icon", icon,
		      NULL);
}

static void
bookmarks_child_changed_cb (EphyNode *node, EphyNode *child, EggAction *action)
{
	gulong id;

	id = EPHY_BOOKMARK_ACTION (action)->priv->bookmark_id;

	if (id == ephy_node_get_id (child))
	{
		sync_bookmark_properties (action, child);
	}
}

static void
ephy_bookmark_action_init (EphyBookmarkAction *action)
{
	EphyBookmarks *bookmarks;
	EphyNode *node;

	action->priv = g_new0 (EphyBookmarkActionPrivate, 1);

	action->priv->location = NULL;
	action->priv->smart_url = NULL;
	action->priv->icon = NULL;


	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_bookmarks (bookmarks);
	g_signal_connect_object (node, "child_changed",
				 G_CALLBACK (bookmarks_child_changed_cb),
				 action, 0);
}

EggAction *
ephy_bookmark_action_new (const char *name, guint id)
{
	EphyNode *bmk;
	EphyBookmarks *bookmarks;
	EggAction *action;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	bmk = ephy_node_get_from_id (id);
	g_return_val_if_fail (bmk != NULL, NULL);

	action =  EGG_ACTION (g_object_new (EPHY_TYPE_BOOKMARK_ACTION,
				            "name", name,
					    "bookmark_id", id,
					    NULL));

	sync_bookmark_properties (action, bmk);

	return action;
}

