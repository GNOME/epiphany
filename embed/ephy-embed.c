/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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
 *  $Id$
 */

#include "config.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ephy-embed.h"

#include "ephy-embed-type-builtins.h"
#include "ephy-marshal.h"

static void ephy_embed_base_init (gpointer g_class);

GType
ephy_embed_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyEmbedIface),
			ephy_embed_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyEmbed",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

static void
ephy_embed_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
/**
 * EphyEmbed::ge-new-window:
 * @embed:
 * @new_embed: the newly opened #EphyEmbed
 *
 * The ::ge_new_window signal is emitted after a new window has been opened by
 * the embed. For example, when a JavaScript popup window is opened.
 **/
		g_signal_new ("ge_new_window",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, new_window),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
/**
 * EphyEmbed::ge-popup-blocked:
 * @embed:
 * @address: The requested URL
 * @target: The requested window name, e.g. "_blank"
 * @features: The requested features: for example, "height=400,width=200"
 *
 * The ::ge_popup_blocked signal is emitted when the viewed web page requests
 * a popup window (with javascript:open()) but popup windows are not allowed.
 **/
		g_signal_new ("ge_popup_blocked",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, popup_blocked),
			      NULL, NULL,
			      ephy_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * EphyEmbed::ge-context-menu:
 * @embed:
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_context_menu signal is emitted when a context menu is to be
 * displayed. This will usually happen when the user right-clicks on a part of
 * @embed.
 **/
		g_signal_new ("ge_context_menu",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, context_menu),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
/**
 * EphyEmbed::ge-favicon:
 * @embed:
 * @address: the URL to @embed's web site's favicon
 *
 * The ::ge_favicon signal is emitted when @embed discovers that a favourite
 * icon (favicon) is available for the site it is visiting.
 **/
		g_signal_new ("ge_favicon",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, favicon),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * EphyEmbed::ge-search-link:
 * @embed:
 * @type: the mime-type of the search description
 * @title: the title of the news feed
 * @address: the URL to @embed's web site's search description
 *
 * The ::ge_rss signal is emitted when @embed discovers that a search
 * description is available for the site it is visiting.
 **/
		g_signal_new ("ge_search_link",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, search_link),
			      NULL, NULL,
			      ephy_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyEmbed::ge-feed-link:
 * @embed:
 * @type: the mime-type of the news feed
 * @title: the title of the news feed
 * @address: the URL to @embed's web site's news feed
 *
 * The ::ge_rss signal is emitted when @embed discovers that a news feed
 * is available for the site it is visiting.
 **/
		g_signal_new ("ge_feed_link",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, feed_link),
			      NULL, NULL,
			      ephy_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * EphyEmbed::ge-dom-mouse-click:
 * @embed:
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_dom_mouse_click signal is emitted when the user clicks in @embed.
 **/
		g_signal_new ("ge_dom_mouse_click",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, dom_mouse_click),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
/**
 * EphyEmbed::ge-dom-mouse-down:
 * @embed:
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_dom_mouse_down signal is emitted when the user depresses a mouse
 * button.
 **/
		g_signal_new ("ge_dom_mouse_down",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, dom_mouse_down),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
/**
 * EphyEmbed::ge-zoom-change:
 * @embed:
 * @zoom: @embed's new zoom level
 *
 * The ::ge_zoom_change signal is emitted when @embed's zoom changes. This can
 * be manual (the user modified the zoom level) or automatic (@embed's zoom is
 * automatically changed when browsing to a new site for which the user
 * previously specified a zoom level).
 *
 * A @zoom value of 1.0 indicates 100% (normal zoom).
 **/
		g_signal_new ("ge_zoom_change",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, zoom_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_FLOAT);
/**
 * EphyEmbed::ge-modal-alert:
 * @embed:
 *
 * The ::ge-modal-alert signal is emitted when a DOM event will open a
 * modal alert.
 *
 * Return %TRUE to prevent the dialog from being opened.
 **/
		g_signal_new ("ge_modal_alert",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, modal_alert),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);
