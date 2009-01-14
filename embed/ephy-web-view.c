/* -*- Mode: C; tab-width: 2; indent-tabs-mode: f; c-basic-offset: 2 -*- */
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
