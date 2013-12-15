/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_BOOKMARKS_H
#define EPHY_BOOKMARKS_H

#include <glib-object.h>

#include "ephy-node.h"

G_BEGIN_DECLS

/* Name of a default smart bookmark, to NOT be displayed in the search engine
 * combo in the preferences dialog. This should exactly match the smart bookmark
 * in default-bookmarks.rdf. */
#define DEFAULT_SMART_BOOKMARK_TEXT	_("Search the web")

#define EPHY_TYPE_BOOKMARKS		(ephy_bookmarks_get_type ())
#define EPHY_BOOKMARKS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_BOOKMARKS, EphyBookmarks))
#define EPHY_BOOKMARKS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_BOOKMARKS, EphyBookmarksClass))
#define EPHY_IS_BOOKMARKS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_BOOKMARKS))
#define EPHY_IS_BOOKMARKS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_BOOKMARKS))
#define EPHY_BOOKMARKS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_BOOKMARKS, EphyBookmarksClass))

typedef struct _EphyBookmarksClass	EphyBookmarksClass;
typedef struct _EphyBookmarks		EphyBookmarks;
typedef struct _EphyBookmarksPrivate	EphyBookmarksPrivate;

typedef enum
{
	EPHY_NODE_BMK_PROP_TITLE	= 2,
	EPHY_NODE_BMK_PROP_LOCATION	= 3,
	EPHY_NODE_BMK_PROP_KEYWORDS	= 4,
	EPHY_NODE_KEYWORD_PROP_NAME	= 5,
	EPHY_NODE_BMK_PROP_USERICON	= 6,
	EPHY_NODE_BMK_PROP_ICON		= 7,
	EPHY_NODE_KEYWORD_PROP_PRIORITY	= 8,
	EPHY_NODE_BMK_PROP_SERVICE_ID	= 14,
	EPHY_NODE_BMK_PROP_IMMUTABLE	= 15
} EphyBookmarkProperty;

struct _EphyBookmarks
{
	GObject parent;

	/*< private >*/
	EphyBookmarksPrivate *priv;
};

struct _EphyBookmarksClass
{
	GObjectClass parent_class;

	void	(* tree_changed)	(EphyBookmarks *eb);
	char *	(* resolve_address)	(EphyBookmarks *eb,
					 const char *address,
					 const char *argument);
};

GType		  ephy_bookmarks_get_type		(void);

EphyBookmarks    *ephy_bookmarks_new			(void);

EphyNode	 *ephy_bookmarks_get_from_id		(EphyBookmarks *eb,
							 long id);

/* Bookmarks */

EphyNode	 *ephy_bookmarks_add			(EphyBookmarks *eb,
							 const char *title,
							 const char *url);

EphyNode*	  ephy_bookmarks_find_bookmark		(EphyBookmarks *eb,
							 const char *url);

gint              ephy_bookmarks_get_similar		(EphyBookmarks *eb,
							 EphyNode *bookmark,
							 GPtrArray *identical,
							 GPtrArray *similar);
    
void		  ephy_bookmarks_set_icon		(EphyBookmarks *eb,
							 const char *url,
							 const char *icon);

void		  ephy_bookmarks_set_usericon		(EphyBookmarks *eb,
							 const char *url,
							 const char *icon);

void		  ephy_bookmarks_set_address    	(EphyBookmarks *eb,
							 EphyNode *bookmark,
							 const char *address);

char		 *ephy_bookmarks_resolve_address	(EphyBookmarks *eb,
							 const char *address,
							 const char *parameter);

guint		ephy_bookmarks_get_smart_bookmark_width (EphyNode *bookmark);


/* Keywords */

EphyNode	 *ephy_bookmarks_add_keyword		(EphyBookmarks *eb,
							 const char *name);

EphyNode	 *ephy_bookmarks_find_keyword		(EphyBookmarks *eb,
							 const char *name,
							 gboolean partial_match);

void		  ephy_bookmarks_remove_keyword		(EphyBookmarks *eb,
							 EphyNode *keyword);

gboolean	  ephy_bookmarks_has_keyword		(EphyBookmarks *eb,
							 EphyNode *keyword,
							 EphyNode *bookmark);

void		  ephy_bookmarks_set_keyword		(EphyBookmarks *eb,
							 EphyNode *keyword,
							 EphyNode *bookmark);

void		  ephy_bookmarks_unset_keyword		(EphyBookmarks *eb,
							 EphyNode *keyword,
							 EphyNode *bookmark);

char		 *ephy_bookmarks_get_topic_uri		(EphyBookmarks *eb,
							 EphyNode *node);

/* Root */

EphyNode	 *ephy_bookmarks_get_keywords		(EphyBookmarks *eb);

EphyNode	 *ephy_bookmarks_get_bookmarks		(EphyBookmarks *eb);

EphyNode	 *ephy_bookmarks_get_not_categorized	(EphyBookmarks *eb);

EphyNode	 *ephy_bookmarks_get_smart_bookmarks	(EphyBookmarks *eb);

EphyNode	 *ephy_bookmarks_get_local		(EphyBookmarks *eb);

/* Comparison functions, useful for sorting lists and arrays. */
int            ephy_bookmarks_compare_topics            (gconstpointer a, gconstpointer b);
int            ephy_bookmarks_compare_topic_pointers    (gconstpointer a, gconstpointer b);
int            ephy_bookmarks_compare_bookmarks         (gconstpointer a, gconstpointer b);
int            ephy_bookmarks_compare_bookmark_pointers (gconstpointer a, gconstpointer b);

G_END_DECLS

#endif
