/*
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003-2004 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-bookmarksbar-model.h"
#include "ephy-bookmarks.h"
#include "ephy-dnd.h"
#include "ephy-node-common.h"
#include "ephy-file-helpers.h"
#include "ephy-history.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <string.h>
#include <glib/gi18n.h>

#define EPHY_BOOKMARKSBARS_XML_FILE	"epiphany-bookmarksbar.xml"
#define EPHY_BOOKMARKSBARS_XML_VERSION	"1.0"

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

#define EPHY_BOOKMARKSBAR_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKSBAR_MODEL, EphyBookmarksBarModelPrivate))

struct EphyBookmarksBarModelPrivate
{
	EphyBookmarks *bookmarks;
	char *xml_file;
	guint timeout;
};

static void ephy_bookmarksbar_model_class_init (EphyBookmarksBarModelClass *klass);
static void ephy_bookmarksbar_model_init       (EphyBookmarksBarModel *t);

static GObjectClass *parent_class = NULL;

GType
ephy_bookmarksbar_model_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info = {
		sizeof (EphyBookmarksBarModelClass),
		NULL,			/* base_init */
		NULL,			/* base_finalize */
		(GClassInitFunc) ephy_bookmarksbar_model_class_init,
		NULL,
		NULL,			/* class_data */
		sizeof (EphyBookmarksBarModel),
		0,			/* n_preallocs */
		(GInstanceInitFunc) ephy_bookmarksbar_model_init
	};

	type = g_type_register_static (EGG_TYPE_TOOLBARS_MODEL,
				       "EphyBookmarksBarModel",
				       &our_info, 0);
	}

	return type;
}

static gboolean
get_toolbar_and_item_pos (EphyBookmarksBarModel *model,
			  const char *name,
			  int *toolbar,
			  int *position)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	int n_toolbars, n_items;
	int t,i;

	n_toolbars = egg_toolbars_model_n_toolbars (eggmodel);

	for (t = 0; t < n_toolbars; t++)
	{
		n_items = egg_toolbars_model_n_items (eggmodel, t);

		for (i = 0; i < n_items; i++)
		{
			const char *i_name;
			gboolean is_separator;

			egg_toolbars_model_item_nth (eggmodel, t,
						     i, &is_separator, NULL, &i_name);
			g_return_val_if_fail (i_name != NULL, FALSE);

			if (strcmp (i_name, name) == 0)
			{
				if (toolbar) *toolbar = t;
				if (position) *position = i;

				return TRUE;
			}
		}
	}

	return FALSE;
}

static int
get_toolbar_pos (EphyBookmarksBarModel *model,
		 const char *name)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	int i, n_toolbars;

	n_toolbars = egg_toolbars_model_n_toolbars (eggmodel);

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth (eggmodel, i);
		if (strcmp (name, t_name) == 0)
		{
			return i;
		}
	}

	return -1;
}

char *
ephy_bookmarksbar_model_get_action_name (EphyBookmarksBarModel *model,
					 long id)
{
	return g_strdup_printf ("GoBookmark-%ld", id);
}

EphyNode *
ephy_bookmarksbar_model_get_node (EphyBookmarksBarModel *model,
				  const char *action_name)
{
	EphyBookmarks *bookmarks = EPHY_BOOKMARKSBAR_MODEL (model)->priv->bookmarks;
	long node_id;

	if (!ephy_string_to_int (action_name + strlen ("GoBookmark-"), &node_id))
	{
		return NULL;
	}

	return ephy_bookmarks_get_from_id (bookmarks, node_id);
}

void
ephy_bookmarksbar_model_add_bookmark (EphyBookmarksBarModel *model,
				      gboolean topic,
				      long id)
{
	char *name;
	int toolbar_position;

	toolbar_position = get_toolbar_pos (model, "BookmarksBar");
	g_return_if_fail (toolbar_position != -1);

	name = ephy_bookmarksbar_model_get_action_name (model, id);
	egg_toolbars_model_add_item (EGG_TOOLBARS_MODEL (model),
				    toolbar_position, -1, name,
				    topic ? EPHY_DND_TOPIC_TYPE :
					    EPHY_DND_URL_TYPE);
	g_free (name);
}

void
ephy_bookmarksbar_model_remove_bookmark (EphyBookmarksBarModel *model,
					 long id)
{
/*	char *action_name;
	int toolbar, position;

	action_name = ephy_bookmarksbar_model_get_action_name (model, id);
	g_return_if_fail (action_name != NULL);

	if (get_toolbar_and_item_pos (model, action_name, &toolbar, &position))
	{
		egg_toolbars_model_remove_item (EGG_TOOLBARS_MODEL (model),
						toolbar, position);
	}

	g_free (action_name);*/
}

