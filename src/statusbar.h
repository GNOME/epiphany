/*
 *  Copyright (C) 2002 Jorn Baayen
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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <gtk/gtkstatusbar.h>

G_BEGIN_DECLS

#define EPHY_TYPE_STATUSBAR		(statusbar_get_type ())
#define EPHY_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_STATUSBAR, Statusbar))
#define EPHY_STATUSBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_STATUSBAR, StatusbarClass))
#define EPHY_IS_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_STATUSBAR))
#define EPHY_IS_STATUSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_STATUSBAR))
#define EPHY_STATUSBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_STATUSBAR, StatusbarClass))

typedef struct Statusbar Statusbar;
typedef struct StatusbarClass StatusbarClass;
typedef struct StatusbarPrivate StatusbarPrivate;

struct Statusbar
{
        GtkStatusbar parent;
        StatusbarPrivate *priv;
};

struct StatusbarClass
{
        GtkStatusbarClass parent_class;
};

GType         statusbar_get_type             (void);

GtkWidget    *statusbar_new                  (void);

void          statusbar_set_security_state   (Statusbar *s,
					      gboolean state,
				              const char *tooltip);

void          statusbar_set_progress         (Statusbar *s,
					      int progress);

void          statusbar_set_message          (Statusbar *s,
					      const gchar *message);

G_END_DECLS

#endif
