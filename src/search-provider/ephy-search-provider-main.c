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

#include "ephy-search-provider.h"
#include "ephy-shell.h"
#include "ephy-file-helpers.h"

#include <glib/gi18n.h>
#include <locale.h>

gint
main (gint    argc,
      gchar **argv)
{
  EphySearchProvider *search_provider;
  int status;
  GError *error = NULL;

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

  search_provider = ephy_search_provider_new ();
  status = g_application_run (G_APPLICATION (search_provider), argc, argv);
  g_object_unref (search_provider);

  ephy_file_helpers_shutdown ();

  return status;
}
