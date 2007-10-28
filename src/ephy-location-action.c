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
 *  $Id$
 */

#include "config.h"

#include "ephy-location-action.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-completion-model.h"
#include "ephy-link.h"
#include "ephy-debug.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkentrycompletion.h>
#include <gtk/gtkmain.h>

#define EPHY_LOCATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_ACTION, EphyLocationActionPrivate))

struct _EphyLocationActionPrivate
{
	EphyWindow *window;
	GList *actions;
	char *address;
	char *typed_address;
	EphyNode *smart_bmks;
	EphyBookmarks *bookmarks;
	GdkPixbuf *icon;
	char *lock_stock_id;
	char *lock_tooltip;
	guint editable : 1;
	guint show_lock : 1;
	guint secure : 1;
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
	PROP_ADDRESS,
	PROP_EDITABLE,
	PROP_ICON,
	PROP_LOCK_STOCK,
	PROP_LOCK_TOOLTIP,
	PROP_SECURE,
	PROP_SHOW_LOCK,
	PROP_WINDOW
};

enum
{
	LOCK_CLICKED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
ephy_location_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo type_info =
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

		type = g_type_register_static (EPHY_TYPE_LINK_ACTION,
					       "EphyLocationAction",
					       &type_info, 0);
	}

	return type;
}

static void
action_activated_cb (GtkEntryCompletion *completion,
                     gint index,
		     EphyLocationAction *action)
{
	GtkWidget *entry;
	char *content;

	entry = gtk_entry_completion_get_entry (completion);
	content = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (content)
	{
		EphyNode *node;
		const char *smart_url;
		char *url;

		node = (EphyNode *)g_list_nth_data (action->priv->actions, index);
		smart_url = ephy_node_get_property_string
	                (node, EPHY_NODE_BMK_PROP_LOCATION);
		g_return_if_fail (smart_url != NULL);

		url = ephy_bookmarks_resolve_address
			(action->priv->bookmarks, smart_url, content);
		g_free (content);
		if (url == NULL) return;

 		ephy_link_open (EPHY_LINK (action), url, NULL,
			        ephy_link_flags_from_current_event () | EPHY_LINK_ALLOW_FIXUP);

		g_free (url);
	}
}

static void
entry_activate_cb (GtkEntry *entry,
		   EphyLocationAction *action)
{
	EphyBookmarks *bookmarks;
	const char *content;
	char *address;

	content = gtk_entry_get_text (entry);
	if (content == NULL || content[0] == '\0') return;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

	address = ephy_bookmarks_resolve_address (bookmarks, content, NULL);
	g_return_if_fail (address != NULL);

	ephy_link_open (EPHY_LINK (action), address, NULL, 
		        ephy_link_flags_from_current_event () | EPHY_LINK_ALLOW_FIXUP);

	g_free (address);
}

static void
user_changed_cb (GtkWidget *proxy, EphyLocationAction *action)
{
	const char *address;

	address = ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (proxy));

	LOG ("user_changed_cb, new address %s", address);

	g_signal_handlers_block_by_func (action, G_CALLBACK (sync_address), proxy);
	ephy_location_action_set_address (action, address, NULL);
	g_signal_handlers_unblock_by_func (action, G_CALLBACK (sync_address), proxy);
}

static void
lock_clicked_cb (GtkWidget *proxy,
		 EphyLocationAction *action)
{
	g_signal_emit (action, signals[LOCK_CLICKED], 0);
}

static void
sync_address (GtkAction *gaction,
	      GParamSpec *pspec,
	      GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationActionPrivate *priv = action->priv;
	EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (proxy);

	LOG ("sync_address %s", action->priv->address);

	g_signal_handlers_block_by_func (proxy, G_CALLBACK (user_changed_cb), action);
	ephy_location_entry_set_location (lentry, priv->address,
					  priv->typed_address);
	g_signal_handlers_unblock_by_func (proxy, G_CALLBACK (user_changed_cb), action);
}

static void
sync_editable (GtkAction *gaction,
	       GParamSpec *pspec,
	       GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (proxy);
	GtkWidget *entry;

	entry = ephy_location_entry_get_entry (lentry);
	gtk_editable_set_editable (GTK_EDITABLE (entry), action->priv->editable);
}

static void
sync_icon (GtkAction *gaction,
	   GParamSpec *pspec,
	   GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationActionPrivate *priv = action->priv;
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (proxy);

	ephy_location_entry_set_favicon (entry, priv->icon);
}

