/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003  Marco Pesenti Gritti
 *  Copyright © 2003  Christian Persch
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
#include "ephy-encoding-menu.h"

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-encoding-dialog.h"
#include "ephy-encodings.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define EPHY_ENCODING_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODING_MENU, EphyEncodingMenuPrivate))

struct _EphyEncodingMenuPrivate
{
	EphyEncodings *encodings;
	EphyWindow *window;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	gboolean update_tag;
	guint merge_id;
	GSList *encodings_radio_group;
	EphyEncodingDialog *dialog;
};

#define ENCODING_PLACEHOLDER_PATH	"/ui/PagePopup/ViewEncodingMenu/ViewEncodingPlaceholder"

static void	ephy_encoding_menu_class_init	  (EphyEncodingMenuClass *klass);
static void	ephy_encoding_menu_init		  (EphyEncodingMenu *menu);

enum
{
	PROP_0,
	PROP_WINDOW
};

G_DEFINE_TYPE (EphyEncodingMenu, ephy_encoding_menu, G_TYPE_OBJECT)

static void
ephy_encoding_menu_init (EphyEncodingMenu *menu)
{
	menu->priv = EPHY_ENCODING_MENU_GET_PRIVATE (menu);

	menu->priv->encodings =
		EPHY_ENCODINGS (ephy_embed_shell_get_encodings
				(EPHY_EMBED_SHELL (ephy_shell_get_default ())));
}

static int
sort_encodings (gconstpointer a, gconstpointer b)
{
	EphyEncoding *enc1 = (EphyEncoding*)a;
	EphyEncoding *enc2 = (EphyEncoding*)b;
	const char *key1, *key2;

	key1 = ephy_encoding_get_collation_key (enc1);
	key2 = ephy_encoding_get_collation_key (enc2);

	return strcmp (key1, key2);
}

static void
add_menu_item (EphyEncoding *encoding, EphyEncodingMenu *menu)
{
	const char *code;
	char action[128], name[128];

	code = ephy_encoding_get_encoding (encoding);

	g_snprintf (action, sizeof (action), "Encoding%s", code);
	g_snprintf (name, sizeof (name), "%sItem", action);

	gtk_ui_manager_add_ui (menu->priv->manager, menu->priv->merge_id,
			       ENCODING_PLACEHOLDER_PATH,
			       name, action,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
}

static void
update_encoding_menu_cb (GtkAction *dummy, EphyEncodingMenu *menu)
{
	EphyEncodingMenuPrivate *p = menu->priv;
	EphyEmbed *embed;
	GtkAction *action;
	char name[128];
	const char *encoding;
	EphyEncoding *enc_node;
	GList *recent, *related = NULL, *l;
	EphyLanguageGroup groups;
	gboolean is_automatic = FALSE;
	WebKitWebView *view;

	START_PROFILER ("Rebuilding encoding menu")

	/* FIXME: block the "activate" signal on the actions instead; needs to 
	 * wait until g_signal_handlers_block_matched supports blocking
	 * by signal id alone.
	 */
	menu->priv->update_tag = TRUE;

	/* get most recently used encodings */
	recent = ephy_encodings_get_recent (p->encodings);

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->window));
	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	encoding = webkit_web_view_get_custom_charset (view);
	if (encoding == NULL) goto build_menu;

	enc_node = ephy_encodings_get_encoding (p->encodings, encoding, TRUE);
	g_assert (EPHY_IS_ENCODING (enc_node));

	action = gtk_action_group_get_action (p->action_group,
					      "ViewEncodingAutomatic");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), is_automatic);
	gtk_action_set_sensitive (action, !is_automatic);

	/* set the encodings group's active member */
	g_snprintf (name, sizeof (name), "Encoding%s", encoding);
	action = gtk_action_group_get_action (p->action_group, name);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	/* get encodings related to the current encoding */
	groups = ephy_encoding_get_language_groups (enc_node);

	related = ephy_encodings_get_encodings (p->encodings, groups);
	related = g_list_sort (related, (GCompareFunc)sort_encodings);

	/* add the current encoding to the list of
	 * things to display, making sure we don't add it more than once
	 */
	if (g_list_find (related, enc_node) == NULL
	    && g_list_find (recent, enc_node) == NULL)
	{
		related = g_list_prepend (related, enc_node);
	}

	/* make sure related and recent are disjoint so we don't display twice */
	for (l = related; l != NULL; l = l->next)
	{
		recent = g_list_remove (recent, l->data);
	}

	recent = g_list_sort (recent, (GCompareFunc)sort_encodings);

