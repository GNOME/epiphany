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
#include "ephy-history.h"
#include "ephy-embed-shell.h"
#include "ephy-shell.h"
#include "ephy-debug.h"
#include "ephy-string.h"

#include <string.h>
#include <glib/gi18n.h>

static void ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass);
static void ephy_toolbars_model_init       (EphyToolbarsModel *t);
static void ephy_toolbars_model_finalize   (GObject *object);

#define EPHY_TOOLBARS_XML_VERSION "1.0"

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

#define EPHY_TOOLBARS_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBARS_MODEL, EphyToolbarsModelPrivate))

struct EphyToolbarsModelPrivate
{
	EphyBookmarks *bookmarks;
	char *xml_file;
	gboolean loading;
};

GType
ephy_toolbars_model_get_type (void)
{
	static GType type = 0;

	if (type == 0)
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

	type = g_type_register_static (EGG_TYPE_TOOLBARS_MODEL,
				       "EphyToolbarsModel",
				       &our_info, 0);
	}

	return type;
}

char *
ephy_toolbars_model_get_action_name (EphyToolbarsModel *model,
				     long id)
{
	return g_strdup_printf ("GoBookmark-%ld", id);
}

EphyNode *
ephy_toolbars_model_get_node (EphyToolbarsModel *model,
			      const char *action_name)
{
	EphyBookmarks *bookmarks = EPHY_TOOLBARS_MODEL (model)->priv->bookmarks;
	long node_id;

	if (!ephy_string_to_int (action_name + strlen ("GoBookmark-"), &node_id))
	{
		return NULL;
	}

	return ephy_bookmarks_get_from_id (bookmarks, node_id);
}

static void
bookmark_destroy_cb (EphyNode *node,
		     EphyToolbarsModel *model)
{
	long id;

	id = ephy_node_get_id (node);
	ephy_toolbars_model_remove_bookmark (model, id);
}

static char *
impl_get_item_name (EggToolbarsModel *t,
		    const char       *type,
		    const char       *id)
{
	EphyToolbarsModel *model = EPHY_TOOLBARS_MODEL (t);
	EphyNode *node;

	if (strcmp (type, EPHY_DND_TOPIC_TYPE) == 0)
	{
		char *uri;

		node = ephy_toolbars_model_get_node (model, id);
		g_return_val_if_fail (node != NULL, NULL);

		uri = ephy_bookmarks_get_topic_uri
			(model->priv->bookmarks, node);

		return uri;
	}
	else if (strcmp (type, EPHY_DND_URL_TYPE) == 0)
	{
		const char *name;

		node = ephy_toolbars_model_get_node (model, id);
		g_return_val_if_fail (node != NULL, NULL);

		name = ephy_node_get_property_string
                        (node, EPHY_NODE_BMK_PROP_LOCATION);

		return g_strdup (name);
	}

	return EGG_TOOLBARS_MODEL_CLASS (parent_class)->get_item_name (t, type, id);
}

static char *
impl_get_item_id (EggToolbarsModel *t,
		  const char       *type,
		  const char       *name)
{
	EphyToolbarsModel *model = EPHY_TOOLBARS_MODEL (t);
	EphyBookmarks *bookmarks = model->priv->bookmarks;

	if (strcmp (type, EPHY_DND_TOPIC_TYPE) == 0)
	{
		EphyNode *topic;

		topic = ephy_bookmarks_find_keyword (bookmarks, name, FALSE);
		if (topic == NULL) return NULL;

		return ephy_toolbars_model_get_action_name
			(model, ephy_node_get_id (topic));
	}
	else if (strcmp (type, EPHY_DND_URL_TYPE) == 0)
	{
		EphyNode *node = NULL;
		gchar **netscape_url;

		netscape_url = g_strsplit (name, "\n", 2);
		node = ephy_bookmarks_find_bookmark (bookmarks, netscape_url[URL]);

		if (!node)
		{
			/* Create the bookmark, it does not exist */
			EphyHistory *gh;
			const char *icon;
			const char *title;

			title = netscape_url[NAME];
			if (title == NULL || *title == '\0')
			{
				title = _("Untitled");
			}

			node = ephy_bookmarks_add (bookmarks, title, netscape_url[URL]);

			if (node != NULL)
			{
				gh = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
				icon = ephy_history_get_icon (gh, netscape_url[URL]);

				if (icon)
				{
					ephy_bookmarks_set_icon (bookmarks, netscape_url[URL], icon);
				}
			}
		}

		g_strfreev (netscape_url);

		if (node == NULL) return NULL;

		return ephy_toolbars_model_get_action_name
			(model, ephy_node_get_id (node));
	}

	return EGG_TOOLBARS_MODEL_CLASS (parent_class)->get_item_id (t, type, name);
}

