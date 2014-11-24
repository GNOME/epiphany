/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2005 Peter Harvey
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
#include "ephy-bookmark-action.h"

#include "ephy-bookmarks-ui.h"
#include "ephy-bookmarks.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-prefs.h"
#include "ephy-favicon-helpers.h"
#include "ephy-gui.h"
#include "ephy-shell.h"
#include "ephy-string.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

/* FIXME tweak this, or make it configurable? (bug 148093) */
#define LABEL_WIDTH_CHARS       32

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

typedef struct
{
	GObject *weak_ptr;
	GtkWidget *entry;
	EphyLinkFlags flags;
} ClipboardCtx;

G_DEFINE_TYPE (EphyBookmarkAction, ephy_bookmark_action, EPHY_TYPE_LINK_ACTION)

static void
favicon_changed_cb (WebKitFaviconDatabase *database,
                    const char *page_address,
                    const char *icon_address,
                    EphyBookmarkAction *action)
{
        const char *icon;

        icon = ephy_node_get_property_string (action->priv->node,
                                              EPHY_NODE_BMK_PROP_ICON);
        if (g_strcmp0 (icon, icon_address) == 0)
        {
                g_signal_handler_disconnect (database, action->priv->cache_handler);
                action->priv->cache_handler = 0;

                g_object_notify (G_OBJECT (action), "icon");
        }
}

static void
async_get_favicon_pixbuf_callback (GObject *source, GAsyncResult *result, gpointer user_data)
{
       GtkWidget *proxy = GTK_WIDGET (user_data);
       WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
       GdkPixbuf *pixbuf = NULL;

       cairo_surface_t *icon_surface = webkit_favicon_database_get_favicon_finish (database, result, NULL);
       if (icon_surface)
       {
               pixbuf = ephy_pixbuf_get_from_surface_scaled (icon_surface, FAVICON_SIZE, FAVICON_SIZE);
               cairo_surface_destroy (icon_surface);
       }

       if (pixbuf)
       {
               if (GTK_IS_MENU_ITEM (proxy))
               {
                       GtkWidget *image;

                       image = gtk_image_new_from_pixbuf (pixbuf);
                       gtk_widget_show (image);

                       gtk_image_menu_item_set_image
                         (GTK_IMAGE_MENU_ITEM (proxy), image);
                       gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (proxy),
                                                                  TRUE);
               }
               g_object_unref (pixbuf);
       }

       g_object_unref (proxy);
}

static void
ephy_bookmark_action_sync_icon (GtkAction *action,
				GParamSpec *pspec,
				GtkWidget *proxy)
{
	EphyBookmarkAction *bma = EPHY_BOOKMARK_ACTION (action);
	const char *page_location;
	WebKitFaviconDatabase *database;
        EphyEmbedShell *shell = ephy_embed_shell_get_default ();

	g_return_if_fail (bma->priv->node != NULL);

	page_location = ephy_node_get_property_string (bma->priv->node,
						       EPHY_NODE_BMK_PROP_LOCATION);

        database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));

	if (page_location && *page_location)
	{
                webkit_favicon_database_get_favicon (database, page_location,
                                                     0, async_get_favicon_pixbuf_callback,
                                                     g_object_ref (proxy));
                if (bma->priv->cache_handler == 0)
                {
	                bma->priv->cache_handler =
			g_signal_connect_object
				(database, "favicon-changed",
				 G_CALLBACK (favicon_changed_cb),
				 action, 0);
                }
	}
}

void
ephy_bookmark_action_activate (EphyBookmarkAction *action,
			       GtkWidget *widget,
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
		char *scheme;
		char *host_name;

		scheme = g_uri_parse_scheme (location);
		host_name = ephy_string_get_host_name (location);
		address = g_strconcat (scheme,
				       "://",
				       host_name,
				       NULL);
		g_free (scheme);
		g_free (host_name);
	}

	if (address == NULL)
	{
		address = ephy_bookmarks_resolve_address (bookmarks, location, text);
	}
	g_return_if_fail (address != NULL);

	flags |= EPHY_LINK_BOOKMARK;

	ephy_link_open (EPHY_LINK (action), address, NULL, flags);

	g_free (address);
	g_free (text);
}

static void
activate_cb (GtkWidget *widget,
	     EphyBookmarkAction *action)
{
       gboolean control = FALSE;
       GdkEvent *event;

       event = gtk_get_current_event ();
       if (event)
       {
	       if (event->type == GDK_KEY_PRESS ||
		   event->type == GDK_KEY_RELEASE)
	       {
		       control = (event->key.state & gtk_accelerator_get_default_mod_mask ()) == GDK_CONTROL_MASK;
	       }

	       gdk_event_free (event);
       }

       ephy_bookmark_action_activate
	 (action, widget, (control || ephy_gui_is_middle_click ()) ? EPHY_LINK_NEW_TAB : 0);
}

