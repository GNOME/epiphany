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

#include "ephy-embed.h"

#include "ephy-marshal.h"
#include "mozilla-embed-single.h"
#include "mozilla-embed.h"

static void ephy_embed_base_init (gpointer g_class);

GType
ephy_embed_get_type (void)
{
	static GType ephy_embed_type = 0;

	if (ephy_embed_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedIFace),
			ephy_embed_base_init,
			NULL,
		};

		ephy_embed_type = g_type_register_static (G_TYPE_INTERFACE,
							  "EphyEmbed",
							  &our_info,
							  (GTypeFlags)0);
	}

	return ephy_embed_type;
}

static void
ephy_embed_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
		g_signal_new ("ge_new_window",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, new_window),
			      NULL, NULL,
			      ephy_marshal_VOID__POINTER_INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_INT);
		g_signal_new ("ge_context_menu",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, context_menu),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
		g_signal_new ("ge_favicon",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, favicon),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
		g_signal_new ("ge_location",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, location),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
		g_signal_new ("ge_net_state",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, net_state),
			      NULL, NULL,
			      ephy_marshal_VOID__STRING_INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_INT);
		g_signal_new ("ge_dom_mouse_click",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, dom_mouse_click),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_POINTER);
		g_signal_new ("ge_dom_mouse_down",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, dom_mouse_down),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_POINTER);
		g_signal_new ("ge_popup_blocked",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, popup_blocked),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
		g_signal_new ("ge_security_change",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, security_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
		g_signal_new ("ge_zoom_change",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIFace, zoom_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_FLOAT);

		initialized = TRUE;
	}
}

void
ephy_embed_load_url (EphyEmbed *embed,
		     const char *url)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->load_url (embed, url);
}

void
ephy_embed_stop_load (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->stop_load (embed);
}

gboolean
ephy_embed_can_go_back (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_back (embed);
}

gboolean
ephy_embed_can_go_forward (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_forward (embed);
}

gboolean
ephy_embed_can_go_up (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_up (embed);
}

GSList *
ephy_embed_get_go_up_list (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_go_up_list (embed);
}

void
ephy_embed_go_back (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_back (embed);
}

void
ephy_embed_go_forward (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_forward (embed);
}

void
ephy_embed_go_up (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_up (embed);
}


char *
ephy_embed_get_title (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_title (embed);
}

char *
ephy_embed_get_location (EphyEmbed *embed,
			 gboolean toplevel)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_location (embed, toplevel);
}

char *
ephy_embed_get_link_message (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_link_message (embed);
}

char *
ephy_embed_get_js_status (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_js_status (embed);
}

void
ephy_embed_reload (EphyEmbed *embed,
		   EmbedReloadFlags flags)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->reload (embed, flags);
}

void
ephy_embed_zoom_set (EphyEmbed *embed,
		     float zoom,
		     gboolean reflow)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->zoom_set (embed, zoom, reflow);
}

float
ephy_embed_zoom_get (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->zoom_get (embed);
}

int
ephy_embed_shistory_n_items  (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->shistory_n_items (embed);
}

void
ephy_embed_shistory_get_nth (EphyEmbed *embed,
			     int nth,
			     gboolean is_relative,
			     char **url,
			     char **title)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->shistory_get_nth (embed, nth, is_relative, url, title);
}

int
ephy_embed_shistory_get_pos (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->shistory_get_pos (embed);
}

void
ephy_embed_shistory_go_nth (EphyEmbed *embed,
			    int nth)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->shistory_go_nth (embed, nth);
}

void
ephy_embed_get_security_level (EphyEmbed *embed,
			       EmbedSecurityLevel *level,
			       char **description)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->get_security_level (embed, level, description);
}

void
ephy_embed_find_set_properties  (EphyEmbed *embed,
				 char *search_string,
				 gboolean case_sensitive,
				 gboolean match_word)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->find_set_properties (embed, search_string, case_sensitive, match_word);
}

gboolean
ephy_embed_find_next (EphyEmbed *embed,
		      gboolean backwards)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->find_next (embed, backwards);
}

void
ephy_embed_activate (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->activate (embed);
}

void
ephy_embed_set_encoding (EphyEmbed *embed,
			 const char *encoding)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->set_encoding (embed, encoding);
}

EphyEncodingInfo *
ephy_embed_get_encoding_info (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_encoding_info (embed);
}

void
ephy_embed_print (EphyEmbed *embed,
		  EmbedPrintInfo *info)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->print (embed, info);
}

void
ephy_embed_print_preview_close (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->print_preview_close (embed);
}

int
ephy_embed_print_preview_n_pages (EphyEmbed *embed)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->print_preview_n_pages (embed);
}

void
ephy_embed_print_preview_navigate (EphyEmbed *embed,
				   EmbedPrintPreviewNavType type,
				   int page)
{
	EphyEmbedIFace *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->print_preview_navigate (embed, type, page);
}
