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

#include <string.h>
#include <time.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktooltips.h>

static void statusbar_class_init (StatusbarClass *klass);
static void statusbar_init (Statusbar *t);
static void statusbar_finalize (GObject *object);

static GObjectClass *parent_class = NULL;

struct StatusbarPrivate
{
	GtkWidget *security_icon;
	GtkWidget *progress;
	GtkTooltips *tooltips;
	GtkWidget *security_evbox;
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

                statusbar_type = g_type_register_static (GTK_TYPE_STATUSBAR,
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
}

static void
create_statusbar_security_icon (Statusbar *s)
{
	GtkWidget *security_frame;

	security_frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (security_frame),
                                   GTK_SHADOW_IN);

        s->priv->security_icon = gtk_image_new ();
        s->priv->security_evbox = gtk_event_box_new ();
        gtk_container_add (GTK_CONTAINER (security_frame),
                           GTK_WIDGET (s->priv->security_evbox));
        gtk_container_add (GTK_CONTAINER (s->priv->security_evbox),
                           GTK_WIDGET (s->priv->security_icon));

	statusbar_set_security_state (s, FALSE, NULL);

        gtk_widget_show_all (security_frame);

	gtk_box_pack_start (GTK_BOX (s),
			    GTK_WIDGET (security_frame),
			    FALSE, TRUE, 0);
}

static void
create_statusbar_progress (Statusbar *s)
{
	s->priv->progress = gtk_progress_bar_new ();
	gtk_widget_show_all (s->priv->progress);

	gtk_box_pack_start (GTK_BOX (s),
			    GTK_WIDGET (s->priv->progress),
			    FALSE, TRUE, 0);
}

static void
statusbar_init (Statusbar *t)
{
        t->priv = g_new0 (StatusbarPrivate, 1);

	t->priv->tooltips = gtk_tooltips_new ();

	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (t), FALSE);

	create_statusbar_progress (t);
	create_statusbar_security_icon (t);
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

GtkWidget *
statusbar_new (void)
{
	GtkWidget *t;

	t = GTK_WIDGET (g_object_new (STATUSBAR_TYPE,
				      NULL));

	return t;
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
	g_return_if_fail (message != NULL);

	gtk_statusbar_pop (GTK_STATUSBAR (s), 0);
	gtk_statusbar_push (GTK_STATUSBAR (s), 0, message);
}

