/*
 *  Copyright (C) 2002-2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#include "config.h"

#include "egg-toolbars-model.h"
#include "eggtypebuiltins.h"
#include "eggmarshalers.h"

#include <unistd.h>
#include <string.h>
#include <libxml/tree.h>
#include <gdk/gdkproperty.h>

static void egg_toolbars_model_class_init (EggToolbarsModelClass *klass);
static void egg_toolbars_model_init       (EggToolbarsModel      *model);
static void egg_toolbars_model_finalize   (GObject               *object);

enum
{
  ITEM_ADDED,
  ITEM_REMOVED,
  TOOLBAR_ADDED,
  TOOLBAR_CHANGED,
  TOOLBAR_REMOVED,
  LAST_SIGNAL
};

typedef struct
{
  char *name;
  EggTbModelFlags flags;
} EggToolbarsToolbar;

typedef struct
{
  char *name;
} EggToolbarsItem;

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

#define EGG_TOOLBARS_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_TOOLBARS_MODEL, EggToolbarsModelPrivate))

struct EggToolbarsModelPrivate
{
  GNode *toolbars;
  GList *types;
  GHashTable *flags;
};

GType
egg_toolbars_model_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      const GTypeInfo our_info = {
	sizeof (EggToolbarsModelClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	(GClassInitFunc) egg_toolbars_model_class_init,
	NULL,
	NULL,			/* class_data */
	sizeof (EggToolbarsModel),
	0,			/* n_preallocs */
	(GInstanceInitFunc) egg_toolbars_model_init
      };
      volatile GType flags_type; /* work around gcc's optimiser */

      /* make sure the flags type is known */
      flags_type = EGG_TYPE_TB_MODEL_FLAGS;

      type = g_type_register_static (G_TYPE_OBJECT,
				     "EggToolbarsModel",
				     &our_info, 0);
    }

  return type;
}

static xmlDocPtr
egg_toolbars_model_to_xml (EggToolbarsModel *model)
{
  GNode *l1, *l2, *tl;
  GList *l3;
  xmlDocPtr doc;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (model), NULL);

  tl = model->priv->toolbars;

  xmlIndentTreeOutput = TRUE;
  doc = xmlNewDoc ((const xmlChar*) "1.0");
  doc->children = xmlNewDocNode (doc, NULL, (const xmlChar*) "toolbars", NULL);

  for (l1 = tl->children; l1 != NULL; l1 = l1->next)
    {
      xmlNodePtr tnode;
      EggToolbarsToolbar *toolbar = l1->data;

      tnode = xmlNewChild (doc->children, NULL, (const xmlChar*) "toolbar", NULL);
      xmlSetProp (tnode, (const xmlChar*) "name", (const xmlChar*) toolbar->name);
      xmlSetProp (tnode, (const xmlChar*) "hidden",
		  (toolbar->flags&EGG_TB_MODEL_HIDDEN) ? (const xmlChar*) "true" : (const xmlChar*) "false");
      xmlSetProp (tnode, (const xmlChar*) "editable",
		  (toolbar->flags&EGG_TB_MODEL_NOT_EDITABLE) ? (const xmlChar*) "false" : (const xmlChar*) "true");

      for (l2 = l1->children; l2 != NULL; l2 = l2->next)
	{
	  xmlNodePtr node;
	  EggToolbarsItem *item = l2->data;

          if (strcmp (item->name, "_separator") == 0)
            {
              node = xmlNewChild (tnode, NULL, (const xmlChar*) "separator", NULL);
              continue;
            }
          
          node = xmlNewChild (tnode, NULL, (const xmlChar*) "toolitem", NULL);
          xmlSetProp (node, (const xmlChar*) "name", (const xmlChar*) item->name);

          /* Add 'data' nodes for each data type which can be written out for this
           * item. Only write types which can be used to restore the data. */
          for (l3 = model->priv->types; l3 != NULL; l3 = l3->next)
            {
              EggToolbarsItemType *type = l3->data;
              if (type->get_name != NULL && type->get_data != NULL)
                {
                  xmlNodePtr dnode;
                  char *tmp;
                  
                  tmp = type->get_data (type, item->name);
                  if (tmp != NULL)
                    {
                      dnode = xmlNewTextChild (node, NULL, (const xmlChar*) "data", (const xmlChar*) tmp);
                      g_free (tmp);
                      
                      tmp = gdk_atom_name (type->type);
                      xmlSetProp (dnode, (const xmlChar*) "type", (const xmlChar*) tmp);
                      g_free (tmp);
                    }
                }
	    }
	}
    }

  return doc;
}

