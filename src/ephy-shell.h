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

#ifndef EPHY_SHELL_H
#define EPHY_SHELL_H

#include "ephy-embed-shell.h"
#include "ephy-bookmarks.h"
#include "ephy-window.h"
#include "ephy-tab.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#ifndef EPHY_SHELL_TYPE_DEF
typedef struct EphyShell EphyShell;
#define EPHY_SHELL_TYPE_DEF
#endif

typedef struct EphyShellClass EphyShellClass;

#define EPHY_SHELL_TYPE             (ephy_shell_get_type ())
#define EPHY_SHELL(obj)             (GTK_CHECK_CAST ((obj), EPHY_SHELL_TYPE, EphyShell))
#define EPHY_SHELL_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_SHELL, EphyShellClass))
#define IS_EPHY_SHELL(obj)          (GTK_CHECK_TYPE ((obj), EPHY_SHELL_TYPE))
#define IS_EPHY_SHELL_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_SHELL))

typedef struct EphyShellPrivate EphyShellPrivate;

extern EphyShell *ephy_shell;

typedef enum
{
	/* Page types */
	EPHY_NEW_TAB_HOME_PAGE		= 1 << 0,
	EPHY_NEW_TAB_NEW_PAGE		= 1 << 1,
	EPHY_NEW_TAB_OPEN_PAGE		= 1 << 2,
	EPHY_NEW_TAB_CLONE_PAGE		= 1 << 3,

	/* Page mode */
	EPHY_NEW_TAB_FULLSCREEN_MODE	= 1 << 4,
	EPHY_NEW_TAB_SOURCE_MODE	= 1 << 5,

	/* Tabs */
	EPHY_NEW_TAB_APPEND_GROUPED	= 1 << 6,
	EPHY_NEW_TAB_APPEND_LAST	= 1 << 7,
	EPHY_NEW_TAB_APPEND_AFTER	= 1 << 8,
	EPHY_NEW_TAB_JUMP		= 1 << 9,
	EPHY_NEW_TAB_IN_NEW_WINDOW	= 1 << 10,
	EPHY_NEW_TAB_IN_EXISTING_WINDOW	= 1 << 11
} EphyNewTabFlags;

struct EphyShell
{
        EphyEmbedShell parent;
        EphyShellPrivate *priv;
};

struct EphyShellClass
{
        EphyEmbedShellClass parent_class;
};

GType               ephy_shell_get_type              (void);

EphyShell	   *ephy_shell_new                   (void);

EphyWindow	   *ephy_shell_get_active_window     (EphyShell *gs);

EphyTab            *ephy_shell_new_tab	             (EphyShell *shell,
						      EphyWindow *parent_window,
						      EphyTab *previous_tab,
						      const char *url,
						      EphyNewTabFlags flags);

GObject		   *ephy_shell_get_session	     (EphyShell *gs);

GObject            *ephy_shell_get_autocompletion    (EphyShell *gs);

EphyBookmarks      *ephy_shell_get_bookmarks         (EphyShell *gs);

GObject		   *ephy_shell_get_toolbars_model    (EphyShell *gs);

void		    ephy_shell_show_bookmarks_editor (EphyShell *gs,
						      GtkWidget *parent);

void		    ephy_shell_show_history_window   (EphyShell *gs,
						      GtkWidget *parent);

gboolean	    ephy_shell_get_server_mode	     (EphyShell *es);

void		    ephy_shell_set_server_mode	     (EphyShell *es,
						      gboolean as_server);

G_END_DECLS

#endif
