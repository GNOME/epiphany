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

#ifndef EPHY_EMBED_POPUP_BW_H
#define EPHY_EMBED_POPUP_BW_H

#include "ephy-embed-popup.h"
#include <bonobo/bonobo-window.h>

G_BEGIN_DECLS

typedef struct EphyEmbedPopupBWClass EphyEmbedPopupBWClass;

#define EPHY_EMBED_POPUP_BW_TYPE		(ephy_embed_popup_bw_get_type ())
#define EPHY_EMBED_POPUP_BW(obj)		(GTK_CHECK_CAST ((obj), EPHY_EMBED_POPUP_BW_TYPE, \
						 EphyEmbedPopupBW))
#define EPHY_EMBED_POPUP_BW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EPHY_EMBED_POPUP_BW_TYPE,\
						 EphyEmbedPopupBWClass))
#define IS_EPHY_EMBED_POPUP_BW(obj)		(GTK_CHECK_TYPE ((obj), EPHY_EMBED_POPUP_BW_TYPE))
#define IS_EPHY_EMBED_POPUP_BW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), EPHY_EMBED_POPUP_BW))
#define EPHY_EMBED_POPUP_BW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), \
						 EPHY_EMBED_POPUP_BW_TYPE, EphyEmbedPopupBWClass))

typedef struct EphyEmbedPopupBW EphyEmbedPopupBW;
typedef struct EphyEmbedPopupBWPrivate EphyEmbedPopupBWPrivate;

struct EphyEmbedPopupBW
{
	EphyEmbedPopup parent;
        EphyEmbedPopupBWPrivate *priv;
};

struct EphyEmbedPopupBWClass
{
        EphyEmbedPopupClass parent_class;
};

GType			ephy_embed_popup_bw_get_type	(void);
EphyEmbedPopupBW *	ephy_embed_popup_bw_new		(BonoboWindow *window);

G_END_DECLS

#endif
