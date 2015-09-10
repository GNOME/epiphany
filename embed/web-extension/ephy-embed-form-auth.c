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

#include <config.h>
#include "ephy-embed-form-auth.h"

struct _EphyEmbedFormAuth
{
  GObject parent_instance;

  guint64 page_id;
  SoupURI *uri;
  WebKitDOMNode *username_node;
  WebKitDOMNode *password_node;
  char *username;
};

G_DEFINE_TYPE (EphyEmbedFormAuth, ephy_embed_form_auth, G_TYPE_OBJECT)

static void
ephy_embed_form_auth_finalize (GObject *object)
{
  EphyEmbedFormAuth *form_auth = EPHY_EMBED_FORM_AUTH (object);

  if (form_auth->uri)
    soup_uri_free (form_auth->uri);
  g_clear_object (&form_auth->username_node);
  g_clear_object (&form_auth->password_node);

  G_OBJECT_CLASS (ephy_embed_form_auth_parent_class)->finalize (object);
}

static void
ephy_embed_form_auth_init (EphyEmbedFormAuth *form_auth)
{
}

static void
ephy_embed_form_auth_class_init (EphyEmbedFormAuthClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_embed_form_auth_finalize;
}

EphyEmbedFormAuth *
ephy_embed_form_auth_new (WebKitWebPage *web_page,
                          WebKitDOMNode *username_node,
                          WebKitDOMNode *password_node,
                          const char* username)
{
  EphyEmbedFormAuth *form_auth;

  g_return_val_if_fail (WEBKIT_DOM_IS_NODE (password_node), NULL);

  form_auth = EPHY_EMBED_FORM_AUTH (g_object_new (EPHY_TYPE_EMBED_FORM_AUTH, NULL));

  form_auth->page_id = webkit_web_page_get_id (web_page);
  form_auth->uri = soup_uri_new (webkit_web_page_get_uri (web_page));
  form_auth->username_node = username_node;
  form_auth->password_node = password_node;
  form_auth->username = g_strdup (username);

  return form_auth;
}

WebKitDOMNode *
ephy_embed_form_auth_get_username_node (EphyEmbedFormAuth *form_auth)
{
  return form_auth->username_node;
}

WebKitDOMNode *
ephy_embed_form_auth_get_password_node (EphyEmbedFormAuth *form_auth)
{
  return form_auth->password_node;
}

SoupURI *
ephy_embed_form_auth_get_uri (EphyEmbedFormAuth *form_auth)
{
  return form_auth->uri;
}

guint64
ephy_embed_form_auth_get_page_id (EphyEmbedFormAuth *form_auth)
{
  return form_auth->page_id;
}

const char*
ephy_embed_form_auth_get_username (EphyEmbedFormAuth *form_auth)
{
  return form_auth->username;
}

WebKitDOMDocument *
ephy_embed_form_auth_get_owner_document (EphyEmbedFormAuth *form_auth)
{
  return webkit_dom_node_get_owner_document (form_auth->password_node);
}
