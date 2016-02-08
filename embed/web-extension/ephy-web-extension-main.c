/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "config.h"

#include "ephy-web-extension.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

static EphyWebExtension *extension = NULL;

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *webkit_extension,
                                                GVariant *user_data)
{
  const char *server_address;
  const char *dot_dir;
  gboolean private_profile;
  GError *error = NULL;

  g_variant_get (user_data, "(m&s&sb)", &server_address, &dot_dir, &private_profile);

  if (!server_address) {
    g_warning ("UI process did not start D-Bus server, giving up.");
    return;
  }

  if (!ephy_file_helpers_init (dot_dir, 0, &error)) {
    g_warning ("Failed to initialize file helpers: %s", error->message);
    g_error_free (error);
  }

  ephy_debug_init ();

  extension = ephy_web_extension_get ();

  ephy_web_extension_initialize (extension,
                                 webkit_extension,
                                 server_address,
                                 dot_dir,
                                 private_profile);
}

static void __attribute__((destructor))
ephy_web_extension_shutdown (void)
{
  if (extension)
    g_object_unref (extension);
}

#pragma GCC diagnostic pop
