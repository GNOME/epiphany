/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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
#include "config.h"
#endif

#include "ephy-tb-button.h"
#include "ephy-gobject-misc.h"
#include "ephy-gui.h"
#include "ephy-marshal.h"
#include "eel-gconf-extensions.h"

#include <libgnome/gnome-i18n.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtktoolbar.h>
#include <bonobo/bonobo-ui-toolbar.h>
#include <string.h>

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbButtonPrivate 
{
	gchar *label;
	GtkWidget *image;
	gboolean use_stock;
	gchar *tooltip_text;
	GtkMenu *menu;
	GtkWidget *arrow_widget;
	gboolean sensitive;

	GtkWidget *button;
	GtkBox *button_box;
	GtkLabel *label_wid;
	gboolean priority;

	gboolean in_bonobo_toobar;

	GtkReliefStyle button_relief;
	GtkOrientation orientation;
	GtkToolbarStyle	style_gtk;
	BonoboUIToolbarStyle style_bonobo;
	GtkIconSize icon_size;	// TODO
	gboolean show_tooltips;
	GtkTooltips *tooltips;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tb_button_class_init		(EphyTbButtonClass *klass);
static void		ephy_tb_button_init			(EphyTbButton *b);
static void		ephy_tb_button_finalize_impl		(GObject *o);
static void		ephy_tb_button_build			(EphyTbButton *b);
static void		ephy_tb_button_empty			(EphyTbButton *b);
static void		ephy_tb_button_parent_set_cb		(GtkWidget *widget, GtkObject *old_parent,
								 EphyTbButton *tb);
static void		ephy_tb_button_gtk_orientation_changed_cb (GtkToolbar *toolbar, GtkOrientation orientation,
								  EphyTbButton *b);
static void		ephy_tb_button_gtk_style_changed_cb	(GtkToolbar *toolbar, GtkToolbarStyle style,
								 EphyTbButton *b);
static void		ephy_tb_button_bonobo_set_orientation_cb	(BonoboUIToolbar *toolbar, GtkOrientation orientation,
								 EphyTbButton *b);
static void		ephy_tb_button_bonobo_set_style_cb	(BonoboUIToolbar *toolbar, EphyTbButton *b);
static gboolean		ephy_tb_button_arrow_key_press_event_cb	(GtkWidget *widget, GdkEventKey *event, 
								 EphyTbButton *b);
static gboolean		ephy_tb_button_arrow_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, 
								   EphyTbButton *b);
static gboolean		ephy_tb_button_button_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, 
								    EphyTbButton *b);
static void 		ephy_tb_button_button_popup_menu_cb	(GtkWidget *w, EphyTbButton *b);
static void 		ephy_tb_button_menu_deactivated_cb	(GtkMenuShell *ms, EphyTbButton *b);


static gpointer gtk_hbox_class;

enum EphyTbButtonSignalsEnum {
	EPHY_TB_BUTTON_MENU_ACTIVATED,
	EPHY_TB_BUTTON_LAST_SIGNAL
};
static gint EphyTbButtonSignals[EPHY_TB_BUTTON_LAST_SIGNAL];

/**
 * TbButton object
 */

MAKE_GET_TYPE (ephy_tb_button, "EphyTbButton", EphyTbButton, ephy_tb_button_class_init, 
	       ephy_tb_button_init, GTK_TYPE_HBOX);