gboolean
ephy_bookmarksbar_model_has_bookmark (EphyBookmarksBarModel *model,
				      long id)
{
	char *action_name;
	gboolean found;
	int toolbar, pos;

	action_name = ephy_bookmarksbar_model_get_action_name (model, id);
	g_return_val_if_fail (action_name != NULL, FALSE);

	found = get_toolbar_and_item_pos (model, action_name, &toolbar, &pos);

	g_free (action_name);

	return found;
}

static gboolean
save_changes_idle (EphyBookmarksBarModel *model)
{
	LOG ("Saving bookmarks toolbars model")

	egg_toolbars_model_save
		(EGG_TOOLBARS_MODEL (model),
		 model->priv->xml_file,
		 EPHY_BOOKMARKSBARS_XML_VERSION);

	model->priv->timeout = 0;

	/* don't run again */
	return FALSE;
}

static void
save_changes (EphyBookmarksBarModel *model)
{
	if (model->priv->timeout == 0)
	{
		model->priv->timeout =
			g_idle_add ((GSourceFunc) save_changes_idle, model);
	}
}

static void
update_flags_and_save_changes (EphyBookmarksBarModel *model)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	int i, n_toolbars;
	int flag = 0;

	n_toolbars = egg_toolbars_model_n_toolbars (eggmodel);

	if (n_toolbars <= 1)
	{
		flag = EGG_TB_MODEL_NOT_REMOVABLE;
	}

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth (eggmodel, i);
		g_return_if_fail (t_name != NULL);

		egg_toolbars_model_set_flags (eggmodel, flag, i);
	}

	save_changes (model);
}

static void
bookmark_destroy_cb (EphyNode *node,
		     EphyBookmarksBarModel *model)
{
	long id;

	id = ephy_node_get_id (node);
	ephy_bookmarksbar_model_remove_bookmark (model, id);
}

static void
item_added_cb (EphyBookmarksBarModel *model,
	       int toolbar_position,
	       int position)
{
	EphyNode *node;
	const char *name;
	gboolean is_separator;

	egg_toolbars_model_item_nth (EGG_TOOLBARS_MODEL (model), toolbar_position,
				     position, &is_separator, NULL, &name);
	if (!is_separator && g_str_has_prefix (name, "GoBookmark-"))
	{
		node = ephy_bookmarksbar_model_get_node (model, name);
		g_return_if_fail (node != NULL);

		ephy_node_signal_connect_object (node,
						 EPHY_NODE_DESTROY,
						 (EphyNodeCallback) bookmark_destroy_cb,
						 G_OBJECT (model));
	}

	save_changes (model);
}

static gboolean
impl_add_item (EggToolbarsModel *eggmodel,
	       int toolbar_position,
	       int position,
	       const char *name,
	       const char *type)
{
	EphyBookmarksBarModel *model = EPHY_BOOKMARKSBAR_MODEL (eggmodel);
	gboolean is_bookmark;

	is_bookmark = strcmp (type, EPHY_DND_TOPIC_TYPE) == 0 ||
		      strcmp (type, EPHY_DND_URL_TYPE) == 0;

	if (!is_bookmark || !get_toolbar_and_item_pos (model, name, NULL, NULL))
	{
		return EGG_TOOLBARS_MODEL_CLASS (parent_class)->add_item
			(eggmodel, toolbar_position, position, name, type);
	}

	return FALSE;
}

static char *
impl_get_item_id (EggToolbarsModel *eggmodel,
		  const char *type,
		  const char *name)
{
	EphyBookmarksBarModel *model = EPHY_BOOKMARKSBAR_MODEL (eggmodel);
	EphyBookmarks *bookmarks = model->priv->bookmarks;

	if (strcmp (type, EPHY_DND_TOPIC_TYPE) == 0)
	{
		EphyNode *topic;

		topic = ephy_bookmarks_find_keyword (bookmarks, name, FALSE);
		if (topic == NULL) return NULL;

		return ephy_bookmarksbar_model_get_action_name
			(model, ephy_node_get_id (topic));
	}
	else if (strcmp (type, EPHY_DND_URL_TYPE) == 0)
	{
		EphyNode *node = NULL;
		gchar **netscape_url;

		netscape_url = g_strsplit (name, "\n", 2);
		if (!netscape_url || !netscape_url[URL]) return NULL;

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

		return ephy_bookmarksbar_model_get_action_name
			(model, ephy_node_get_id (node));
	}

	return EGG_TOOLBARS_MODEL_CLASS (parent_class)->get_item_id (eggmodel, type, name);
}

