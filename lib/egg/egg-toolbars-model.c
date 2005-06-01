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
static void egg_toolbars_model_init       (EggToolbarsModel      *t);
static void egg_toolbars_model_finalize   (GObject               *object);

enum
{
  ITEM_ADDED,
  ITEM_REMOVED,
  TOOLBAR_ADDED,
  TOOLBAR_CHANGED,
  TOOLBAR_REMOVED,
  GET_ITEM_TYPE,
  GET_ITEM_ID,
  GET_ITEM_DATA,
  LAST_SIGNAL
};

typedef struct
{
  char *name;
  EggTbModelFlags flags;
} EggToolbarsToolbar;

typedef struct
{
  char *id;
  char *type;
  gboolean separator;
} EggToolbarsItem;

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

#define EGG_TOOLBARS_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_TOOLBARS_MODEL, EggToolbarsModelPrivate))

struct EggToolbarsModelPrivate
{
  GNode *toolbars;
};

GType
egg_toolbars_model_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo our_info = {
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
egg_toolbars_model_to_xml (EggToolbarsModel *t)
{
  GNode *l1, *l2, *tl;
  xmlDocPtr doc;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (t), NULL);

  tl = t->priv->toolbars;

  xmlIndentTreeOutput = TRUE;
  doc = xmlNewDoc ((const xmlChar*) "1.0");
  doc->children = xmlNewDocNode (doc, NULL, (const xmlChar*) "toolbars", NULL);

  for (l1 = tl->children; l1 != NULL; l1 = l1->next)
    {
      xmlNodePtr tnode;
      EggToolbarsToolbar *toolbar = l1->data;

      tnode = xmlNewChild (doc->children, NULL, (const xmlChar*) "toolbar", NULL);
      xmlSetProp (tnode, (const xmlChar*) "name", (const xmlChar*) toolbar->name);

      for (l2 = l1->children; l2 != NULL; l2 = l2->next)
	{
	  xmlNodePtr node;
	  EggToolbarsItem *item = l2->data;

	  if (item->separator)
	    {
	      node = xmlNewChild (tnode, NULL, (const xmlChar*) "separator", NULL);
	    }
	  else
	    {
	      char *data;

	      node = xmlNewChild (tnode, NULL, (const xmlChar*) "toolitem", NULL);
	      data = egg_toolbars_model_get_item_data (t, item->type, item->id);
	      xmlSetProp (node, (const xmlChar*) "type", (const xmlChar*) item->type);
	      xmlSetProp (node, (const xmlChar*) "name", (const xmlChar*) data);
	      g_free (data);
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
egg_toolbars_model_save (EggToolbarsModel *t,
			 const char *xml_file,
			 const char *version)
{
  xmlDocPtr doc;
  xmlNodePtr root;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (t));

  doc = egg_toolbars_model_to_xml (t);
  root = xmlDocGetRootElement (doc);
  xmlSetProp (root, (const xmlChar*) "version", (const xmlChar*) version);
  safe_save_xml (xml_file, doc);
  xmlFreeDoc (doc);
}

static EggToolbarsToolbar *
toolbars_toolbar_new (const char *name)
{
  EggToolbarsToolbar *toolbar;

  toolbar = g_new (EggToolbarsToolbar, 1);
  toolbar->name = g_strdup (name);
  toolbar->flags = 0;

  return toolbar;
}

static EggToolbarsItem *
toolbars_item_new (const char *id,
		   const char *type,
		   gboolean    separator)
{
  EggToolbarsItem *item;

  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (type != NULL, NULL);

  item = g_new (EggToolbarsItem, 1);
  item->id = g_strdup (id);
  item->type = g_strdup (type);
  item->separator = separator;

  return item;
}

static void
free_toolbar_node (GNode *toolbar_node)
{
  EggToolbarsToolbar *toolbar = toolbar_node->data;

  g_free (toolbar->name);
  g_free (toolbar);

  g_node_destroy (toolbar_node);
}

static void
free_item_node (GNode *item_node)
{
  EggToolbarsItem *item = item_node->data;

  g_free (item->id);
  g_free (item->type);
  g_free (item);

  g_node_destroy (item_node);
}

EggTbModelFlags
egg_toolbars_model_get_flags (EggToolbarsModel *t,
			      int               toolbar_position)
{
  GNode *toolbar_node;
  EggToolbarsToolbar *toolbar;

  toolbar_node = g_node_nth_child (t->priv->toolbars, toolbar_position);
  g_return_val_if_fail (toolbar_node != NULL, 0);

  toolbar = toolbar_node->data;

  return toolbar->flags;
}

void
egg_toolbars_model_set_flags (EggToolbarsModel *t,
			      int               toolbar_position,
			      EggTbModelFlags   flags)
{
  GNode *toolbar_node;
  EggToolbarsToolbar *toolbar;

  toolbar_node = g_node_nth_child (t->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar_node != NULL);

  toolbar = toolbar_node->data;

  toolbar->flags = flags;

  g_signal_emit (G_OBJECT (t), signals[TOOLBAR_CHANGED],
		 0, toolbar_position);
}

void
egg_toolbars_model_add_separator (EggToolbarsModel *t,
			          int		    toolbar_position,
			          int		    position)
{
  GNode *parent_node;
  GNode *node;
  EggToolbarsItem *item;
  int real_position;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (t));

  parent_node = g_node_nth_child (t->priv->toolbars, toolbar_position);
  item = toolbars_item_new ("separator", EGG_TOOLBAR_ITEM_TYPE, TRUE);
  node = g_node_new (item);
  g_node_insert (parent_node, position, node);

  real_position = g_node_child_position (parent_node, node);

  g_signal_emit (G_OBJECT (t), signals[ITEM_ADDED], 0,
		 toolbar_position, real_position);
}

static gboolean
impl_add_item (EggToolbarsModel    *t,
	       int		    toolbar_position,
	       int		    position,
	       const char          *id,
	       const char          *type)
{
  GNode *parent_node;
  GNode *node;
  EggToolbarsItem *item;
  int real_position;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (t), FALSE);
  g_return_val_if_fail (id != NULL, FALSE);
  g_return_val_if_fail (type != NULL, FALSE);

  parent_node = g_node_nth_child (t->priv->toolbars, toolbar_position);
  item = toolbars_item_new (id, type, FALSE);
  node = g_node_new (item);
  g_node_insert (parent_node, position, node);

  real_position = g_node_child_position (parent_node, node);

  g_signal_emit (G_OBJECT (t), signals[ITEM_ADDED], 0,
		 toolbar_position, real_position);

  return TRUE;
}

static void
parse_item_list (EggToolbarsModel *t,
		 xmlNodePtr        child,
		 int               position)
{
  while (child)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "toolitem"))
	{
	  xmlChar *name, *type;
	  char *id;

	  name = xmlGetProp (child, (const xmlChar*) "name");
	  type = xmlGetProp (child, (const xmlChar*) "type");
          if (type == NULL)
            {
              type = xmlCharStrdup (EGG_TOOLBAR_ITEM_TYPE);
            }

	  if (name != NULL && name[0] != '\0' && type != NULL)
	    {
              id = egg_toolbars_model_get_item_id (t, (const char*)type, (const char*)name);
	      if (id != NULL)
	        {
	          egg_toolbars_model_add_item (t, position, -1, id, (const char*)type);
                }
              g_free (id);
            }
	  xmlFree (name);
          xmlFree (type);
	}
      else if (xmlStrEqual (child->name, (const xmlChar*) "separator"))
	{
	  egg_toolbars_model_add_separator (t, position, -1);
	}

      child = child->next;
    }
}

