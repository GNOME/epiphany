/*
 *  Copyright (C) 2002 Christophe Fergeau
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

#include "ephy-arrow-toolbutton.h"
#include "ephy-marshal.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <gtk/gtkarrow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmain.h>

struct EphyArrowToolButtonPrivate
{
	GtkWidget *arrow_widget;
	GtkWidget *button;
	GtkMenu *menu;
};

enum EphyArrowToolButtonSignalsEnum {
	EPHY_ARROW_TOOL_BUTTON_MENU_ACTIVATED,
	EPHY_ARROW_TOOL_BUTTON_LAST_SIGNAL
};

/* GObject boilerplate code */
static void ephy_arrow_toolbutton_init         (EphyArrowToolButton *arrow_toolbutton);
static void ephy_arrow_toolbutton_class_init   (EphyArrowToolButtonClass *klass);
static void ephy_arrow_toolbutton_finalize     (GObject *object);

static GObjectClass *parent_class = NULL;

static gint EphyArrowToolButtonSignals[EPHY_ARROW_TOOL_BUTTON_LAST_SIGNAL];

GType
ephy_arrow_toolbutton_get_type (void)
{
        static GType ephy_arrow_toolbutton_type = 0;

        if (ephy_arrow_toolbutton_type == 0)
        {
                static const GTypeInfo our_info =
			{
				sizeof (EphyArrowToolButtonClass),
				NULL, /* base_init */
				NULL, /* base_finalize */
				(GClassInitFunc) ephy_arrow_toolbutton_class_init,
				NULL,
				NULL, /* class_data */
				sizeof (EphyArrowToolButton),
				0, /* n_preallocs */
				(GInstanceInitFunc) ephy_arrow_toolbutton_init
			};

                ephy_arrow_toolbutton_type = g_type_register_static (EGG_TYPE_TOOL_BUTTON,
							             "EphyArrowToolButton",
							             &our_info, 0);
        }

        return ephy_arrow_toolbutton_type;
}

static void
ephy_arrow_toolbutton_class_init (EphyArrowToolButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_arrow_toolbutton_finalize;

	EphyArrowToolButtonSignals[EPHY_ARROW_TOOL_BUTTON_MENU_ACTIVATED] =
		g_signal_new
		("menu-activated", G_OBJECT_CLASS_TYPE (klass),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                 G_STRUCT_OFFSET (EphyArrowToolButtonClass, menu_activated),
		 NULL, NULL,
		 ephy_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
}

static void
button_state_changed_cb (GtkWidget *widget,
			 GtkStateType previous_state,
			 EphyArrowToolButton *b)
{
	EphyArrowToolButtonPrivate *p = b->priv;
	GtkWidget *button;
	GtkStateType state = GTK_WIDGET_STATE (widget);
	GtkStateType other;

	if (state == GTK_STATE_ACTIVE ||
	    state == GTK_STATE_SELECTED ||
	    state == GTK_STATE_INSENSITIVE)
	{
		return;
	}

	button = (widget == p->arrow_widget) ? p->button : p->arrow_widget;
	other = GTK_WIDGET_STATE (button);

	if (state != other)
	{
		gtk_widget_set_state (button, state);
	}
}

static void
popup_menu_under_arrow (EphyArrowToolButton *b, GdkEventButton *event)
{
	EphyArrowToolButtonPrivate *p = b->priv;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->arrow_widget), TRUE);
	LOG ("Emit menu activated signal");
	g_signal_emit (b, EphyArrowToolButtonSignals[EPHY_ARROW_TOOL_BUTTON_MENU_ACTIVATED], 0);
	gtk_menu_popup (p->menu, NULL, NULL, ephy_gui_menu_position_under_widget, b,
			event ? event->button : 0,
			event ? event->time : gtk_get_current_event_time ());
}

static void
menu_deactivated_cb (GtkMenuShell *ms, EphyArrowToolButton *b)
{
	EphyArrowToolButtonPrivate *p = b->priv;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->arrow_widget), FALSE);
}

static gboolean
arrow_button_press_event_cb  (GtkWidget *widget, GdkEventButton *event, EphyArrowToolButton *b)
{
	popup_menu_under_arrow (b, event);
	return TRUE;
}

static gboolean
arrow_key_press_event_cb (GtkWidget *widget, GdkEventKey *event, EphyArrowToolButton *b)
{
	if (event->keyval == GDK_space
	    || event->keyval == GDK_KP_Space
	    || event->keyval == GDK_Return
	    || event->keyval == GDK_KP_Enter
	    || event->keyval == GDK_Menu)
	{
		popup_menu_under_arrow (b, NULL);
	}

	return FALSE;
}

static void
ephy_arrow_toolbutton_init (EphyArrowToolButton *arrowtb)
{
	GtkWidget *hbox;
	GtkWidget *arrow;
	GtkWidget *arrow_button;
	GtkWidget *real_button;

	arrowtb->priv = g_new (EphyArrowToolButtonPrivate, 1);

	egg_tool_item_set_homogeneous (EGG_TOOL_ITEM (arrowtb), FALSE);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	real_button = EGG_TOOL_BUTTON (arrowtb)->button;
	g_object_ref (real_button);
	gtk_container_remove (GTK_CONTAINER (arrowtb), real_button);
	gtk_container_add (GTK_CONTAINER (hbox), real_button);
	gtk_container_add (GTK_CONTAINER (arrowtb), hbox);

	arrow_button = gtk_toggle_button_new ();
	gtk_widget_show (arrow_button);
	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_button_set_relief (GTK_BUTTON (arrow_button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (arrow_button), arrow);

	gtk_box_pack_end (GTK_BOX (hbox), arrow_button,
			  FALSE, FALSE, 0);

	arrowtb->priv->button = real_button;
	arrowtb->priv->arrow_widget = arrow_button;

	arrowtb->priv->menu = GTK_MENU (gtk_menu_new ());
	g_signal_connect (arrowtb->priv->menu, "deactivate",
			  G_CALLBACK (menu_deactivated_cb), arrowtb);

	g_signal_connect (real_button, "state_changed",
			  G_CALLBACK (button_state_changed_cb),
			  arrowtb);
	g_signal_connect (arrow_button, "state_changed",
			  G_CALLBACK (button_state_changed_cb),
			  arrowtb);
	g_signal_connect (arrow_button, "key_press_event",
			  G_CALLBACK (arrow_key_press_event_cb),
			  arrowtb);
	g_signal_connect (arrow_button, "button_press_event",
			  G_CALLBACK (arrow_button_press_event_cb),
			  arrowtb);
}

static void
ephy_arrow_toolbutton_finalize (GObject *object)
{
	EphyArrowToolButton *arrow_toolbutton = EPHY_ARROW_TOOLBUTTON (object);

	gtk_widget_destroy (GTK_WIDGET (arrow_toolbutton->priv->menu));

	g_free (arrow_toolbutton->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkMenuShell *
ephy_arrow_toolbutton_get_menu (EphyArrowToolButton *b)
{
	return GTK_MENU_SHELL (b->priv->menu);
}