/**
 * EphyEmbed::ge-modal-alert-closed:
 * @embed:
 *
 * The ::ge-modal-alert-closed signal is emitted when a modal alert put up by a
 * DOM event was closed.
 **/
		g_signal_new ("ge_modal_alert_closed",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, modal_alert_closed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

/**
 * EphyEmbed::ge-document-type:
 * @embed:
 * @type: the new document type
 *
 * The ::ge-document-type signal is emitted when @embed determines the type of its document.
 **/
		g_signal_new ("ge_document_type",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, document_type),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_EMBED_DOCUMENT_TYPE);
/**
 * EphyEmbed::dom-content-loaded:
 * @embed:
 *
 * The ::dom-content-loaded signal is emitted when 
 * the document has been loaded (excluding images and other loads initiated by this document).
 * That's true also for frameset and all the frames within it.
 **/
		g_signal_new ("dom_content_loaded",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, dom_content_loaded),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

/**
 * EphyEmbed::ge-search-key-press:
 * @embed:
 * @event: the #GdkEventKey which triggered this signal
 *
 * The ::ge-search-key-press signal is emitted for keypresses which
 * should be used for find implementations.
 **/
		g_signal_new ("ge-search-key-press",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, search_key_press),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__BOXED,
			      G_TYPE_BOOLEAN,
			      1,
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyEmbed::close-request
 * @embed:
 *
 * The ::close signal is emitted when the embed request closing.
 * Return %TRUE to prevent closing. You HAVE to process removal of the embed
 * as soon as possible after that.
 **/
		g_signal_new ("close-request",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, close_request),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__VOID,
			      G_TYPE_BOOLEAN,
			      0);
/**
 * EphyEmbed::content-blocked:
 * @embed:
 * @uri: blocked URI 
 *
 * The ::content-blocked signal is emitted when an url has been blocked.
 **/
		g_signal_new ("content-blocked",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, content_blocked),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

		initialized = TRUE;
	}

}

/**
 * ephy_embed_load_url:
 * @embed: an #EphyEmbed
 * @url: a URL
 *
 * Loads a new web page in @embed.
 **/
void
ephy_embed_load_url (EphyEmbed *embed,
		     const char *url)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->load_url (embed, url);
}

/**
 * ephy_embed_load:
 * @embed: an #EphyEmbed
 * @url: an URL
 * @flags: flags modifying load behaviour
 * @previous_embed: the referrer embed or %NULL
 *
 * Loads a new web page in @embed.
 **/
void
ephy_embed_load (EphyEmbed *embed,
		 const char *url,
		 EphyEmbedLoadFlags flags,
		 EphyEmbed *referring_embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->load (embed, url, flags, referring_embed);
}

/**
 * ephy_embed_stop_load:
 * @embed: an #EphyEmbed
 *
 * If @embed is loading, stops it from continuing.
 **/
void
ephy_embed_stop_load (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->stop_load (embed);
}

/**
 * ephy_embed_can_go_back:
 * @embed: an #EphyEmbed
 *
 * Return value: %TRUE if @embed can return to a previously-visited location
 **/
gboolean
ephy_embed_can_go_back (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_back (embed);
}

/**
 * ephy_embed_can_go_forward:
 * @embed: an #EphyEmbed
 *
 * Return value: %TRUE if @embed has gone back, and can thus go forward again
 **/
gboolean
ephy_embed_can_go_forward (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_forward (embed);
}

/**
 * ephy_embed_can_go_up:
 * @embed: an #EphyEmbed
 *
 * Returns whether @embed can travel to a higher-level directory on the server.
 * For example, for http://www.example.com/subdir/index.html, returns %TRUE; for
 * http://www.example.com/index.html, returns %FALSE.
 *
 * Return value: %TRUE if @embed can browse to a higher-level directory
 **/
gboolean
ephy_embed_can_go_up (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_up (embed);
}

