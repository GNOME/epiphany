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

#ifndef EPHY_BOOKMARKS_H
#define EPHY_BOOKMARKS_H

#include <glib-object.h>

#include "ephy-node.h"

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKS		(ephy_bookmarks_get_type ())
#define EPHY_BOOKMARKS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_BOOKMARKS, EphyBookmarks))
#define EPHY_BOOKMARKS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_BOOKMARKS, EphyBookmarksClass))
#define EPHY_IS_BOOKMARKS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_BOOKMARKS))
#define EPHY_IS_BOOKMARKS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_BOOKMARKS))
#define EPHY_BOOKMARKS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_BOOKMARKS, EphyBookmarksClass))

typedef struct EphyBookmarksClass EphyBookmarksClass;
typedef struct EphyBookmarks EphyBookmarks;
typedef struct EphyBookmarksPrivate EphyBookmarksPrivate;

enum
{
	EPHY_NODE_BMK_PROP_TITLE = 2,
	EPHY_NODE_BMK_PROP_LOCATION = 3,
	EPHY_NODE_BMK_PROP_KEYWORDS = 4,
	EPHY_NODE_KEYWORD_PROP_NAME = 5,
	EPHY_NODE_BMK_PROP_ICON = 7,
	EPHY_NODE_KEYWORD_PROP_PRIORITY = 8
};

struct EphyBookmarks
{
        GObject parent;
        EphyBookmarksPrivate *priv;
};

struct EphyBookmarksClass
{
        GObjectClass parent_class;

	void (* tree_changed) (EphyBookmarks *eb);
};

GType		ephy_bookmarks_get_type		(void);

EphyBookmarks  *ephy_bookmarks_new		(void);

EphyNode	*ephy_bookmarks_get_from_id     (EphyBookmarks *eb,
						 long id);

/* Bookmarks */

EphyNode        *ephy_bookmarks_add		(EphyBookmarks *eb,
						 const char *title,
						 const char *url);

EphyNode*	 ephy_bookmarks_find_bookmark   (EphyBookmarks *eb,
						 const char *url);

void		 ephy_bookmarks_set_icon	(EphyBookmarks *eb,
						 const char *url,
						 const char *icon);

void		 ephy_bookmarks_set_address     (EphyBookmarks *eb,
			                         EphyNode *bookmark,
			                         const char *address);

char		*ephy_bookmarks_solve_smart_url (EphyBookmarks *eb,
						 const char *smart_url,
						 const char *content);

/* Keywords */

EphyNode       *ephy_bookmarks_add_keyword	(EphyBookmarks *eb,
						 const char *name);

EphyNode       *ephy_bookmarks_find_keyword     (EphyBookmarks *eb,
			                         const char *name,
						 gboolean partial_match);

void            ephy_bookmarks_remove_keyword   (EphyBookmarks *eb,
						 EphyNode *keyword);

gboolean        ephy_bookmarks_has_keyword      (EphyBookmarks *eb,
						 EphyNode *keyword,
						 EphyNode *bookmark);

void            ephy_bookmarks_set_keyword      (EphyBookmarks *eb,
						 EphyNode *keyword,
						 EphyNode *bookmark);

void            ephy_bookmarks_unset_keyword    (EphyBookmarks *eb,
						 EphyNode *keyword,
						 EphyNode *bookmark);

char           *ephy_bookmarks_get_topic_uri    (EphyBookmarks *eb,
			                         EphyNode *node);

/* Favorites */

EphyNode       *ephy_bookmarks_get_favorites	(EphyBookmarks *eb);

/* Root */

EphyNode       *ephy_bookmarks_get_keywords	(EphyBookmarks *eb);

EphyNode       *ephy_bookmarks_get_bookmarks	(EphyBookmarks *eb);

EphyNode       *ephy_bookmarks_get_not_categorized (EphyBookmarks *eb);

EphyNode       *ephy_bookmarks_get_smart_bookmarks (EphyBookmarks *eb);

G_END_DECLS

#endif
