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

#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
  const char *name;
  const char *test;
} SchemeTest;

typedef struct {
  const char *name;
  const char *test;
  const char *result;
} NormalizeTest;

typedef struct {
  const char *name;
  const char *test;
  gboolean result;
} IsEmptyTest;

static const SchemeTest tests_has_scheme[] = {
  { "http", "http://www.gnome.org/" },
  { "http_with_port", "http://www.gnome.org:8080" },
  { "https", "https://www.gnome.org/" },
  { "http_caps", "HTTP://www.gnome.org/" },
  { "file", "file:///home/epiphany/code" },
  { "javascript", "javascript:var a=b;" },
  { "data", "data:\%20\%40" },
  { "about", "about:epiphany" },
  { "ephy-about", "ephy-about:memory" },
  { "gopher", "gopher://gnome.org/" },
};

static const SchemeTest tests_no_scheme[] = {
  { "mailto", "mailto:rupert@gnome.org" },
  { "hostname", "localhost" },
  { "hostname_with_port", "localhost:8080" },
  { "ip_address", "192.168.0.1" },
  { "ip_address_with_port", "192.168.0.1:8080" },
  { "http_no_double_colon", "http//www.gnome.org/" },
#if 0
  { "double_colon_first", ":var a=b;" },
#endif
  { "double_double_colon", "epip:hany-about:memory" },
  { "slashes_first", "//localhost:8080" },
  { "unknown_scheme", "pher://gnome.org/" },
  { "unknown_scheme_with_user", "whatever://rup:ert@gnome.org" },
  { "empty_string", "" },
};

static const NormalizeTest tests_normalize[] = {
  { "append_file_to_path", "/etc/passwd", "file:///etc/passwd" },
  { "append_https_to_domain", "gnome.org", "https://gnome.org" },
  { "append_https_to_www", "www.gnome.org", "https://www.gnome.org" },
  { "append_https_to_hostname", "gnome", "https://gnome" },
  { "append_https_to_hostname_with_port", "localhost:8080", "https://localhost:8080" },
  { "append_https_to_ip_address", "192.168.0.1", "https://192.168.0.1" },
  { "append_https_to_ip_address_with_port", "192.168.0.1:8080", "https://192.168.0.1:8080" },
  { "convert_about_to_ephy_about", "about:epiphany", "ephy-about:epiphany" },
  { "untouched_http", "http://gnome.org", "http://gnome.org" },
};

static const IsEmptyTest tests_is_empty[] = {
  { "NULL", NULL, TRUE },
  { "zero-length", "", TRUE },
  { "about:blank", "about:blank", TRUE },
  { "about:blanco", "about:blanco", FALSE },
  { "non-blank-URI", "http://www.gnome.org", FALSE },
  { "random", "what is this, I don't even", FALSE },
  { "overview", "ephy-about:overview", TRUE },
  { "incognito", "ephy-about:incognito", TRUE }
};

static void
test_address_no_web_scheme (const char *test)
{
  g_assert_false (ephy_embed_utils_address_has_web_scheme (test));
}

static void
test_address_has_web_scheme (const char *test)
{
  g_assert_true (ephy_embed_utils_address_has_web_scheme (test));
}

static void
test_normalize_address (const NormalizeTest *test)
{
  char *normalized;

  normalized = ephy_embed_utils_normalize_address (test->test);

  g_assert_cmpstr (test->result, ==, normalized);
  g_free (normalized);
}

static void
test_is_empty (const IsEmptyTest *test)
{
  g_assert_true (ephy_embed_utils_url_is_empty (test->test) == test->result);
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  guint i;
  gtk_test_init (&argc, &argv);

  ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_TESTING_MODE, NULL);
  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);

  for (i = 0; i < G_N_ELEMENTS (tests_has_scheme); i++) {
    SchemeTest test;
    char *test_name;

    test = tests_has_scheme[i];
    test_name = g_strconcat ("/embed/ephy-embed-utils/has_web_scheme_",
                             test.name, NULL);

    g_test_add_data_func (test_name, test.test,
                          (GTestDataFunc)test_address_has_web_scheme);

    g_free (test_name);
  }

  for (i = 0; i < G_N_ELEMENTS (tests_no_scheme); i++) {
    SchemeTest test;
    char *test_name;

    test = tests_no_scheme[i];
    test_name = g_strconcat ("/embed/ephy-embed-utils/no_web_scheme_",
                             test.name, NULL);

    g_test_add_data_func (test_name, test.test,
                          (GTestDataFunc)test_address_no_web_scheme);

    g_free (test_name);
  }

  for (i = 0; i < G_N_ELEMENTS (tests_normalize); i++) {
    NormalizeTest test;
    char *test_name;

    test = tests_normalize[i];
    test_name = g_strconcat ("/embed/ephy-embed-utils/normalize_",
                             test.name, NULL);

    g_test_add_data_func (test_name, tests_normalize + i,
                          (GTestDataFunc)test_normalize_address);

    g_free (test_name);
  }

  for (i = 0; i < G_N_ELEMENTS (tests_is_empty); i++) {
    IsEmptyTest test;
    char *test_name;

    test = tests_is_empty[i];
    test_name = g_strconcat ("/embed/ephy-embed-utils/is_empty_",
                             test.name, NULL);

    g_test_add_data_func (test_name, tests_is_empty + i,
                          (GTestDataFunc)test_is_empty);

    g_free (test_name);
  }

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  return ret;
}
