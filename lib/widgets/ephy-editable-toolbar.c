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

#include "ephy-editable-toolbar.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <libxml/parser.h>

static void ephy_editable_toolbar_class_init (EphyEditableToolbarClass *klass);
static void ephy_editable_toolbar_init (EphyEditableToolbar *t);
static void ephy_editable_toolbar_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_MENU_MERGE
};

enum
{
	REQUEST_ACTION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static gint EphyEditableToolbarSignals[LAST_SIGNAL];

struct EphyEditableToolbarPrivate
{
	EggMenuMerge *merge;
	EggAction *separator;
	GList *available_actions;
	GList *toolbars;
	char *filename;
};

GType
ephy_editable_toolbar_get_type (void)
{
        static GType ephy_editable_toolbar_type = 0;

        if (ephy_editable_toolbar_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEditableToolbarClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_editable_toolbar_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyEditableToolbar),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_editable_toolbar_init
                };

                ephy_editable_toolbar_type = g_type_register_static (G_TYPE_OBJECT,
						                     "EphyEditableToolbar",
						                     &our_info, 0);
        }

        return ephy_editable_toolbar_type;

}

static EggAction *
find_action (EphyEditableToolbar *t, const char *name)
{
	GList *l = t->priv->merge->action_groups;
	EggAction *action = NULL;

	for (; l != NULL; l = l->next)
	{
		EggAction *tmp;

		tmp = egg_action_group_get_action
			(EGG_ACTION_GROUP (l->data), name);
		if (tmp) action = tmp;
	}

	return action;
}

static void
add_action_to_list (EphyEditableToolbar *t, GList **list, const char *name)
{
	EggAction *action = NULL;

	action = find_action (t, name);

	if (!action)
	{
		g_signal_emit (t, EphyEditableToolbarSignals[REQUEST_ACTION], 0, name);
	}

	action = find_action (t, name);

	if (action)
	{
		*list = g_list_append (*list, action);
	}
}

static void
parse_item_list (EphyEditableToolbar *t,
		 xmlNodePtr child,
		 GList **actions)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolitem"))
		{
			xmlChar *verb;

			verb = xmlGetProp (child, "verb");
			add_action_to_list (t, actions, verb);

			xmlFree (verb);
		}
		else if (xmlStrEqual (child->name, "separator"))
		{
			*actions = g_list_append
				(*actions, t->priv->separator);
		}

		child = child->next;
	}
}

static void
parse_toolbars (EphyEditableToolbar *t,
		xmlNodePtr child,
		GList **toolbars)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolbar"))
		{
			GList *list = NULL;

			parse_item_list (t, child->children, &list);
			*toolbars = g_list_append (*toolbars, list);
		}

		child = child->next;
	}
}

static void
load_defaults (EphyEditableToolbar *t)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	xmlNodePtr root;
	const char *xml_filepath;

	LOG ("Load default toolbar info")

	xml_filepath = ephy_file ("epiphany-toolbar.xml");

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);

	child = root->children;
	while (child)
	{
		if (xmlStrEqual (child->name, "available"))
		{
			parse_item_list (t, child->children,
					 &t->priv->available_actions);
		}
		else if (xmlStrEqual (child->name, "default") &&
			 t->priv->toolbars == NULL)
		{
			parse_toolbars (t, child->children,
					&t->priv->toolbars);
		}

		child = child->next;
	}
}

static void
load_toolbar (EphyEditableToolbar *t)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	const char *xml_filepath = t->priv->filename;

	LOG ("Load custom toolbar")

	if (!g_file_test (xml_filepath, G_FILE_TEST_EXISTS)) return;

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);

	parse_toolbars (t, root->children,
			&t->priv->toolbars);
}

static xmlDocPtr
toolbar_list_to_xml (EphyEditableToolbar *t, GList *tl)
{
	GList *l1, *l2;
	xmlDocPtr doc;

	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");
	doc->children = xmlNewDocNode (doc, NULL, "toolbars", NULL);

	for (l1 = tl; l1 != NULL; l1 = l1->next)
	{
		xmlNodePtr tnode;

		tnode = xmlNewChild (doc->children, NULL, "toolbar", NULL);

		for (l2 = l1->data; l2 != NULL; l2 = l2->next)
		{
			xmlNodePtr node;
			EggAction *action = EGG_ACTION (l2->data);

			if (action == t->priv->separator)
			{
				 node = xmlNewChild (tnode, NULL, "separator", NULL);
			}
			else
			{
				node = xmlNewChild (tnode, NULL, "toolitem", NULL);
				xmlSetProp (node, "verb", action->name);
			}
		}
	}

	return doc;
}