build_menu:
	/* clear the menu */
	if (p->merge_id > 0)
	{
		gtk_ui_manager_remove_ui (p->manager, p->merge_id);
		gtk_ui_manager_ensure_update (p->manager);
	}

	/* build the new menu */
	p->merge_id = gtk_ui_manager_new_merge_id (p->manager);

	gtk_ui_manager_add_ui (p->manager, p->merge_id,
			       ENCODING_PLACEHOLDER_PATH,
			       "ViewEncodingAutomaticItem",
			       "ViewEncodingAutomatic",
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	gtk_ui_manager_add_ui (p->manager, p->merge_id,
			       ENCODING_PLACEHOLDER_PATH,
			       "Sep1Item", "Sep1",
			       GTK_UI_MANAGER_SEPARATOR, FALSE);

	g_list_foreach (recent, (GFunc)add_menu_item, menu);

	gtk_ui_manager_add_ui (p->manager, p->merge_id,
			       ENCODING_PLACEHOLDER_PATH,
			       "Sep2Item", "Sep2",
			       GTK_UI_MANAGER_SEPARATOR, FALSE);

	g_list_foreach (related, (GFunc)add_menu_item, menu);

	gtk_ui_manager_add_ui (p->manager, p->merge_id,
			       ENCODING_PLACEHOLDER_PATH,
			       "Sep3Item", "Sep3",
			       GTK_UI_MANAGER_SEPARATOR, FALSE);

	gtk_ui_manager_add_ui (p->manager, p->merge_id,
			       ENCODING_PLACEHOLDER_PATH,
			       "ViewEncodingOtherItem",
			       "ViewEncodingOther",
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	/* cleanup */
	g_list_free (related);
	g_list_free (recent);

	menu->priv->update_tag = FALSE;

	STOP_PROFILER ("Rebuilding encoding menu")
}

static void
encoding_activate_cb (GtkAction *action, EphyEncodingMenu *menu)
{
	EphyEmbed *embed;
	WebKitWebView *view;
	const char *name, *encoding;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) == FALSE
	    || menu->priv->update_tag)
	{
		return;
	}

	name = gtk_action_get_name (GTK_ACTION (action));
	encoding = name + strlen("Encoding");

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (menu->priv->window));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	webkit_web_view_set_custom_charset (view, encoding);

	ephy_encodings_add_recent (menu->priv->encodings, encoding);
}

static void
add_action (EphyEncodings *encodings, EphyEncoding *encoding, EphyEncodingMenu *menu)
{
	GtkAction *action;
	char name[128] = "";
	const char *encoding_str, *title;

	encoding_str = ephy_encoding_get_encoding (encoding);
	title = ephy_encoding_get_title (encoding);

	LOG ("add_action for encoding '%s'", encoding_str);

	g_snprintf (name, sizeof (name), "Encoding%s", encoding_str);

	action = g_object_new (GTK_TYPE_RADIO_ACTION,
			       "name", name,
			       "label", title,
			       NULL);

	gtk_radio_action_set_group (GTK_RADIO_ACTION (action),
				    menu->priv->encodings_radio_group);
	menu->priv->encodings_radio_group = gtk_radio_action_get_group
						(GTK_RADIO_ACTION (action));

	g_signal_connect (action, "activate",
			  G_CALLBACK (encoding_activate_cb),
			  menu);

	gtk_action_group_add_action_with_accel
		(menu->priv->action_group, action, NULL);
	g_object_unref (action);
}

