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

#ifndef EPHY_TOOLBAR_H
#define EPHY_TOOLBAR_H

#include <glib-object.h>

#include "ephy-toolbar-item.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyToolbar EphyToolbar;
typedef struct _EphyToolbarClass EphyToolbarClass;
typedef struct _EphyToolbarPrivate EphyToolbarPrivate;

/**
 * Toolbar object
 */

#define EPHY_TYPE_TOOLBAR		(ephy_toolbar_get_type())
#define EPHY_TOOLBAR(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TOOLBAR,\
					 EphyToolbar))
#define EPHY_TOOLBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TOOLBAR,\
					 EphyToolbarClass))
#define EPHY_IS_TOOLBAR(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TOOLBAR))
#define EPHY_IS_TOOLBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TOOLBAR))
#define EPHY_TOOLBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TOOLBAR,\
					 EphyToolbarClass))

struct _EphyToolbarClass
{
	GObjectClass parent_class;

	/* signals */
	void	(*changed)	(EphyToolbar *tb);

};

/* Remember: fields are public read-only */
struct _EphyToolbar
{
	GObject parent_object;

	EphyToolbarPrivate *priv;
};

GType		ephy_toolbar_get_type		(void);
EphyToolbar *	ephy_toolbar_new		(void);
gboolean	ephy_toolbar_parse		(EphyToolbar *tb, const gchar *cfg);
gchar *		ephy_toolbar_to_string		(EphyToolbar *tb);
gboolean	ephy_toolbar_listen_to_gconf	(EphyToolbar *tb, const gchar *gconf_key);
EphyTbItem *	ephy_toolbar_get_item_by_id	(EphyToolbar *tb, const gchar *id);
const GSList *	ephy_toolbar_get_item_list	(EphyToolbar *tb);
void		ephy_toolbar_add_item		(EphyToolbar *tb, EphyTbItem *it, gint index);
void		ephy_toolbar_remove_item	(EphyToolbar *tb, EphyTbItem *it);
void		ephy_toolbar_set_fixed_order	(EphyToolbar *tb, gboolean value);
void		ephy_toolbar_set_check_unique	(EphyToolbar *tb, gboolean value);
gboolean	ephy_toolbar_get_check_unique	(EphyToolbar *tb);

G_END_DECLS

#endif
