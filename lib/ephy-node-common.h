/*
 *  Copyright © 2003 Xan Lopez <xan@masilla.org>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef __EPHY_NODE_COMMON_H
#define __EPHY_NODE_COMMON_H

/* Databases */
#define EPHY_NODE_DB_HISTORY "EphyHistory"
#define EPHY_NODE_DB_BOOKMARKS "EphyBookmarks"
#define EPHY_NODE_DB_SITEICONS "EphySiteIcons"
#define EPHY_NODE_DB_STATES "EphyStates"

/* Root nodes */
enum
{
	BOOKMARKS_NODE_ID = 0,
	KEYWORDS_NODE_ID = 1,
	FAVORITES_NODE_ID = 2,
	BMKS_NOTCATEGORIZED_NODE_ID = 3,
	STATES_NODE_ID = 4,
	HOSTS_NODE_ID = 5,
	PAGES_NODE_ID = 6,
	ICONS_NODE_ID = 9,
	SMARTBOOKMARKS_NODE_ID = 10,
	BMKS_LOCAL_NODE_ID = 11,
};

typedef enum
{
	EPHY_NODE_ALL_PRIORITY,
	EPHY_NODE_SPECIAL_PRIORITY,
	EPHY_NODE_NORMAL_PRIORITY
} EphyNodePriority;

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#endif /* EPHY_NODE_COMMON_H */
