/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2001, 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "toolbar.h"
#include "ephy-favicon-action.h"
#include "ephy-go-action.h"
#include "ephy-location-entry.h"
#include "ephy-location-action.h"
#include "ephy-navigation-action.h"
#include "ephy-spinner.h"
#include "ephy-dnd.h"
#include "ephy-topic-action.h"
#include "ephy-zoom-action.h"
#include "ephy-shell.h"
#include "ephy-stock-icons.h"
#include "window-commands.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtkuimanager.h>

static void toolbar_class_init (ToolbarClass *klass);
static void toolbar_init (Toolbar *t);
static void toolbar_finalize (GObject *object);
static void toolbar_set_window (Toolbar *t, EphyWindow *window);

static GtkTargetEntry drag_targets[] =
{
	{ EGG_TOOLBAR_ITEM_TYPE,	0,	0 },
	{ EPHY_DND_TOPIC_TYPE,		0,	1 },
	{ EPHY_DND_URL_TYPE,		0,	2 }
};
static int n_drag_targets = G_N_ELEMENTS (drag_targets);

enum
{
	PROP_0,
	PROP_WINDOW
};

static GObjectClass *parent_class = NULL;

#define CONF_LOCKDOWN_DISABLE_ARBITRARY_URL  "/apps/epiphany/lockdown/disable_arbitrary_url"

#define EPHY_TOOLBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBAR, ToolbarPrivate))

struct ToolbarPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
	gboolean updating_address;
	GtkWidget *spinner;
	guint disable_arbitrary_url_notifier_id;
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

                toolbar_type = g_type_register_static (EGG_TYPE_EDITABLE_TOOLBAR,
						       "Toolbar",
						       &our_info, 0);
        }

        return toolbar_type;

}

static void
update_location_editable (Toolbar *t)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean editable;

	editable = !eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL);

	/* Restore the real web page address when disabling entry */
	if (!editable)
	{
		EphyEmbed *embed;
		char *address;

		embed = ephy_window_get_active_embed (t->priv->window);
		if (EPHY_IS_EMBED (embed))
		{
			address = ephy_embed_get_location (embed, TRUE);
			toolbar_set_location (t, address);
			g_free (address);
		}
	}

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Location");
	g_object_set (G_OBJECT (action), "editable", editable, NULL);
}

