/*
 *  Copyright (C) 2003  David Bordoley
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

#include <gtk/gtkaccelmap.h>
#include <gtk/gtkaction.h>
#include <gtk/gtkuimanager.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

#define MAX_LABEL_LENGTH 30

/**
 * Private data
 */

#define EPHY_TABS_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TABS_MENU, EphyTabsMenuPrivate))

struct _EphyTabsMenuPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
	guint ui_id;
};

/**
 * Private functions, only availble from this file
 */
static void	ephy_tabs_menu_class_init	(EphyTabsMenuClass *klass);
static void	ephy_tabs_menu_init	  	(EphyTabsMenu *menu);
static void	ephy_tabs_menu_finalize_impl 	(GObject *o);

enum
{
	PROP_0,
	PROP_WINDOW
};

static gpointer g_object_class;

/**
 * EphyTabsMenu object
 */

GType
ephy_tabs_menu_get_type (void)
{
        static GType ephy_tabs_menu_type = 0;

        if (ephy_tabs_menu_type == 0)
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

                ephy_tabs_menu_type = g_type_register_static (G_TYPE_OBJECT,
							      "EphyTabsMenu",
							      &our_info, 0);
        }

        return ephy_tabs_menu_type;
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
                        m->priv->window = g_value_get_object (value);
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

	G_OBJECT_CLASS (klass)->finalize = ephy_tabs_menu_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_tabs_menu_set_property;
	object_class->get_property = ephy_tabs_menu_get_property;

	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
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

	menu->priv->ui_id = 0;
	menu->priv->action_group = NULL;
}

static void
ephy_tabs_menu_clean (EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *p = menu->priv;
	GtkUIManager *merge = GTK_UI_MANAGER (p->window->ui_merge);

	if (p->ui_id > 0)
	{
		gtk_ui_manager_remove_ui (merge, p->ui_id);
		gtk_ui_manager_ensure_update (merge);
		p->ui_id = 0;
	}

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group (merge, p->action_group);
		g_object_unref (p->action_group);
	}
}

static void
ephy_tabs_menu_finalize_impl (GObject *o)
{
	EphyTabsMenu *menu = EPHY_TABS_MENU (o);
	EphyTabsMenuPrivate *p = menu->priv;

	if (p->action_group != NULL)
	{
		g_object_unref (p->action_group);
	}

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}


EphyTabsMenu *
ephy_tabs_menu_new (EphyWindow *window)
{
	return EPHY_TABS_MENU (g_object_new (EPHY_TYPE_TABS_MENU,
					     "EphyWindow", window,
					     NULL));
}

static void
tab_set_action_accelerator (GtkActionGroup *action_group,
			    GtkAction *action,
			    guint tab_number)
{
	const char *action_name, *action_group_name;
	char *accel_path = NULL;
	char accel[7];
	gint accel_number;
	guint accel_key;
	GdkModifierType accel_mods;

	action_name = gtk_action_get_name (action);
	action_group_name = gtk_action_group_get_name (action_group);

	/* set the accel path for the menu item */
	accel_path = g_strconcat ("<Actions>/", action_group_name, "/",
				  action_name, NULL);

	/* Only the first ten tabs get accelerators starting from 1 through 0 */
	if (tab_number < 10)
	{
		accel_key = 0;
		accel_number = (tab_number + 1) % 10;

		g_snprintf (accel, 7, "<alt>%d", accel_number);

		gtk_accelerator_parse (accel, &accel_key, &accel_mods);

		if (accel_key != 0)
		{
			gtk_action_set_accel_path (action, accel_path);
		}
	}
	else
	{
		gtk_action_set_accel_path (action, accel_path);
	}
}

void
ephy_tabs_menu_update (EphyTabsMenu *menu)
{
	EphyTabsMenuPrivate *p;
	GtkUIManager *merge;
	EphyTab *tab;
	GtkAction *action;
	guint i = 0;
	guint num = 0;
	GList *tabs = NULL, *l;

	g_return_if_fail (EPHY_IS_TABS_MENU (menu));
	p = menu->priv;
	merge = GTK_UI_MANAGER (p->window->ui_merge);
	
	LOG ("Rebuilding open tabs menu")

	START_PROFILER ("Rebuilding tabs menu")

	ephy_tabs_menu_clean (menu);

	tabs = ephy_window_get_tabs (p->window);

	num = g_list_length (tabs);
	if (num == 0) return;

	p->action_group = gtk_action_group_new ("TabsActions");
	p->ui_id = gtk_ui_manager_new_merge_id (merge);

	for (l = tabs; l != NULL; l = l->next)
	{
		const char *action_name;
		char *name;


		tab = (EphyTab *) l->data;
		action = GTK_ACTION (ephy_tab_get_action (tab));
		action_name = gtk_action_get_name (action);
		name = g_strdup_printf ("%sMenu", action_name);

		tab_set_action_accelerator (p->action_group, action, i);

		gtk_action_group_add_action (p->action_group, action);

		gtk_ui_manager_add_ui (merge, p->ui_id,
				       "/menubar/TabsMenu/TabsOpen",
				       name, action_name,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
		g_free (name);
	}

	g_list_free (tabs);

	gtk_ui_manager_insert_action_group (merge, p->action_group, 0);

	STOP_PROFILER ("Rebuilding tabs menu")
}
