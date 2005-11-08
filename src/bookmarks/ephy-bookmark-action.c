/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2005 Peter Harvey
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

#include "egg-editable-toolbar.h"
#include "ephy-bookmark-action.h"
#include "ephy-bookmarks.h"
#include "ephy-link-action.h"
#include "ephy-link.h"
#include "ephy-favicon-cache.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#include <string.h>

/* FIXME tweak this, or make it configurable? (bug 148093) */
#define ENTRY_WIDTH_CHARS	12
#define TOOLITEM_WIDTH_CHARS	20
#define LABEL_WIDTH_CHARS       32

static void ephy_bookmark_action_init       (EphyBookmarkAction *action);
static void ephy_bookmark_action_class_init (EphyBookmarkActionClass *class);

#define EPHY_BOOKMARK_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARK_ACTION, EphyBookmarkActionPrivate))

struct _EphyBookmarkActionPrivate
{
	EphyNode *node;
	gboolean smart_url;
	guint cache_handler;
};

enum
{
	PROP_0,
	PROP_BOOKMARK,
	PROP_TOOLTIP,
	PROP_LOCATION,
	PROP_SMART_URL,
	PROP_ICON
};

static GObjectClass *parent_class = NULL;

GType
ephy_bookmark_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
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

		type = g_type_register_static (EPHY_TYPE_LINK_ACTION,
					       "EphyBookmarkAction",
					       &type_info, 0);
	}

	return type;
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *item, *button, *hbox, *label, *icon, *entry;

	LOG ("Creating tool item for action %p", action);

	item = GTK_ACTION_CLASS (parent_class)->create_tool_item (action);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	button = gtk_button_new ();
	gtk_widget_add_events (GTK_WIDGET (button), GDK_BUTTON1_MOTION_MASK);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);
	g_object_set_data (G_OBJECT (item), "egg-drag-source", button);

	entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (entry), ENTRY_WIDTH_CHARS);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "entry", entry);

	hbox = gtk_hbox_new (FALSE, 3);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	icon = gtk_image_new ();
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (hbox), icon, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "icon", icon);

	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (label), TOOLITEM_WIDTH_CHARS);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "label", label);

	return item;
}

static void
ephy_bookmark_action_sync_smart_url (GtkAction *gaction, GParamSpec *pspec, GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy))
	{
		EphyBookmarkAction *action = EPHY_BOOKMARK_ACTION (gaction);
		EphyBookmarkActionPrivate *priv = action->priv;
		gboolean is_smart_url = priv->smart_url;
		GtkWidget *entry, *icon;
		guint width;

		width = is_smart_url ? ephy_bookmarks_get_smart_bookmark_width (priv->node) : 0;

		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
		icon = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "icon"));

		g_object_set (icon, "visible", !is_smart_url, NULL);
		g_object_set (entry, "visible", is_smart_url, NULL);
		gtk_entry_set_width_chars (GTK_ENTRY (entry),
					   width > 0 ? width : ENTRY_WIDTH_CHARS);
	}
}

static void
favicon_cache_changed_cb (EphyFaviconCache *cache,
			  const char *icon_address,
			  EphyBookmarkAction *action)
{
	const char *icon;

	g_return_if_fail (action->priv->node != NULL);

	icon = ephy_node_get_property_string (action->priv->node,
					      EPHY_NODE_BMK_PROP_ICON);
	
	if (icon != NULL && strcmp (icon, icon_address) == 0)
	{
		g_signal_handler_disconnect (cache, action->priv->cache_handler);
		action->priv->cache_handler = 0;

		g_object_notify (G_OBJECT (action), "icon");
	}
}

