/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
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

#include "ephy-tab.h"
#include "ephy-type-builtins.h"
#include "ephy-embed-type-builtins.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-debug.h"
#include "ephy-string.h"
#include "ephy-notebook.h"
#include "ephy-file-helpers.h"
#include "ephy-zoom.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-permission-manager.h"
#include "ephy-link.h"

#include <glib/gi18n.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkselection.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkclipboard.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-monitor.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

#ifdef ENABLE_PYTHON
#include "ephy-python.h"
#endif

#define EPHY_TAB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TAB, EphyTabPrivate))

struct _EphyTabPrivate
{
	int width;
	int height;
	guint idle_resize_handler;
};

static void ephy_tab_class_init		(EphyTabClass *klass);
static void ephy_tab_init		(EphyTab *tab);
static void ephy_tab_dispose		(GObject *object);
static void ephy_tab_finalize		(GObject *object);

enum
{
	PROP_0,
};

static GObjectClass *parent_class;

/* Class functions */

GType
ephy_tab_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
        {
                const GTypeInfo our_info =
                {
                        sizeof (EphyTabClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_tab_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyTab),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_tab_init
                };
		const GInterfaceInfo link_info = 
		{
			NULL,
			NULL,
			NULL
		};


                type = g_type_register_static (GTK_TYPE_BIN,
					       "EphyTab",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
        }

        return type;
}

static void
ephy_tab_size_request (GtkWidget *widget,
		       GtkRequisition *requisition)
{
	GtkWidget *child;

	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

	child = GTK_BIN (widget)->child;
	
	if (child && GTK_WIDGET_VISIBLE (child))
	{
		GtkRequisition child_requisition;
		gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
	}
}

static void
ephy_tab_size_allocate (GtkWidget *widget,
			GtkAllocation *allocation)
{
	GtkWidget *child;
	GtkAllocation invalid = { -1, -1, 1, 1 };

	widget->allocation = *allocation;

	child = GTK_BIN (widget)->child;
	g_return_if_fail (child != NULL);

	/* only resize if we're mapped (bug #128191),
	 * or if this is the initial size-allocate (bug #156854).
	 */
	if (GTK_WIDGET_MAPPED (child) ||
	    memcmp (&child->allocation, &invalid, sizeof (GtkAllocation)) == 0)
	{
		gtk_widget_size_allocate (child, allocation);
	}
}

static void
ephy_tab_map (GtkWidget *widget)
{
	GtkWidget *child;

	g_return_if_fail (GTK_WIDGET_REALIZED (widget));

	child = GTK_BIN (widget)->child;
	g_return_if_fail (child != NULL);

	/* we do this since the window might have been resized while this
	 * tab wasn't mapped (i.e. was a non-active tab during the resize).
	 * See bug #156854.
	 */
	if (memcmp (&widget->allocation, &child->allocation,
	    sizeof (GtkAllocation)) != 0)
	{
		gtk_widget_size_allocate (child, &widget->allocation);
	}

	GTK_WIDGET_CLASS (parent_class)->map (widget);	
}

static void
ephy_tab_grab_focus (GtkWidget *widget)
{
	EphyTab *tab = EPHY_TAB (widget);

	gtk_widget_grab_focus (GTK_WIDGET (ephy_tab_get_embed (tab)));
}

static void
ephy_tab_class_init (EphyTabClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

        parent_class = g_type_class_peek_parent (class);

	object_class->dispose = ephy_tab_dispose;
	object_class->finalize = ephy_tab_finalize;

	widget_class->size_allocate = ephy_tab_size_allocate;
	widget_class->size_request = ephy_tab_size_request;
	widget_class->map = ephy_tab_map;
	widget_class->grab_focus = ephy_tab_grab_focus;

	g_type_class_add_private (object_class, sizeof (EphyTabPrivate));
}