static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{
	LOG ("Connecting action %p to proxy %p", action, proxy);

	GTK_ACTION_CLASS (ephy_bookmark_action_parent_class)->connect_proxy (action, proxy);

	ephy_bookmark_action_sync_icon (action, NULL, proxy);
	g_signal_connect_object (action, "notify::icon",
				 G_CALLBACK (ephy_bookmark_action_sync_icon), proxy, 0);

	if (GTK_IS_MENU_ITEM (proxy))
	{
		GtkLabel *label;

		label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

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
	EphyBookmarks *bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	EphyNode *smart = ephy_bookmarks_get_smart_bookmarks (bookmarks);
	EphyNode *node = action->priv->node;
	const char *title;
	
	g_return_if_fail (action != NULL);
	g_return_if_fail (node != NULL);
	
	g_object_freeze_notify (G_OBJECT (action));

	/* Set smart_url */
	action->priv->smart_url = ephy_node_has_child (smart, node);
	g_object_notify (G_OBJECT (action), "smarturl");
	
	/* Set title */
	title = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_TITLE);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_static_string (&value, title);
	g_object_set_property (G_OBJECT (action), "label", &value);
	g_value_unset (&value);
	
	/* Notify all other properties */
	g_object_notify (G_OBJECT (action), "location");
	g_object_notify (G_OBJECT (action), "icon");
	
	g_object_thaw_notify (G_OBJECT (action));

	/* We could force a tooltip re-query with gtk_tooltip_trigger_tooltip_query
	 * here, but it's not really worth it. Just show the updated tip next time
	 * the tip is queried.
	 */
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
	EphyBookmarkActionPrivate *priv = action->priv;
	GObject *object = G_OBJECT (action);

	g_return_if_fail (node != NULL);

	priv->node = node;

	g_object_freeze_notify (object);

	g_object_notify (object, "bookmark");
	ephy_bookmark_action_updated (action);

	g_object_thaw_notify (object);
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
	EphyBookmarkActionPrivate *priv = action->priv;

	g_return_if_fail (priv->node != NULL);

	switch (prop_id)
	{
		case PROP_BOOKMARK:
			g_value_set_pointer (value, priv->node);
			break;
		case PROP_TOOLTIP:
		case PROP_LOCATION:
			g_value_set_string (value,
				ephy_node_get_property_string (priv->node,
					EPHY_NODE_BMK_PROP_LOCATION));
			break;
		case PROP_SMART_URL:
			g_value_set_boolean (value, priv->smart_url);
			break;
		case PROP_ICON:
			g_value_set_string (value,
				ephy_node_get_property_string (priv->node,
					EPHY_NODE_BMK_PROP_ICON));
			break;
	}
}

static void
ephy_bookmark_action_init (EphyBookmarkAction *action)
{
	action->priv = EPHY_BOOKMARK_ACTION_GET_PRIVATE (action);
	
	action->priv->cache_handler = 0;
}

static void
ephy_bookmark_action_dispose (GObject *object)
{
	EphyBookmarkAction *action = (EphyBookmarkAction *) object;
	EphyBookmarkActionPrivate *priv = action->priv;

	if (priv->cache_handler != 0)
	{
                EphyEmbedShell *shell = ephy_embed_shell_get_default ();
		WebKitFaviconDatabase *database;

                database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));
		g_signal_handler_disconnect (database, priv->cache_handler);
		priv->cache_handler = 0;
	}

	G_OBJECT_CLASS (ephy_bookmark_action_parent_class)->dispose (object);
}

static void
ephy_bookmark_action_class_init (EphyBookmarkActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->menu_item_type = GTK_TYPE_IMAGE_MENU_ITEM;
	action_class->connect_proxy = connect_proxy;

	object_class->dispose = ephy_bookmark_action_dispose;
	object_class->set_property = ephy_bookmark_action_set_property;
	object_class->get_property = ephy_bookmark_action_get_property;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark", NULL, NULL,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));

	/* overwrite GtkActionClass::tooltip, so we can use the url as tooltip */
	g_object_class_install_property (object_class,
					 PROP_TOOLTIP,
					 g_param_spec_string  ("tooltip", NULL, NULL,
							       NULL,
							       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	
	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_string  ("location", NULL, NULL,
							       NULL,
							       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	g_object_class_install_property (object_class,
					 PROP_SMART_URL,
					 g_param_spec_boolean  ("smarturl", NULL, NULL,
								FALSE,
								G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_string  ("icon", NULL, NULL,
							       NULL,
							       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyBookmarkActionPrivate));
}

GtkAction *
ephy_bookmark_action_new (EphyNode *node,
			  const char *name)
{
	g_assert (name != NULL);

	return  GTK_ACTION (g_object_new (EPHY_TYPE_BOOKMARK_ACTION,
					  "name", name,
					  "bookmark", node,
					  NULL));
}