static void
ephy_bookmark_action_sync_icon (GtkAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	EphyBookmarkAction *bma = EPHY_BOOKMARK_ACTION (action);
	const char *icon_location;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	g_return_if_fail (bma->priv->node != NULL);

	icon_location = ephy_node_get_property_string (bma->priv->node,
						       EPHY_NODE_BMK_PROP_ICON);

	cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell)));

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);

		if (pixbuf == NULL && bma->priv->cache_handler == 0)
		{
			bma->priv->cache_handler =
				g_signal_connect_object
					(cache, "changed",
					 G_CALLBACK (favicon_cache_changed_cb),
					 action, 0);
		}
	}

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		GtkImage *icon;

		icon = GTK_IMAGE (g_object_get_data (G_OBJECT (proxy), "icon"));
		g_return_if_fail (icon != NULL);

		if (pixbuf == NULL)
		{
			pixbuf = gtk_widget_render_icon (proxy, GTK_STOCK_NEW,
							 GTK_ICON_SIZE_MENU, NULL);
		}

		gtk_image_set_from_pixbuf (icon, pixbuf);
	}
	else if (GTK_IS_MENU_ITEM (proxy) && pixbuf)
	{
		GtkWidget *image;

		image = gtk_image_new_from_pixbuf (pixbuf);
		gtk_widget_show (image);

		gtk_image_menu_item_set_image
			(GTK_IMAGE_MENU_ITEM (proxy), image);
	}

	if (pixbuf)
	{
		g_object_unref (pixbuf);
	}
}

static void
ephy_bookmark_action_sync_label (GtkAction *gaction,
				 GParamSpec *pspec,
				 GtkWidget *proxy)
{
	EphyBookmarkAction *action = EPHY_BOOKMARK_ACTION (gaction);

	g_return_if_fail (EPHY_IS_NODE (action->priv->node));

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		GtkWidget *label = NULL;
		const char *title;
		char *label_text;

		label = g_object_get_data (G_OBJECT (proxy), "label");
		g_return_if_fail (label != NULL);

		title = ephy_node_get_property_string
			(action->priv->node, EPHY_NODE_BMK_PROP_TITLE);

		if (action->priv->smart_url)
		{
			label_text = g_strdup_printf (_("%s:"), title);
	
			gtk_label_set_label (GTK_LABEL (label), label_text);
			g_free (label_text);
		}
		else
		{
			gtk_label_set_label (GTK_LABEL (label), title);
		}
	}
}

static void
bookmark_activate_with_flags (GtkWidget *widget,
				EphyBookmarkAction *action,
				EphyLinkFlags flags)
{
	EphyBookmarkActionPrivate *priv = action->priv;
	EphyBookmarks *bookmarks;
	const char *location;
	char *address = NULL, *text = NULL;

	g_return_if_fail (priv->node != NULL);

	location = ephy_node_get_property_string
			(priv->node, EPHY_NODE_BMK_PROP_LOCATION);
	g_return_if_fail (location != NULL);
	
	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	
	if (GTK_IS_EDITABLE (widget))
	{
		text = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
	}
	
	/* The entered search term is empty, and we have a smart bookmark */
	if ((text == NULL || text[0] == '\0') && strstr (location, "%s") != NULL)
	{
		GnomeVFSURI *uri = gnome_vfs_uri_new (location);
		if (uri != NULL)
		{
			address = g_strconcat (
					gnome_vfs_uri_get_scheme (uri),
					"://",
					gnome_vfs_uri_get_host_name (uri),
					NULL);
			gnome_vfs_uri_unref (uri);
		}
	}

	if (address == NULL)
	{
		address = ephy_bookmarks_resolve_address (bookmarks, location, text);
	}
	g_return_if_fail (address != NULL);

	ephy_link_open (EPHY_LINK (action), address, NULL, flags);

	g_free (address);
	g_free (text);
}

static void
open_in_tab_activate_cb (GtkWidget *widget,
			 EphyBookmarkAction *action)
{
	bookmark_activate_with_flags(widget, action,
			EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO);
}

static void
open_in_window_activate_cb (GtkWidget *widget,
			EphyBookmarkAction *action)
{
	bookmark_activate_with_flags(widget, action,
			EPHY_LINK_NEW_WINDOW);
}

static void
activate_cb (GtkWidget *widget,
	     	EphyBookmarkAction *action)
{
	bookmark_activate_with_flags(widget, action,
			ephy_gui_is_middle_click () ? EPHY_LINK_NEW_TAB : 0);
}

