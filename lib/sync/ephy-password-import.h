/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 20120 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include <glib.h>

#include "ephy-password-manager.h"

typedef enum {
  CHROME,
  CHROMIUM,
  CSV
} ChromeType;

void ephy_password_import_from_chrome_async (EphyPasswordManager *manager, ChromeType type, GAsyncReadyCallback callback, gpointer user_data);
gboolean ephy_password_import_from_chrome_finish (GObject *source_object, GAsyncResult *result, GError **error);
gboolean ephy_password_import_from_csv (EphyPasswordManager *manager, const char *filename, GError **error);
