/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
 *            (C) 2001, 2002 Jorn Baayen
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

#include "toolbar.h"
#include "egg-menu-merge.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-location-entry.h"
#include "ephy-dnd.h"
#include "ephy-spinner.h"
#include "ephy-spinner-action.h"
#include "ephy-location-action.h"
#include "ephy-favicon-action.h"
#include "ephy-navigation-action.h"
#include "ephy-bookmark-action.h"
#include "window-commands.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "ephy-new-bookmark.h"
#include "ephy-toolbars-group.h"

#include <string.h>

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


enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static GObjectClass *parent_class = NULL;

struct ToolbarPrivate
{
	EphyWindow *window;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
	gboolean visibility;
	GtkWidget *location_entry;
	GtkWidget *spinner;
	GtkWidget *favicon;
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

                toolbar_type = g_type_register_static (EPHY_EDITABLE_TOOLBAR_TYPE,
						       "Toolbar",
						       &our_info, 0);
        }

        return toolbar_type;

}

static void
go_location_cb (EggAction *action, char *location, EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_load_url (embed, location);
}

static EggAction *
get_bookmark_action (Toolbar *t, EphyBookmarks *bookmarks, gulong id, const char *action_name)
{
	EggAction *action;

	LOG ("Creating action for bookmark id %ld", id)

	action = ephy_bookmark_action_new (action_name, id);

	g_signal_connect (action, "go_location",
			  G_CALLBACK (go_location_cb), t->priv->window);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	return action;
}

static EggAction *
toolbar_get_action (EphyEditableToolbar *etoolbar,
		    const char *type,
		    const char *name)
{
	Toolbar *t = TOOLBAR (etoolbar);
	EggAction *action = NULL;
	EphyBookmarks *bookmarks;
	gulong id = 0;
	char action_name[255];

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	if (type && (strcmp (type, EPHY_DND_URL_TYPE) == 0))
	{
		GtkWidget *new_bookmark;
		const char *url;
		const char *title = NULL;
		GList *uris;

		uris = ephy_dnd_uri_list_extract_uris (name);
		g_return_val_if_fail (uris != NULL, NULL);
		url = (const char *)uris->data;
		if (uris->next)
		{
			title = (const char *)uris->next->data;
		}

		LOG ("Action for bookmark -%s-. EphyBookmarks %p", url, bookmarks)
		id = ephy_bookmarks_get_bookmark_id (bookmarks, url);

		if (id == 0)
		{
			new_bookmark = ephy_new_bookmark_new
				(bookmarks, GTK_WINDOW (t->priv->window), url);
			ephy_new_bookmark_set_title (EPHY_NEW_BOOKMARK (new_bookmark),
						     title);
			gtk_dialog_run (GTK_DIALOG (new_bookmark));
			id = ephy_new_bookmark_get_id (EPHY_NEW_BOOKMARK (new_bookmark));
			gtk_widget_destroy (new_bookmark);
		}

		g_list_foreach (uris, (GFunc)g_free, NULL);
		g_list_free (uris);
	}
	else if (g_str_has_prefix (name, "GoBookmarkId"))
	{
		if (!ephy_str_to_int (name + strlen ("GoBookmarkId"), &id))
		{
			id = 0;
		}
	}

	if (id != 0)
	{
		snprintf (action_name, 255, "GoBookmarkId%ld", id);
		action = EPHY_EDITABLE_TOOLBAR_CLASS
			(parent_class)->get_action (etoolbar, NULL, action_name);
		if (action == NULL)
		{
			action = get_bookmark_action (t, bookmarks, id, action_name);
		}
	}
	else
	{
		action = EPHY_EDITABLE_TOOLBAR_CLASS
			(parent_class)->get_action (etoolbar, type, name);
	}

	return action;
}

static void
toolbar_class_init (ToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEditableToolbarClass *eet_class;

        parent_class = g_type_class_peek_parent (klass);
	eet_class = EPHY_EDITABLE_TOOLBAR_CLASS (klass);

        object_class->finalize = toolbar_finalize;
	object_class->set_property = toolbar_set_property;
	object_class->get_property = toolbar_get_property;

	eet_class->get_action = toolbar_get_action;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
toolbar_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        Toolbar *t = TOOLBAR (object);

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
        Toolbar *t = TOOLBAR (object);

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
	EggAction *action;

	t->priv->action_group = egg_action_group_new ("SpecialToolbarActions");

	action = g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			       "name", "NavigationBack",
			       "label", _("Back"),
			       "stock_id", GTK_STOCK_GO_BACK,
			       "window", t->priv->window,
			       "direction", EPHY_NAVIGATION_DIRECTION_BACK,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_go_back), t->priv->window);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			       "name", "NavigationForward",
			       "label", _("Forward"),
			       "stock_id", GTK_STOCK_GO_FORWARD,
			       "window", t->priv->window,
			       "direction", EPHY_NAVIGATION_DIRECTION_FORWARD,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_go_forward), t->priv->window);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_NAVIGATION_ACTION,
			       "name", "NavigationUp",
			       "label", _("Up"),
			       "window", t->priv->window,
			       "direction", EPHY_NAVIGATION_DIRECTION_UP,
			       "stock_id", GTK_STOCK_GO_UP,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (window_cmd_go_up), t->priv->window);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_SPINNER_ACTION,
			       "name", "Spinner",
			       "label", "Spinner",
			       NULL);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_LOCATION_ACTION,
			       "name", "Location",
			       "label", "Location",
			       NULL);
	g_signal_connect (action, "go_location",
			  G_CALLBACK (go_location_cb), t->priv->window);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_FAVICON_ACTION,
			       "name", "Favicon",
			       "label", "Favicon",
			       "window", t->priv->window,
			       NULL);
	egg_action_group_add_action (t->priv->action_group, action);
	g_object_unref (action);
}

