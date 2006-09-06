/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#include "config.h"

#include "ephy-statusbar.h"
#include "ephy-stock-icons.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtklabel.h>	
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkwidget.h>

static void ephy_statusbar_class_init	(EphyStatusbarClass *klass);
static void ephy_statusbar_init		(EphyStatusbar *t);
static void ephy_statusbar_finalize	(GObject *object);

#define EPHY_STATUSBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_STATUSBAR, EphyStatusbarPrivate))

struct _EphyStatusbarPrivate
{
	GtkWidget *icon_container;

	GtkWidget *caret_indicator;
	GtkWidget *security_icon;
	GtkWidget *progressbar;
	GtkWidget *security_evbox;
	GtkWidget *popups_manager_icon;
	GtkWidget *popups_manager_evbox;
};

enum
{
	LOCK_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GObjectClass *parent_class;

GType
ephy_statusbar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyStatusbarClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_statusbar_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyStatusbar),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_statusbar_init
		};

		type = g_type_register_static (GTK_TYPE_STATUSBAR,
					       "EphyStatusbar",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_statusbar_class_init (EphyStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_statusbar_finalize;

	signals[LOCK_CLICKED] =
		g_signal_new
			("lock-clicked",
			 EPHY_TYPE_STATUSBAR,
			 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			 G_STRUCT_OFFSET (EphyStatusbarClass, lock_clicked),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);

	g_type_class_add_private (object_class, sizeof (EphyStatusbarPrivate));
}

static void
create_caret_indicator (EphyStatusbar *statusbar)
{
	EphyStatusbarPrivate *priv = statusbar->priv;
	GtkWidget *label, *ebox;

	priv->caret_indicator = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->caret_indicator),
				   GTK_SHADOW_IN);
	ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
	gtk_container_add (GTK_CONTAINER (priv->caret_indicator), ebox);
	gtk_widget_show (ebox);

	/* Translators: this is displayed in the statusbar; choose a short word
	 * or even an abbreviation.
	 */
	label = gtk_label_new (_("Caret"));
	gtk_container_add (GTK_CONTAINER (ebox), label);
	gtk_widget_show (label);

	gtk_tooltips_set_tip (statusbar->tooltips, ebox,
			      /* Translators: this is the tooltip on the "Caret" icon
			       * in the statusbar.
			       */
			      _("In keyboard selection mode, press F7 to exit"),
			      NULL);

	gtk_box_pack_start (GTK_BOX (priv->icon_container), priv->caret_indicator,
			    FALSE, FALSE, 0);
}

static gboolean
padlock_button_press_cb (GtkWidget *ebox,
                         GdkEventButton *event,
			 EphyStatusbar *statusbar)
{
        if (event->type == GDK_BUTTON_PRESS &&
	    event->button == 1 /* left */ &&
	    (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
        {
		g_signal_emit (statusbar, signals[LOCK_CLICKED], 0);

                return TRUE;
        }

        return FALSE;
}

static void
create_statusbar_security_icon (EphyStatusbar *s)
{
	s->security_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (s->security_frame),
				   GTK_SHADOW_IN);

	s->priv->security_icon = gtk_image_new ();
	s->priv->security_evbox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (s->priv->security_evbox),
					  FALSE);
	gtk_widget_add_events (s->priv->security_evbox, GDK_BUTTON_PRESS_MASK);
	g_signal_connect (s->priv->security_evbox, "button-press-event",
			  G_CALLBACK (padlock_button_press_cb), s);

	gtk_container_add (GTK_CONTAINER (s->security_frame),
			   GTK_WIDGET (s->priv->security_evbox));
	gtk_container_add (GTK_CONTAINER (s->priv->security_evbox),
			   GTK_WIDGET (s->priv->security_icon));

	ephy_statusbar_set_security_state (s, NULL, NULL);

	gtk_widget_show_all (s->security_frame);

	gtk_box_pack_start (GTK_BOX (s->priv->icon_container),
			    GTK_WIDGET (s->security_frame),
			    FALSE, TRUE, 0);
}

