/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-toolbar-item-factory.h"
#include <string.h>

#include "ephy-tbi-zoom.h"
#include "ephy-tbi-separator.h"
#include "ephy-tbi-favicon.h"
#include "ephy-tbi-spinner.h"
#include "ephy-tbi-location.h"
#include "ephy-tbi-navigation-history.h"
#include "ephy-tbi-std-toolitem.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

typedef struct
{
	const char *type_name;
	EphyTbItemConstructor constructor;
} EphyTbItemTypeInfo;

static EphyTbItemTypeInfo ephy_tb_item_default_types[] =
{
	{ "std_toolitem",		(EphyTbItemConstructor) ephy_tbi_std_toolitem_new },
	{ "navigation_history",		(EphyTbItemConstructor) ephy_tbi_navigation_history_new },
	{ "zoom",			(EphyTbItemConstructor) ephy_tbi_zoom_new },
	{ "location",			(EphyTbItemConstructor) ephy_tbi_location_new },
	{ "spinner",			(EphyTbItemConstructor) ephy_tbi_spinner_new },
	{ "favicon",			(EphyTbItemConstructor) ephy_tbi_favicon_new },
	{ "separator",			(EphyTbItemConstructor) ephy_tbi_separator_new },
	{ NULL,				NULL }
};

static GHashTable *ephy_tb_item_known_types = NULL;

static void
ephy_tb_item_factory_init (void)
{
	if (ephy_tb_item_known_types == NULL)
	{
		int i;
		ephy_tb_item_known_types = g_hash_table_new (g_str_hash, g_str_equal);

		for (i = 0; ephy_tb_item_default_types[i].type_name; ++i)
		{
			ephy_toolbar_item_register_type (ephy_tb_item_default_types[i].type_name,
							 ephy_tb_item_default_types[i].constructor);
		}
	}
}

EphyTbItem *
ephy_toolbar_item_create_from_string (const gchar *str)
{
	EphyTbItem *ret = NULL;
	gchar *type;
	gchar *props;
	gchar *id;
	const gchar *rest;
	const gchar *lpar;
	const gchar *rpar;
	const gchar *eq;
	EphyTbItemConstructor constructor;

	ephy_tb_item_factory_init ();

	rest = str;

	eq = strchr (rest, '=');
	if (eq)
	{
		id = g_strndup (rest, eq - rest);
		rest = eq + 1;
	}
	else
	{
		id = NULL;
	}

	lpar = strchr (rest, '(');
	if (lpar)
	{
		type = g_strndup (rest, lpar - rest);
		rest = lpar + 1;

		rpar = strchr (rest, ')');
		if (rpar)
		{
			props = g_strndup (rest, rpar - rest);
			rest = rpar + 1;
		}
		else
		{
			props = g_strdup (rest);
		}
	}
	else
	{
		type = g_strdup (rest);
		props = NULL;
	}

	DEBUG_MSG (("ephy_toolbar_item_create_from_string id=%s type=%s props=%s\n", id, type, props));

	constructor = g_hash_table_lookup (ephy_tb_item_known_types, type);

	if (constructor)
	{
		ret = constructor ();
		if (id)
		{
			ephy_tb_item_set_id (ret, id);
		}
		if (props)
		{
			ephy_tb_item_parse_properties (ret, props);
		}
	}

	if (!ret)
	{
		g_warning ("Error creating toolbar item of type %s", type);
	}

	if (id)
	{
		g_free (id);
	}
	if (type)
	{
		g_free (type);
	}
	if (props)
	{
		g_free (props);
	}

	return ret;
}

void
ephy_toolbar_item_register_type (const gchar *type, EphyTbItemConstructor constructor)
{
	ephy_tb_item_factory_init ();
	g_hash_table_insert (ephy_tb_item_known_types, g_strdup (type), constructor);
}
