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

#include "egg-toolbars-group.h"

#include <string.h>

static void egg_toolbars_group_class_init (EggToolbarsGroupClass *klass);
static void egg_toolbars_group_init       (EggToolbarsGroup      *t);
static void egg_toolbars_group_finalize   (GObject               *object);

enum
{
  CHANGED,
  LAST_SIGNAL
};

static guint egg_toolbars_group_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

struct EggToolbarsGroupPrivate
{
  GNode *available_actions;
  GNode *toolbars;
  char *defaults;
  char *user;
};

GType
egg_toolbars_group_get_type (void)
{
  static GType egg_toolbars_group_type = 0;

  if (egg_toolbars_group_type == 0)
    {
      static const GTypeInfo our_info = {
	sizeof (EggToolbarsGroupClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	(GClassInitFunc) egg_toolbars_group_class_init,
	NULL,
	NULL,			/* class_data */
	sizeof (EggToolbarsGroup),
	0,			/* n_preallocs */
	(GInstanceInitFunc) egg_toolbars_group_init
      };

      egg_toolbars_group_type = g_type_register_static (G_TYPE_OBJECT,
							"EggToolbarsGroup",
							&our_info, 0);
    }

  return egg_toolbars_group_type;

}

static xmlDocPtr
egg_toolbars_group_to_xml (EggToolbarsGroup *t)
{
  GNode *l1, *l2, *tl;
  xmlDocPtr doc;

  g_return_val_if_fail (IS_EGG_TOOLBARS_GROUP (t), NULL);

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
	  EggToolbarsItem *item = l2->data;

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
toolbars_group_save (EggToolbarsGroup *t)
{
  xmlDocPtr doc;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (t));

  doc = egg_toolbars_group_to_xml (t);
  xmlSaveFormatFile (t->priv->user, doc, 1);
  xmlFreeDoc (doc);
}

static EggToolbarsToolbar *
toolbars_toolbar_new (void)
{
  EggToolbarsToolbar *toolbar;
  static int id = 0;

  toolbar = g_new0 (EggToolbarsToolbar, 1);
  toolbar->id = g_strdup_printf ("Toolbar%d", id);

  id++;

  return toolbar;
}

static EggToolbarsItem *
toolbars_item_new (const char *action,
		   gboolean    separator)
{
  EggToolbarsItem *item;
  static int id = 0;

  g_return_val_if_fail (action != NULL, NULL);

  item = g_new0 (EggToolbarsItem, 1);
  item->action = g_strdup (action);
  item->separator = separator;
  item->id = g_strdup_printf ("TI%d", id);

  id++;

  return item;
}

static void
free_toolbar_node (EggToolbarsToolbar *toolbar)
{
  g_return_if_fail (toolbar != NULL);

  g_free (toolbar->id);
  g_free (toolbar);
}

static void
free_item_node (EggToolbarsItem *item)
{
  g_return_if_fail (item != NULL);

  g_free (item->action);
  g_free (item->id);
  g_free (item);
}

static void
add_action (EggToolbarsGroup *t,
	    GNode            *parent,
	    int               pos,
	    const char       *name)
{
  EggToolbarsItem *item;
  GNode *node;
  gboolean separator;

  separator = (strcmp (name, "separator") == 0);
  item = toolbars_item_new (name, separator);
  item->parent = parent->data;
  node = g_node_new (item);

  g_node_insert (parent, pos, node);
}

void
egg_toolbars_group_add_item (EggToolbarsGroup    *t,
			     EggToolbarsToolbar  *parent,
			     int		  pos,
			     const char          *name)
{
  GNode *parent_node;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (t));
  g_return_if_fail (parent != NULL);
  g_return_if_fail (name != NULL);

  parent_node =
    g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, parent);

  add_action (t, parent_node, pos, name);

  toolbars_group_save (t);

  g_signal_emit (G_OBJECT (t), egg_toolbars_group_signals[CHANGED], 0);
}

static void
parse_item_list (EggToolbarsGroup *t,
		 xmlNodePtr        child,
		 GNode            *parent)
{
  while (child)
    {
      if (xmlStrEqual (child->name, "toolitem"))
	{
	  xmlChar *verb;

	  verb = xmlGetProp (child, "verb");
	  add_action (t, parent, -1, verb);

	  xmlFree (verb);
	}
      else if (xmlStrEqual (child->name, "separator"))
	{
	  add_action (t, parent, -1, "separator");
	}

      child = child->next;
    }
}

static GNode *
add_toolbar (EggToolbarsGroup *t)
{
  EggToolbarsToolbar *toolbar;
  GNode *node;

  toolbar = toolbars_toolbar_new ();
  node = g_node_new (toolbar);
  g_node_append (t->priv->toolbars, node);

  return node;
}

