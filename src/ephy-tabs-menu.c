/*
 *  Copyright © 2003  David Bordoley
 *  Copyright © 2003-2004 Christian Persch
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

#include "ephy-tabs-menu.h"
#include "ephy-notebook.h"
#include "ephy-marshal.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkaccelmap.h>
#include <gtk/gtkaction.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkradioaction.h>
#include <gtk/gtkuimanager.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

#define LABEL_WIDTH_CHARS 32
#define ACTION_VERB_FORMAT		"JmpTab%x"
#define ACTION_VERB_FORMAT_LENGTH	strlen (ACTION_VERB_FORMAT) + 14 + 1
#define ACCEL_PATH_FORMAT		"<Actions>/TabsActions/%s"
#define ACCEL_PATH_FORMAT_LENGTH	strlen (ACCEL_PATH_FORMAT) -2 + ACTION_VERB_FORMAT_LENGTH
#define DATA_KEY			"EphyTabsMenu::Action"

#define EPHY_TABS_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TABS_MENU, EphyTabsMenuPrivate))

struct _EphyTabsMenuPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
	GtkAction *anchor_action;
	guint ui_id;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

static void	ephy_tabs_menu_class_init	(EphyTabsMenuClass *klass);
static void	ephy_tabs_menu_init	  	(EphyTabsMenu *menu);

GType
ephy_tabs_menu_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyTabsMenuClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_tabs_menu_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyTab),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_tabs_menu_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyTabsMenu",
					       &our_info, 0);
	}

	return type;
}

static void
tab_action_activate_cb (GtkToggleAction *action,
			EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *priv = menu->priv;
	EphyTab *tab;

	if (gtk_toggle_action_get_active (action) == FALSE)
	{
		return;
	}

	tab = g_object_get_data (G_OBJECT (action), DATA_KEY);
	g_return_if_fail (tab != NULL);

	LOG ("tab_action_activate_cb tab=%p", tab);

	if (ephy_window_get_active_tab (priv->window) != tab)
	{
		ephy_window_jump_to_tab (priv->window, tab);
	}
}

static void
sync_tab_title (EphyTab *tab,
		GParamSpec *pspec,
		GtkAction *action)
{
	const char *title;

	title = ephy_tab_get_title_composite (tab);

	g_object_set (action, "label", title, NULL);
}

static void
notebook_page_added_cb (EphyNotebook *notebook,
			EphyTab *tab,
			guint position,
			EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *priv = menu->priv;
	GtkAction *action;
	char verb[ACTION_VERB_FORMAT_LENGTH];
	GSList *group;

	LOG ("tab_added_cb tab=%p", tab);

	g_snprintf (verb, sizeof (verb), ACTION_VERB_FORMAT,
		    _ephy_tab_get_id (tab));
  
	action = g_object_new (GTK_TYPE_RADIO_ACTION,
			       "name", verb,
			       "tooltip", _("Switch to this tab"),
			       NULL);

	sync_tab_title (tab, NULL, action);
	/* make sure the action is alive when handling the signal, see bug #169833 */
	g_signal_connect_object (tab, "notify::title",
				 G_CALLBACK (sync_tab_title), action, 0);

	gtk_action_group_add_action_with_accel (priv->action_group, action, NULL);

	group = gtk_radio_action_get_group (GTK_RADIO_ACTION (priv->anchor_action));
	gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);

	/* set this here too, since tab-added comes after notify::active-tab */
	if (ephy_window_get_active_tab (priv->window) == tab)
	{
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}

	g_object_set_data (G_OBJECT (tab), DATA_KEY, action);
	g_object_set_data (G_OBJECT (action), DATA_KEY, tab);

	g_signal_connect (action, "activate",
			  G_CALLBACK (tab_action_activate_cb), menu);

	g_object_unref (action);

	ephy_tabs_menu_update (menu);
}

static void
notebook_page_removed_cb (EphyNotebook *notebook,
			  EphyTab *tab,
			  guint position,
			  EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *priv = menu->priv;
	GtkAction *action;
													     
	LOG ("tab_removed_cb tab=%p", tab);

	action = g_object_get_data (G_OBJECT (tab), DATA_KEY);
	g_return_if_fail (action != NULL);

	g_signal_handlers_disconnect_by_func
		(tab, G_CALLBACK (sync_tab_title), action);

	g_signal_handlers_disconnect_by_func
		(action, G_CALLBACK (tab_action_activate_cb), menu);

	g_object_set_data (G_OBJECT (tab), DATA_KEY, NULL);
 	gtk_action_group_remove_action (priv->action_group, action);

	ephy_tabs_menu_update (menu);
}

static void
notebook_page_reordered_cb (EphyNotebook *notebook,
			    EphyTab *tab,
			    guint position,
			    EphyTabsMenu *menu)
{
	LOG ("tabs_reordered_cb");
 
	ephy_tabs_menu_update (menu);
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
		  GtkAction *action,
		  GtkWidget *proxy,
		  gpointer dummy)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		GtkLabel *label;

		label = GTK_LABEL (GTK_BIN (proxy)->child);

		gtk_label_set_use_underline (label, FALSE);
		gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars (label, LABEL_WIDTH_CHARS);
	}
}

