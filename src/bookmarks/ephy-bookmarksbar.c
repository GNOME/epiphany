/*
 *  Copyright (C) 2001, 2002 Jorn Baayen
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003-2004 Christian Persch
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

#include "ephy-bookmarksbar.h"
#include "ephy-bookmarksbar-model.h"
#include "ephy-bookmarks.h"
#include "ephy-shell.h"
#include "ephy-topic-action.h"
#include "ephy-bookmark-action.h"
#include "ephy-new-bookmark.h"
#include "ephy-stock-icons.h"
#include "ephy-dnd.h"
#include "ephy-debug.h"

#include <gtk/gtkuimanager.h>
#include <gtk/gtktoolbar.h>
#include <glib/gi18n.h>
#include <string.h>

static GtkTargetEntry drag_targets[] =
{
	{ EPHY_DND_TOPIC_TYPE,		0,	0 },
	{ EPHY_DND_URL_TYPE,		0,	1 }
};
static int n_drag_targets = G_N_ELEMENTS (drag_targets);

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_WINDOW
};

static GObjectClass *parent_class = NULL;

#define EPHY_BOOKMARKSBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKSBAR, EphyBookmarksBarPrivate))

struct EphyBookmarksBarPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
	EphyBookmarks *bookmarks;
	EggToolbarsModel *toolbars_model;
};

static void ephy_bookmarksbar_class_init	(EphyBookmarksBarClass *klass);
static void ephy_bookmarksbar_init		(EphyBookmarksBar *toolbar);

GType
ephy_bookmarksbar_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyBookmarksBarClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_bookmarksbar_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyBookmarksBar),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_bookmarksbar_init
		};

		type = g_type_register_static (EGG_TYPE_EDITABLE_TOOLBAR,
					       "EphyBookmarksBar",
					       &our_info, 0);
	}

	return type;
}

static void
go_location_cb (GtkAction *action,
		char *location,
		EphyBookmarksBar *toolbar)
{
	EphyWindow *window = toolbar->priv->window;
	GdkEvent *event;
	gboolean new_tab = FALSE;

	g_return_if_fail (window != NULL);

	event = gtk_get_current_event ();
	if (event != NULL)
	{
		if (event->type == GDK_BUTTON_RELEASE)
		{
			guint modifiers, button, state;

			modifiers = gtk_accelerator_get_default_mod_mask ();
			button = event->button.button;
			state = event->button.state;

			/* middle-click or control-click */
			if ((button == 1 && ((state & modifiers) == GDK_CONTROL_MASK)) ||
			    (button == 2))
			{
				new_tab = TRUE;
			}
		}

		gdk_event_free (event);
	}

	if (new_tab)
	{
		ephy_shell_new_tab (ephy_shell, window,
				    ephy_window_get_active_tab (window),
				    location,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}
	else
	{
		ephy_window_load_url (window, location);
	}
}

static gboolean
remove_action_from_model (EggToolbarsModel *model, const char *name)
{
	int n_toolbars, n_items, t, i;

	n_toolbars = egg_toolbars_model_n_toolbars (model);

	for (t = 0; t < n_toolbars; t++)
	{
		n_items = egg_toolbars_model_n_items (model, t);

		for (i = 0; i < n_items; i++)
		{
			const char *i_name;
			gboolean is_separator;

			egg_toolbars_model_item_nth (model, t , i, &is_separator,
						     &i_name, NULL);
			g_return_val_if_fail (i_name != NULL, FALSE);

			if (strcmp (i_name, name) == 0)
			{
				egg_toolbars_model_remove_item (model, t, i);
				
				if (!remove_action_from_model (model, name))
				{
					return FALSE;
				}
			}
		}
	}

	return FALSE;
}

static void
bookmark_destroy_cb (EphyNode *node,
		     EphyBookmarksBar *toolbar)
{
	EggToolbarsModel *model;
	GtkAction *action;
	char *name;
	long id;
	

	model = toolbar->priv->toolbars_model;
	id = ephy_node_get_id (node);
	name = ephy_bookmarksbar_model_get_action_name
				(EPHY_BOOKMARKSBAR_MODEL (model), id);
	remove_action_from_model (model, name);

	model = EGG_TOOLBARS_MODEL (ephy_shell_get_toolbars_model
						(ephy_shell, FALSE));
	remove_action_from_model (model, name);

	action = gtk_action_group_get_action (toolbar->priv->action_group, name);
	if (action)
	{
		gtk_action_group_remove_action (toolbar->priv->action_group, action);
	}

	g_free (name);
}