static void
ephy_tb_button_class_init (EphyTbButtonClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tb_button_finalize_impl;
	gtk_hbox_class = g_type_class_peek_parent (klass);

	EphyTbButtonSignals[EPHY_TB_BUTTON_MENU_ACTIVATED] = g_signal_new (
		"menu-activated", G_OBJECT_CLASS_TYPE (klass),  
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyTbButtonClass, menu_activated), 
		NULL, NULL, 
		ephy_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void 
ephy_tb_button_init (EphyTbButton *tb)
{
	EphyTbButtonPrivate *p = g_new0 (EphyTbButtonPrivate, 1);
	tb->priv = p;
	p->label = g_strdup ("");
	p->tooltip_text = g_strdup ("");

	p->button_relief = GTK_RELIEF_NORMAL;
	p->orientation = GTK_ORIENTATION_HORIZONTAL;
	p->style_gtk = GTK_TOOLBAR_BOTH_HORIZ;
	p->style_bonobo = BONOBO_UI_TOOLBAR_STYLE_PRIORITY_TEXT;
	p->icon_size =  GTK_ICON_SIZE_LARGE_TOOLBAR;
	p->show_tooltips = TRUE;

	g_signal_connect (tb, "parent-set", 
			  G_CALLBACK (ephy_tb_button_parent_set_cb), tb);
}

EphyTbButton *
ephy_tb_button_new (void)
{
	EphyTbButton *ret = g_object_new (EPHY_TYPE_TB_BUTTON, NULL);
	return ret;
}

static void
ephy_tb_button_finalize_impl (GObject *o)
{
	EphyTbButton *it = EPHY_TB_BUTTON (o);
	EphyTbButtonPrivate *p = it->priv;

	ephy_tb_button_empty (it);

	if (p->image)
	{
		g_object_unref (p->image);
	}

	if (p->tooltips)
	{
		g_object_unref (p->tooltips);
	}

	if (p->menu)
	{
		g_object_unref (p->menu);
	}

	if (p->arrow_widget)
	{
		g_object_unref (p->arrow_widget);
	}

	g_free (p);
	
	DEBUG_MSG (("EphyTbButton finalized\n"));
	
	G_OBJECT_CLASS (gtk_hbox_class)->finalize (o);
}

void
ephy_tb_button_set_label (EphyTbButton *b, const gchar *text)
{
	EphyTbButtonPrivate *p = b->priv;
	g_free (p->label);
	p->label = g_strdup (text);
	DEBUG_MSG (("EphyTbButton label set to '%s'\n", p->label));
	if (!p->label_wid || p->use_stock)
	{
		ephy_tb_button_build (b);
	}
	else
	{
		gtk_label_set_text (p->label_wid, p->label);
	}
}

void
ephy_tb_button_set_tooltip_text (EphyTbButton *b, const gchar *text)
{
	EphyTbButtonPrivate *p = b->priv;
	g_free (p->tooltip_text);
	p->tooltip_text = g_strdup (text);

	if (!p->tooltips || !p->button)
	{
		ephy_tb_button_build (b);
	}
	else
	{
		gtk_tooltips_set_tip (p->tooltips, p->button,
				      p->tooltip_text, p->tooltip_text);
	}
}

/* this function comes directly from gtktoolbar.c */
static gchar * 
elide_underscores (const gchar *original)
{
	gchar *q, *result;
	const gchar *p;
	gboolean last_underscore;
	
	q = result = g_malloc (strlen (original) + 1);
	last_underscore = FALSE;
	
	for (p = original; *p; p++)
	{
		if (!last_underscore && *p == '_')
		{
			last_underscore = TRUE;
		}
		else
		{
			last_underscore = FALSE;
			*q++ = *p;
		}
	}
	
	*q = '\0';
	
	return result;
}

static void
ephy_tb_button_build (EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	GtkWidget *align;
	GtkWidget *image;
	GtkStockItem stock_item;
	gboolean really_use_stock = p->use_stock && gtk_stock_lookup (p->label, &stock_item);
	gboolean show_image = p->label[0] == '\0'
		|| (p->in_bonobo_toobar && p->style_bonobo != BONOBO_UI_TOOLBAR_STYLE_TEXT_ONLY)
		|| (!p->in_bonobo_toobar && p->style_gtk != GTK_TOOLBAR_TEXT);
	gboolean show_label = !show_image
		|| (p->priority && ((p->in_bonobo_toobar && p->style_bonobo == BONOBO_UI_TOOLBAR_STYLE_PRIORITY_TEXT)
				 || (!p->in_bonobo_toobar && p->style_gtk == GTK_TOOLBAR_BOTH_HORIZ)))
		|| (!p->in_bonobo_toobar 
		    && (p->style_gtk == GTK_TOOLBAR_BOTH 
			|| p->style_gtk == GTK_TOOLBAR_TEXT
			/* CHECK: what about GTK_TOOLBAR_BOTH_HORIZ? */ ))
		|| (p->in_bonobo_toobar
		    && (p->style_bonobo == BONOBO_UI_TOOLBAR_STYLE_ICONS_AND_TEXT
			|| p->style_bonobo == BONOBO_UI_TOOLBAR_STYLE_TEXT_ONLY));
	
	ephy_tb_button_empty (b);

	if (!p->button)
	{
		p->button = gtk_button_new ();
		g_object_ref (p->button);
		gtk_widget_show (p->button);
		gtk_box_pack_start_defaults (GTK_BOX (b), p->button);
	}

	gtk_button_set_relief (GTK_BUTTON (p->button), p->button_relief);
	gtk_widget_set_sensitive (p->button, p->sensitive);
	if (p->tooltips)
	{
		gtk_tooltips_set_tip (p->tooltips, p->button,
				      p->tooltip_text, p->tooltip_text);
	}
	g_signal_connect (p->button, "button_press_event",
			  G_CALLBACK (ephy_tb_button_button_button_press_event_cb), b);
	g_signal_connect (p->button, "popup_menu", 
			  G_CALLBACK (ephy_tb_button_button_popup_menu_cb), b);

	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (align);
	gtk_container_add (GTK_CONTAINER (p->button), align);

	if ((p->in_bonobo_toobar && p->style_bonobo == BONOBO_UI_TOOLBAR_STYLE_ICONS_AND_TEXT)
	    || (!p->in_bonobo_toobar && p->style_gtk == GTK_TOOLBAR_BOTH)
	    || p->orientation == GTK_ORIENTATION_VERTICAL)
	{
		p->button_box = GTK_BOX (gtk_vbox_new (FALSE, 2));
	}
	else
	{
		p->button_box = GTK_BOX (gtk_hbox_new (FALSE, 2));
	}
	g_object_ref (p->button_box);
	gtk_widget_show (GTK_WIDGET (p->button_box));
	gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (p->button_box));
	
	if (!p->image && really_use_stock && show_image)
	{
		image = gtk_image_new_from_stock (p->label, p->icon_size);
	}
	else
	{
		image = p->image;
	}

	if (image)
	{
		if (show_image)
		{
			gtk_box_pack_start_defaults (p->button_box, GTK_WIDGET (image));
			gtk_widget_show (image);
		}
	}
	else
	{
		show_label = TRUE;
	}

	if (show_label)
	{
		p->label_wid = GTK_LABEL (gtk_label_new (p->label));
		g_object_ref (p->label_wid);
		
		if (really_use_stock)
		{
			gchar *l = elide_underscores (stock_item.label);
			gtk_label_set_text (p->label_wid, l);
			g_free (l);
		}
		
		gtk_widget_show (GTK_WIDGET (p->label_wid));
		gtk_box_pack_end_defaults (p->button_box, GTK_WIDGET (p->label_wid));
	}

	DEBUG_MSG (("EphyTbButton built, label='%s'\n", p->label));
}

