/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Red Hat Inc.
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
#include "ephy-uri-helpers.h"

#include <glib.h>

static void
test_ephy_uri_decode (void)
{
  g_autofree char *result = NULL;

  result = ephy_uri_decode ("https://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8");
  g_assert_cmpstr (result, ==, "https://ja.wikipedia.org/wiki/メインページ");

  g_clear_pointer (&result, g_free);
  result = ephy_uri_decode ("https://xn--9dbaqfu.xn--4dbrk0ce/");
  g_assert_cmpstr (result, ==, "https://כולנו.ישראל/");
}

int
main (int   argc,
      char *argv[])
{
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/lib/ephy-uri-helpers/decode", test_ephy_uri_decode);

  ret = g_test_run ();

  return ret;
}
