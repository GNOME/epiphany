/* 
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2005 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include "ephy-fullscreen-popup.h"
#include "ephy-spinner.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define EPHY_FULLSCREEN_POPUP_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FULLSCREEN_POPUP, EphyFullscreenPopupPrivate))

struct _EphyFullscreenPopupPrivate
{
	EphyWindow *window;
	GtkWidget *frame;
	EphySpinner *spinner;
	GtkWidget *lock;
	GtkWidget *lock_ebox;
	GtkWidget *button;
	guint show_button : 1;
	guint spinning : 1;
	guint show_lock : 1;
};

enum
{
	PROP_0,
	PROP_WINDOW,
};

enum
{
	EXIT_CLICKED,
	LOCK_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyFullscreenPopup, ephy_fullscreen_popup, GTK_TYPE_WINDOW)

static void
exit_button_clicked_cb (GtkWidget *button,
			EphyFullscreenPopup *popup)
{
	g_signal_emit (popup, signals[EXIT_CLICKED], 0);
}

static gboolean
lock_button_press_cb (GtkWidget *ebox,
		      GdkEventButton *event,
		      EphyFullscreenPopup *popup)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 1)
	{
		g_signal_emit (popup, signals[LOCK_CLICKED], 0);

		return TRUE;
	}

	return FALSE;
}

static void
ephy_fullscreen_popup_update_visibility (EphyFullscreenPopup *popup)
{
	EphyFullscreenPopupPrivate *priv = popup->priv;
	gboolean show_frame;

	show_frame = priv->spinning || priv->show_lock;

	g_object_set (priv->button, "visible", priv->show_button,
				    "sensitive", priv->show_button, NULL);
	g_object_set (priv->frame, "visible", show_frame, NULL);
	g_object_set (priv->spinner, "visible", priv->spinning, NULL);
	g_object_set (priv->lock_ebox, "visible", priv->show_lock, NULL);
}

static void
ephy_fullscreen_popup_update_spinner (EphyFullscreenPopup *popup)
{
	EphyFullscreenPopupPrivate *priv = popup->priv;

	if (priv->spinning && gtk_widget_get_visible (GTK_WIDGET (popup)))
	{
		ephy_spinner_start (priv->spinner);
	}
	else
	{
		ephy_spinner_stop (priv->spinner);
	}
}

static void
ephy_fullscreen_popup_update_position (EphyFullscreenPopup *popup)
{
	GtkWidget *widget = GTK_WIDGET (popup);
	GtkRequisition requisition;
	GdkScreen *screen;
	GdkRectangle screen_rect;
	int popup_width;

	gtk_widget_size_request (widget, &requisition);
	popup_width = requisition.width;

	screen = gtk_widget_get_screen (widget);
	gdk_screen_get_monitor_geometry
		(screen,
		 gdk_screen_get_monitor_at_window (screen,
						   gtk_widget_get_window (widget)),
		 &screen_rect);

	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	{
		gtk_window_move (GTK_WINDOW (widget),
				 screen_rect.x, screen_rect.y);
	}
	else
	{
		gtk_window_move (GTK_WINDOW (widget),
				 screen_rect.x + screen_rect.width - popup_width,
				 screen_rect.y);
	}
}

static void
ephy_fullscreen_popup_set_window (EphyFullscreenPopup *popup,
				  EphyWindow *window)
{
	EphyFullscreenPopupPrivate *priv = popup->priv;
	GdkScreen *screen;

	priv->window = window;

	/* FIXME multihead: screen change? */
	screen = gtk_widget_get_screen (GTK_WIDGET (priv->window));
	g_signal_connect_swapped (screen, "size-changed",
				  G_CALLBACK (ephy_fullscreen_popup_update_position), popup);
}

/* public functions */

void
ephy_fullscreen_popup_set_show_leave (EphyFullscreenPopup *popup,
				      gboolean show_button)
{
	EphyFullscreenPopupPrivate *priv = popup->priv;

	priv->show_button = show_button;
	ephy_fullscreen_popup_update_visibility (popup);
}

void
ephy_fullscreen_popup_set_spinning (EphyFullscreenPopup *popup,
				    gboolean spinning)
{
	EphyFullscreenPopupPrivate *priv = popup->priv;

	priv->spinning = spinning;
	ephy_fullscreen_popup_update_visibility (popup);
	ephy_fullscreen_popup_update_spinner (popup);
}

