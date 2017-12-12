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
#include "ephy-synchronizable.h"

#include <math.h>

G_DEFINE_INTERFACE (EphySynchronizable, ephy_synchronizable, JSON_TYPE_SERIALIZABLE);

static void
ephy_synchronizable_default_init (EphySynchronizableInterface *iface)
{
  iface->get_id = ephy_synchronizable_get_id;
  iface->get_server_time_modified = ephy_synchronizable_get_server_time_modified;
  iface->set_server_time_modified = ephy_synchronizable_set_server_time_modified;
  iface->to_bso = ephy_synchronizable_to_bso;
}

/**
 * ephy_synchronizable_get_id:
 * @synchronizable: an #EphySynchronizable
 *
 * Returns @synchronizable's id.
 *
 * Return value: (transfer none): @synchronizable's id
 **/
const char *
ephy_synchronizable_get_id (EphySynchronizable *synchronizable)
{
  EphySynchronizableInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));

  iface = EPHY_SYNCHRONIZABLE_GET_IFACE (synchronizable);
  return iface->get_id (synchronizable);
}

/**
 * ephy_synchronizable_get_server_time_modified:
 * @synchronizable: an #EphySynchronizable
 *
 * Returns @synchronizable's last modification time on the storage server.
 *
 * Return value: @synchronizable's last modification time
 **/
gint64
ephy_synchronizable_get_server_time_modified (EphySynchronizable *synchronizable)
{
  EphySynchronizableInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));

  iface = EPHY_SYNCHRONIZABLE_GET_IFACE (synchronizable);
  return iface->get_server_time_modified (synchronizable);
}

/**
 * ephy_synchronizable_set_server_time_modified:
 * @synchronizable: an #EphySynchronizable
 * @server_time_modified: the last modification time on the storage server
 *
 * Sets @server_time_modified as @synchronizable's last modification time.
 **/
void
ephy_synchronizable_set_server_time_modified (EphySynchronizable *synchronizable,
                                              gint64              server_time_modified)
{
  EphySynchronizableInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));

  iface = EPHY_SYNCHRONIZABLE_GET_IFACE (synchronizable);
  iface->set_server_time_modified (synchronizable, server_time_modified);
}

/**
 * ephy_synchronizable_to_bso:
 * @synchronizable: an #EphySynchronizable
 * @bundle: a %SyncCryptoKeyBundle holding the encryption key and the HMAC key
 *          used to validate and encrypt the Basic Storage Object
 *
 * Converts an #EphySynchronizable into its JSON string representation
 * of a Basic Storage Object from the client's point of view
 * (i.e. the %modified field is missing). Check the BSO format documentation
 * (https://docs.services.mozilla.com/storage/apis-1.5.html#basic-storage-object)
 * for more details.
 *
 * Return value: (transfer full): @synchronizable's BSO representation as a #JsonNode
 **/
JsonNode *
ephy_synchronizable_to_bso (EphySynchronizable  *synchronizable,
                            SyncCryptoKeyBundle *bundle)
{
  EphySynchronizableInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (bundle);

  iface = EPHY_SYNCHRONIZABLE_GET_IFACE (synchronizable);
  return iface->to_bso (synchronizable, bundle);
}

/**
 * ephy_synchronizable_from_bso:
 * @bso: a #JsonNode representing the Basic Storage Object
 * @gtype: the #GType of object to construct
 * @bundle: a %SyncCryptoKeyBundle holding the encryption key and the HMAC key
 *          used to validate and decrypt the Basic Storage Object
 * @is_deleted: return value for a flag that says whether the object
 *              was marked as deleted
 *
 * Converts a JSON object representing the Basic Storage Object
 * from the server's point of view (i.e. the %modified field is present)
 * into an object of type @gtype. See the BSO format documentation
 * (https://docs.services.mozilla.com/storage/apis-1.5.html#basic-storage-object)
 * for more details.
 *
 * Note: The @gtype must be a sub-type of #EphySynchronizable (i.e. must
 * implement the #EphySynchronizable interface). It is up to the caller to cast
 * the returned #GObject to the type of @gtype.
 *
 *  Return value: (transfer full): a #GObject or %NULL
 **/
GObject *
ephy_synchronizable_from_bso (JsonNode            *bso,
                              GType                gtype,
                              SyncCryptoKeyBundle *bundle,
                              gboolean            *is_deleted)
{
  GObject *object = NULL;
  GError *error = NULL;
  JsonNode *node = NULL;
  JsonObject *json;
  char *serialized = NULL;
  const char *payload = NULL;
  double server_time_modified;

  g_assert (bso);
  g_assert (bundle);
  g_assert (is_deleted);

  json = json_node_get_object (bso);
  if (!json) {
    g_warning ("JSON node does not hold a JSON object");
    goto out;
  }
  payload = json_object_get_string_member (json, "payload");
  server_time_modified = json_object_get_double_member (json, "modified");
  if (!payload || !server_time_modified) {
    g_warning ("JSON object has missing or invalid members");
    goto out;
  }

  serialized = ephy_sync_crypto_decrypt_record (payload, bundle);
  if (!serialized) {
    g_warning ("Failed to decrypt the BSO payload");
    goto out;
  }
  node = json_from_string (serialized, &error);
  if (error) {
    g_warning ("Decrypted text is not a valid JSON: %s", error->message);
    goto out;
  }
  json = json_node_get_object (node);
  if (!json) {
    g_warning ("Decrypted JSON node does not hold a JSON object");
    goto out;
  }
  *is_deleted = json_object_has_member (json, "deleted");

  object = json_gobject_from_data (gtype, serialized, -1, &error);
  if (error) {
    g_warning ("Failed to create GObject from BSO: %s", error->message);
    goto out;
  }

  ephy_synchronizable_set_server_time_modified (EPHY_SYNCHRONIZABLE (object),
                                                ceil (server_time_modified));

out:
  if (node)
    json_node_unref (node);
  if (error)
    g_error_free (error);
  g_free (serialized);

  return object;
}

/**
 * ephy_synchronizable_default_to_bso:
 * @synchronizable: an #EphySynchronizable
 * @bundle: a %SyncCryptoKeyBundle holding the encryption key and the HMAC key
 *          used to validate and encrypt the Basic Storage Object
 *
 * Calls the default implementation of the #EphySynchronizable
 * #EphySynchronizableInterface.to_bso() virtual function.
 *
 * This function can be used inside a custom implementation of the
 * #EphySynchronizableInterface.to_bso() virtual function in lieu of
 * calling the default implementation through g_type_default_interface_peek().
 *
 * Return value: (transfer full): @synchronizable's BSO representation as a #JsonNode
 **/
JsonNode *
ephy_synchronizable_default_to_bso (EphySynchronizable  *synchronizable,
                                    SyncCryptoKeyBundle *bundle)
{
  JsonNode *bso;
  JsonObject *object;
  char *serialized;
  char *payload;

  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));
  g_assert (bundle);

  serialized = json_gobject_to_data (G_OBJECT (synchronizable), NULL);
  payload = ephy_sync_crypto_encrypt_record (serialized, bundle);
  bso = json_node_new (JSON_NODE_OBJECT);
  object = json_object_new ();
  json_object_set_string_member (object, "id", ephy_synchronizable_get_id (synchronizable));
  json_object_set_string_member (object, "payload", payload);
  json_node_set_object (bso, object);

  json_object_unref (object);
  g_free (payload);
  g_free (serialized);

  return bso;
}
