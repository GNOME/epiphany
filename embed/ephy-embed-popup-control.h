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

#include <bonobo/bonobo-control.h>

#include "ephy-embed.h"
#include "ephy-embed-event.h"

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_POPUP_CONTROL		(ephy_embed_popup_control_get_type ())
#define EPHY_EMBED_POPUP_CONTROL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_POPUP_CONTROL, EphyEmbedPopupControl))
#define EPHY_EMBED_POPUP_CONTROL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_POPUP_CONTROL, EphyEmbedPopupControlClass))
#define EPHY_IS_EMBED_POPUP_CONTROL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_POPUP_CONTROL))
#define EPHY_IS_EMBED_POPUP_CONTROL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_POPUP_CONTROL))
#define EPHY_EMBED_POPUP_CONTROL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EMBED_POPUP_CONTROL, EphyEmbedPopupControlClass))

typedef struct EphyEmbedPopupControlClass EphyEmbedPopupControlClass;
typedef struct EphyEmbedPopupControl EphyEmbedPopupControl;
typedef struct EphyEmbedPopupControlPrivate EphyEmbedPopupControlPrivate;

struct EphyEmbedPopupControl
{
	GObject parent;
        EphyEmbedPopupControlPrivate *priv;
};

struct EphyEmbedPopupControlClass
{
        GObjectClass parent_class;
};

GType		       ephy_embed_popup_control_get_type	(void);

EphyEmbedPopupControl *ephy_embed_popup_control_new		(BonoboControl *control);

EphyEmbedEvent        *ephy_embed_popup_control_get_event       (EphyEmbedPopupControl *p);

void		       ephy_embed_popup_control_set_event       (EphyEmbedPopupControl *p,
								 EphyEmbedEvent *event);

void		       ephy_embed_popup_control_connect_verbs   (EphyEmbedPopupControl *p,
				                                 BonoboUIComponent *ui_component);

void		       ephy_embed_popup_control_show            (EphyEmbedPopupControl *pp,
			                                         EphyEmbed *embed);

G_END_DECLS

#endif
