/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 David Bordoley
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-go-action.h"

#include <gtk/gtk.h>
#include <bonobo/bonobo-i18n.h>
#include "eggtoolitem.h"
#include "ephy-debug.h"

static void ephy_go_action_init       (EphyGoAction *action);
static void ephy_go_action_class_init (EphyGoActionClass *class);
static void ephy_go_action_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_go_action_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyGoActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_go_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyGoAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_go_action_init,
		};

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphyGoAction",
					       &type_info, 0);
	}
	return type;
}

static void
button_clicked (GtkWidget *widget, EggAction *action)
{
	g_signal_emit_by_name (action, "activate");
}

static GtkWidget *
create_tool_item (EggAction *action)
{
	GtkWidget *button;
	GtkWidget *item;

	item = GTK_WIDGET (egg_tool_item_new ());

	button = gtk_button_new_with_label (_("Go"));
	gtk_button_set_relief(GTK_BUTTON (button), GTK_RELIEF_NONE);
	
	g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (button_clicked), action);

	gtk_container_add (GTK_CONTAINER (item), button);
	gtk_widget_show (button);

	return item;
}

static void
ephy_go_action_class_init (EphyGoActionClass *class)
{
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ephy_go_action_finalize;

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->create_tool_item = create_tool_item;
}

static void
ephy_go_action_init (EphyGoAction *action)
{
}

static void
ephy_go_action_finalize (GObject *object)
{
	g_return_if_fail (EPHY_IS_GO_ACTION (object));

	LOG ("Go action finalized")

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
