/*
 *  Copyright (C) 2002 Jorn Baayen
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

#include "statusbar.h"
#include "ephy-stock-icons.h"
#include "ephy-bonobo-extensions.h"

#include <string.h>
#include <time.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktooltips.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-control.h>

static void statusbar_class_init (StatusbarClass *klass);
static void statusbar_init (Statusbar *t);
static void statusbar_finalize (GObject *object);
static void
statusbar_set_property (GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec);
static void
statusbar_get_property (GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec);
static void
statusbar_set_window (Statusbar *t, EphyWindow *window);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static GObjectClass *parent_class = NULL;

struct StatusbarPrivate
{
	EphyWindow *window;
	BonoboUIComponent *ui_component;
	GtkWidget *security_icon;
	GtkWidget *progress;
	GtkTooltips *tooltips;
	GtkWidget *security_evbox;
	gboolean visibility;
};

GType 
statusbar_get_type (void)
{
        static GType statusbar_type = 0;

        if (statusbar_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (StatusbarClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) statusbar_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (Statusbar),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) statusbar_init
                };

                statusbar_type = g_type_register_static (G_TYPE_OBJECT,
						         "Statusbar",
						         &our_info, 0);
        }

        return statusbar_type;

}

static void
statusbar_class_init (StatusbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = statusbar_finalize;
	object_class->set_property = statusbar_set_property;
        object_class->get_property = statusbar_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
statusbar_set_property (GObject *object,
	                guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
        Statusbar *s = STATUSBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        statusbar_set_window (s, g_value_get_object (value));
                        break;
        }
}

static void
statusbar_get_property (GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
        Statusbar *s = STATUSBAR (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, s->priv->window);
                        break;
        }
}

static void
create_statusbar_security_icon (Statusbar *s)
{
	GtkWidget *security_frame;
	BonoboControl *control;

	security_frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (security_frame),
                                   GTK_SHADOW_IN);

        s->priv->security_icon = gtk_image_new ();
        s->priv->security_evbox = gtk_event_box_new ();
        gtk_container_add (GTK_CONTAINER (security_frame),
                           GTK_WIDGET (s->priv->security_evbox));
        gtk_container_add (GTK_CONTAINER (s->priv->security_evbox),
                           GTK_WIDGET (s->priv->security_icon));
	/*
	g_signal_connect (G_OBJECT (security_eventbox),
                          "button_release_event",
                          GTK_SIGNAL_FUNC
                                (security_icon_button_release_cb), t);
				*/

	control = bonobo_control_new (security_frame);
        bonobo_ui_component_object_set (s->priv->ui_component,
                                        "/status/SecurityIconWrapper",
                                        BONOBO_OBJREF (control),
                                        NULL);
	bonobo_object_unref (control);

	statusbar_set_security_state (s, FALSE, NULL);

        gtk_widget_show_all (security_frame);
}

static void
create_statusbar_progress (Statusbar *s)
{
	BonoboControl *control;

	s->priv->progress = gtk_progress_bar_new ();

	control = bonobo_control_new (s->priv->progress);
        bonobo_ui_component_object_set (s->priv->ui_component,
                                        "/status/ProgressWrapper",
                                        BONOBO_OBJREF (control),
                                        NULL);

	gtk_widget_show_all (s->priv->progress);
}

static void
statusbar_set_window (Statusbar *s, EphyWindow *window)
{
	g_return_if_fail (s->priv->window == NULL);

        s->priv->window = window;
        s->priv->ui_component = BONOBO_UI_COMPONENT
                                (s->priv->window->ui_component);

	create_statusbar_progress (s);
	create_statusbar_security_icon (s);
}

static void
statusbar_init (Statusbar *t)
{
        t->priv = g_new0 (StatusbarPrivate, 1);
	t->priv->visibility = TRUE;

	t->priv->tooltips = gtk_tooltips_new ();
}

static void
statusbar_finalize (GObject *object)
{
	Statusbar *t;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_STATUSBAR (object));

	t = STATUSBAR (object);

        g_return_if_fail (t->priv != NULL);

	gtk_object_destroy (GTK_OBJECT (t->priv->tooltips));

        g_free (t->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

Statusbar *
statusbar_new (EphyWindow *window)
{
	Statusbar *t;

	t = STATUSBAR (g_object_new (STATUSBAR_TYPE,
				     "EphyWindow", window,
				     NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

void
statusbar_set_visibility (Statusbar *t,
			  gboolean visibility)
{
	if (visibility == t->priv->visibility) return;

	t->priv->visibility = visibility;

	ephy_bonobo_set_hidden (BONOBO_UI_COMPONENT(t->priv->ui_component),
                                "/status",
                                !visibility);
}

void
statusbar_set_security_state (Statusbar *t,
			      gboolean state,
			      const char *tooltip)
{
	const char *stock;

	stock = state ? EPHY_STOCK_SECURE : EPHY_STOCK_UNSECURE;

        gtk_image_set_from_stock (GTK_IMAGE (t->priv->security_icon), stock,
				  GTK_ICON_SIZE_MENU);

	gtk_tooltips_set_tip (t->priv->tooltips, t->priv->security_evbox,
			      tooltip, NULL);
}

void
statusbar_set_progress (Statusbar *t,
			int progress)
{
	if (progress == -1)
	{
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR(t->priv->progress));
	}
	else
	{
		float tmp;
		tmp = (float)(progress) / 100;
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(t->priv->progress),
					       tmp);
	}
}

void
statusbar_set_message (Statusbar *s,
		       const char *message)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT(s->priv->ui_component));
	g_return_if_fail (message != NULL);

	/* Bonobo doesnt like 0 length messages */
	if (g_utf8_strlen (message, -1) == 0)
	{
		message = " ";
	}

	if (bonobo_ui_component_get_container (s->priv->ui_component)) /* should not do this here... */
	{
		bonobo_ui_component_set_status (s->priv->ui_component,
						message,
						NULL);
	}
}