void
ephy_tb_button_set_priority (EphyTbButton *b, gboolean priority)
{
	EphyTbButtonPrivate *p = b->priv;
	if (p->priority != priority)
	{
		p->priority = priority;
		ephy_tb_button_build (b);
	}
}

void
ephy_tb_button_set_image (EphyTbButton *b, GtkWidget *image)
{
	EphyTbButtonPrivate *p = b->priv;
	if (p->image)
	{
		g_object_unref (p->image);
	}
	p->image = image ? g_object_ref (image) : NULL;
	ephy_tb_button_build (b);
}

static void
button_state_changed_cb (GtkWidget *widget, GtkStateType previous_state, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
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

void
ephy_tb_button_set_show_arrow (EphyTbButton *b, gboolean value)
{
	EphyTbButtonPrivate *p = b->priv;

	if (p->arrow_widget && !value)
	{
		if (p->arrow_widget->parent == GTK_WIDGET (b))
		{
			gtk_container_remove (GTK_CONTAINER (b), p->arrow_widget);
		}
		g_object_unref (p->arrow_widget);
		p->arrow_widget = NULL;
	}
	else if (!p->arrow_widget && value)
	{
		p->arrow_widget = gtk_toggle_button_new ();
		gtk_button_set_relief (GTK_BUTTON (p->arrow_widget), GTK_RELIEF_NONE);

		gtk_container_add (GTK_CONTAINER (p->arrow_widget),
				   gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT));

		g_object_ref (p->arrow_widget);
		gtk_object_sink (GTK_OBJECT (p->arrow_widget));

		g_signal_connect (p->arrow_widget, "key_press_event",
				  G_CALLBACK (ephy_tb_button_arrow_key_press_event_cb),
				  b);
		g_signal_connect  (p->arrow_widget, "button_press_event",
				   G_CALLBACK (ephy_tb_button_arrow_button_press_event_cb),
				   b);
		g_signal_connect  (p->arrow_widget, "state_changed",
				   G_CALLBACK (button_state_changed_cb),
				   b);
		g_signal_connect  (p->button, "state_changed",
				   G_CALLBACK (button_state_changed_cb),
				   b);

		gtk_widget_show_all (p->arrow_widget);
		gtk_box_pack_end_defaults (GTK_BOX (b), p->arrow_widget);
		gtk_widget_set_sensitive (p->arrow_widget, value);
	}
}

