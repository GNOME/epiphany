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

#ifndef EPHY_TOOLBAR_ITEM_H
#define EPHY_TOOLBAR_ITEM_H

#include <glib-object.h>

#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbItem EphyTbItem;
typedef struct _EphyTbItemClass EphyTbItemClass;
typedef struct _EphyTbItemPrivate EphyTbItemPrivate;

/**
 * TbItem object
 */

#define EPHY_TYPE_TB_ITEM		(ephy_tb_item_get_type())
#define EPHY_TB_ITEM(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TB_ITEM,\
					 EphyTbItem))
#define EPHY_TB_ITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TB_ITEM,\
					 EphyTbItemClass))
#define EPHY_IS_TB_ITEM(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TB_ITEM))
#define EPHY_IS_TB_ITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TB_ITEM))
#define EPHY_TB_ITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TB_ITEM,\
					 EphyTbItemClass))

struct _EphyTbItemClass
{
	GObjectClass parent_class;

	/* virtual */
	GtkWidget *	(*get_widget)		(EphyTbItem *it);
	GdkPixbuf *	(*get_icon)		(EphyTbItem *it);
	gchar *		(*get_name_human)	(EphyTbItem *it);
	gchar *		(*to_string)		(EphyTbItem *it);
	gboolean	(*is_unique)		(EphyTbItem *it);
	void		(*add_to_bonobo_tb)	(EphyTbItem *it, BonoboUIComponent *ui,
						 const char *container_path, guint index);
	EphyTbItem *	(*clone)		(EphyTbItem *it);
	void		(*parse_properties)	(EphyTbItem *it, const gchar *props);
};

/* Remember: fields are public read-only */
struct _EphyTbItem
{
	GObject parent_object;

	gchar *id;

	EphyTbItemPrivate *priv;
};

/* this class is abstract */

GType		ephy_tb_item_get_type		(void);
GtkWidget *	ephy_tb_item_get_widget		(EphyTbItem *i);
GdkPixbuf *	ephy_tb_item_get_icon		(EphyTbItem *i);
gchar *		ephy_tb_item_get_name_human	(EphyTbItem *i);
gchar *		ephy_tb_item_to_string		(EphyTbItem *i);
gboolean	ephy_tb_item_is_unique		(EphyTbItem *i);
void		ephy_tb_item_add_to_bonobo_tb	(EphyTbItem *i, BonoboUIComponent *ui,
						 const char *container_path, guint index);
EphyTbItem *	ephy_tb_item_clone		(EphyTbItem *i);
void		ephy_tb_item_set_id		(EphyTbItem *i, const gchar *id);
void		ephy_tb_item_parse_properties	(EphyTbItem *i, const gchar *props);

G_END_DECLS

#endif