static gboolean
safe_save_xml (const char *xml_file, xmlDocPtr doc)
{
	char *tmp_file;
	char *old_file;
	gboolean old_exist;
	gboolean retval = TRUE;

	tmp_file = g_strconcat (xml_file, ".tmp", NULL);
	old_file = g_strconcat (xml_file, ".old", NULL);

	if (xmlSaveFormatFile (tmp_file, doc, 1) <= 0)
	{
		g_warning ("Failed to write XML data to %s", tmp_file);
		goto failed;
	}

	old_exist = g_file_test (xml_file, G_FILE_TEST_EXISTS);

	if (old_exist)
	{
		if (rename (xml_file, old_file) < 0)
		{
			g_warning ("Failed to rename %s to %s", xml_file, old_file);
			retval = FALSE;
			goto failed;
		}
	}

	if (rename (tmp_file, xml_file) < 0)
	{
		g_warning ("Failed to rename %s to %s", tmp_file, xml_file);

		if (rename (old_file, xml_file) < 0)
		{
			g_warning ("Failed to restore %s from %s", xml_file, tmp_file);
		}
		retval = FALSE;
		goto failed;
	}

	if (old_exist)
	{
		if (unlink (old_file) < 0)
		{
			g_warning ("Failed to delete old file %s", old_file);
		}
	}

	failed:
	g_free (old_file);
	g_free (tmp_file);

	return retval;
}

void
egg_toolbars_model_save_toolbars (EggToolbarsModel *model,
				  const char *xml_file,
				  const char *version)
{
  xmlDocPtr doc;
  xmlNodePtr root;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (model));

  doc = egg_toolbars_model_to_xml (model);
  root = xmlDocGetRootElement (doc);
  xmlSetProp (root, (const xmlChar*) "version", (const xmlChar*) version);
  safe_save_xml (xml_file, doc);
  xmlFreeDoc (doc);
}

static gboolean
is_unique (EggToolbarsModel *model,
	   EggToolbarsItem *idata)
{
  EggToolbarsItem *idata2;
  GNode *toolbar, *item;
  
   
  for(toolbar = g_node_first_child (model->priv->toolbars);
      toolbar != NULL; toolbar = g_node_next_sibling (toolbar))
    {
      for(item = g_node_first_child (toolbar);
	  item != NULL; item = g_node_next_sibling (item))
        {
	  idata2 = item->data;
	  
	  if (idata != idata2 && strcmp (idata->name, idata2->name) == 0)
	    {
	      return FALSE;
	    }
	}
    }
  
  return TRUE;
}

static GNode *
toolbar_node_new (const char *name)
{
  EggToolbarsToolbar *toolbar;

  toolbar = g_new (EggToolbarsToolbar, 1);
  toolbar->name = g_strdup (name);
  toolbar->flags = 0;

  return g_node_new (toolbar);
}

