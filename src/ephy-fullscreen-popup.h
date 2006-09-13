/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2005 Christian Persch
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_FULLSCREEN_POPUP_H
#define EPHY_FULLSCREEN_POPUP_H

#include <gtk/gtkwindow.h>
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FULLSCREEN_POPUP		(ephy_fullscreen_popup_get_type ())
#define EPHY_FULLSCREEN_POPUP(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_FULLSCREEN_POPUP, EphyFullscreenPopup))
#define EPHY_FULLSCREEN_POPUP_CLASS(k)  	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_FULLSCREEN_POPUP, EphyFullscreenPopupClass))
#define EPHY_IS_FULLSCREEN_POPUP(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_FULLSCREEN_POPUP))
#define EPHY_IS_FULLSCREEN_POPUP_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_FULLSCREEN_POPUP))
#define EPHY_FULLSCREEN_POPUP_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_FULLSCREEN_POPUP, EphyFullscreenPopupClass))

typedef struct _EphyFullscreenPopup		EphyFullscreenPopup;
typedef struct _EphyFullscreenPopupPrivate	EphyFullscreenPopupPrivate;
typedef struct _EphyFullscreenPopupClass	EphyFullscreenPopupClass;

struct _EphyFullscreenPopup
{
	GtkWindow parent_instance;

	/*< private >*/
	EphyFullscreenPopupPrivate *priv;
};

struct _EphyFullscreenPopupClass
{
	GtkWindowClass parent_class;

	void (* exit_clicked)	(EphyFullscreenPopup *popup);
	void (* lock_clicked)	(EphyFullscreenPopup *popup);
};

GType	   ephy_fullscreen_popup_get_type	    (void);

GtkWidget *ephy_fullscreen_popup_new		    (EphyWindow *window);

void	   ephy_fullscreen_popup_set_show_leave	    (EphyFullscreenPopup *popup,
						     gboolean show_button);

void	   ephy_fullscreen_popup_set_spinning	    (EphyFullscreenPopup *popup,
						     gboolean spinning);

void	   ephy_fullscreen_popup_set_security_state (EphyFullscreenPopup *popup,
						     gboolean show_lock,
						     const char *stock,
						     const char *tooltip);

G_END_DECLS

#endif /* !EPHY_FULLSCREEN_POPUP_H */
