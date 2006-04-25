/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005 Christian Persch
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
 *
 *  $Id$
 */

#ifndef EPHY_TAB_H
#define EPHY_TAB_H

#include "ephy-embed.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
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

typedef enum
{
	EPHY_TAB_NAV_UP		= 1 << 0,
	EPHY_TAB_NAV_BACK	= 1 << 1,
	EPHY_TAB_NAV_FORWARD	= 1 << 2
} EphyTabNavigationFlags;

typedef enum
{
	EPHY_TAB_ADDRESS_EXPIRE_NOW,
	EPHY_TAB_ADDRESS_EXPIRE_NEXT,
	EPHY_TAB_ADDRESS_EXPIRE_CURRENT
} EphyTabAddressExpire;

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

EphyEmbedDocumentType	ephy_tab_get_document_type	(EphyTab *tab);

GdkPixbuf	       *ephy_tab_get_icon		(EphyTab *tab);

const char	       *ephy_tab_get_icon_address	(EphyTab *tab);

void			ephy_tab_set_icon_address	(EphyTab *tab,
							 const char *address);

gboolean		ephy_tab_get_load_status	(EphyTab *tab);

const char             *ephy_tab_get_link_message	(EphyTab *tab);


int			ephy_tab_get_load_percent	(EphyTab *tab);

const char             *ephy_tab_get_address		(EphyTab *tab);

const char	       *ephy_tab_get_typed_address	(EphyTab *tab);

void			ephy_tab_set_typed_address	(EphyTab *tab,
							 const char *address,
							 EphyTabAddressExpire expire);

EphyTabNavigationFlags	ephy_tab_get_navigation_flags	(EphyTab *tab);

EphyEmbedSecurityLevel	ephy_tab_get_security_level	(EphyTab *tab);

void			ephy_tab_get_size		(EphyTab *tab,
							 int *width,
							 int *height);

void			ephy_tab_set_size		(EphyTab *tab,
							 int width,
							 int height);

const char             *ephy_tab_get_status_message	(EphyTab *tab);

const char             *ephy_tab_get_title		(EphyTab *tab);

const char             *ephy_tab_get_title_composite	(EphyTab *tab);

gboolean		ephy_tab_get_visibility		(EphyTab *tab);

float			ephy_tab_get_zoom		(EphyTab *tab);

/* private */
guint		       _ephy_tab_get_id			(EphyTab *tab);

G_END_DECLS

#endif
