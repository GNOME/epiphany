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
 *
 *  $Id$
 */

#include "ephy-toolbars-model.h"
#include "ephy-dnd.h"
#include "ephy-bookmarks.h"
#include "ephy-node-common.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <string.h>

static void ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass);
static void ephy_toolbars_model_init       (EphyToolbarsModel *t);
static void ephy_toolbars_model_finalize   (GObject *object);

enum
{
  	ACTION_ADDED,
  	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_BOOKMARKS
};

enum
{
	URL,
	NAME
};

static GObjectClass *parent_class = NULL;

struct EphyToolbarsModelPrivate
{
	EphyBookmarks *bookmarks;
	char *xml_file;
};

GType
ephy_toolbars_model_get_type (void)
{
	static GType ephy_toolbars_model_type = 0;

	if (ephy_toolbars_model_type == 0)
	{
		static const GTypeInfo our_info = {
		sizeof (EphyToolbarsModelClass),
		NULL,			/* base_init */
		NULL,			/* base_finalize */
		(GClassInitFunc) ephy_toolbars_model_class_init,
		NULL,
		NULL,			/* class_data */
		sizeof (EphyToolbarsModel),
		0,			/* n_preallocs */
		(GInstanceInitFunc) ephy_toolbars_model_init
	};

	ephy_toolbars_model_type = g_type_register_static (EGG_TOOLBARS_MODEL_TYPE,
							   "EphyToolbarsModel",
							   &our_info, 0);
	}

	return ephy_toolbars_model_type;
}

char *
ephy_toolbars_model_get_action_name (EphyToolbarsModel *model,
				     gboolean topic, long id)
{
	char *action_name;
	const char *name;
	EphyNode *node;
	EphyNodePriority priority;

	node = ephy_bookmarks_get_from_id (model->priv->bookmarks, id);
	priority = ephy_node_get_property_int
		(node, EPHY_NODE_KEYWORD_PROP_PRIORITY);

	if (topic)
	{
		if (priority != EPHY_NODE_NORMAL_PRIORITY)
		{
			action_name = g_strdup_printf ("GoSpecialTopic-%ld", id);
		}
		else
		{
			name = ephy_node_get_property_string
				(node, EPHY_NODE_KEYWORD_PROP_NAME);
			action_name = g_strdup_printf ("GoTopic-%s", name);
		}
	}
	else
	{
		name = ephy_node_get_property_string
			(node, EPHY_NODE_BMK_PROP_LOCATION);
		action_name = g_strdup_printf ("GoBookmark-%s", name);
	}

	return action_name;
}

static const char *
impl_add_item (EggToolbarsModel *t,
	       int toolbar_position,
	       int position,
	       GdkAtom type,
	       const char *name)
{
	char *action_name = NULL;
	const char *res;
	gboolean topic = FALSE, normal_item = FALSE;
	long id = -1;

	LOG ("Add item %s", name)

	if (gdk_atom_intern (EPHY_DND_TOPIC_TYPE, FALSE) == type)
	{
		GList *nodes;

		topic = TRUE;
		nodes = ephy_dnd_node_list_extract_nodes (name);
		id = ephy_node_get_id (nodes->data);
		action_name = ephy_toolbars_model_get_action_name
			(EPHY_TOOLBARS_MODEL (t), TRUE, id);
		g_list_free (nodes);
	}
	else if (gdk_atom_intern (EPHY_DND_BOOKMARK_TYPE, FALSE) == type)
	{
		GList *nodes;

		nodes = ephy_dnd_node_list_extract_nodes (name);
		id = ephy_node_get_id (nodes->data);
		action_name = ephy_toolbars_model_get_action_name
			(EPHY_TOOLBARS_MODEL (t), FALSE, id);
		g_list_free (nodes);
	}
	else if (gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE) == type)
	{
		EphyNode *node = NULL;
		EphyBookmarks *bookmarks;
		gchar **netscape_url;
		
		netscape_url = g_strsplit (name, "\n", 2);
		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		node = ephy_bookmarks_find_bookmark (bookmarks, netscape_url[URL]); 

		if (!node)
		{
			/* Create the bookmark, it does not exist */
			EphyHistory *gh;
			const char *icon;

			node = ephy_bookmarks_add (bookmarks, netscape_url[NAME], netscape_url[URL]);
			g_return_val_if_fail (node != NULL, NULL);

			gh = ephy_embed_shell_get_global_history (EPHY_EMBED_SHELL (ephy_shell));
			icon = ephy_history_get_icon (gh, netscape_url[URL]);
		
			if (icon)
			{
				ephy_bookmarks_set_icon (bookmarks, netscape_url[URL], icon);
			}
		}

		id = ephy_node_get_id (node);
		action_name = ephy_toolbars_model_get_action_name
			(EPHY_TOOLBARS_MODEL (t), FALSE, id);
		g_strfreev (netscape_url);
	}
	else
	{
		normal_item = TRUE;
	}

	res = action_name ? action_name : name;

	if (normal_item ||
	    !ephy_toolbars_model_has_bookmark (EPHY_TOOLBARS_MODEL (t), topic, id))
	{
		EGG_TOOLBARS_MODEL_CLASS (parent_class)->add_item
			(t, toolbar_position, position, type, res);
	}

	return res;
}

