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

typedef struct EphyBookmarksClass EphyBookmarksClass;

#define EPHY_BOOKMARKS_TYPE             (ephy_bookmarks_get_type ())
#define EPHY_BOOKMARKS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_BOOKMARKS_TYPE, EphyBookmarks))
#define EPHY_BOOKMARKS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_BOOKMARKS_TYPE, EphyBookmarksClass))
#define IS_EPHY_BOOKMARKS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_BOOKMARKS_TYPE))
#define IS_EPHY_BOOKMARKS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_BOOKMARKS_TYPE))

typedef struct EphyBookmarks EphyBookmarks;
typedef struct EphyBookmarksPrivate EphyBookmarksPrivate;

enum
{
	EPHY_NODE_BMK_PROP_TITLE = 2,
	EPHY_NODE_BMK_PROP_LOCATION = 3,
	EPHY_NODE_BMK_PROP_KEYWORDS = 4,
	EPHY_NODE_KEYWORD_PROP_NAME = 5,
	EPHY_NODE_BMK_PROP_SMART_LOCATION = 6,
	EPHY_NODE_BMK_PROP_ICON = 7,
	EPHY_NODE_KEYWORD_PROP_PRIORITY = 8
};

typedef enum
{
	EPHY_BOOKMARKS_KEYWORD_ALL_PRIORITY,
	EPHY_BOOKMARKS_KEYWORD_SPECIAL_PRIORITY,
	EPHY_BOOKMARKS_KEYWORD_NORMAL_PRIORITY
} EphyBookmarksKeywordPriority;

struct EphyBookmarks
{
        GObject parent;
        EphyBookmarksPrivate *priv;
};

struct EphyBookmarksClass
{
        GObjectClass parent_class;

	void (* bookmark_remove) (EphyBookmarks *eb,
			          long id);
	void (* topic_remove)    (EphyBookmarks *eb,
			          long id);
};

GType		ephy_bookmarks_get_type		(void);

EphyBookmarks  *ephy_bookmarks_new		(void);

/* Bookmarks */

EphyNode       *ephy_bookmarks_add		(EphyBookmarks *eb,
						 const char *title,
						 const char *url,
						 const char *smart_url);

guint		 ephy_bookmarks_get_bookmark_id (EphyBookmarks *eb,
						 const char *url);

void		 ephy_bookmarks_set_icon	(EphyBookmarks *eb,
						 const char *url,
						 const char *icon);

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

/* Favorites */

EphyNode       *ephy_bookmarks_get_favorites	(EphyBookmarks *eb);

/* Root */

EphyNode       *ephy_bookmarks_get_keywords	(EphyBookmarks *eb);

EphyNode       *ephy_bookmarks_get_bookmarks	(EphyBookmarks *eb);

G_END_DECLS

#endif
