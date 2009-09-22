/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2001, 2002 Jorn Baayen
 *  Copyright © 2003, 2004, 2005 Christian Persch
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

#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-toolbar.h"
#include "ephy-link.h"
#include "ephy-go-action.h"
#include "ephy-home-action.h"
#include "ephy-location-entry.h"
#include "ephy-location-action.h"
#include "ephy-navigation-action.h"
#include "ephy-topic-action.h"
#include "ephy-zoom-action.h"
#include "ephy-spinner-tool-item.h"
#include "ephy-dnd.h"
#include "ephy-shell.h"
#include "ephy-stock-icons.h"
#include "ephy-action-helper.h"
#include "window-commands.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

enum
{
	BACK_ACTION,
	FORWARD_ACTION,
	UP_ACTION,
	LOCATION_ACTION,
	ZOOM_ACTION,
	LAST_ACTION
};

enum
{
	SENS_FLAG = 1 << 0
};

#define EPHY_TOOLBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBAR, EphyToolbarPrivate))

struct _EphyToolbarPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
	GtkAction *actions[LAST_ACTION];
	GtkWidget *fixed_toolbar;
	EphySpinnerToolItem *spinner;
	GtkToolItem *sep_item;
	GtkToolItem *exit_button;
	gulong set_focus_handler;

	guint updating_address : 1;
	guint show_lock : 1;
	guint is_secure : 1;
	guint leave_fullscreen_visible : 1;
	guint spinning : 1;
};

static const GtkTargetEntry drag_targets [] =
{
	{ EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0 },
	{ EPHY_DND_TOPIC_TYPE,   0,		      1 },
	{ EPHY_DND_URL_TYPE,	 0,		      2 }
};

enum
{
	PROP_0,
	PROP_WINDOW
};

enum
{
	ACTIVATION_FINISHED,
	EXIT_CLICKED,
	LOCK_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ephy_toolbar_iface_init (EphyLinkIface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (EphyToolbar, ephy_toolbar, EGG_TYPE_EDITABLE_TOOLBAR,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                ephy_toolbar_iface_init))

/* helper functions */

static void
exit_button_clicked_cb (GtkWidget *button,
			EphyToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[EXIT_CLICKED], 0);
}

static void
ephy_toolbar_update_fixed_visibility (EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	gboolean show;

	show = priv->leave_fullscreen_visible;
	g_object_set (priv->sep_item, "visible", show, NULL);
	g_object_set (priv->fixed_toolbar, "visible", show, NULL);
}

static void
ephy_toolbar_update_spinner (EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	ephy_spinner_tool_item_set_spinning (priv->spinner, priv->spinning);
}

static void 
maybe_finish_activation_cb (EphyWindow *window,
			    GtkWidget *widget,
			    EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	GtkWidget *wtoolbar = GTK_WIDGET (toolbar);

	while (widget != NULL && widget != wtoolbar)
	{
		widget = gtk_widget_get_parent (widget);
	}

	/* if widget == toolbar, the new focus widget is in the toolbar, so we
	 * don't deactivate.
	 */
	if (widget != wtoolbar)
	{
		g_signal_handler_disconnect (window, priv->set_focus_handler);
		toolbar->priv->set_focus_handler = 0;

		g_signal_emit (toolbar, signals[ACTIVATION_FINISHED], 0);
	}
}

static void
sync_user_input_cb (EphyLocationAction *action,
		    GParamSpec *pspec,
		    EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	EphyEmbed *embed;
	const char *address;

	LOG ("sync_user_input_cb");

	if (priv->updating_address) return;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (priv->window));
        g_assert (EPHY_IS_EMBED (embed));

	address = ephy_location_action_get_address (action);

	priv->updating_address = TRUE;
	ephy_web_view_set_typed_address (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), address);
	priv->updating_address = FALSE;
}

static void
lock_clicked_cb (EphyLocationAction *action,
		 EphyToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[LOCK_CLICKED], 0);
}

static void
zoom_to_level_cb (GtkAction *action,
		  float zoom,
		  EphyToolbar *toolbar)
{
	ephy_window_set_zoom (toolbar->priv->window, zoom);
}