static void
create_statusbar_popups_manager_icon (EphyStatusbar *s)
{
	s->popups_manager_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (s->popups_manager_frame),
				   GTK_SHADOW_IN);

	s->priv->popups_manager_icon = gtk_image_new_from_stock
		(EPHY_STOCK_POPUPS, GTK_ICON_SIZE_MENU);

	s->priv->popups_manager_evbox = gtk_event_box_new ();

	gtk_event_box_set_visible_window
		(GTK_EVENT_BOX (s->priv->popups_manager_evbox), FALSE);

	gtk_container_add (GTK_CONTAINER (s->popups_manager_frame),
			   GTK_WIDGET (s->priv->popups_manager_evbox));
	gtk_container_add (GTK_CONTAINER (s->priv->popups_manager_evbox),
			   GTK_WIDGET (s->priv->popups_manager_icon));

	gtk_widget_show (s->priv->popups_manager_evbox);
	gtk_widget_show (s->priv->popups_manager_icon);

	/* note lack of gtk_widget_show (s->popups_manager_frame); */

	gtk_box_pack_start (GTK_BOX (s->priv->icon_container),
			    GTK_WIDGET (s->popups_manager_frame),
			    FALSE, TRUE, 0);
}

static void
create_statusbar_progress (EphyStatusbar *s)
{
	s->priv->progressbar = gtk_progress_bar_new ();
	gtk_widget_show_all (s->priv->progressbar);

	gtk_box_pack_end (GTK_BOX (s),
			  GTK_WIDGET (s->priv->progressbar),
			  FALSE, TRUE, 0);
}

static void
sync_shadow_type (EphyStatusbar *statusbar,
		  GtkStyle *previous_style,
		  gpointer dummy)
{
	GtkShadowType shadow;
	GList *children, *l;

	gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow-type",
			      &shadow, NULL);

	children = gtk_container_get_children
		(GTK_CONTAINER (statusbar->priv->icon_container));
	for (l = children; l != NULL; l = l->next)
	{
		if (GTK_IS_FRAME (l->data))
		{
			gtk_frame_set_shadow_type (GTK_FRAME (l->data), shadow);
		}
	}
	g_list_free (children);
}

static void
ephy_statusbar_init (EphyStatusbar *t)
{
	t->priv = EPHY_STATUSBAR_GET_PRIVATE (t);

	t->tooltips = gtk_tooltips_new ();
	g_object_ref_sink (t->tooltips);

	t->priv->icon_container = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (t), t->priv->icon_container, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (t), t->priv->icon_container, 0);
	gtk_widget_show (t->priv->icon_container);

	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (t), TRUE);

	create_statusbar_progress (t);
	create_statusbar_security_icon (t);
	create_statusbar_popups_manager_icon (t);
	create_caret_indicator (t);

	sync_shadow_type (t, NULL, NULL);
	g_signal_connect (t, "style-set", G_CALLBACK (sync_shadow_type), NULL);
}

static void
ephy_statusbar_finalize (GObject *object)
{
	EphyStatusbar *t = EPHY_STATUSBAR (object);

	g_object_unref (t->tooltips);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * ephy_statusbar_new:
 * 
 * Creates a new #EphyStatusbar.
 * 
 * Return value: the new #EphyStatusbar object
 **/
GtkWidget *
ephy_statusbar_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_STATUSBAR, NULL));
}

/**
 * ephy_statusbar_set_caret_mode:
 * @statusbar: an #EphyStatusbar
 * @enabled:
 * 
 * Sets the statusbar's caret browsing mode indicator.
 **/
void
ephy_statusbar_set_caret_mode (EphyStatusbar *statusbar,
			       gboolean enabled)
{
	EphyStatusbarPrivate *priv = statusbar->priv;

	enabled = enabled != FALSE;

	g_object_set (priv->caret_indicator, "visible", enabled, NULL);
}

/**
 * ephy_statusbar_set_security_state:
 * @statusbar: an #EphyStatusbar
 * @stock_id: stock-id of the icon showing the security state
 * @tooltip: a string detailing the security state
 * 
 * Sets the statusbar's security icon and its tooltip.
 **/