static void
ephy_toolbars_model_set_bookmarks (EphyToolbarsModel *model, EphyBookmarks *bookmarks)
{
	model->priv->bookmarks = bookmarks;
	g_object_ref (model->priv->bookmarks);
}

static void
ephy_toolbars_model_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	EphyToolbarsModel *model;

	model = EPHY_TOOLBARS_MODEL (object);

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			ephy_toolbars_model_set_bookmarks (model, g_value_get_object (value));
			break;
	}
}

static void
ephy_toolbars_model_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	EphyToolbarsModel *model;

	model = EPHY_TOOLBARS_MODEL (object);

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			g_value_set_object (value, model->priv->bookmarks);
			break;
	}
}

static void
ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EggToolbarsModelClass *etm_class;

	etm_class = EGG_TOOLBARS_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_toolbars_model_finalize;
	object_class->set_property = ephy_toolbars_model_set_property;
	object_class->get_property = ephy_toolbars_model_get_property;

	etm_class->add_item = impl_add_item;

	g_object_class_install_property (object_class,
                                         PROP_BOOKMARKS,
                                         g_param_spec_object ("bookmarks",
                                                              "Bookmarks",
                                                              "Bookmarks",
                                                              EPHY_BOOKMARKS_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
item_added (EphyToolbarsModel *model, int toolbar_position, int position)
{
	egg_toolbars_model_save (EGG_TOOLBARS_MODEL (model),
				 model->priv->xml_file);
}

static void
item_removed (EphyToolbarsModel *model, int toolbar_position, int position)
{
	egg_toolbars_model_save (EGG_TOOLBARS_MODEL (model),
				 model->priv->xml_file);
}

static void
toolbar_added (EphyToolbarsModel *model, int position)
{
	egg_toolbars_model_save (EGG_TOOLBARS_MODEL (model),
				 model->priv->xml_file);
}

static void
toolbar_removed (EphyToolbarsModel *model, int position)
{
	egg_toolbars_model_save (EGG_TOOLBARS_MODEL (model),
				 model->priv->xml_file);
}

static void
ephy_toolbars_model_init (EphyToolbarsModel *t)
{
	EggToolbarsModel *egg_model = EGG_TOOLBARS_MODEL (t);

	t->priv = g_new0 (EphyToolbarsModelPrivate, 1);
	t->priv->bookmarks = NULL;

	t->priv->xml_file = g_build_filename (ephy_dot_dir (),
                                              "ephy-toolbars.xml",
                                              NULL);

	if (g_file_test (t->priv->xml_file, G_FILE_TEST_EXISTS))
	{
		egg_toolbars_model_load (egg_model,
					 t->priv->xml_file);
	}
	else
	{
		const char *default_xml;

		default_xml = ephy_file ("epiphany-toolbar.xml");
		egg_toolbars_model_load (egg_model, default_xml);
	}

	g_signal_connect (t, "item_added", G_CALLBACK (item_added), NULL);
	g_signal_connect (t, "item_removed", G_CALLBACK (item_removed), NULL);
	g_signal_connect (t, "toolbar_added", G_CALLBACK (toolbar_added), NULL);
	g_signal_connect (t, "toolbar_removed", G_CALLBACK (toolbar_removed), NULL);
}

static void
ephy_toolbars_model_finalize (GObject *object)
{
	EphyToolbarsModel *t = EPHY_TOOLBARS_MODEL (object);

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EPHY_TOOLBARS_MODEL (object));

	g_object_unref (t->priv->bookmarks);

	g_free (t->priv->xml_file);

	g_free (t->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyToolbarsModel *
ephy_toolbars_model_new (EphyBookmarks *bookmarks)
{
	EphyToolbarsModel *t;

	t = EPHY_TOOLBARS_MODEL (g_object_new (EPHY_TOOLBARS_MODEL_TYPE,
					       "bookmarks", bookmarks,
					       NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

static int
get_item_pos (EphyToolbarsModel *model,
	      int toolbar_pos,
	      const char *name)
{
	int i, n_items;

	n_items = egg_toolbars_model_n_items
		(EGG_TOOLBARS_MODEL (model), toolbar_pos);

	for (i = 0; i < n_items; i++)
	{
		const char *i_name;
		gboolean is_separator;

		i_name = egg_toolbars_model_item_nth
			(EGG_TOOLBARS_MODEL (model), toolbar_pos, i,
			 &is_separator);
		if (!is_separator && strcmp (name, i_name) == 0)
		{
			return i;
		}
	}

	return -1;
}

static int
get_toolbar_pos (EphyToolbarsModel *model,
		 const char *name)
{
	int i, n_toolbars;

	n_toolbars = egg_toolbars_model_n_toolbars
		(EGG_TOOLBARS_MODEL (model));

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth
			(EGG_TOOLBARS_MODEL (model), i);
		if (strcmp (name, t_name) == 0)
		{
			return i;
		}
	}

	return -1;
}

void
ephy_toolbars_model_remove_bookmark (EphyToolbarsModel *model,
				     gboolean topic,
				     long id)
{
	char *action_name;
	int toolbar_position, position;

	action_name = ephy_toolbars_model_get_action_name (model, topic, id);

	toolbar_position = get_toolbar_pos (model, "BookmarksBar");
	g_return_if_fail (toolbar_position != -1);

	position = get_item_pos (model, toolbar_position, action_name);

	egg_toolbars_model_remove_item (EGG_TOOLBARS_MODEL (model),
				        toolbar_position, position);

	g_free (action_name);
}

void
ephy_toolbars_model_add_bookmark (EphyToolbarsModel *model,
				  gboolean topic,
				  long id)
{
	char *action_name;
	int toolbar_position;

	action_name = ephy_toolbars_model_get_action_name (model, topic, id);

	toolbar_position = get_toolbar_pos (model, "BookmarksBar");
	g_return_if_fail (toolbar_position != -1);

	egg_toolbars_model_add_item (EGG_TOOLBARS_MODEL (model),
				     toolbar_position, -1,
				     0, action_name);

	g_free (action_name);
}

gboolean
ephy_toolbars_model_has_bookmark (EphyToolbarsModel *model,
				  gboolean topic,
				  long id)
{
	char *action_name;
	int toolbar_position, position;

	action_name = ephy_toolbars_model_get_action_name (model, topic, id);

	toolbar_position = get_toolbar_pos (model, "BookmarksBar");
	g_return_val_if_fail (toolbar_position != -1, FALSE);
	position = get_item_pos (model, toolbar_position, action_name);

	g_free (action_name);

	return (position != -1);
}

void
ephy_toolbars_model_set_flag (EphyToolbarsModel *model,
			      EggTbModelFlags flags)
{
	EggToolbarsModel *t = EGG_TOOLBARS_MODEL (model);
	int i, n_toolbars;

	n_toolbars = egg_toolbars_model_n_toolbars
		(EGG_TOOLBARS_MODEL (model));

	for (i = 0; i < n_toolbars; i++)
	{
		EggTbModelFlags old_flags;

		old_flags = egg_toolbars_model_get_flags (t, i);

		egg_toolbars_model_set_flags (t, old_flags | flags, i);
	}
}

void
ephy_toolbars_model_unset_flag (EphyToolbarsModel *model,
			        EggTbModelFlags flags)
{
	EggToolbarsModel *t = EGG_TOOLBARS_MODEL (model);
	int i, n_toolbars;

	n_toolbars = egg_toolbars_model_n_toolbars
		(EGG_TOOLBARS_MODEL (model));

	for (i = 0; i < n_toolbars; i++)
	{
		EggTbModelFlags old_flags;

		old_flags = egg_toolbars_model_get_flags (t, i);

		egg_toolbars_model_set_flags (t, old_flags ^ flags, i);
	}
}
