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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <gtk/gtktoolitem.h>

#include "ephy-bookmark-action.h"
#include "ephy-bookmarks.h"
#include "ephy-favicon-cache.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#define MAX_LABEL_LENGTH 30

static void ephy_bookmark_action_init       (EphyBookmarkAction *action);
static void ephy_bookmark_action_class_init (EphyBookmarkActionClass *class);

#define EPHY_BOOKMARK_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARK_ACTION, EphyBookmarkActionPrivate))

struct EphyBookmarkActionPrivate
{
	int bookmark_id;
	char *location;
	gboolean smart_url;
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

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyBookmarkAction",
					       &type_info, 0);
	}
	return type;
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *item, *button, *hbox, *label, *icon, *entry;

	LOG ("Creating tool item for action %p", action)

	item = (* GTK_ACTION_CLASS (parent_class)->create_tool_item) (action);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	entry = gtk_entry_new ();
	gtk_widget_set_size_request (entry, 120, -1);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "entry", entry);

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
ephy_bookmark_action_sync_smart_url (GtkAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy))
	{
		GtkWidget *entry;
		gboolean smart_url;

		smart_url = EPHY_BOOKMARK_ACTION (action)->priv->smart_url;
		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));

		if (smart_url)
		{
			gtk_widget_show (entry);
		}
		else
		{
			gtk_widget_hide (entry);
		}
	}
}

static void
ephy_bookmark_action_sync_icon (GtkAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	char *icon_location;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	icon_location = EPHY_BOOKMARK_ACTION (action)->priv->icon;

	cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell)));

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	if (pixbuf == NULL) return;

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		GtkImage *icon;

		icon = GTK_IMAGE (g_object_get_data (G_OBJECT (proxy), "icon"));
		g_return_if_fail (icon != NULL);

		gtk_image_set_from_pixbuf (icon, pixbuf);
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		GtkWidget *image;

		image = gtk_image_new_from_pixbuf (pixbuf);
		gtk_widget_show (image);

		gtk_image_menu_item_set_image
			(GTK_IMAGE_MENU_ITEM (proxy), image);
	}

	g_object_unref (pixbuf);
}

static void
ephy_bookmark_action_sync_label (GtkAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkWidget *label;
	char *label_text;
	char *title;
	GValue value = { 0, };

	g_value_init (&value, G_TYPE_STRING);
	g_object_get_property (G_OBJECT (action), "label", &value);
                                                                                                                             
	title = ephy_string_shorten (g_value_get_string (&value),
				     MAX_LABEL_LENGTH);
	g_value_unset (&value);

	if (EPHY_BOOKMARK_ACTION (action)->priv->smart_url
	    && GTK_IS_TOOL_ITEM (proxy))
	{
		label_text = g_strdup_printf (_("%s:"), title);
	}
	else
	{
		label_text = g_strdup (title);
	}

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		label = g_object_get_data (G_OBJECT (proxy), "label");
		g_return_if_fail (label != NULL);
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		label = GTK_BIN (proxy)->child;
	}
	else
	{
		g_warning ("Unkown widget");
		return;
	}

	gtk_label_set_label (GTK_LABEL (label), label_text);

	g_free (label_text);
	g_free (title);
}

static void
activate_cb (GtkWidget *widget, GtkAction *action)
{
	char *location = NULL;
	char *text = NULL;

	if (GTK_IS_EDITABLE (widget))
	{
		text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
	}

	if (text != NULL && text[0] != '\0')
	{
		char *smart_url;
		EphyBookmarks *bookmarks;

		bookmarks = ephy_shell_get_bookmarks (ephy_shell);

		smart_url = EPHY_BOOKMARK_ACTION (action)->priv->location;
		location = ephy_bookmarks_solve_smart_url (bookmarks,
							   smart_url,
							   text);
	}
	else
	{
		EphyBookmarkAction *baction = EPHY_BOOKMARK_ACTION (action);

		if (baction->priv->smart_url)
		{
			GnomeVFSURI *uri;

			uri = gnome_vfs_uri_new (baction->priv->location);

			if (uri)
			{
				location = g_strdup (gnome_vfs_uri_get_host_name (uri));
				gnome_vfs_uri_unref (uri);
			}
		}

		if (location == NULL)
		{
			location = g_strdup (baction->priv->location);
		}
	}

	g_signal_emit (action,
		       ephy_bookmark_action_signals[GO_LOCATION],
		       0, location);

	g_free (location);
	g_free (text);
}

