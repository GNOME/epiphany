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
#include "ephy-string.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkaction.h>
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
ephy_encoding_menu_set_window (EphyEncodingMenu *menu, EphyWindow *window)
{
	EphyEmbedSingle *single;
	GtkActionGroup *action_group;
	GList *encodings, *groups, *l;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	menu->priv->window = window;
	menu->priv->manager = GTK_UI_MANAGER (window->ui_merge);

	action_group = gtk_action_group_new ("EncodingActions");
	menu->priv->action_group = action_group;

	single = ephy_embed_shell_get_embed_single (EPHY_EMBED_SHELL (ephy_shell));
	g_return_if_fail (single != NULL);

	ephy_embed_single_get_encodings (single, LG_ALL, FALSE, &encodings);

	for (l = encodings; l != NULL; l = l->next)
	{
		const EncodingInfo *info = (EncodingInfo *) l->data;
		GtkAction *action;
		char name[32];

		g_snprintf (name, 32, "Encoding%s", info->encoding);
		action = g_object_new (GTK_TYPE_ACTION,
				       "name", name,
				       "label", info->title,
				       NULL);

		g_signal_connect (action, "activate",
				  G_CALLBACK (ephy_encoding_menu_verb_cb),
				  menu);
	
		gtk_action_group_add_action (menu->priv->action_group, action);
		g_object_unref (action);
	}
	
	g_list_foreach (encodings, (GFunc) encoding_info_free, NULL);
	g_list_free (encodings);

	ephy_embed_single_get_language_groups (single, &groups);

	for (l = groups; l != NULL; l = l->next)
	{
		const LanguageGroupInfo *info = (LanguageGroupInfo *) l->data;
		GtkAction *action;
		char name[32];

		g_snprintf (name, 32, "EncodingGroup%d", info->group);
	
		action = g_object_new (GTK_TYPE_ACTION,
				       "name", name,
				       "label", info->title,
				       NULL);
		gtk_action_group_add_action (menu->priv->action_group, action);
		g_object_unref (action);
	}

	g_list_foreach (groups, (GFunc) language_group_info_free, NULL);
	g_list_free (groups);

	gtk_ui_manager_insert_action_group (menu->priv->manager,
					    action_group, 0);
	g_object_unref (action_group);

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
	EphyEmbedSingle *single;
	EphyEncodingMenuPrivate *p = menu->priv;
	GList *encodings, *groups, *l;

	single = ephy_embed_shell_get_embed_single (EPHY_EMBED_SHELL (ephy_shell));
	ephy_embed_single_get_language_groups (single, &groups);

	if (p->merge_id > 0)
	{
		gtk_ui_manager_remove_ui (p->manager, p->merge_id);
		gtk_ui_manager_ensure_update (p->manager);
	}

	p->merge_id = gtk_ui_manager_new_merge_id (p->manager);

	gtk_ui_manager_add_ui (p->manager, p->merge_id, ENCODING_PLACEHOLDER_PATH,
			       "ViewEncodingMenu", "ViewEncoding",
			       GTK_UI_MANAGER_MENU, FALSE);

	for (l = groups; l != NULL; l = l->next)
	{
		const LanguageGroupInfo *info = (LanguageGroupInfo *) l->data;
		char name[32], action[36], path[128];
		GList *enc;

		g_snprintf (action, 32, "EncodingGroup%d", info->group);
		g_snprintf (name, 36, "%sMenu", action);
		g_snprintf (path, 128, "%s/%s", ENCODING_MENU_PATH, name);

		gtk_ui_manager_add_ui (p->manager, p->merge_id,
				       ENCODING_MENU_PATH,
				       name, action,
				       GTK_UI_MANAGER_MENU, FALSE);

		ephy_embed_single_get_encodings (single, info->group,
						 FALSE, &encodings);

		for (enc = encodings; enc != NULL; enc = enc->next)
		{
			const EncodingInfo *info = (EncodingInfo *) enc->data;

			g_snprintf (action, 32, "Encoding%s", info->encoding);
			g_snprintf (name, 36, "%sItem", action);

			gtk_ui_manager_add_ui (p->manager, p->merge_id, path,
					       name, action,
					       GTK_UI_MANAGER_MENUITEM, FALSE);
		}

		g_list_foreach (encodings, (GFunc) encoding_info_free, NULL);
		g_list_free (encodings);
	}

	g_list_foreach (groups, (GFunc) language_group_info_free, NULL);
	g_list_free (groups);
}
