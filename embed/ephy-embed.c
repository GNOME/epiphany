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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-marshal.h"
#include "ephy-embed.h"
#include "mozilla-embed-single.h"

#include "mozilla-embed.h"

enum
{
	NEW_WINDOW,
	CONTEXT_MENU,
	LINK_MESSAGE,
	FAVICON,
	JS_STATUS,
	LOCATION,
	TITLE,
	PROGRESS,
	NET_STATE,
	VISIBILITY,
	DESTROY_BRSR,
	OPEN_URI,
	SIZE_TO,
	DOM_MOUSE_CLICK,
	DOM_MOUSE_DOWN,
	SECURITY_CHANGE,
	ZOOM_CHANGE,
	LAST_SIGNAL
};

static void
ephy_embed_base_init (gpointer base_class);

struct EphyEmbedPrivate
{
	gpointer dummy;
};

static guint ephy_embed_signals[LAST_SIGNAL] = { 0 };

GType
ephy_embed_get_type (void)
{
        static GType ephy_embed_type = 0;

        if (ephy_embed_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEmbedClass),
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
	ephy_embed_signals[NEW_WINDOW] =
                g_signal_new ("ge_new_window",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, new_window),
                              NULL, NULL,
                              ephy_marshal_VOID__POINTER_INT,
			      G_TYPE_NONE,
                              2,
                              G_TYPE_POINTER,
			      G_TYPE_INT);
	ephy_embed_signals[LINK_MESSAGE] =
                g_signal_new ("ge_link_message",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, link_message),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
	ephy_embed_signals[CONTEXT_MENU] =
                g_signal_new ("ge_context_menu",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, context_menu),
                              NULL, NULL,
                              ephy_marshal_INT__OBJECT,
                              G_TYPE_INT,
                              1,
			      G_TYPE_OBJECT);
	ephy_embed_signals[FAVICON] =
                g_signal_new ("ge_favicon",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, favicon),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
	ephy_embed_signals[JS_STATUS] =
                g_signal_new ("ge_js_status",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, js_status),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING,
			      G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
	ephy_embed_signals[LOCATION] =
                g_signal_new ("ge_location",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, location),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
	ephy_embed_signals[TITLE] =
                g_signal_new ("ge_title",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, title),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
	ephy_embed_signals[PROGRESS] =
                g_signal_new ("ge_progress",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, progress),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING_INT_INT,
                              G_TYPE_NONE,
                              3,
			      G_TYPE_STRING,
			      G_TYPE_INT,
			      G_TYPE_INT);
	ephy_embed_signals[NET_STATE] =
                g_signal_new ("ge_net_state",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, net_state),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING_INT,
                              G_TYPE_NONE,
                              2,
			      G_TYPE_STRING,
			      G_TYPE_INT);
	ephy_embed_signals[VISIBILITY] =
                g_signal_new ("ge_visibility",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, visibility),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_BOOLEAN);
	ephy_embed_signals[DESTROY_BRSR] =
                g_signal_new ("ge_destroy_brsr",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyEmbedClass, destroy_brsr),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
	ephy_embed_signals[OPEN_URI] =
                g_signal_new ("ge_open_uri",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, open_uri),
                              NULL, NULL,
                              ephy_marshal_INT__STRING,
                              G_TYPE_INT,
                              1,
			      G_TYPE_STRING);
	ephy_embed_signals[SIZE_TO] =
                g_signal_new ("ge_size_to",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, size_to),
                              NULL, NULL,
                              ephy_marshal_VOID__INT_INT,
                              G_TYPE_NONE,
                              2,
			      G_TYPE_INT,
			      G_TYPE_INT);
	ephy_embed_signals[DOM_MOUSE_CLICK] =
                g_signal_new ("ge_dom_mouse_click",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, dom_mouse_click),
                              NULL, NULL,
                              ephy_marshal_INT__OBJECT,
                              G_TYPE_INT,
                              1,
			      G_TYPE_POINTER);
	ephy_embed_signals[DOM_MOUSE_DOWN] =
                g_signal_new ("ge_dom_mouse_down",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, dom_mouse_down),
                              NULL, NULL,
                              ephy_marshal_INT__OBJECT,
                              G_TYPE_INT,
                              1,
			      G_TYPE_POINTER);
	ephy_embed_signals[SECURITY_CHANGE] =
                g_signal_new ("ge_security_change",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, security_change),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_INT);
	ephy_embed_signals[ZOOM_CHANGE] =
                g_signal_new ("ge_zoom_change",
                              EPHY_TYPE_EMBED,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedClass, zoom_change),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__FLOAT,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_FLOAT);
	initialized = TRUE;
	}
}

EphyEmbed *
ephy_embed_new (GObject *single)
{
	if (MOZILLA_IS_EMBED_SINGLE (single))
	{
		return EPHY_EMBED (g_object_new
			(MOZILLA_TYPE_EMBED, NULL));
	}

	g_assert_not_reached ();

	return NULL;
}

