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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-statusbar.h"
#include "ephy-stock-icons.h"
#include "ephy-string.h"

#include <string.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktooltips.h>

static void ephy_statusbar_class_init	(EphyStatusbarClass *klass);
static void ephy_statusbar_init		(EphyStatusbar *t);
static void ephy_statusbar_finalize	(GObject *object);

static GObjectClass *parent_class = NULL;

#define EPHY_STATUSBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_STATUSBAR, EphyStatusbarPrivate))

struct EphyStatusbarPrivate
{
	GtkWidget *security_icon;
	GtkWidget *progressbar;
	GtkTooltips *tooltips;
	GtkWidget *security_evbox;
};

GType
ephy_statusbar_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
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

	g_type_class_add_private (object_class, sizeof (EphyStatusbarPrivate));
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
	gtk_container_add (GTK_CONTAINER (s->security_frame),
			   GTK_WIDGET (s->priv->security_evbox));
	gtk_container_add (GTK_CONTAINER (s->priv->security_evbox),
			   GTK_WIDGET (s->priv->security_icon));

	ephy_statusbar_set_security_state (s, FALSE, NULL);

	gtk_widget_show_all (s->security_frame);

	gtk_box_pack_end (GTK_BOX (s),
			  GTK_WIDGET (s->security_frame),
			  FALSE, TRUE, 0);
}

static void
create_statusbar_progress (EphyStatusbar *s)
{
	s->priv->progressbar = gtk_progress_bar_new ();
	gtk_widget_show_all (s->priv->progressbar);

	gtk_box_pack_start (GTK_BOX (s),
			    GTK_WIDGET (s->priv->progressbar),
			    FALSE, TRUE, 0);
}

static void
ephy_statusbar_init (EphyStatusbar *t)
{
	t->priv = EPHY_STATUSBAR_GET_PRIVATE (t);

	t->priv->tooltips = gtk_tooltips_new ();
	g_object_ref (G_OBJECT (t->priv->tooltips));
	gtk_object_sink (GTK_OBJECT (t->priv->tooltips));

	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (t), FALSE);

	create_statusbar_progress (t);
	create_statusbar_security_icon (t);
}

static void
ephy_statusbar_finalize (GObject *object)
{
	EphyStatusbar *t = EPHY_STATUSBAR (object);

	g_object_unref (t->priv->tooltips);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
ephy_statusbar_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_STATUSBAR, NULL));
}

void
ephy_statusbar_set_security_state (EphyStatusbar *t,
				   gboolean secure,
				   const char *tooltip)
{
	const char *stock;

	stock = secure ? EPHY_STOCK_SECURE : EPHY_STOCK_UNSECURE;

	gtk_image_set_from_stock (GTK_IMAGE (t->priv->security_icon), stock,
				  GTK_ICON_SIZE_MENU);

	gtk_tooltips_set_tip (t->priv->tooltips, t->priv->security_evbox,
			      tooltip, NULL);
}

void
ephy_statusbar_set_progress (EphyStatusbar *t,
			     int progress)
{
	if (progress == -1)
	{
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR(t->priv->progressbar));
	}
	else
	{
		float fraction;
		fraction = (float)(progress) / 100;
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(t->priv->progressbar),
					       fraction);
	}
}

void
ephy_statusbar_add_widget (EphyStatusbar *statusbar,
			   GtkWidget *widget)
{
	g_return_if_fail (EPHY_IS_STATUSBAR (statusbar));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_box_pack_start (GTK_BOX (statusbar), widget, FALSE, FALSE, 0);
}