void
ephy_tb_button_set_enable_menu (EphyTbButton *b, gboolean value)
{
	EphyTbButtonPrivate *p = b->priv;
	if (value && !p->menu)
	{
		p->menu = GTK_MENU (gtk_menu_new ());
		g_signal_connect (p->menu, "deactivate",
				  G_CALLBACK (ephy_tb_button_menu_deactivated_cb), b);
	}
	else if (!value && p->menu)
	{
		g_object_unref (p->menu);
		p->menu = FALSE;
	}

}

GtkMenuShell *
ephy_tb_button_get_menu (EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	return p->menu ? GTK_MENU_SHELL (p->menu) : NULL;
}

GtkButton *
ephy_tb_button_get_button (EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	if (!p->button)
	{
		ephy_tb_button_build (b);
	}
	return GTK_BUTTON (p->button);
}

void
ephy_tb_button_set_use_stock (EphyTbButton *b, gboolean value)
{
	EphyTbButtonPrivate *p = b->priv;
	if (value != p->use_stock)
	{
		p->use_stock = value;
		ephy_tb_button_build (b);
	}
}

static void
ephy_tb_button_empty (EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;

	if (p->button)
	{
		if (GTK_BIN (p->button)->child)
		{
			gtk_container_remove (GTK_CONTAINER (p->button), GTK_BIN (p->button)->child);
		}
	}

	if (p->button_box)
	{
		g_object_unref (p->button_box);
		p->button_box = NULL;
	}

	if (p->label_wid)
	{
		g_object_unref (p->label_wid);
		p->label_wid = NULL;
	}
}

