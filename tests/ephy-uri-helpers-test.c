/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-url-helpers-test.c
 * This file is part of Epiphany
 *
 * Copyright © 2013 Bastien Nocera <hadess@hadess.net>
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
#include "ephy-debug.h"
#include "ephy-uri-helpers.h"
#include "ephy-settings.h"

#include <glib.h>
#include <gtk/gtk.h>

static void
test_ephy_uri_helpers_remove_tracking (void)
{
  struct {
    const char *input;
    const char *output;
  } const items[] = {
    { "http://www.test.com/", "http://www.test.com/" },
    { "http://www.test.com/?key=foo", "http://www.test.com/?key=foo" },
    /* From the description in https://addons.mozilla.org/fr/firefox/addon/pure-url/ */
    { "http://bigpicture.ru/?p=431513&utm_source=feedburner&utm_medium=feed&utm_campaign=Feed%%3A+bigpictures+%%28%%D0%%9D%%D0%%9E%%D0%%92%%D0%%9E%%D0%%A1%%D0%%A2%%D0%%98+%%D0%%92+%%D0%%A4%%D0%%9E%%D0%%A2%%D0%%9E%%D0%%93%%D0%%A0%%D0%%90%%D0%%A4%%D0%%98%%D0%%AF%%D0%%A5%%29", "http://bigpicture.ru/?p=431513" },
    { "http://www.test.com/?utm_source=feedburner", "http://www.test.com/" },
    { "http://www.test.com/?feature=foo", "http://www.test.com/?feature=foo" },
    { "http://foo.youtube.com/?feature=foo", "http://foo.youtube.com/" },
    /* https://bugzilla.gnome.org/show_bug.cgi?id=724724 */
    { "http://git.savannah.gnu.org/gitweb/?p=grep.git;a=commit;h=97318f5e59a1ef6feb8a378434a00932a3fc1e0b",
      "http://git.savannah.gnu.org/gitweb/?p=grep.git;a=commit;h=97318f5e59a1ef6feb8a378434a00932a3fc1e0b"},
    /* https://bugzilla.gnome.org/show_bug.cgi?id=730464 */
    { "https://mail.google.com/mail/u/0/?ui=2&ik=37373eb942&rid=7cea..&auto=1&view=lno&_reqid=1168127&pcd=1&mb=0&rt=j",
      "https://mail.google.com/mail/u/0/?ui=2&ik=37373eb942&rid=7cea..&auto=1&view=lno&_reqid=1168127&pcd=1&mb=0&rt=j" },
    { "http://www.test.com/?utm_source=feedburner&view=lno&_reqid=1234", "http://www.test.com/?view=lno&_reqid=1234" },
    { "http://www.test.com/?some&valid&query", "http://www.test.com/?some&valid&query" },
    { "http://www.test.com/?utm_source=feedburner&some&valid&query", "http://www.test.com/?some&valid&query" },

  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (items); i++) {
    char *result;

    g_test_message ("TRACKING: uri: %s; expected: %s;",
                    items[i].input, items[i].output);
    result = ephy_remove_tracking_from_uri (items[i].input);
    if (result == NULL)
      result = g_strdup (items[i].input);
    g_assert_cmpstr (items[i].output, ==, result);
    g_free (result);
  }
}

int
main (int argc, char *argv[])
{
  int ret;

  /* This should affect only this test, we use it so we can test for
   * default directory changes. */
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  g_test_add_func ("/lib/ephy-uri-helpers/remove_tracking",
                   test_ephy_uri_helpers_remove_tracking);

  ret = g_test_run ();

  return ret;
}
