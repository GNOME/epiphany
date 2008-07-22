/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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
 *  $Id$
 */

#ifndef EPHY_EMBED_SHELL_H
#define EPHY_EMBED_SHELL_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_SHELL		(ephy_embed_shell_get_type ())
#define EPHY_EMBED_SHELL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_SHELL, EphyEmbedShell))
#define EPHY_EMBED_SHELL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellClass))
#define EPHY_IS_EMBED_SHELL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_SHELL))
#define EPHY_IS_EMBED_SHELL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_SHELL))
#define EPHY_EMBED_SHELL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellClass))

typedef struct _EphyEmbedShellClass	EphyEmbedShellClass;
typedef struct _EphyEmbedShell		EphyEmbedShell;
typedef struct _EphyEmbedShellPrivate	EphyEmbedShellPrivate;

extern EphyEmbedShell *embed_shell;

struct _EphyEmbedShell
{
	GObject parent;

	/*< private >*/
	EphyEmbedShellPrivate *priv;
};

struct _EphyEmbedShellClass
{
	GObjectClass parent_class;

	void	  (* prepare_close)	(EphyEmbedShell *shell);

	/*< private >*/
	GObject * (* get_embed_single)  (EphyEmbedShell *shell);
};

GType		   ephy_embed_shell_get_type		(void);

EphyEmbedShell	  *ephy_embed_shell_get_default		(void);

GObject		  *ephy_embed_shell_get_favicon_cache	(EphyEmbedShell *ges);

GObject		  *ephy_embed_shell_get_global_history	(EphyEmbedShell *shell);

GObject		  *ephy_embed_shell_get_downloader_view	(EphyEmbedShell *shell);

GObject		  *ephy_embed_shell_get_downloader_view_nocreate	(EphyEmbedShell *shell);

GObject		  *ephy_embed_shell_get_encodings	(EphyEmbedShell *shell);

GObject		  *ephy_embed_shell_get_embed_single	(EphyEmbedShell *shell);

GObject        	  *ephy_embed_shell_get_adblock_manager	(EphyEmbedShell *shell);

void		   ephy_embed_shell_prepare_close	(EphyEmbedShell *shell);

void		   ephy_embed_shell_set_page_setup	(EphyEmbedShell *shell,
							 GtkPageSetup *page_setup);
		
GtkPageSetup	  *ephy_embed_shell_get_page_setup	(EphyEmbedShell *shell);

void		   ephy_embed_shell_set_print_settings	(EphyEmbedShell *shell,
							 GtkPrintSettings *settings);
		
GtkPrintSettings  *ephy_embed_shell_get_print_settings	(EphyEmbedShell *shell);

G_END_DECLS

#endif /* !EPHY_EMBED_SHELL_H */
