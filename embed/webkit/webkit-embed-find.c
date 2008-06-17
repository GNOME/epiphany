/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-debug.h"
#include "ephy-embed-find.h"
#include "ephy-embed-shell.h"

#include <webkit/webkit.h>

#include "webkit-embed-find.h"

#define WEBKIT_EMBED_FIND_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), WEBKIT_TYPE_EMBED_FIND, WebKitEmbedFindPrivate))

struct _WebKitEmbedFindPrivate
{
  WebKitWebView *web_view;
  char *find_string;
  gboolean case_sensitive;
};

static void
impl_set_embed (EphyEmbedFind *efind,
                EphyEmbed *embed)
{
  WebKitEmbedFindPrivate *priv = WEBKIT_EMBED_FIND (efind)->priv;

  priv->web_view = WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (gtk_bin_get_child (GTK_BIN (embed)))));
}

static void
set_string_and_highlight (WebKitEmbedFindPrivate *priv, const char *find_string)
{
  WebKitWebView *web_view = priv->web_view;

  if (g_strcmp0 (priv->find_string, find_string) != 0) {
    g_free (priv->find_string);
    priv->find_string = g_strdup (find_string);
  }
  webkit_web_view_unmark_text_matches (web_view);
  webkit_web_view_mark_text_matches (web_view,
                                     priv->find_string,
                                     priv->case_sensitive,
                                     0);
  webkit_web_view_set_highlight_text_matches (web_view, TRUE);
}

static void
impl_set_properties (EphyEmbedFind *efind,
                     const char *find_string,
                     gboolean case_sensitive)
{
  WebKitEmbedFindPrivate *priv = WEBKIT_EMBED_FIND (efind)->priv;
  
  priv->case_sensitive = case_sensitive;
  set_string_and_highlight (priv, find_string);
}

static EphyEmbedFindResult
real_find (WebKitEmbedFind *wefind,
           gboolean forward)
{
  WebKitEmbedFindPrivate *priv = wefind->priv;
  WebKitWebView *web_view = priv->web_view;

  if (!webkit_web_view_search_text 
         (web_view, priv->find_string, priv->case_sensitive, TRUE, FALSE)) {
    /* not found, try to wrap */
    if (!webkit_web_view_search_text 
           (web_view, priv->find_string, priv->case_sensitive, TRUE, TRUE)) {
      /* there's no result */
      return EPHY_EMBED_FIND_NOTFOUND;
    } else {
      /* found wrapped */
      return EPHY_EMBED_FIND_FOUNDWRAPPED;
    }
  }

  return EPHY_EMBED_FIND_FOUND;
}

static EphyEmbedFindResult
impl_find (EphyEmbedFind *efind,
           const char *find_string,
           gboolean links_only)
{
  WebKitEmbedFindPrivate *priv = WEBKIT_EMBED_FIND (efind)->priv;

  set_string_and_highlight (priv, find_string);

  return real_find (WEBKIT_EMBED_FIND (efind), TRUE);
}

static EphyEmbedFindResult
impl_find_again (EphyEmbedFind *efind,
                 gboolean forward,
                 gboolean links_only)
{
  return real_find (WEBKIT_EMBED_FIND (efind), forward);
}

static void
impl_set_selection (EphyEmbedFind *efind,
                    gboolean attention)
{
  WebKitWebView *web_view = WEBKIT_EMBED_FIND (efind)->priv->web_view;
  
  webkit_web_view_set_highlight_text_matches (web_view, attention);
}

static gboolean
impl_activate_link (EphyEmbedFind *efind,
                    GdkModifierType mask)
{
  return FALSE;
}

static void
ephy_find_iface_init (EphyEmbedFindIface *iface)
{
  iface->set_embed = impl_set_embed;
  iface->set_properties = impl_set_properties;
  iface->find = impl_find;
  iface->find_again = impl_find_again;
  iface->set_selection = impl_set_selection;
  iface->activate_link = impl_activate_link;
}

static void
webkit_embed_find_init (WebKitEmbedFind *find)
{
  WebKitEmbedFindPrivate *priv = find->priv = WEBKIT_EMBED_FIND_GET_PRIVATE (find);

  priv->web_view = NULL;
  priv->case_sensitive = FALSE;
  priv->find_string = NULL;
}

G_DEFINE_TYPE_WITH_CODE (WebKitEmbedFind, webkit_embed_find, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_FIND,
                                                ephy_find_iface_init))

static void
webkit_embed_find_finalize (GObject *o)
{
  WebKitEmbedFindPrivate *priv = WEBKIT_EMBED_FIND (o)->priv;

  g_free (priv->find_string);
  G_OBJECT_CLASS (webkit_embed_find_parent_class)->finalize (o);
}

static void
webkit_embed_find_class_init (WebKitEmbedFindClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = webkit_embed_find_finalize;

  g_type_class_add_private (object_class, sizeof (WebKitEmbedFindPrivate));
}