static gboolean
create_menu_proxy (GtkToolItem *item, GtkAction *action)
{
	EphyBookmarkAction *bm_action = EPHY_BOOKMARK_ACTION (action);
	GtkWidget *menu_item;
	char *menu_id;

	LOG ("create_menu_proxy item %p, action %p", item, action);

	menu_item = GTK_ACTION_GET_CLASS (action)->create_menu_item (action);

	GTK_ACTION_GET_CLASS (action)->connect_proxy (action, menu_item);

	menu_id = g_strdup_printf ("ephy-bookmark-action-%d-menu-id",
				   bm_action->priv->bookmark_id);

	gtk_tool_item_set_proxy_menu_item (item, menu_id, menu_item);

	g_free (menu_id);

	return TRUE;	
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *button, *entry;

	LOG ("Connecting action %p to proxy %p", action, proxy)

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);

	ephy_bookmark_action_sync_label (action, NULL, proxy);
	g_signal_connect_object (action, "notify::label",
			         G_CALLBACK (ephy_bookmark_action_sync_label), proxy, 0);

	ephy_bookmark_action_sync_icon (action, NULL, proxy);
	g_signal_connect_object (action, "notify::icon",
			         G_CALLBACK (ephy_bookmark_action_sync_icon), proxy, 0);

	ephy_bookmark_action_sync_smart_url (action, NULL, proxy);
	g_signal_connect_object (action, "notify::smarturl",
			         G_CALLBACK (ephy_bookmark_action_sync_smart_url), proxy, 0);

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		button = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "button"));
		g_signal_connect (button, "clicked", G_CALLBACK (activate_cb), action);

		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
		g_signal_connect (entry, "activate", G_CALLBACK (activate_cb), action);

		g_signal_connect (proxy, "create_menu_proxy", G_CALLBACK (create_menu_proxy), action);
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "activate", G_CALLBACK (activate_cb), action);
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
			bmk->priv->smart_url = g_value_get_boolean (value);
			g_object_notify (object, "smarturl");
			break;
		case PROP_ICON:
			g_free (bmk->priv->icon);
			bmk->priv->icon = g_strdup (g_value_get_string (value));
			g_object_notify (object, "icon");
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
        EphyBookmarkAction *eba = EPHY_BOOKMARK_ACTION (object);

	g_free (eba->priv->location);
	g_free (eba->priv->icon);

	LOG ("Bookmark action finalized")

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmark_action_class_init (EphyBookmarkActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->menu_item_type = GTK_TYPE_IMAGE_MENU_ITEM;
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
                                         g_param_spec_boolean  ("smarturl",
                                                                "Smart url",
                                                                "Smart url",
                                                                FALSE,
                                                                G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_ICON,
                                         g_param_spec_string  ("icon",
                                                               "Icon",
                                                               "Icon",
                                                               NULL,
                                                               G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(EphyBookmarkActionPrivate));
}

static void
sync_bookmark_properties (GtkAction *action, EphyNode *bmk)
{
	const char *tmp, *location, *icon;
	char *title;
	gboolean smart_url;
	EphyBookmarks *bookmarks;
	EphyNode *smart_bmks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	smart_bmks = ephy_bookmarks_get_smart_bookmarks (bookmarks);

	icon = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_ICON);
	location = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_LOCATION);
	smart_url = ephy_node_has_child (smart_bmks, bmk);
	tmp = ephy_node_get_property_string
		(bmk, EPHY_NODE_BMK_PROP_TITLE);
	title = ephy_string_double_underscores (tmp);

	g_object_set (action,
		      "label", title,
		      "location", location,
		      "smarturl", smart_url,
		      "icon", icon,
		      NULL);

	g_free (title);
}

static void
bookmarks_child_changed_cb (EphyNode *node, EphyNode *child, GtkAction *action)
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

	action->priv = EPHY_BOOKMARK_ACTION_GET_PRIVATE (action);

	action->priv->location = NULL;
	action->priv->icon = NULL;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_bookmarks (bookmarks);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) bookmarks_child_changed_cb,
				         G_OBJECT (action));
}

GtkAction *
ephy_bookmark_action_new (const char *name, guint id)
{
	EphyNode *bmk;
	EphyBookmarks *bookmarks;
	GtkAction *action;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	bmk = ephy_bookmarks_get_from_id (bookmarks, id);
	g_return_val_if_fail (bmk != NULL, NULL);

	action =  GTK_ACTION (g_object_new (EPHY_TYPE_BOOKMARK_ACTION,
				            "name", name,
					    "bookmark_id", id,
					    NULL));

	sync_bookmark_properties (action, bmk);

	return action;
}
