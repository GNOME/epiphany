/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#include "ephy-bookmarks-toolbar.h"
#include "ephy-bookmark-action.h"
#include "ephy-topic-action.h"
#include "ephy-gobject-misc.h"
#include "egg-menu-merge.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

/**
 * Private data
 */
struct _EphyBookmarksToolbarPrivate
{
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	EggActionGroup *action_group;
	guint ui_id;
};

typedef struct
{
	EphyWindow *window;
	const char *url;
} FavoriteData;

/**
 * Private functions, only availble from this file
 */
static void	ephy_bookmarks_toolbar_class_init	  (EphyBookmarksToolbarClass *klass);
static void	ephy_bookmarks_toolbar_init	  (EphyBookmarksToolbar *wrhm);
static void	ephy_bookmarks_toolbar_finalize_impl (GObject *o);
static void	ephy_bookmarks_toolbar_rebuild	  (EphyBookmarksToolbar *wrhm);
static void     ephy_bookmarks_toolbar_set_property  (GObject *object,
						   guint prop_id,
						   const GValue *value,
						   GParamSpec *pspec);
static void	ephy_bookmarks_toolbar_get_property  (GObject *object,
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
 * EphyBookmarksToolbar object
 */
MAKE_GET_TYPE (ephy_bookmarks_toolbar,
	       "EphyBookmarksToolbar", EphyBookmarksToolbar,
	       ephy_bookmarks_toolbar_class_init, ephy_bookmarks_toolbar_init,
	       G_TYPE_OBJECT);

static void
ephy_bookmarks_toolbar_class_init (EphyBookmarksToolbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = ephy_bookmarks_toolbar_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_bookmarks_toolbar_set_property;
	object_class->get_property = ephy_bookmarks_toolbar_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
bookmarks_changed_cb (EphyNode *node,
		      EphyNode *child,
		      EphyBookmarksToolbar *bt)
{
	/* FIXME this is updating way too often, be smarter */

	ephy_bookmarks_toolbar_update (bt);
}

static void
ephy_bookmarks_toolbar_init (EphyBookmarksToolbar *wrhm)
{
	EphyBookmarksToolbarPrivate *p = g_new0 (EphyBookmarksToolbarPrivate, 1);
	EphyNode *bms, *topics;

	wrhm->priv = p;

	wrhm->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	wrhm->priv->ui_id = -1;
	wrhm->priv->action_group = NULL;

	bms = ephy_bookmarks_get_bookmarks (p->bookmarks);
	topics = ephy_bookmarks_get_keywords (p->bookmarks);

	g_signal_connect (bms, "child_changed",
			  G_CALLBACK (bookmarks_changed_cb), wrhm);
	g_signal_connect (topics, "child_changed",
			  G_CALLBACK (bookmarks_changed_cb), wrhm);
}

static void
ephy_bookmarks_toolbar_clean (EphyBookmarksToolbar *wrhm)
{
	EphyBookmarksToolbarPrivate *p = wrhm->priv;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);

	if (p->ui_id >= 0)
	{
		egg_menu_merge_remove_ui (merge, p->ui_id);
	}

	if (p->action_group != NULL)
	{
		egg_menu_merge_remove_action_group (merge, p->action_group);
		g_object_unref (p->action_group);
	}
}

static void
ephy_bookmarks_toolbar_finalize_impl (GObject *o)
{
	EphyBookmarksToolbar *wrhm = EPHY_BOOKMARKS_TOOLBAR (o);
	EphyBookmarksToolbarPrivate *p = wrhm->priv;

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
ephy_bookmarks_toolbar_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        EphyBookmarksToolbar *m = EPHY_BOOKMARKS_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        m->priv->window = g_value_get_object (value);
			ephy_bookmarks_toolbar_rebuild (m);
                        break;
        }
}

static void
ephy_bookmarks_toolbar_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
        EphyBookmarksToolbar *m = EPHY_BOOKMARKS_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, m->priv->window);
                        break;
        }
}

static void
go_location_cb (EggAction *action, char *location, EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_load_url (embed, location);
}

EphyBookmarksToolbar *
ephy_bookmarks_toolbar_new (EphyWindow *window)
{
	EphyBookmarksToolbar *ret = g_object_new (EPHY_TYPE_BOOKMARKS_TOOLBAR,
					       "EphyWindow", window,
					       NULL);
	return ret;
}