int
egg_toolbars_model_add_toolbar (EggToolbarsModel *t,
				int               position,
				const char       *name)
{
  GNode *node;
  int real_position;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (t), -1);

  node = g_node_new (toolbars_toolbar_new (name));
  g_node_insert (t->priv->toolbars, position, node);

  real_position = g_node_child_position (t->priv->toolbars, node);

  g_signal_emit (G_OBJECT (t), signals[TOOLBAR_ADDED],
		 0, real_position);

  return g_node_child_position (t->priv->toolbars, node);
}

static void
parse_toolbars (EggToolbarsModel *t,
		xmlNodePtr        child)
{
  while (child)
    {
      if (xmlStrEqual (child->name, (const xmlChar*) "toolbar"))
	{
	  xmlChar *name;
	  xmlChar *style;
	  int position;

	  name = xmlGetProp (child, (const xmlChar*) "name");
	  position = egg_toolbars_model_add_toolbar (t, -1, (const char*) name);
	  xmlFree (name);

	  style = xmlGetProp (child, (const xmlChar*) "style");
	  if (style && xmlStrEqual (style, (const xmlChar*) "icons-only"))
	    {
	      /* FIXME: use toolbar position instead of 0 */
	      egg_toolbars_model_set_flags (t, 0, EGG_TB_MODEL_ICONS);
	    }
	  xmlFree (style);

	  parse_item_list (t, child->children, position);
	}

      child = child->next;
    }
}

