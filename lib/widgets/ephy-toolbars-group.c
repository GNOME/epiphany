/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#include "ephy-toolbars-group.h"
#include "ephy-debug.h"

#include <libgnome/gnome-i18n.h>
#include <string.h>

static void	ephy_toolbars_group_class_init   (EphyToolbarsGroupClass *klass);
static void	ephy_toolbars_group_init	 (EphyToolbarsGroup *t);
static void	ephy_toolbars_group_finalize	 (GObject *object);

enum
{
	CHANGED,
	LAST_SIGNAL
};

static guint ephy_toolbars_group_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

struct EphyToolbarsGroupPrivate
{
	GNode *available_actions;
	GNode *toolbars;
	char *defaults;
	char *user;
};

GType
ephy_toolbars_group_get_type (void)
{
        static GType ephy_toolbars_group_type = 0;

        if (ephy_toolbars_group_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyToolbarsGroupClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_toolbars_group_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyToolbarsGroup),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_toolbars_group_init
                };

                ephy_toolbars_group_type = g_type_register_static (G_TYPE_OBJECT,
						                   "EphyToolbarsGroup",
						                   &our_info, 0);
        }

        return ephy_toolbars_group_type;

}

static xmlDocPtr
ephy_toolbars_group_to_xml (EphyToolbarsGroup *t)
{
	GNode *l1, *l2, *tl;
	xmlDocPtr doc;

	g_return_val_if_fail (IS_EPHY_TOOLBARS_GROUP (t), NULL);

	tl = t->priv->toolbars;

	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");
	doc->children = xmlNewDocNode (doc, NULL, "toolbars", NULL);

	for (l1 = tl->children; l1 != NULL; l1 = l1->next)
	{
		xmlNodePtr tnode;

		tnode = xmlNewChild (doc->children, NULL, "toolbar", NULL);

		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			xmlNodePtr node;
			EphyToolbarsItem *item = l2->data;

			if (item->separator)
			{
				 node = xmlNewChild (tnode, NULL, "separator", NULL);
			}
			else
			{
				node = xmlNewChild (tnode, NULL, "toolitem", NULL);
				xmlSetProp (node, "verb", item->action);
			}
		}
	}

	return doc;
}

static void
toolbars_group_save (EphyToolbarsGroup *t)
{
	xmlDocPtr doc;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (t));

	doc = ephy_toolbars_group_to_xml (t);
	xmlSaveFormatFile (t->priv->user, doc, 1);
	xmlFreeDoc (doc);
}

static EphyToolbarsToolbar *
toolbars_toolbar_new (void)
{
	EphyToolbarsToolbar *toolbar;

	toolbar = g_new0 (EphyToolbarsToolbar, 1);
	toolbar->widget = NULL;

	return toolbar;
}

static EphyToolbarsItem *
toolbars_item_new (const char *action, gboolean separator)
{
	EphyToolbarsItem *item;

	g_return_val_if_fail (action != NULL, NULL);

	item = g_new0 (EphyToolbarsItem, 1);
	item->action = g_strdup (action);
	item->separator = separator;
	item->widget = NULL;

	return item;
}

static void
free_toolbar_node (EphyToolbarsToolbar *toolbar)
{
	g_return_if_fail (toolbar != NULL);

	g_free (toolbar);
}

static void
free_item_node (EphyToolbarsItem *item)
{
	g_return_if_fail (item != NULL);

	g_free (item->action);
	g_free (item);
}

static void
add_action (EphyToolbarsGroup *t,
	    GNode *parent,
	    GNode *sibling,
	    const char *name)
{
	EphyToolbarsItem *item;
	GNode *node;
	gboolean separator;

	LOG ("Add action, name %s", name)

	separator = (strcmp (name, "separator") == 0);
	item = toolbars_item_new (name, separator);
	item->parent = parent->data;
	node = g_node_new (item);

	g_node_insert_before (parent, sibling, node);
}

void
ephy_toolbars_group_add_item (EphyToolbarsGroup *t,
			      EphyToolbarsToolbar *parent,
			      EphyToolbarsItem *sibling,
			      const char *name)
{
	GNode *parent_node;
	GNode *sibling_node = NULL;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (t));
	g_return_if_fail (parent != NULL);
	g_return_if_fail (name != NULL);

	parent_node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, parent);

	if (sibling)
	{
		sibling_node = g_node_find (t->priv->toolbars, G_IN_ORDER,
					    G_TRAVERSE_ALL, sibling);
		g_return_if_fail (sibling_node != NULL);
	}

	add_action (t, parent_node, sibling_node, name);

	toolbars_group_save (t);

	g_signal_emit (G_OBJECT (t), ephy_toolbars_group_signals[CHANGED], 0);
}

