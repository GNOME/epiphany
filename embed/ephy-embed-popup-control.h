/*
 *  Copyright (C) 2000, 2001, 2002 Ricardo Fernández Pascual
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

#ifndef EPHY_EMBED_POPUP_CONTROL_H
#define EPHY_EMBED_POPUP_CONTROL_H

#include "ephy-embed-popup.h"
#include <bonobo/bonobo-control.h>

G_BEGIN_DECLS

typedef struct EphyEmbedPopupControlClass EphyEmbedPopupControlClass;

#define EPHY_EMBED_POPUP_CONTROL_TYPE		(ephy_embed_popup_control_get_type ())
#define EPHY_EMBED_POPUP_CONTROL(obj)		(GTK_CHECK_CAST ((obj), EPHY_EMBED_POPUP_CONTROL_TYPE, \
						 EphyEmbedPopupControl))
#define EPHY_EMBED_POPUP_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), \
						 EPHY_EMBED_POPUP_CONTROL_TYPE,\
						 EphyEmbedPopupControlClass))
#define IS_EPHY_EMBED_POPUP_CONTROL(obj)	(GTK_CHECK_TYPE ((obj), EPHY_EMBED_POPUP_CONTROL_TYPE))
#define IS_EPHY_EMBED_POPUP_CONTROL_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), \
						 EPHY_EMBED_POPUP_CONTROL))
#define EPHY_EMBED_POPUP_CONTROL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
						 EPHY_EMBED_POPUP_CONTROL_TYPE, \
						 EphyEmbedPopupControlClass))

typedef struct EphyEmbedPopupControl EphyEmbedPopupControl;
typedef struct EphyEmbedPopupControlPrivate EphyEmbedPopupControlPrivate;

struct EphyEmbedPopupControl
{
	EphyEmbedPopup parent;
        EphyEmbedPopupControlPrivate *priv;
};

struct EphyEmbedPopupControlClass
{
        EphyEmbedPopupClass parent_class;
};

GType		       ephy_embed_popup_control_get_type	(void);
EphyEmbedPopupControl *ephy_embed_popup_control_new		(BonoboControl *control);

G_END_DECLS

#endif