static void
ephy_encoding_menu_view_dialog_cb (GtkAction *action, EphyEncodingMenu *menu)
{
	if (menu->priv->dialog == NULL)
	{
		EphyEncodingDialog **dialog = &menu->priv->dialog;
		menu->priv->dialog = ephy_encoding_dialog_new
					(menu->priv->window);

		g_object_add_weak_pointer(G_OBJECT (menu->priv->dialog),
					  (gpointer *)dialog);
	}

	ephy_dialog_show (EPHY_DIALOG (menu->priv->dialog));
}

static void
ephy_encoding_menu_automatic_cb (GtkAction *action, EphyEncodingMenu *menu)
{
	EphyEmbed *embed;
	WebKitWebView *view;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) == FALSE
	    || menu->priv->update_tag)
	{
		return;
	}

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (menu->priv->window));

	/* setting NULL will clear the forced encoding */
	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	webkit_web_view_set_custom_charset (view, NULL);
}

static const GtkActionEntry menu_entries [] =
{
	{ "ViewEncodingOther", NULL, N_("_Other…"), NULL,
	  N_("Other encodings"),
	  G_CALLBACK (ephy_encoding_menu_view_dialog_cb) }
};

static const GtkToggleActionEntry toggle_menu_entries [] =
{
	{ "ViewEncodingAutomatic", NULL, N_("_Automatic"), NULL,
	  N_("Use the encoding specified by the document"),
	  G_CALLBACK (ephy_encoding_menu_automatic_cb), FALSE }
};

static void
ephy_encoding_menu_set_window (EphyEncodingMenu *menu, EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *encodings, *p;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	menu->priv->window = window;
	menu->priv->manager = ephy_window_get_ui_manager (window);

	action_group = gtk_action_group_new ("EncodingActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	menu->priv->action_group = action_group;

	gtk_action_group_add_actions (action_group, menu_entries,
				      G_N_ELEMENTS (menu_entries), menu);
	gtk_action_group_add_toggle_actions (action_group, toggle_menu_entries,
                                    	     G_N_ELEMENTS (toggle_menu_entries), menu);

	/* add actions for the existing encodings */
	encodings = ephy_encodings_get_all (menu->priv->encodings);
	for (p = encodings; p; p = p->next)
	{
		EphyEncoding *encoding;

		encoding = (EphyEncoding *)p->data;
		add_action (menu->priv->encodings, encoding, menu);
	}
	g_list_free (encodings);

	/* When we encounter an unknown encoding, it is added to the
	 * database, so we need to listen to child_added on the
	 * encodings node to add an action for it.
	 */
	g_signal_connect (menu->priv->encodings, "encoding-added",
			  G_CALLBACK (add_action), menu);

	gtk_ui_manager_insert_action_group (menu->priv->manager,
					    action_group, 0);
	g_object_unref (action_group);

	action = gtk_ui_manager_get_action (menu->priv->manager,
					    "/ui/PagePopup/ViewEncodingMenu");
	g_signal_connect_object (action, "activate",
				 G_CALLBACK (update_encoding_menu_cb),
				 menu, 0);
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
ephy_encoding_menu_finalize (GObject *object)
{
	EphyEncodingMenu *menu = EPHY_ENCODING_MENU (object); 

	if (menu->priv->dialog)
	{
		g_object_unref (menu->priv->dialog);
	}

	G_OBJECT_CLASS (ephy_encoding_menu_parent_class)->finalize (object);
}

static void
ephy_encoding_menu_class_init (EphyEncodingMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_encoding_menu_finalize;
	object_class->set_property = ephy_encoding_menu_set_property;
	object_class->get_property = ephy_encoding_menu_get_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
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
