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
#include <gtk/gtkeventbox.h>

#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-tbi-favicon.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbiFaviconPrivate
{
	GtkWidget *widget;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tbi_favicon_class_init			(EphyTbiFaviconClass *klass);
static void		ephy_tbi_favicon_init			(EphyTbiFavicon *tb);
static void		ephy_tbi_favicon_finalize_impl		(GObject *o);
static GtkWidget *	ephy_tbi_favicon_get_widget_impl		(EphyTbItem *i);
static GdkPixbuf *	ephy_tbi_favicon_get_icon_impl		(EphyTbItem *i);
static gchar *		ephy_tbi_favicon_get_name_human_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_favicon_to_string_impl		(EphyTbItem *i);
static gboolean		ephy_tbi_favicon_is_unique_impl		(EphyTbItem *i);
static EphyTbItem *	ephy_tbi_favicon_clone_impl			(EphyTbItem *i);
static void		ephy_tbi_favicon_parse_properties_impl	(EphyTbItem *i, const gchar *props);
static void		ephy_tbi_favicon_add_to_bonobo_tb_impl	(EphyTbItem *i,
								 BonoboUIComponent *ui,
								 const char *container_path,
								 guint index);

static gpointer ephy_tb_item_class;

/**
 * TbiFavicon object
 */

MAKE_GET_TYPE (ephy_tbi_favicon, "EphyTbiFavicon", EphyTbiFavicon, ephy_tbi_favicon_class_init,
	       ephy_tbi_favicon_init, EPHY_TYPE_TB_ITEM);

static void
ephy_tbi_favicon_class_init (EphyTbiFaviconClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tbi_favicon_finalize_impl;

	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_tbi_favicon_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_tbi_favicon_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_tbi_favicon_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_tbi_favicon_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->is_unique = ephy_tbi_favicon_is_unique_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_tbi_favicon_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_tbi_favicon_parse_properties_impl;
	EPHY_TB_ITEM_CLASS (klass)->add_to_bonobo_tb = ephy_tbi_favicon_add_to_bonobo_tb_impl;

	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void
ephy_tbi_favicon_init (EphyTbiFavicon *tb)
{
	EphyTbiFaviconPrivate *p = g_new0 (EphyTbiFaviconPrivate, 1);
	tb->priv = p;
}

EphyTbiFavicon *
ephy_tbi_favicon_new (void)
{
	EphyTbiFavicon *ret = g_object_new (EPHY_TYPE_TBI_FAVICON, NULL);
	return ret;
}

static void
ephy_tbi_favicon_finalize_impl (GObject *o)
{
	EphyTbiFavicon *it = EPHY_TBI_FAVICON (o);
	EphyTbiFaviconPrivate *p = it->priv;

	if (p->widget)
	{
		g_object_unref (p->widget);
	}

	g_free (p);

	DEBUG_MSG (("EphyTbiFavicon finalized\n"));

	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static GtkWidget *
ephy_tbi_favicon_get_widget_impl (EphyTbItem *i)
{
	EphyTbiFavicon *iz = EPHY_TBI_FAVICON (i);
	EphyTbiFaviconPrivate *p = iz->priv;

	if (!p->widget)
	{
		/* here, we create only the event_box. */
		p->widget = gtk_event_box_new ();
		g_object_ref (p->widget);
		gtk_object_sink (GTK_OBJECT (p->widget));
	}

	return p->widget;
}

static GdkPixbuf *
ephy_tbi_favicon_get_icon_impl (EphyTbItem *i)
{
	/* need an icon for this */
	return NULL;
}

static gchar *
ephy_tbi_favicon_get_name_human_impl (EphyTbItem *i)
{
	return g_strdup (_("Drag Handle"));
}

static gchar *
ephy_tbi_favicon_to_string_impl (EphyTbItem *i)
{
	/* if it had any properties, the string should include them */
	return g_strdup_printf ("%s=favicon", i->id);
}

static gboolean
ephy_tbi_favicon_is_unique_impl (EphyTbItem *i)
{
	return TRUE;
}

static EphyTbItem *
ephy_tbi_favicon_clone_impl (EphyTbItem *i)
{
	EphyTbItem *ret = EPHY_TB_ITEM (ephy_tbi_favicon_new ());

	ephy_tb_item_set_id (ret, i->id);

	/* should copy properties too, if any */

	return ret;
}

static void
ephy_tbi_favicon_add_to_bonobo_tb_impl (EphyTbItem *i, BonoboUIComponent *ui,
				       const char *container_path, guint index)
{
	GtkWidget *w = ephy_tb_item_get_widget (i);
	gtk_widget_show (w);
	ephy_bonobo_add_numbered_control (ui, w, index, container_path);
}

static void
ephy_tbi_favicon_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	/* we have no properties */
}

