/*
 *  Copyright (C) 2002 Christophe Fergeau
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

#ifndef EPHY_NOTEBOOK_H
#define EPHY_NOTEBOOK_H

#include <glib.h>
#include <gtk/gtknotebook.h>

G_BEGIN_DECLS

typedef struct EphyNotebookClass EphyNotebookClass;

#define EPHY_NOTEBOOK_TYPE             (ephy_notebook_get_type ())
#define EPHY_NOTEBOOK(obj)             (GTK_CHECK_CAST ((obj), EPHY_NOTEBOOK_TYPE, EphyNotebook))
#define EPHY_NOTEBOOK_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_NOTEBOOK_TYPE, EphyNotebookClass))
#define IS_EPHY_NOTEBOOK(obj)          (GTK_CHECK_TYPE ((obj), EPHY_NOTEBOOK_TYPE))
#define IS_EPHY_NOTEBOOK_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_NOTEBOOK))

typedef struct EphyNotebook EphyNotebook;
typedef struct EphyNotebookPrivate EphyNotebookPrivate;

typedef enum
{
	EPHY_NOTEBOOK_TAB_LOAD_NORMAL,
	EPHY_NOTEBOOK_TAB_LOAD_LOADING,
	EPHY_NOTEBOOK_TAB_LOAD_COMPLETED
} EphyNotebookPageLoadStatus;

enum
{
	EPHY_NOTEBOOK_INSERT_LAST = -1,
	EPHY_NOTEBOOK_INSERT_GROUPED = -2
};

struct EphyNotebook
{
	GtkNotebook parent;
        EphyNotebookPrivate *priv;
};

struct EphyNotebookClass
{
        GtkNotebookClass parent_class;

	/* Signals */
	void (* tab_dropped)       (EphyNotebook *dest,
				    GtkWidget *widget,
				    EphyNotebook *src,
				    gint src_page);
	void (* tab_detached)      (EphyNotebook *dest,
				    gint cur_page,
				    gint root_x, gint root_y);

};

GType		ephy_notebook_get_type		(void);

GtkWidget      *ephy_notebook_new		(void);

void		ephy_notebook_insert_page	(EphyNotebook *nb,
						 GtkWidget *child,
						 int position,
						 gboolean jump_to);

void		ephy_notebook_remove_page	(EphyNotebook *nb,
						 GtkWidget *child);

void            ephy_notebook_move_page         (EphyNotebook *src,
						 EphyNotebook *dest,
						 GtkWidget *src_page,
						 gint dest_page);

void		ephy_notebook_set_page_status	(EphyNotebook *nb,
						 GtkWidget *child,
						 EphyNotebookPageLoadStatus status);

void		ephy_notebook_set_page_title	(EphyNotebook *nb,
						 GtkWidget *child,
						 const char *title);

G_END_DECLS;

#endif /* EPHY_NOTEBOOK_H */
