 /*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-embed-single.h"
#include "ephy-marshal.h"

static void ephy_embed_single_class_init (gpointer g_class);

GType
ephy_embed_single_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedSingleClass),
			ephy_embed_single_class_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyEmbedSingle",
					       &our_info,
					       (GTypeFlags) 0);
	}

	return type;
}

static void
ephy_embed_single_class_init (gpointer g_class)
{
	static gboolean initialised = FALSE;

	if (initialised == FALSE)
	{
	/**
	 * EphyEmbedSingle::handle_content
	 * @single: the #EphyEmbedSingle
	 * @mime_type: the mime type of the content
	 * @address: the address of the content
	 *
	 * The handle_content signal is emitted when encountering content
	 * of a mime type Epiphany is unable to handle itself.
	 *
	 * Returning TRUE will stop the signal emission.
	 **/
	g_signal_new ("handle_content",
		      EPHY_TYPE_EMBED_SINGLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedSingleClass, handle_content),
		      g_signal_accumulator_true_handled, NULL,
		      ephy_marshal_BOOLEAN__STRING_STRING,
		      G_TYPE_BOOLEAN,
		      2,
		      G_TYPE_STRING,
		      G_TYPE_STRING);

	initialised = TRUE;
	}
}

/**
 * ephy_embed_single_clear_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the mozilla cache.
 **/
void
ephy_embed_single_clear_cache (EphyEmbedSingle *single)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (single);
	klass->clear_cache (single);
}

/**
 * ephy_embed_single_clear_auth_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the mozilla http authentication cache.
 **/
void
ephy_embed_single_clear_auth_cache (EphyEmbedSingle *single)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (single);
	klass->clear_auth_cache (single);
}

/**
 * ephy_embed_single_set_offline_mode:
 * @single: the #EphyEmbedSingle
 * @offline: whether being off-line
 * 
 * Sets the state of the net connection.
 **/
void
ephy_embed_single_set_offline_mode (EphyEmbedSingle *single,
				    gboolean offline)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (single);
	klass->set_offline_mode (single, offline);
}

/**
 * ephy_embed_single_load_proxy_autoconf:
 * @single: the #EphyEmbedSingle
 * @address: the address of the PAC file
 * 
 * Sets the address of the PAC file, and loads the proxy configuration from it.
 **/
void
ephy_embed_single_load_proxy_autoconf (EphyEmbedSingle *single,
				       const char* address)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (single);
	klass->load_proxy_autoconf (single, address);
}

/**
 * ephy_embed_single_get_font_list:
 * @single: the #EphyEmbedSingle
 * @langGroup: a mozilla font language group name, or NULL
 * 
 * Returns the list of fonts matching @langGroup, or all fonts if @langGroup is
 * NULL.
 * 
 * Return value: a list of font names
 **/
GList *
ephy_embed_single_get_font_list (EphyEmbedSingle *single,
				 const char *langGroup)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (single);
	return klass->get_font_list (single, langGroup);
}