static void
parse_item_list (EphyToolbarsGroup *t,
		 xmlNodePtr child,
		 GNode *parent)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolitem"))
		{
			xmlChar *verb;

			verb = xmlGetProp (child, "verb");
			add_action (t, parent, NULL, verb);

			xmlFree (verb);
		}
		else if (xmlStrEqual (child->name, "separator"))
		{
			add_action (t, parent, NULL, "separator");
		}

		child = child->next;
	}
}

static GNode *
add_toolbar (EphyToolbarsGroup *t)
{
	EphyToolbarsToolbar *toolbar;
	GNode *node;

	toolbar = toolbars_toolbar_new ();
	node = g_node_new (toolbar);
	g_node_append (t->priv->toolbars, node);

	return node;
}

EphyToolbarsToolbar *
ephy_toolbars_group_add_toolbar (EphyToolbarsGroup *t)
{
	GNode *node;

	g_return_val_if_fail (IS_EPHY_TOOLBARS_GROUP (t), NULL);

	node = add_toolbar (t);
	g_return_val_if_fail (node != NULL, NULL);

	toolbars_group_save (t);

	g_signal_emit (G_OBJECT (t), ephy_toolbars_group_signals[CHANGED], 0);

	return node->data;
}

static void
parse_toolbars (EphyToolbarsGroup *t,
		xmlNodePtr child)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolbar"))
		{
			GNode *node;

			node = add_toolbar (t);
			parse_item_list (t, child->children, node);
		}

		child = child->next;
	}
}

static void
load_defaults (EphyToolbarsGroup *t)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	xmlNodePtr root;
	const char *xml_filepath;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (t));

	LOG ("Load default toolbar info")

	xml_filepath = t->priv->defaults;

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);

	child = root->children;
	while (child)
	{
		if (xmlStrEqual (child->name, "available"))
		{
			t->priv->available_actions = g_node_new (NULL);
			parse_item_list (t, child->children,
					 t->priv->available_actions);
		}
		else if (xmlStrEqual (child->name, "default") &&
			 t->priv->toolbars == NULL)
		{
			t->priv->toolbars = g_node_new (NULL);
			parse_toolbars (t, child->children);
		}

		child = child->next;
	}

	xmlFreeDoc (doc);
}

static void
load_toolbar (EphyToolbarsGroup *t)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	const char *xml_filepath = t->priv->user;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (t));

	LOG ("Load custom toolbar")

	if (!g_file_test (xml_filepath, G_FILE_TEST_EXISTS)) return;

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);

	t->priv->toolbars = g_node_new (NULL);
	parse_toolbars (t, root->children);

	xmlFreeDoc (doc);
}

char *
ephy_toolbars_group_to_string (EphyToolbarsGroup *t)
{
	GString *s;
	GNode *l1, *l2, *tl;
	char *result;
	int k = 0;

	g_return_val_if_fail (IS_EPHY_TOOLBARS_GROUP (t), NULL);

	tl = t->priv->toolbars;

	g_return_val_if_fail (tl != NULL, NULL);

	s = g_string_new (NULL);
	g_string_append (s, "<Root>");
	for (l1 = tl->children; l1 != NULL; l1 = l1->next)
	{
		int i = 0;

		g_string_append_printf
			(s, "<dockitem name=\"Toolbar%d\">\n", k);

		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			EphyToolbarsItem *item = l2->data;

			if (item->separator)
			{
				g_string_append_printf
					(s, "<placeholder name=\"PlaceHolder%d-%d\">"
					 "<separator name=\"TS\"/>"
					 "</placeholder>\n", i, k);
			}
			else
			{
				g_string_append_printf
					(s, "<placeholder name=\"PlaceHolder%d-%d\">"
					 "<toolitem name=\"TI%s\" verb=\"%s\"/>"
					 "</placeholder>\n",
					 i, k, item->action, item->action);
			}
			i++;
		}

		g_string_append (s, "</dockitem>\n");

		k++;
	}
	g_string_append (s, "</Root>");

	result = g_string_free (s, FALSE);

	return result;
}

static void
ephy_toolbars_group_class_init (EphyToolbarsGroupClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_toolbars_group_finalize;

	ephy_toolbars_group_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyToolbarsGroupClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

}

static void
ephy_toolbars_group_init (EphyToolbarsGroup *t)
{
        t->priv = g_new0 (EphyToolbarsGroupPrivate, 1);

	t->priv->available_actions = NULL;
	t->priv->toolbars = NULL;
	t->priv->user = NULL;
	t->priv->defaults = NULL;
}

