/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "ephy-string.h"

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
  char *uri;
  char *hostname;
} HostnameTest;

static const HostnameTest hostname_tests[] = {
  { "http://www.google.com", "www.google.com" },
  { "http://www.google.com/this/is/a/path", "www.google.com" },
  { "www.google.com", "www.google.com" },
  { "google.com", "google.com" },
  { "file:///tmp/", NULL },
  { "about:blank", NULL },
  { "ephy-about:applications", NULL },
  { NULL, NULL },
  { "garbage garbage", "garbage garbage" } /* FIXME: should this be NULL? */
};

static void
test_ephy_string_get_hostname (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (hostname_tests); i++) {
    char *host;
    HostnameTest test = hostname_tests[i];

    host = ephy_string_get_host_name (test.uri);
    g_assert_cmpstr (host, ==, test.hostname);
    g_free (host);
  }
}

int
main (int argc, char *argv[])
{
  gboolean ret;

  gtk_test_init (&argc, &argv);

  g_test_add_func ("/lib/ephy-string/get_hostname",
                   test_ephy_string_get_hostname);

  ret = g_test_run ();

  return ret;
}