static GNode *
item_node_new (const char *name, EggToolbarsModel *model)
{
  EggToolbarsItem *item;
  int flags;

  g_return_val_if_fail (name != NULL, NULL);

  item = g_new (EggToolbarsItem, 1);
  item->name = g_strdup (name);

  flags = GPOINTER_TO_INT (g_hash_table_lookup (model->priv->flags, item->name));
  if ((flags & EGG_TB_MODEL_NAME_INFINITE) == 0)
    g_hash_table_insert (model->priv->flags,
			 g_strdup (item->name),
			 GINT_TO_POINTER (flags | EGG_TB_MODEL_NAME_USED));

  return g_node_new (item);
}

static void
item_node_free (GNode *item_node, EggToolbarsModel *model)
{
  EggToolbarsItem *item = item_node->data;
  int flags;

  flags = GPOINTER_TO_INT (g_hash_table_lookup (model->priv->flags, item->name));
  if ((flags & EGG_TB_MODEL_NAME_INFINITE) == 0 && is_unique (model, item))
    g_hash_table_insert (model->priv->flags,
			 g_strdup (item->name),
			 GINT_TO_POINTER (flags & ~EGG_TB_MODEL_NAME_USED));

  g_free (item->name);
  g_free (item);

  g_node_destroy (item_node);
}

static void
toolbar_node_free (GNode *toolbar_node, EggToolbarsModel *model)
{
  EggToolbarsToolbar *toolbar = toolbar_node->data;

  g_node_children_foreach (toolbar_node, G_TRAVERSE_ALL,
    			   (GNodeForeachFunc) item_node_free, model);
    
  g_free (toolbar->name);
  g_free (toolbar);

  g_node_destroy (toolbar_node);
}

EggTbModelFlags
egg_toolbars_model_get_flags (EggToolbarsModel *model,
			      int               toolbar_position)
{
  GNode *toolbar_node;
  EggToolbarsToolbar *toolbar;

  toolbar_node = g_node_nth_child (model->priv->toolbars, toolbar_position);
  g_return_val_if_fail (toolbar_node != NULL, 0);

  toolbar = toolbar_node->data;

  return toolbar->flags;
}

void
egg_toolbars_model_set_flags (EggToolbarsModel *model,
			      int               toolbar_position,
			      EggTbModelFlags   flags)
{
  GNode *toolbar_node;
  EggToolbarsToolbar *toolbar;

  toolbar_node = g_node_nth_child (model->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar_node != NULL);

  toolbar = toolbar_node->data;

  toolbar->flags = flags;

  g_signal_emit (G_OBJECT (model), signals[TOOLBAR_CHANGED],
		 0, toolbar_position);
}


char *
egg_toolbars_model_get_data (EggToolbarsModel *model,
                             GdkAtom           type,
                             const char       *name)
{
  EggToolbarsItemType *t;
  char *data = NULL;
  GList *l;
  
  if (type == GDK_NONE || type == gdk_atom_intern (EGG_TOOLBAR_ITEM_TYPE, FALSE))
    {
      g_return_val_if_fail (name != NULL, NULL);
      g_return_val_if_fail (*name != 0,   NULL);
      return strdup (name);
    }
  
  for (l = model->priv->types; l != NULL; l = l->next)
    {
      t = l->data;
      if (t->type == type && t->get_data != NULL)
        {
          data = t->get_data (t, name);
	  if (data != NULL) break;
        }
    }
  
  return data;
}

char *
egg_toolbars_model_get_name (EggToolbarsModel *model,
                             GdkAtom           type,
                             const char       *data,
                             gboolean          create)
{
  EggToolbarsItemType *t;
  char *name = NULL;
  GList *l;
  
  if (type == GDK_NONE || type == gdk_atom_intern (EGG_TOOLBAR_ITEM_TYPE, FALSE))
    {
      g_return_val_if_fail (data, NULL);
      g_return_val_if_fail (*data, NULL);
      return strdup (data);
    }
  
  if (create)
    {
      for (l = model->priv->types; name == NULL && l != NULL; l = l->next)
        {
          t = l->data;
          if (t->type == type && t->new_name != NULL)
            name = t->new_name (t, data);
        }
      
      return name;
    }
  else
    {
      for (l = model->priv->types; name == NULL && l != NULL; l = l->next)
        {
          t = l->data;
          if (t->type == type && t->get_name != NULL)
            name = t->get_name (t, data);
        }
      
      return name;
    }  
}