static char *
impl_get_item_type (EggToolbarsModel *model,
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

	return EGG_TOOLBARS_MODEL_CLASS (parent_class)->get_item_type (model, type);
}

static void
load_toolbars (EphyBookmarksBarModel *model)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	gboolean success = FALSE;

	success = egg_toolbars_model_load (eggmodel, model->priv->xml_file);
	LOG ("Loading the toolbars was %ssuccessful", success ? "" : "un")

	/* Try migration first: load the old layout, and remove every toolbar
	 * except the BookmarksBar toolbar
	 */
	if (success == FALSE)
	{
		char *old_xml;
		int i, n_toolbars;

		old_xml = g_build_filename (ephy_dot_dir (),
					    "epiphany-toolbars.xml",
					    NULL);
		success = egg_toolbars_model_load (eggmodel, old_xml);
		g_free (old_xml);

		if (success)
		{
			n_toolbars = egg_toolbars_model_n_toolbars (eggmodel);

			for (i = n_toolbars - 1; i >= 0; i--)
			{
				const char *t_name;

				t_name = egg_toolbars_model_toolbar_nth (eggmodel, i);
				g_return_if_fail (t_name != NULL);

				if (strcmp (t_name, "BookmarksBar") != 0)
				{
					egg_toolbars_model_remove_toolbar (eggmodel, i);
				}
			}
		}

		LOG ("Migration was %ssuccessful", success ? "" : "un")
	}

	/* Load default set */
	if (success == FALSE)
	{
		egg_toolbars_model_load
			(eggmodel, ephy_file ("epiphany-bookmarksbar.xml"));	
		LOG ("Loading the default toolbars was %ssuccessful", success ? "" : "un")
	}
	
	/* Ensure that we have a BookmarksBar */
	if (get_toolbar_pos (model, "BookmarksBar") == -1)
	{
		egg_toolbars_model_add_toolbar
			(eggmodel, -1, "BookmarksBar");
	}
}	

static void
ephy_bookmarksbar_model_init (EphyBookmarksBarModel *model)
{
	model->priv = EPHY_BOOKMARKSBAR_MODEL_GET_PRIVATE (model);

	model->priv->xml_file = g_build_filename (ephy_dot_dir (),
						  EPHY_BOOKMARKSBARS_XML_FILE,
						  NULL);

	g_signal_connect_after (model, "item_added",
				G_CALLBACK (item_added_cb), NULL);
	g_signal_connect_after (model, "item_removed",
				G_CALLBACK (save_changes), NULL);
	g_signal_connect_after (model, "toolbar_added",
				G_CALLBACK (update_flags_and_save_changes), NULL);
	g_signal_connect_after (model, "toolbar_removed",
				G_CALLBACK (update_flags_and_save_changes), NULL);
}

static void
ephy_bookmarksbar_model_finalize (GObject *object)
{
	EphyBookmarksBarModel *model = EPHY_BOOKMARKSBAR_MODEL (object);

	if (model->priv->timeout != 0)
	{
		g_source_remove (model->priv->timeout);
		model->priv->timeout = 0;
	}

	g_free (model->priv->xml_file);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmarksbar_model_set_property (GObject *object,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
	EphyBookmarksBarModel *model = EPHY_BOOKMARKSBAR_MODEL (object);

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			/* we're owned by bookmarks, so don't g_object_ref() here */
			model->priv->bookmarks = g_value_get_object (value);
			load_toolbars (model);
			break;
	}
}

static void
ephy_bookmarksbar_model_get_property (GObject *object,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_bookmarksbar_model_class_init (EphyBookmarksBarModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EggToolbarsModelClass *eggclass = EGG_TOOLBARS_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_bookmarksbar_model_finalize;
	object_class->set_property = ephy_bookmarksbar_model_set_property;
	object_class->get_property = ephy_bookmarksbar_model_get_property;

	eggclass->add_item = impl_add_item;
	eggclass->get_item_id = impl_get_item_id;
	eggclass->get_item_type = impl_get_item_type;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks",
							      "Bookmarks",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyBookmarksBarModelPrivate));
}

EggToolbarsModel *
ephy_bookmarksbar_model_new (EphyBookmarks *bookmarks)
{
	return EGG_TOOLBARS_MODEL (g_object_new (EPHY_TYPE_BOOKMARKSBAR_MODEL,
						 "bookmarks", bookmarks,
						 NULL));
}
