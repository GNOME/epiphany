/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#ifndef EPHY_STATUSBAR_H
#define EPHY_STATUSBAR_H

#include <gtk/gtkstatusbar.h>

G_BEGIN_DECLS

#define EPHY_TYPE_STATUSBAR		(ephy_statusbar_get_type ())
#define EPHY_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_STATUSBAR, EphyStatusbar))
#define EPHY_STATUSBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_STATUSBAR, EphyStatusbarClass))
#define EPHY_IS_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_STATUSBAR))
#define EPHY_IS_STATUSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_STATUSBAR))
#define EPHY_STATUSBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_STATUSBAR, EphyStatusbarClass))

typedef struct EphyStatusbar		EphyStatusbar;
typedef struct EphyStatusbarPrivate	EphyStatusbarPrivate;
typedef struct EphyStatusbarClass	EphyStatusbarClass;

struct EphyStatusbar
{
        GtkStatusbar parent;

	/*< public >*/
	GtkWidget *security_frame;

	/*< private >*/
        EphyStatusbarPrivate *priv;
};

struct EphyStatusbarClass
{
        GtkStatusbarClass parent_class;
};

GType         ephy_statusbar_get_type			(void);

GtkWidget    *ephy_statusbar_new			(void);

void          ephy_statusbar_set_security_state		(EphyStatusbar *statusbar,
							 gboolean secure,
							 const char *tooltip);

void          ephy_statusbar_set_progress		(EphyStatusbar *statusbar,
							 int progress);

G_END_DECLS

#endif