static void
ephy_toolbar_set_window (EphyToolbar *toolbar,
			 EphyWindow *window)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	GtkUIManager *manager;
	GtkAction *action;

	priv->window = window;
	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));

	priv->action_group = gtk_action_group_new ("SpecialToolbarActions");
	gtk_ui_manager_insert_action_group (manager, priv->action_group, -1);
	g_object_unref (priv->action_group);

	action = priv->actions[BACK_ACTION] =
		g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			      "name", "NavigationBack",
			      "label", _("_Back"),
			      "stock_id", GTK_STOCK_GO_BACK,
			      "tooltip", _("Go to the previous visited page"),
			      /* this is the tooltip on the Back button's drop-down arrow, which will show
			       * a menu with all sites you can go 'back' to
			       */
			      "arrow-tooltip", _("Back history"),
			      "window", priv->window,
			      "direction", EPHY_NAVIGATION_DIRECTION_BACK,
			      "is_important", TRUE,
			      NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	gtk_action_group_add_action_with_accel (priv->action_group, action,
						"<alt>Left");
	g_object_unref (action);

	action = priv->actions[FORWARD_ACTION] =
		g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			      "name", "NavigationForward",
			      "label", _("_Forward"),
			      "stock_id", GTK_STOCK_GO_FORWARD,
			      "tooltip", _("Go to the next visited page"),
			      /* this is the tooltip on the Forward button's drop-down arrow, which will show
			       * a menu with all sites you can go 'forward' to
			       */
			      "arrow-tooltip", _("Forward history"),
			      "window", priv->window,
			      "direction", EPHY_NAVIGATION_DIRECTION_FORWARD,
			      NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	gtk_action_group_add_action_with_accel (priv->action_group, action,
						"<alt>Right");
	g_object_unref (action);

	action = priv->actions[UP_ACTION] =
		g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			      "name", "NavigationUp",
			      "label", _("_Up"),
			      "stock_id", GTK_STOCK_GO_UP,
			      "tooltip", _("Go up one level"),
			      /* this is the tooltip on the Up button's drop-down arrow, which will show
			       * a menu with al sites you can go 'up' to
			       */
			      "arrow-tooltip", _("List of upper levels"),
			      "window", priv->window,
			      "direction", EPHY_NAVIGATION_DIRECTION_UP,
			      NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	gtk_action_group_add_action_with_accel (priv->action_group, action,
						"<alt>Up");
	g_object_unref (action);

	/* FIXME: I'm still waiting for the exact term to 
	 * user here from the docs team.
	 */
	action = priv->actions[LOCATION_ACTION] =
		g_object_new (EPHY_TYPE_LOCATION_ACTION,
			      "name", "Location",
			      "label", _("Address Entry"),
			      "stock_id", EPHY_STOCK_ENTRY,
			      "tooltip", _("Enter a web address to open, or a phrase to search for"),
			      "visible-overflown", FALSE,
			      "window", priv->window,
			      NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	g_signal_connect (action, "notify::address",
			  G_CALLBACK (sync_user_input_cb), toolbar);
	g_signal_connect (action, "lock-clicked",
			  G_CALLBACK (lock_clicked_cb), toolbar);
	gtk_action_group_add_action (priv->action_group, action);
	g_object_unref (action);

	action = priv->actions[ZOOM_ACTION] =
		g_object_new (EPHY_TYPE_ZOOM_ACTION,
			      "name", "Zoom",
			      "label", _("Zoom"),
			      "stock_id", GTK_STOCK_ZOOM_IN,
			      "tooltip", _("Adjust the text size"),
			      "zoom", 1.0,
			      NULL);
	g_signal_connect (action, "zoom_to_level",
			  G_CALLBACK (zoom_to_level_cb), toolbar);
	gtk_action_group_add_action (priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_GO_ACTION,
			       "name", "ToolbarGo",
			       "label", _("Go"),
			       "stock_id", GTK_STOCK_JUMP_TO,
			       "tooltip", _("Go to the address entered in the address entry"),
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_load_location), priv->window);
	gtk_action_group_add_action (priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_HOME_ACTION,
			       "name", "GoHome",
			       "label", _("_Home"),
			       "stock_id", GTK_STOCK_HOME,
			       "tooltip", _("Go to the home page"),
			       "is_important", TRUE,
			       NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	gtk_action_group_add_action_with_accel (priv->action_group, action, "<alt>Home");
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_HOME_ACTION,
			       "name", "FileNewTab",
			       "label", _("New _Tab"),
			       "stock_id", STOCK_NEW_TAB,
			       "tooltip", _("Open a new tab"),
			       NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	gtk_action_group_add_action_with_accel (priv->action_group, action, "<control>T");
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_HOME_ACTION,
			       "name", "FileNewWindow",
			       "label", _("_New Window"),
			       "stock_id", STOCK_NEW_WINDOW,
			       "tooltip", _("Open a new window"),
			       NULL);
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), toolbar);
	gtk_action_group_add_action_with_accel (priv->action_group, action, "<control>N");
	g_object_unref (action);

}

/* public functions */

GtkActionGroup *
ephy_toolbar_get_action_group (EphyToolbar *toolbar)
{
   return toolbar->priv->action_group;   
}

