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

#ifndef EPHY_TYPES_H
#define EPHY_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	G_OK,
	G_FAILED,
	G_NOT_IMPLEMENTED
} gresult;

/* Ids of the root nodes of history,
 * bookmarks and favicon cache */
enum
{
	BOOKMARKS_NODE_ID = 0,
	KEYWORDS_NODE_ID = 1,
	FAVORITES_NODE_ID = 2,
	HOSTS_NODE_ID = 5,
	PAGES_NODE_ID = 6,
	ICONS_NODE_ID = 9,
};

G_END_DECLS

#endif