static gboolean
impl_add_item (EggToolbarsModel    *model,
	       int		    toolbar_position,
	       int		    position,
	       const char          *name)
{
  GNode *parent_node;
  GNode *child_node;
  int real_position;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (model), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  parent_node = g_node_nth_child (model->priv->toolbars, toolbar_position);
  child_node = item_node_new (name, model);
  g_node_insert (parent_node, position, child_node);

  real_position = g_node_child_position (parent_node, child_node);

  g_signal_emit (G_OBJECT (model), signals[ITEM_ADDED], 0,
		 toolbar_position, real_position);

  return TRUE;
}

gboolean
egg_toolbars_model_add_item (EggToolbarsModel *model,
			     int	       toolbar_position,
			     int               position,
			     const char       *name)
{
  EggToolbarsModelClass *klass = EGG_TOOLBARS_MODEL_GET_CLASS (model);
  return klass->add_item (model, toolbar_position, position, name);
}

int
egg_toolbars_model_add_toolbar (EggToolbarsModel *model,
				int               position,
				const char       *name)
{
  GNode *node;
  int real_position;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (model), -1);

  node = toolbar_node_new (name);
  g_node_insert (model->priv->toolbars, position, node);

  real_position = g_node_child_position (model->priv->toolbars, node);

  g_signal_emit (G_OBJECT (model), signals[TOOLBAR_ADDED],
		 0, real_position);

  return g_node_child_position (model->priv->toolbars, node);
}

static char *
parse_data_list (EggToolbarsModel *model,
		 xmlNodePtr        child,
                 gboolean          create)
{
  char *name = NULL;
  while (child && name == NULL)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "data"))
        {
          xmlChar *type = xmlGetProp (child, (const xmlChar*) "type");
          xmlChar *data = xmlNodeGetContent (child);
  
          if (type != NULL)
            {
              GdkAtom atom = gdk_atom_intern ((const char*) type, TRUE);
              name = egg_toolbars_model_get_name (model, atom, (const char*) data, create);
            }
          
          xmlFree (type);
          xmlFree (data);
        }
      
      child = child->next;
    }
  
  return name;
}

static void
parse_item_list (EggToolbarsModel *model,
		 xmlNodePtr        child,
		 int               position)
{
  while (child)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "toolitem"))
	{
          char *name;

          /* Try to get the name using the data elements first,
             as they are more 'portable' or 'persistent'. */
          name = parse_data_list (model, child->children, FALSE);
          if (name == NULL)
            {
              name = parse_data_list (model, child->children, TRUE);
            }
          
          /* If that fails, try to use the name. */
          if (name == NULL)
            {
              xmlChar *type = xmlGetProp (child, (const xmlChar*) "type");
              xmlChar *data = xmlGetProp (child, (const xmlChar*) "name");
              GdkAtom  atom = type ? gdk_atom_intern ((const char*) type, TRUE) : GDK_NONE;
              
              /* If an old format, try to use it. */
              name = egg_toolbars_model_get_name (model, atom, (const char*) data, FALSE);
              if (name == NULL)
                {
                  name = egg_toolbars_model_get_name (model, atom, (const char*) data, TRUE);
                }
              
              xmlFree (type);
              xmlFree (data);
            }
          
          if (name != NULL)
            {
              egg_toolbars_model_add_item (model, position, -1, name);
              g_free (name);
            }
	}
      else if (xmlStrEqual (child->name, (const xmlChar*) "separator"))
	{
          egg_toolbars_model_add_item (model, position, -1, "_separator");
	}

      child = child->next;
    }
}

