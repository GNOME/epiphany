/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
 *  Copyright (C) 2001, 2002 Jorn Baayen
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
#include <config.h>
#endif

#include "toolbar.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-location-entry.h"
#include "ephy-dnd.h"
#include "ephy-spinner.h"
#include "ephy-spinner-action.h"
#include "ephy-location-action.h"
#include "ephy-favicon-action.h"
#include "ephy-topic-action.h"
#include "ephy-go-action.h"
#include "ephy-navigation-action.h"
#include "ephy-bookmark-action.h"
#include "ephy-zoom-action.h"
#include "window-commands.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "ephy-new-bookmark.h"
#include "ephy-stock-icons.h"
#include "ephy-toolbars-model.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtktoolbar.h>

static void toolbar_class_init (ToolbarClass *klass);
static void toolbar_init (Toolbar *t);
static void toolbar_finalize (GObject *object);
static void toolbar_set_window (Toolbar *t, EphyWindow *window);
static void
toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec);
static void
toolbar_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec);

static GtkTargetEntry drag_targets[] = {
  { EGG_TOOLBAR_ITEM_TYPE, 0, 0 },
  { EPHY_DND_TOPIC_TYPE, 0, 1 },
  { EPHY_DND_URL_TYPE, 0, 2 },
};
static int n_drag_targets = G_N_ELEMENTS (drag_targets);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static GObjectClass *parent_class = NULL;

#define EPHY_TOOLBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBAR, ToolbarPrivate))

struct ToolbarPrivate
{
	EphyWindow *window;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	gboolean visibility;
	gboolean can_set_location;
	GtkWidget *spinner;
	GtkWidget *favicon;
	GtkWidget *go;
};

GType
toolbar_get_type (void)
{
        static GType toolbar_type = 0;

        if (toolbar_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (ToolbarClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) toolbar_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (Toolbar),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) toolbar_init
                };

                toolbar_type = g_type_register_static (EGG_EDITABLE_TOOLBAR_TYPE,
						       "Toolbar",
						       &our_info, 0);
        }

        return toolbar_type;

}

static void
go_location_cb (GtkAction *action, char *location, EphyWindow *window)
{
	ephy_window_load_url (window, location);
}

static void
zoom_to_level_cb (GtkAction *action, float zoom, EphyWindow *window)
{
	ephy_window_set_zoom (window, zoom);
}

static void
bookmark_destroy_cb (EphyNode *node,
		     Toolbar *t)
{
	GtkAction *action;
	char *name;
	EphyToolbarsModel *model;
	long id;

	model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell));

	id = ephy_node_get_id (node);
	name = ephy_toolbars_model_get_action_name (model, id);
	action = gtk_action_group_get_action (t->priv->action_group, name);
	if (action)
	{
		gtk_action_group_remove_action (t->priv->action_group, action);
	}

	g_free (name);
}

static void
toolbar_ensure_action (Toolbar *t,
		       const char *name)
{
	GtkAction *action = NULL;
	EphyToolbarsModel *model;
	EphyBookmarks *bookmarks;
	EphyNode *bmks, *topics;

	model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell));
	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	bmks = ephy_bookmarks_get_bookmarks (bookmarks);
	topics = ephy_bookmarks_get_keywords (bookmarks);

	LOG ("Ensure action %s", name)

	if (g_str_has_prefix (name, "GoBookmark-"))
	{
		EphyNode *node;

		node = ephy_toolbars_model_get_node (model,name);
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
				  G_CALLBACK (go_location_cb), t->priv->window);
		gtk_action_group_add_action (t->priv->action_group, action);
		g_object_unref (action);

		ephy_node_signal_connect_object (node,
					         EPHY_NODE_DESTROY,
					         (EphyNodeCallback) bookmark_destroy_cb,
					         G_OBJECT (t));
	}
}

static void
toolbar_class_init (ToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = toolbar_finalize;
	object_class->set_property = toolbar_set_property;
	object_class->get_property = toolbar_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(ToolbarPrivate));
}

