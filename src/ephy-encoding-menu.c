/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
 *  Copyright (C) 2003  Marco Pesenti Gritti
 *  Copyright (C) 2003  Christian Persch
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

#include "ephy-encoding-menu.h"
#include "ephy-langs.h"
#include "ephy-encodings.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkaction.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkradioaction.h>
#include <gtk/gtkuimanager.h>
#include <string.h>

#define EPHY_ENCODING_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODING_MENU, EphyEncodingMenuPrivate))

struct _EphyEncodingMenuPrivate
{
	EphyWindow *window;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	gboolean update_tag;
	guint merge_id;
};

#define ENCODING_PLACEHOLDER_PATH	"/menubar/ViewMenu/ViewEncodingsPlaceholder"
#define ENCODING_MENU_PATH		"/menubar/ViewMenu/ViewEncodingsPlaceholder/ViewEncodingMenu"

static void	ephy_encoding_menu_class_init	(EphyEncodingMenuClass *klass);
static void	ephy_encoding_menu_init		(EphyEncodingMenu *menu);
static void	ephy_encoding_menu_rebuild	(EphyEncodingMenu *menu);

enum
{
	PROP_0,
	PROP_WINDOW
};

static GObjectClass *parent_class = NULL;

GType
ephy_encoding_menu_get_type (void)
{
	static GType ephy_encoding_menu_type = 0;

	if (ephy_encoding_menu_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEncodingMenuClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_encoding_menu_class_init,
			NULL,
			NULL,
			sizeof (EphyEncodingMenu),
			0,
			(GInstanceInitFunc) ephy_encoding_menu_init
		};

		ephy_encoding_menu_type = g_type_register_static (G_TYPE_OBJECT,
								  "EphyEncodingMenu",
								  &our_info, 0);
	}

	return ephy_encoding_menu_type;
}

static void
ephy_encoding_menu_verb_cb (GtkAction *action,
			    EphyEncodingMenu *menu)
{
	EphyEmbed *embed;
	const char *encoding;
	const char *action_name;

	/* do nothing if this action was _de_activated, or if we're updating
	 * the menu, i.e. setting the active encoding from the document
	 */
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) == FALSE
	    || menu->priv->update_tag)
	{
		return;
	}

	embed = ephy_window_get_active_embed (menu->priv->window);
	g_return_if_fail (embed != NULL);

	action_name = gtk_action_get_name (action);

	if (strncmp (action_name, "Encoding", 8) == 0)
	{
		encoding = action_name + 8;

		LOG ("Switching to encoding %s", encoding)

		ephy_embed_set_encoding (embed, encoding);
	}
}

static void
ephy_encoding_menu_init (EphyEncodingMenu *menu)
{
	menu->priv = EPHY_ENCODING_MENU_GET_PRIVATE (menu);

	menu->priv->update_tag = FALSE;
	menu->priv->action_group = NULL;
	menu->priv->merge_id = 0;
}

static void
update_encoding_menu_cb (GtkAction *dummy, EphyEncodingMenu *menu)
{
	EphyEmbed *embed;
	GtkAction *action = NULL;
	char *encoding;

	embed = ephy_window_get_active_embed (menu->priv->window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_encoding (embed, &encoding);

	if (encoding != NULL)
	{
		char name[32];

		g_snprintf (name, 32, "Encoding%s", encoding);
		action = gtk_action_group_get_action (menu->priv->action_group,
						      name);
	}

	if (action != NULL)
	{
		/* FIXME: block the "activate" signal instead; needs to wait
		 * until g_signal_handlers_block_matched supports blocking
		 * by signal id alone.
		 */
		menu->priv->update_tag = TRUE;
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		menu->priv->update_tag = FALSE;
	}
	else
	{
		g_warning ("Could not find action for encoding '%s'!\n", encoding);
	}

	g_free (encoding);
}

static void
ephy_encoding_menu_set_window (EphyEncodingMenu *menu, EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *encodings, *groups, *l;
	GSList *radio_group = NULL;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	menu->priv->window = window;
	menu->priv->manager = GTK_UI_MANAGER (window->ui_merge);

	action_group = gtk_action_group_new ("EncodingActions");
	menu->priv->action_group = action_group;

	encodings = ephy_encodings_get_list (LG_ALL, FALSE);
	for (l = encodings; l != NULL; l = l->next)
	{
		const EphyEncodingInfo *info = (EphyEncodingInfo *) l->data;
		char name[32];

		g_snprintf (name, 32, "Encoding%s", info->encoding);
		action = g_object_new (GTK_TYPE_RADIO_ACTION,
				       "name", name,
				       "label", info->title,
				       NULL);

		gtk_radio_action_set_group (GTK_RADIO_ACTION (action), radio_group);
		radio_group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));

		g_signal_connect (action, "activate",
				  G_CALLBACK (ephy_encoding_menu_verb_cb),
				  menu);
	
		gtk_action_group_add_action (menu->priv->action_group, action);
		g_object_unref (action);
	}
	
	g_list_foreach (encodings, (GFunc) ephy_encoding_info_free, NULL);
	g_list_free (encodings);

	groups = ephy_lang_get_group_list ();
	for (l = groups; l != NULL; l = l->next)
	{
		const EphyLanguageGroupInfo *info = (EphyLanguageGroupInfo *) l->data;
		char name[32];

		g_snprintf (name, 32, "EncodingGroup%d", info->group);
	
		action = g_object_new (GTK_TYPE_ACTION,
				       "name", name,
				       "label", info->title,
				       NULL);
		gtk_action_group_add_action (menu->priv->action_group, action);
		g_object_unref (action);
	}

	g_list_foreach (groups, (GFunc) ephy_lang_group_info_free, NULL);
	g_list_free (groups);

	gtk_ui_manager_insert_action_group (menu->priv->manager,
					    action_group, 0);
	g_object_unref (action_group);

	action = gtk_ui_manager_get_action (menu->priv->manager,
					    "/menubar/ViewMenu");
	if (action != NULL)
	{
		g_signal_connect_object (action, "activate",
					 G_CALLBACK (update_encoding_menu_cb),
					 menu, 0);
	}

	ephy_encoding_menu_rebuild (menu);
}