static void
parse_toolbars (EggToolbarsModel *model,
		xmlNodePtr        child)
{
  while (child)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "toolbar"))
	{
	  xmlChar *string;
	  int position;
          EggTbModelFlags flags;

	  string = xmlGetProp (child, (const xmlChar*) "name");
	  position = egg_toolbars_model_add_toolbar (model, -1, (const char*) string);
          flags = egg_toolbars_model_get_flags (model, position);
	  xmlFree (string);

	  string = xmlGetProp (child, (const xmlChar*) "editable");
          if (string && xmlStrEqual (string, (const xmlChar*) "false"))
            flags |= EGG_TB_MODEL_NOT_EDITABLE;
	  xmlFree (string);

	  string = xmlGetProp (child, (const xmlChar*) "hidden");
          if (string && xmlStrEqual (string, (const xmlChar*) "true"))
            flags |= EGG_TB_MODEL_HIDDEN;
	  xmlFree (string);

	  string = xmlGetProp (child, (const xmlChar*) "style");
	  if (string && xmlStrEqual (string, (const xmlChar*) "icons-only"))
            flags |= EGG_TB_MODEL_ICONS;
	  xmlFree (string);

          egg_toolbars_model_set_flags (model, position, flags);
          
	  parse_item_list (model, child->children, position);
	}

      child = child->next;
    }
}

gboolean
egg_toolbars_model_load_toolbars (EggToolbarsModel *model,
				  const char *xml_file)
{
  xmlDocPtr doc;
  xmlNodePtr root;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (model), FALSE);

  if (!xml_file || !g_file_test (xml_file, G_FILE_TEST_EXISTS)) return FALSE;

  doc = xmlParseFile (xml_file);
  if (doc == NULL)
  {
    g_warning ("Failed to load XML data from %s", xml_file);
    return FALSE;
  }
  root = xmlDocGetRootElement (doc);

  parse_toolbars (model, root->children);

  xmlFreeDoc (doc);

  return TRUE;
}

static void
parse_available_list (EggToolbarsModel *model,
		      xmlNodePtr        child)
{
  gint flags;
  
  while (child)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "toolitem"))
	{
	  xmlChar *name;

	  name = xmlGetProp (child, (const xmlChar*) "name");
	  flags = egg_toolbars_model_get_name_flags
	    (model, (const char*)name);
	  egg_toolbars_model_set_name_flags
	    (model, (const char*)name, flags | EGG_TB_MODEL_NAME_KNOWN);
	  xmlFree (name);
	}
      child = child->next;
    }
}

static void
parse_names (EggToolbarsModel *model,
	     xmlNodePtr        child)
{
  while (child)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "available"))
	{
	  parse_available_list (model, child->children);
	}

      child = child->next;
    }
}

gboolean
egg_toolbars_model_load_names (EggToolbarsModel *model,
			       const char *xml_file)
{
  xmlDocPtr doc;
  xmlNodePtr root;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (model), FALSE);

  if (!xml_file || !g_file_test (xml_file, G_FILE_TEST_EXISTS)) return FALSE;

  doc = xmlParseFile (xml_file);
  if (doc == NULL)
  {
    g_warning ("Failed to load XML data from %s", xml_file);
    return FALSE;
  }
  root = xmlDocGetRootElement (doc);

  parse_names (model, root->children);

  xmlFreeDoc (doc);

  return TRUE;
}

static void
egg_toolbars_model_class_init (EggToolbarsModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = egg_toolbars_model_finalize;

  klass->add_item = impl_add_item;

  signals[ITEM_ADDED] =
    g_signal_new ("item_added",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, item_added),
		  NULL, NULL, _egg_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  signals[TOOLBAR_ADDED] =
    g_signal_new ("toolbar_added",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, toolbar_added),
		  NULL, NULL, g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE, 1, G_TYPE_INT);
  signals[ITEM_REMOVED] =
    g_signal_new ("item_removed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, item_removed),
		  NULL, NULL, _egg_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  signals[TOOLBAR_REMOVED] =
    g_signal_new ("toolbar_removed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, toolbar_removed),
		  NULL, NULL, g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE, 1, G_TYPE_INT);
  signals[TOOLBAR_CHANGED] =
    g_signal_new ("toolbar_changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, toolbar_changed),
		  NULL, NULL, g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE, 1, G_TYPE_INT);

  g_type_class_add_private (object_class, sizeof (EggToolbarsModelPrivate));
}

