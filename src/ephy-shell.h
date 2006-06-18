/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2006 Christian Persch
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

#ifndef EPHY_SHELL_H
#define EPHY_SHELL_H

#include "ephy-embed-shell.h"
#include "ephy-bookmarks.h"
#include "ephy-window.h"
#include "ephy-tab.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SHELL		(ephy_shell_get_type ())
#define EPHY_SHELL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SHELL, EphyShell))
#define EPHY_SHELL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SHELL, EphyShellClass))
#define EPHY_IS_SHELL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SHELL))
#define EPHY_IS_SHELL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SHELL))
#define EPHY_SHELL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SHELL, EphyShellClass))

typedef struct _EphyShell		EphyShell;
typedef struct _EphyShellClass		EphyShellClass;
typedef struct _EphyShellPrivate	EphyShellPrivate;

extern EphyShell *ephy_shell;

typedef enum
{
	/* Page types */
	EPHY_NEW_TAB_HOME_PAGE		= 1 << 0,
	EPHY_NEW_TAB_NEW_PAGE		= 1 << 1,
	EPHY_NEW_TAB_OPEN_PAGE		= 1 << 2,

	/* Page mode */
	EPHY_NEW_TAB_FULLSCREEN_MODE	= 1 << 4,
	EPHY_NEW_TAB_DONT_SHOW_WINDOW	= 1 << 5,

	/* Tabs */
	EPHY_NEW_TAB_APPEND_LAST	= 1 << 7,
	EPHY_NEW_TAB_APPEND_AFTER	= 1 << 8,
	EPHY_NEW_TAB_JUMP		= 1 << 9,
	EPHY_NEW_TAB_IN_NEW_WINDOW	= 1 << 10,
	EPHY_NEW_TAB_IN_EXISTING_WINDOW	= 1 << 11,
} EphyNewTabFlags;

struct _EphyShell
{
	EphyEmbedShell parent;

	/*< private >*/
	EphyShellPrivate *priv;
};

struct _EphyShellClass
{
	EphyEmbedShellClass parent_class;
};

GType		ephy_new_tab_flags_get_type		(void) G_GNUC_CONST;

GType		ephy_shell_get_type			(void);

EphyShell      *ephy_shell_get_default			(void);

EphyTab	       *ephy_shell_new_tab			(EphyShell *shell,
							 EphyWindow *parent_window,
							 EphyTab *previous_tab,
							 const char *url,
							 EphyNewTabFlags flags);

EphyTab	       *ephy_shell_new_tab_full			(EphyShell *shell,
							 EphyWindow *parent_window,
							 EphyTab *previous_tab,
							 const char *url,
							 EphyNewTabFlags flags,
							 EphyEmbedChrome chrome,
							 gboolean is_popup,
							 guint32 user_time);

GObject	       *ephy_shell_get_session			(EphyShell *shell);

GObject	       *ephy_shell_get_net_monitor		(EphyShell *shell);

EphyBookmarks  *ephy_shell_get_bookmarks		(EphyShell *shell);

GObject	       *ephy_shell_get_toolbars_model		(EphyShell *shell,
							 gboolean fullscreen);

GObject	       *ephy_shell_get_extensions_manager	(EphyShell *shell);

GtkWidget      *ephy_shell_get_bookmarks_editor 	(EphyShell *shell);

GtkWidget      *ephy_shell_get_history_window		(EphyShell *shell);

GObject        *ephy_shell_get_pdm_dialog		(EphyShell *shell);

GObject        *ephy_shell_get_prefs_dialog		(EphyShell *shell);

/* private API */
void	       _ephy_shell_create_instance		(void);

G_END_DECLS

#endif