static void
sync_active_tab (EphyWindow *window,
		 GParamSpec *pspec,
		 EphyTabsMenu *menu)
{
	EphyTab *tab;
	GtkAction *action;

	tab = ephy_window_get_active_tab (window);
	if (tab == NULL) return;

	LOG ("active tab is tab %p", tab);

	action = g_object_get_data (G_OBJECT (tab), DATA_KEY);
	/* happens initially, since the ::active-tab comes before
	* the ::tab-added signal
	*/
	/* FIXME that's not true with gtk+ 2.9 anymore */
	if (action == NULL) return;

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
}

static void
ephy_tabs_menu_set_window (EphyTabsMenu *menu,
			   EphyWindow *window)
{
	EphyTabsMenuPrivate *priv = menu->priv;
	GtkWidget *notebook;
	GtkUIManager *manager;

	priv->window = window;

	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	priv->action_group = gtk_action_group_new ("TabsActions");
	gtk_ui_manager_insert_action_group (manager, priv->action_group, -1);
	g_object_unref (priv->action_group);

	priv->anchor_action = g_object_new (GTK_TYPE_RADIO_ACTION,
					    "name", "TabsMenuAnchorAction",
					    NULL);
	gtk_action_group_add_action (priv->action_group, priv->anchor_action);

	g_signal_connect (priv->action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	g_signal_connect (window, "notify::active-tab",
			  G_CALLBACK (sync_active_tab), menu);

	notebook = ephy_window_get_notebook (window);
	g_signal_connect_object (notebook, "page-added",
				 G_CALLBACK (notebook_page_added_cb), menu, 0);
	g_signal_connect_object (notebook, "page-removed",
				 G_CALLBACK (notebook_page_removed_cb), menu, 0);
	g_signal_connect_object (notebook, "page-reordered",
				 G_CALLBACK (notebook_page_reordered_cb), menu, 0);
}

static void
ephy_tabs_menu_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	EphyTabsMenu *menu = EPHY_TABS_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			ephy_tabs_menu_set_window (menu, g_value_get_object (value));
			break;
	}
}

static void
ephy_tabs_menu_get_property (GObject *object,
			     guint prop_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	/* no readable properties */
	g_return_if_reached ();
}

static void
ephy_tabs_menu_class_init (EphyTabsMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = ephy_tabs_menu_set_property;
	object_class->get_property = ephy_tabs_menu_get_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyTabsMenuPrivate));
}

static void
ephy_tabs_menu_init (EphyTabsMenu *menu)
{
	menu->priv = EPHY_TABS_MENU_GET_PRIVATE (menu);
}

static void
ephy_tabs_menu_clean (EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *p = menu->priv;
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (p->window));

	if (p->ui_id != 0)
	{
		gtk_ui_manager_remove_ui (manager, p->ui_id);
		gtk_ui_manager_ensure_update (manager);
		p->ui_id = 0;
	}
}

EphyTabsMenu *
ephy_tabs_menu_new (EphyWindow *window)
{
	return EPHY_TABS_MENU (g_object_new (EPHY_TYPE_TABS_MENU,
					     "window", window,
					     NULL));
}

static void
tab_set_action_accelerator (GtkActionGroup *action_group,
			    GtkAction *action,
			    guint tab_number)
{
	const char *verb;
	char accel_path[ACCEL_PATH_FORMAT_LENGTH];
	char accel[7];
	gint accel_number;
	guint accel_key;
	GdkModifierType accel_mods;

	verb = gtk_action_get_name (action);

	/* set the accel path for the menu item */
	g_snprintf (accel_path, sizeof (accel_path),
		    ACCEL_PATH_FORMAT, verb);
	gtk_action_set_accel_path (action, accel_path);

	/* Only the first ten tabs get accelerators starting from 1 through 0 */
	if (tab_number < 10)
	{
		accel_key = 0;
		accel_number = (tab_number + 1) % 10;

		g_snprintf (accel, sizeof (accel), "<alt>%d", accel_number);

		gtk_accelerator_parse (accel, &accel_key, &accel_mods);

		if (accel_key != 0)
		{
			gtk_accel_map_change_entry (accel_path, accel_key,
						    accel_mods, TRUE);
		}
	}
	else
	{
		gtk_accel_map_change_entry (accel_path, 0, 0, TRUE);
	}
}

void
ephy_tabs_menu_update (EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *p = menu->priv;
	GtkUIManager *manager;
	GtkAction *action;
	GList *tabs = NULL, *l;
	guint i = 0;
	const char *verb;

	LOG ("Rebuilding open tabs menu");

	START_PROFILER ("Rebuilding tabs menu")

	ephy_tabs_menu_clean (menu);

	tabs = ephy_window_get_tabs (p->window);

	if (g_list_length (tabs) == 0) return;

	manager =  GTK_UI_MANAGER (ephy_window_get_ui_manager (p->window));
	p->ui_id = gtk_ui_manager_new_merge_id (manager);

	for (l = tabs; l != NULL; l = l->next)
	{
		action = g_object_get_data (G_OBJECT (l->data), DATA_KEY);
		g_return_if_fail (action != NULL);
  
		verb = gtk_action_get_name (action);

		tab_set_action_accelerator (p->action_group, action, i++);

		gtk_ui_manager_add_ui (manager, p->ui_id,
				       "/menubar/TabsMenu/TabsOpen",
				       verb, verb,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
	}

	g_list_free (tabs);

	STOP_PROFILER ("Rebuilding tabs menu")
}