/**
 * ephy_embed_get_go_up_list:
 * @embed: an #EphyEmbed
 *
 * Returns a list of (%char *) URLs to higher-level directories on the same
 * server, in order of deepest to shallowest. For example, given
 * "http://www.example.com/dir/subdir/file.html", will return a list containing
 * "http://www.example.com/dir/subdir/", "http://www.example.com/dir/" and
 * "http://www.example.com/".
 *
 * Return value: a list of URLs higher up in @embed's web page's directory
 * hierarchy
 **/
GSList *
ephy_embed_get_go_up_list (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_go_up_list (embed);
}

/**
 * ephy_embed_go_back:
 * @embed: an #EphyEmbed
 *
 * Causes @embed to return to the previously-visited web page.
 **/
void
ephy_embed_go_back (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_back (embed);
}

/**
 * ephy_embed_go_forward:
 * @embed: an #EphyEmbed
 *
 * If @embed has returned to a previously-visited web page, proceed forward to
 * the next page.
 **/
void
ephy_embed_go_forward (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_forward (embed);
}

/**
 * ephy_embed_go_up:
 * @embed: an #EphyEmbed
 *
 * Moves @embed one level up in its web page's directory hierarchy.
 **/
void
ephy_embed_go_up (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_up (embed);
}

/**
 * ephy_embed_get_title:
 * @embed: an #EphyEmbed
 *
 * Return value: the title of the web page displayed in @embed
 **/
const char *
ephy_embed_get_title (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_title (embed);
}

/**
 * ephy_embed_get_location:
 * @embed: an #EphyEmbed
 * @toplevel: %FALSE to return the location of the focused frame only
 *
 * Returns the URL of the web page displayed in @embed.
 *
 * If the web page contains frames, @toplevel will determine which location to
 * retrieve. If @toplevel is %TRUE, the return value will be the location of the
 * frameset document. If @toplevel is %FALSE, the return value will be the
 * location of the currently-focused frame.
 *
 * Return value: the URL of the web page displayed in @embed
 **/
char *
ephy_embed_get_location (EphyEmbed *embed,
			 gboolean toplevel)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_location (embed, toplevel);
}

/**
 * ephy_embed_get_link_message:
 * @embed: an #EphyEmbed
 *
 * When the user is hovering the mouse over a hyperlink, returns the URL of the
 * hyperlink.
 *
 * Return value: the URL of the link over which the mouse is hovering
 **/
const char *
ephy_embed_get_link_message (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_link_message (embed);
}

/**
 * ephy_embed_get_js_status:
 * @embed: an #EphyEmbed
 *
 * Displays the message JavaScript is attempting to display in the statusbar.
 *
 * Note that Epiphany does not display JavaScript statusbar messages.
 *
 * Return value: a message from JavaScript meant to be displayed in the
 *		 statusbar
 **/
char *
ephy_embed_get_js_status (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_js_status (embed);
}

/**
 * ephy_embed_reload:
 * @embed: an #EphyEmbed
 * @force: %TRUE to bypass cache
 *
 * Reloads the web page being displayed in @embed.
 *
 * If @force is %TRUE, cache and proxy will be bypassed when
 * reloading the page.
 **/
void
ephy_embed_reload (EphyEmbed *embed,
		   gboolean force)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->reload (embed, force);
}

/**
 * ephy_embed_set_zoom:
 * @embed: an #EphyEmbed
 * @zoom: the new zoom level
 *
 * Sets the zoom level for a web page.
 *
 * Zoom is normally controlled by the Epiphany itself and remembered in
 * Epiphany's history data. Be very careful not to break this behavior if using
 * this function; better yet, don't use this function at all.
 **/
void
ephy_embed_set_zoom (EphyEmbed *embed,
		     float zoom)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->set_zoom (embed, zoom);
}

/**
 * ephy_embed_get_zoom:
 * @embed: an #EphyEmbed
 *
 * Returns the zoom level of @embed. A zoom of 1.0 corresponds to 100% (normal
 * size).
 *
 * Return value: the zoom level of @embed
 **/
float
ephy_embed_get_zoom (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_zoom (embed);
}