static char *
impl_get_item_type (EggToolbarsModel *t,
		    GdkAtom type)
{
	if (gdk_atom_intern (EPHY_DND_TOPIC_TYPE, FALSE) == type)
	{
		return g_strdup (EPHY_DND_TOPIC_TYPE);
	}
	else if (gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE) == type)
	{
		return g_strdup (EPHY_DND_URL_TYPE);
	}

	return EGG_TOOLBARS_MODEL_CLASS (parent_class)->get_item_type (t, type);
}

static gboolean
get_toolbar_and_item_pos (EphyToolbarsModel *model,
			  const char *action_name,
			  int *toolbar,
			  int *position)
{
	int n_toolbars, n_items;
	int t,i;

	n_toolbars = egg_toolbars_model_n_toolbars (EGG_TOOLBARS_MODEL (model));

	for (t = 0; t < n_toolbars; t++)
	{
		n_items = egg_toolbars_model_n_items
				(EGG_TOOLBARS_MODEL (model), t);

		for (i = 0; i < n_items; i++)
		{
			const char *i_name;
			gboolean is_separator;

			i_name = egg_toolbars_model_item_nth
					(EGG_TOOLBARS_MODEL (model), t, i,
					 &is_separator);
			g_return_val_if_fail (i_name != NULL, FALSE);

			if (strcmp (i_name, action_name) == 0)
			{
				if (toolbar) *toolbar = t;
				if (position) *position = i;

				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
impl_add_item (EggToolbarsModel    *t,
	       int		    toolbar_position,
	       int		    position,
	       const char          *id,
	       const char          *type)
{
	EphyToolbarsModel *model = EPHY_TOOLBARS_MODEL (t);
	gboolean is_bookmark;

	is_bookmark = strcmp (type, EPHY_DND_TOPIC_TYPE) == 0 ||
	              strcmp (type, EPHY_DND_URL_TYPE) == 0;

	if (!is_bookmark || !get_toolbar_and_item_pos (model, id, NULL, NULL))
	{
		return EGG_TOOLBARS_MODEL_CLASS (parent_class)->add_item
			(t, toolbar_position, position, id, type);
	}
	else
	{
		return FALSE;
	}
}

static void
connect_item (EphyToolbarsModel *model,
	      const char *name)
{
	EphyNode *node;

	if (g_str_has_prefix (name, "GoBookmark-"))
	{
		node = ephy_toolbars_model_get_node (model, name);
		g_return_if_fail (node != NULL);

		ephy_node_signal_connect_object (node,
					         EPHY_NODE_DESTROY,
					         (EphyNodeCallback) bookmark_destroy_cb,
					         G_OBJECT (model));
	}
}

static void
update_toolbar_removeable_flag (EggToolbarsModel *model)
{
	int i, n_toolbars;
	int flag = 0;

	n_toolbars = egg_toolbars_model_n_toolbars (model);

	/* If there is only one toolbar and the bookmarks bar */
	if (n_toolbars <= 2)
	{
		flag = EGG_TB_MODEL_NOT_REMOVABLE;
	}

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth (model, i);
		g_return_if_fail (t_name != NULL);

		if (!(strcmp (t_name, "BookmarksBar") == 0))
		{
			egg_toolbars_model_set_flags (model, flag, i);
		}
	}
}

static void
ephy_toolbars_model_set_bookmarks (EphyToolbarsModel *model, EphyBookmarks *bookmarks)
{
	EggToolbarsModel *egg_model = EGG_TOOLBARS_MODEL (model);
	gboolean success = FALSE;

	model->priv->bookmarks = g_object_ref (bookmarks);

	model->priv->loading = TRUE;

	if (g_file_test (model->priv->xml_file, G_FILE_TEST_EXISTS))
	{
		success = egg_toolbars_model_load (egg_model,
					 model->priv->xml_file);
	}

	if (success == FALSE)
	{
		const char *default_xml;

		default_xml = ephy_file ("epiphany-toolbar.xml");
		egg_toolbars_model_load (egg_model, default_xml);
	}

	model->priv->loading = FALSE;
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
	etm_class->get_item_id = impl_get_item_id;
	etm_class->get_item_name = impl_get_item_name;
	etm_class->get_item_type = impl_get_item_type;

	g_object_class_install_property (object_class,
                                         PROP_BOOKMARKS,
                                         g_param_spec_object ("bookmarks",
                                                              "Bookmarks",
                                                              "Bookmarks",
                                                              EPHY_TYPE_BOOKMARKS,
                                                              G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(EphyToolbarsModelPrivate));
}

static void
save_changes (EphyToolbarsModel *model)
{
	if (!model->priv->loading)
	{
		egg_toolbars_model_save (EGG_TOOLBARS_MODEL (model),
					 model->priv->xml_file,
					 EPHY_TOOLBARS_XML_VERSION);
	}
}

static void
item_added (EphyToolbarsModel *model, int toolbar_position, int position)
{
	const char *i_name;
	gboolean is_separator;

	i_name = egg_toolbars_model_item_nth
		(EGG_TOOLBARS_MODEL (model), toolbar_position,
		 position, &is_separator);
	if (!is_separator)
	{
		connect_item (model, i_name);
	}

	save_changes (model);
}

static void
item_removed (EphyToolbarsModel *model, int toolbar_position, int position)
{
	save_changes (model);
}

static void
toolbar_added (EphyToolbarsModel *model, int position)
{
	save_changes (model);
	update_toolbar_removeable_flag (EGG_TOOLBARS_MODEL (model));
}

static void
toolbar_removed (EphyToolbarsModel *model, int position)
{
	save_changes (model);
	update_toolbar_removeable_flag (EGG_TOOLBARS_MODEL (model));
}

static void
ephy_toolbars_model_init (EphyToolbarsModel *t)
{
	t->priv = EPHY_TOOLBARS_MODEL_GET_PRIVATE (t);

	t->priv->bookmarks = NULL;
	t->priv->loading = FALSE;
	t->priv->xml_file = g_build_filename (ephy_dot_dir (),
                                              "epiphany-toolbars.xml",
                                              NULL);

	g_signal_connect (t, "item_added", G_CALLBACK (item_added), NULL);
	g_signal_connect (t, "item_removed", G_CALLBACK (item_removed), NULL);
	g_signal_connect (t, "toolbar_added", G_CALLBACK (toolbar_added), NULL);
	g_signal_connect (t, "toolbar_removed", G_CALLBACK (toolbar_removed), NULL);
}

static void
ephy_toolbars_model_finalize (GObject *object)
{
	EphyToolbarsModel *t = EPHY_TOOLBARS_MODEL (object);

	save_changes (t);

	g_object_unref (t->priv->bookmarks);

	g_free (t->priv->xml_file);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyToolbarsModel *
ephy_toolbars_model_new (EphyBookmarks *bookmarks)
{
	return EPHY_TOOLBARS_MODEL (g_object_new (EPHY_TYPE_TOOLBARS_MODEL,
						  "bookmarks", bookmarks,
						  NULL));
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
				     long id)
{
	char *action_name;
	int toolbar, position;

	action_name = ephy_toolbars_model_get_action_name (model, id);
	g_return_if_fail (action_name != NULL);

	if (get_toolbar_and_item_pos (model, action_name, &toolbar, &position))
	{
		egg_toolbars_model_remove_item (EGG_TOOLBARS_MODEL (model),
					        toolbar, position);
	}

	g_free (action_name);
}

void
ephy_toolbars_model_add_bookmark (EphyToolbarsModel *model,
				  gboolean topic,
				  long id)
{
	char *name;
	int toolbar_position;

	toolbar_position = get_toolbar_pos (model, "BookmarksBar");
	g_return_if_fail (toolbar_position != -1);

	name = ephy_toolbars_model_get_action_name (model, id);
	egg_toolbars_model_add_item (EGG_TOOLBARS_MODEL (model),
				     toolbar_position, -1, name,
				     topic ? EPHY_DND_TOPIC_TYPE :
					     EPHY_DND_URL_TYPE);
	g_free (name);
}

gboolean
ephy_toolbars_model_has_bookmark (EphyToolbarsModel *model,
				  long id)
{
	char *action_name;
	gboolean found;
	int toolbar, pos;

	action_name = ephy_toolbars_model_get_action_name (model, id);
	g_return_val_if_fail (action_name != NULL, FALSE);

	found = get_toolbar_and_item_pos (model, action_name, &toolbar, &pos);

	g_free (action_name);

	return found;
}
