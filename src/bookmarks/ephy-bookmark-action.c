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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <gtk/gtktoolitem.h>

#include "ephy-bookmark-action.h"
#include "ephy-marshal.h"
#include "ephy-dnd.h"
#include "ephy-bookmarksbar.h"
#include "ephy-bookmarks.h"
#include "ephy-favicon-cache.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "ephy-gui.h"

#include <string.h>

#define MAX_LABEL_LENGTH 32

static void ephy_bookmark_action_init       (EphyBookmarkAction *action);
static void ephy_bookmark_action_class_init (EphyBookmarkActionClass *class);

#define EPHY_BOOKMARK_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARK_ACTION, EphyBookmarkActionPrivate))

static GtkTargetEntry drag_targets[] =
{
	{ EPHY_DND_URL_TYPE, 0,	0 }
};
static int n_drag_targets = G_N_ELEMENTS (drag_targets);

struct EphyBookmarkActionPrivate
{
	EphyNode *bmk_node;
	int bookmark_id;
	char *location;
	gboolean smart_url;
	char *icon;
	guint cache_handler;

	guint motion_handler;
	gint drag_x;
	gint drag_y;
};

enum
{
	PROP_0,
	PROP_BOOKMARK_ID,
	PROP_TOOLTIP,
	PROP_LOCATION,
	PROP_SMART_URL,
	PROP_ICON
};

enum
{
	OPEN,
	OPEN_IN_TAB,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };

GType
ephy_bookmark_action_get_type (void)
{
	static GType type = 0;

	if (type == 0)
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
	gtk_widget_add_events (GTK_WIDGET (button), GDK_BUTTON1_MOTION_MASK);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	entry = gtk_entry_new ();
	gtk_widget_set_size_request (entry, 120, -1);
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
		GtkWidget *entry, *icon;
		gboolean smart_url;

		smart_url = EPHY_BOOKMARK_ACTION (action)->priv->smart_url;
		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
		icon = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "icon"));

		if (smart_url)
		{
			gtk_widget_hide (icon);
			gtk_widget_show (entry);
		}
		else
		{
			gtk_widget_show (icon);
			gtk_widget_hide (entry);
		}
	}
}

static void
favicon_cache_changed_cb (EphyFaviconCache *cache,
			  const char *icon,
			  EphyBookmarkAction *action)
{
	if (action->priv->icon && strcmp (icon, action->priv->icon) == 0)
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
	char *icon_location;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	icon_location = bma->priv->icon;

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
open_in_tab_activate_cb (GtkWidget *widget, EphyBookmarkAction *action)
{
	g_signal_emit (action, signals[OPEN_IN_TAB], 0,
		       action->priv->location, FALSE);
}

static void
open_in_window_activate_cb (GtkWidget *widget, EphyBookmarkAction *action)
{
	g_signal_emit (action, signals[OPEN_IN_TAB], 0,
		       action->priv->location, TRUE);
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

	if (ephy_gui_is_middle_click ())
	{
		g_signal_emit (action, signals[OPEN_IN_TAB], 0, location, FALSE);
	}
	else
	{
		g_signal_emit (action, signals[OPEN], 0, location);
	}

	g_free (location);
	g_free (text);
}

static void
stop_drag_check (EphyBookmarkAction *action, GtkWidget *widget)
{
	if (action->priv->motion_handler)
	{
		g_signal_handler_disconnect (widget, action->priv->motion_handler);
		action->priv->motion_handler = 0;
	}
}

static void
drag_data_get_cb (GtkWidget *widget, GdkDragContext *context,
		  GtkSelectionData *selection_data, guint info,
		  guint32 time, EphyBookmarkAction *action)
{
	char *address = action->priv->location;

	g_return_if_fail (address != NULL);

	gtk_selection_data_set (selection_data, selection_data->target, 8,
				(unsigned char *) address, strlen (address));
}

static int
get_item_position (GtkWidget *widget, gboolean *last)
{
	GtkWidget *item, *toolbar;
	int index;

	item = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM);
	g_return_val_if_fail (item != NULL, -1);

	toolbar = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOLBAR);
	g_return_val_if_fail (toolbar != NULL, -1);

	index = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolbar),
				            GTK_TOOL_ITEM (item));
	if (last)
	{
		int n_items;

		n_items = gtk_toolbar_get_n_items (GTK_TOOLBAR (toolbar));
		*last = (index == n_items - 1);
	}

	return index;
}

