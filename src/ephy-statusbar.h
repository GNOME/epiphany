/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_STATUSBAR_H
#define EPHY_STATUSBAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_STATUSBAR		(ephy_statusbar_get_type ())
#define EPHY_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_STATUSBAR, EphyStatusbar))
#define EPHY_STATUSBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_STATUSBAR, EphyStatusbarClass))
#define EPHY_IS_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_STATUSBAR))
#define EPHY_IS_STATUSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_STATUSBAR))
#define EPHY_STATUSBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_STATUSBAR, EphyStatusbarClass))

typedef struct _EphyStatusbar		EphyStatusbar;
typedef struct _EphyStatusbarPrivate	EphyStatusbarPrivate;
typedef struct _EphyStatusbarClass	EphyStatusbarClass;

struct _EphyStatusbar
{
        GtkStatusbar parent;

	/*< private >*/
        EphyStatusbarPrivate *priv;
};

struct _EphyStatusbarClass
{
        GtkStatusbarClass parent_class;

	/* Signals */
	void (* lock_clicked)	(EphyStatusbar *statusbar);
};

GType         ephy_statusbar_get_type			(void);

GtkWidget    *ephy_statusbar_new			(void);

void	      ephy_statusbar_set_caret_mode		(EphyStatusbar *statusbar,
							 gboolean enabled);

void          ephy_statusbar_set_popups_state		(EphyStatusbar *statusbar,
							 gboolean hidden,
							 const char *tooltip);

void	      ephy_statusbar_add_widget			(EphyStatusbar *statusbar,
							 GtkWidget *widget);

void	      ephy_statusbar_remove_widget		(EphyStatusbar *statusbar,
							 GtkWidget *widget);

G_END_DECLS

#endif
