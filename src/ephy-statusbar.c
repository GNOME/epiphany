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
 */

#include "config.h"

#include "ephy-statusbar.h"
#include "ephy-stock-icons.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>	

/**
 * SECTION:ephy-statusbar
 * @short_description: A statusbar widget for Epiphany
 *
 * #EphyStatusbar is Epiphany's default statusbar for all windows.
 */

static void ephy_statusbar_class_init	(EphyStatusbarClass *klass);
static void ephy_statusbar_init		(EphyStatusbar *t);

#define EPHY_STATUSBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_STATUSBAR, EphyStatusbarPrivate))

struct _EphyStatusbarPrivate
{
	GtkWidget *hbox;
	GtkWidget *icon_container;
};

G_DEFINE_TYPE (EphyStatusbar, ephy_statusbar, GTK_TYPE_STATUSBAR)

static void
ephy_statusbar_class_init (EphyStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (EphyStatusbarPrivate));
}

static void
ephy_statusbar_init (EphyStatusbar *t)
{
	GtkStatusbar *gstatusbar = GTK_STATUSBAR (t);
	EphyStatusbarPrivate *priv;

	priv = t->priv = EPHY_STATUSBAR_GET_PRIVATE (t);

	gtk_statusbar_set_has_resize_grip (gstatusbar, TRUE);

#if GTK_CHECK_VERSION (2, 19, 1)
	priv->hbox = gtk_statusbar_get_message_area (gstatusbar);
#else
	priv->hbox = gtk_hbox_new (FALSE, 4);
#endif
	priv->icon_container = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->icon_container,
			    FALSE, FALSE, 0);
	gtk_widget_show (priv->icon_container);

#if GTK_CHECK_VERSION (2, 19, 1)
	gtk_box_reorder_child (GTK_BOX (priv->hbox), priv->icon_container, 0);
#else
	/* Put the label in the hbox, and substitute the hbox into the frame */
	g_object_ref (gstatusbar->label);
	gtk_container_remove (GTK_CONTAINER (gstatusbar->frame), gstatusbar->label);
	gtk_box_pack_start (GTK_BOX (priv->hbox), gstatusbar->label, TRUE, TRUE, 0);
	g_object_unref (gstatusbar->label);
	gtk_container_add (GTK_CONTAINER (gstatusbar->frame), priv->hbox);
	gtk_widget_show (priv->hbox);
#endif
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

static void
sync_visibility (GtkWidget *widget,
		 GParamSpec *pspec,
		 GtkWidget *separator)
{
	if (gtk_widget_get_visible (widget))
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