/**
 * ephy_embed_scroll:
 * @embed: an #EphyEmbed
 * @num_lines: The number of lines to scroll by
 *
 * Scrolls the view by lines. Positive numbers scroll down, negative
 * numbers scroll up
 *
 **/
void
ephy_embed_scroll (EphyEmbed *embed,
		   int num_lines)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->scroll_lines (embed, num_lines);
}

/**
 * ephy_embed_page_scroll:
 * @embed: an #EphyEmbed
 * @num_lines: The number of pages to scroll by
 *
 * Scrolls the view by pages. Positive numbers scroll down, negative
 * numbers scroll up
 *
 **/
void
ephy_embed_page_scroll (EphyEmbed *embed,
			int num_pages)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->scroll_pages (embed, num_pages);
}

/**
 * ephy_embed_scroll_pixels:
 * @embed: an #EphyEmbed
 * @dx: the number of pixels to scroll in X direction
 * @dy: the number of pixels to scroll in Y direction
 *
 * Scrolls the view by pixels. Positive numbers scroll down resp. right,
 * negative numbers scroll up resp. left.
 *
 **/
void
ephy_embed_scroll_pixels (EphyEmbed *embed,
			  int dx,
			  int dy)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->scroll_pixels (embed, dx, dy);
}

/**
 * ephy_embed_shistory_copy:
 * @source: the #EphyEmbed to copy the history from
 * @dest: the #EphyEmbed to copy the history to
 * @copy_back: %TRUE to copy the back history
 * @copy_forward: %TRUE to copy the forward history
 * @copy_current: %TRUE to set the current page to that in the copied history
 *
 * Copy's the back and/or forward history from @source to @dest,
 * and optionally set @dest to the current page of @source as well.
 **/
void
ephy_embed_shistory_copy (EphyEmbed *source,
			  EphyEmbed *dest,
			  gboolean copy_back,
			  gboolean copy_forward,
			  gboolean copy_current)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (source);
	iface->shistory_copy (source, dest, copy_back, copy_forward, copy_current);
}

/**
 * ephy_embed_get_security_level:
 * @embed: an #EphyEmbed
 * @level: return value of security level
 * @description: return value of the description of the security level
 *
 * Fetches the #EphyEmbedSecurityLevel and a newly-allocated string description
 * of the security state of @embed.
 **/
void
ephy_embed_get_security_level (EphyEmbed *embed,
			       EphyEmbedSecurityLevel *level,
			       char **description)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->get_security_level (embed, level, description);
}
/**
 * ephy_embed_show_page_certificate:
 * @embed: an #EphyEmbed
 *
 * Shows a dialogue displaying the certificate of the currently loaded page
 * of @embed, if it was loaded over a secure connection; else does nothing.
 **/
void
ephy_embed_show_page_certificate (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->show_page_certificate (embed);
}

/**
 * ephy_embed_close:
 * @embed: an #EphyEmbed
 *
 * Closes the @embed
 **/
void
ephy_embed_close (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->close (embed);
}

/**
 * ephy_embed_set_encoding:
 * @embed: an #EphyEmbed
 * @encoding: the desired encoding
 *
 * Sets @embed's character encoding to @encoding. These cryptic encoding
 * strings are listed in <filename>embed/ephy-encodings.c</filename>.
 *
 * Pass an empty string (not NULL) in @encoding to reset @embed to use the
 * document-specified encoding.
 **/
void
ephy_embed_set_encoding (EphyEmbed *embed,
			 const char *encoding)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->set_encoding (embed, encoding);
}

/**
 * ephy_embed_get_encoding:
 * @embed: an #EphyEmbed
 *
 * Returns the @embed's document's encoding
 **/
char *
ephy_embed_get_encoding (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_encoding (embed);
}

/**
 * ephy_embed_has_automatic_encoding:
 * @embed: an #EphyEmbed
 *
 * Returns whether the @embed's document's was determined by the document itself
 **/
gboolean
ephy_embed_has_automatic_encoding (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->has_automatic_encoding (embed);
}

/**
 * ephy_embed_print:
 * @embed: an #EphyEmbed
 *
 * Sends a document to the printer.
 *
 **/