static void
ephy_bookmarksbar_action_request (EggEditableToolbar *eggtoolbar,
				  const char *name)
{
	EphyBookmarksBar *toolbar = EPHY_BOOKMARKSBAR (eggtoolbar);
	GtkAction *action = NULL;
	EphyNode *bmks, *topics;

	bmks = ephy_bookmarks_get_bookmarks (toolbar->priv->bookmarks);
	topics = ephy_bookmarks_get_keywords (toolbar->priv->bookmarks);

	LOG ("Action request for action '%s'", name)

	if (g_str_has_prefix (name, "GoBookmark-"))
	{
		EphyNode *node;

		node = ephy_bookmarksbar_model_get_node
			(EPHY_BOOKMARKSBAR_MODEL (toolbar->priv->toolbars_model),
			 name);
		g_return_if_fail (node != NULL);

		if (ephy_node_has_child (topics, node))
		{
			action = ephy_topic_action_new (name, ephy_node_get_id (node));
		}
		else if (ephy_node_has_child (bmks, node))
		{
			action = ephy_bookmark_action_new (name, ephy_node_get_id (node));
		}

		g_return_if_fail (action != NULL);

		g_signal_connect (action, "go_location",
				  G_CALLBACK (go_location_cb), toolbar);
		gtk_action_group_add_action (toolbar->priv->action_group, action);
		g_object_unref (action);

		ephy_node_signal_connect_object (node,
						 EPHY_NODE_DESTROY,
						 (EphyNodeCallback) bookmark_destroy_cb,
						 G_OBJECT (toolbar));
	}

	if (EGG_EDITABLE_TOOLBAR_CLASS (parent_class)->action_request)
	{
		EGG_EDITABLE_TOOLBAR_CLASS (parent_class)->action_request (eggtoolbar, name);
	}
}

static void
toolbar_added_cb (EggToolbarsModel *model,
		  int position,
		  EggEditableToolbar *toolbar)
{
	const char *t_name;

	t_name = egg_toolbars_model_toolbar_nth (model, position);
	g_return_if_fail (t_name != NULL);

	egg_editable_toolbar_set_drag_dest
		(toolbar, drag_targets, n_drag_targets, t_name);
}

static void
ephy_bookmarksbar_set_window (EphyBookmarksBar *toolbar,
				   EphyWindow *window)
{
	EggToolbarsModel *model = toolbar->priv->toolbars_model;
	GtkUIManager *manager = GTK_UI_MANAGER (window->ui_merge);

	g_return_if_fail (toolbar->priv->window == NULL);
	g_return_if_fail (model != NULL);

	toolbar->priv->window = window;

	toolbar->priv->action_group =
		gtk_action_group_new ("BookmarksToolbarActions");

	gtk_ui_manager_insert_action_group (manager,
					    toolbar->priv->action_group, -1);

	g_object_set (G_OBJECT (toolbar),
		      "MenuMerge", manager,
		      "ToolbarsModel", model,
		      NULL);
}

static void
ephy_bookmarksbar_realize (GtkWidget *widget)
{
	EggEditableToolbar *eggtoolbar = EGG_EDITABLE_TOOLBAR (widget);
	EphyBookmarksBar *toolbar = EPHY_BOOKMARKSBAR (widget);
	EggToolbarsModel *model = toolbar->priv->toolbars_model;
	int i, n_toolbars;

	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	g_signal_connect (model, "toolbar_added",
			  G_CALLBACK (toolbar_added_cb), toolbar);

	/* now that the toolbar has been constructed, set drag dests */
	n_toolbars = egg_toolbars_model_n_toolbars (model);
	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth (model, i);
		g_return_if_fail (t_name != NULL);

		egg_editable_toolbar_set_drag_dest
			(eggtoolbar, drag_targets, n_drag_targets, t_name);
	}	
}

static void
ephy_bookmarksbar_unrealize (GtkWidget *widget)
{
	EphyBookmarksBar *toolbar = EPHY_BOOKMARKSBAR (widget);
	EggToolbarsModel *model = toolbar->priv->toolbars_model;

	g_signal_handlers_disconnect_by_func
		(model, G_CALLBACK (toolbar_added_cb), toolbar);

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
ephy_bookmarksbar_init (EphyBookmarksBar *toolbar)
{
	toolbar->priv = EPHY_BOOKMARKSBAR_GET_PRIVATE (toolbar);
}

static void
ephy_bookmarksbar_finalize (GObject *object)
{
	EphyBookmarksBar *toolbar = EPHY_BOOKMARKSBAR (object);

	g_object_unref (toolbar->priv->action_group);

	g_signal_handlers_disconnect_by_func
		(toolbar->priv->toolbars_model,
		 G_CALLBACK (toolbar_added_cb),
		 toolbar);

	LOG ("EphyBookmarksBar %p finalised", object)

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmarksbar_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	EphyBookmarksBar *toolbar = EPHY_BOOKMARKSBAR (object);

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			toolbar->priv->bookmarks = g_value_get_object (value);
			toolbar->priv->toolbars_model =
				ephy_bookmarks_get_toolbars_model (toolbar->priv->bookmarks);
			break;
		case PROP_WINDOW:
			ephy_bookmarksbar_set_window (toolbar, g_value_get_object (value));
			break;
	}
}

static void
ephy_bookmarksbar_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_bookmarksbar_class_init (EphyBookmarksBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	EggEditableToolbarClass *eet_class = EGG_EDITABLE_TOOLBAR_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_bookmarksbar_finalize;
	object_class->set_property = ephy_bookmarksbar_set_property;
	object_class->get_property = ephy_bookmarksbar_get_property;

	widget_class->realize = ephy_bookmarksbar_realize;
	widget_class->unrealize = ephy_bookmarksbar_unrealize;

	eet_class->action_request = ephy_bookmarksbar_action_request;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks",
							      "Bookmarks Model",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyBookmarksBarPrivate));
}

GtkWidget *
ephy_bookmarksbar_new (EphyWindow *window)
{
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	return GTK_WIDGET (g_object_new (EPHY_TYPE_BOOKMARKSBAR,
					 "bookmarks", bookmarks,
					 "window", window,
					 NULL));
}
