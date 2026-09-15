/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Epiphany Developers
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

#include "ephy-output-encoding.h"

#include <glib.h>

static void
test_encode_for_html (void)
{
  g_autofree char *result = NULL;

  result = ephy_encode_for_html ("<a href=\"/test?x=1&y='2'\">Click & Read</a>");
  g_assert_cmpstr (result, ==, "&lt;a href=&quot;&#x2F;test?x=1&amp;y=&#x27;2&#x27;&quot;&gt;Click &amp; Read&lt;&#x2F;a&gt;");
}

static void
test_encode_for_html_attribute (void)
{
  g_autofree char *result = NULL;

  result = ephy_encode_for_html_attribute ("Hello, World!");
  g_assert_cmpstr (result, ==, "Hello&#x2c;&#x20;World&#x21;");
}

static void
test_encode_for_js_quoted_data_value (void)
{
  g_autofree char *result = NULL;

  /* Alphanumerics should remain unchanged, non-alnum must be \uXXXX (hex, 4 digits, no semicolon). */
  result = ephy_encode_for_js_quoted_data_value ("{\"key\": \"val\"}");
  g_assert_cmpstr (result, ==, "\\u007b\\u0022key\\u0022\\u003a\\u0020\\u0022val\\u0022\\u007d");
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/lib/ephy-output-encoding/html", test_encode_for_html);
  g_test_add_func ("/lib/ephy-output-encoding/html-attribute", test_encode_for_html_attribute);
  g_test_add_func ("/lib/ephy-output-encoding/js-quoted-data-value", test_encode_for_js_quoted_data_value);

  ret = g_test_run ();

  return ret;
}