static void
ephy_tab_dispose (GObject *object)
{
	EphyTab *tab = EPHY_TAB (object);
	EphyTabPrivate *priv = tab->priv;

	if (priv->idle_resize_handler != 0)
	{
		g_source_remove (priv->idle_resize_handler);
		priv->idle_resize_handler = 0;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ephy_tab_finalize (GObject *object)
{
	EphyTab *tab = EPHY_TAB (object);

	G_OBJECT_CLASS (parent_class)->finalize (object);

#ifdef ENABLE_PYTHON
	ephy_python_schedule_gc ();
#endif

	LOG ("EphyTab finalized %p", tab);
}

static gboolean
address_has_web_scheme (const char *address)
{
	gboolean has_web_scheme;

	if (address == NULL) return FALSE;

	has_web_scheme = (g_str_has_prefix (address, "http:") ||
			  g_str_has_prefix (address, "https:") ||
			  g_str_has_prefix (address, "ftp:") ||
			  g_str_has_prefix (address, "file:") ||
			  g_str_has_prefix (address, "data:") ||
			  g_str_has_prefix (address, "about:") ||
			  g_str_has_prefix (address, "gopher:"));

	return has_web_scheme;
}

static gboolean
let_me_resize_hack (EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	gtk_widget_set_size_request (GTK_WIDGET (tab), -1, -1);

	priv->idle_resize_handler = 0;	
	return FALSE;
}

/* Public functions */

/**
 * ephy_tab_new:
 *
 * Equivalent to g_object_new(), but returns an #EphyTab so you don't have to
 * cast it.
 *
 * Returns: a new #EphyTab
 **/
EphyTab *
ephy_tab_new (void)
{
	return EPHY_TAB (g_object_new (EPHY_TYPE_TAB, NULL));
}

/**
 * ephy_tab_get_embed:
 * @tab: an #EphyTab
 *
 * Returns @tab's #EphyEmbed.
 *
 * Return value: @tab's #EphyEmbed
 **/
EphyEmbed *
ephy_tab_get_embed (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return EPHY_EMBED (gtk_bin_get_child (GTK_BIN (tab)));
}

/**
 * ephy_tab_for_embed
 * @embed: an #EphyEmbed
 *
 * Returns the #EphyTab which holds @embed.
 *
 * Return value: the #EphyTab which holds @embed
 **/
EphyTab *
ephy_tab_for_embed (EphyEmbed *embed)
{
	GtkWidget *parent;

	g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

	parent = GTK_WIDGET (embed)->parent;
	g_return_val_if_fail (parent != NULL, NULL);

	return EPHY_TAB (parent);
}

/**
 * ephy_tab_get_size:
 * @tab: an #EphyTab
 * @width: return location for width, or %NULL
 * @height: return location for height, or %NULL
 *
 * Obtains the size of @tab. This is not guaranteed to be the actual number of
 * pixels occupied by the #EphyTab.
 **/
void
ephy_tab_get_size (EphyTab *tab, int *width, int *height)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	if (width != NULL)
	{
		*width = tab->priv->width;
	}
	if (height != NULL)
	{
		*height = tab->priv->height;
	}
}

/**
 * ephy_tab_set_size:
 * @tab: an #EphyTab
 * @width:
 * @height:
 *
 * Sets the size of @tab. This is not guaranteed to actually resize the tab.
 **/
void
ephy_tab_set_size (EphyTab *tab,
		   int width,
		   int height)
{
	EphyTabPrivate *priv = tab->priv;
	GtkWidget *widget = GTK_WIDGET (tab);
	GtkAllocation allocation;

	priv->width = width;
	priv->height = height;

	gtk_widget_set_size_request (widget, width, height);

	/* HACK: When the web site changes both width and height,
	 * we will first get a width change, then a height change,
	 * without actually resizing the window in between (since
	 * that happens only on idle).
	 * If we don't set the allocation, GtkMozEmbed doesn't tell
	 * mozilla the new width, so the height change sets the width
	 * back to the old value!
	 */
	allocation.x = widget->allocation.x;
	allocation.y = widget->allocation.y;
	allocation.width = width;
	allocation.height = height;
	gtk_widget_size_allocate (widget, &allocation);

	/* HACK: reset widget requisition after the container
	 * has been resized. It appears to be the only way
	 * to have the window sized according to embed
	 * size correctly.
	 */
	if (priv->idle_resize_handler == 0)
	{
		priv->idle_resize_handler =
			g_idle_add ((GSourceFunc) let_me_resize_hack, tab);
	}
}

/* Private callbacks for embed signals */

static void
ephy_tab_init (EphyTab *tab)
{
	EphyTabPrivate *priv;
	GObject *embed;

	LOG ("EphyTab initialising %p", tab);

	priv = tab->priv = EPHY_TAB_GET_PRIVATE (tab);

	tab->priv->width = -1;
	tab->priv->height = -1;

	embed = ephy_embed_factory_new_object (EPHY_TYPE_EMBED);
	g_assert (embed != NULL);

	gtk_container_add (GTK_CONTAINER (tab), GTK_WIDGET (embed));
	gtk_widget_show (GTK_WIDGET (embed));
}
