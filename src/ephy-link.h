/*
 *  Copyright © 2004 Christian Persch
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

#ifndef EPHY_LINK_H
#define EPHY_LINK_H

#include <glib-object.h>
#include "ephy-tab.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LINK			(ephy_link_get_type ())
#define EPHY_LINK(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_LINK, EphyLink))
#define EPHY_LINK_IFACE(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_LINK, EphyLinkIface))
#define EPHY_IS_LINK(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_LINK))
#define EPHY_IS_LINK_IFACE(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_LINK))
#define EPHY_LINK_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_LINK, EphyLinkIface))

typedef struct _EphyLink	EphyLink;
typedef struct _EphyLinkIface	EphyLinkIface;

typedef enum
{
	EPHY_LINK_NEW_WINDOW	= 1 << 0,
	EPHY_LINK_NEW_TAB	= 1 << 1,
	EPHY_LINK_JUMP_TO	= 1 << 2
} EphyLinkFlags;

struct _EphyLinkIface
{
	GTypeInterface base_iface;

	/* Signals */
	EphyTab * (* open_link)	(EphyLink *link,
				 const char *address,
				 EphyTab *tab,
				 EphyLinkFlags flags);
};

GType	 ephy_link_flags_get_type	(void);

GType	 ephy_link_get_type		(void);

EphyTab	*ephy_link_open			(EphyLink *link,
					 const char *address,
					 EphyTab *tab,
					 EphyLinkFlags flags);

EphyLinkFlags ephy_link_flags_from_current_event (void);

G_END_DECLS

#endif /* EPHY_LINK_H */
