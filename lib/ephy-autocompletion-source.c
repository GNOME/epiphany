/* 
 *  Copyright (C) 2002  Ricardo Fernándezs Pascual <ric@users.sourceforge.net>
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

#include "ephy-autocompletion-source.h"
#include "ephy-marshal.h"

static void		ephy_autocompletion_source_base_init		(gpointer g_class);

GType
ephy_autocompletion_source_get_type (void)
{
	static GType autocompletion_source_type = 0;

	if (! autocompletion_source_type)
	{
		static const GTypeInfo autocompletion_source_info =
		{
			sizeof (EphyAutocompletionSourceIface),		/* class_size */
			ephy_autocompletion_source_base_init,		/* base_init */
			NULL,						/* base_finalize */
			NULL,
			NULL,						/* class_finalize */
			NULL,						/* class_data */
			0,
			0,						/* n_preallocs */
			NULL
		};

		autocompletion_source_type = g_type_register_static
			(G_TYPE_INTERFACE, "EphyAutocompletionSource", &autocompletion_source_info, 0);
		g_type_interface_add_prerequisite (autocompletion_source_type, G_TYPE_OBJECT);
	}

	return autocompletion_source_type;
}

static void
ephy_autocompletion_source_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
		g_signal_new ("data-changed",
			      EPHY_TYPE_AUTOCOMPLETION_SOURCE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyAutocompletionSourceIface, data_changed),
			      NULL, NULL,
			      ephy_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
		initialized = TRUE;
	}
}


void
ephy_autocompletion_source_foreach (EphyAutocompletionSource *source,
				    const gchar *basic_key,
				    EphyAutocompletionSourceForeachFunc func,
				    gpointer data)
{
	(* EPHY_AUTOCOMPLETION_SOURCE_GET_IFACE (source)->foreach) (source, basic_key, func, data);
}

void
ephy_autocompletion_source_set_basic_key (EphyAutocompletionSource *source,
					  const gchar *basic_key)
{
	(* EPHY_AUTOCOMPLETION_SOURCE_GET_IFACE (source)->set_basic_key) (source, basic_key);
}
