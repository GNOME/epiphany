/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "ephy-spinner-action.h"
#include "ephy-spinner.h"
#include "eggtoolitem.h"
#include "eggtoolbar.h"

static void ephy_spinner_action_init       (EphySpinnerAction *action);
static void ephy_spinner_action_class_init (EphySpinnerActionClass *class);

struct EphySpinnerActionPrivate
{
	gboolean throbbing;
};

enum
{
	PROP_0,
	PROP_THROBBING
};

static GObjectClass *parent_class = NULL;

GType
ephy_spinner_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphySpinnerActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_spinner_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphySpinnerAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_spinner_action_init,
		};

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphySpinnerAction",
					       &type_info, 0);
	}
	return type;
}

static void
ephy_spinner_action_sync_throbbing (EggAction *action, GParamSpec *pspec,
			            GtkWidget *proxy)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (g_object_get_data (G_OBJECT (proxy), "spinner"));

	if (EPHY_SPINNER_ACTION (action)->priv->throbbing)
	{
		ephy_spinner_start (spinner);
	}
	else
	{
		ephy_spinner_stop (spinner);
	}
}

static GtkWidget *
create_tool_item (EggAction *action)
{
	GtkWidget *item;
	GtkWidget *spinner;

	item = GTK_WIDGET (egg_tool_item_new ());

	spinner = ephy_spinner_new ();
	gtk_widget_show (spinner);
	gtk_container_add (GTK_CONTAINER (item), spinner);
	egg_tool_item_set_pack_end (EGG_TOOL_ITEM (item), TRUE);
	egg_tool_item_set_homogeneous (EGG_TOOL_ITEM (item), FALSE);
	g_object_set_data (G_OBJECT (item), "spinner", spinner);

	return item;
}

static void
toolbar_style_sync (EggToolbar *toolbar,
		    GtkToolbarStyle style,
		    GtkWidget *proxy)
{
	gboolean small;
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (g_object_get_data (G_OBJECT (proxy), "spinner"));

	small = (style != GTK_TOOLBAR_BOTH);
	ephy_spinner_set_small_mode (EPHY_SPINNER (spinner), small);
}

static void
item_parent_set_cb (GtkWidget *item, GtkWidget *previous_parent)
{
	EggToolbar *toolbar;
	GtkToolbarStyle style;

	if (item->parent == NULL) return;

	toolbar = EGG_TOOLBAR (item->parent);

	g_signal_connect_object (toolbar, "style_changed",
			         G_CALLBACK (toolbar_style_sync),
			         item, 0);

	style = egg_toolbar_get_style (toolbar);
	toolbar_style_sync (toolbar, style, item);
}

static void
connect_proxy (EggAction *action, GtkWidget *proxy)
{
	g_signal_connect_object (action, "notify::throbbing",
				 G_CALLBACK (ephy_spinner_action_sync_throbbing),
				 proxy, 0);
	g_signal_connect_object (proxy, "parent_set",
			         G_CALLBACK (item_parent_set_cb),
			         proxy, 0);

	(* EGG_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
ephy_spinner_action_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	EphySpinnerAction *spin;

	spin = EPHY_SPINNER_ACTION (object);

	switch (prop_id)
	{
		case PROP_THROBBING:
			spin->priv->throbbing = g_value_get_boolean (value);
			g_object_notify(object, "throbbing");
			break;
	}
}

static void
ephy_spinner_action_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	EphySpinnerAction *spin;

	spin = EPHY_SPINNER_ACTION (object);

	switch (prop_id)
	{
		case PROP_THROBBING:
			g_value_set_boolean (value, spin->priv->throbbing);
			break;
	}
}

static void
ephy_spinner_action_finalize (GObject *object)
{
	EphySpinnerAction *action = EPHY_SPINNER_ACTION (object);

	g_return_if_fail (action->priv != NULL);

	g_free (action->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_spinner_action_class_init (EphySpinnerActionClass *class)
{
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->toolbar_item_type = EPHY_SPINNER_TYPE;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	object_class->finalize = ephy_spinner_action_finalize;
	object_class->set_property = ephy_spinner_action_set_property;
	object_class->get_property = ephy_spinner_action_get_property;

	g_object_class_install_property (object_class,
                                         PROP_THROBBING,
                                         g_param_spec_boolean ("throbbing",
                                                               "Throbbing",
                                                               "Throbbing",
                                                               FALSE,
                                                               G_PARAM_READWRITE));
}

static void
ephy_spinner_action_init (EphySpinnerAction *action)
{
	action->priv = g_new0 (EphySpinnerActionPrivate, 1);
}