static void
add_toolitem (const char *verb,
	      GString *xml)
{
	static int name_id = 0;

	name_id++;

	g_string_append (xml, "<toolitem name=\"");
	g_string_append_printf (xml, "Name%d", name_id);
	g_string_append (xml, "Menu");
	g_string_append (xml, "\" verb=\"");
	g_string_append (xml, verb);
	g_string_append (xml, "\"/>\n");
}

static void
ephy_bookmarks_toolbar_rebuild (EphyBookmarksToolbar *wrhm)
{
	EphyBookmarksToolbarPrivate *p = wrhm->priv;
	GString *xml;
	EphyNode *bms;
	EphyNode *topics;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);
	GError *error = NULL;
	GPtrArray *children;
	gint i;

	LOG ("Rebuilding recent history menu")

	ephy_bookmarks_toolbar_clean (wrhm);

	bms = ephy_bookmarks_get_bookmarks (p->bookmarks);
	topics = ephy_bookmarks_get_keywords (p->bookmarks);

	xml = g_string_new (NULL);
	g_string_append (xml, "<Root><dockitem name=\"BookmarksToolbar\">");

	p->action_group = egg_action_group_new ("BookmarksToolbar");
	egg_menu_merge_insert_action_group (merge, p->action_group, 0);

	children = ephy_node_get_children (topics);
	for (i = 0; i < children->len; i++)
	{
		gulong id;
		char *verb;
		EphyNode *child;
		EggAction *action;
		gboolean in_toolbar;

		child = g_ptr_array_index (children, i);

		in_toolbar = ephy_node_get_property_boolean
			(child, EPHY_NODE_BMK_PROP_SHOW_IN_TOOLBAR);
		if (!in_toolbar) continue;

		id = ephy_node_get_id (child);
		verb = g_strdup_printf ("GoBookmark%ld", id);

		action = ephy_topic_action_new (verb, id);
		egg_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		g_signal_connect (action, "go_location",
				  G_CALLBACK (go_location_cb),
				  p->window);

		add_toolitem (verb, xml);

		g_free (verb);
	}
	ephy_node_thaw (topics);

	children = ephy_node_get_children (bms);
	for (i = 0; i < children->len; i++)
	{
		gulong id;
		char *verb;
		EphyNode *child;
		EggAction *action;
		gboolean in_toolbar;

		child = g_ptr_array_index (children, i);

		in_toolbar = ephy_node_get_property_boolean
			(child, EPHY_NODE_BMK_PROP_SHOW_IN_TOOLBAR);
		if (!in_toolbar) continue;

		id = ephy_node_get_id (child);
		verb = g_strdup_printf ("GoBookmark%ld", id);

		action = ephy_bookmark_action_new (verb, id);
		egg_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		g_signal_connect (action, "go_location",
				  G_CALLBACK (go_location_cb),
				  p->window);

		add_toolitem (verb, xml);

		g_free (verb);
	}
	ephy_node_thaw (bms);

	g_string_append (xml, "</dockitem></Root>");

	LOG ("Merging ui\n%s",xml->str);
	p->ui_id = egg_menu_merge_add_ui_from_string
		(merge, xml->str, -1, &error);

	egg_menu_merge_ensure_update (merge);

	g_string_free (xml, TRUE);
}

void
ephy_bookmarks_toolbar_update (EphyBookmarksToolbar *wrhm)
{
	ephy_bookmarks_toolbar_rebuild (wrhm);
}

void
ephy_bookmarks_toolbar_show (EphyBookmarksToolbar *wrhm)
{
	GtkWidget *widget;
	EggMenuMerge *merge = EGG_MENU_MERGE (wrhm->priv->window->ui_merge);

	widget = egg_menu_merge_get_widget (merge, "/BookmarksToolbar");
	gtk_widget_show (widget);
}

void
ephy_bookmarks_toolbar_hide (EphyBookmarksToolbar *wrhm)
{
	GtkWidget *widget;
	EggMenuMerge *merge = EGG_MENU_MERGE (wrhm->priv->window->ui_merge);

	widget = egg_menu_merge_get_widget (merge, "/BookmarksToolbar");
	gtk_widget_hide (widget);
}
