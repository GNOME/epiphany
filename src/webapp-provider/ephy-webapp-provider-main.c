/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright (c) 2013 Igalia S.L.
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

#include "ephy-webapp-provider.h"
#include "ephy-file-helpers.h"

#include <glib/gi18n.h>
#include <locale.h>

int
main (int    argc,
      char **argv)
{
  g_autoptr (EphyWebAppProviderService) webapp_provider = NULL;
  int status;
  GError *error = NULL;

  g_setenv ("GIO_USE_VFS", "local", TRUE);

  g_debug ("started %s", argv[0]);

  /* Initialize the i18n stuff */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  if (!ephy_file_helpers_init (NULL, 0, &error)) {
    g_printerr ("%s\n", error->message);
    g_error_free (error);
    return 1;
  }

  webapp_provider = ephy_web_app_provider_service_new ();
  status = g_application_run (G_APPLICATION (webapp_provider), argc, argv);

  ephy_file_helpers_shutdown ();

  g_debug ("stopping %s with status %d", argv[0], status);

  return status;
}
