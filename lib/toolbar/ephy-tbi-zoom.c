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

#include <libgnome/gnome-i18n.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <string.h>

#include "ephy-tbi-zoom.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-bonobo-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbiZoomPrivate
{
	GtkWidget *widget;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	guint notification;
};

enum
{
        TOOLBAR_ITEM_STYLE_PROP,
        TOOLBAR_ITEM_ORIENTATION_PROP,
	TOOLBAR_ITEM_WANT_LABEL_PROP
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tbi_zoom_class_init		(EphyTbiZoomClass *klass);
static void		ephy_tbi_zoom_init			(EphyTbiZoom *tb);
static void		ephy_tbi_zoom_finalize_impl		(GObject *o);
static GtkWidget *	ephy_tbi_zoom_get_widget_impl		(EphyTbItem *i);
static GdkPixbuf *	ephy_tbi_zoom_get_icon_impl		(EphyTbItem *i);
static gchar *		ephy_tbi_zoom_get_name_human_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_zoom_to_string_impl		(EphyTbItem *i);
static gboolean		ephy_tbi_zoom_is_unique_impl		(EphyTbItem *i);
static EphyTbItem *	ephy_tbi_zoom_clone_impl		(EphyTbItem *i);
static void		ephy_tbi_zoom_parse_properties_impl	(EphyTbItem *i, const gchar *props);
static void		ephy_tbi_zoom_add_to_bonobo_tb_impl	(EphyTbItem *i,
								 BonoboUIComponent *ui,
								 const char *container_path,
								 guint index);
static void		ephy_tbi_zoom_setup_label		(EphyTbiZoom *it);
static void		ephy_tbi_zoom_notification_cb		(GConfClient* client,
								 guint cnxn_id,
								 GConfEntry *entry,
								 gpointer user_data);


static gpointer ephy_tb_item_class;

/**
 * TbiZoom object
 */

MAKE_GET_TYPE (ephy_tbi_zoom, "EphyTbiZoom", EphyTbiZoom, ephy_tbi_zoom_class_init,
	       ephy_tbi_zoom_init, EPHY_TYPE_TB_ITEM);

static void
ephy_tbi_zoom_class_init (EphyTbiZoomClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tbi_zoom_finalize_impl;

	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_tbi_zoom_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_tbi_zoom_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_tbi_zoom_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_tbi_zoom_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->is_unique = ephy_tbi_zoom_is_unique_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_tbi_zoom_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_tbi_zoom_parse_properties_impl;
	EPHY_TB_ITEM_CLASS (klass)->add_to_bonobo_tb = ephy_tbi_zoom_add_to_bonobo_tb_impl;

	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void
ephy_tbi_zoom_init (EphyTbiZoom *tbi)
{
	EphyTbiZoomPrivate *p = g_new0 (EphyTbiZoomPrivate, 1);
	tbi->priv = p;

	p->notification = eel_gconf_notification_add (CONF_DESKTOP_TOOLBAR_STYLE,
						      ephy_tbi_zoom_notification_cb,
						      tbi);
}

EphyTbiZoom *
ephy_tbi_zoom_new (void)
{
	EphyTbiZoom *ret = g_object_new (EPHY_TYPE_TBI_ZOOM, NULL);
	return ret;
}

static void
ephy_tbi_zoom_finalize_impl (GObject *o)
{
	EphyTbiZoom *it = EPHY_TBI_ZOOM (o);
	EphyTbiZoomPrivate *p = it->priv;

	if (p->widget)
	{
		g_object_unref (p->widget);
	}

	if (p->label)
	{
		g_object_unref (p->label);
	}

	if (p->vbox)
	{
		g_object_unref (p->vbox);
	}

	if (p->hbox)
	{
		g_object_unref (p->hbox);
	}

	if (p->notification)
	{
		eel_gconf_notification_remove (p->notification);
	}

	g_free (p);

	DEBUG_MSG (("EphyTbiZoom finalized\n"));

	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static GtkWidget *
ephy_tbi_zoom_get_widget_impl (EphyTbItem *i)
{
	EphyTbiZoom *iz = EPHY_TBI_ZOOM (i);
	EphyTbiZoomPrivate *p = iz->priv;

	if (!p->widget)
	{
		p->widget = gtk_spin_button_new_with_range (1, 999, 10);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (p->widget), 100);
		g_object_ref (p->widget);
		gtk_object_sink (GTK_OBJECT (p->widget));
		p->label = gtk_label_new (_("Zoom"));
		g_object_ref (p->label);
		gtk_object_sink (GTK_OBJECT (p->label));
		p->vbox = gtk_vbox_new (FALSE, 0);
		g_object_ref (p->vbox);
		gtk_object_sink (GTK_OBJECT (p->vbox));
		p->hbox = gtk_hbox_new (FALSE, 0);
		g_object_ref (p->hbox);
		gtk_object_sink (GTK_OBJECT (p->hbox));

		gtk_box_pack_start_defaults (GTK_BOX (p->hbox), p->vbox);
		gtk_box_pack_start_defaults (GTK_BOX (p->vbox), p->widget);
		gtk_widget_show (p->vbox);
		gtk_widget_show (p->hbox);
	}

	return p->widget;
}

static GdkPixbuf *
ephy_tbi_zoom_get_icon_impl (EphyTbItem *i)
{
	static GdkPixbuf *pb = NULL;
	if (!pb)
	{
		/* what's the easier way? */
		GtkWidget *b = gtk_spin_button_new_with_range (0, 1, 0.5);
		pb = gtk_widget_render_icon (b,
					     GTK_STOCK_ZOOM_IN,
					     GTK_ICON_SIZE_SMALL_TOOLBAR,
					     NULL);
		gtk_widget_destroy (b);
	}
	return g_object_ref (pb);
}

static gchar *
ephy_tbi_zoom_get_name_human_impl (EphyTbItem *i)
{
	return g_strdup (_("Zoom"));
}

static gchar *
ephy_tbi_zoom_to_string_impl (EphyTbItem *i)
{
	/* if it had any properties, the string should include them */
	return g_strdup_printf ("%s=zoom", i->id);
}

static gboolean
ephy_tbi_zoom_is_unique_impl (EphyTbItem *i)
{
	return TRUE;
}

static EphyTbItem *
ephy_tbi_zoom_clone_impl (EphyTbItem *i)
{
	EphyTbItem *ret = EPHY_TB_ITEM (ephy_tbi_zoom_new ());

	ephy_tb_item_set_id (ret, i->id);

	/* should copy properties too, if any */
	/* the zoom value is not copied, not sure if it should... */

	return ret;
}

static void
ephy_tbi_zoom_add_to_bonobo_tb_impl (EphyTbItem *i, BonoboUIComponent *ui,
				     const char *container_path, guint index)
{
	GtkWidget *w = ephy_tb_item_get_widget (i);
	EphyTbiZoomPrivate *p = EPHY_TBI_ZOOM (i)->priv;
	gtk_widget_show (w);
	ephy_bonobo_add_numbered_widget (ui, p->hbox, index, container_path);
	ephy_tbi_zoom_setup_label (EPHY_TBI_ZOOM (i));
}

static void
ephy_tbi_zoom_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	/* we have no properties */
}

static void
ephy_tbi_zoom_setup_label (EphyTbiZoom *it)
{
	EphyTbiZoomPrivate *p = it->priv;
	gchar *style = eel_gconf_get_string (CONF_DESKTOP_TOOLBAR_STYLE);
	ephy_tb_item_get_widget (EPHY_TB_ITEM (it));

	g_object_ref (p->label);
	if (p->label->parent)
	{
		gtk_container_remove (GTK_CONTAINER (p->label->parent), p->label);
	}

	if (!strcmp (style, "both_horiz") || !strcmp (style, "text"))
	{
		gtk_widget_show (p->label);
		gtk_box_pack_start_defaults (GTK_BOX (p->hbox), p->label);
	} 
	else if (!strcmp (style, "both"))
	{
		gtk_widget_show (p->label);
		gtk_box_pack_start_defaults (GTK_BOX (p->vbox), p->label);
	} 
	else
	{
		gtk_widget_hide (p->label);
	}

	g_free (style);
	g_object_unref (p->label);
}

static void
ephy_tbi_zoom_notification_cb (GConfClient* client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer user_data)
{
	ephy_tbi_zoom_setup_label (user_data);
}

