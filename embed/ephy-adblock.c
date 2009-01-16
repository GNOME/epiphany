/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
 *  Copyright © 2005 Jean-François Rameau
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

#include "config.h"

#include "ephy-adblock.h"

GType
ephy_adblock_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyAdBlockIface),
			NULL,
			NULL,
		};
	
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyAdBlock",
					       &our_info, 0);
	}

	return type;
}

gboolean
ephy_adblock_should_load (EphyAdBlock *adblock,
			  EphyEmbed *embed,
			  const char *url,
			  AdUriCheckType check_type)
{
	EphyAdBlockIface *iface = EPHY_ADBLOCK_GET_IFACE (adblock);
	
	if (iface->should_load)
	{
		return iface->should_load (adblock, embed, url, check_type);
	}

	return TRUE;
}

void
ephy_adblock_edit_rule (EphyAdBlock *adblock,
			const char *url,
			gboolean allowed)
{
	EphyAdBlockIface *iface = EPHY_ADBLOCK_GET_IFACE (adblock);
	if (iface->edit_rule)
	{
		iface->edit_rule (adblock, url, allowed);
	}
}
