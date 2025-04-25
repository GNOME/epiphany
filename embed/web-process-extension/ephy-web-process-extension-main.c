/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "ephy-web-process-extension.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-settings.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

static EphyWebProcessExtension *extension = NULL;

G_MODULE_EXPORT void
webkit_web_process_extension_initialize_with_user_data (WebKitWebProcessExtension *webkit_extension,
                                                        GVariant                  *user_data)
{
  const char *guid;
  const char *profile_dir;
  gboolean should_remember_passwords;
  g_autoptr (GVariant) web_extensions = NULL;
  g_autoptr (GError) error = NULL;

  ephy_debug_set_fatal_criticals ();

  g_variant_get (user_data, "(&sm&sbv)", &guid, &profile_dir, &should_remember_passwords, &web_extensions);

  if (!ephy_file_helpers_init (profile_dir, 0, &error))
    g_warning ("Failed to initialize file helpers: %s", error->message);

  ephy_debug_init ();

  ephy_flatpak_utils_set_is_web_process_extension ();
  ephy_settings_set_is_web_process_extension ();

  extension = ephy_web_process_extension_get ();

  ephy_web_process_extension_initialize (extension,
                                         webkit_extension,
                                         guid,
                                         should_remember_passwords,
                                         web_extensions);
}

static void __attribute__((destructor))
ephy_web_process_extension_shutdown (void)
{
  g_clear_object (&extension);
  ephy_settings_shutdown ();
  ephy_file_helpers_shutdown ();
}

#pragma GCC diagnostic pop