void
ephy_embed_print (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->print (embed);
}

/**
 * ephy_embed_set_print_preview_mode:
 * @embed: an #EphyEmbed
 * @preview_mode: Whether the print preview mode is enabled.
 *
 * Enable and disable the print preview mode.
 **/
void
ephy_embed_set_print_preview_mode (EphyEmbed *embed,
				   gboolean preview_mode)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->set_print_preview_mode (embed, preview_mode);
}

/**
 * ephy_embed_print_preview_n_pages:
 * @embed: an #EphyEmbed
 *
 * Returns the number of pages which would appear in @embed's loaded document
 * if it were to be printed.
 *
 * Return value: the number of pages in @embed's loaded document
 **/
int
ephy_embed_print_preview_n_pages (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->print_preview_n_pages (embed);
}

/**
 * ephy_embed_print_preview_navigate:
 * @embed: an #EphyEmbed
 * @type: an #EphyPrintPreviewNavType which determines where to navigate
 * @page: if @type is %EPHY_EMBED_PRINTPREVIEW_GOTO_PAGENUM, the desired page number
 *
 * Navigates @embed's print preview.
 **/
void
ephy_embed_print_preview_navigate (EphyEmbed *embed,
				   EphyEmbedPrintPreviewNavType type,
				   int page)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->print_preview_navigate (embed, type, page);
}

/**
 * ephy_embed_has_modified_forms:
 * @embed: an #EphyEmbed
 *
 * Returns %TRUE if the user has modified &lt;input&gt; or &lt;textarea&gt;
 * values in @embed's loaded document.
 *
 * Return value: %TRUE if @embed has user-modified forms
 **/
gboolean
ephy_embed_has_modified_forms (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->has_modified_forms (embed);
}

/**
 * ephy_embed_get_document_type:
 * @embed: an #EphyEmbed
 *
 * Returns the type of document loaded in the @embed
 *
 * Return value: the #EphyEmbedDocumentType
 **/
EphyEmbedDocumentType
ephy_embed_get_document_type (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_document_type (embed);
}

/**
 * ephy_embed_get_load_percent:
 * @embed: an #EphyEmbed
 *
 * Returns the page load percentage (displayed in the progressbar).
 *
 * Return value: a percentage from 0 to 100.
 **/
int
ephy_embed_get_load_percent (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_load_percent (embed);
}

/**
 * ephy_embed_get_load_status:
 * @embed: an #EphyEmbed
 *
 * Returns whether the web page in @embed has finished loading. A web page is
 * only finished loading after all images, styles, and other dependencies have
 * been downloaded and rendered.
 *
 * Return value: %TRUE if the page is still loading, %FALSE if complete
 **/
gboolean
ephy_embed_get_load_status (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_load_status (embed);
}

/**
 * ephy_embed_get_navigation_flags:
 * @embed: an #EphyEmbed
 *
 * Returns @embed's navigation flags.
 *
 * Return value: @embed's navigation flags
 **/
EphyEmbedNavigationFlags
ephy_embed_get_navigation_flags (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_navigation_flags (embed);
}

/**
 * ephy_embed_get_typed_address:
 * @embed: an #EphyEmbed
 *
 * Returns the text that @embed's #EphyWindow will display in its location toolbar
 * entry when @embed is selected.
 *
 * This is not guaranteed to be the same as @embed's location,
 * available through ephy_embed_get_location(). As the user types a new address
 * into the location entry, ephy_embed_get_location()'s returned string will
 * change.
 *
 * Return value: @embed's #EphyWindow's location entry when @embed is selected
 **/
const char *
ephy_embed_get_typed_address (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_typed_address (embed);
}

/**
 * ephy_embed_set_typed_address:
 * @embed: an #EphyEmbed
 * @address: the new typed address, or %NULL to clear it
 * @expire: when to expire this address_expire
 * 
 * Sets the text that @embed's #EphyWindow will display in its location toolbar
 * entry when @embed is selected.
 **/
void
ephy_embed_set_typed_address (EphyEmbed *embed,
			      const char *address,
			      EphyEmbedAddressExpire expire)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->set_typed_address (embed, address, expire);
}

