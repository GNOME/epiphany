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
#include "ephy-gobject-misc.h"
#include "ephy-string.h"
#include "egg-menu-merge.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

/**
 * Private data
 */
struct _EphyEncodingMenuPrivate
{
	EphyWindow *window;
	EggActionGroup *action_group;
};

typedef struct
{
	EphyWindow *window;
	const char *encoding;
} EncodingData;

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

/**
 * EphyEncodingMenu object
 */
MAKE_GET_TYPE (ephy_encoding_menu,
	       "EphyEncodingMenu", EphyEncodingMenu,
	       ephy_encoding_menu_class_init, ephy_encoding_menu_init,
	       G_TYPE_OBJECT);

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
		egg_menu_merge_remove_action_group
			(EGG_MENU_MERGE (p->window->ui_merge),
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
ephy_encoding_menu_verb_cb (EggMenuMerge *merge,
			    EncodingData *data)
{
	EphyWindow *window = data->window;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_set_charset (embed, data->encoding);
}

static void
build_group (EggActionGroup *action_group, GString *xml_string, const char *group, int index)
{
	char *tmp;
	char *verb;
	EggAction *action;

	verb = g_strdup_printf ("CharsetGroup%d", index);

	action = g_object_new (EGG_TYPE_ACTION,
			       "name", verb,
			       "label", group,
			       NULL);
	egg_action_group_add_action (action_group, action);
	g_object_unref (action);

	tmp = g_strdup_printf ("<submenu name=\"CharsetGroup%dItem\" name=\"%s\">\n",
			       index, verb);
	xml_string = g_string_append (xml_string, tmp);
	g_free (tmp);
	g_free (verb);
}

static void
build_charset (EggActionGroup *action_group,
	       GString *xml_string,
	       const CharsetInfo *info,
	       int index,
	       EncodingData *edata)
{
	char *tmp;
	char *verb;
	EggAction *action;

	verb = g_strdup_printf ("Charset%d", index);
	action = g_object_new (EGG_TYPE_ACTION,
			       "name", verb,
			       "label", info->title,
			       NULL);
	g_signal_connect_closure
		(action, "activate",
		 g_cclosure_new (G_CALLBACK (ephy_encoding_menu_verb_cb),
				 edata,
				 (GClosureNotify)g_free),
		 FALSE);
	egg_action_group_add_action (action_group, action);
	g_object_unref (action);

	tmp = g_strdup_printf ("<menuitem name=\"Charset%dItem\" verb=\"%s\"/>\n",
			       index, verb);
	xml_string = g_string_append (xml_string, tmp);

	g_free (tmp);
	g_free (verb);
}

static void
ephy_encoding_menu_rebuild (EphyEncodingMenu *wrhm)
{
	EphyEncodingMenuPrivate *p = wrhm->priv;
	GString *xml;
	GList *groups, *gl;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);
	int group_index = 0, charset_index = 0;

	LOG ("Rebuilding encoding menu")

	xml = g_string_new (NULL);
	g_string_append (xml, "<Root><menu><submenu name=\"ViewMenu\">"
			      "<placeholder name=\"ViewEncodingsPlaceholder\">"
			      "<submenu name=\"ViewEncodingMenu\" verb=\"ViewEncoding\">");

	p->action_group = egg_action_group_new ("EncodingActions");
	egg_menu_merge_insert_action_group (merge, p->action_group, 0);

	ephy_embed_shell_get_charset_groups (embed_shell, &groups);

	for (gl = groups; gl != NULL; gl = gl->next)
        {
		GList *charsets, *cl;
		const char *group = (const char *)gl->data;

		build_group (p->action_group, xml, group, group_index);

		ephy_embed_shell_get_charset_titles (embed_shell,
                                                     group,
                                                     &charsets);

		for (cl = charsets; cl != NULL; cl = cl->next)
                {
			const CharsetInfo *info = cl->data;
			EncodingData *edata;

			edata = g_new0 (EncodingData, 1);
			edata->encoding = info->name;
			edata->window = p->window;

			build_charset (p->action_group, xml, info,
				       charset_index, edata);
			charset_index++;
		}


		g_list_free (charsets);
		g_string_append (xml, "</submenu>");
		group_index++;
	}

	g_string_append (xml, "</submenu></placeholder></submenu></menu></Root>");

	egg_menu_merge_add_ui_from_string
		(merge, xml->str, -1, NULL);

	g_string_free (xml, TRUE);
}