EggToolbarsToolbar *
egg_toolbars_group_add_toolbar (EggToolbarsGroup *t)
{
  GNode *node;

  g_return_val_if_fail (IS_EGG_TOOLBARS_GROUP (t), NULL);

  node = add_toolbar (t);
  g_return_val_if_fail (node != NULL, NULL);

  toolbars_group_save (t);

  g_signal_emit (G_OBJECT (t), egg_toolbars_group_signals[CHANGED], 0);

  return node->data;
}

static void
parse_toolbars (EggToolbarsGroup *t,
		xmlNodePtr        child)
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
load_defaults (EggToolbarsGroup *t)
{
  xmlDocPtr doc;
  xmlNodePtr child;
  xmlNodePtr root;
  const char *xml_filepath;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (t));

  xml_filepath = t->priv->defaults;

  doc = xmlParseFile (xml_filepath);
  root = xmlDocGetRootElement (doc);

  child = root->children;
  while (child)
    {
      if (xmlStrEqual (child->name, "available"))
	{
	  t->priv->available_actions = g_node_new (NULL);
	  parse_item_list (t, child->children, t->priv->available_actions);
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
load_toolbar (EggToolbarsGroup *t)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  const char *xml_filepath = t->priv->user;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (t));

  if (!g_file_test (xml_filepath, G_FILE_TEST_EXISTS))
    return;

  doc = xmlParseFile (xml_filepath);
  root = xmlDocGetRootElement (doc);

  t->priv->toolbars = g_node_new (NULL);
  parse_toolbars (t, root->children);

  xmlFreeDoc (doc);
}

char *
egg_toolbars_group_to_string (EggToolbarsGroup *t)
{
  GString *s;
  GNode *l1, *l2, *tl;
  char *result;
  int k = 0;

  g_return_val_if_fail (IS_EGG_TOOLBARS_GROUP (t), NULL);

  tl = t->priv->toolbars;

  g_return_val_if_fail (tl != NULL, NULL);

  s = g_string_new (NULL);
  g_string_append (s, "<Root>");
  for (l1 = tl->children; l1 != NULL; l1 = l1->next)
    {
      int i = 0;
      EggToolbarsToolbar *toolbar = l1->data;

      g_string_append_printf (s, "<dockitem name=\"%s\">\n", toolbar->id);

      for (l2 = l1->children; l2 != NULL; l2 = l2->next)
	{
	  EggToolbarsItem *item = l2->data;

	  if (item->separator)
	    {
	      g_string_append_printf
		(s, "<placeholder name=\"PlaceHolder%d-%d\">"
		 "<separator name=\"%s\"/>"
		 "</placeholder>\n", i, k, item->id);
	    }
	  else
	    {
	      g_string_append_printf
		(s, "<placeholder name=\"PlaceHolder%d-%d\">"
		 "<toolitem name=\"%s\" verb=\"%s\"/>"
		 "</placeholder>\n", i, k, item->id, item->action);
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
egg_toolbars_group_class_init (EggToolbarsGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = egg_toolbars_group_finalize;

  egg_toolbars_group_signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsGroupClass, changed),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

}

static void
egg_toolbars_group_init (EggToolbarsGroup *t)
{
  t->priv = g_new0 (EggToolbarsGroupPrivate, 1);

  t->priv->available_actions = NULL;
  t->priv->toolbars = NULL;
  t->priv->user = NULL;
  t->priv->defaults = NULL;
}

static void
egg_toolbars_group_finalize (GObject *object)
{
  EggToolbarsGroup *t = EGG_TOOLBARS_GROUP (object);

  g_return_if_fail (object != NULL);
  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (object));

  g_node_children_foreach (t->priv->available_actions, G_IN_ORDER,
			   (GNodeForeachFunc) free_item_node, NULL);
  egg_toolbars_group_foreach_toolbar
    (t, (EggToolbarsGroupForeachToolbarFunc) free_toolbar_node, NULL);
  egg_toolbars_group_foreach_item
    (t, (EggToolbarsGroupForeachItemFunc) free_item_node, NULL);
  g_node_destroy (t->priv->available_actions);
  g_node_destroy (t->priv->toolbars);

  g_free (t->priv->user);
  g_free (t->priv->defaults);

  g_free (t->priv);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

EggToolbarsGroup *
egg_toolbars_group_new (void)
{
  EggToolbarsGroup *t;

  t = EGG_TOOLBARS_GROUP (g_object_new (EGG_TOOLBARS_GROUP_TYPE, NULL));

  g_return_val_if_fail (t->priv != NULL, NULL);

  return t;
}

static void
remove_action (EggToolbarsItem *item,
	       gpointer	        *data)
{
  char *action = data[0];
  EggToolbarsGroup *group = EGG_TOOLBARS_GROUP (data[1]);

  if (strcmp (item->action, action) == 0)
    {
        egg_toolbars_group_remove_item (group, item);
    }
}

void
egg_toolbars_group_remove_action (EggToolbarsGroup *group,
				  const char	   *action)
{
  gpointer data[2];
  data[0] = (char *)action;
  data[1] = group;
  egg_toolbars_group_foreach_item
    (group, (EggToolbarsGroupForeachItemFunc) remove_action, data);
  g_signal_emit (G_OBJECT (group), egg_toolbars_group_signals[CHANGED], 0);
}

void
egg_toolbars_group_remove_toolbar (EggToolbarsGroup   *t,
				   EggToolbarsToolbar *toolbar)
{
  GNode *node;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (t));
  g_return_if_fail (toolbar != NULL);

  node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, toolbar);
  g_return_if_fail (node != NULL);
  free_toolbar_node (node->data);
  g_node_destroy (node);

  toolbars_group_save (t);

  g_signal_emit (G_OBJECT (t), egg_toolbars_group_signals[CHANGED], 0);
}

void
egg_toolbars_group_remove_item (EggToolbarsGroup *t,
				EggToolbarsItem  *item)
{
  GNode *node;
  GNode *toolbar;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (t));
  g_return_if_fail (item != NULL);

  node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, item);
  g_return_if_fail (node != NULL);
  toolbar = node->parent;
  free_item_node (node->data);
  g_node_destroy (node);

  if (g_node_n_children (toolbar) == 0)
    {
      free_toolbar_node (toolbar->data);
      g_node_destroy (toolbar);
    }

  toolbars_group_save (t);

  g_signal_emit (G_OBJECT (t), egg_toolbars_group_signals[CHANGED], 0);
}