void
ephy_toolbar_set_favicon (EphyToolbar *toolbar,
			  GdkPixbuf *icon)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	g_object_set (priv->actions[LOCATION_ACTION], "icon", icon, NULL);
}

void
ephy_toolbar_set_show_leave_fullscreen (EphyToolbar *toolbar,
					gboolean show)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	priv->leave_fullscreen_visible = show != FALSE;

	ephy_toolbar_update_fixed_visibility (toolbar);
}

void
ephy_toolbar_activate_location (EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	GSList *proxies;
	GtkWidget *entry = NULL;
	gboolean visible;

	proxies = gtk_action_get_proxies (priv->actions[LOCATION_ACTION]);

	if (proxies != NULL && EPHY_IS_LOCATION_ENTRY (proxies->data))
	{
		entry = GTK_WIDGET (proxies->data);
	}

	if (entry == NULL)
	{
		/* happens when the user has removed the location entry from
		 * the toolbars.
		 */
		return;
	}

	g_object_get (G_OBJECT (toolbar), "visible", &visible, NULL);
	if (visible == FALSE)
	{
		gtk_widget_show (GTK_WIDGET (toolbar));
		toolbar->priv->set_focus_handler =
			g_signal_connect (toolbar->priv->window, "set-focus",
					  G_CALLBACK (maybe_finish_activation_cb),
					  toolbar);
	}

	ephy_location_entry_activate (EPHY_LOCATION_ENTRY (entry));
}

const char *
ephy_toolbar_get_location (EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	EphyLocationAction *action = EPHY_LOCATION_ACTION (priv->actions[LOCATION_ACTION]);

	return ephy_location_action_get_address (action);
}

void
ephy_toolbar_set_location (EphyToolbar *toolbar,
			   const char *address,
			   const char *typed_address)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	EphyLocationAction *action = EPHY_LOCATION_ACTION (priv->actions[LOCATION_ACTION]);

	if (priv->updating_address) return;

	priv->updating_address = TRUE;
	ephy_location_action_set_address (action, address, typed_address);
	priv->updating_address = FALSE;
}

void
ephy_toolbar_set_navigation_actions (EphyToolbar *toolbar,
				     gboolean back,
				     gboolean forward,
				     gboolean up)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	ephy_action_change_sensitivity_flags (priv->actions[BACK_ACTION], SENS_FLAG, !back);
	ephy_action_change_sensitivity_flags (priv->actions[FORWARD_ACTION], SENS_FLAG, !forward);
	ephy_action_change_sensitivity_flags (priv->actions[UP_ACTION], SENS_FLAG, !up);
}

void
ephy_toolbar_set_navigation_tooltips (EphyToolbar *toolbar,
				      const char *back_title,
				      const char *forward_title)
{
	EphyToolbarPrivate *priv = toolbar->priv;
	GValue value = { 0 };

	g_value_init (&value, G_TYPE_STRING);

	g_value_set_static_string (&value, back_title);
	g_object_set_property (G_OBJECT (priv->actions[BACK_ACTION]),
			       "tooltip", &value);

	g_value_set_static_string (&value, forward_title);
	g_object_set_property (G_OBJECT (priv->actions[FORWARD_ACTION]),
			       "tooltip", &value);
	g_value_unset (&value);
}

void
ephy_toolbar_set_security_state (EphyToolbar *toolbar,
				 gboolean is_secure,
				 gboolean show_lock,
				 const char *stock_id,
				 const char *tooltip)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	priv->show_lock = show_lock != FALSE;
	priv->is_secure = is_secure != FALSE;

	g_object_set (priv->actions[LOCATION_ACTION],
		      "lock-stock-id", stock_id,
		      "lock-tooltip", tooltip,
		      "show-lock", priv->show_lock,
		      "secure", is_secure,
		      NULL);
}

void
ephy_toolbar_set_spinning (EphyToolbar *toolbar,
			   gboolean spinning)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	priv->spinning = spinning != FALSE;

	ephy_toolbar_update_spinner (toolbar);
}

void
ephy_toolbar_set_zoom (EphyToolbar *toolbar,
		       gboolean can_zoom,
		       float zoom)
{
	EphyToolbarPrivate *priv = toolbar->priv;

	gtk_action_set_sensitive (priv->actions[ZOOM_ACTION], can_zoom);
	g_object_set (priv->actions[ZOOM_ACTION], "zoom", can_zoom ? zoom : 1.0, NULL);
}

/* Class implementation */

static void
ephy_toolbar_show (GtkWidget *widget)
{
	EphyToolbar *toolbar = EPHY_TOOLBAR (widget);

	GTK_WIDGET_CLASS (ephy_toolbar_parent_class)->show (widget);

	ephy_toolbar_update_spinner (toolbar);
}

