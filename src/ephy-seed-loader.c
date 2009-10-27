/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2009, Robert Carr <carrr@rpi.edu>
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

#include "ephy-seed-loader.h"
#include "ephy-seed-extension.h"
#include "ephy-loader.h"
#include "ephy-debug.h"

#include <seed.h>

#define EPHY_SEED_LOADER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SEED_LOADER, EphySeedLoaderPrivate))

struct _EphySeedLoaderPrivate
{
  gpointer dummy;
};

static GObject *
impl_get_object (EphyLoader *eloader,
                 GKeyFile *keyfile)
{
  char *filename;
  GObject *object;

  g_return_val_if_fail (keyfile != NULL, NULL);

  filename = g_key_file_get_string (keyfile, "Loader", "Module", NULL);
  if (filename == NULL) {
    g_warning ("NULL module name!\n");
    return NULL;
  }

  object = g_object_new (EPHY_TYPE_SEED_EXTENSION,
                         "filename", filename,
                         NULL);

  g_free (filename);

  return object;
}

static void
impl_release_object (EphyLoader *eloader,
                     GObject *object)
{
  g_return_if_fail (object != NULL);

  g_object_unref (object);
}

static void
ephy_seed_loader_iface_init (EphyLoaderIface *iface)
{
  iface->type = "seed";
  iface->get_object = impl_get_object;
  iface->release_object = impl_release_object;
}

G_DEFINE_TYPE_WITH_CODE (EphySeedLoader, ephy_seed_loader, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LOADER, ephy_seed_loader_iface_init))

static void
ephy_seed_loader_init (EphySeedLoader *loader)
{
  loader->priv = EPHY_SEED_LOADER_GET_PRIVATE (loader);

  LOG ("EphySeedLoader initialising");

}

static void
ephy_seed_loader_finalize (GObject *object)
{
  LOG ("EphySeedLoader finalising");

  G_OBJECT_CLASS (ephy_seed_loader_parent_class)->finalize (object);
}

static void
ephy_seed_loader_class_init (EphySeedLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_seed_loader_finalize;

  g_type_class_add_private (object_class, sizeof (EphySeedLoaderPrivate));
}

