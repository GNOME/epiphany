/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016, 2018 Igalia S.L.
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

#pragma once

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SOURCE_TRANSFORM_HANDLER (ephy_source_transform_handler_get_type ())

G_DECLARE_DERIVABLE_TYPE (EphySourceTransformHandler, ephy_source_transform_handler, EPHY, SOURCE_TRANSFORM_HANDLER, GObject)

typedef struct {
  EphySourceTransformHandler *transform_handler;
  WebKitURISchemeRequest *scheme_request;
  WebKitWebView *web_view;
  GCancellable *cancellable;
  guint load_changed_id;
} EphySourceTransformRequest;

struct _EphySourceTransformHandlerClass
{
  GObjectClass parent_class;

  guchar *(* transform_source) (EphySourceTransformHandler *handler,
                                EphySourceTransformRequest *request,
                                const guchar               *source,
                                gsize                       length);
};

void ephy_source_transform_handler_handle_request (EphySourceTransformHandler  *handler,
                                                   WebKitURISchemeRequest      *request);

const char *ephy_source_transform_handler_get_uri (EphySourceTransformHandler *handler);

WebKitWebView *ephy_source_transform_request_get_web_view (EphySourceTransformRequest *request);

void ephy_source_transform_handler_finish_request (EphySourceTransformRequest *request,
                                                   gchar                      *data);

G_END_DECLS
