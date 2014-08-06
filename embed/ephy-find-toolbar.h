/*
 *  Copyright © 2004  Tommi Komulainen
 *  Copyright © 2004, 2005  Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_FIND_TOOLBAR_H
#define EPHY_FIND_TOOLBAR_H

#include <gtk/gtk.h>

#include "ephy-web-view.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FIND_TOOLBAR		(ephy_find_toolbar_get_type ())
#define EPHY_FIND_TOOLBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbar))
#define EPHY_FIND_TOOLBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarClass))
#define EPHY_IS_FIND_TOOLBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_FIND_TOOLBAR))
#define EPHY_IS_FIND_TOOLBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_FIND_TOOLBAR))
#define EPHY_FIND_TOOLBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarClass))

typedef struct _EphyFindToolbar		EphyFindToolbar;
typedef struct _EphyFindToolbarPrivate	EphyFindToolbarPrivate;
typedef struct _EphyFindToolbarClass	EphyFindToolbarClass;

struct _EphyFindToolbar
{
	GtkSearchBar parent;

	/*< private >*/
	EphyFindToolbarPrivate *priv;
};

struct _EphyFindToolbarClass
{
	GtkSearchBarClass parent_class;

	/* Signals */
	void (* next)		(EphyFindToolbar *toolbar);
	void (* previous)	(EphyFindToolbar *toolbar);
	void (* close)		(EphyFindToolbar *toolbar);
};

GType		 ephy_find_toolbar_get_type	 (void) G_GNUC_CONST;

EphyFindToolbar *ephy_find_toolbar_new		 (WebKitWebView *web_view);

const char	*ephy_find_toolbar_get_text	 (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_find_next	 (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_open		 (EphyFindToolbar *toolbar,
						  gboolean links_only,
						  gboolean clear_search);

void		 ephy_find_toolbar_close	 (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_request_close (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_toggle_state	 (EphyFindToolbar *toolbar);

G_END_DECLS

#endif /* EPHY_FIND_TOOLBAR_H */
