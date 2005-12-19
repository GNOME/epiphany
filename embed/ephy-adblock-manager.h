/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
 *  Copyright (C) 2005 Jean-François Rameau
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

#ifndef EPHY_ADBLOCK_MANAGER_H
#define EPHY_ADBLOCK_MANAGER_H

#include <glib-object.h>
#include "ephy-adblock.h"

G_BEGIN_DECLS

#define EPHY_TYPE_ADBLOCK_MANAGER ephy_adblock_manager_get_type()
#define EPHY_ADBLOCK_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_ADBLOCK_MANAGER, EphyAdBlockManager))
#define EPHY_ADBLOCK_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_ADBLOCK_MANAGER, EphyAdBlockManagerwClass))
#define EPHY_IS_ADBLOCK_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_ADBLOCK_MANAGER))
#define EPHY_IS_ADBLOCK_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_ADBLOCK_MANAGER))
#define EPHY_ADBLOCK_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_ADBLOCK_MANAGER, EphyAdBlockManagerwClass))

typedef struct _EphyAdBlockManager             EphyAdBlockManager;
typedef struct _EphyAdBlockManagerClass        EphyAdBlockManagerClass;
typedef struct _EphyAdBlockManagerPrivate      EphyAdBlockManagerPrivate;

struct _EphyAdBlockManager {
        GObject parent;

	/* < private > */
	EphyAdBlockManagerPrivate *priv;
};

struct _EphyAdBlockManagerClass {
        GObjectClass parent_class;
};

GType 			ephy_adblock_manager_get_type (void);

gboolean		ephy_adblock_manager_should_load (EphyAdBlockManager *self,
				    	 	    	  const char *url,
				    	 	    	  AdUriCheckType check_type);

void 			ephy_adblock_manager_set_blocker (EphyAdBlockManager *self,
							  EphyAdBlock *blocker);

G_END_DECLS

#endif
