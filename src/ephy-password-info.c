/*
 *  Copyright © 2009 Holger Hans Peter Freyther
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

#include "ephy-password-info.h"

#include <gnome-keyring-memory.h>


static EphyPasswordInfo*
password_info_copy (EphyPasswordInfo *info)
{
  EphyPasswordInfo *other = g_new (EphyPasswordInfo, 1);
  if (other == NULL)
    return NULL;

  other->keyring_id = info->keyring_id;
  other->secret = gnome_keyring_memory_strdup (info->secret);
  return other;
}

static void
password_info_free (EphyPasswordInfo *info)
{
  gnome_keyring_memory_free (info->secret);
  g_free (info);
}

GType
ephy_password_info_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    type = g_boxed_type_register_static("EphyTypePasswordInfo",
        (GBoxedCopyFunc) password_info_copy,
        (GBoxedFreeFunc) password_info_free);
  }

  return type;
}

EphyPasswordInfo
*ephy_password_info_new (guint32 key_id)
{
  EphyPasswordInfo *info = g_new0 (EphyPasswordInfo, 1);
  if (info == NULL)
    return NULL;

  info->keyring_id = key_id;
  return info;
}
