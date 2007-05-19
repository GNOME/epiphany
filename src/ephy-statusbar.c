/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2007 Christian Persch
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
 *  $Id$
 */

#include "config.h"

#include "ephy-statusbar.h"
#include "ephy-stock-icons.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtklabel.h>	
#include <gtk/gtkvbox.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkvseparator.h>
#include <gtk/gtkversion.h>

static void ephy_statusbar_class_init	(EphyStatusbarClass *klass);
static void ephy_statusbar_init		(EphyStatusbar *t);
static void ephy_statusbar_finalize	(GObject *object);

#define EPHY_STATUSBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_STATUSBAR, EphyStatusbarPrivate))

struct _EphyStatusbarPrivate
{
	GtkWidget *hbox;
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

#if !GTK_CHECK_VERSION (2, 11, 0)
static void
ephy_statusbar_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
	GtkStatusbar *gstatusbar = GTK_STATUSBAR (widget);
	EphyStatusbar *statusbar = EPHY_STATUSBAR (widget);
	EphyStatusbarPrivate *priv = statusbar->priv;
	GtkWidget *label;

	/* HACK HACK HACK ! */
	label = gstatusbar->label;
	gstatusbar->label = priv->hbox;

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	gstatusbar->label = label;
}
#endif /* !GTK 2.11.0 */

static void
ephy_statusbar_class_init (EphyStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
#if !GTK_CHECK_VERSION (2, 11, 0)
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
#endif

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_statusbar_finalize;

#if !GTK_CHECK_VERSION (2, 11, 0)
	widget_class->size_allocate = ephy_statusbar_size_allocate;
#endif

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

	priv->caret_indicator = ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
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

	ephy_statusbar_add_widget (statusbar, priv->caret_indicator);
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
create_icon_frame (EphyStatusbar *statusbar,
		   const char *stock_id,
		   GCallback button_press_cb,
		   GtkWidget **_evbox,
		   GtkWidget **_icon)
{
	GtkWidget *evbox, *icon;

	evbox = gtk_event_box_new ();
	gtk_event_box_set_visible_window  (GTK_EVENT_BOX (evbox), FALSE);
	if (button_press_cb)
	{
		gtk_widget_add_events (evbox, GDK_BUTTON_PRESS_MASK);
		g_signal_connect (evbox, "button-press-event",
				  G_CALLBACK (padlock_button_press_cb), statusbar);
	}

	icon = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (evbox), icon);
	gtk_widget_show (icon);

	ephy_statusbar_add_widget (statusbar, evbox);

	*_evbox = evbox;
	*_icon = icon;
}

static void
create_statusbar_progress (EphyStatusbar *s)
{
	EphyStatusbarPrivate *priv = s->priv;
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (priv->hbox), vbox, FALSE, FALSE, 0);

	priv->progressbar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (vbox),
			    GTK_WIDGET (priv->progressbar),
		 	    TRUE, TRUE, 1);

        /* We need to set the vertical size request to a small value here,
         * because the progressbar's default size request is taller than the whole
         * statusbar. Packing it with expand&fill in the vbox above will nevertheless
         * make it use the greatest available height.
         */
	gtk_widget_set_size_request (priv->progressbar, -1, 10);

	gtk_widget_show_all (vbox);
}

static void
ephy_statusbar_init (EphyStatusbar *t)
{
	GtkStatusbar *gstatusbar = GTK_STATUSBAR (t);
	EphyStatusbarPrivate *priv;

	priv = t->priv = EPHY_STATUSBAR_GET_PRIVATE (t);

	gtk_statusbar_set_has_resize_grip (gstatusbar, TRUE);

	t->tooltips = gtk_tooltips_new ();
	g_object_ref_sink (t->tooltips);

	priv->hbox = gtk_hbox_new (FALSE, 4);

	priv->icon_container = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->icon_container,
			    FALSE, FALSE, 0);
	gtk_widget_show (priv->icon_container);

	/* Put the label in the hbox, and substitute the hbox into the frame */
	g_object_ref (gstatusbar->label);
	gtk_container_remove (GTK_CONTAINER (gstatusbar->frame), gstatusbar->label);
	gtk_box_pack_start (GTK_BOX (priv->hbox), gstatusbar->label, TRUE, TRUE, 0);
	g_object_unref (gstatusbar->label);
	gtk_container_add (GTK_CONTAINER (gstatusbar->frame), priv->hbox);
	gtk_widget_show (priv->hbox);

	/* Create security icon */
	create_icon_frame (t,
			   NULL,
			   G_CALLBACK (padlock_button_press_cb),
			   &priv->security_evbox,
			   &priv->security_icon);
	gtk_widget_show (priv->security_evbox);

	/* Create popup-blocked icon */
	create_icon_frame (t,
			   EPHY_STOCK_POPUPS,
			   NULL,
			   &priv->popups_manager_evbox,
			   &priv->popups_manager_icon);
	/* don't show priv->popups_manager_evbox yet */

	create_caret_indicator (t);
	create_statusbar_progress (t);
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
	EphyStatusbarPrivate *priv = statusbar->priv;

	if (hidden)
	{
		gtk_widget_hide (priv->popups_manager_evbox);
	}
	else
	{
		gtk_tooltips_set_tip (statusbar->tooltips,
				      priv->popups_manager_evbox,
				      tooltip, NULL);

		gtk_widget_show (priv->popups_manager_evbox);
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

static void
sync_visibility (GtkWidget *widget,
		 GParamSpec *pspec,
		 GtkWidget *separator)
{
	if (GTK_WIDGET_VISIBLE (widget))
	{
		gtk_widget_show (separator);
	}
	else
	{
		gtk_widget_hide (separator);
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
	EphyStatusbarPrivate *priv;
	GtkWidget *vsep;

	g_return_if_fail (EPHY_IS_STATUSBAR (statusbar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	priv = statusbar->priv;

	gtk_box_pack_start (GTK_BOX (priv->icon_container),
			    widget, FALSE, FALSE, 0);

	vsep = gtk_vseparator_new ();
	gtk_box_pack_start (GTK_BOX (priv->icon_container),
			    vsep, FALSE, FALSE, 0);
	sync_visibility (widget, NULL, vsep);
	g_object_set_data (G_OBJECT (widget), "EphyStatusbar::separator", vsep);
	g_signal_connect (widget, "notify::visible",
			  G_CALLBACK (sync_visibility), vsep);
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
	EphyStatusbarPrivate *priv;
	GtkWidget *vsep;

	g_return_if_fail (EPHY_IS_STATUSBAR (statusbar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	priv = statusbar->priv;

	vsep = g_object_steal_data (G_OBJECT (widget), "EphyStatusbar::separator");
	g_return_if_fail (vsep != NULL);

	g_signal_handlers_disconnect_by_func
		(widget, G_CALLBACK (sync_visibility), vsep);

	gtk_container_remove (GTK_CONTAINER (priv->icon_container), vsep);
	gtk_container_remove (GTK_CONTAINER (priv->icon_container), widget);
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

	return statusbar->priv->security_evbox;
}
