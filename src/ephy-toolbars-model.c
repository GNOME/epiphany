/*
 *  Copyright © 2002-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-toolbars-model.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "eggtypebuiltins.h"
#include "ephy-debug.h"

#include <string.h>

#define EPHY_TOOLBARS_XML_FILE		"epiphany-toolbars-3.xml"
#define EPHY_TOOLBARS_XML_VERSION	"1.1"

#define EPHY_TOOLBARS_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOOLBARS_MODEL, EphyToolbarsModelPrivate))

struct _EphyToolbarsModelPrivate
{
	char *xml_file;
	EggTbModelFlags style;
	guint timeout;
	guint style_notifier_id;
};

static void ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass);
static void ephy_toolbars_model_init       (EphyToolbarsModel *model);

G_DEFINE_TYPE (EphyToolbarsModel, ephy_toolbars_model, EGG_TYPE_TOOLBARS_MODEL)

static gboolean
save_changes_idle (EphyToolbarsModel *model)
{
	LOG ("Saving toolbars model");

	egg_toolbars_model_save_toolbars
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
update_flags (EphyToolbarsModel *model)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	int i, n_toolbars;
	int flag = 0;

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
		flags &= ~(EGG_TB_MODEL_NOT_REMOVABLE | EGG_TB_MODEL_STYLES_MASK);
		flags |= flag;
		flags |= model->priv->style;
		egg_toolbars_model_set_flags (eggmodel, i, flags);
	}
}

static void
update_flags_and_save_changes (EphyToolbarsModel *model)
{
	update_flags (model);
	save_changes (model);
}

static EggTbModelFlags
get_toolbar_style (void)
{
	GFlagsClass *flags_class;
	const GFlagsValue *value;
	EggTbModelFlags flags = 0;
	char *pref;

	pref = eel_gconf_get_string (CONF_INTERFACE_TOOLBAR_STYLE);
	if (pref != NULL)
	{
		flags_class = g_type_class_ref (EGG_TYPE_TB_MODEL_FLAGS);
		value = g_flags_get_value_by_nick (flags_class, pref);
		if (value != NULL)
		{
			flags = value->value;
		}
		g_type_class_unref (flags_class);
	}
	flags &= EGG_TB_MODEL_STYLES_MASK;

	g_free (pref);

	return flags;
}

static void
toolbar_style_notifier (GConfClient *client,
			guint cnxn_id,
			GConfEntry *entry,
			EphyToolbarsModel *model)
{
	model->priv->style = get_toolbar_style ();

	update_flags (model);
}

void
ephy_toolbars_model_load (EphyToolbarsModel *model)
{
	EggToolbarsModel *eggmodel = EGG_TOOLBARS_MODEL (model);
	gboolean success;
	int i;

	egg_toolbars_model_load_names (eggmodel, ephy_file ("epiphany-toolbar.xml"));
  
	success = egg_toolbars_model_load_toolbars (eggmodel, model->priv->xml_file);
	LOG ("Loading the toolbars was %ssuccessful", success ? "" : "un");

	/* maybe an old format, try to migrate: load the old layout, and
	 * remove the BookmarksBar toolbar
	 */
	if (success == FALSE)
	{
		char *old_xml;

		old_xml = g_build_filename (ephy_dot_dir (),
					    "epiphany-toolbars-2.xml",
					    NULL);
		success = egg_toolbars_model_load_toolbars (eggmodel, old_xml);
		g_free (old_xml);

		if (success == TRUE)
		{
			old_xml = g_build_filename (ephy_dot_dir (),
						    "epiphany-bookmarksbar.xml",
						    NULL);
			egg_toolbars_model_load_toolbars (eggmodel, old_xml);
			g_free (old_xml);
		}

		LOG ("Migration was %ssuccessful", success ? "" : "un");
	}
	
	if (success == FALSE)
	{
		char *old_xml;

		old_xml = g_build_filename (ephy_dot_dir (),
					    "epiphany-toolbars.xml",
					    NULL);
		success = egg_toolbars_model_load_toolbars (eggmodel, old_xml);
		g_free (old_xml);

		LOG ("Migration was %ssuccessful", success ? "" : "un");
	}

	/* Still no success, load the default toolbars */
	if (success == FALSE)
	{
		success = egg_toolbars_model_load_toolbars
				(eggmodel, ephy_file ("epiphany-toolbar.xml"));
		LOG ("Loading the default toolbars was %ssuccessful", success ? "" : "un");
	}

	/* Cleanup any empty toolbars */
	for (i = egg_toolbars_model_n_toolbars (eggmodel)-1; i >= 0; i--)
	{
		if (egg_toolbars_model_n_items (eggmodel, i) == 0)
		{
			egg_toolbars_model_remove_toolbar (eggmodel, i);
		}
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
	EphyToolbarsModelPrivate *priv;

	priv = model->priv = EPHY_TOOLBARS_MODEL_GET_PRIVATE (model);

	priv->xml_file = g_build_filename (ephy_dot_dir (),
					   EPHY_TOOLBARS_XML_FILE,
					   NULL);

	priv->style = get_toolbar_style ();
	priv->style_notifier_id = eel_gconf_notification_add
		(CONF_INTERFACE_TOOLBAR_STYLE,
		 (GConfClientNotifyFunc) toolbar_style_notifier, model);
	
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
ephy_toolbars_model_dispose (GObject *object)
{
	EphyToolbarsModel *model = EPHY_TOOLBARS_MODEL (object);

	save_changes_idle (model);

	G_OBJECT_CLASS (ephy_toolbars_model_parent_class)->dispose (object);
}

static void
ephy_toolbars_model_finalize (GObject *object)
{
	EphyToolbarsModel *model = EPHY_TOOLBARS_MODEL (object);
	EphyToolbarsModelPrivate *priv = model->priv;

	if (priv->style_notifier_id != 0)
	{
		eel_gconf_notification_remove (priv->style_notifier_id);
	}

	if (priv->timeout != 0)
	{
		g_source_remove (priv->timeout);
	}

	g_free (priv->xml_file);

	G_OBJECT_CLASS (ephy_toolbars_model_parent_class)->finalize (object);
}

static void
ephy_toolbars_model_class_init (EphyToolbarsModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ephy_toolbars_model_dispose;
	object_class->finalize = ephy_toolbars_model_finalize;

	g_type_class_add_private (object_class, sizeof (EphyToolbarsModelPrivate));
}

EggToolbarsModel *
ephy_toolbars_model_new (void)
{
	return EGG_TOOLBARS_MODEL (g_object_new (EPHY_TYPE_TOOLBARS_MODEL, NULL));
}