/**
 * ephy_embed_get_address:
 * @embed: an #EphyEmbed
 *
 * Returns the address of the currently loaded page.
 *
 * Return value: @embed's address. Will never be %NULL.
 **/
const char *
ephy_embed_get_address (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_address (embed);
}

/**
 * ephy_embed_get_status_message:
 * @embed: an #EphyEmbed
 *
 * Returns the message displayed in @embed's #EphyWindow's
 * #EphyStatusbar. If the user is hovering the mouse over a hyperlink,
 * this function will return the same value as
 * ephy_embed_get_link_message(). Otherwise, it will return a network
 * status message, or NULL.
 *
 * The message returned has a limited lifetime, and so should be copied with
 * g_strdup() if it must be stored.
 *
 * Return value: The current statusbar message
 **/
const char *
ephy_embed_get_status_message (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_status_message (embed);
}

/**
 * ephy_embed_get_icon:
 * @embed: an #EphyEmbed
 *
 * Returns the embed's site icon as a #GdkPixbuf,
 * or %NULL if it is not available.
 *
 * Return value: a the embed's site icon
 **/
GdkPixbuf *
ephy_embed_get_icon (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_icon (embed);
}

/**
 * ephy_embed_get_icon_address:
 * @embed: an #EphyEmbed
 *
 * Returns a URL which points to @embed's site icon.
 *
 * Return value: the URL of @embed's site icon
 **/
const char *
ephy_embed_get_icon_address (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_icon_address (embed);
}

/**
 * ephy_embed_get_is_blank:
 * @embed: an #EphyEmbed
 *
 * Returns whether the	@embed's address is "blank".
 *
 * Return value: %TRUE if the @embed's address is "blank"
 **/
gboolean
ephy_embed_get_is_blank (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_is_blank (embed);
}

const char *
ephy_embed_get_loading_title (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_loading_title (embed);
}

/**
 * ephy_embed_get_visibility:
 * @embed: an #EphyEmbed
 *
 * Returns whether the @embed's toplevel is visible or not. Used
 * mostly for popup visibility management.
 *
 * Return value: %TRUE if @embed's "visibility" property is set
 **/
gboolean
ephy_embed_get_visibility (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_visibility (embed);
}

/**
 * ephy_embed_get_backward_history:
 * @embed: an #EphyEmbed
 *
 * Returns a #GList of #EphyHistoryItem compromising the
 * history items preceding the current location.
 *
 * Return value: a #GList with the preceding history items
 **/
GList*
ephy_embed_get_backward_history (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_backward_history (embed);
}

/**
 * ephy_embed_get_forward_history:
 * @embed: an #EphyEmbed
 *
 * Returns a #GList of #EphyHistoryItem compromising the
 * history items succeeding the current location.
 *
 * Return value: a #GList with the succeeding history items
 **/
GList*
ephy_embed_get_forward_history (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_forward_history (embed);
}

/**
 * ephy_embed_get_previous_history_item:
 * @embed: an #EphyEmbed
 *
 * Returns the preceding #EphyHistoryItem in the history list.
 *
 * Return value: the preceding #EphyHistoryItem
 **/
EphyHistoryItem*
ephy_embed_get_previous_history_item (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_previous_history_item (embed);
}

/**
 * ephy_embed_get_next_history_item:
 * @embed: an #EphyEmbed
 *
 * Returns the succeeding #EphyHistoryItem in the history list.
 *
 * Return value: the succeeding #EphyHistoryItem
 **/
EphyHistoryItem*
ephy_embed_get_next_history_item (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_next_history_item (embed);
}

/**
 * ephy_embed_go_to_history_item:
 * @embed: an #EphyEmbed
 * @history_item: an #EphyHistoryItem
 *
 * Opens the webpage specified by @history_item in @embed's history.
 *
 **/
void
ephy_embed_go_to_history_item (EphyEmbed *embed, EphyHistoryItem *history_item)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_to_history_item (embed, history_item);
}