void
ephy_embed_get_capabilities (EphyEmbed *embed,
                               EmbedCapabilities *caps)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        klass->get_capabilities (embed, caps);
}

gresult
ephy_embed_load_url (EphyEmbed *embed,
                     const char *url)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->load_url (embed, url);
}

gresult
ephy_embed_stop_load (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->stop_load (embed);
}

gresult
ephy_embed_can_go_back (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->can_go_back (embed);
}

gresult
ephy_embed_can_go_forward (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->can_go_forward (embed);
}

gresult
ephy_embed_can_go_up (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->can_go_up (embed);
}

gresult
ephy_embed_get_go_up_list (EphyEmbed *embed, GSList **l)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->get_go_up_list (embed, l);
}

gresult
ephy_embed_go_back (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->go_back (embed);
}

gresult
ephy_embed_go_forward (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->go_forward (embed);
}

gresult
ephy_embed_go_up (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->go_up (embed);
}

gresult
ephy_embed_render_data (EphyEmbed *embed,
                        const char *data,
                        guint32 len,
                        const char *base_uri,
                        const char *mime_type)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->render_data (embed, data, len, base_uri, mime_type);
}

gresult
ephy_embed_open_stream (EphyEmbed *embed,
                        const char *base_uri,
                        const char *mime_type)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->open_stream (embed, base_uri, mime_type);
}

gresult
ephy_embed_append_data (EphyEmbed *embed,
                        const char *data,
                        guint32 len)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->append_data (embed, data, len);
}

gresult
ephy_embed_close_stream (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->close_stream (embed);
}

gresult
ephy_embed_get_title (EphyEmbed *embed,
                      char **title)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->get_title (embed, title);
}

gresult
ephy_embed_get_location (EphyEmbed *embed,
                         gboolean toplevel,
                         char **location)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->get_location (embed, toplevel, location);
}

gresult
ephy_embed_reload (EphyEmbed *embed,
                   EmbedReloadFlags flags)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->reload (embed, flags);
}

gresult
ephy_embed_copy_page (EphyEmbed *dest,
		      EphyEmbed *source,
		      EmbedDisplayType display_type)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (dest);
	return klass->copy_page (dest, source, display_type);
}

gresult
ephy_embed_zoom_set (EphyEmbed *embed,
                     float zoom,
                     gboolean reflow)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->zoom_set (embed, zoom, reflow);
}

gresult
ephy_embed_zoom_get (EphyEmbed *embed,
                     float *zoom)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->zoom_get (embed, zoom);
}

gresult
ephy_embed_shistory_count  (EphyEmbed *embed,
                            int *count)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->shistory_count (embed, count);
}

gresult
ephy_embed_shistory_get_nth (EphyEmbed *embed,
                             int nth,
                             gboolean is_relative,
                             char **url,
                             char **title)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->shistory_get_nth (embed, nth, is_relative, url, title);
}

gresult
ephy_embed_shistory_get_pos (EphyEmbed *embed,
                             int *pos)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->shistory_get_pos (embed, pos);
}

gresult
ephy_embed_shistory_go_nth (EphyEmbed *embed,
                            int nth)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->shistory_go_nth (embed, nth);
}

gresult
ephy_embed_get_security_level (EphyEmbed *embed,
                               EmbedSecurityLevel *level,
                               char **description)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->get_security_level (embed, level, description);
}

gresult
ephy_embed_find_set_properties  (EphyEmbed *embed,
				 char *search_string,
				 gboolean case_sensitive,
				 gboolean match_word)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->find_set_properties (embed, search_string, case_sensitive, match_word);
}

gresult
ephy_embed_find_next (EphyEmbed *embed,
		      gboolean backwards)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->find_next (embed, backwards);
}

gresult
ephy_embed_activate (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->activate (embed);
}

gresult
ephy_embed_set_encoding (EphyEmbed *embed,
			 const char *encoding)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
	return klass->set_encoding (embed, encoding);
}

gresult
ephy_embed_get_encoding_info (EphyEmbed *embed,
			      EphyEncodingInfo **info)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
	return klass->get_encoding_info (embed, info);
}

gresult
ephy_embed_print (EphyEmbed *embed,
                  EmbedPrintInfo *info)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->print (embed, info);
}

gresult
ephy_embed_print_preview_close (EphyEmbed *embed)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
        return klass->print_preview_close (embed);
}

gresult
ephy_embed_print_preview_num_pages (EphyEmbed *embed, gint *retNum)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
	return klass->print_preview_num_pages (embed, retNum);
}

gresult
ephy_embed_print_preview_navigate (EphyEmbed *embed,
				   EmbedPrintPreviewNavType navType,
				   gint pageNum)
{
	EphyEmbedClass *klass = EPHY_EMBED_GET_CLASS (embed);
	return klass->print_preview_navigate (embed, navType, pageNum);
}

