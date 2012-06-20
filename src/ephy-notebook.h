/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#ifndef EPHY_NOTEBOOK_H
#define EPHY_NOTEBOOK_H

#include <glib.h>
#include <gtk/gtk.h>

#include "ephy-embed.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NOTEBOOK		(ephy_notebook_get_type ())
#define EPHY_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NOTEBOOK, EphyNotebook))
#define EPHY_NOTEBOOK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NOTEBOOK, EphyNotebookClass))
#define EPHY_IS_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NOTEBOOK))
#define EPHY_IS_NOTEBOOK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NOTEBOOK))
#define EPHY_NOTEBOOK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NOTEBOOK, EphyNotebookClass))

typedef struct _EphyNotebookClass	EphyNotebookClass;
typedef struct _EphyNotebook		EphyNotebook;
typedef struct _EphyNotebookPrivate	EphyNotebookPrivate;

struct _EphyNotebook
{
	GtkNotebook parent;

	/*< private >*/
        EphyNotebookPrivate *priv;
};

struct _EphyNotebookClass
{
        GtkNotebookClass parent_class;

	/* Signals */
	void	 (* tab_close_req)  (EphyNotebook *notebook,
				     EphyEmbed *embed);
};

GType		ephy_notebook_get_type		(void);

int		ephy_notebook_add_tab		(EphyNotebook *nb,
						 EphyEmbed *embed,
						 int position,
						 gboolean jump_to);
	
void		ephy_notebook_set_tabs_allowed	(EphyNotebook *nb,
						 gboolean tabs_allowed);

void            ephy_notebook_next_page         (EphyNotebook *notebook);

void            ephy_notebook_prev_page         (EphyNotebook *notebook);

G_END_DECLS

#endif /* EPHY_NOTEBOOK_H */
