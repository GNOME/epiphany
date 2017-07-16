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

#include "ephy-synchronizable.h"

struct _EphyPasswordRecord {
  GObject parent_instance;

  char    *id;
  char    *hostname;
  char    *form_submit_url;
  char    *username;
  char    *password;
  char    *username_field;
  char    *password_field;
  guint64  time_created;
  guint64  time_password_changed;

  double   server_time_modified;
};

static void json_serializable_iface_init (JsonSerializableIface *iface);
static void ephy_synchronizable_iface_init (EphySynchronizableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyPasswordRecord, ephy_password_record, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE,
                                                json_serializable_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE,
                                                ephy_synchronizable_iface_init))

enum {
  PROP_0,
  PROP_ID,                    /* Firefox Sync */
  PROP_HOSTNAME,              /* Epiphany && Firefox Sync */
  PROP_FORM_SUBMIT_URL,       /* Firefox Sync */
  PROP_USERNAME,              /* Epiphany && Firefox Sync */
  PROP_PASSWORD,              /* Epiphany && Firefox Sync */
  PROP_USERNAME_FIELD,        /* Epiphany && Firefox Sync */
  PROP_PASSWORD_FIELD,        /* Epiphany && Firefox Sync */
  PROP_TIME_CREATED,          /* Firefox Sync */
  PROP_TIME_PASSWORD_CHANGED, /* Firefox Sync */
  LAST_PROP
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
    case PROP_ID:
      g_free (self->id);
      self->id = g_strdup (g_value_get_string (value));
      break;
    case PROP_HOSTNAME:
      g_free (self->hostname);
      self->hostname = g_strdup (g_value_get_string (value));
      break;
    case PROP_FORM_SUBMIT_URL:
      g_free (self->form_submit_url);
      self->form_submit_url = g_strdup (g_value_get_string (value));
      break;
    case PROP_USERNAME:
      g_free (self->username);
      self->username = g_strdup (g_value_get_string (value));
      break;
    case PROP_PASSWORD:
      g_free (self->password);
      self->password = g_strdup (g_value_get_string (value));
      break;
    case PROP_USERNAME_FIELD:
      g_free (self->username_field);
      self->username_field = g_strdup (g_value_get_string (value));
      break;
    case PROP_PASSWORD_FIELD:
      g_free (self->password_field);
      self->password_field = g_strdup (g_value_get_string (value));
      break;
    case PROP_TIME_CREATED:
      self->time_created = g_value_get_uint64 (value);
      break;
    case PROP_TIME_PASSWORD_CHANGED:
      self->time_password_changed = g_value_get_uint64 (value);
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
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_HOSTNAME:
      g_value_set_string (value, self->hostname);
      break;
    case PROP_FORM_SUBMIT_URL:
      g_value_set_string (value, self->form_submit_url);
      break;
    case PROP_USERNAME:
      g_value_set_string (value, self->username);
      break;
    case PROP_PASSWORD:
      g_value_set_string (value, self->password);
      break;
    case PROP_USERNAME_FIELD:
      g_value_set_string (value, self->username_field);
      break;
    case PROP_PASSWORD_FIELD:
      g_value_set_string (value, self->password_field);
      break;
    case PROP_TIME_CREATED:
      g_value_set_uint64 (value, self->time_created);
      break;
    case PROP_TIME_PASSWORD_CHANGED:
      g_value_set_uint64 (value, self->time_password_changed);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_password_record_dispose (GObject *object)
{
  EphyPasswordRecord *self = EPHY_PASSWORD_RECORD (object);

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->hostname, g_free);
  g_clear_pointer (&self->form_submit_url, g_free);
  g_clear_pointer (&self->username, g_free);
  g_clear_pointer (&self->password, g_free);
  g_clear_pointer (&self->username_field, g_free);
  g_clear_pointer (&self->password_field, g_free);

  G_OBJECT_CLASS (ephy_password_record_parent_class)->dispose (object);
}

