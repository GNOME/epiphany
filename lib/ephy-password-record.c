/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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
#include "ephy-password-record.h"

struct _EphyPasswordRecord {
  GObject parent_instance;

  char       *origin;
  char       *form_username;
  char       *form_password;
  char       *username;
  char       *password;
};

G_DEFINE_TYPE (EphyPasswordRecord, ephy_password_record, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ORIGIN,
  PROP_FORM_USERNAME,
  PROP_FORM_PASSWORD,
  PROP_USERNAME,
  PROP_PASSWORD,
  LAST_PROP,
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_password_record_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphyPasswordRecord *self = EPHY_PASSWORD_RECORD (object);

  switch (prop_id) {
    case PROP_ORIGIN:
      g_free (self->origin);
      self->origin = g_strdup (g_value_get_string (value));
      break;
    case PROP_FORM_USERNAME:
      g_free (self->form_username);
      self->form_username = g_strdup (g_value_get_string (value));
      break;
    case PROP_FORM_PASSWORD:
      g_free (self->form_password);
      self->form_password = g_strdup (g_value_get_string (value));
      break;
    case PROP_USERNAME:
      g_free (self->username);
      self->username = g_strdup (g_value_get_string (value));
      break;
    case PROP_PASSWORD:
      ephy_password_record_set_password (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_password_record_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EphyPasswordRecord *self = EPHY_PASSWORD_RECORD (object);

  switch (prop_id) {
    case PROP_ORIGIN:
      g_value_set_string (value, ephy_password_record_get_origin (self));
      break;
    case PROP_FORM_USERNAME:
      g_value_set_string (value, ephy_password_record_get_form_username (self));
      break;
    case PROP_FORM_PASSWORD:
      g_value_set_string (value, ephy_password_record_get_form_password (self));
      break;
    case PROP_USERNAME:
      g_value_set_string (value, ephy_password_record_get_username (self));
      break;
    case PROP_PASSWORD:
      g_value_set_string (value, ephy_password_record_get_password (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_password_record_dispose (GObject *object)
{
  EphyPasswordRecord *self = EPHY_PASSWORD_RECORD (object);

  g_clear_pointer (&self->origin, g_free);
  g_clear_pointer (&self->form_username, g_free);
  g_clear_pointer (&self->form_password, g_free);
  g_clear_pointer (&self->username, g_free);
  g_clear_pointer (&self->password, g_free);

  G_OBJECT_CLASS (ephy_password_record_parent_class)->dispose (object);
}

static void
ephy_password_record_class_init (EphyPasswordRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_password_record_set_property;
  object_class->get_property = ephy_password_record_get_property;
  object_class->dispose = ephy_password_record_dispose;

  obj_properties[PROP_ORIGIN] =
    g_param_spec_string ("origin",
                         "Origin",
                         "Origin url that password is applicable at",
                         "Default origin",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_FORM_USERNAME] =
    g_param_spec_string ("form-username",
                         "Form username",
                         "HTML field name of the username",
                         "Default form username",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_FORM_PASSWORD] =
    g_param_spec_string ("form-password",
                         "Form password",
                         "HTML field name of the password",
                         "Default form password",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_USERNAME] =
    g_param_spec_string ("username",
                         "Username",
                         "Username to log in as",
                         "Default username",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_PASSWORD] =
    g_param_spec_string ("password",
                         "Password",
                         "Password for the username",
                         "Default password",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_password_record_init (EphyPasswordRecord *self)
{
}

EphyPasswordRecord *
ephy_password_record_new (const char *origin,
                          const char *form_username,
                          const char *form_password,
                          const char *username,
                          const char *password)
{
  return EPHY_PASSWORD_RECORD (g_object_new (EPHY_TYPE_PASSWORD_RECORD,
                                             "origin", origin,
                                             "form-username", form_username,
                                             "form-password", form_password,
                                             "username", username,
                                             "password", password,
                                             NULL));
}

const char *
ephy_password_record_get_origin (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->origin;
}

const char *
ephy_password_record_get_form_username (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->form_username;
}

const char *
ephy_password_record_get_form_password (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->form_password;
}

const char *
ephy_password_record_get_username (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->username;
}

const char *
ephy_password_record_get_password (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->password;
}

void
ephy_password_record_set_password (EphyPasswordRecord *self,
                                   const char         *password)
{
  g_return_if_fail (EPHY_IS_PASSWORD_RECORD (self));

  g_free (self->password);
  self->password = g_strdup (password);
}