static void
ephy_toolbar_hide (GtkWidget *widget)
{
	EphyToolbar *toolbar = EPHY_TOOLBAR (widget);

	GTK_WIDGET_CLASS (ephy_toolbar_parent_class)->hide (widget);

	ephy_toolbar_update_spinner (toolbar);
}

static void
ephy_toolbar_init (EphyToolbar *toolbar)
{
	EphyToolbarPrivate *priv;

	priv = toolbar->priv = EPHY_TOOLBAR_GET_PRIVATE (toolbar);
}

static GObject *
ephy_toolbar_constructor (GType type,
			  guint n_construct_properties,
			  GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyToolbar *toolbar;
	EphyToolbarPrivate *priv;
	GtkToolbar *gtoolbar;

	object = G_OBJECT_CLASS (ephy_toolbar_parent_class)->constructor (type,
                                                                          n_construct_properties,
                                                                          construct_params);

	toolbar = EPHY_TOOLBAR (object);
	priv = toolbar->priv;

	priv->fixed_toolbar = gtk_toolbar_new ();
	gtoolbar = GTK_TOOLBAR (priv->fixed_toolbar);
	gtk_toolbar_set_show_arrow (gtoolbar, FALSE);

	priv->spinner = EPHY_SPINNER_TOOL_ITEM (ephy_spinner_tool_item_new ());
	gtk_toolbar_insert (gtoolbar, GTK_TOOL_ITEM (priv->spinner), -1);
	gtk_widget_show (GTK_WIDGET (priv->spinner));

	priv->sep_item = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (gtoolbar, priv->sep_item, -1);

	priv->exit_button = gtk_tool_button_new_from_stock (GTK_STOCK_LEAVE_FULLSCREEN);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (priv->exit_button), _("Leave Fullscreen"));
	gtk_tool_item_set_is_important (priv->exit_button, TRUE);
	g_signal_connect (priv->exit_button, "clicked",
			  G_CALLBACK (exit_button_clicked_cb), toolbar);
	gtk_toolbar_insert (gtoolbar, priv->exit_button, -1);
	
	egg_editable_toolbar_set_fixed (EGG_EDITABLE_TOOLBAR (toolbar), gtoolbar);

	ephy_toolbar_update_fixed_visibility (toolbar);

	return object;
}

static void
ephy_toolbar_finalize (GObject *object)
{
	EphyToolbar *toolbar = EPHY_TOOLBAR (object);
	EphyToolbarPrivate *priv = toolbar->priv;

	if (priv->set_focus_handler != 0)
	{
		g_signal_handler_disconnect (priv->window,
					     priv->set_focus_handler);
	}

	G_OBJECT_CLASS (ephy_toolbar_parent_class)->finalize (object);
}

static void
ephy_toolbar_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_toolbar_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	EphyToolbar *toolbar = EPHY_TOOLBAR (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			ephy_toolbar_set_window (toolbar, g_value_get_object (value));
			break;
	}
}

static void
ephy_toolbar_class_init (EphyToolbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructor = ephy_toolbar_constructor;
	object_class->finalize = ephy_toolbar_finalize;
	object_class->set_property = ephy_toolbar_set_property;
	object_class->get_property = ephy_toolbar_get_property;

	widget_class->show = ephy_toolbar_show;
	widget_class->hide = ephy_toolbar_hide;

	signals[ACTIVATION_FINISHED] =
		g_signal_new ("activation-finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyToolbarClass, activation_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	signals[EXIT_CLICKED] =
		g_signal_new
			("exit-clicked",
			 EPHY_TYPE_TOOLBAR,
			 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (EphyToolbarClass, exit_clicked),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);

	signals[LOCK_CLICKED] =
		g_signal_new
			("lock-clicked",
			 EPHY_TYPE_TOOLBAR,
			 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (EphyToolbarClass, lock_clicked),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyToolbarPrivate));
}

EphyToolbar *
ephy_toolbar_new (EphyWindow *window)
{
	EggEditableToolbar *etoolbar;
	
	etoolbar = EGG_EDITABLE_TOOLBAR 
	  (g_object_new (EPHY_TYPE_TOOLBAR,
			 "window", window,
			 "ui-manager", ephy_window_get_ui_manager (window),
			 "popup-path", "/ToolbarPopup",
			 NULL));
	
	egg_editable_toolbar_add_visibility 
	  (etoolbar, "/menubar/ViewMenu/ViewTogglesGroup/ToolbarMenu/ViewToolbarsGroup");
	egg_editable_toolbar_add_visibility 
	  (etoolbar, "/ToolbarPopup/ViewToolbarsGroup");

	return EPHY_TOOLBAR (etoolbar);
}