static char *
toolbar_list_to_string (EphyEditableToolbar *t, GList *tl)
{
	GString *s;
	GList *l1, *l2;
	char *result;

	s = g_string_new (NULL);
	g_string_append (s, "<Root>");
	for (l1 = tl; l1 != NULL; l1 = l1->next)
	{
		int i = 0;

		g_string_append_printf
			(s, "<dockitem name=\"Toolbar%d\">",
			 g_list_index (tl, l1->data));

		for (l2 = l1->data; l2 != NULL; l2 = l2->next)
		{
			EggAction *action = EGG_ACTION (l2->data);

			if (action == t->priv->separator)
			{
				g_string_append_printf
					(s, "<separator name=\"ToolItem%d\"/>", i);
			}
			else
			{
				g_string_append_printf
					(s, "<toolitem name=\"ToolItem%d\" verb=\"%s\"/>",
					 i, action->name);
			}

			i++;
		}

		g_string_append (s, "</dockitem>");
	}
	g_string_append (s, "</Root>");

	result = s->str;

	g_string_free (s, FALSE);

	return result;
}

static void
do_merge (EphyEditableToolbar *t)
{
	GList *tl;
	char *str;

	tl = t->priv->toolbars;

	str = toolbar_list_to_string (t, tl);

	LOG ("Merge UI\n%s", str)

	egg_menu_merge_add_ui_from_string
		(t->priv->merge, str, -1, NULL);
	egg_menu_merge_ensure_update (t->priv->merge);

	g_free (str);
}

static void
ephy_editable_toolbar_set_merge (EphyEditableToolbar *t, EggMenuMerge *merge)
{
	t->priv->merge = merge;

	LOG ("Got MenuMerge")

	load_toolbar (t);
	load_defaults (t);
	do_merge (t);
}

static void
ephy_editable_toolbar_set_property (GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
        EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        switch (prop_id)
        {
		case PROP_MENU_MERGE:
		ephy_editable_toolbar_set_merge (t, g_value_get_object (value));
		break;
        }
}

static void
ephy_editable_toolbar_get_property (GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
        EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_MENU_MERGE:
                        g_value_set_object (value, t->priv->merge);
                        break;
        }
}

static void
ephy_editable_toolbar_class_init (EphyEditableToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_editable_toolbar_finalize;

	object_class->set_property = ephy_editable_toolbar_set_property;
	object_class->get_property = ephy_editable_toolbar_get_property;

	EphyEditableToolbarSignals[REQUEST_ACTION] =
		g_signal_new
		("request_action", G_OBJECT_CLASS_TYPE (klass),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                 G_STRUCT_OFFSET (EphyEditableToolbarClass, request_action),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__STRING,
		 G_TYPE_NONE, 1,
		 G_TYPE_STRING);

	g_object_class_install_property (object_class,
                                         PROP_MENU_MERGE,
                                         g_param_spec_object ("MenuMerge",
                                                              "MenuMerge",
                                                              "Menu merge",
                                                              EGG_TYPE_MENU_MERGE,
                                                              G_PARAM_READWRITE));
}

static void
ephy_editable_toolbar_init (EphyEditableToolbar *t)
{
        t->priv = g_new0 (EphyEditableToolbarPrivate, 1);

	t->priv->merge = NULL;
	t->priv->available_actions = NULL;
	t->priv->toolbars = NULL;
	t->priv->filename = g_build_filename (ephy_dot_dir (), "toolbar.xml", NULL);
	t->priv->separator = g_object_new (EGG_TYPE_ACTION, NULL);
}

static void
ephy_editable_toolbar_save (EphyEditableToolbar *t)
{
	xmlDocPtr doc;

	doc = toolbar_list_to_xml (t, t->priv->toolbars);
	xmlSaveFormatFile (t->priv->filename, doc, 1);
	xmlFreeDoc (doc);
}

static void
ephy_editable_toolbar_finalize (GObject *object)
{
	EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (object));

	ephy_editable_toolbar_save (t);

	g_object_unref (t->priv->separator);
	g_free (t->priv->filename);

        g_free (t->priv);

	LOG ("EphyEditableToolbar finalized")

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyEditableToolbar *
ephy_editable_toolbar_new (EggMenuMerge *merge)
{
	EphyEditableToolbar *t;

	t = EPHY_EDITABLE_TOOLBAR (g_object_new (EPHY_EDITABLE_TOOLBAR_TYPE,
				   "MenuMerge", merge,
				   NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}