gboolean
egg_toolbars_model_load (EggToolbarsModel *t,
			 const char *xml_file)
{
  xmlDocPtr doc;
  xmlNodePtr root;

  g_return_val_if_fail (EGG_IS_TOOLBARS_MODEL (t), FALSE);

  if (!xml_file || !g_file_test (xml_file, G_FILE_TEST_EXISTS)) return FALSE;

  doc = xmlParseFile (xml_file);
  if (doc == NULL)
  {
    g_warning ("Failed to load XML data from %s", xml_file);
    return FALSE;
  }
  root = xmlDocGetRootElement (doc);

  parse_toolbars (t, root->children);

  xmlFreeDoc (doc);

  return TRUE;
}

static char *
impl_get_item_id (EggToolbarsModel *t,
		  const char       *type,
		  const char       *data)
{
  if (strcmp (type, EGG_TOOLBAR_ITEM_TYPE) == 0)
    {
      return g_strdup (data);
    }

  return NULL;
}

static char *
impl_get_item_data (EggToolbarsModel *t,
		    const char       *type,
		    const char       *id)
{
  if (strcmp (type, EGG_TOOLBAR_ITEM_TYPE) == 0)
    {
      return g_strdup (id);
    }

  return NULL;
}

static char *
impl_get_item_type (EggToolbarsModel *t,
		    GdkAtom type)
{
  if (gdk_atom_intern (EGG_TOOLBAR_ITEM_TYPE, FALSE) == type)
    {
      return g_strdup (EGG_TOOLBAR_ITEM_TYPE);
    }

  return NULL;
}

static gboolean
_egg_accumulator_STRING (GSignalInvocationHint *ihint,
                         GValue                *return_accu,
                         const GValue          *handler_return,
                         gpointer               dummy)
{
  gboolean continue_emission;
  const char *retval;

  retval = g_value_get_string (handler_return);
  g_value_set_string (return_accu, retval);
  continue_emission = !retval || !retval[0];
  
  return continue_emission;
}


static void
egg_toolbars_model_class_init (EggToolbarsModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = egg_toolbars_model_finalize;

  klass->add_item = impl_add_item;
  klass->get_item_id = impl_get_item_id;
  klass->get_item_data = impl_get_item_data;
  klass->get_item_type = impl_get_item_type;

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
  signals[GET_ITEM_TYPE] =
    g_signal_new ("get_item_type",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, get_item_type),
		  _egg_accumulator_STRING, NULL,
		  _egg_marshal_STRING__POINTER,
		  G_TYPE_STRING, 1, G_TYPE_POINTER);
  signals[GET_ITEM_ID] =
    g_signal_new ("get_item_id",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, get_item_id),
		  _egg_accumulator_STRING, NULL,
		  _egg_marshal_STRING__STRING_STRING,
		  G_TYPE_STRING, 2, G_TYPE_STRING, G_TYPE_STRING);
  signals[GET_ITEM_DATA] =
    g_signal_new ("get_item_data",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggToolbarsModelClass, get_item_data),
		  _egg_accumulator_STRING, NULL,
		  _egg_marshal_STRING__STRING_STRING,
		  G_TYPE_STRING, 2, G_TYPE_STRING, G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof (EggToolbarsModelPrivate));
}

static void
egg_toolbars_model_init (EggToolbarsModel *t)
{
  t->priv =EGG_TOOLBARS_MODEL_GET_PRIVATE (t);

  t->priv->toolbars = g_node_new (NULL);
}

static void
free_toolbar (GNode *toolbar_node)
{
  g_node_children_foreach (toolbar_node, G_TRAVERSE_ALL,
    			   (GNodeForeachFunc) free_item_node, NULL);
  free_toolbar_node (toolbar_node);
}

static void
egg_toolbars_model_finalize (GObject *object)
{
  EggToolbarsModel *t = EGG_TOOLBARS_MODEL (object);

  g_node_children_foreach (t->priv->toolbars, G_TRAVERSE_ALL,
    			   (GNodeForeachFunc) free_toolbar, NULL);
  g_node_destroy (t->priv->toolbars);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

EggToolbarsModel *
egg_toolbars_model_new (void)
{
  return EGG_TOOLBARS_MODEL (g_object_new (EGG_TYPE_TOOLBARS_MODEL, NULL));
}

void
egg_toolbars_model_remove_toolbar (EggToolbarsModel   *t,
				   int                 position)
{
  GNode *node;
  EggTbModelFlags flags;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (t));

  flags = egg_toolbars_model_get_flags (t, position);

  if (!(flags & EGG_TB_MODEL_NOT_REMOVABLE))
    {
      node = g_node_nth_child (t->priv->toolbars, position);
      g_return_if_fail (node != NULL);

      free_toolbar_node (node);

      g_signal_emit (G_OBJECT (t), signals[TOOLBAR_REMOVED],
		     0, position);
    }
}