static void
properties_activate_cb (GtkWidget *menu,
			EphyBookmarkAction *action)
{
	GtkWidget *window, *proxy;
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	proxy = g_object_get_data (G_OBJECT (menu), "proxy");
	window = gtk_widget_get_toplevel (proxy);

	ephy_bookmarks_show_bookmark_properties
		(bookmarks, action->priv->node, window);
}

static void
show_context_menu (EphyBookmarkAction *action,
		   GtkWidget *proxy,
		   GdkEventButton *event,
		   GtkMenuPositionFunc func)
{
	GtkWidget *menu, *item;

	menu = gtk_menu_new ();

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_in_tab_activate_cb), action);

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_in_window_activate_cb), action);

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
	g_object_set_data (G_OBJECT (item), "proxy", proxy);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (properties_activate_cb), action);
    
	egg_editable_toolbar_add_popup_items (proxy, GTK_MENU (menu));

	if (event != NULL)
	{
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, func, proxy,
				event->button, event->time);
	}
	else
	{
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, func, proxy, 0,
				gtk_get_current_event_time ());
		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	}
}

static gboolean
popup_menu_cb (GtkWidget *widget, EphyBookmarkAction *action)
{
	show_context_menu (action, widget, NULL, ephy_gui_menu_position_under_widget);
	return TRUE;
}

static gboolean
button_press_cb (GtkWidget *widget,
		 GdkEventButton *event,
		 EphyBookmarkAction *action)
{
	if (event->button == 3)
	{
		show_context_menu (action, widget, event, NULL);
		return TRUE;
	}
	else if (event->button == 2)	
	{
		gtk_button_pressed (GTK_BUTTON (widget));
	}

	return FALSE;
}

static gboolean
button_release_cb (GtkWidget *widget,
		   GdkEventButton *event,
		   EphyBookmarkAction *action)
{
	if (event->button == 2)	
	{
		gtk_button_released (GTK_BUTTON (widget));
	}

	return FALSE;
}


static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *button, *entry;

	LOG ("Connecting action %p to proxy %p", action, proxy);

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);

	ephy_bookmark_action_sync_icon (action, NULL, proxy);
	g_signal_connect_object (action, "notify::icon",
				 G_CALLBACK (ephy_bookmark_action_sync_icon), proxy, 0);

	ephy_bookmark_action_sync_smart_url (action, NULL, proxy);
	g_signal_connect_object (action, "notify::smarturl",
				 G_CALLBACK (ephy_bookmark_action_sync_smart_url), proxy, 0);

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		ephy_bookmark_action_sync_label (action, NULL, proxy);
		g_signal_connect_object (action, "notify::label",
					 G_CALLBACK (ephy_bookmark_action_sync_label), proxy, 0);

		button = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "button"));
		g_signal_connect (button, "clicked", G_CALLBACK (activate_cb), action);
		g_signal_connect (button, "popup_menu",
				  G_CALLBACK (popup_menu_cb), action);
		g_signal_connect (button, "button-press-event",
				  G_CALLBACK (button_press_cb), action);
		g_signal_connect (button, "button-release-event",
				  G_CALLBACK (button_release_cb), action);

		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
		g_signal_connect (entry, "activate", G_CALLBACK (activate_cb), action);
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		GtkLabel *label;

		label = (GtkLabel *) ((GtkBin *) proxy)->child;

		gtk_label_set_use_underline (label, FALSE);
		gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars (label, LABEL_WIDTH_CHARS);

		g_signal_connect (proxy, "activate", G_CALLBACK (activate_cb), action);
	}
}

void
ephy_bookmark_action_updated (EphyBookmarkAction *action)
{
	GValue value = { 0, };
	EphyBookmarks *bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	EphyNode *smart = ephy_bookmarks_get_smart_bookmarks (bookmarks);
	EphyNode *node = action->priv->node;
	const char *title;
	
	g_return_if_fail (action != NULL);
	g_return_if_fail (node != NULL);
	
	g_object_freeze_notify (G_OBJECT (action));

	// Set smart_url
	action->priv->smart_url = ephy_node_has_child (smart, node);
	g_object_notify (G_OBJECT (action), "smarturl");
	
	// Set title
	title = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_TITLE);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_static_string (&value, title);
	g_object_set_property (G_OBJECT (action), "label", &value);
	g_value_unset (&value);
	
	// Notify all other properties
	g_object_notify (G_OBJECT (action), "location");
	g_object_notify (G_OBJECT (action), "tooltip");
	g_object_notify (G_OBJECT (action), "icon");
	
	g_object_thaw_notify (G_OBJECT (action));        
}