static void
ephy_password_record_class_init (EphyPasswordRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_password_record_set_property;
  object_class->get_property = ephy_password_record_get_property;
  object_class->dispose = ephy_password_record_dispose;

  obj_properties[PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Id of the password record",
                         "Default id",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_HOSTNAME] =
    g_param_spec_string ("hostname",
                         "Hostname",
                         "Hostname url that password is applicable at",
                         "Default hostname",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_FORM_SUBMIT_URL] =
    g_param_spec_string ("formSubmitURL",
                         "Form submit URL",
                         "Submission URL set by form",
                         "Default form submit URL",
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
  obj_properties[PROP_USERNAME_FIELD] =
    g_param_spec_string ("usernameField",
                         "Username field",
                         "HTML field name of the username",
                         "Default username field",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_PASSWORD_FIELD] =
    g_param_spec_string ("passwordField",
                         "Password field",
                         "HTML field name of the password",
                         "Default password field",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_TIME_CREATED] =
    g_param_spec_uint64 ("timeCreated",
                         "Time created",
                         "Unix timestamp in milliseconds at which the password was created",
                         0,
                         G_MAXUINT64,
                         0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_TIME_PASSWORD_CHANGED] =
    g_param_spec_uint64 ("timePasswordChanged",
                         "Time password changed",
                         "Unix timestamp in milliseconds at which the password was changed",
                         0,
                         G_MAXUINT64,
                         0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_password_record_init (EphyPasswordRecord *self)
{
}

EphyPasswordRecord *
ephy_password_record_new (const char *id,
                          const char *hostname,
                          const char *username,
                          const char *password,
                          const char *username_field,
                          const char *password_field,
                          guint64     time_created,
                          guint64     time_password_changed)
{
  return EPHY_PASSWORD_RECORD (g_object_new (EPHY_TYPE_PASSWORD_RECORD,
                                             "id", id,
                                             "hostname", hostname,
                                             "formSubmitURL", hostname,
                                             "username", username,
                                             "password", password,
                                             "usernameField", username_field,
                                             "passwordField", password_field,
                                             "timeCreated", time_created,
                                             "timePasswordChanged", time_password_changed,
                                             NULL));
}

const char *
ephy_password_record_get_id (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->id;
}

const char *
ephy_password_record_get_hostname (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->hostname;
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

const char *
ephy_password_record_get_username_field (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->username_field;
}

const char *
ephy_password_record_get_password_field (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), NULL);

  return self->password_field;
}

guint64
ephy_password_record_get_time_password_changed (EphyPasswordRecord *self)
{
  g_return_val_if_fail (EPHY_IS_PASSWORD_RECORD (self), 0);

  return self->time_password_changed;
}

static void
json_serializable_iface_init (JsonSerializableIface *iface)
{
  iface->serialize_property = json_serializable_default_serialize_property;
  iface->deserialize_property = json_serializable_default_deserialize_property;
}

static const char *
synchronizable_get_id (EphySynchronizable *synchronizable)
{
  return ephy_password_record_get_id (EPHY_PASSWORD_RECORD (synchronizable));
}

static double
synchronizable_get_server_time_modified (EphySynchronizable *synchronizable)
{
  return EPHY_PASSWORD_RECORD (synchronizable)->server_time_modified;
}

static void
synchronizable_set_server_time_modified (EphySynchronizable *synchronizable,
                                         double              server_time_modified)
{
  EPHY_PASSWORD_RECORD (synchronizable)->server_time_modified = server_time_modified;
}

static void
ephy_synchronizable_iface_init (EphySynchronizableInterface *iface)
{
  iface->get_id = synchronizable_get_id;
  iface->get_server_time_modified = synchronizable_get_server_time_modified;
  iface->set_server_time_modified = synchronizable_set_server_time_modified;
  iface->to_bso = ephy_synchronizable_default_to_bso;
}
