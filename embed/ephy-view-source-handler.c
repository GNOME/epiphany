/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-view-source-handler.h"

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-web-view.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

struct _EphyViewSourceHandler {
  EphySourceTransformHandler parent_instance;
};

G_DEFINE_TYPE (EphyViewSourceHandler, ephy_view_source_handler, EPHY_TYPE_SOURCE_TRANSFORM_HANDLER)

static guchar *
ephy_view_source_handler_transform_source (EphySourceTransformHandler *handler,
                                           const guchar               *data,
                                           gsize                       length)
{
  g_autofree char *escaped_str = NULL;
  char *html;

  /* Warning: data is not a string, so we pass length here because it's not NUL-terminated. */
  escaped_str = g_markup_escape_text ((const char *)data, length);

  html = g_strdup_printf ("<head>"
                          "  <link rel='stylesheet' href='ephy-resource:///org/gnome/epiphany/highlightjs/default.css'>"
                          "  <title>%s</title>"
                          "</head>"
                          "<body class='hljs'>"
                          "  <script src='ephy-resource:///org/gnome/epiphany/highlightjs/highlight.js'></script>"
                          "  <script>hljs.initHighlightingOnLoad();</script>"
                          "  <pre><code class='html'>%s</code></pre>"
                          "</body>",
                          ephy_source_transform_handler_get_uri (handler),
                          escaped_str);

  return (guchar *)html;
}

static void
ephy_view_source_handler_init (EphyViewSourceHandler *handler)
{
}

static void
ephy_view_source_handler_class_init (EphyViewSourceHandlerClass *klass)
{
  EphySourceTransformHandlerClass *handler_class = EPHY_SOURCE_TRANSFORM_HANDLER_CLASS (klass);

  handler_class->transform_source = ephy_view_source_handler_transform_source;
}

EphyViewSourceHandler *
ephy_view_source_handler_new (void)
{
  return EPHY_VIEW_SOURCE_HANDLER (g_object_new (EPHY_TYPE_VIEW_SOURCE_HANDLER, NULL));
}
