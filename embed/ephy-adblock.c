/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011, 2012 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Some parts of this file based on the previous 'adblock' extension,
 *  licensed with the GNU General Public License 2 and later versions,
 *  Copyright (C) 2003 Marco Pesenti Gritti, Christian Persch.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "ephy-adblock.h"

#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "uri-tester.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define EPHY_ADBLOCK_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ADBLOCK, EphyAdBlockPrivate))

struct EphyAdBlockPrivate
{
  UriTester *tester;
};

G_DEFINE_TYPE (EphyAdBlock, ephy_adblock, G_TYPE_OBJECT)

/* Private functions. */

static void
ephy_adblock_init (EphyAdBlock *adblock)
{
  LOG ("EphyAdblock initialising");

  adblock->priv = EPHY_ADBLOCK_GET_PRIVATE (adblock);
  adblock->priv->tester = uri_tester_new ();
}

static void
ephy_adblock_dispose (GObject *object)
{
  EphyAdBlock *adblock = NULL;

  LOG ("EphyAdblock disposing");

  adblock = EPHY_ADBLOCK (object);
  g_clear_object (&adblock->priv->tester);

  G_OBJECT_CLASS (ephy_adblock_parent_class)->dispose (object);
}

static void
ephy_adblock_class_init (EphyAdBlockClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_adblock_dispose;

  g_type_class_add_private (object_class, sizeof (EphyAdBlockPrivate));
}

gboolean
ephy_adblock_should_load (EphyAdBlock *adblock,
                          EphyEmbed *embed,
                          const char *url,
                          AdUriCheckType type)
{
  EphyWebView* web_view = NULL;
  const char *address = NULL;

  g_return_val_if_fail (adblock != NULL, TRUE);
  g_return_val_if_fail (embed != NULL, TRUE);
  g_return_val_if_fail (url, TRUE);

  web_view = ephy_embed_get_web_view (embed);
  address = ephy_web_view_get_address (web_view);

  return !uri_tester_test_uri (adblock->priv->tester, url, address, type);
}