void
egg_toolbars_model_remove_item (EggToolbarsModel *t,
				int               toolbar_position,
				int               position)
{
  GNode *node, *toolbar;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (t));

  toolbar = g_node_nth_child (t->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar != NULL);

  node = g_node_nth_child (toolbar, position);
  g_return_if_fail (node != NULL);

  free_item_node (node);

  g_signal_emit (G_OBJECT (t), signals[ITEM_REMOVED], 0,
		 toolbar_position, position);
}

void
egg_toolbars_model_move_item (EggToolbarsModel *t,
			      int               toolbar_position,
			      int               position,
			      int		new_toolbar_position,
			      int		new_position)
{
  GNode *node, *toolbar, *new_toolbar;

  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (t));

  toolbar = g_node_nth_child (t->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar != NULL);

  new_toolbar = g_node_nth_child (t->priv->toolbars, new_toolbar_position);
  g_return_if_fail (new_toolbar != NULL);

  node = g_node_nth_child (toolbar, position);
  g_return_if_fail (node != NULL);

  g_node_unlink (node);

  g_signal_emit (G_OBJECT (t), signals[ITEM_REMOVED], 0,
		 toolbar_position, position);

  g_node_insert (new_toolbar, new_position, node);

  g_signal_emit (G_OBJECT (t), signals[ITEM_ADDED], 0,
		 new_toolbar_position, new_position);
}

int
egg_toolbars_model_n_items (EggToolbarsModel *t,
			    int               toolbar_position)
{
  GNode *toolbar;

  toolbar = g_node_nth_child (t->priv->toolbars, toolbar_position);
  g_return_val_if_fail (toolbar != NULL, -1);

  return g_node_n_children (toolbar);
}

void
egg_toolbars_model_item_nth (EggToolbarsModel *t,
			     int	       toolbar_position,
			     int               position,
			     gboolean         *is_separator,
			     const char      **id,
			     const char      **type)
{
  GNode *toolbar;
  GNode *item;
  EggToolbarsItem *idata;

  toolbar = g_node_nth_child (t->priv->toolbars, toolbar_position);
  g_return_if_fail (toolbar != NULL);

  item = g_node_nth_child (toolbar, position);
  g_return_if_fail (item != NULL);

  idata = item->data;

  *is_separator = idata->separator;

  if (id)
    {
      *id = idata->id;
    }

  if (type)
    {
      *type = idata->type;
    }
}

int
egg_toolbars_model_n_toolbars (EggToolbarsModel *t)
{
  return g_node_n_children (t->priv->toolbars);
}

const char *
egg_toolbars_model_toolbar_nth (EggToolbarsModel *t,
				int               position)
{
  GNode *toolbar;
  EggToolbarsToolbar *tdata;

  toolbar = g_node_nth_child (t->priv->toolbars, position);
  g_return_val_if_fail (toolbar != NULL, NULL);

  tdata = toolbar->data;

  return tdata->name;
}

gboolean
egg_toolbars_model_add_item (EggToolbarsModel *t,
			     int	       toolbar_position,
			     int               position,
			     const char       *id,
			     const char       *type)
{
  EggToolbarsModelClass *klass = EGG_TOOLBARS_MODEL_GET_CLASS (t);
  return klass->add_item (t, toolbar_position, position, id, type);
}

char *
egg_toolbars_model_get_item_id (EggToolbarsModel *t,
			        const char       *type,
			        const char       *name)
{
  char *retval;

  g_signal_emit (t, signals[GET_ITEM_ID], 0, type, name, &retval);

  return retval;
}

char *
egg_toolbars_model_get_item_data (EggToolbarsModel *t,
				  const char       *type,
			          const char       *id)
{
  char *retval;

  g_signal_emit (t, signals[GET_ITEM_DATA], 0, type, id, &retval);

  return retval;
}

char *
egg_toolbars_model_get_item_type (EggToolbarsModel *t,
				  GdkAtom type)
{
  char *retval;

  g_signal_emit (t, signals[GET_ITEM_TYPE], 0, type, &retval);

  return retval;
}
