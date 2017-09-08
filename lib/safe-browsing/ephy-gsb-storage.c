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
#include "ephy-gsb-storage.h"

#include "ephy-debug.h"

struct _EphyGSBStorage {
  GObject parent_instance;

  char *db_path;
};

G_DEFINE_TYPE (EphyGSBStorage, ephy_gsb_storage, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_DB_PATH,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_gsb_storage_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  switch (prop_id) {
    case PROP_DB_PATH:
      g_free (self->db_path);
      self->db_path = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_storage_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  switch (prop_id) {
    case PROP_DB_PATH:
      g_value_set_string (value, self->db_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_storage_finalize (GObject *object)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  g_free (self->db_path);

  G_OBJECT_CLASS (ephy_gsb_storage_parent_class)->finalize (object);
}

static void
ephy_gsb_storage_constructed (GObject *object)
{
  EphyGSBStorage *self = EPHY_GSB_STORAGE (object);

  G_OBJECT_CLASS (ephy_gsb_storage_parent_class)->constructed (object);

  /* TODO: Check database existence/integrity. */
}

static void
ephy_gsb_storage_init (EphyGSBStorage *self)
{
}

static void
ephy_gsb_storage_class_init (EphyGSBStorageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_gsb_storage_set_property;
  object_class->get_property = ephy_gsb_storage_get_property;
  object_class->constructed = ephy_gsb_storage_constructed;
  object_class->finalize = ephy_gsb_storage_finalize;

  obj_properties[PROP_DB_PATH] =
    g_param_spec_string ("db-path",
                         "Database path",
                         "The path of the SQLite file holding the lists of unsafe web resources",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyGSBStorage *
ephy_gsb_storage_new (const char *db_path)
{
  return g_object_new (EPHY_TYPE_GSB_STORAGE, "db-path", db_path, NULL);
}