static void
sync_lock_stock_id (GtkAction *gaction,
		    GParamSpec *pspec,
		    GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationActionPrivate *priv = action->priv;
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (proxy);

	ephy_location_entry_set_lock_stock (entry, priv->lock_stock_id);
}

static void
sync_lock_tooltip (GtkAction *gaction,
		   GParamSpec *pspec,
		   GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationActionPrivate *priv = action->priv;
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (proxy);

	ephy_location_entry_set_lock_tooltip (entry, priv->lock_tooltip);
}

static void
sync_secure (GtkAction *gaction,
	     GParamSpec *pspec,
	     GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationActionPrivate *priv = action->priv;
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (proxy);

	ephy_location_entry_set_secure (entry, priv->secure);
}

static void
sync_show_lock (GtkAction *gaction,
	        GParamSpec *pspec,
	        GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationActionPrivate *priv = action->priv;
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (proxy);

	ephy_location_entry_set_show_lock (entry, priv->show_lock);
}

static char *
get_location_cb (EphyLocationEntry *entry,
		EphyLocationAction *action)
{
	EphyLocationActionPrivate *priv = action->priv;
	EphyEmbed *embed;
	
	embed = ephy_window_get_active_tab (priv->window);

	return g_strdup (ephy_embed_get_address (embed));
}

static char *
get_title_cb (EphyLocationEntry *entry,
	      EphyLocationAction *action)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_tab (action->priv->window);

	return g_strdup (ephy_embed_get_title (embed));
}

static void
remove_completion_actions (GtkAction *gaction,
			   GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (proxy);
	GtkWidget *entry;
	GtkEntryCompletion *completion;
	GList *l;

	entry = ephy_location_entry_get_entry (lentry);
	completion = gtk_entry_get_completion (GTK_ENTRY (entry));

	for (l = action->priv->actions; l != NULL; l = l->next)
	{
		gtk_entry_completion_delete_action (completion, 0);
	}

	g_signal_handlers_disconnect_by_func
			(completion, G_CALLBACK (action_activated_cb), action);
}

static void
add_completion_actions (GtkAction *gaction,
			GtkWidget *proxy)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (gaction);
	EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (proxy);
	GtkWidget *entry;
	GtkEntryCompletion *completion;
	GList *l;

	entry = ephy_location_entry_get_entry (lentry);
	completion = gtk_entry_get_completion (GTK_ENTRY (entry));

	for (l = action->priv->actions; l != NULL; l = l->next)
	{
		EphyNode *bmk = l->data;
		const char *title;
		int index;

		index = g_list_position (action->priv->actions, l);
		title = ephy_node_get_property_string
	                (bmk, EPHY_NODE_BMK_PROP_TITLE);
		gtk_entry_completion_insert_action_text (completion, index, (char*)title);
	}

	g_signal_connect (completion, "action_activated",
			  G_CALLBACK (action_activated_cb), action);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (proxy);
		EphyCompletionModel *model;
		GtkWidget *entry;

		model = ephy_completion_model_new ();
		ephy_location_entry_set_completion (EPHY_LOCATION_ENTRY (proxy),
						    GTK_TREE_MODEL (model),
						    EPHY_COMPLETION_TEXT_COL,
						    EPHY_COMPLETION_ACTION_COL,
						    EPHY_COMPLETION_KEYWORDS_COL,
						    EPHY_COMPLETION_RELEVANCE_COL,
						    EPHY_COMPLETION_EXTRA_COL,
						    EPHY_COMPLETION_FAVICON_COL,
						    EPHY_COMPLETION_URL_COL);

		add_completion_actions (action, proxy);

		sync_address (action, NULL, proxy);
		g_signal_connect_object (action, "notify::address",
					 G_CALLBACK (sync_address), proxy, 0);
		sync_editable (action, NULL, proxy);
		g_signal_connect_object (action, "notify::editable",
					 G_CALLBACK (sync_editable), proxy, 0);
		sync_icon (action, NULL, proxy);
		g_signal_connect_object (action, "notify::icon",
					 G_CALLBACK (sync_icon), proxy, 0);
		sync_lock_stock_id (action, NULL, proxy);
		g_signal_connect_object (action, "notify::lock-stock-id",
					 G_CALLBACK (sync_lock_stock_id), proxy, 0);
		sync_lock_tooltip (action, NULL, proxy);
		g_signal_connect_object (action, "notify::lock-tooltip",
					 G_CALLBACK (sync_lock_tooltip), proxy, 0);
		sync_secure (action, NULL, proxy);
		g_signal_connect_object (action, "notify::secure",
					 G_CALLBACK (sync_secure), proxy, 0);
		sync_show_lock (action, NULL, proxy);
		g_signal_connect_object (action, "notify::show-lock",
					 G_CALLBACK (sync_show_lock), proxy, 0);

		entry = ephy_location_entry_get_entry (lentry);
		g_signal_connect_object (entry, "activate",
					 G_CALLBACK (entry_activate_cb),
					 action, 0);
		g_signal_connect_object (proxy, "user-changed",
					 G_CALLBACK (user_changed_cb), action, 0);
		g_signal_connect_object (proxy, "lock-clicked",
					 G_CALLBACK (lock_clicked_cb), action, 0);
		g_signal_connect_object (proxy, "get-location",
					 G_CALLBACK (get_location_cb), action, 0);
		g_signal_connect_object (proxy, "get-title",
					 G_CALLBACK (get_title_cb), action, 0);
	}

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
}

