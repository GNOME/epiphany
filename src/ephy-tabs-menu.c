/*
 *  Copyright (C) 2003  David Bordoley
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

#include "ephy-tabs-menu.h"
#include "ephy-string.h"
#include "ephy-marshal.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkaccelmap.h>
#include <gtk/gtkaction.h>
#include <gtk/gtkradioaction.h>
#include <gtk/gtkuimanager.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

#define MAX_LABEL_LENGTH 30

#define EPHY_TABS_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TABS_MENU, EphyTabsMenuPrivate))

struct _EphyTabsMenuPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
	guint ui_id;
};

static void	ephy_tabs_menu_class_init	(EphyTabsMenuClass *klass);
static void	ephy_tabs_menu_init	  	(EphyTabsMenu *menu);

enum
{
	PROP_0,
	PROP_WINDOW
};

GType
ephy_tabs_menu_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
        {
                static const GTypeInfo our_info =
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
tab_added_cb (EphyNotebook *notebook, EphyTab *tab, EphyTabsMenu *menu)
{
	GtkAction *action;
	char accel_path[40];

        g_return_if_fail (EPHY_IS_TAB (tab));

	action = GTK_ACTION (ephy_tab_get_action (tab));

	g_snprintf (accel_path, sizeof (accel_path),
		    "<Actions>/TabsActions/%s", gtk_action_get_name (action));
	gtk_action_set_accel_path (action, accel_path);

	gtk_action_group_add_action (menu->priv->action_group, action);

	ephy_tabs_menu_update (menu);
}

static void
tab_removed_cb (EphyNotebook *notebook, EphyTab *tab, EphyTabsMenu *menu)
{
	GtkAction *action;
                                                                                                                             
        g_return_if_fail (EPHY_IS_TAB (tab));

	action = GTK_ACTION (ephy_tab_get_action (tab));
	gtk_action_group_remove_action (menu->priv->action_group, action);

	ephy_tabs_menu_update (menu);
}

static void
tabs_reordered_cb (EphyNotebook *notebook, EphyTabsMenu *menu)
{
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
		gtk_label_set_ellipsize (GTK_LABEL (GTK_BIN (proxy)->child),
					 PANGO_ELLIPSIZE_END);
	}
}

static void
ephy_tabs_menu_set_window (EphyTabsMenu *menu, EphyWindow *window)
{
	GtkWidget *notebook;
	GtkUIManager *manager;

	menu->priv->window = window;

	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	menu->priv->action_group = gtk_action_group_new ("TabsActions");
	gtk_ui_manager_insert_action_group (manager, menu->priv->action_group, -1);
	g_object_unref (menu->priv->action_group);

	g_signal_connect (menu->priv->action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	notebook = ephy_window_get_notebook (window);
	g_signal_connect_object (notebook, "tab_added",
			         G_CALLBACK (tab_added_cb), menu, 0);
	g_signal_connect_object (notebook, "tab_removed",
			         G_CALLBACK (tab_removed_cb), menu, 0);
	g_signal_connect_object (notebook, "tabs_reordered",
				 G_CALLBACK (tabs_reordered_cb), menu, 0);
}

static void
ephy_tabs_menu_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
        EphyTabsMenu *m = EPHY_TABS_MENU (object);

        switch (prop_id)
        {
                case PROP_WINDOW:
                        ephy_tabs_menu_set_window
				(m, EPHY_WINDOW (g_value_get_object (value)));
                        break;
        }
}

static void
ephy_tabs_menu_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
        EphyTabsMenu *m = EPHY_TABS_MENU (object);

        switch (prop_id)
        {
                case PROP_WINDOW:
                        g_value_set_object (value, m->priv->window);
                        break;
        }
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
                                                              G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyTabsMenuPrivate));
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

	if (p->ui_id > 0)
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
	const char *action_name, *action_group_name;
	char *accel_path;
	char accel[7];
	gint accel_number;
	guint accel_key;
	GdkModifierType accel_mods;

	action_name = gtk_action_get_name (action);
	action_group_name = gtk_action_group_get_name (action_group);

	/* set the accel path for the menu item */
	accel_path = g_strconcat ("<Actions>/", action_group_name, "/",
				  action_name, NULL);
	gtk_action_set_accel_path (action, accel_path);

	/* Only the first ten tabs get accelerators starting from 1 through 0 */
	if (tab_number < 10)
	{
		accel_key = 0;
		accel_number = (tab_number + 1) % 10;

		g_snprintf (accel, 7, "<alt>%d", accel_number);

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

	g_free (accel_path);
}

void
ephy_tabs_menu_update (EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *p = menu->priv;
	GtkUIManager *manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (p->window));
	EphyTab *tab;
	GtkAction *action;
	guint i = 0;
	GList *tabs = NULL, *l;

	LOG ("Rebuilding open tabs menu")

	START_PROFILER ("Rebuilding tabs menu")

	ephy_tabs_menu_clean (menu);

	tabs = ephy_window_get_tabs (p->window);

	if (g_list_length (tabs) == 0) return;

	p->ui_id = gtk_ui_manager_new_merge_id (manager);

	for (l = tabs; l != NULL; l = l->next)
	{
		const char *action_name;
		char *name;

		tab = (EphyTab *) l->data;
		action = GTK_ACTION (ephy_tab_get_action (tab));
		action_name = gtk_action_get_name (action);
		name = g_strdup_printf ("%sMenu", action_name);

		tab_set_action_accelerator (p->action_group, action, i++);

		gtk_ui_manager_add_ui (manager, p->ui_id,
				       "/menubar/TabsMenu/TabsOpen",
				       name, action_name,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
		g_free (name);
	}

	g_list_free (tabs);

	STOP_PROFILER ("Rebuilding tabs menu")
}
