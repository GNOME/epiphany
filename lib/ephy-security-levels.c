/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
    case EPHY_SECURITY_LEVEL_LOCAL_PAGE:
    case EPHY_SECURITY_LEVEL_TO_BE_DETERMINED:
      result = NULL;
      break;
    case EPHY_SECURITY_LEVEL_NO_SECURITY:
    case EPHY_SECURITY_LEVEL_MIXED_CONTENT:
    case EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE:
      result = "channel-insecure-symbolic";
      break;
    case EPHY_SECURITY_LEVEL_STRONG_SECURITY:
      result = "channel-secure-symbolic";
      break;
    default:
      g_assert_not_reached ();
  }

  return result;
}

gboolean
ephy_security_level_is_secure (EphySecurityLevel level)
{
  return level >= EPHY_SECURITY_LEVEL_STRONG_SECURITY;
}
