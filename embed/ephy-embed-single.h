/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#ifndef EPHY_EMBED_SINGLE_H
#define EPHY_EMBED_SINGLE_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_SINGLE		(ephy_embed_single_get_type ())
#define EPHY_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingle))
#define EPHY_EMBED_SINGLE_IFACE(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleIFace))
#define EPHY_IS_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_IS_EMBED_SINGLE_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_EMBED_SINGLE_GET_IFACE(i)	(G_TYPE_INSTANCE_GET_INTERFACE ((i), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleIFace))

typedef struct EphyEmbedSingle		EphyEmbedSingle;
typedef struct EphyEmbedSingleIFace	EphyEmbedSingleIFace;

struct EphyEmbedSingleIFace
{
	GTypeInterface base_iface;

	/* Signals */

	gboolean (* handle_content) (EphyEmbedSingle *shell,
				     char *mime_type,
				     char *uri);

	/* Methods */

	void	(* clear_cache)         (EphyEmbedSingle *shell);
	void	(* clear_auth_cache)	(EphyEmbedSingle *shell);
	void	(* set_offline_mode)    (EphyEmbedSingle *shell,
					 gboolean offline);
	void	(* load_proxy_autoconf) (EphyEmbedSingle *shell,
					 const char* url);
	GList *	(* get_font_list)	(EphyEmbedSingle *shell,
					 const char *langGroup);
};

GType	ephy_embed_single_get_type		(void);

void	ephy_embed_single_clear_cache		(EphyEmbedSingle *shell);

void	ephy_embed_single_clear_auth_cache	(EphyEmbedSingle *shell);

void	ephy_embed_single_set_offline_mode	(EphyEmbedSingle *shell,
						 gboolean offline);

void	ephy_embed_single_load_proxy_autoconf	(EphyEmbedSingle *shell,
						 const char* url);

GList  *ephy_embed_single_get_font_list		(EphyEmbedSingle *shell,
						 const char *langGroup);

G_END_DECLS

#endif
