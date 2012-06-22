/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "ephy-request-about.h"

#include "ephy-about-handler.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libsoup/soup-uri.h>

G_DEFINE_TYPE (EphyRequestAbout, ephy_request_about, SOUP_TYPE_REQUEST)

struct _EphyRequestAboutPrivate {
  gssize content_length;
};

static void
ephy_request_about_init (EphyRequestAbout *about)
{
  about->priv = G_TYPE_INSTANCE_GET_PRIVATE (about, EPHY_TYPE_REQUEST_ABOUT, EphyRequestAboutPrivate);
}

static gboolean
ephy_request_about_check_uri (SoupRequest  *request,
                              SoupURI      *uri,
                              GError      **error)
{
  return uri->host == NULL;
}

static GInputStream *
ephy_request_about_send (SoupRequest          *request,
                         GCancellable         *cancellable,
                         GError              **error)
{
  EphyRequestAbout *about = EPHY_REQUEST_ABOUT (request);
  SoupURI *uri = soup_request_get_uri (request);
  GString *data_str = ephy_about_handler_handle (uri->path);

  about->priv->content_length = data_str->len;
  return g_memory_input_stream_new_from_data (g_string_free (data_str, FALSE), about->priv->content_length, g_free);
}

static goffset
ephy_request_about_get_content_length (SoupRequest *request)
{
  return  EPHY_REQUEST_ABOUT (request)->priv->content_length;
}

static const char *
ephy_request_about_get_content_type (SoupRequest *request)
{
  return "text/html";
}

static const char *about_schemes[] = { EPHY_ABOUT_SCHEME, NULL };

static void
ephy_request_about_class_init (EphyRequestAboutClass *request_about_class)
{
  SoupRequestClass *request_class = SOUP_REQUEST_CLASS (request_about_class);

  request_class->schemes = about_schemes;
  request_class->check_uri = ephy_request_about_check_uri;
  request_class->send = ephy_request_about_send;
  request_class->get_content_length = ephy_request_about_get_content_length;
  request_class->get_content_type = ephy_request_about_get_content_type;

  g_type_class_add_private (request_about_class, sizeof (EphyRequestAboutPrivate));
}
