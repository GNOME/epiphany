/*
 *  Copyright (C) 2003  Christian Persch
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

#include "ephy-zoom-control.h"
#include "ephy-marshal.h"
#include "ephy-zoom.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktooltips.h>
#include <glib/gi18n.h>

#define EPHY_ZOOM_CONTROL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ZOOM_CONTROL, EphyZoomControlPrivate))

struct _EphyZoomControlPrivate {
	GtkComboBox *combo;
	float zoom;
	guint handler_id;
};

enum
{
	PROP_0,
	PROP_ZOOM
};

enum
{
	ZOOM_TO_LEVEL_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

static void	ephy_zoom_control_class_init	(EphyZoomControlClass *klass);
static void	ephy_zoom_control_init		(EphyZoomControl *control);
static void	ephy_zoom_control_finalize	(GObject *o);

#define MENU_ID "ephy-zoom-control-menu-id"

GType
ephy_zoom_control_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
			{
				sizeof (EphyZoomControlClass),
				NULL, /* base_init */
				NULL, /* base_finalize */
				(GClassInitFunc) ephy_zoom_control_class_init,
				NULL,
				NULL, /* class_data */
				sizeof (EphyZoomControl),
				0, /* n_preallocs */
				(GInstanceInitFunc) ephy_zoom_control_init,
			};

		type = g_type_register_static (GTK_TYPE_TOOL_ITEM,
					       "EphyZoomControl",
					       &our_info, 0);
	}

	return type;
}

static void
proxy_menu_activate_cb (GtkMenuItem *menu_item, EphyZoomControl *control)
{
	gint index;
	float zoom;

	/* menu item was toggled OFF */
	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item))) return;

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "zoom-level"));
	zoom = zoom_levels[index].level;

	if (zoom != control->priv->zoom)
	{
		g_signal_emit (control, signals[ZOOM_TO_LEVEL_SIGNAL], 0, zoom);
	}
}

static gboolean
ephy_zoom_control_create_menu_proxy (GtkToolItem *item)
{
	EphyZoomControl *control = EPHY_ZOOM_CONTROL (item);
	EphyZoomControlPrivate *p = control->priv;
	GtkWidget *menu, *menu_item;
	GSList *group = NULL;
	gint i;

	menu = gtk_menu_new ();

	for (i = 0; i < n_zoom_levels; i++)
	{
		menu_item = gtk_radio_menu_item_new_with_label (group, _(zoom_levels[i].name));
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));

		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
						p->zoom == zoom_levels[i].level);

		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

		g_object_set_data (G_OBJECT (menu_item), "zoom-level", GINT_TO_POINTER (i));
		g_signal_connect_object (G_OBJECT (menu_item), "activate",
					 G_CALLBACK (proxy_menu_activate_cb), control, 0);
	}

	menu_item = gtk_menu_item_new_with_mnemonic (_("_Zoom"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);

	gtk_widget_show (menu);
	gtk_widget_show (menu_item);

	g_object_ref (menu_item);
	gtk_object_sink (GTK_OBJECT (menu_item));
	gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
	g_object_unref (menu_item);

	return TRUE;
}

static void
combo_changed_cb (GtkComboBox *combo, EphyZoomControl *control)
{
	gint index;
	float zoom;

	index = gtk_combo_box_get_active (combo);
	zoom = zoom_levels[index].level;

	if (zoom != control->priv->zoom)
	{
		g_signal_emit (control, signals[ZOOM_TO_LEVEL_SIGNAL], 0, zoom);	
	}
}

static void
sync_zoom_cb (EphyZoomControl *control, GParamSpec *pspec, gpointer data)
{
	EphyZoomControlPrivate *p = control->priv;
	guint index;

	index = ephy_zoom_get_zoom_level_index (p->zoom);

	g_signal_handler_block (p->combo, p->handler_id);
	gtk_combo_box_set_active (p->combo, index);
	g_signal_handler_unblock (p->combo, p->handler_id);	
}

