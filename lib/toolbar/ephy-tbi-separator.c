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
#include <gtk/gtkstock.h>

#include "ephy-tbi-separator.h"
#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-bonobo-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbiSeparatorPrivate
{
	GtkWidget *widget;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tbi_separator_class_init		(EphyTbiSeparatorClass *klass);
static void		ephy_tbi_separator_init			(EphyTbiSeparator *tb);
static void		ephy_tbi_separator_finalize_impl	(GObject *o);
static GtkWidget *	ephy_tbi_separator_get_widget_impl	(EphyTbItem *i);
static GdkPixbuf *	ephy_tbi_separator_get_icon_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_separator_get_name_human_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_separator_to_string_impl	(EphyTbItem *i);
static gboolean		ephy_tbi_separator_is_unique_impl	(EphyTbItem *i);
static EphyTbItem *	ephy_tbi_separator_clone_impl		(EphyTbItem *i);
static void		ephy_tbi_separator_parse_properties_impl(EphyTbItem *i, const gchar *props);
static void		ephy_tbi_separator_add_to_bonobo_tb_impl(EphyTbItem *i,
								 BonoboUIComponent *ui,
								 const char *container_path,
								 guint index);

static gpointer ephy_tb_item_class;

/**
 * TbiSeparator object
 */

MAKE_GET_TYPE (ephy_tbi_separator, "EphyTbiSeparator", EphyTbiSeparator, ephy_tbi_separator_class_init,
	       ephy_tbi_separator_init, EPHY_TYPE_TB_ITEM);

static void
ephy_tbi_separator_class_init (EphyTbiSeparatorClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tbi_separator_finalize_impl;

	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_tbi_separator_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_tbi_separator_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_tbi_separator_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_tbi_separator_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->is_unique = ephy_tbi_separator_is_unique_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_tbi_separator_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_tbi_separator_parse_properties_impl;
	EPHY_TB_ITEM_CLASS (klass)->add_to_bonobo_tb = ephy_tbi_separator_add_to_bonobo_tb_impl;

	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void
ephy_tbi_separator_init (EphyTbiSeparator *tb)
{
	EphyTbiSeparatorPrivate *p = g_new0 (EphyTbiSeparatorPrivate, 1);
	tb->priv = p;
}

EphyTbiSeparator *
ephy_tbi_separator_new (void)
{
	EphyTbiSeparator *ret = g_object_new (EPHY_TYPE_TBI_SEPARATOR, NULL);
	return ret;
}

static void
ephy_tbi_separator_finalize_impl (GObject *o)
{
	EphyTbiSeparator *it = EPHY_TBI_SEPARATOR (o);
	EphyTbiSeparatorPrivate *p = it->priv;

	if (p->widget)
	{
		g_object_unref (p->widget);
	}

	g_free (p);

	DEBUG_MSG (("EphyTbiSeparator finalized\n"));

	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static GtkWidget *
ephy_tbi_separator_get_widget_impl (EphyTbItem *i)
{
	return NULL;
}

static GdkPixbuf *
ephy_tbi_separator_get_icon_impl (EphyTbItem *i)
{
	return NULL;
}

static gchar *
ephy_tbi_separator_get_name_human_impl (EphyTbItem *i)
{
	return g_strdup (_("Separator"));
}

static gchar *
ephy_tbi_separator_to_string_impl (EphyTbItem *i)
{
	/* if it had any properties, the string should include them */
	return g_strdup_printf ("%s=separator", i->id);
}

static gboolean
ephy_tbi_separator_is_unique_impl (EphyTbItem *i)
{
	return FALSE;
}

static EphyTbItem *
ephy_tbi_separator_clone_impl (EphyTbItem *i)
{
	EphyTbItem *ret = EPHY_TB_ITEM (ephy_tbi_separator_new ());

	ephy_tb_item_set_id (ret, i->id);

	/* should copy properties too, if any */

	return ret;
}

static void
ephy_tbi_separator_add_to_bonobo_tb_impl (EphyTbItem *i, BonoboUIComponent *ui,
				          const char *container_path, guint index)
{
	static gint hack = 0;
	gchar *xml;

	xml = g_strdup_printf ("<separator name=\"sep%d\"/>", ++hack);
	bonobo_ui_component_set (ui, container_path, xml, NULL);
	g_free (xml);
}

static void
ephy_tbi_separator_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	/* we have no properties */
}

