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

#include "ephy-history-item.h"

GType
ephy_history_item_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo our_info =
		{
			sizeof (EphyHistoryItemIface),
			NULL,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
                                   "EphyHistoryItem",
                                   &our_info, (GTypeFlags)0);
	}

	return type;
}

const char*
ephy_history_item_get_url (EphyHistoryItem *item)
{
	EphyHistoryItemIface *iface = EPHY_HISTORY_ITEM_GET_IFACE (item);
	return iface->get_url (item);
}

const char*
ephy_history_item_get_title (EphyHistoryItem *item)
{
	EphyHistoryItemIface *iface = EPHY_HISTORY_ITEM_GET_IFACE (item);
	return iface->get_title (item);
}
