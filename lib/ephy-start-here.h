/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#ifndef EPHY_START_HERE_H
#define EPHY_START_HERE_H

#include <glib-object.h>

#include "ephy-node.h"

G_BEGIN_DECLS

typedef struct EphyStartHereClass EphyStartHereClass;

#define EPHY_START_HERE_TYPE             (ephy_start_here_get_type ())
#define EPHY_START_HERE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_START_HERE_TYPE, EphyStartHere))
#define EPHY_START_HERE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_START_HERE_TYPE, EphyStartHereClass))
#define IS_EPHY_START_HERE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_START_HERE_TYPE))
#define IS_EPHY_START_HERE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_START_HERE_TYPE))

typedef struct EphyStartHere EphyStartHere;
typedef struct EphyStartHerePrivate EphyStartHerePrivate;

struct EphyStartHere
{
        GObject parent;
        EphyStartHerePrivate *priv;
};

struct EphyStartHereClass
{
        GObjectClass parent_class;
};

GType		ephy_start_here_get_type	(void);

EphyStartHere  *ephy_start_here_new		(void);

char	       *ephy_start_here_get_page	(EphyStartHere *sh,
						 const char *id);

G_END_DECLS

#endif
