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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-tabs-menu.h"
#include "ephy-gobject-misc.h"
#include "ephy-string.h"
#include "egg-menu-merge.h"
#include "ephy-marshal.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

#define MAX_LABEL_LENGTH 30

/**
 * Private data
 */
struct _EphyTabsMenuPrivate
{
	EphyWindow *window;
	EggActionGroup *action_group;
	guint ui_id;
};

typedef struct
{
	EphyWindow *window;
	EphyTab *tab;
} TabsData;

/**
 * Private functions, only availble from this file
 */
static void	ephy_tabs_menu_class_init	  (EphyTabsMenuClass *klass);
static void	ephy_tabs_menu_init	  (EphyTabsMenu *wrhm);
static void	ephy_tabs_menu_finalize_impl (GObject *o);
static void	ephy_tabs_menu_rebuild	  (EphyTabsMenu *wrhm);
static void     ephy_tabs_menu_set_property  (GObject *object,
						   guint prop_id,
						   const GValue *value,
						   GParamSpec *pspec);
static void	ephy_tabs_menu_get_property  (GObject *object,
						   guint prop_id,
						   GValue *value,
						   GParamSpec *pspec);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static gpointer g_object_class;

/**
 * EphyTabsMenu object
 */
MAKE_GET_TYPE (ephy_tabs_menu,
	       "EphyTabsMenu", EphyTabsMenu,
	       ephy_tabs_menu_class_init, ephy_tabs_menu_init,
	       G_TYPE_OBJECT);

static void
ephy_tabs_menu_class_init (EphyTabsMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = ephy_tabs_menu_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_tabs_menu_set_property;
	object_class->get_property = ephy_tabs_menu_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
ephy_tabs_menu_init (EphyTabsMenu *wrhm)
{
	EphyTabsMenuPrivate *p = g_new0 (EphyTabsMenuPrivate, 1);
	wrhm->priv = p;

	wrhm->priv->ui_id = -1;
	wrhm->priv->action_group = NULL;
}

static void
ephy_tabs_menu_clean (EphyTabsMenu *wrhm)
{
	EphyTabsMenuPrivate *p = wrhm->priv;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);

	if (p->ui_id >= 0)
	{
		egg_menu_merge_remove_ui (merge, p->ui_id);
		egg_menu_merge_ensure_update (merge);
	}

	if (p->action_group != NULL)
	{
		egg_menu_merge_remove_action_group (merge, p->action_group);
		g_object_unref (p->action_group);
	}
}

static void
ephy_tabs_menu_finalize_impl (GObject *o)
{
	EphyTabsMenu *wrhm = EPHY_TABS_MENU (o);
	EphyTabsMenuPrivate *p = wrhm->priv;

	if (p->action_group != NULL)
	{
		egg_menu_merge_remove_action_group
			(EGG_MENU_MERGE (p->window->ui_merge),
			 p->action_group);
		g_object_unref (p->action_group);
	}

	g_free (p);

	G_OBJECT_CLASS (g_object_class)->finalize (o);
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
                case PROP_EPHY_WINDOW:
                        m->priv->window = g_value_get_object (value);
			ephy_tabs_menu_rebuild (m);
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
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, m->priv->window);
                        break;
        }
}

EphyTabsMenu *
ephy_tabs_menu_new (EphyWindow *window)
{
	EphyTabsMenu *ret = g_object_new (EPHY_TYPE_TABS_MENU,
					       "EphyWindow", window,
					       NULL);
	return ret;
}

static void
ephy_tabs_menu_verb_cb (EggMenuMerge *merge,
			TabsData *data)
{
	ephy_window_jump_to_tab (data->window, data->tab);
}


/* This code is from EggActionGroup:
 * Ideally either EggAction should support setting an accelerator from
 * a string or EggActionGroup would support adding single EggActionEntry's
 * to an action group.
 */
static void
ephy_tabs_menu_set_action_accelerator (EggActionGroup *action_group,
				       EggAction *action,
				       int tab_number)
{
	gchar *accel_path;
	gint accel_number;

	g_return_if_fail (tab_number >= 0);

	/* set the accel path for the menu item */
	accel_path = g_strconcat ("<Actions>/", action_group->name, "/", action->name, NULL);

	/* Only the first ten tabs get accelerators starting from 1 through 0 */
	if (tab_number == 9)
	{
		accel_number = 0;
	}
	else
	{
		accel_number = (tab_number + 1);
	}

	if (accel_number < 10)
	{
		guint accel_key = 0;
		GdkModifierType accel_mods;
		gchar* accelerator;

		accelerator = g_strdup_printf ("<alt>%d", accel_number);

		gtk_accelerator_parse (accelerator, &accel_key,
					&accel_mods);
		if (accel_key)
			gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);

		g_free (accelerator);
	}

	action->accel_quark = g_quark_from_string (accel_path);
	g_free (accel_path);
}

static void
ephy_tabs_menu_rebuild (EphyTabsMenu *wrhm)
{
	EphyTabsMenuPrivate *p = wrhm->priv;
	GString *xml;
	gint i;
	GList *tabs;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);

	LOG ("Rebuilding open tabs menu")

	ephy_tabs_menu_clean (wrhm);

	tabs = ephy_window_get_tabs (p->window);

	xml = g_string_new (NULL);
	g_string_append (xml, "<Root><menu><submenu name=\"TabsMenu\">"
			      "<placeholder name=\"TabsOpen\">");

	p->action_group = egg_action_group_new ("TabsActions");
	egg_menu_merge_insert_action_group (merge, p->action_group, 0);

	for (i = 0; i < g_list_length (tabs); i++)
	{
		char *verb = g_strdup_printf ("TabsOpen%d", i);
		char *title_s;
		const char *title;
		xmlChar *label_x;
		EphyTab *child;
		TabsData *data;
		EggAction *action;

		child = g_list_nth_data (tabs, i);

		title = ephy_tab_get_title(child);
		title_s = ephy_string_shorten (title, MAX_LABEL_LENGTH);
		label_x = xmlEncodeSpecialChars (NULL, title_s);

		data = g_new0 (TabsData, 1);
		data->window = wrhm->priv->window;
		data->tab = child;

		action = g_object_new (EGG_TYPE_ACTION,
				       "name", verb,
				       "label", label_x,
				       "tooltip", "Hello",
				       "stock_id", NULL,
				       NULL);

		g_signal_connect_closure
			(action, "activate",
			 g_cclosure_new (G_CALLBACK (ephy_tabs_menu_verb_cb),
					 data,
					 (GClosureNotify)g_free),
			 FALSE);

		ephy_tabs_menu_set_action_accelerator (p->action_group, action, i);

		egg_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		g_string_append (xml, "<menuitem name=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "Menu");
		g_string_append (xml, "\" verb=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "\"/>\n");

		xmlFree (label_x);
		g_free (title_s);
		g_free (verb);
	}

	g_string_append (xml, "</placeholder></submenu></menu></Root>");

	if (g_list_length (tabs) > 0)
	{
		GError *error = NULL;
		LOG ("Merging ui\n%s",xml->str);
		p->ui_id = egg_menu_merge_add_ui_from_string
			(merge, xml->str, -1, &error);
	}

	g_string_free (xml, TRUE);
}

void ephy_tabs_menu_update (EphyTabsMenu *wrhm)
{
	ephy_tabs_menu_rebuild (wrhm);
}
