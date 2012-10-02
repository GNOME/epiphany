/*
 *  Copyright © 2011 Igalia S.L.
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
#include "ephy-adblock-extension.h"

#include "ephy-adblock.h"
#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "uri-tester.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define EPHY_ADBLOCK_EXTENSION_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ADBLOCK_EXTENSION, EphyAdblockExtensionPrivate))

struct EphyAdblockExtensionPrivate
{
  UriTester *tester;
};

static void ephy_adblock_adblock_iface_init (EphyAdBlockIface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyAdblockExtension,
                         ephy_adblock_extension,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_ADBLOCK,
                                                ephy_adblock_adblock_iface_init))

/* Private functions. */

static void
ephy_adblock_extension_init (EphyAdblockExtension *extension)
{
  LOG ("EphyAdblockExtension initialising");

  extension->priv = EPHY_ADBLOCK_EXTENSION_GET_PRIVATE (extension);
  extension->priv->tester = uri_tester_new ();
}

static void
ephy_adblock_extension_dispose (GObject *object)
{
  EphyAdblockExtension *extension = NULL;

  LOG ("EphyAdblockExtension disposing");

  extension = EPHY_ADBLOCK_EXTENSION (object);
  g_clear_object (&extension->priv->tester);

  G_OBJECT_CLASS (ephy_adblock_extension_parent_class)->dispose (object);
}

static void
ephy_adblock_extension_finalize (GObject *object)
{
  LOG ("EphyAdblockExtension finalising");

  G_OBJECT_CLASS (ephy_adblock_extension_parent_class)->finalize (object);
}

static void
ephy_adblock_extension_class_init (EphyAdblockExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_adblock_extension_dispose;
  object_class->finalize = ephy_adblock_extension_finalize;

  g_type_class_add_private (object_class, sizeof (EphyAdblockExtensionPrivate));
}

static gboolean
ephy_adblock_impl_should_load (EphyAdBlock *blocker,
                               EphyEmbed *embed,
                               const char *url,
                               AdUriCheckType type)
{
  EphyAdblockExtension *self = NULL;
  EphyWebView* web_view = NULL;
  const char *address = NULL;

  LOG ("ephy_adblock_impl_should_load checking %s", url);

  self = EPHY_ADBLOCK_EXTENSION (blocker);
  g_return_val_if_fail (self != NULL, TRUE);

  web_view = ephy_embed_get_web_view (embed);
  address = ephy_web_view_get_address (web_view);

  return !uri_tester_test_uri (self->priv->tester, url, address, type);
}

static void
ephy_adblock_adblock_iface_init (EphyAdBlockIface *iface)
{
  iface->should_load = ephy_adblock_impl_should_load;
}