static void
ephy_tb_button_parent_set_cb (GtkWidget *widget, GtkObject *old_parent, EphyTbButton *tb)
{
	EphyTbButtonPrivate *p = tb->priv;
	GtkWidget *new_parent = widget->parent;

	DEBUG_MSG (("EphyTbButton parent changed (widget=%p, button=%p, old=%p, new=%p)\n", 
		    widget, tb, old_parent, new_parent));

	if (new_parent)
	{
		GtkToolbar *gtktb = NULL;
		BonoboUIToolbar *btb = NULL;
		while (new_parent && !gtktb && !btb)
		{
			DEBUG_MSG (("new_parent ia a %s\n", g_type_name_from_instance ((void *) new_parent)));

			if (GTK_IS_TOOLBAR (new_parent))
			{
				gtktb = GTK_TOOLBAR (new_parent);
			}
			else if (BONOBO_IS_UI_TOOLBAR (new_parent))
			{
				btb = BONOBO_UI_TOOLBAR (new_parent);
			}
			else
			{
				g_signal_connect (new_parent, "parent_set", 
						  G_CALLBACK (ephy_tb_button_parent_set_cb), tb);
			}
			new_parent = new_parent->parent;
		}

		if (gtktb)
		{
			DEBUG_MSG (("EphyTbButton getting style from a GtkToolbar (%p)\n", gtktb));
			p->in_bonobo_toobar = FALSE;

			gtk_widget_ensure_style (GTK_WIDGET (gtktb));
			gtk_widget_style_get (GTK_WIDGET (gtktb), "button_relief", &p->button_relief, NULL);

			p->orientation = gtk_toolbar_get_orientation (gtktb);
			p->style_gtk = gtk_toolbar_get_style (gtktb);
			p->icon_size = gtk_toolbar_get_icon_size (gtktb);
			p->show_tooltips = gtk_toolbar_get_tooltips (gtktb);

			if (p->tooltips)
			{
				g_object_unref (p->tooltips);
			}
			p->tooltips = gtk_tooltips_new ();
			if (p->show_tooltips)
			{
				gtk_tooltips_enable (p->tooltips);
			}
			else
			{
				gtk_tooltips_disable (p->tooltips);
			}
			g_object_ref (p->tooltips);
			gtk_object_sink (GTK_OBJECT (p->tooltips));

			g_signal_connect (gtktb, "orientation-changed", 
					  G_CALLBACK (ephy_tb_button_gtk_orientation_changed_cb), tb);
			g_signal_connect (gtktb, "style-changed", 
					  G_CALLBACK (ephy_tb_button_gtk_style_changed_cb), tb);

			ephy_tb_button_build (tb);
		}

		if (btb)
		{
			DEBUG_MSG (("EphyTbButton getting style from a BonoboUIToolbar (%p)\n", btb));
			p->in_bonobo_toobar = TRUE;

			p->button_relief = GTK_RELIEF_NONE;

			p->orientation = bonobo_ui_toolbar_get_orientation (btb);
			p->style_bonobo = bonobo_ui_toolbar_get_style (btb);
			//p->icon_size = ???;
			p->show_tooltips = TRUE;

			if (p->tooltips)
			{
				g_object_unref (p->tooltips);
			}
			p->tooltips = bonobo_ui_toolbar_get_tooltips (btb);
			g_object_ref (p->tooltips);

			g_signal_connect (btb, "set-orientation",
					  G_CALLBACK (ephy_tb_button_bonobo_set_orientation_cb), tb);
			g_signal_connect (btb, "set-style",
					  G_CALLBACK (ephy_tb_button_bonobo_set_style_cb), tb);

			ephy_tb_button_build (tb);
		}
	}
	else
	{
		while (old_parent)
		{
			g_signal_handlers_disconnect_matched (old_parent, G_SIGNAL_MATCH_DATA, 
							      0, 0, NULL, NULL, tb);
			if (GTK_IS_WIDGET (old_parent))
			{
				old_parent = GTK_WIDGET (old_parent)->parent 
					? GTK_OBJECT (GTK_WIDGET (old_parent)->parent)
					: NULL;
			}
			else
			{
				old_parent = NULL;
			}
		}
	}
}

static void 
ephy_tb_button_gtk_orientation_changed_cb (GtkToolbar *toolbar, GtkOrientation orientation, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	if (p->orientation != orientation)
	{
		p->orientation = orientation;
		ephy_tb_button_build (b);
	}
}

