/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-topic-action.h"
#include "ephy-node.h"
#include "ephy-node-common.h"
#include "ephy-nodes-cover.h"
#include "ephy-bookmarks.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-bookmarks-menu.h"
#include "ephy-shell.h"
#include "ephy-gui.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

static const GtkTargetEntry dest_drag_types[] = {
	{ EPHY_DND_URL_TYPE, 0, 0},
};

#define TOOLITEM_WIDTH_CHARS	24
#define MENUITEM_WIDTH_CHARS	32
#define LABEL_WIDTH_CHARS       32

#define EPHY_TOPIC_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPIC_ACTION, EphyTopicActionPrivate))

struct _EphyTopicActionPrivate
{
	EphyNode *node;
	GtkUIManager *manager;
	guint merge_id;
};

enum
{
	PROP_0,
	PROP_TOPIC,
	PROP_MANAGER
};

G_DEFINE_TYPE (EphyTopicAction, ephy_topic_action, GTK_TYPE_ACTION)

static void
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *selection_data,
		       guint info,
		       guint time,
		       GtkAction *action)
{  
	const char *data;
	EphyBookmarks *bookmarks;
	EphyNode *bookmark, *topic;
	gchar **netscape_url;
	
	topic = ephy_topic_action_get_topic (EPHY_TOPIC_ACTION (action));
	
	data = (char *) gtk_selection_data_get_data (selection_data);
	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	
	netscape_url = g_strsplit (data, "\n", 2);
	if (!netscape_url || !netscape_url[0])
	{
		g_strfreev (netscape_url);
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	bookmark = ephy_bookmarks_find_bookmark (bookmarks, netscape_url[0]);
	if (bookmark == NULL)
	{
		bookmark = ephy_bookmarks_add (bookmarks, netscape_url[1], netscape_url[0]);
	}

	g_strfreev (netscape_url);
	
	if (bookmark != NULL)
	{
		ephy_bookmarks_set_keyword (bookmarks, topic, bookmark);
		gtk_drag_finish (context, TRUE, FALSE, time);
	}
	else
	{
		gtk_drag_finish (context, FALSE, FALSE, time);
	}
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *item;
	GtkWidget *button;
	GtkWidget *arrow;
	GtkWidget *hbox;
	GtkWidget *label;

	item = GTK_ACTION_CLASS (ephy_topic_action_parent_class)->create_tool_item (action);

	button = gtk_toggle_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (item), button);
	g_object_set_data (G_OBJECT (item), "button", button);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_widget_show (arrow);

	hbox = gtk_hbox_new (FALSE, 3);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (label), TOOLITEM_WIDTH_CHARS);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (item), "label", label);

	return item;
}

static void
ephy_topic_action_sync_label (GtkAction *action,
			      GParamSpec *pspec,
			      GtkWidget *proxy)
{
	GtkWidget *label = NULL;
	GValue value = { 0, };
	const char *label_text;

	g_value_init (&value, G_TYPE_STRING);
	g_object_get_property (G_OBJECT (action), "label", &value);

	label_text = g_value_get_string (&value);

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		label = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "label"));
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		label = gtk_bin_get_child (GTK_BIN (proxy));
	}
	else
	{
		g_warning ("Unknown widget");
		return;
	}

	g_return_if_fail (label != NULL);

	if (label_text)
	{
		gtk_label_set_label (GTK_LABEL (label), label_text);
	}
	
	g_value_unset (&value);
}

static GtkWidget *
get_popup (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;
	char path[40];

	g_snprintf (path, sizeof (path), "/PopupTopic%ld",
		    (long int) ephy_node_get_id (action->priv->node));

	if (priv->merge_id == 0)
	{
		GString *popup_menu_string;

		popup_menu_string = g_string_new (NULL);
		g_string_append_printf (popup_menu_string, "<ui><popup name=\"%s\">", path + 1);

		ephy_bookmarks_menu_build (popup_menu_string, priv->node);
		g_string_append (popup_menu_string, "</popup></ui>");

		priv->merge_id = gtk_ui_manager_add_ui_from_string
			(priv->manager, popup_menu_string->str,
			 popup_menu_string->len, 0);

		g_string_free (popup_menu_string, TRUE);
	}

	return gtk_ui_manager_get_widget (priv->manager, path);
}

