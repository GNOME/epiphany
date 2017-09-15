/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include "ephy-gsb-utils.h"

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
  const char *url_raw;
  const char *url_canonical;
} CanonicalizeTest;

/*
 * Tests from: https://developers.google.com/safe-browsing/v4/urls-hashing#canonicalization
 */
static const CanonicalizeTest canonicalize_tests[] = {
  {"http://host/%25%32%35", "http://host/%25"},
  {"http://host/%25%32%35%25%32%35", "http://host/%25%25"},
  {"http://host/%2525252525252525", "http://host/%25"},
  {"http://host/asdf%25%32%35asd", "http://host/asdf%25asd"},
  {"http://host/%%%25%32%35asd%%", "http://host/%25%25%25asd%25%25"},
  {"http://www.google.com/", "http://www.google.com/"},
  {"http://%31%36%38%2e%31%38%38%2e%39%39%2e%32%36/%2E%73%65%63%75%72%65/%77%77%77%2E%65%62%61%79%2E%63%6F%6D/", "http://168.188.99.26/.secure/www.ebay.com/"},
  {"http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/", "http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/"},
  {"http://host%23.com/%257Ea%2521b%2540c%2523d%2524e%25f%255E00%252611%252A22%252833%252944_55%252B", "http://host%23.com/~a!b@c%23d$e%25f^00&11*22(33)44_55+"},
  {"http://3279880203/blah", "http://195.127.0.11/blah"},
  {"http://www.google.com/blah/..", "http://www.google.com/"},
  {"www.google.com/", "http://www.google.com/"},
  {"www.google.com", "http://www.google.com/"},
  {"http://www.evil.com/blah#frag", "http://www.evil.com/blah"},
  {"http://www.GOOgle.com/", "http://www.google.com/"},
  {"http://www.google.com.../", "http://www.google.com/"},
  {"http://www.google.com/foo\tbar\rbaz\n2", "http://www.google.com/foobarbaz2"},
  {"http://www.google.com/q?", "http://www.google.com/q?"},
  {"http://www.google.com/q?r?", "http://www.google.com/q?r?"},
  {"http://www.google.com/q?r?s", "http://www.google.com/q?r?s"},
  {"http://evil.com/foo#bar#baz", "http://evil.com/foo"},
  {"http://evil.com/foo;", "http://evil.com/foo;"},
  {"http://evil.com/foo?bar;", "http://evil.com/foo?bar;"},
  {"http://\x01\x80.com/", "http://%01%80.com/"},
  {"http://notrailingslash.com", "http://notrailingslash.com/"},
  {"http://www.gotaport.com:1234/", "http://www.gotaport.com/"},
  {"  http://www.google.com/  ", "http://www.google.com/"},
  {"http:// leadingspace.com/", "http://%20leadingspace.com/"},
  {"http://%20leadingspace.com/", "http://%20leadingspace.com/"},
  {"%20leadingspace.com/", "http://%20leadingspace.com/"},
  {"https://www.securesite.com/", "https://www.securesite.com/"},
  {"http://host.com/ab%23cd", "http://host.com/ab%23cd"},
  {"http://host.com//twoslashes?more//slashes", "http://host.com/twoslashes?more//slashes"}
};

static void
test_ephy_gsb_utils_canonicalize (void)
{
  for (guint i = 0; i < G_N_ELEMENTS (canonicalize_tests); i++) {
    CanonicalizeTest test = canonicalize_tests[i];
    char *url_canonical;

    url_canonical = ephy_gsb_utils_canonicalize (test.url_raw, NULL, NULL, NULL);
    g_assert_cmpstr (url_canonical, ==, test.url_canonical);

    g_free (url_canonical);
  }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/lib/safe-browsing/test_ephy_gsb_utils_canonicalize",
                   test_ephy_gsb_utils_canonicalize);

  return g_test_run ();
}