static void
toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        Toolbar *t = EPHY_TOOLBAR (object);

        switch (prop_id)
        {
		case PROP_EPHY_WINDOW:
		toolbar_set_window (t, g_value_get_object (value));
		break;
        }
}

static void
toolbar_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
        Toolbar *t = EPHY_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, t->priv->window);
                        break;
        }
}

static void
toolbar_setup_actions (Toolbar *t)
{
	GtkAction *action;

	t->priv->action_group = gtk_action_group_new ("SpecialToolbarActions");

	action = g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			       "name", "NavigationBack",
			       "label", _("Back"),
			       "stock_id", GTK_STOCK_GO_BACK,
			       "tooltip", _("Go back"),
			       "window", t->priv->window,
			       "direction", EPHY_NAVIGATION_DIRECTION_BACK,
			       "is_important", TRUE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_go_back), t->priv->window);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			       "name", "NavigationForward",
			       "label", _("Forward"),
			       "stock_id", GTK_STOCK_GO_FORWARD,
			       "tooltip", _("Go forward"),
			       "window", t->priv->window,
			       "direction", EPHY_NAVIGATION_DIRECTION_FORWARD,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_go_forward), t->priv->window);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			       "name", "NavigationUp",
			       "label", _("Up"),
			       "stock_id", GTK_STOCK_GO_UP,
			       "tooltip", _("Go up"),
			       "window", t->priv->window,
			       "direction", EPHY_NAVIGATION_DIRECTION_UP,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_go_up), t->priv->window);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_SPINNER_ACTION,
			       "name", "Spinner",
			       "label", _("Spinner"),
			       NULL);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	/* FIXME: I'm still waiting for the exact term to 
	 * user here from the docs team.
	 */
	action = g_object_new (EPHY_TYPE_LOCATION_ACTION,
			       "name", "Location",
			       "label", _("Address Entry"),
			       "stock_id", EPHY_STOCK_ENTRY,
			       "tooltip", _("Enter a web address to open, or a phrase to search for on the web"),
			       NULL);
	g_signal_connect (action, "go_location",
			  G_CALLBACK (go_location_cb), t->priv->window);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_ZOOM_ACTION,
			       "name", "Zoom",
			       "label", _("Zoom"),
			       "tooltip", _("Adjust the text size"),
			       "zoom", 1.0,
			       NULL);
	g_signal_connect (action, "zoom_to_level",
			  G_CALLBACK (zoom_to_level_cb), t->priv->window);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_FAVICON_ACTION,
			       "name", "Favicon",
			       "label", _("Favicon"),
			       "window", t->priv->window,
			       NULL);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_GO_ACTION,
			       "name", "ToolbarGo",
			       "label", _("Go"),
			       "stock_id", GTK_STOCK_JUMP_TO,
			       "tooltip", _("Go to the address entered in the address entry"),
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_load_location), t->priv->window);
	gtk_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);
}

static void
action_request_cb (EggEditableToolbar *etoolbar,
		   char *action_name,
		   gpointer data)
{
	toolbar_ensure_action (EPHY_TOOLBAR (etoolbar), action_name);
}

static void
init_bookmarks_toolbar (Toolbar *t)
{
	EphyToolbarsModel *model;
	int i, n_toolbars;

	model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell));
	n_toolbars = egg_toolbars_model_n_toolbars
		(EGG_TOOLBARS_MODEL (model));

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth
			(EGG_TOOLBARS_MODEL (model), i);
		g_return_if_fail (t_name != NULL);

		if (strcmp (t_name, "BookmarksBar") == 0)
		{
			egg_editable_toolbar_set_drag_dest
				(EGG_EDITABLE_TOOLBAR (t),
				 drag_targets, n_drag_targets,
				 t_name);
			egg_toolbars_model_set_flags
				(EGG_TOOLBARS_MODEL (model),
				 EGG_TB_MODEL_NOT_REMOVABLE, i);
		}
	}
}