static void
ephy_toolbars_group_finalize (GObject *object)
{
	EphyToolbarsGroup *t = EPHY_TOOLBARS_GROUP (object);

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (object));

	g_node_children_foreach (t->priv->available_actions, G_IN_ORDER,
				 (GNodeForeachFunc)free_item_node, NULL);
	ephy_toolbars_group_foreach_toolbar
		(t, (EphyToolbarsGroupForeachToolbarFunc)free_toolbar_node, NULL);
	ephy_toolbars_group_foreach_item
		(t, (EphyToolbarsGroupForeachItemFunc)free_item_node, NULL);
	g_node_destroy (t->priv->available_actions);
	g_node_destroy (t->priv->toolbars);

	g_free (t->priv->user);
	g_free (t->priv->defaults);

        g_free (t->priv);

	LOG ("EphyToolbarsGroup finalized")

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyToolbarsGroup *
ephy_toolbars_group_new (void)
{
	EphyToolbarsGroup *t;

	t = EPHY_TOOLBARS_GROUP (g_object_new (EPHY_TOOLBARS_GROUP_TYPE,
				   NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

void
ephy_toolbars_group_remove_toolbar (EphyToolbarsGroup *t,
				    EphyToolbarsToolbar *toolbar)
{
	GNode *node;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (t));
	g_return_if_fail (toolbar != NULL);

	node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, toolbar);
	g_return_if_fail (node != NULL);
	free_toolbar_node (node->data);
	g_node_destroy (node);

	toolbars_group_save (t);

	g_signal_emit (G_OBJECT (t), ephy_toolbars_group_signals[CHANGED], 0);
}

void
ephy_toolbars_group_remove_item	(EphyToolbarsGroup *t,
				 EphyToolbarsItem *item)
{
	GNode *node;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (t));
	g_return_if_fail (item != NULL);

	node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, item);
	g_return_if_fail (node != NULL);
	free_toolbar_node (node->data);
	g_node_destroy (node);

	toolbars_group_save (t);

	g_signal_emit (G_OBJECT (t), ephy_toolbars_group_signals[CHANGED], 0);
}

void
ephy_toolbars_group_set_source (EphyToolbarsGroup *group,
				const char *defaults,
				const char *user)
{
	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (group));
	g_return_if_fail (defaults != NULL);
	g_return_if_fail (user != NULL);

	group->priv->defaults = g_strdup (defaults);
	group->priv->user = g_strdup (user);

	load_toolbar (group);
	load_defaults (group);
}

static gboolean
is_item_in_toolbars (EphyToolbarsGroup *group, const char *action)
{
	GNode *l1, *l2;

	g_return_val_if_fail (IS_EPHY_TOOLBARS_GROUP (group), FALSE);
	g_return_val_if_fail (action != NULL, FALSE);

	for (l1 = group->priv->toolbars->children; l1 != NULL; l1 = l1->next)
	{
		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			EphyToolbarsItem *item;

			item = (EphyToolbarsItem *) l2->data;
			if (strcmp (action, item->action) == 0) return TRUE;
		}
	}

	return FALSE;
}

void
ephy_toolbars_group_foreach_available (EphyToolbarsGroup *group,
				       EphyToolbarsGroupForeachItemFunc func,
				       gpointer data)
{
	GNode *l1;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (group));

	for (l1 = group->priv->available_actions->children; l1 != NULL; l1 = l1->next)
	{
		EphyToolbarsItem *item;

		item = (EphyToolbarsItem *)l1->data;

		if (!is_item_in_toolbars (group, item->action))
		{
			func (item, data);
		}
	}
}

void
ephy_toolbars_group_foreach_toolbar (EphyToolbarsGroup *group,
				     EphyToolbarsGroupForeachToolbarFunc func,
				     gpointer data)
{
	GNode *l1;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (group));

	for (l1 = group->priv->toolbars->children; l1 != NULL; l1 = l1->next)
	{
		func (l1->data, data);
	}
}

void
ephy_toolbars_group_foreach_item (EphyToolbarsGroup *group,
				  EphyToolbarsGroupForeachItemFunc func,
				  gpointer data)
{
	GNode *l1, *l2;

	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (group));

	for (l1 = group->priv->toolbars->children; l1 != NULL; l1 = l1->next)
	{
		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			func (l2->data, data);
		}
	}
}

char *
ephy_toolbars_group_get_path (EphyToolbarsGroup *t,
			      gpointer item)
{
	GNode *node;
	char *path = NULL;
	EphyToolbarsItem *titem;

	g_return_val_if_fail (IS_EPHY_TOOLBARS_GROUP (t), NULL);

	node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, item);
	g_return_val_if_fail (node != NULL, NULL);
	titem = (EphyToolbarsItem *)node->data;

	switch (g_node_depth (node))
	{
		case 2:
			path = g_strdup_printf ("/Toolbar%d",
				                g_node_child_position (node->parent, node));
			break;
		case 3:
			path = g_strdup_printf
				("/Toolbar%d/PlaceHolder%d-%d/%s%s",
				 g_node_child_position (node->parent->parent, node->parent),
				 g_node_child_position (node->parent, node),
				 g_node_child_position (node->parent->parent, node->parent),
				 titem->separator ? "TS" : "TI",
				 titem->separator ? "" : titem->action);
			break;
		default:
			g_assert_not_reached ();
	}

	return path;
}