static void
toolbar_set_window (Toolbar *t, EphyWindow *window)
{
	g_return_if_fail (t->priv->window == NULL);

	t->priv->window = window;
	t->priv->ui_merge = EGG_MENU_MERGE (window->ui_merge);

	toolbar_setup_actions (t);
	egg_menu_merge_insert_action_group (t->priv->ui_merge,
					    t->priv->action_group, 1);
	g_object_set (t, "MenuMerge", t->priv->ui_merge, NULL);
}

static void
toolbar_init (Toolbar *t)
{
	static EphyToolbarsGroup *group = NULL;

        t->priv = g_new0 (ToolbarPrivate, 1);

	t->priv->window = NULL;
	t->priv->ui_merge = NULL;
	t->priv->visibility = TRUE;

	if (group == NULL)
	{
		char *user;

		user = g_build_filename (ephy_dot_dir (), "toolbar.xml", NULL);
		group = ephy_toolbars_group_new ();
		ephy_toolbars_group_set_source
			(group, ephy_file ("epiphany-toolbar.xml"), user);
		g_free (user);
	}

	g_object_set (t, "ToolbarsGroup", group, NULL);
}

static void
toolbar_finalize (GObject *object)
{
	Toolbar *t;
	ToolbarPrivate *p;
	EggMenuMerge *merge;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_TOOLBAR (object));

	t = TOOLBAR (object);
	p = t->priv;
	merge = EGG_MENU_MERGE (t->priv->window->ui_merge);

        g_return_if_fail (p != NULL);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	g_object_unref (t->priv->action_group);
	egg_menu_merge_remove_action_group (merge, t->priv->action_group);

        g_free (t->priv);

	LOG ("Toolbar finalized")
}

Toolbar *
toolbar_new (EphyWindow *window)
{
	Toolbar *t;

	t = TOOLBAR (g_object_new (TOOLBAR_TYPE,
				   "EphyWindow", window,
				   NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

void
toolbar_activate_location (Toolbar *t)
{
	EggAction *action;
	GtkWidget *location;

	action = egg_action_group_get_action
		(t->priv->action_group, "Location");
	location = ephy_location_action_get_widget
		(EPHY_LOCATION_ACTION (action));
	g_return_if_fail (location != NULL);

	ephy_location_entry_activate
		(EPHY_LOCATION_ENTRY(location));
}

void
toolbar_spinner_start (Toolbar *t)
{
	EggActionGroup *action_group;
	EggAction *action;

	action_group = t->priv->action_group;
	action = egg_action_group_get_action (action_group, "Spinner");
	g_object_set (action, "throbbing", TRUE, NULL);
}

void
toolbar_spinner_stop (Toolbar *t)
{
	EggActionGroup *action_group;
	EggAction *action;

	action_group = t->priv->action_group;
	action = egg_action_group_get_action (action_group, "Spinner");
	g_object_set (action, "throbbing", FALSE, NULL);
}

void
toolbar_set_location (Toolbar *t,
		      const char *alocation)
{
	EggAction *action;
	GtkWidget *location;

	action = egg_action_group_get_action
		(t->priv->action_group, "Location");
	location = ephy_location_action_get_widget
		(EPHY_LOCATION_ACTION (action));
	g_return_if_fail (location != NULL);

	ephy_location_entry_set_location
		(EPHY_LOCATION_ENTRY (location), alocation);
}

void
toolbar_update_favicon (Toolbar *t)
{
	EphyTab *tab;
	const char *url;
	EggActionGroup *action_group;
	EggAction *action;

	tab = ephy_window_get_active_tab (t->priv->window);
	url = ephy_tab_get_favicon_url (tab);
	action_group = t->priv->action_group;
	action = egg_action_group_get_action (action_group, "Favicon");
	g_object_set (action, "icon", url, NULL);
}

char *
toolbar_get_location (Toolbar *t)
{
	EggAction *action;
	GtkWidget *location;

	action = egg_action_group_get_action
		(t->priv->action_group, "Location");
	location = ephy_location_action_get_widget
		(EPHY_LOCATION_ACTION (action));
	g_return_val_if_fail (location != NULL, NULL);

	return ephy_location_entry_get_location
		(EPHY_LOCATION_ENTRY (location));
}

void
toolbar_clear_location_history (Toolbar *t)
{
	EggAction *action;
	GtkWidget *location;

	action = egg_action_group_get_action
		(t->priv->action_group, "Location");
	location = ephy_location_action_get_widget
		(EPHY_LOCATION_ACTION (action));
	g_return_if_fail (location != NULL);

	ephy_location_entry_clear_history (EPHY_LOCATION_ENTRY (location));
}

void
toolbar_update_navigation_actions (Toolbar *t, gboolean back, gboolean forward, gboolean up)
{
	EggActionGroup *action_group;
	EggAction *action;

	action_group = t->priv->action_group;
	action = egg_action_group_get_action (action_group, "NavigationBack");
	g_object_set (action, "sensitive", !back, NULL);
	action = egg_action_group_get_action (action_group, "NavigationForward");
	g_object_set (action, "sensitive", !forward, NULL);
	action = egg_action_group_get_action (action_group, "NavigationUp");
	g_object_set (action, "sensitive", !up, NULL);
}

