/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Igalia S.L.
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

#include <config.h>
#include "ephy-embed-form-auth.h"

#include "ephy-uri-helpers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

struct _EphyEmbedFormAuth {
  GObject parent_instance;

  guint64 page_id;
  char *origin;
  char *target_origin;
  WebKitDOMNode *username_node;
  WebKitDOMNode *password_node;
  char *username;
  char *password;
  gboolean password_updated;
};

G_DEFINE_TYPE (EphyEmbedFormAuth, ephy_embed_form_auth, G_TYPE_OBJECT)

static void
ephy_embed_form_auth_finalize (GObject *object)
{
  EphyEmbedFormAuth *form_auth = EPHY_EMBED_FORM_AUTH (object);

  g_free (form_auth->username);
  g_free (form_auth->password);
  g_free (form_auth->origin);
  g_free (form_auth->target_origin);
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
                          const char    *form_action,
                          WebKitDOMNode *username_node,
                          WebKitDOMNode *password_node,
                          const char    *username,
                          const char    *password)
{
  EphyEmbedFormAuth *form_auth;
  char *origin;

  g_assert (WEBKIT_DOM_IS_NODE (password_node));

  origin = ephy_uri_to_security_origin (webkit_web_page_get_uri (web_page));
  if (!origin)
    return NULL;

  form_auth = EPHY_EMBED_FORM_AUTH (g_object_new (EPHY_TYPE_EMBED_FORM_AUTH, NULL));

  form_auth->page_id = webkit_web_page_get_id (web_page);
  form_auth->origin = origin;
  form_auth->username_node = username_node;
  form_auth->password_node = password_node;
  form_auth->username = g_strdup (username);
  form_auth->password = g_strdup (password);

  if (form_action)
    form_auth->target_origin = ephy_uri_to_security_origin (form_action);

  if (!form_auth->target_origin)
    form_auth->target_origin = g_strdup (form_auth->origin);

  return form_auth;
}

const char *
ephy_embed_form_auth_get_origin (EphyEmbedFormAuth *form_auth)
{
  return form_auth->origin;
}

const char *
ephy_embed_form_auth_get_target_origin (EphyEmbedFormAuth *form_auth)
{
  return form_auth->target_origin;
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

guint64
ephy_embed_form_auth_get_page_id (EphyEmbedFormAuth *form_auth)
{
  return form_auth->page_id;
}

const char *
ephy_embed_form_auth_get_username (EphyEmbedFormAuth *form_auth)
{
  return form_auth->username;
}

const char *
ephy_embed_form_auth_get_password (EphyEmbedFormAuth *form_auth)
{
  return form_auth->password;
}

gboolean
ephy_embed_form_auth_get_password_updated (EphyEmbedFormAuth *form_auth)
{
  return form_auth->password_updated;
}

void
ephy_embed_form_auth_set_password_updated (EphyEmbedFormAuth *form_auth,
                                           gboolean           password_updated)
{
  form_auth->password_updated = password_updated;
}

WebKitDOMDocument *
ephy_embed_form_auth_get_owner_document (EphyEmbedFormAuth *form_auth)
{
  return webkit_dom_node_get_owner_document (form_auth->password_node);
}

#pragma GCC diagnostic pop
