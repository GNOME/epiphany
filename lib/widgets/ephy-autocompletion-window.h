/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#ifndef EPHY_AUTOCOMPLETION_WINDOW_H
#define EPHY_AUTOCOMPLETION_WINDOW_H

#include <glib-object.h>
#include <gtk/gtkwidget.h>

#include "ephy-autocompletion.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyAutocompletionWindow EphyAutocompletionWindow;
typedef struct _EphyAutocompletionWindowClass EphyAutocompletionWindowClass;
typedef struct _EphyAutocompletionWindowPrivate EphyAutocompletionWindowPrivate;

/**
 * Editor object
 */

#define EPHY_TYPE_AUTOCOMPLETION_WINDOW		   (ephy_autocompletion_window_get_type())
#define EPHY_AUTOCOMPLETION_WINDOW(object)	   (G_TYPE_CHECK_INSTANCE_CAST((object), \
						    EPHY_TYPE_AUTOCOMPLETION_WINDOW,\
						    EphyAutocompletionWindow))
#define EPHY_AUTOCOMPLETION_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
						    EPHY_TYPE_AUTOCOMPLETION_WINDOW,\
						    EphyAutocompletionWindowClass))
#define EPHY_IS_AUTOCOMPLETION_WINDOW(object)	   (G_TYPE_CHECK_INSTANCE_TYPE((object), \
						    EPHY_TYPE_AUTOCOMPLETION_WINDOW))
#define EPHY_IS_AUTOCOMPLETION_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						    EPHY_TYPE_AUTOCOMPLETION_WINDOW))
#define EPHY_AUTOCOMPLETION_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
						    EPHY_TYPE_AUTOCOMPLETION_WINDOW,\
						    EphyAutocompletionWindowClass))

struct _EphyAutocompletionWindowClass
{
	GObjectClass parent_class;

	/* signals */
	void		(*hidden)			(EphyAutocompletionWindow *aw);
	void		(*activated)			(EphyAutocompletionWindow *aw,
							 const char *target,
							 int action);
	void		(*selected)			(EphyAutocompletionWindow *aw,
							 const char *target,
							 int action);
};

/* Remember: fields are public read-only */
struct _EphyAutocompletionWindow
{
	GObject parent_object;

	EphyAutocompletionWindowPrivate *priv;
};

GType			ephy_autocompletion_window_get_type		(void);
EphyAutocompletionWindow *ephy_autocompletion_window_new		(EphyAutocompletion *ac,
									 GtkWidget *parent);
void			ephy_autocompletion_window_set_parent_widget	(EphyAutocompletionWindow *aw,
									 GtkWidget *w);
void			ephy_autocompletion_window_set_autocompletion	(EphyAutocompletionWindow *aw,
									 EphyAutocompletion *ac);
void			ephy_autocompletion_window_show			(EphyAutocompletionWindow *aw);
void			ephy_autocompletion_window_hide			(EphyAutocompletionWindow *aw);
void			ephy_autocompletion_window_unselect		(EphyAutocompletionWindow *aw);

G_END_DECLS

#endif
