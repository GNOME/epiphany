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

#include "ephy-encoding-menu.h"
#include "ephy-string.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkuimanager.h>
#include <string.h>

/**
 * Private data
 */
struct _EphyEncodingMenuPrivate
{
	EphyWindow *window;
	GtkActionGroup *action_group;
};

/**
 * Private functions, only availble from this file
 */
static void	ephy_encoding_menu_class_init	  (EphyEncodingMenuClass *klass);
static void	ephy_encoding_menu_init	  (EphyEncodingMenu *wrhm);
static void	ephy_encoding_menu_finalize_impl (GObject *o);
static void	ephy_encoding_menu_rebuild	  (EphyEncodingMenu *wrhm);
static void     ephy_encoding_menu_set_property  (GObject *object,
						   guint prop_id,
						   const GValue *value,
						   GParamSpec *pspec);
static void	ephy_encoding_menu_get_property  (GObject *object,
						   guint prop_id,
						   GValue *value,
						   GParamSpec *pspec);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static gpointer g_object_class;

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
ephy_encoding_menu_class_init (EphyEncodingMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = ephy_encoding_menu_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_encoding_menu_set_property;
	object_class->get_property = ephy_encoding_menu_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
ephy_encoding_menu_init (EphyEncodingMenu *wrhm)
{
	EphyEncodingMenuPrivate *p = g_new0 (EphyEncodingMenuPrivate, 1);
	wrhm->priv = p;

	wrhm->priv->action_group = NULL;
}

static void
ephy_encoding_menu_finalize_impl (GObject *o)
{
	EphyEncodingMenu *wrhm = EPHY_ENCODING_MENU (o);
	EphyEncodingMenuPrivate *p = wrhm->priv;

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group
			(GTK_UI_MANAGER (p->window->ui_merge),
			 p->action_group);
		g_object_unref (p->action_group);
	}

	g_free (p);

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}

static void
ephy_encoding_menu_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        EphyEncodingMenu *m = EPHY_ENCODING_MENU (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        m->priv->window = g_value_get_object (value);
			ephy_encoding_menu_rebuild (m);
                        break;
        }
}

static void
ephy_encoding_menu_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
        EphyEncodingMenu *m = EPHY_ENCODING_MENU (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, m->priv->window);
                        break;
        }
}

EphyEncodingMenu *
ephy_encoding_menu_new (EphyWindow *window)
{
	EphyEncodingMenu *ret = g_object_new (EPHY_TYPE_ENCODING_MENU,
					       "EphyWindow", window,
					       NULL);
	return ret;
}

static void
ephy_encoding_menu_verb_cb (GtkAction *action,
			    EphyEncodingMenu *menu)
{
	EphyWindow *window;
	EphyEmbed *embed;
	const char *encoding;
	const char *action_name;

	window = menu->priv->window;

	embed = ephy_window_get_active_embed (window);
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
build_group (GtkActionGroup *action_group,
	     GString *xml_string,
	     const LanguageGroupInfo *info)
{
	gchar *tmp;
	gchar *verb;
	GtkAction *action;

	verb = g_strdup_printf ("EncodingGroup%d", info->group);

	action = g_object_new (GTK_TYPE_ACTION,
			       "name", verb,
			       "label", info->title,
			       NULL);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	tmp = g_strdup_printf ("<menu name=\"%sItem\" action=\"%s\">\n",
			       verb, verb);
	xml_string = g_string_append (xml_string, tmp);
	g_free (tmp);
	g_free (verb);
}

static void
build_encoding (EphyEncodingMenu *menu,
		GtkActionGroup *action_group,
		GString *xml_string,
		const EncodingInfo *info)
{
	char *tmp;
	char *verb;
	GtkAction *action;

	verb = g_strdup_printf ("Encoding%s", info->encoding);
	action = g_object_new (GTK_TYPE_ACTION,
			       "name", verb,
			       "label", info->title,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (ephy_encoding_menu_verb_cb),
			  menu);

	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	tmp = g_strdup_printf ("<menuitem name=\"%sItem\" action=\"%s\"/>\n",
			       verb, verb);
	xml_string = g_string_append (xml_string, tmp);

	g_free (tmp);
	g_free (verb);
}

static void
ephy_encoding_menu_rebuild (EphyEncodingMenu *wrhm)
{
	EphyEmbedSingle *single;
	EphyEncodingMenuPrivate *p = wrhm->priv;
	GtkUIManager *merge = GTK_UI_MANAGER (p->window->ui_merge);
	GString *xml;
	GList *groups, *lg, *encodings, *enc;

	p->action_group = NULL;

	LOG ("Rebuilding encoding menu")

	single = ephy_embed_shell_get_embed_single (EPHY_EMBED_SHELL (ephy_shell));
	g_return_if_fail (single != NULL);

	ephy_embed_single_get_language_groups (single, &groups);

	xml = g_string_new (NULL);
	g_string_append (xml, "<ui><menubar><menu name=\"ViewMenu\">"
			      "<placeholder name=\"ViewEncodingsPlaceholder\">"
			      "<menu name=\"ViewEncodingMenu\" action=\"ViewEncoding\">");

	p->action_group = gtk_action_group_new ("EncodingActions");
	gtk_ui_manager_insert_action_group (merge, p->action_group, 0);

	for (lg = groups; lg != NULL; lg = lg->next)
        {
		const LanguageGroupInfo *lang_info = (LanguageGroupInfo *) lg->data;

		build_group (p->action_group, xml, lang_info);

		ephy_embed_single_get_encodings (single, lang_info->group,
						FALSE, &encodings);

		for (enc = encodings; enc != NULL; enc = enc->next)
                {
			const EncodingInfo *info = (EncodingInfo *) enc->data;

			build_encoding (wrhm, p->action_group, xml, info);
		}

		g_list_foreach (encodings, (GFunc) encoding_info_free, NULL);
		g_list_free (encodings);
		
		g_string_append (xml, "</menu>");
	}

	g_list_foreach (groups, (GFunc) language_group_info_free, NULL);
	g_list_free (groups);

	g_string_append (xml, "</menu></placeholder></menu></menubar></ui>");

	gtk_ui_manager_add_ui_from_string (merge, xml->str, -1, NULL);

	g_string_free (xml, TRUE);
}
