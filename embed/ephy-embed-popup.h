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

#ifndef EPHY_EMBED_POPUP_H
#define EPHY_EMBED_POPUP_H

#include "ephy-embed.h"
#include <glib-object.h>
#include <bonobo/bonobo-ui-component.h>

G_BEGIN_DECLS

typedef struct EphyEmbedPopupClass EphyEmbedPopupClass;

#define EPHY_EMBED_POPUP_TYPE             (ephy_embed_popup_get_type ())
#define EPHY_EMBED_POPUP(obj)             (GTK_CHECK_CAST ((obj), EPHY_EMBED_POPUP_TYPE, EphyEmbedPopup))
#define EPHY_EMBED_POPUP_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_EMBED_POPUP_TYPE, EphyEmbedPopupClass))
#define IS_EPHY_EMBED_POPUP(obj)          (GTK_CHECK_TYPE ((obj), EPHY_EMBED_POPUP_TYPE))
#define IS_EPHY_EMBED_POPUP_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_EMBED_POPUP))
#define EPHY_EMBED_POPUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_EMBED_POPUP_TYPE, EphyEmbedPopupClass))

typedef struct EphyEmbedPopup EphyEmbedPopup;
typedef struct EphyEmbedPopupPrivate EphyEmbedPopupPrivate;

struct EphyEmbedPopup
{
	GObject parent;
        EphyEmbedPopupPrivate *priv;
};

struct EphyEmbedPopupClass
{
        GObjectClass parent_class;

	void	(*show)					(EphyEmbedPopup *p,
							 EphyEmbed *embed);
};



/* this class is abstract, don't look for the constructor */

GType			ephy_embed_popup_get_type		(void);
void			ephy_embed_popup_connect_verbs		(EphyEmbedPopup *p,
								 BonoboUIComponent *ui_component);
void			ephy_embed_popup_set_embed		(EphyEmbedPopup *p,
								 EphyEmbed *e);
EphyEmbed *		ephy_embed_popup_get_embed		(EphyEmbedPopup *p);
void			ephy_embed_popup_set_event		(EphyEmbedPopup *p,
								 EphyEmbedEvent *event);
EphyEmbedEvent *	ephy_embed_popup_get_event		(EphyEmbedPopup *p);
void			ephy_embed_popup_show			(EphyEmbedPopup *p,
								 EphyEmbed *embed);
const char *		ephy_embed_popup_get_popup_path		(EphyEmbedPopup *p);

G_END_DECLS

#endif