void
ephy_statusbar_set_security_state (EphyStatusbar *statusbar,
				   const char *stock_id,
				   const char *tooltip)
{
	gtk_image_set_from_stock (GTK_IMAGE (statusbar->priv->security_icon),
				  stock_id, GTK_ICON_SIZE_MENU);

	gtk_tooltips_set_tip (statusbar->tooltips, statusbar->priv->security_evbox,
			      tooltip, NULL);
}

/**
 * ephy_statusbar_set_popups_state:
 * @statusbar: an #EphyStatusbar
 * @hidden: %TRUE if popups have been hidden
 * @tooltip: a string to display as tooltip, or %NULL
 *
 * Sets the statusbar's popup-blocker icon's tooltip and visibility.
 **/
void
ephy_statusbar_set_popups_state (EphyStatusbar *statusbar,
				 gboolean hidden,
				 const char *tooltip)
{
	if (hidden)
	{
		gtk_widget_hide (statusbar->popups_manager_frame);
	}
	else
	{
		gtk_tooltips_set_tip (statusbar->tooltips,
				      statusbar->priv->popups_manager_evbox,
				      tooltip, NULL);

		gtk_widget_show (statusbar->popups_manager_frame);
	}
}

/**
 * ephy_statusbar_set_progress:
 * @statusbar: a #EphyStatusbar
 * @progress: the progress as an integer between 0 and 100 per cent.
 * 
 * Sets the statusbar's progress.
 **/
void
ephy_statusbar_set_progress (EphyStatusbar *statusbar,
			     int progress)
{
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (statusbar->priv->progressbar),
				       (float) (progress) / 100.0);

	if (progress < 100)
	{
		gtk_widget_show (statusbar->priv->progressbar);
	}
	else
	{
		gtk_widget_hide (statusbar->priv->progressbar);
	}
}

/**
 * ephy_statusbar_add_widget:
 * @statusbar: an #EphyStatusbar
 * @widget: a #GtkWidget
 * 
 * Adds the @widget to the statusbar. Use this function whenever you want to
 * add a widget to the statusbar. You can remove the widget again with
 * ephy_statusbar_remove_widget().
 **/
void
ephy_statusbar_add_widget (EphyStatusbar *statusbar,
			   GtkWidget *widget)
{
	g_return_if_fail (EPHY_IS_STATUSBAR (statusbar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_box_pack_start (GTK_BOX (statusbar->priv->icon_container),
			    widget, FALSE, FALSE, 0);

	if (GTK_IS_FRAME (widget))
	{
		GtkShadowType shadow;

		gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow-type",
				      &shadow, NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (widget), shadow);
	}
}

/**
 * ephy_statusbar_remove_widget:
 * @statusbar: an #EphyStatusbar
 * @widget: a #GtkWidget
 *
 * Removes @widget, which must have been added to @statusbar using
 * ephy_statusbar_add_widget ().
 */
void
ephy_statusbar_remove_widget (EphyStatusbar *statusbar,
			      GtkWidget *widget)
{
	g_return_if_fail (EPHY_IS_STATUSBAR (statusbar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_container_remove (GTK_CONTAINER (statusbar->priv->icon_container),
			      widget);
}


/**
 * ephy_statusbar_get_tooltips:
 * @statusbar: an #EphyStatusbar
 *
 * Return value: the statusbar's #GtkTooltips object
 */
GtkTooltips *
ephy_statusbar_get_tooltips (EphyStatusbar *statusbar)
{
	g_return_val_if_fail (EPHY_IS_STATUSBAR (statusbar), NULL);

	return statusbar->tooltips;
}

/**
 * ephy_statusbar_get_security_frame:
 * @statusbar: an #EphyStatusbar
 *
 * Return value: the statusbar's lock icon frame
 */
GtkWidget *
ephy_statusbar_get_security_frame (EphyStatusbar *statusbar)
{
	g_return_val_if_fail (EPHY_IS_STATUSBAR (statusbar), NULL);

	return statusbar->security_frame;
}
