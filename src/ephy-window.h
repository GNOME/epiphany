/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#ifndef EPHY_WINDOW_H
#define EPHY_WINDOW_H

#include "ephy-embed.h"
#include "ephy-dialog.h"
#include "ephy-notebook.h"
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

typedef struct EphyWindowClass EphyWindowClass;

#define EPHY_WINDOW_TYPE             (ephy_window_get_type ())
#define EPHY_WINDOW(obj)             (GTK_CHECK_CAST ((obj), EPHY_WINDOW_TYPE, EphyWindow))
#define EPHY_WINDOW_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_WINDOW, EphyWindowClass))
#define IS_EPHY_WINDOW(obj)          (GTK_CHECK_TYPE ((obj), EPHY_WINDOW_TYPE))
#define IS_EPHY_WINDOW_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_WINDOW))

typedef struct EphyWindow EphyWindow;
typedef struct EphyWindowPrivate EphyWindowPrivate;
typedef struct Toolbar Toolbar;

struct EphyWindow
{
        GtkWindow parent;
        EphyWindowPrivate *priv;

	/* Public to toolbar and statusbar, dont use outside */
	GObject *ui_merge;
};

struct EphyWindowClass
{
        GtkWindowClass parent_class;
};

typedef enum
{
	NormalMode,
	FullscreenMode
} EphyWindowMode;

typedef enum
{
	TabsControl,
	NavControl,
	FindControl,
	ZoomControl,
	CharsetsControl,
	TitleControl,
	LocationControl,
	FaviconControl,
	StatusbarSecurityControl,
	StatusbarMessageControl,
	StatusbarProgressControl,
	SpinnerControl,
	WindowVisibilityControl,
	BMAndHistoryControl,
	TabsAppeareanceControl,
	FavoritesControl
} ControlID;

/* Include the header down here to resolve circular dependency */
#include "ephy-tab.h"

GType		  ephy_window_get_type		  (void);

EphyWindow	 *ephy_window_new		  (void);

void		  ephy_window_set_chrome	  (EphyWindow *window,
						   EmbedChromeMask chrome_flags);

EmbedChromeMask   ephy_window_get_chrome	  (EphyWindow *window);

GtkWidget	 *ephy_window_get_notebook	  (EphyWindow *window);

void		  ephy_window_add_tab		  (EphyWindow *window,
						   EphyTab *tab,
						   gboolean jump_to);

void		  ephy_window_remove_tab	  (EphyWindow *window,
						   EphyTab *tab);

void		  ephy_window_jump_to_tab	  (EphyWindow *window,
						   EphyTab *tab);

void		  ephy_window_load_url		  (EphyWindow *window,
						   const char *url);

void		  ephy_window_set_zoom		  (EphyWindow *window,
						   gint zoom);

void		  ephy_window_activate_location	  (EphyWindow *window);

void		  ephy_window_update_control	  (EphyWindow *window,
						   ControlID control);

void		  ephy_window_update_all_controls (EphyWindow *window);

EphyTab		 *ephy_window_get_active_tab	  (EphyWindow *window);

EphyEmbed	 *ephy_window_get_active_embed	  (EphyWindow *window);

GList		 *ephy_window_get_tabs		  (EphyWindow *window);

Toolbar		 *ephy_window_get_toolbar	  (EphyWindow *window);

/* Dialogs */

EphyDialog       *ephy_window_get_find_dialog	  (EphyWindow *window);

void		  ephy_window_show_history	  (EphyWindow *window);

G_END_DECLS

#endif
