/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2014 Igalia S.L.
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
#include "ephy-security-levels.h"

/**
 * ephy_security_level_to_icon_name:
 * @level: an #EphySecurityLevel
 *
 * Returns: the icon name corresponding to this security level,
 *   or NULL if no icon should be shown.
 */
const char *
ephy_security_level_to_icon_name (EphySecurityLevel level)
{
  const char *result;

  switch (level) {
  case EPHY_SECURITY_LEVEL_NO_SECURITY:
    result = NULL;
    break;
  case EPHY_SECURITY_LEVEL_BROKEN_SECURITY:
    result = "channel-insecure-symbolic";
    break;
  case EPHY_SECURITY_LEVEL_MIXED_CONTENT:
    result = "dialog-warning-symbolic";
    break;
  case EPHY_SECURITY_LEVEL_STRONG_SECURITY:
    result = "channel-secure-symbolic";
    break;
  default:
    g_assert_not_reached ();
  }

  return result;
}