EphyNode *
ephy_bookmark_action_get_bookmark (EphyBookmarkAction *action)
{
	return action->priv->node;
}

void
ephy_bookmark_action_set_bookmark (EphyBookmarkAction *action,
				   EphyNode *node)
{        
	g_return_if_fail (node != NULL);
	
	action->priv->node = node;
	
	g_object_freeze_notify (G_OBJECT (action));
	
	g_object_notify (G_OBJECT (action), "bookmark");
	ephy_bookmark_action_updated (action);
	
	g_object_thaw_notify (G_OBJECT (action));        
}

static void
ephy_bookmark_action_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyBookmarkAction *action = EPHY_BOOKMARK_ACTION (object);

	switch (prop_id)
	{
		case PROP_BOOKMARK:
			ephy_bookmark_action_set_bookmark (action, g_value_get_pointer (value));
			break;
		case PROP_TOOLTIP:
		case PROP_LOCATION:
		case PROP_SMART_URL:
		case PROP_ICON:
			/* not writable */
			break;
	}
}

static void
ephy_bookmark_action_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	EphyBookmarkAction *action = EPHY_BOOKMARK_ACTION (object);

	g_return_if_fail (action->priv->node != NULL);

	switch (prop_id)
	{
		case PROP_BOOKMARK:
			g_value_set_pointer (value, action->priv->node);
			break;
		case PROP_TOOLTIP:
		case PROP_LOCATION:
			g_value_set_string (value,
				ephy_node_get_property_string (action->priv->node,
					EPHY_NODE_BMK_PROP_LOCATION));
			break;
		case PROP_SMART_URL:
			g_value_set_boolean (value, action->priv->smart_url);
			break;
		case PROP_ICON:
			g_value_set_string (value,
				ephy_node_get_property_string (action->priv->node,
					EPHY_NODE_BMK_PROP_ICON));
			break;
	}
}

static void
ephy_bookmark_action_finalize (GObject *object)
{
	LOG ("Bookmark action %p finalized", object);

	parent_class->finalize (object);
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

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "Bookmark",
							       "Bookmark",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));

	/* overwrite GtkActionClass::tooltip, so we can use the url as tooltip */
	g_object_class_install_property (object_class,
					 PROP_TOOLTIP,
					 g_param_spec_string  ("tooltip",
							       "Tooltip",
							       "Tooltip",
							       NULL,
							       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	
	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_string  ("location",
							       "Location",
							       "Location",
							       NULL,
							       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	g_object_class_install_property (object_class,
					 PROP_SMART_URL,
					 g_param_spec_boolean  ("smarturl",
								"Smart url",
								"Smart url",
								FALSE,
								G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_string  ("icon",
							       "Icon",
							       "Icon",
							       NULL,
							       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyBookmarkActionPrivate));
}

static void
ephy_bookmark_action_init (EphyBookmarkAction *action)
{
	action->priv = EPHY_BOOKMARK_ACTION_GET_PRIVATE (action);
	action->priv->cache_handler = 0;
}

char *
ephy_bookmark_action_name (EphyNode *node)
{
	return g_strdup_printf("Bmk%u", ephy_node_get_id (node));
}

GtkAction *
ephy_bookmark_action_new (EphyNode *node, char *name)
{
	GtkAction *action;
	
	if(!name) name = ephy_bookmark_action_name (node);
	g_return_val_if_fail (name, NULL);
	
	action =  GTK_ACTION (g_object_new (EPHY_TYPE_BOOKMARK_ACTION,
					    "name", name,
					    "bookmark", node,
					    NULL));

	return action;
}