static void 
ephy_tb_button_gtk_style_changed_cb (GtkToolbar *toolbar, GtkToolbarStyle style, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	if (p->style_gtk != style)
	{
		p->style_gtk = style;
		ephy_tb_button_build (b);
	}
}

static void 
ephy_tb_button_bonobo_set_orientation_cb (BonoboUIToolbar *toolbar, GtkOrientation orientation, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	if (p->orientation != orientation)
	{
		p->orientation = orientation;
		ephy_tb_button_build (b);
	}
}

static void 
ephy_tb_button_bonobo_set_style_cb (BonoboUIToolbar *toolbar, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	BonoboUIToolbarStyle style = bonobo_ui_toolbar_get_style (toolbar);
	if (style != p->style_bonobo)
	{
		p->style_bonobo = style;
		ephy_tb_button_build (b);
	}
}

static void 
ephy_tb_button_popup_menu_under_arrow (EphyTbButton *b, GdkEventButton *event)
{
	EphyTbButtonPrivate *p = b->priv;

	if (p->menu)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->arrow_widget), TRUE);
		g_signal_emit (b, EphyTbButtonSignals[EPHY_TB_BUTTON_MENU_ACTIVATED], 0);
		gtk_menu_popup (p->menu, NULL, NULL, ephy_gui_menu_position_under_widget, p->arrow_widget, 
				event ? event->button : 0, 
				event ? event->time : gtk_get_current_event_time ());
	}
}

static void 
ephy_tb_button_menu_deactivated_cb (GtkMenuShell *ms, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;
	if (p->arrow_widget)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->arrow_widget), FALSE);
	}
}

static gboolean
ephy_tb_button_arrow_button_press_event_cb  (GtkWidget *widget, GdkEventButton *event, EphyTbButton *b)
{
	ephy_tb_button_popup_menu_under_arrow (b, event);
	return TRUE;
}

static gboolean
ephy_tb_button_arrow_key_press_event_cb (GtkWidget *widget, GdkEventKey *event, EphyTbButton *b)
{
	if (event->keyval == GDK_space
	    || event->keyval == GDK_KP_Space
	    || event->keyval == GDK_Return
	    || event->keyval == GDK_KP_Enter
	    || event->keyval == GDK_Menu)
	{
		ephy_tb_button_popup_menu_under_arrow (b, NULL);
	}

	return FALSE;
}

static gboolean
ephy_tb_button_button_button_press_event_cb (GtkWidget *widget, GdkEventButton *event, 
					    EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;

	if (event->button == 3 && p->menu)
	{
		g_signal_emit (b, EphyTbButtonSignals[EPHY_TB_BUTTON_MENU_ACTIVATED], 0);
		gtk_menu_popup (p->menu, NULL, NULL, NULL, b, 
				event ? event->button : 0, 
				event ? event->time : gtk_get_current_event_time ());
		return TRUE;
	}
	
	return FALSE;
}

static void 
ephy_tb_button_button_popup_menu_cb (GtkWidget *w, EphyTbButton *b)
{
	EphyTbButtonPrivate *p = b->priv;

	g_signal_emit (b, EphyTbButtonSignals[EPHY_TB_BUTTON_MENU_ACTIVATED], 0);
	gtk_menu_popup (p->menu, NULL, NULL, 
			ephy_gui_menu_position_under_widget, b, 0, gtk_get_current_event_time ());
}

void
ephy_tb_button_set_sensitivity (EphyTbButton *b, gboolean value)
{
	EphyTbButtonPrivate *p = b->priv;

	p->sensitive = value;

	if (!p->button)
	{
		ephy_tb_button_build (b);
	}
	else
	{
		gtk_widget_set_sensitive (p->button, value);
		if (p->arrow_widget)
		{
			gtk_widget_set_sensitive (p->arrow_widget, value);
		}
	}
}