static void
update_toolbar_remove_flag (EphyToolbarsModel *model, gpointer data)
{
	int i, n_toolbars;
	int not_removable = 0;

	n_toolbars = egg_toolbars_model_n_toolbars
		(EGG_TOOLBARS_MODEL (model));

	/* If there is only one toolbar and the bookmarks bar */
	if (n_toolbars <= 2)
	{
		not_removable = EGG_TB_MODEL_NOT_REMOVABLE;
	}

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth
			(EGG_TOOLBARS_MODEL (model), i);
		g_return_if_fail (t_name != NULL);

		if (!(strcmp (t_name, "BookmarksBar") == 0))
		{
			egg_toolbars_model_set_flags
				(EGG_TOOLBARS_MODEL (model),
				 not_removable, i);
		}
	}
}

static GtkWidget *
get_location_entry (Toolbar *t)
{
	GtkAction *action;
	GtkWidget *location;

	action = gtk_action_group_get_action
		(t->priv->action_group, "Location");
	location = ephy_location_action_get_widget
		(EPHY_LOCATION_ACTION (action));

	return location;
}

static void
location_user_changed_cb (GtkWidget *entry, Toolbar *t)
{
	EphyTab *tab;
	char *address;

	tab = ephy_window_get_active_tab (t->priv->window);
	g_return_if_fail (EPHY_IS_TAB (tab));

	t->priv->can_set_location = FALSE;
	address = ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (entry));
	ephy_tab_set_location (tab, address, TAB_ADDRESS_EXPIRE_CURRENT);
	g_free (address);
	t->priv->can_set_location = TRUE;
}

static void
toolbar_set_window (Toolbar *t, EphyWindow *window)
{
	EphyToolbarsModel *model;

	g_return_if_fail (t->priv->window == NULL);

	t->priv->window = window;
	t->priv->ui_merge = GTK_UI_MANAGER (window->ui_merge);

	toolbar_setup_actions (t);
	gtk_ui_manager_insert_action_group (t->priv->ui_merge,
					    t->priv->action_group, 1);

	g_signal_connect (t, "action_request",
			  G_CALLBACK (action_request_cb),
			  NULL);

	model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell));
	g_signal_connect (EGG_TOOLBARS_MODEL (model), "toolbar_added",
			  G_CALLBACK (update_toolbar_remove_flag),
			  NULL);
	g_signal_connect (EGG_TOOLBARS_MODEL (model), "toolbar_removed",
			  G_CALLBACK (update_toolbar_remove_flag),
			  NULL);
	g_object_set (G_OBJECT (t),
		      "ToolbarsModel", model,
		      "MenuMerge", t->priv->ui_merge,
		      NULL);
	init_bookmarks_toolbar (t);

	g_signal_connect_object (get_location_entry (t), "user_changed",
			         G_CALLBACK (location_user_changed_cb),
			         t, 0);
}

static void
toolbar_init (Toolbar *t)
{
	t->priv = EPHY_TOOLBAR_GET_PRIVATE (t);

	t->priv->window = NULL;
	t->priv->ui_merge = NULL;
	t->priv->visibility = TRUE;
	t->priv->can_set_location = TRUE;
}

static void
toolbar_finalize (GObject *object)
{
	Toolbar *t;
	ToolbarPrivate *p;
	GtkUIManager *merge;

	t = EPHY_TOOLBAR (object);
	p = t->priv;
	merge = GTK_UI_MANAGER (t->priv->window->ui_merge);

	/* FIXME: why not at the end? */
	G_OBJECT_CLASS (parent_class)->finalize (object);

	g_object_unref (t->priv->action_group);

	LOG ("Toolbar finalized")
}

Toolbar *
toolbar_new (EphyWindow *window)
{
	Toolbar *t;

	t = EPHY_TOOLBAR (g_object_new (EPHY_TYPE_TOOLBAR,
					"EphyWindow", window,
					NULL));

	return t;
}

static void
location_finished_cb (GtkWidget *location, GtkWidget *toolbar)
{
	gtk_widget_hide (toolbar);

	g_signal_handlers_disconnect_by_func (G_OBJECT (location),
                                              G_CALLBACK (location_finished_cb),
                                              toolbar);
}

