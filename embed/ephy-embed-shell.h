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

#ifndef EPHY_EMBED_SHELL_H
#define EPHY_EMBED_SHELL_H

#include "ephy-embed.h"
#include "ephy-embed-single.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "downloader-view.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct EphyEmbedShellClass EphyEmbedShellClass;

#define EPHY_EMBED_SHELL_TYPE             (ephy_embed_shell_get_type ())
#define EPHY_EMBED_SHELL_IMPL             (ephy_embed_shell_get_impl ())
#define EPHY_EMBED_SHELL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_EMBED_SHELL_TYPE, EphyEmbedShell))
#define EPHY_EMBED_SHELL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_EMBED_SHELL_TYPE, EphyEmbedShellClass))
#define IS_EPHY_EMBED_SHELL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_EMBED_SHELL_TYPE))
#define IS_EPHY_EMBED_SHELL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_EMBED_SHELL_TYPE))
#define EPHY_EMBED_SHELL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_EMBED_SHELL_TYPE, EphyEmbedShellClass))

typedef struct EphyEmbedShell EphyEmbedShell;
typedef struct EphyEmbedShellPrivate EphyEmbedShellPrivate;

extern EphyEmbedShell *embed_shell;

struct EphyEmbedShell
{
	GObject parent;
        EphyEmbedShellPrivate *priv;
};

struct EphyEmbedShellClass
{
        GObjectClass parent_class;

	/* Methods */
	EphyHistory    * (* get_global_history)  (EphyEmbedShell *shell);
	DownloaderView * (* get_downloader_view) (EphyEmbedShell *shell);
};

GType             ephy_embed_shell_get_type            (void);

GType             ephy_embed_shell_get_impl            (void);

EphyEmbedShell   *ephy_embed_shell_new                 (const char *type);

EphyFaviconCache *ephy_embed_shell_get_favicon_cache   (EphyEmbedShell *ges);

EphyHistory      *ephy_embed_shell_get_global_history  (EphyEmbedShell *shell);

DownloaderView   *ephy_embed_shell_get_downloader_view (EphyEmbedShell *shell);

EphyEmbedSingle  *ephy_embed_shell_get_embed_single    (EphyEmbedShell *shell);

G_END_DECLS

#endif