static void
erase_popup (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;

	if (priv->merge_id != 0)
	{
		gtk_ui_manager_remove_ui (priv->manager, priv->merge_id);
		priv->merge_id = 0;
	}
}

static void
child_added_cb (EphyNode *node, EphyNode *child, GObject *object)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);
	erase_popup (action);
}

static void
child_changed_cb (EphyNode *node,
		  EphyNode *child,
		  guint property,
		  GObject *object)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);

	erase_popup (action);
}

static void
child_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint index,
		  GObject *object)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);

	erase_popup (action);
}

static void
menu_destroy_cb (GtkWidget *menuitem,
		 gpointer user_data)
{
	/* Save the submenu from similar destruction,
	 * because it doesn't rightly belong to this menuitem. */
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), NULL);
}

static void
menu_init_cb (GtkWidget *menuitem,
	      EphyTopicAction *action)
{
	if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem)) == NULL)
	{
		GtkWidget *popup;

		popup = get_popup (action);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), popup);
		g_signal_connect (menuitem, "destroy",
				  G_CALLBACK (menu_destroy_cb), NULL);
	}
}

static void
button_deactivate_cb (GtkMenuShell *ms,
		      GtkWidget *button)
{
	GtkWidget *window = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
	gtk_button_released (GTK_BUTTON (button));

	g_object_set_data (G_OBJECT (window),
			   "active-topic-action-button", NULL);

	/*
		Currently, GObject leaks connection IDs created with
		g_signal_connect_object ()
		See glib bug #118536
	*/
	g_signal_handlers_disconnect_by_func(ms, button_deactivate_cb, button);
}

static void
button_toggled_cb (GtkWidget *button,
		   EphyTopicAction *action)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
		GtkWidget *popup;
		GtkWidget *window;

		window = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);

		g_object_set_data (G_OBJECT (window),
				   "active-topic-action-button",
				   button);

		popup = get_popup (action);

		g_signal_connect_object (popup, "deactivate",
					 G_CALLBACK (button_deactivate_cb), button, 0);

		/* FIXME: ephy_gui_menu_position_menu_on_toolbar? */
		gtk_menu_popup (GTK_MENU (popup), NULL, NULL,
				ephy_gui_menu_position_under_widget,
				button, 1, gtk_get_current_event_time ());
	}
}

static gboolean
button_release_cb (GtkWidget *button,
                   GdkEventButton *event,
		   EphyTopicAction *action)
{
	if (event->button == 1)
	{
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON (button), FALSE);
	}

	return FALSE;
}

static gboolean
button_press_cb (GtkWidget *button,
		 GdkEventButton *event,
		 EphyTopicAction *action)
{
	if (event->button == 1)
	{
		if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		{
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
button_enter_cb (GtkWidget *button,
		 GdkEventCrossing *event,
		 EphyTopicAction *action)
{
	GtkWidget *window;
	GtkWidget *active_button;

	window = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);
	active_button = g_object_get_data (G_OBJECT (window),
					   "active-topic-action-button");

	if (active_button &&
	    active_button != button &&
	    GTK_IS_TOGGLE_BUTTON (active_button) &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (active_button)))
	{
		EphyTopicAction *active_action;
		GtkWidget *ancestor;

		ancestor = gtk_widget_get_ancestor (active_button, GTK_TYPE_TOOL_ITEM);
		active_action = (EphyTopicAction*)gtk_activatable_get_related_action (GTK_ACTIVATABLE (ancestor));

		erase_popup (active_action);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_button), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}

	return FALSE;
}

static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{
	GTK_ACTION_CLASS (ephy_topic_action_parent_class)->connect_proxy (action, proxy);
    
	ephy_topic_action_sync_label (action, NULL, proxy);
	g_signal_connect_object (action, "notify::label",
				 G_CALLBACK (ephy_topic_action_sync_label), proxy, 0);

	if (GTK_IS_TOOL_ITEM (proxy))
	{
		GtkWidget *button;

		button = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "button"));

		g_signal_connect (button, "toggled",
				  G_CALLBACK (button_toggled_cb), action);
		g_signal_connect (button, "button-press-event",
				  G_CALLBACK (button_press_cb), action);
		g_signal_connect (button, "button-release-event",
				  G_CALLBACK (button_release_cb), action);
		g_signal_connect (button, "enter-notify-event",
				  G_CALLBACK (button_enter_cb), action);
		/* FIXME: what about keyboard (toggled by Space) ? */

		g_signal_connect (button, "drag-data-received",
				  G_CALLBACK (drag_data_received_cb), action);
		gtk_drag_dest_set (button, GTK_DEST_DEFAULT_ALL, dest_drag_types,
				   G_N_ELEMENTS (dest_drag_types), GDK_ACTION_COPY);
	}
	else if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "map",
				  G_CALLBACK (menu_init_cb), action);
	}
}