void
toolbar_activate_location (Toolbar *t)
{
	GtkWidget *location;
	GtkWidget *location_tb;

	location = get_location_entry (t);
	g_return_if_fail (location != NULL);

	location_tb = gtk_widget_get_ancestor (location, GTK_TYPE_TOOLBAR);
	g_return_if_fail (location_tb != NULL);

	if (!GTK_WIDGET_VISIBLE (location_tb))
	{
		g_signal_connect (location, "finished",
				  G_CALLBACK (location_finished_cb), location_tb);
		gtk_widget_show (location_tb);
	}

	ephy_location_entry_activate
		(EPHY_LOCATION_ENTRY(location));
}

void
toolbar_spinner_start (Toolbar *t)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Spinner");
	g_object_set (action, "throbbing", TRUE, NULL);
}

void
toolbar_spinner_stop (Toolbar *t)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Spinner");
	g_object_set (action, "throbbing", FALSE, NULL);
}

void
toolbar_set_location (Toolbar *t,
		      const char *alocation)
{
	GtkWidget *location;

	if (t->priv->can_set_location)
	{
		location = get_location_entry (t);
		g_return_if_fail (location != NULL);

		ephy_location_entry_set_location
			(EPHY_LOCATION_ENTRY (location),
			 alocation ? alocation : "");
	}
}

void
toolbar_update_favicon (Toolbar *t)
{
	EphyTab *tab;
	const char *url;
	GtkActionGroup *action_group;
	GtkAction *action;

	tab = ephy_window_get_active_tab (t->priv->window);
	url = ephy_tab_get_icon_address (tab);
	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Favicon");
	g_object_set (action, "icon", url, NULL);
}

char *
toolbar_get_location (Toolbar *t)
{
	GtkWidget *location;

	location = get_location_entry (t);
	g_return_val_if_fail (location != NULL, NULL);

	return ephy_location_entry_get_location
		(EPHY_LOCATION_ENTRY (location));
}

void
toolbar_clear_location_history (Toolbar *t)
{
	GtkWidget *location;

	location = get_location_entry (t),
	g_return_if_fail (location != NULL);

	ephy_location_entry_clear_history (EPHY_LOCATION_ENTRY (location));
}

void
toolbar_update_navigation_actions (Toolbar *t, gboolean back, gboolean forward, gboolean up)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "NavigationBack");
	g_object_set (action, "sensitive", back, NULL);
	action = gtk_action_group_get_action (action_group, "NavigationForward");
	g_object_set (action, "sensitive", forward, NULL);
	action = gtk_action_group_get_action (action_group, "NavigationUp");
	g_object_set (action, "sensitive", up, NULL);
}

void
toolbar_set_visibility (Toolbar *t,
			gboolean normal_toolbars,
			gboolean bmk_toolbars)
{
	EphyToolbarsModel *model;
	int i, n_toolbars;

	model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell));
	n_toolbars = egg_toolbars_model_n_toolbars
		(EGG_TOOLBARS_MODEL (model));

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth
			(EGG_TOOLBARS_MODEL (model), i);
		g_return_if_fail (t_name != NULL);
		if (strcmp (t_name, "BookmarksBar") == 0)
		{
			if (bmk_toolbars)
			{
				egg_editable_toolbar_show
					(EGG_EDITABLE_TOOLBAR (t), t_name);
			}
			else
			{
				egg_editable_toolbar_hide
					(EGG_EDITABLE_TOOLBAR (t), t_name);
			}
		}
		else
		{
			if (normal_toolbars)
			{
				egg_editable_toolbar_show
					(EGG_EDITABLE_TOOLBAR (t), t_name);
			}
			else
			{
				egg_editable_toolbar_hide
					(EGG_EDITABLE_TOOLBAR (t), t_name);
			}
		}
	}
}

void
toolbar_update_zoom (Toolbar *t, float zoom)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Zoom");
	g_object_set (action, "zoom", zoom, NULL);
}
