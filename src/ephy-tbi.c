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

#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-tbi.h"
#include "ephy-debug.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>

/**
 * Private data
 */
struct _EphyTbiPrivate
{
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tbi_class_init		(EphyTbiClass *klass);
static void		ephy_tbi_init			(EphyTbi *tb);
static void		ephy_tbi_finalize_impl		(GObject *o);
static GtkWidget *	ephy_tbi_get_widget_impl	(EphyTbItem *i);
static GdkPixbuf *	ephy_tbi_get_icon_impl		(EphyTbItem *i);
static gchar *		ephy_tbi_get_name_human_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_to_string_impl		(EphyTbItem *i);
static gboolean		ephy_tbi_is_unique_impl		(EphyTbItem *i);
static EphyTbItem *	ephy_tbi_clone_impl		(EphyTbItem *i);
static void		ephy_tbi_parse_properties_impl	(EphyTbItem *i, const gchar *props);
static void		ephy_tbi_add_to_bonobo_tb_impl	(EphyTbItem *i,
							 BonoboUIComponent *ui,
							 const char *container_path,
							 guint index);


static gpointer ephy_tb_item_class;

/**
 * EphyTbi object
 */

MAKE_GET_TYPE (ephy_tbi, "EphyTbi", EphyTbi, ephy_tbi_class_init,
	       ephy_tbi_init, EPHY_TYPE_TB_ITEM);

static void
ephy_tbi_class_init (EphyTbiClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tbi_finalize_impl;

	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_tbi_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_tbi_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_tbi_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_tbi_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->is_unique = ephy_tbi_is_unique_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_tbi_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_tbi_parse_properties_impl;
	EPHY_TB_ITEM_CLASS (klass)->add_to_bonobo_tb = ephy_tbi_add_to_bonobo_tb_impl;

	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void
ephy_tbi_init (EphyTbi *tbi)
{
	tbi->window = NULL;
}

static void
ephy_tbi_finalize_impl (GObject *o)
{
	EphyTbi *it = EPHY_TBI (o);

	if (it->window)
	{
		g_object_remove_weak_pointer (G_OBJECT (it->window),
					      (gpointer *) &it->window);
	}

	LOG ("EphyTbi finalized")

	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static GtkWidget *
ephy_tbi_get_widget_impl (EphyTbItem *i)
{
	/* this class is abstract */
	g_assert_not_reached ();

	return NULL;
}

static GdkPixbuf *
ephy_tbi_get_icon_impl (EphyTbItem *i)
{
	return NULL;
}

static gchar *
ephy_tbi_get_name_human_impl (EphyTbItem *i)
{
	/* this class is abstract */
	g_assert_not_reached ();

	return NULL;
}

static gchar *
ephy_tbi_to_string_impl (EphyTbItem *i)
{
	/* this class is abstract */
	g_assert_not_reached ();

	return NULL;
}

static gboolean
ephy_tbi_is_unique_impl (EphyTbItem *i)
{
	return TRUE;
}

static EphyTbItem *
ephy_tbi_clone_impl (EphyTbItem *i)
{
	/* you can't clone this directly because this class is abstract */
	g_assert_not_reached ();
	return NULL;
}

static void
ephy_tbi_add_to_bonobo_tb_impl (EphyTbItem *i, BonoboUIComponent *ui,
				const char *container_path, guint index)
{
	GtkWidget *w = ephy_tb_item_get_widget (i);
	gtk_widget_show (w);
	ephy_bonobo_add_numbered_widget (ui, w, index, container_path);
}

static void
ephy_tbi_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	/* we have no properties */
}

void
ephy_tbi_set_window (EphyTbi *it, EphyWindow *w)
{
	if (it->window)
	{
		g_object_remove_weak_pointer (G_OBJECT (it->window),
					      (gpointer *) &it->window);
	}

	it->window = w;

	if (it->window)
	{
		g_object_add_weak_pointer (G_OBJECT (it->window),
					   (gpointer *) &it->window);
	}
}

EphyWindow *
ephy_tbi_get_window (EphyTbi *tbi)
{
	return tbi->window;
}