void
ephy_topic_action_updated (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;
	GValue value = { 0, };
	const char *title;
	int priority;
	
	g_return_if_fail (priv->node != NULL);
	
	priority = ephy_node_get_property_int 
		(priv->node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
	
	if (priority == EPHY_NODE_ALL_PRIORITY)
	{
		title = _("Bookmarks");
	}
	else
	{
		title = ephy_node_get_property_string
			(priv->node, EPHY_NODE_KEYWORD_PROP_NAME);
	}
	
	g_value_init(&value, G_TYPE_STRING);
	g_value_set_static_string (&value, title);
	g_object_set_property (G_OBJECT (action), "label", &value);
	g_object_set_property (G_OBJECT (action), "tooltip", &value);
	g_value_unset (&value);
}

EphyNode *
ephy_topic_action_get_topic (EphyTopicAction *action)
{
	EphyTopicActionPrivate *priv = action->priv;

	return priv->node;
}

void
ephy_topic_action_set_topic (EphyTopicAction *action,
			     EphyNode *node)
{
	EphyTopicActionPrivate *priv = action->priv;
	GObject *object = G_OBJECT (action);

	g_return_if_fail (node != NULL);
	
	if (priv->node == node) return;

	if (priv->node != NULL)
	{
		ephy_node_signal_disconnect_object
			(priv->node, EPHY_NODE_CHILD_ADDED,
			 (EphyNodeCallback) child_added_cb, object);
		ephy_node_signal_disconnect_object
			(priv->node, EPHY_NODE_CHILD_CHANGED,
			 (EphyNodeCallback)child_changed_cb, object);
		ephy_node_signal_disconnect_object
			(priv->node, EPHY_NODE_CHILD_REMOVED,
			 (EphyNodeCallback)child_removed_cb, object);
	}

	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) child_added_cb,
					 object);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback) child_changed_cb,
					 object);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) child_removed_cb,
					 object);

	priv->node = node;
	
	erase_popup (action);
	
	g_object_freeze_notify (object);
	g_object_notify (object, "topic");
	ephy_topic_action_updated (action);
	g_object_thaw_notify (object);
}

static void
ephy_topic_action_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);
	EphyTopicActionPrivate *priv = action->priv;

	switch (prop_id)
	{
		case PROP_TOPIC:
			ephy_topic_action_set_topic (action, g_value_get_pointer (value));
			break;
		case PROP_MANAGER:
			priv->manager = g_value_get_object (value);
			break;
	}
}

static void
ephy_topic_action_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EphyTopicAction *action = EPHY_TOPIC_ACTION (object);
	EphyTopicActionPrivate *priv = action->priv;

	switch (prop_id)
	{
		case PROP_TOPIC:
			g_value_set_pointer (value, priv->node);
			break;
		case PROP_MANAGER:
			g_value_set_object (value, priv->manager);
			break;
	}
}

static void
ephy_topic_action_init (EphyTopicAction *action)
{
	action->priv = EPHY_TOPIC_ACTION_GET_PRIVATE (action);
}

static void
ephy_topic_action_class_init (EphyTopicActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	object_class->set_property = ephy_topic_action_set_property;
	object_class->get_property = ephy_topic_action_get_property;

	g_object_class_install_property (object_class,
					 PROP_TOPIC,
					 g_param_spec_pointer ("topic",
							       "Topic",
							       "Topic",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_MANAGER,
					 g_param_spec_object ("manager",
							      "Manager",
							      "UI Manager",
							      GTK_TYPE_UI_MANAGER,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));
	
	g_type_class_add_private (object_class, sizeof(EphyTopicActionPrivate));
}

GtkAction *
ephy_topic_action_new (EphyNode *node,
		       GtkUIManager *manager,
		       const char *name)
{
	g_assert (name != NULL);

	return GTK_ACTION (g_object_new (EPHY_TYPE_TOPIC_ACTION,
					 "name", name,
					 "topic", node,
					 "manager", manager,
					 NULL));
}
