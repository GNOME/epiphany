/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#ifndef EPHY_TAB_H
#define EPHY_TAB_H

#include "ephy-embed.h"

#include <glib-object.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TAB		(ephy_tab_get_type ())
#define EPHY_TAB(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TAB, EphyTab))
#define EPHY_TAB_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TAB, EphyTabClass))
#define EPHY_IS_TAB(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TAB))
#define EPHY_IS_TAB_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TAB))
#define EPHY_TAB_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TAB, EphyTabClass))

typedef struct EphyTabClass EphyTabClass;
typedef struct EphyTab EphyTab;
typedef struct EphyTabPrivate EphyTabPrivate;

typedef enum
{
	TAB_NAV_UP	= 1 << 0,
	TAB_NAV_BACK	= 1 << 1,
	TAB_NAV_FORWARD	= 1 << 2,
} TabNavigationFlags;

typedef enum
{
	TAB_ADDRESS_EXPIRE_NOW,
	TAB_ADDRESS_EXPIRE_NEXT,
	TAB_ADDRESS_EXPIRE_CURRENT
} TabAddressExpire;

struct EphyTab
{
        GObject parent;
        EphyTabPrivate *priv;
};

struct EphyTabClass
{
	GObjectClass parent_class;
};

/* Include the header down here to resolve circular dependency */
#include "ephy-window.h"

GType			ephy_tab_get_type		(void);

EphyTab	               *ephy_tab_new			(void);

GObject                *ephy_tab_get_action		(EphyTab *tab);

EphyEmbed              *ephy_tab_get_embed		(EphyTab *tab);

void			ephy_tab_set_window		(EphyTab *tab,
							 EphyWindow *window);

EphyWindow             *ephy_tab_get_window		(EphyTab *tab);

const char             *ephy_tab_get_icon_address	(EphyTab *tab);

gboolean		ephy_tab_get_load_status	(EphyTab *tab);

const char             *ephy_tab_get_link_message	(EphyTab *tab);


int			ephy_tab_get_load_percent	(EphyTab *tab);

void			ephy_tab_set_location		(EphyTab *tab,
							 const char *location,
							 TabAddressExpire expire);

const char             *ephy_tab_get_location		(EphyTab *tab);

TabNavigationFlags	ephy_tab_get_navigation_flags	(EphyTab *tab);

EmbedSecurityLevel	ephy_tab_get_security_level	(EphyTab *tab);

void			ephy_tab_get_size		(EphyTab *tab,
							 int *width,
							 int *height);

const char             *ephy_tab_get_status_message	(EphyTab *tab);

const char             *ephy_tab_get_title		(EphyTab *tab);

void			ephy_tab_set_visibility		(EphyTab *tab,
							 gboolean visible);

gboolean		ephy_tab_get_visibility		(EphyTab *tab);

float			ephy_tab_get_zoom		(EphyTab *tab);

G_END_DECLS

#endif