static void
disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GTK_ACTION_CLASS (parent_class)->disconnect_proxy (action, proxy);

	if (EPHY_IS_LOCATION_ENTRY (proxy))
	{
		EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (proxy);
		GtkWidget *entry;

		entry = ephy_location_entry_get_entry (lentry);

		g_signal_handlers_disconnect_matched (action, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, proxy);
		g_signal_handlers_disconnect_matched (proxy, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, action);
		g_signal_handlers_disconnect_matched (entry, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, action);
	}
}

static void
ephy_location_action_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);
	EphyLocationActionPrivate *priv = action->priv;

	switch (prop_id)
	{
		case PROP_ADDRESS:
			ephy_location_action_set_address (action, g_value_get_string (value), NULL);
			break;
		case PROP_EDITABLE:
			priv->editable = g_value_get_boolean (value);
			break;
		case PROP_ICON:
			if (priv->icon != NULL)
			{
				g_object_unref (priv->icon);
			}
			priv->icon = GDK_PIXBUF (g_value_dup_object (value));
			break;
		case PROP_LOCK_STOCK:
			g_free (priv->lock_stock_id);
			priv->lock_stock_id = g_value_dup_string (value);
			break;
		case PROP_LOCK_TOOLTIP:
			g_free (priv->lock_tooltip);
			priv->lock_tooltip = g_value_dup_string (value);
			break;
		case PROP_SECURE:
			priv->secure = g_value_get_boolean (value);
			break;
		case PROP_SHOW_LOCK:
			priv->show_lock = g_value_get_boolean (value);
			break;
		case PROP_WINDOW:
			priv->window = EPHY_WINDOW (g_value_get_object (value));
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
	EphyLocationActionPrivate *priv = action->priv;

	switch (prop_id)
	{
		case PROP_ADDRESS:
			g_value_set_string (value, ephy_location_action_get_address (action));
			break;
		case PROP_EDITABLE:
			g_value_set_boolean (value, priv->editable);
			break;
		case PROP_ICON:
			g_value_set_object (value, priv->icon);
			break;
		case PROP_LOCK_STOCK:
			g_value_set_string (value, priv->lock_stock_id);
			break;
		case PROP_LOCK_TOOLTIP:
			g_value_set_string (value, priv->lock_tooltip);
			break;
		case PROP_SECURE:
			g_value_set_boolean (value, priv->secure);
			break;
		case PROP_SHOW_LOCK:
			g_value_set_boolean (value, priv->show_lock);
			break;
		case PROP_WINDOW:
			/* not readable */
			break;
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

	signals[LOCK_CLICKED] = g_signal_new (
		"lock-clicked",
		EPHY_TYPE_LOCATION_ACTION,
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationActionClass, lock_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);

	g_object_class_install_property (object_class,
					 PROP_ADDRESS,
					 g_param_spec_string ("address",
							      "Address",
							      "The address",
							      "",
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	g_object_class_install_property (object_class,
					 PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       "Editable",
							       "Editable",
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_object ("icon",
							      "Icon",
							      "The icon",
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_LOCK_STOCK,
					 g_param_spec_string  ("lock-stock-id",
							       "Lock Stock ID",
							       "Lock Stock ID",
							       NULL,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_LOCK_TOOLTIP,
					 g_param_spec_string  ("lock-tooltip",
							       "Lock Tooltip",
							       "The icon",
							       NULL,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_SECURE,
					 g_param_spec_boolean ("secure",
							       "Secure",
							       "Secure",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_SHOW_LOCK,
					 g_param_spec_boolean ("show-lock",
							       "Show Lock",
							       "Show Lock",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The navigation window",
							      G_TYPE_OBJECT,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyLocationActionPrivate));
}

static int
compare_actions (gconstpointer a,
		 gconstpointer b)
{
        EphyNode *node_a = (EphyNode *)a;
        EphyNode *node_b = (EphyNode *)b;
	const char *title1, *title2;
	int retval;

	title1 = ephy_node_get_property_string (node_a, EPHY_NODE_BMK_PROP_TITLE);
	title2 = ephy_node_get_property_string (node_b, EPHY_NODE_BMK_PROP_TITLE);

	if (title1 == NULL)
	{
		retval = -1;
	}
	else if (title2 == NULL)
	{
		retval = 1;
	}
	else
	{
		char *str_a, *str_b;

		str_a = g_utf8_casefold (title1, -1);
		str_b = g_utf8_casefold (title2, -1);
		retval = g_utf8_collate (str_a, str_b);
		g_free (str_a);
		g_free (str_b);
	}

	return retval;
}

static void
init_actions_list (EphyLocationAction *action)
{
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (action->priv->smart_bmks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		action->priv->actions = g_list_prepend
			(action->priv->actions, kid);
	}

	action->priv->actions =
		g_list_sort (action->priv->actions, (GCompareFunc) compare_actions);
}

static void
update_actions_list (EphyLocationAction *la)
{
	GSList *l;
	GtkAction *action = GTK_ACTION (la);

	l = gtk_action_get_proxies (action);
	for (; l != NULL; l = l->next)
	{
		remove_completion_actions (action, GTK_WIDGET (l->data));
	}

	g_list_free (la->priv->actions);
	la->priv->actions = NULL;
	init_actions_list (la);

	l = gtk_action_get_proxies (action);
	for (; l != NULL; l = l->next)
	{
		add_completion_actions (action, l->data);
	}
}

static void
actions_child_removed_cb (EphyNode *node,
		          EphyNode *child,
		          guint old_index,
		          EphyLocationAction *action)
{
	update_actions_list (action);
}

static void
actions_child_added_cb (EphyNode *node,
		        EphyNode *child,
		        EphyLocationAction *action)
{
	update_actions_list (action);
}

static void
actions_child_changed_cb (EphyNode *node,
		          EphyNode *child,
			  guint property_id,
		          EphyLocationAction *action)
{
	update_actions_list (action);
}

static void
ephy_location_action_init (EphyLocationAction *action)
{
	EphyLocationActionPrivate *priv;

	priv = action->priv = EPHY_LOCATION_ACTION_GET_PRIVATE (action);

	priv->address = g_strdup ("");
	priv->editable = TRUE;
	priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	priv->smart_bmks = ephy_bookmarks_get_smart_bookmarks
		(action->priv->bookmarks);

	init_actions_list (action);

	ephy_node_signal_connect_object (priv->smart_bmks,
			                 EPHY_NODE_CHILD_ADDED,
			                 (EphyNodeCallback)actions_child_added_cb,
			                 G_OBJECT (action));
	ephy_node_signal_connect_object (priv->smart_bmks,
			                 EPHY_NODE_CHILD_REMOVED,
			                 (EphyNodeCallback)actions_child_removed_cb,
			                 G_OBJECT (action));
	ephy_node_signal_connect_object (priv->smart_bmks,
			                 EPHY_NODE_CHILD_CHANGED,
			                 (EphyNodeCallback)actions_child_changed_cb,
			                 G_OBJECT (action));
}

static void
ephy_location_action_finalize (GObject *object)
{
	EphyLocationAction *action = EPHY_LOCATION_ACTION (object);
	EphyLocationActionPrivate *priv = action->priv;

	if (priv->icon != NULL)
	{
		g_object_unref (priv->icon);
	}

	g_list_free (priv->actions);
	g_free (priv->address);
	g_free (priv->typed_address);
	g_free (priv->lock_stock_id);
	g_free (priv->lock_tooltip);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

const char *
ephy_location_action_get_address (EphyLocationAction *action)
{
	g_return_val_if_fail (EPHY_IS_LOCATION_ACTION (action), "");

	return action->priv->address;
}

void
ephy_location_action_set_address (EphyLocationAction *action,
				  const char *address,
				  const char *typed_address)
{
	EphyLocationActionPrivate *priv;

	g_return_if_fail (EPHY_IS_LOCATION_ACTION (action));

	priv = action->priv;

	LOG ("set_address %s", address);

	g_free (priv->address);
	priv->address = g_strdup (address);

	g_free (priv->typed_address);
	priv->typed_address = typed_address ? g_strdup (typed_address) : NULL;

	g_object_notify (G_OBJECT (action), "address");
}
