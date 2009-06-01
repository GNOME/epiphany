/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2008 Gustavo Noronha Silva
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

#include "ephy-web-view.h"
#include "ephy-debug.h"
#include "ephy-embed-utils.h"

#include <gtk/gtk.h>
#include <webkit/webkit.h>

static void     ephy_web_view_class_init   (EphyWebViewClass *klass);
static void     ephy_web_view_init         (EphyWebView *gs);

G_DEFINE_TYPE (EphyWebView, ephy_web_view, WEBKIT_TYPE_WEB_VIEW)

static void
ephy_web_view_class_init (EphyWebViewClass *klass)
{
}

static void
ephy_web_view_init (EphyWebView *web_view)
{
}

/**
 * ephy_web_view_new:
 *
 * Equivalent to g_object_new() but returns an #GtkWidget so you don't have
 * to cast it when dealing with most code.
 *
 * Return value: the newly created #EphyWebView widget
 **/
GtkWidget *
ephy_web_view_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_WEB_VIEW, NULL));
}

/**
 * ephy_web_view_load_request:
 * @web_view: the #EphyWebView in which to load the request
 * @request: the #WebKitNetworkRequest to be loaded
 *
 * Loads the given #WebKitNetworkRequest in the given #EphyWebView.
 **/
void
ephy_web_view_load_request (EphyWebView *web_view,
                            WebKitNetworkRequest *request)
{
  WebKitWebFrame *main_frame;

	g_return_if_fail(EPHY_IS_WEB_VIEW(web_view));
	g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));

	main_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(web_view));
  webkit_web_frame_load_request(main_frame, request);
}

/**
 * ephy_web_view_copy_back_history:
 * @source: the #EphyWebView from which to get the back history
 * @dest: the #EphyWebView to copy the history to
 *
 * Sets the back history (up to the current item) of @source as the
 * back history of @dest.
 *
 * Useful to keep the history when opening links in new tabs or
 * windows.
 **/
void
ephy_web_view_copy_back_history (EphyWebView *source,
																 EphyWebView *dest)
{
  WebKitWebView *source_view, *dest_view;
  WebKitWebBackForwardList* source_bflist, *dest_bflist;
  WebKitWebHistoryItem *item;
  GList *items;

	g_return_if_fail(EPHY_IS_WEB_VIEW(source));
	g_return_if_fail(EPHY_IS_WEB_VIEW(dest));

  source_view = WEBKIT_WEB_VIEW (source);
  dest_view = WEBKIT_WEB_VIEW (dest);

  source_bflist = webkit_web_view_get_back_forward_list (source_view);
  dest_bflist = webkit_web_view_get_back_forward_list (dest_view);

	items = webkit_web_back_forward_list_get_back_list_with_limit (source_bflist, EPHY_WEBKIT_BACK_FORWARD_LIMIT);
	/* We want to add the items in the reverse order here, so the
		 history ends up the same */
	items = g_list_reverse (items);
	for (; items; items = items->next) {
		item = (WebKitWebHistoryItem*)items->data;
		webkit_web_back_forward_list_add_item (dest_bflist, g_object_ref (item));
	}
	g_list_free (items);

  /* The ephy/gecko behavior is to add the current item of the source
     embed at the end of the back history, so keep doing that */
  item = webkit_web_back_forward_list_get_current_item (source_bflist);
  webkit_web_back_forward_list_add_item (dest_bflist, g_object_ref (item));
}
