/*
 *  Copyright (C) 2002-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "ephy-toolbars-model.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <string.h>

#define EPHY_TOOLBARS_XML_FILE		"epiphany-toolbars-2.xml"
#define EPHY_TOOLBARS_XML_VERSION	"1.0"

#define EPHY_TOOLBARS_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBARS_MODEL, EphyToolbarsModelPrivate))

struct EphyToolbarsModelPrivate
{
	char *xml_file;
	guint timeout;
};

static void ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass);
static void ephy_toolbars_model_init       (EphyToolbarsModel *model);

static GObjectClass *parent_class = NULL;

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

static gboolean
save_changes_idle (EphyToolbarsModel *model)
{
	LOG ("Saving toolbars model")

	egg_toolbars_model_save
		(EGG_TOOLBARS_MODEL (model),
		 model->priv->xml_file,
		 EPHY_TOOLBARS_XML_VERSION);

	model->priv->timeout = 0;

	/* don't run again */
	return FALSE;
}

static void
save_changes (EphyToolbarsModel *model)
{
	if (model->priv->timeout == 0)
	{
		model->priv->timeout =
			g_idle_add ((GSourceFunc) save_changes_idle, model);
	}
}

static void
update_flags_and_save_changes (EphyToolbarsModel *model)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	int i, n_toolbars;
	int flag = EGG_TB_MODEL_ACCEPT_ITEMS_ONLY;

	n_toolbars = egg_toolbars_model_n_toolbars (eggmodel);

	if (n_toolbars <= 1)
	{
		flag |= EGG_TB_MODEL_NOT_REMOVABLE;
	}

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;
		EggTbModelFlags flags;

		t_name = egg_toolbars_model_toolbar_nth (eggmodel, i);
		g_return_if_fail (t_name != NULL);

		flags = egg_toolbars_model_get_flags (eggmodel, i);
		egg_toolbars_model_set_flags (eggmodel, flags | flag, i);
	}

	save_changes (model);
}

static int
get_toolbar_pos (EggToolbarsModel *model,
		 const char *name)
{
	int i, n_toolbars;

	n_toolbars = egg_toolbars_model_n_toolbars (model);

	for (i = 0; i < n_toolbars; i++)
	{
		const char *t_name;

		t_name = egg_toolbars_model_toolbar_nth (model, i);
		g_return_val_if_fail (t_name != NULL, -1);
		if (strcmp (name, t_name) == 0)
		{
			return i;
		}
	}

	return -1;
}

void
ephy_toolbars_model_load (EphyToolbarsModel *model)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	gboolean success;

	success = egg_toolbars_model_load (eggmodel, model->priv->xml_file);
	LOG ("Loading the toolbars was %ssuccessful", success ? "" : "un")

	/* maybe an old format, try to migrate: load the old layout, and
	 * remove the BookmarksBar toolbar
	 */
	if (success == FALSE)
	{
		char *old_xml;
		int toolbar;

		old_xml = g_build_filename (ephy_dot_dir (),
					    "epiphany-toolbars.xml",
					    NULL);
		success = egg_toolbars_model_load (eggmodel, old_xml);
		g_free (old_xml);

		if (success)
		{
			toolbar = get_toolbar_pos (eggmodel, "BookmarksBar");
			if (toolbar != -1)
			{
				egg_toolbars_model_remove_toolbar (eggmodel, toolbar);
			}
		}

		LOG ("Migration was %ssuccessful", success ? "" : "un")
	}

	/* Still no success, load the default toolbars */
	if (success == FALSE)
	{
		success = egg_toolbars_model_load
				(eggmodel, ephy_file ("epiphany-toolbar.xml"));
		LOG ("Loading the default toolbars was %ssuccessful", success ? "" : "un")
	}

	/* Ensure we have at least 1 toolbar */
	if (egg_toolbars_model_n_toolbars (eggmodel) < 1)
	{
		egg_toolbars_model_add_toolbar (eggmodel, 0, "DefaultToolbar");
	}
}

static void
ephy_toolbars_model_init (EphyToolbarsModel *model)
{
	model->priv = EPHY_TOOLBARS_MODEL_GET_PRIVATE (model);

	model->priv->xml_file = g_build_filename (ephy_dot_dir (),
						  EPHY_TOOLBARS_XML_FILE,
						  NULL);

	g_signal_connect_after (model, "item_added",
				G_CALLBACK (save_changes), NULL);
	g_signal_connect_after (model, "item_removed",
				G_CALLBACK (save_changes), NULL);
	g_signal_connect_after (model, "toolbar_added",
				G_CALLBACK (update_flags_and_save_changes), NULL);
	g_signal_connect_after (model, "toolbar_removed",
				G_CALLBACK (update_flags_and_save_changes), NULL);
}

static void
ephy_toolbars_model_finalize (GObject *object)
{
	EphyToolbarsModel *model = EPHY_TOOLBARS_MODEL (object);

	if (model->priv->timeout != 0)
	{
		g_source_remove (model->priv->timeout);
		model->priv->timeout = 0;
	}

	/* FIXME: we should detect when item data changes, and save then instead */
	save_changes_idle (model);

	g_free (model->priv->xml_file);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_toolbars_model_finalize;

	g_type_class_add_private (object_class, sizeof (EphyToolbarsModelPrivate));
}

EggToolbarsModel *
ephy_toolbars_model_new (void)
{
	return EGG_TOOLBARS_MODEL (g_object_new (EPHY_TYPE_TOOLBARS_MODEL, NULL));
}
