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

struct _EphyEmbedFormAuthPrivate
{
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
  EphyEmbedFormAuthPrivate *priv = EPHY_EMBED_FORM_AUTH (object)->priv;

  if (priv->uri)
    soup_uri_free (priv->uri);
  g_clear_object (&priv->username_node);
  g_clear_object (&priv->password_node);

  G_OBJECT_CLASS (ephy_embed_form_auth_parent_class)->finalize (object);
}

static void
ephy_embed_form_auth_init (EphyEmbedFormAuth *form_auth)
{
  form_auth->priv = G_TYPE_INSTANCE_GET_PRIVATE (form_auth, EPHY_TYPE_EMBED_FORM_AUTH, EphyEmbedFormAuthPrivate);
}

static void
ephy_embed_form_auth_class_init (EphyEmbedFormAuthClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_embed_form_auth_finalize;
  g_type_class_add_private (object_class, sizeof (EphyEmbedFormAuthPrivate));
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

  form_auth->priv->page_id = webkit_web_page_get_id (web_page);
  form_auth->priv->uri = soup_uri_new (webkit_web_page_get_uri (web_page));
  form_auth->priv->username_node = username_node;
  form_auth->priv->password_node = password_node;
  form_auth->priv->username = g_strdup (username);

  return form_auth;
}

WebKitDOMNode *
ephy_embed_form_auth_get_username_node (EphyEmbedFormAuth *form_auth)
{
  return form_auth->priv->username_node;
}

WebKitDOMNode *
ephy_embed_form_auth_get_password_node (EphyEmbedFormAuth *form_auth)
{
  return form_auth->priv->password_node;
}

SoupURI *
ephy_embed_form_auth_get_uri (EphyEmbedFormAuth *form_auth)
{
  return form_auth->priv->uri;
}

guint64
ephy_embed_form_auth_get_page_id (EphyEmbedFormAuth *form_auth)
{
  return form_auth->priv->page_id;
}

const char*
ephy_embed_form_auth_get_username (EphyEmbedFormAuth *form_auth)
{
  return form_auth->priv->username;
}

WebKitDOMDocument *
ephy_embed_form_auth_get_owner_document (EphyEmbedFormAuth *form_auth)
{
  return webkit_dom_node_get_owner_document (form_auth->priv->password_node);
}
