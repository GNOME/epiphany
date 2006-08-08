/*
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#ifndef EPHY_NOTEBOOK_H
#define EPHY_NOTEBOOK_H

#include "ephy-tab.h"

#include <glib.h>
#include <gtk/gtknotebook.h>

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
				     EphyTab *tab);
};

GType		ephy_notebook_get_type		(void);

int		ephy_notebook_add_tab		(EphyNotebook *nb,
						 EphyTab *tab,
						 int position,
						 gboolean jump_to);
	
void		ephy_notebook_set_show_tabs	(EphyNotebook *nb,
						 gboolean show_tabs);

void		ephy_notebook_set_dnd_enabled	(EphyNotebook *nb,
						 gboolean enabled);

GList *         ephy_notebook_get_focused_pages (EphyNotebook *nb);

G_END_DECLS

#endif /* EPHY_NOTEBOOK_H */
