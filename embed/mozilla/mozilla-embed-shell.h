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

#ifndef MOZILLA_EMBED_SHELL_H
#define MOZILLA_EMBED_SHELL_H

#include "glib.h"
#include "ephy-embed-shell.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define MOZILLA_EMBED_SHELL_TYPE             (mozilla_embed_shell_get_type ())
#define MOZILLA_EMBED_SHELL(obj)             (GTK_CHECK_CAST ((obj), MOZILLA_EMBED_SHELL_TYPE, MozillaEmbedShell))
#define MOZILLA_EMBED_SHELL_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), MOZILLA_EMBED_SHELL_TYPE, MozillaEmbedShellClass))
#define IS_MOZILLA_EMBED_SHELL(obj)          (GTK_CHECK_TYPE ((obj), MOZILLA_EMBED_SHELL_TYPE))
#define IS_MOZILLA_EMBED_SHELL_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), MOZILLA_EMBED_SHELL))
#define MOZILLA_EMBED_SHELL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MOZILLA_EMBED_SHELL_TYPE, MozillaEmbedShellClass))

typedef struct MozillaEmbedShell MozillaEmbedShell;
typedef struct MozillaEmbedShellPrivate MozillaEmbedShellPrivate;

extern GObject *mozilla_js_console;

struct MozillaEmbedShell
{
	EphyEmbedShell parent;
        MozillaEmbedShellPrivate *priv;
};

struct MozillaEmbedShellClass
{
        EphyEmbedShellClass parent_class;
};

GType             mozilla_embed_shell_get_type            (void);

G_END_DECLS

#endif