void
ephy_fullscreen_popup_set_security_state (EphyFullscreenPopup *popup,
					  gboolean show_lock,
					  const char *stock,
					  const char *tooltip)
{
	EphyFullscreenPopupPrivate *priv = popup->priv;

	priv->show_lock = show_lock != FALSE;
	gtk_image_set_from_stock (GTK_IMAGE (priv->lock), stock, GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text (priv->lock, tooltip);

	ephy_fullscreen_popup_update_visibility (popup);
}

/* Class implementation */

static void
ephy_fullscreen_popup_init (EphyFullscreenPopup *popup)
{
	EphyFullscreenPopupPrivate *priv;

	priv = popup->priv = EPHY_FULLSCREEN_POPUP_GET_PRIVATE (popup);

	priv->show_button = TRUE;
}

static GObject *
ephy_fullscreen_popup_constructor (GType type,
				   guint n_construct_properties,
				   GObjectConstructParam *construct_params)

{
	GObject *object;
	EphyFullscreenPopup *popup;
	EphyFullscreenPopupPrivate *priv;
	GtkWindow *window;
	GtkWidget *hbox, *frame_hbox, *icon;

	object = G_OBJECT_CLASS (ephy_fullscreen_popup_parent_class)->constructor (type,
                                                                                   n_construct_properties,
                                                                                   construct_params);

	window = GTK_WINDOW (object);
	popup = EPHY_FULLSCREEN_POPUP (window);
	priv = popup->priv;

	gtk_window_set_resizable (window, FALSE);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (window), hbox);
	gtk_widget_show (hbox);

	/* frame */
	priv->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (hbox), priv->frame, FALSE, FALSE, 0);

	frame_hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (priv->frame), frame_hbox);
	gtk_widget_show (frame_hbox);

	/* add spinner */
	priv->spinner = EPHY_SPINNER (ephy_spinner_new ());
	ephy_spinner_set_size (EPHY_SPINNER (priv->spinner), GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (frame_hbox), GTK_WIDGET (priv->spinner), FALSE, FALSE, 0);

	/* lock */
	priv->lock = gtk_image_new ();
	gtk_widget_show (priv->lock);

	priv->lock_ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (priv->lock_ebox), FALSE);
	gtk_widget_add_events (priv->lock_ebox, GDK_BUTTON_PRESS_MASK);
	g_signal_connect (priv->lock_ebox, "button-press-event",
			  G_CALLBACK (lock_button_press_cb), popup);
	gtk_container_add (GTK_CONTAINER (priv->lock_ebox), priv->lock);
	gtk_box_pack_start (GTK_BOX (frame_hbox), priv->lock_ebox, FALSE, FALSE, 0);
	gtk_widget_show (priv->lock_ebox);

	/* exit button */
	priv->button = gtk_button_new_with_label (_("Leave Fullscreen"));
	icon = gtk_image_new_from_stock (GTK_STOCK_LEAVE_FULLSCREEN, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (priv->button), icon);
	/* don't show the image! see bug #307818 */
	g_signal_connect (priv->button, "clicked",
			  G_CALLBACK (exit_button_clicked_cb), popup);
	gtk_box_pack_start (GTK_BOX (hbox), priv->button, FALSE, FALSE, 0);
	gtk_widget_show (priv->button);

	ephy_fullscreen_popup_update_visibility (popup);

	return object;
}

static void
ephy_fullscreen_popup_finalize (GObject *object)
{
	EphyFullscreenPopup *popup = EPHY_FULLSCREEN_POPUP (object);
	EphyFullscreenPopupPrivate *priv = popup->priv;
	GdkScreen *screen;

	screen = gtk_widget_get_screen (GTK_WIDGET (priv->window));
	g_signal_handlers_disconnect_matched (screen, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, popup);

	g_signal_handlers_disconnect_matched (priv->window, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, popup);

	G_OBJECT_CLASS (ephy_fullscreen_popup_parent_class)->finalize (object);
}

static void
ephy_fullscreen_popup_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_fullscreen_popup_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	EphyFullscreenPopup *popup = EPHY_FULLSCREEN_POPUP (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			ephy_fullscreen_popup_set_window (popup, g_value_get_object (value));
			break;
	}
}

static void
ephy_fullscreen_popup_show (GtkWidget *widget)
{
	EphyFullscreenPopup *popup = EPHY_FULLSCREEN_POPUP (widget);

	GTK_WIDGET_CLASS (ephy_fullscreen_popup_parent_class)->show (widget);

	ephy_fullscreen_popup_update_spinner (popup);
}

static void
ephy_fullscreen_popup_hide (GtkWidget *widget)
{
	EphyFullscreenPopup *popup = EPHY_FULLSCREEN_POPUP (widget);

	GTK_WIDGET_CLASS (ephy_fullscreen_popup_parent_class)->hide (widget);

	ephy_fullscreen_popup_update_spinner (popup);
}

static void
ephy_fullscreen_popup_size_request (GtkWidget *widget,
				    GtkRequisition *requisition)
{
	EphyFullscreenPopup *popup = EPHY_FULLSCREEN_POPUP (widget);

	GTK_WIDGET_CLASS (ephy_fullscreen_popup_parent_class)->size_request (widget, requisition);

	if (GTK_WIDGET_REALIZED (widget))
	{
		ephy_fullscreen_popup_update_position (popup);
	}
}

static void
ephy_fullscreen_popup_realize (GtkWidget *widget)
{
	EphyFullscreenPopup *popup = EPHY_FULLSCREEN_POPUP (widget);

	GTK_WIDGET_CLASS (ephy_fullscreen_popup_parent_class)->realize (widget);

	ephy_fullscreen_popup_update_position (popup);
}

static void
ephy_fullscreen_popup_class_init (EphyFullscreenPopupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructor = ephy_fullscreen_popup_constructor;
	object_class->finalize = ephy_fullscreen_popup_finalize;
	object_class->get_property = ephy_fullscreen_popup_get_property;
	object_class->set_property = ephy_fullscreen_popup_set_property;

	widget_class->show = ephy_fullscreen_popup_show;
	widget_class->hide = ephy_fullscreen_popup_hide;
	widget_class->size_request = ephy_fullscreen_popup_size_request;
	widget_class->realize = ephy_fullscreen_popup_realize;

	signals[EXIT_CLICKED] =
		g_signal_new
			("exit-clicked",
			 EPHY_TYPE_FULLSCREEN_POPUP,
			 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (EphyFullscreenPopupClass, exit_clicked),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);
	signals[LOCK_CLICKED] =
		g_signal_new
			("lock-clicked",
			 EPHY_TYPE_FULLSCREEN_POPUP,
			 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET (EphyFullscreenPopupClass, lock_clicked),
			 NULL, NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyFullscreenPopupPrivate));
}

GtkWidget *
ephy_fullscreen_popup_new (EphyWindow *window)
{
	return g_object_new (EPHY_TYPE_FULLSCREEN_POPUP,
			     "type", GTK_WINDOW_POPUP,
			     "window", window,
			     NULL);
}