static void
egg_toolbars_model_init (EggToolbarsModel *model)
{
  model->priv =EGG_TOOLBARS_MODEL_GET_PRIVATE (model);

  model->priv->toolbars = g_node_new (NULL);
  model->priv->flags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  egg_toolbars_model_set_name_flags (model, "_separator", 
				     EGG_TB_MODEL_NAME_KNOWN |
				     EGG_TB_MODEL_NAME_INFINITE);
}

static void
egg_toolbars_model_finalize (GObject *object)
{
  EggToolbarsModel *model = EGG_TOOLBARS_MODEL (object);

  g_node_children_foreach (model->priv->toolbars, G_TRAVERSE_ALL,
    			   (GNodeForeachFunc) toolbar_node_free, model);
  g_node_destroy (model->priv->toolbars);
  g_hash_table_destroy (model->priv->flags);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

EggToolbarsModel *
egg_toolbars_model_new (void)
{
  return EGG_TOOLBARS_MODEL (g_object_new (EGG_TYPE_TOOLBARS_MODEL, NULL));
}

void
egg_toolbars_model_remove_toolbar (EggToolbarsModel   *model,
				   int                 position)
{
  GNode *node;
  EggTbModelFlags flags;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (model));

  flags = egg_toolbars_model_get_flags (model, position);

  if (!(flags & EGG_TB_MODEL_NOT_REMOVABLE))
    {
      node = g_node_nth_child (model->priv->toolbars, position);
      g_return_if_fail (node != NULL);

      toolbar_node_free (node, model);

      g_signal_emit (G_OBJECT (model), signals[TOOLBAR_REMOVED],
		     0, position);
    }
}

void
egg_toolbars_model_remove_item (EggToolbarsModel *model,
				int               toolbar_position,
				int               position)
{
  GNode *node, *toolbar;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (model));

  toolbar = g_node_nth_child (model->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar != NULL);

  node = g_node_nth_child (toolbar, position);
  g_return_if_fail (node != NULL);

  item_node_free (node, model);

  g_signal_emit (G_OBJECT (model), signals[ITEM_REMOVED], 0,
		 toolbar_position, position);
}

void
egg_toolbars_model_move_item (EggToolbarsModel *model,
			      int               toolbar_position,
			      int               position,
			      int		new_toolbar_position,
			      int		new_position)
{
  GNode *node, *toolbar, *new_toolbar;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (model));

  toolbar = g_node_nth_child (model->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar != NULL);

  new_toolbar = g_node_nth_child (model->priv->toolbars, new_toolbar_position);
  g_return_if_fail (new_toolbar != NULL);

  node = g_node_nth_child (toolbar, position);
  g_return_if_fail (node != NULL);

  g_node_unlink (node);

  g_signal_emit (G_OBJECT (model), signals[ITEM_REMOVED], 0,
		 toolbar_position, position);

  g_node_insert (new_toolbar, new_position, node);

  g_signal_emit (G_OBJECT (model), signals[ITEM_ADDED], 0,
		 new_toolbar_position, new_position);
}