static void
remove_from_model (GtkWidget *widget)
{
	EphyBookmarks *bookmarks;
	EggToolbarsModel *model;
	int pos;

	pos = get_item_position (widget, NULL);

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	model = EGG_TOOLBARS_MODEL (ephy_bookmarks_get_toolbars_model (bookmarks));

	egg_toolbars_model_remove_item (model, 0, pos);
}

static void
move_in_model (GtkWidget *widget, int direction)
{
	EphyBookmarks *bookmarks;
	EggToolbarsModel *model;
	int pos, new_pos;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	model = EGG_TOOLBARS_MODEL (ephy_bookmarks_get_toolbars_model (bookmarks));

	pos = get_item_position (widget, NULL);
	new_pos = MAX (0, pos + direction);

	egg_toolbars_model_move_item (model, 0, pos, 0, new_pos);
}

static void
drag_data_delete_cb (GtkWidget *widget, GdkDragContext *context,
		     EphyBookmarkAction *action)
{
	remove_from_model (widget);
}

static gboolean
drag_motion_cb (GtkWidget *widget, GdkEventMotion *event, EphyBookmarkAction *action)
{
	if (gtk_drag_check_threshold (widget, action->priv->drag_x,
				      action->priv->drag_y, event->x, event->y))
	{
		GtkTargetList *target_list;

		target_list = gtk_target_list_new (drag_targets, n_drag_targets);

		stop_drag_check (action, widget);
		gtk_drag_begin (widget, target_list, GDK_ACTION_MOVE |
				GDK_ACTION_COPY, 1, (GdkEvent*)event);

		gtk_target_list_unref (target_list);
	}

	return TRUE;
}

static void
remove_activate_cb (GtkWidget *menu, GtkWidget *proxy)
{
	remove_from_model (proxy);
}

static void
move_left_activate_cb (GtkWidget *menu, GtkWidget *proxy)
{
	move_in_model (proxy, -1);
}

static void
move_right_activate_cb (GtkWidget *menu, GtkWidget *proxy)
{
	move_in_model (proxy, +1);
}

static void
properties_activate_cb (GtkWidget *menu, EphyBookmarkAction *action)
{
	GtkWidget *window, *proxy, *dialog;
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	proxy = g_object_get_data (G_OBJECT (menu), "proxy");
	window = gtk_widget_get_toplevel (proxy);

	dialog = ephy_bookmarks_show_bookmark_properties
		(bookmarks, action->priv->bmk_node, window);
}

static void
show_context_menu (EphyBookmarkAction *action, GtkWidget *proxy,
		   GtkMenuPositionFunc func)
{
	GtkWidget *menu, *item;
	gboolean last;

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

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, NULL);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (remove_activate_cb), proxy);

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Move _Left"));
	gtk_widget_set_sensitive (item, get_item_position (proxy, NULL) > 0);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (move_left_activate_cb), proxy);

	item = gtk_menu_item_new_with_mnemonic (_("Move Ri_ght"));
	get_item_position (proxy, &last);
	gtk_widget_set_sensitive (item, !last);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "activate",
			  G_CALLBACK (move_right_activate_cb), proxy);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, func, proxy, 3,
			gtk_get_current_event_time ());
}

static gboolean
popup_menu_cb (GtkWidget *widget, EphyBookmarkAction *action)
{
	if (gtk_widget_get_ancestor (widget, EPHY_TYPE_BOOKMARKSBAR))
        {
                show_context_menu (action, widget,
				   ephy_gui_menu_position_under_widget);
		return TRUE;
        }

	return FALSE;
}

static gboolean
button_press_cb (GtkWidget *widget,
		 GdkEventButton *event,
		 EphyBookmarkAction *action)
{
	if (event->button == 3 &&
	    gtk_widget_get_ancestor (widget, EPHY_TYPE_BOOKMARKSBAR))	
	{
		show_context_menu (action, widget, NULL);
		return TRUE;
	}
	else if (event->button == 2)	
	{
		gtk_button_pressed (GTK_BUTTON (widget));
	}
	else if (event->button == 1 &&
		 gtk_widget_get_ancestor (widget, EPHY_TYPE_BOOKMARKSBAR))
	{
		action->priv->drag_x = event->x;
		action->priv->drag_y = event->y;
		action->priv->motion_handler = g_signal_connect
			(widget, "motion_notify_event", G_CALLBACK (drag_motion_cb), action);
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
	else if (event->button == 1)
	{
		stop_drag_check (action, widget);
	}

	return FALSE;
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
		g_signal_connect (button, "popup_menu",
				  G_CALLBACK (popup_menu_cb), action);
		g_signal_connect (button, "button-press-event",
				  G_CALLBACK (button_press_cb), action);
		g_signal_connect (button, "button-release-event",
				  G_CALLBACK (button_release_cb), action);
		g_signal_connect (button, "drag_data_get",
				  G_CALLBACK (drag_data_get_cb), action);
		g_signal_connect (button, "drag_data_delete",
				  G_CALLBACK (drag_data_delete_cb), action);

		entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
		g_signal_connect (entry, "activate", G_CALLBACK (activate_cb), action);
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "activate", G_CALLBACK (activate_cb), action);
	}
}

