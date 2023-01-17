/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2023 Jamie Murphy <hello@itsjamie.dev>
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

#include "ephy-permissions-manager.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PERMISSION_POPOVER (ephy_permission_popover_get_type())

G_DECLARE_FINAL_TYPE (EphyPermissionPopover, ephy_permission_popover, EPHY, PERMISSION_POPOVER, GtkPopover)

EphyPermissionPopover   *ephy_permission_popover_new                    (EphyPermissionType       permission_type,
                                                                         WebKitPermissionRequest *permission_request,
                                                                         const char              *origin);

EphyPermissionType       ephy_permission_popover_get_permission_type    (EphyPermissionPopover   *self);

WebKitPermissionRequest *ephy_permission_popover_get_permission_request (EphyPermissionPopover   *self);

const char              *ephy_permission_popover_get_origin             (EphyPermissionPopover   *self);

void                     ephy_permission_popover_get_text               (EphyPermissionPopover   *self,
                                                                         char                   **title,
                                                                         char                   **message);

G_END_DECLS
