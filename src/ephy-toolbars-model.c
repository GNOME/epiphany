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

#include "ephy-toolbars-model.h"
#include "ephy-dnd.h"
#include "ephy-new-bookmark.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

static void ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass);
static void ephy_toolbars_model_init       (EphyToolbarsModel *t);
static void ephy_toolbars_model_finalize   (GObject *object);

enum
{
  ACTION_ADDED,
  LAST_SIGNAL
};

static guint ephy_toolbars_model_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

struct EphyToolbarsModelPrivate
{
	gpointer dummy;
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

static const char *
impl_add_item (EggToolbarsModel *t,
	       int toolbar_position,
	       int position,
	       GdkAtom type,
	       const char *name)
{
	EphyBookmarks *bookmarks;
	char *action_name = NULL;
	const char *res;

	LOG ("Add item %s", name)

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	if (gdk_atom_intern (EPHY_DND_TOPIC_TYPE, FALSE) == type)
	{
		GList *nodes;
		int id;

		nodes = ephy_dnd_node_list_extract_nodes (name);
		id = ephy_node_get_id (EPHY_NODE (nodes->data));
		action_name = g_strdup_printf ("GoTopicId%d", id);
		g_list_free (nodes);
	}
	else if (gdk_atom_intern (EPHY_DND_BOOKMARK_TYPE, FALSE) == type)
	{
		GList *nodes;
		int id;

		nodes = ephy_dnd_node_list_extract_nodes (name);
		id = ephy_node_get_id (EPHY_NODE (nodes->data));
		action_name = g_strdup_printf ("GoBookmarkId%d", id);
		g_list_free (nodes);
	}

	res = action_name ? action_name : name;

	g_signal_emit (G_OBJECT (t),
		       ephy_toolbars_model_signals[ACTION_ADDED], 0, res);

	EGG_TOOLBARS_MODEL_CLASS (parent_class)->add_item
		(t, toolbar_position, position, type, res);

	return res;
}

static void
ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EggToolbarsModelClass *etm_class;

	etm_class = EGG_TOOLBARS_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_toolbars_model_finalize;

	etm_class->add_item = impl_add_item;

	ephy_toolbars_model_signals[ACTION_ADDED] =
	g_signal_new ("action_added",
		      G_OBJECT_CLASS_TYPE (object_class),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyToolbarsModelClass, action_added),
		      NULL, NULL, g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
ephy_toolbars_model_init (EphyToolbarsModel *t)
{
	t->priv = g_new0 (EphyToolbarsModelPrivate, 1);
}

static void
ephy_toolbars_model_finalize (GObject *object)
{
	EphyToolbarsModel *t = EPHY_TOOLBARS_MODEL (object);

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EPHY_TOOLBARS_MODEL (object));

	g_free (t->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyToolbarsModel *
ephy_toolbars_model_new (void)
{
	EphyToolbarsModel *t;

	t = EPHY_TOOLBARS_MODEL (g_object_new (EPHY_TOOLBARS_MODEL_TYPE, NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}
