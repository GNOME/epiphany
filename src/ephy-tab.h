/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
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

#ifndef EPHY_TAB_H
#define EPHY_TAB_H

#include "ephy-embed.h"

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TAB		(ephy_tab_get_type ())
#define EPHY_TAB(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TAB, EphyTab))
#define EPHY_TAB_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TAB, EphyTabClass))
#define EPHY_IS_TAB(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TAB))
#define EPHY_IS_TAB_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TAB))
#define EPHY_TAB_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TAB, EphyTabClass))

typedef struct _EphyTabClass	EphyTabClass;
typedef struct _EphyTab		EphyTab;
typedef struct _EphyTabPrivate	EphyTabPrivate;

struct _EphyTab
{
	GtkBin parent;

	/*< private >*/
	EphyTabPrivate *priv;
};

struct _EphyTabClass
{
	GtkBinClass parent_class;
};

GType			ephy_tab_get_type		(void);

EphyTab	               *ephy_tab_new			(void);

EphyEmbed              *ephy_tab_get_embed		(EphyTab *tab);

EphyTab		       *ephy_tab_for_embed		(EphyEmbed *embed);

void			ephy_tab_get_size		(EphyTab *tab,
							 int *width,
							 int *height);

void			ephy_tab_set_size		(EphyTab *tab,
							 int width,
							 int height);

/* private */
guint		       _ephy_tab_get_id			(EphyTab *tab);

G_END_DECLS

#endif
