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

#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-toolbar-item.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbItemPrivate
{
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tb_item_class_init			(EphyTbItemClass *klass);
static void		ephy_tb_item_init			(EphyTbItem *tb);
static void		ephy_tb_item_finalize_impl		(GObject *o);

static gpointer g_object_class;

/**
 * TbItem object
 */

MAKE_GET_TYPE (ephy_tb_item, "EphyTbItem", EphyTbItem, ephy_tb_item_class_init,
	       ephy_tb_item_init, G_TYPE_OBJECT);

static void
ephy_tb_item_class_init (EphyTbItemClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tb_item_finalize_impl;

	g_object_class = g_type_class_peek_parent (klass);
}

static void
ephy_tb_item_init (EphyTbItem *it)
{
	EphyTbItemPrivate *p = g_new0 (EphyTbItemPrivate, 1);
	it->priv = p;
	it->id = g_strdup ("");
}

static void
ephy_tb_item_finalize_impl (GObject *o)
{
	EphyTbItem *it = EPHY_TB_ITEM (o);
	EphyTbItemPrivate *p = it->priv;

	g_free (it->id);
	g_free (p);

	DEBUG_MSG (("EphyTbItem finalized\n"));

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}

GtkWidget *
ephy_tb_item_get_widget (EphyTbItem *i)
{
	return EPHY_TB_ITEM_GET_CLASS (i)->get_widget (i);
}

GdkPixbuf *
ephy_tb_item_get_icon (EphyTbItem *i)
{
	return EPHY_TB_ITEM_GET_CLASS (i)->get_icon (i);
}

gchar *
ephy_tb_item_get_name_human (EphyTbItem *i)
{
	return EPHY_TB_ITEM_GET_CLASS (i)->get_name_human (i);
}

gchar *
ephy_tb_item_to_string (EphyTbItem *i)
{
	return EPHY_TB_ITEM_GET_CLASS (i)->to_string (i);
}

gboolean
ephy_tb_item_is_unique (EphyTbItem *i)
{
	return EPHY_TB_ITEM_GET_CLASS (i)->is_unique (i);
}

EphyTbItem *
ephy_tb_item_clone (EphyTbItem *i)
{
	return EPHY_TB_ITEM_GET_CLASS (i)->clone (i);
}

void
ephy_tb_item_add_to_bonobo_tb (EphyTbItem *i, BonoboUIComponent *ui,
			      const char *container_path, guint index)
{
	EPHY_TB_ITEM_GET_CLASS (i)->add_to_bonobo_tb (i, ui, container_path, index);
}

void
ephy_tb_item_set_id (EphyTbItem *i, const gchar *id)
{
	g_return_if_fail (EPHY_IS_TB_ITEM (i));

	g_free (i->id);
	i->id = g_strdup (id);
}

void
ephy_tb_item_parse_properties (EphyTbItem *i, const gchar *props)
{
	EPHY_TB_ITEM_GET_CLASS (i)->parse_properties (i, props);
}
