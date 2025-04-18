/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Igalia S.L.
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

#pragma once

#include <gio/gio.h>

typedef enum {
  EPHY_OPEN_URI_FLAGS_NONE                     = 0,
  EPHY_OPEN_URI_FLAGS_REQUIRE_USER_INTERACTION = 1 << 0
} EphyOpenUriFlags;

void     ephy_flatpak_utils_set_is_web_process_extension (void);

gboolean ephy_is_running_inside_sandbox                  (void);

void     ephy_open_uri_via_flatpak_portal                (const char       *uri,
                                                          EphyOpenUriFlags  flags);
void     ephy_open_directory_via_flatpak_portal          (const char       *uri,
                                                          EphyOpenUriFlags  flags);

gboolean ephy_can_install_web_apps                       (void);
