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
ephy_tabs_menu_verb_cb (EggAction *action, EphyTab *tab)
{
	EphyWindow *window;

	g_return_if_fail (IS_EPHY_TAB (tab));

	window = ephy_tab_get_window (tab);	
	ephy_window_jump_to_tab (window, tab);
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
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);
	GString *xml;
	guint i = 0;
	guint len;
	GList *tabs, *l;
	GError *error = NULL;

	LOG ("Rebuilding open tabs menu")

	START_PROFILER ("Rebuilding tabs menu")

	ephy_tabs_menu_clean (wrhm);

	tabs = ephy_window_get_tabs (p->window);
	len = g_list_length (tabs);
	if (len == 0) return;

	/* it's faster to preallocate */
	xml = g_string_sized_new (52 * len + 105);

	g_string_append (xml, "<Root><menu><submenu name=\"TabsMenu\">"
			      "<placeholder name=\"TabsOpen\">");

	p->action_group = egg_action_group_new ("TabsActions");
	egg_menu_merge_insert_action_group (merge, p->action_group, 0);

	for (l = tabs; l != NULL; l = l->next)
	{
		gchar *verb = g_strdup_printf ("TabsOpen%d", i);
		gchar *title_s;
		const gchar *title;
		EphyTab *tab;
		EggAction *action;

		tab = (EphyTab *) l->data;

		title = ephy_tab_get_title (tab);
		title_s = ephy_string_shorten (title, MAX_LABEL_LENGTH);

		action = g_object_new (EGG_TYPE_ACTION,
				       "name", verb,
				       "label", title_s,
				       "tooltip", title,
				       "stock_id", NULL,
				       NULL);

		g_signal_connect (action, "activate",
				  G_CALLBACK (ephy_tabs_menu_verb_cb), tab);

		ephy_tabs_menu_set_action_accelerator (p->action_group, action, i);

		egg_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		g_string_append (xml, "<menuitem name=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "Menu");
		g_string_append (xml, "\" verb=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "\"/>\n");

		g_free (title_s);
		g_free (verb);

		++i;
	}

	g_string_append (xml, "</placeholder></submenu></menu></Root>");

	p->ui_id = egg_menu_merge_add_ui_from_string (merge, xml->str, -1, &error);

	g_string_free (xml, TRUE);
	
	STOP_PROFILER ("Rebuilding tabs menu")
}

void ephy_tabs_menu_update (EphyTabsMenu *wrhm)
{
	ephy_tabs_menu_rebuild (wrhm);
}