void
egg_toolbars_model_delete_item (EggToolbarsModel *model,
				const char       *name)
{
  EggToolbarsItem *idata;
  EggToolbarsToolbar *tdata;
  GNode *toolbar, *item, *next;
  int tpos, ipos;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (model));
  
  toolbar = g_node_first_child (model->priv->toolbars);
  tpos = 0;
  
  while (toolbar != NULL)
    {
      item = g_node_first_child (toolbar);
      ipos = 0;
      
      /* Don't delete toolbars that were already empty */
      if (item == NULL)
        {
	  toolbar = g_node_next_sibling (toolbar);
	  continue;
        }
      
      while (item != NULL)
        {
	  next = g_node_next_sibling (item);
	  idata = item->data;
	  if (strcmp (idata->name, name) == 0)
	    {
	      item_node_free (item, model);
	      g_signal_emit (G_OBJECT (model),
			     signals[ITEM_REMOVED],
			     0, tpos, ipos);
	    }
	  else
	    {
	      ipos++;
	    }
	  
	  item = next;
        }
      
      next = g_node_next_sibling (toolbar);
      tdata = toolbar->data;
      if (!(tdata->flags & EGG_TB_MODEL_NOT_REMOVABLE) &&
	  g_node_first_child (toolbar) == NULL)
        {
	  toolbar_node_free (toolbar, model);

	  g_signal_emit (G_OBJECT (model),
			 signals[TOOLBAR_REMOVED],
			 0, tpos);
        }
      else
        {
	  tpos++;
        }
      
      toolbar = next;
    }
}

int
egg_toolbars_model_n_items (EggToolbarsModel *model,
			    int               toolbar_position)
{
  GNode *toolbar;

  toolbar = g_node_nth_child (model->priv->toolbars, toolbar_position);
  g_return_val_if_fail (toolbar != NULL, -1);

  return g_node_n_children (toolbar);
}

const char *
egg_toolbars_model_item_nth (EggToolbarsModel *model,
			     int	       toolbar_position,
			     int               position)
{
  GNode *toolbar;
  GNode *item;
  EggToolbarsItem *idata;

  toolbar = g_node_nth_child (model->priv->toolbars, toolbar_position);
  g_return_val_if_fail (toolbar != NULL, NULL);

  item = g_node_nth_child (toolbar, position);
  g_return_val_if_fail (item != NULL, NULL);

  idata = item->data;
  return idata->name;
}

int
egg_toolbars_model_n_toolbars (EggToolbarsModel *model)
{
  return g_node_n_children (model->priv->toolbars);
}

const char *
egg_toolbars_model_toolbar_nth (EggToolbarsModel *model,
				int               position)
{
  GNode *toolbar;
  EggToolbarsToolbar *tdata;

  toolbar = g_node_nth_child (model->priv->toolbars, position);
  g_return_val_if_fail (toolbar != NULL, NULL);

  tdata = toolbar->data;

  return tdata->name;
}

GList *
egg_toolbars_model_get_types (EggToolbarsModel *model)
{
  return model->priv->types;
}

void
egg_toolbars_model_set_types (EggToolbarsModel *model, GList *types)
{
  model->priv->types = types;
}

static void
fill_avail_array (gpointer key, gpointer value, GPtrArray *array)
{
  int flags = GPOINTER_TO_INT (value);
  if ((flags & EGG_TB_MODEL_NAME_KNOWN) && !(flags & EGG_TB_MODEL_NAME_USED))
      g_ptr_array_add (array, key);
}

GPtrArray *
egg_toolbars_model_get_name_avail (EggToolbarsModel *model)
{
  GPtrArray *array = g_ptr_array_new ();
  g_hash_table_foreach (model->priv->flags, (GHFunc) fill_avail_array, array);
  return array;
}

gint
egg_toolbars_model_get_name_flags (EggToolbarsModel *model, const char *name)
{
  return GPOINTER_TO_INT (g_hash_table_lookup (model->priv->flags, name));
}

void
egg_toolbars_model_set_name_flags (EggToolbarsModel *model, const char *name, gint flags)
{
  g_hash_table_insert (model->priv->flags, g_strdup (name), GINT_TO_POINTER (flags));
}