static void
bookmark_destroy_cb (EphyNode *node, EphyBookmarkAction *action)
{
	action->priv->bmk_node = NULL;
}

static void
ephy_bookmark_action_set_bookmark_id (EphyBookmarkAction *action, guint id)
{
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	action->priv->bookmark_id = id;
	action->priv->bmk_node = ephy_bookmarks_get_from_id
				(bookmarks, action->priv->bookmark_id);

	ephy_node_signal_connect_object (action->priv->bmk_node, EPHY_NODE_DESTROY,
					 (EphyNodeCallback) bookmark_destroy_cb,
					 G_OBJECT (action));
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
			ephy_bookmark_action_set_bookmark_id
					(bmk, g_value_get_int (value));
			break;
		case PROP_TOOLTIP:
		case PROP_LOCATION:
			g_free (bmk->priv->location);
			bmk->priv->location = g_strdup (g_value_get_string (value));
			break;
		case PROP_SMART_URL:
			bmk->priv->smart_url = g_value_get_boolean (value);
			break;
		case PROP_ICON:
			g_free (bmk->priv->icon);
			bmk->priv->icon = g_value_dup_string (value);
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
		case PROP_TOOLTIP:
		case PROP_LOCATION:
			g_value_set_string (value, bmk->priv->location);
			break;
		case PROP_SMART_URL:
			g_value_set_boolean (value, bmk->priv->smart_url);
			break;
		case PROP_ICON:
			g_value_set_string (value, bmk->priv->icon);
			break;
	}
}

static void
ephy_bookmark_action_finalize (GObject *object)
{
        EphyBookmarkAction *eba = EPHY_BOOKMARK_ACTION (object);

	g_free (eba->priv->location);
	g_free (eba->priv->icon);

	LOG ("Bookmark action %p finalized", object)

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

	signals[OPEN] =
                g_signal_new ("open",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyBookmarkActionClass, open),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);

	signals[OPEN_IN_TAB] =
                g_signal_new ("open_in_tab",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyBookmarkActionClass, open_in_tab),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING_BOOLEAN,
                              G_TYPE_NONE,
                              2,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN);

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
                                         PROP_TOOLTIP,
                                         g_param_spec_string  ("tooltip",
                                                               "Tooltip",
                                                               "Tooltip",
                                                               NULL,
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
bookmarks_child_changed_cb (EphyNode *node,
			    EphyNode *child,
			    guint property_id,
			    GtkAction *action)
{
	guint id;

	id = EPHY_BOOKMARK_ACTION (action)->priv->bookmark_id;

	if (id == ephy_node_get_id (child))
	{
		sync_bookmark_properties (action, child);
	}
}

static void
smart_child_added_cb (EphyNode *smart_bmks,
		      EphyNode *child,
		      EphyBookmarkAction *action)
{
	if (action->priv->bookmark_id == ephy_node_get_id (child))
	{
		g_object_set (action, "smarturl", TRUE, NULL);
	}
}

static void
smart_child_removed_cb (EphyNode *smart_bmks,
			EphyNode *child,
			guint old_index,
			EphyBookmarkAction *action)
{
	if (action->priv->bookmark_id == ephy_node_get_id (child))
	{
		g_object_set (action, "smarturl", FALSE, NULL);
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
	action->priv->cache_handler = 0;
	action->priv->motion_handler = 0;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	node = ephy_bookmarks_get_bookmarks (bookmarks);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) bookmarks_child_changed_cb,
				         G_OBJECT (action));

	node = ephy_bookmarks_get_smart_bookmarks (bookmarks);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) smart_child_added_cb,
					 G_OBJECT (action));
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) smart_child_removed_cb,
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