static void
ephy_encoding_menu_set_property (GObject *object,
				 guint prop_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	EphyEncodingMenu *menu = EPHY_ENCODING_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			ephy_encoding_menu_set_window (menu, g_value_get_object (value));
			break;
	}
}

static void
ephy_encoding_menu_get_property (GObject *object,
				 guint prop_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	EphyEncodingMenu *menu = EPHY_ENCODING_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, menu->priv->window);
			break;
	}
}

static void
ephy_encoding_menu_class_init (EphyEncodingMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_encoding_menu_set_property;
	object_class->get_property = ephy_encoding_menu_get_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyEncodingMenuPrivate));
}

EphyEncodingMenu *
ephy_encoding_menu_new (EphyWindow *window)
{
	return g_object_new (EPHY_TYPE_ENCODING_MENU,
			     "window", window,
			     NULL);
}

static void
ephy_encoding_menu_rebuild (EphyEncodingMenu *menu)
{
	EphyEncodingMenuPrivate *p = menu->priv;
	GList *groups, *l;

	if (p->merge_id > 0)
	{
		gtk_ui_manager_remove_ui (p->manager, p->merge_id);
		gtk_ui_manager_ensure_update (p->manager);
	}

	p->merge_id = gtk_ui_manager_new_merge_id (p->manager);

	gtk_ui_manager_add_ui (p->manager, p->merge_id, ENCODING_PLACEHOLDER_PATH,
			       "ViewEncodingMenu", "ViewEncoding",
			       GTK_UI_MANAGER_MENU, FALSE);

	groups = ephy_lang_get_group_list ();
	for (l = groups; l != NULL; l = l->next)
	{
		const EphyLanguageGroupInfo *info = (EphyLanguageGroupInfo *) l->data;
		char name[32], action[36], path[128];
		GList *encodings, *enc;

		g_snprintf (action, 32, "EncodingGroup%d", info->group);
		g_snprintf (name, 36, "%sMenu", action);
		g_snprintf (path, 128, "%s/%s", ENCODING_MENU_PATH, name);

		gtk_ui_manager_add_ui (p->manager, p->merge_id,
				       ENCODING_MENU_PATH,
				       name, action,
				       GTK_UI_MANAGER_MENU, FALSE);

		encodings = ephy_encodings_get_list (info->group, FALSE);
		for (enc = encodings; enc != NULL; enc = enc->next)
		{
			const EphyEncodingInfo *info = (EphyEncodingInfo *) enc->data;

			g_snprintf (action, 32, "Encoding%s", info->encoding);
			g_snprintf (name, 36, "%sItem", action);

			gtk_ui_manager_add_ui (p->manager, p->merge_id, path,
					       name, action,
					       GTK_UI_MANAGER_MENUITEM, FALSE);
		}

		g_list_foreach (encodings, (GFunc) ephy_encoding_info_free, NULL);
		g_list_free (encodings);
	}

	g_list_foreach (groups, (GFunc) ephy_lang_group_info_free, NULL);
	g_list_free (groups);
}