static void
ephy_zoom_control_init (EphyZoomControl *control)
{
	EphyZoomControlPrivate *p;
	GtkComboBox *combo;
	GtkWidget *box;
	guint i;

	p = EPHY_ZOOM_CONTROL_GET_PRIVATE (control);
	control->priv = p;

	p->zoom = 1.0;

	combo = GTK_COMBO_BOX (gtk_combo_box_new_text ());

	for (i = 0; i < n_zoom_levels; i++)
	{
		gtk_combo_box_append_text (combo, _(zoom_levels[i].name));
	}

	p->combo = combo;
	g_object_ref (combo);
	gtk_object_sink (GTK_OBJECT (combo));
	gtk_widget_show (GTK_WIDGET (combo));

	i = ephy_zoom_get_zoom_level_index (p->zoom);
	gtk_combo_box_set_active (combo, i);

	box = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (combo), TRUE, FALSE, 0);
	gtk_widget_show (box);

	gtk_container_add (GTK_CONTAINER (control), box);

	p->handler_id = g_signal_connect (combo, "changed",
					  G_CALLBACK (combo_changed_cb), control);
	
	g_signal_connect_object (control, "notify::zoom",
				 G_CALLBACK (sync_zoom_cb), NULL, 0);
}

static void
ephy_zoom_control_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyZoomControl *control;
	EphyZoomControlPrivate *p;

	control = EPHY_ZOOM_CONTROL (object);
	p = control->priv;

	switch (prop_id)
	{
		case PROP_ZOOM:
			p->zoom = g_value_get_float (value);
			g_object_notify (object, "zoom");
			break;
	}
}

static void
ephy_zoom_control_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EphyZoomControl *control;
	EphyZoomControlPrivate *p;

	control = EPHY_ZOOM_CONTROL (object);
	p = control->priv;

	switch (prop_id)
	{
		case PROP_ZOOM:
			g_value_set_float (value, p->zoom);
			break;
	}
}

static gboolean
ephy_zoom_control_set_tooltip (GtkToolItem *tool_item,
			       GtkTooltips *tooltips,
			       const char *tip_text,
			       const char *tip_private)
{
	EphyZoomControl *control = EPHY_ZOOM_CONTROL (tool_item);

	gtk_tooltips_set_tip (tooltips, GTK_WIDGET (control->priv->combo),
			      tip_text, tip_private);

	return TRUE;
}

static void
ephy_zoom_control_class_init (EphyZoomControlClass *klass)
{
	GObjectClass *object_class;
	GtkToolItemClass *tool_item_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = (GObjectClass *)klass;
	tool_item_class = (GtkToolItemClass *)klass;

	object_class->set_property = ephy_zoom_control_set_property;
	object_class->get_property = ephy_zoom_control_get_property;
	object_class->finalize = ephy_zoom_control_finalize;

	tool_item_class->create_menu_proxy = ephy_zoom_control_create_menu_proxy;
	tool_item_class->set_tooltip = ephy_zoom_control_set_tooltip;

	g_object_class_install_property (object_class,
					 PROP_ZOOM,
					 g_param_spec_float ("zoom",
							     "Zoom",
							     "Zoom level to display in the item.",
							     ZOOM_MINIMAL,
							     ZOOM_MAXIMAL,
							     1.0,
							     G_PARAM_READWRITE));

	signals[ZOOM_TO_LEVEL_SIGNAL] =
		g_signal_new ("zoom_to_level",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyZoomControlClass,
					       zoom_to_level),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_FLOAT);

	g_type_class_add_private (object_class, sizeof (EphyZoomControlPrivate));
}

static void
ephy_zoom_control_finalize (GObject *o)
{
	EphyZoomControl *control = EPHY_ZOOM_CONTROL (o);

	g_object_unref (control->priv->combo);

	G_OBJECT_CLASS (parent_class)->finalize (o);
}

void
ephy_zoom_control_set_zoom_level (EphyZoomControl *control, float zoom)
{
	g_return_if_fail (EPHY_IS_ZOOM_CONTROL (control));
	
	if (zoom < ZOOM_MINIMAL || zoom > ZOOM_MAXIMAL) return;

	control->priv->zoom = zoom;
	g_object_notify (G_OBJECT (control), "zoom");
}

float
ephy_zoom_control_get_zoom_level (EphyZoomControl *control)
{
	g_return_val_if_fail (EPHY_IS_ZOOM_CONTROL (control), 1.0);
	
	return control->priv->zoom;
}
