/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-gsb-service.h"

#include "ephy-debug.h"
#include "ephy-gsb-storage.h"

struct _EphyGSBService {
  GObject parent_instance;

  EphyGSBStorage *storage;
};

G_DEFINE_TYPE (EphyGSBService, ephy_gsb_service, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_GSB_STORAGE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_gsb_service_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  switch (prop_id) {
    case PROP_GSB_STORAGE:
      if (self->storage)
        g_object_unref (self->storage);
      self->storage = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_service_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  switch (prop_id) {
    case PROP_GSB_STORAGE:
      g_value_set_object (value, self->storage);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_service_dispose (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  g_clear_object (&self->storage);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->dispose (object);
}

static void
ephy_gsb_service_constructed (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->constructed (object);

  /* TODO: Perform an initial database update if necessary. */
}

static void
ephy_gsb_service_init (EphyGSBService *self)
{
}

static void
ephy_gsb_service_class_init (EphyGSBServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_gsb_service_set_property;
  object_class->get_property = ephy_gsb_service_get_property;
  object_class->constructed = ephy_gsb_service_constructed;
  object_class->dispose = ephy_gsb_service_dispose;

  obj_properties[PROP_GSB_STORAGE] =
    g_param_spec_object ("gsb-storage",
                         "GSB filename",
                         "Handler object for the Google Safe Browsing database",
                         EPHY_TYPE_GSB_STORAGE,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyGSBService *
ephy_gsb_service_new (const char *db_path)
{
  EphyGSBService *service;
  EphyGSBStorage *storage;

  storage = ephy_gsb_storage_new (db_path);
  service = g_object_new (EPHY_TYPE_GSB_SERVICE, "gsb-storage", storage, NULL);
  g_object_unref (storage);

  return service;
}
