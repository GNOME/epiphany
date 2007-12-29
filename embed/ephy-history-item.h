/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2007 Xan Lopez
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

#ifndef __EPHY_HISTORY_ITEM_H__
#define __EPHY_HISTORY_ITEM_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_ITEM			      (ephy_history_item_get_type ())
#define EPHY_HISTORY_ITEM(o)			        (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_HISTORY_ITEM, EphyHistoryItem))
#define EPHY_HISTORY_ITEM_IFACE(k)		    (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_HISTORY_ITEM, EphyHistoryItemIface))
#define EPHY_IS_HISTORY_ITEM(o)		        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_HISTORY_ITEM))
#define EPHY_IS_HISTORY_ITEM_IFACE(k)		  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_HISTORY_ITEM))
#define EPHY_HISTORY_ITEM_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_HISTORY_ITEM, EphyHistoryItemIface))

typedef struct _EphyHistoryItem	      EphyHistoryItem;
typedef struct _EphyHistoryItemIface	EphyHistoryItemIface;

struct _EphyHistoryItemIface
{
	GTypeInterface base_iface;

  const char * (* get_url)   (EphyHistoryItem *item);
  const char * (* get_title) (EphyHistoryItem *item);
};

GType       ephy_history_item_get_type  (void);
const char* ephy_history_item_get_url   (EphyHistoryItem *item);
const char* ephy_history_item_get_title (EphyHistoryItem *item);

G_END_DECLS

#endif /* __EPHY_HISTORY_ITEM_H__ */
