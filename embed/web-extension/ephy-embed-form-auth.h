/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Igalia S.L.
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

#ifndef EPHY_EMBED_FORM_AUTH_H
#define EPHY_EMBED_FORM_AUTH_H

#include <glib-object.h>
#include <libsoup/soup.h>
#include <webkit2/webkit-web-extension.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_FORM_AUTH     (ephy_embed_form_auth_get_type ())
#define EPHY_EMBED_FORM_AUTH(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_EMBED_FORM_AUTH, EphyEmbedFormAuth))
#define EPHY_IS_EMBED_FORM_AUTH(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_EMBED_FORM_AUTH))

typedef struct _EphyEmbedFormAuthClass   EphyEmbedFormAuthClass;
typedef struct _EphyEmbedFormAuth        EphyEmbedFormAuth;
typedef struct _EphyEmbedFormAuthPrivate EphyEmbedFormAuthPrivate;

struct _EphyEmbedFormAuth
{
  GObject parent;

  EphyEmbedFormAuthPrivate *priv;
};

struct _EphyEmbedFormAuthClass
{
  GObjectClass parent_class;
};

GType              ephy_embed_form_auth_get_type          (void);
EphyEmbedFormAuth *ephy_embed_form_auth_new               (WebKitWebPage     *web_page,
                                                           WebKitDOMNode     *username_node,
                                                           WebKitDOMNode     *password_node,
                                                           const char        *username);
WebKitDOMNode     *ephy_embed_form_auth_get_username_node (EphyEmbedFormAuth *form_auth);
WebKitDOMNode     *ephy_embed_form_auth_get_password_node (EphyEmbedFormAuth *form_auth);
SoupURI           *ephy_embed_form_auth_get_uri           (EphyEmbedFormAuth *form_auth);
guint64            ephy_embed_form_auth_get_page_id       (EphyEmbedFormAuth *form_auth);
const char        *ephy_embed_form_auth_get_username      (EphyEmbedFormAuth *form_auth);
WebKitDOMDocument *ephy_embed_form_auth_get_owner_document(EphyEmbedFormAuth *form_auth);

G_END_DECLS

#endif /* EPHY_EMBED_FORM_AUTH_H */
