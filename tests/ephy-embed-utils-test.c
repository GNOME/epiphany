/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-embed-utils-test.c
 * This file is part of Epiphany
 *
 * Copyright © 2012 Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ephy-embed-utils.h"
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

static const SchemeTest tests_has_scheme[] = {
  { "http", "http://www.gnome.org/" },
  { "http_with_port", "http://www.gnome.org:8080" },
  { "https", "https://www.gnome.org/" },
  { "http_caps", "HTTP://www.gnome.org/" },
  { "ftp_with_user", "ftp://rupert:bananas@ftp.gnome.org/epiphany/" },
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
  { "append_http_to_domain", "gnome.org", "http://gnome.org" },
  { "append_http_to_www", "www.gnome.org", "http://www.gnome.org" },
  { "append_http_to_hostname", "gnome", "http://gnome" },
  { "append_http_to_hostname_with_port", "localhost:8080", "http://localhost:8080" },
  { "append_http_to_ip_address", "192.168.0.1", "http://192.168.0.1" },
  { "append_http_to_ip_address_with_port", "192.168.0.1:8080", "http://192.168.0.1:8080" },
  { "convert_about_to_ephy_about", "about:epiphany", "ephy-about:epiphany" },
  { "untouched_http", "http://gnome.org", "http://gnome.org" },
};

static void
test_address_no_web_scheme (const char *test)
{
  g_assert (ephy_embed_utils_address_has_web_scheme (test) == FALSE);
}

static void
test_address_has_web_scheme (const char *test)
{
  g_assert (ephy_embed_utils_address_has_web_scheme (test) == TRUE);
}

static void
test_normalize_address (const NormalizeTest *test)
{
  char *normalized;

  normalized = ephy_embed_utils_normalize_address (test->test);

  g_assert_cmpstr (test->result, ==, normalized);
  g_free (normalized);
}

int
main (int argc, char *argv[])
{
  int i;
  gtk_test_init (&argc, &argv);

  for (i = 0; i < G_N_ELEMENTS (tests_has_scheme); i++) {
    SchemeTest test;
    char *test_name;

    test = tests_has_scheme[i];
    test_name = g_strconcat ("/embed/ephy-embed-utils/has_web_scheme_",
                             test.name, NULL);

    g_test_add_data_func (test_name, test.test,
                          (GTestDataFunc) test_address_has_web_scheme);

    g_free (test_name);
  }

  for (i = 0; i < G_N_ELEMENTS (tests_no_scheme); i++) {
    SchemeTest test;
    char *test_name;

    test = tests_no_scheme[i];
    test_name = g_strconcat ("/embed/ephy-embed-utils/no_web_scheme_",
                             test.name, NULL);

    g_test_add_data_func (test_name, test.test,
                          (GTestDataFunc) test_address_no_web_scheme);

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

  return g_test_run ();
}
