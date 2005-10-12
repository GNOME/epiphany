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

#include "ephy-embed.h"

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_SINGLE		(ephy_embed_single_get_type ())
#define EPHY_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingle))
#define EPHY_EMBED_SINGLE_IFACE(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleIface))
#define EPHY_IS_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_IS_EMBED_SINGLE_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_EMBED_SINGLE_GET_IFACE(i)	(G_TYPE_INSTANCE_GET_INTERFACE ((i), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleIface))

typedef struct _EphyEmbedSingle		EphyEmbedSingle;
typedef struct _EphyEmbedSingleIface	EphyEmbedSingleIface;

typedef enum
{
	EPHY_CONTENT_CHECK_TYPE_OTHER	    = 1U,
	EPHY_CONTENT_CHECK_TYPE_SCRIPT	    = 2U, /* Indicates an executable script (such as JavaScript) */
	EPHY_CONTENT_CHECK_TYPE_IMAGE	    = 3U, /* Indicates an image (e.g., IMG elements) */
	EPHY_CONTENT_CHECK_TYPE_STYLESHEET  = 4U, /* Indicates a stylesheet (e.g., STYLE elements) */
	EPHY_CONTENT_CHECK_TYPE_OBJECT	    = 5U, /* Indicates a generic object (plugin-handled content typically falls under this category) */
	EPHY_CONTENT_CHECK_TYPE_DOCUMENT    = 6U, /* Indicates a document at the top-level (i.e., in a browser) */
	EPHY_CONTENT_CHECK_TYPE_SUBDOCUMENT = 7U, /* Indicates a document contained within another document (e.g., IFRAMEs, FRAMES, and OBJECTs) */
	EPHY_CONTENT_CHECK_TYPE_REFRESH	    = 8U  /* Indicates a timed refresh */
} EphyContentCheckType;

struct _EphyEmbedSingleIface
{
	GTypeInterface base_iface;

	/* Signals */

	EphyEmbed * (* new_window)  (EphyEmbedSingle *single,
				     EphyEmbed *parent_embed,
				     EphyEmbedChrome chromemask);

	gboolean (* handle_content) (EphyEmbedSingle *shell,
				     char *mime_type,
				     char *uri);

        gboolean (* add_sidebar)    (EphyEmbedSingle *single,
				     const char *url,
				     const char *title);

        gboolean (* add_search_engine) (EphyEmbedSingle *single,
					const char *url,
					const char *icon_url,
					const char *title);

	gboolean (* check_content)  (EphyEmbedSingle *single,
				     EphyContentCheckType type,
				     const char *address,
				     const char *requesting_address,
				     const char *mime_type_guess);
	/* Methods */

	GtkWidget *	  (* open_window)	(EphyEmbedSingle *single,
						 EphyEmbed *parent,
						 const char *address,
						 const char *name,
						 const char *features);
	void		  (* clear_cache)	(EphyEmbedSingle *shell);
	void		  (* clear_auth_cache)	(EphyEmbedSingle *shell);
	void		  (* set_network_status)(EphyEmbedSingle *shell,
						 gboolean offline);
	gboolean	  (* get_network_status)(EphyEmbedSingle *single);
	GList *		  (* get_font_list)	(EphyEmbedSingle *shell,
						 const char *langGroup);
};

GType		ephy_embed_single_get_type		(void);

GtkWidget      *ephy_embed_single_open_window		(EphyEmbedSingle *single,
							 EphyEmbed *parent,
							 const char *address,
							 const char *name,
							 const char *features);

void		ephy_embed_single_clear_cache		(EphyEmbedSingle *single);

void		ephy_embed_single_clear_auth_cache	(EphyEmbedSingle *single);

void		ephy_embed_single_set_network_status	(EphyEmbedSingle *single,
							 gboolean online);

gboolean	ephy_embed_single_get_network_status	(EphyEmbedSingle *single);

GList		*ephy_embed_single_get_font_list	(EphyEmbedSingle *single,
							 const char *lang_group);

G_END_DECLS

#endif