static void
arbitrary_url_notifier (GConfClient *client,
		        guint cnxn_id,
		        GConfEntry *entry,
		        Toolbar *t)
{
	update_location_editable (t);
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
toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        Toolbar *t = EPHY_TOOLBAR (object);

        switch (prop_id)
        {
		case PROP_WINDOW:
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
	/* no readable properties */
	g_assert_not_reached ();
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
toolbar_realize (GtkWidget *widget)
{
	EggEditableToolbar *eggtoolbar = EGG_EDITABLE_TOOLBAR (widget);
	Toolbar *toolbar = EPHY_TOOLBAR (widget);
	EggToolbarsModel *model = egg_editable_toolbar_get_model (eggtoolbar);
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
toolbar_unrealize (GtkWidget *widget)
{
	EggEditableToolbar *eggtoolbar = EGG_EDITABLE_TOOLBAR (widget);
	Toolbar *toolbar = EPHY_TOOLBAR (widget);
	EggToolbarsModel *model = egg_editable_toolbar_get_model (eggtoolbar);

	g_signal_handlers_disconnect_by_func
		(model, G_CALLBACK (toolbar_added_cb), toolbar);

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
toolbar_class_init (ToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = toolbar_finalize;
	object_class->set_property = toolbar_set_property;
	object_class->get_property = toolbar_get_property;
	widget_class->realize = toolbar_realize;
	widget_class->unrealize = toolbar_unrealize;

	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(ToolbarPrivate));
}

static void
sync_user_input_cb (EphyLocationAction *action, GParamSpec *pspec, Toolbar *t)
{
	EphyTab *tab;
	const char *address;

	LOG ("sync_user_input_cb")

	if (t->priv->updating_address) return;

	tab = ephy_window_get_active_tab (t->priv->window);
	g_return_if_fail (EPHY_IS_TAB (tab));

	address = ephy_location_action_get_address (action);

	t->priv->updating_address = TRUE;
	ephy_tab_set_location (tab, address, TAB_ADDRESS_EXPIRE_CURRENT);
	t->priv->updating_address = FALSE;
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
	g_signal_connect (action, "notify::address",
			  G_CALLBACK (sync_user_input_cb), t);
	gtk_action_group_add_action (t->priv->action_group, action);
	update_location_editable (t);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_ZOOM_ACTION,
			       "name", "Zoom",
			       "label", _("Zoom"),
			       "stock_id", GTK_STOCK_ZOOM_IN,
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
toolbar_set_window (Toolbar *t, EphyWindow *window)
{
	GtkUIManager *manager;

	g_return_if_fail (t->priv->window == NULL);

	t->priv->window = window;
	manager = GTK_UI_MANAGER (window->ui_merge);

	toolbar_setup_actions (t);
	gtk_ui_manager_insert_action_group (manager,
					    t->priv->action_group, 1);

	t->priv->disable_arbitrary_url_notifier_id = eel_gconf_notification_add
		(CONF_LOCKDOWN_DISABLE_ARBITRARY_URL,
		 (GConfClientNotifyFunc)arbitrary_url_notifier, t);
}

static void
toolbar_style_sync (GtkToolbar *toolbar,
		    GtkToolbarStyle style,
		    GtkWidget *spinner)
{
	gboolean small;

	small = (style != GTK_TOOLBAR_BOTH);

	ephy_spinner_set_small_mode (EPHY_SPINNER (spinner), small);
}

static void
create_spinner (Toolbar *t)
{
	GtkWidget *spinner;
	GtkToolbar *toolbar;

	spinner = ephy_spinner_new ();
	gtk_widget_show (spinner);
	t->priv->spinner = spinner;

	toolbar = egg_editable_toolbar_set_fixed
		(EGG_EDITABLE_TOOLBAR (t), spinner);

	g_signal_connect (toolbar, "style_changed",
			  G_CALLBACK (toolbar_style_sync),
			  spinner);
}

static void
toolbar_init (Toolbar *t)
{
	t->priv = EPHY_TOOLBAR_GET_PRIVATE (t);

	create_spinner (t);
}

static void
toolbar_finalize (GObject *object)
{
	Toolbar *t = EPHY_TOOLBAR (object);
	EggEditableToolbar *eggtoolbar = EGG_EDITABLE_TOOLBAR (object);

	eel_gconf_notification_remove
		(t->priv->disable_arbitrary_url_notifier_id);

	g_signal_handlers_disconnect_by_func
		(egg_editable_toolbar_get_model (eggtoolbar),
		 G_CALLBACK (toolbar_added_cb), t);

	g_object_unref (t->priv->action_group);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("Toolbar finalized")
}

Toolbar *
toolbar_new (EphyWindow *window)
{
	return EPHY_TOOLBAR (g_object_new (EPHY_TYPE_TOOLBAR,
					   "window", window,
					   "MenuMerge", window->ui_merge,
					   "ToolbarsModel", ephy_shell_get_toolbars_model (ephy_shell, FALSE),
					   NULL));
}

void
toolbar_activate_location (Toolbar *t)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Location");

	gtk_action_activate (action);
}

void
toolbar_spinner_start (Toolbar *t)
{
	ephy_spinner_start (EPHY_SPINNER (t->priv->spinner));
}

void
toolbar_spinner_stop (Toolbar *t)
{
	ephy_spinner_stop (EPHY_SPINNER (t->priv->spinner));
}

void
toolbar_set_location (Toolbar *t,
		      const char *address)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	if (t->priv->updating_address) return;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Location");

	t->priv->updating_address = TRUE;
	ephy_location_action_set_address (EPHY_LOCATION_ACTION (action), address);
	t->priv->updating_address = FALSE;
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

const char *
toolbar_get_location (Toolbar *t)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Location");

	return ephy_location_action_get_address (EPHY_LOCATION_ACTION (action));
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
toolbar_update_zoom (Toolbar *t, float zoom)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = t->priv->action_group;
	action = gtk_action_group_get_action (action_group, "Zoom");
	g_object_set (action, "zoom", zoom, NULL);
}
