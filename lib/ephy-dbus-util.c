/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ephy-dbus-util.h"

gboolean
ephy_dbus_peer_is_authorized (GCredentials *peer_credentials)
{
  static GCredentials *own_credentials = NULL;
  GError *error = NULL;

  if (!own_credentials)
    own_credentials = g_credentials_new ();

  if (peer_credentials && g_credentials_is_same_user (peer_credentials, own_credentials, &error))
    return TRUE;

  if (error) {
    g_warning ("Failed to authorize web extension connection: %s", error->message);
    g_error_free (error);
  }
  return FALSE;
}
