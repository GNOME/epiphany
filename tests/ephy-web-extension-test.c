/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Roland Chamuel (pxrb).
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

#include "ephy-web-extension.h"

static void
test_ephy_invalid_command_parse (void)
{
  g_autofree char *result = NULL;

  result = ephy_web_extension_parse_command_key ("Alt+Shift+Space");
  g_assert_cmpstr (result, ==, "<Alt><Shift>space");
}

int
main (int   argc,
      char *argv[])
{
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/lib/ephy-web-extension/invalid_command_parse", test_ephy_invalid_command_parse);

  ret = g_test_run ();

  return ret;
}