void
egg_toolbars_group_set_source (EggToolbarsGroup *group,
			       const char       *defaults,
			       const char       *user)
{
  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (group));
  g_return_if_fail (defaults != NULL);

  group->priv->defaults = g_strdup (defaults);
  group->priv->user = g_strdup (user);

  load_toolbar (group);
  load_defaults (group);
}

static gboolean
is_item_in_toolbars (EggToolbarsGroup *group,
		     const char       *action)
{
  GNode *l1, *l2;

  g_return_val_if_fail (IS_EGG_TOOLBARS_GROUP (group), FALSE);
  g_return_val_if_fail (action != NULL, FALSE);

  for (l1 = group->priv->toolbars->children; l1 != NULL; l1 = l1->next)
    {
      for (l2 = l1->children; l2 != NULL; l2 = l2->next)
	{
	  EggToolbarsItem *item;

	  item = (EggToolbarsItem *) l2->data;
	  if (strcmp (action, item->action) == 0)
	    return TRUE;
	}
    }

  return FALSE;
}

void
egg_toolbars_group_foreach_available (EggToolbarsGroup		      *group,
				      EggToolbarsGroupForeachItemFunc  func,
				      gpointer			       data)
{
  GNode *l1;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (group));

  for (l1 = group->priv->available_actions->children; l1 != NULL;
       l1 = l1->next)
    {
      EggToolbarsItem *item;

      item = (EggToolbarsItem *) l1->data;

      if (!is_item_in_toolbars (group, item->action))
	{
	  func (item, data);
	}
    }
}

void
egg_toolbars_group_foreach_toolbar (EggToolbarsGroup		       *group,
				    EggToolbarsGroupForeachToolbarFunc  func,
				    gpointer				data)
{
  GNode *l1;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (group));

  for (l1 = group->priv->toolbars->children; l1 != NULL; l1 = l1->next)
    {
      func (l1->data, data);
    }
}

void
egg_toolbars_group_foreach_item (EggToolbarsGroup		 *group,
				 EggToolbarsGroupForeachItemFunc  func,
				 gpointer			  data)
{
  GNode *l1, *l2;

  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (group));

  for (l1 = group->priv->toolbars->children; l1 != NULL; l1 = l1->next)
    {
      for (l2 = l1->children; l2 != NULL; l2 = l2->next)
	{
	  func (l2->data, data);
	}
    }
}

char *
egg_toolbars_group_get_path (EggToolbarsGroup *t,
			     gpointer	       item)
{
  GNode *node;
  char *path = NULL;
  EggToolbarsItem *titem;
  EggToolbarsToolbar *toolbar;

  g_return_val_if_fail (IS_EGG_TOOLBARS_GROUP (t), NULL);

  node = g_node_find (t->priv->toolbars, G_IN_ORDER, G_TRAVERSE_ALL, item);
  g_return_val_if_fail (node != NULL, NULL);
  titem = (EggToolbarsItem *) node->data;
  toolbar = (EggToolbarsToolbar *) node->data;

  switch (g_node_depth (node))
    {
    case 2:
      path = g_strdup_printf ("/%s", toolbar->id);
      break;
    case 3:
      path = g_strdup_printf
	("/Toolbar%d/PlaceHolder%d-%d/%s",
	 g_node_child_position (node->parent->parent, node->parent),
	 g_node_child_position (node->parent, node),
	 g_node_child_position (node->parent->parent, node->parent),
	 titem->id);
      break;
    default:
      g_assert_not_reached ();
    }

  return path;
}
