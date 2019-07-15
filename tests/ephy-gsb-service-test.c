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

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-gsb-service.h"
#include "ephy-gsb-utils.h"

#include <glib.h>
#include <glib/gstdio.h>
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

typedef struct {
  const char *url;
  guint num_hashes;
  const char *hashes_hex[64];
} ComputeHashesTest;

/*
 * Tests from: https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions
 */
static const ComputeHashesTest compute_hashes_tests[] = {
  {
    "http://a.b.c/1/2.html?param=1",
    8,
    {
      "1cd5cf5ed8e6df424bdbb400f7b2a3fcb215c4c3f7fa2965a11446cde3c162f3",
      "8b19a5a51125f023af4a26e2aef4caae352623d05ffdc859433be84823ec4053",
      "f9c142c4c0c9e669e0924b45f5b1b8dd1fdf85d182b674a4ec415b1f58ac2667",
      "59e650c465d9cbded1f95322e19fb1481f9500342a240c4a18a7a5ef4b103e1c",
      "9b7d85bbdfa3c8ba1796a96ea91094730350c8b12a9552028123b1cc1918cc56",
      "1803dee47cc6adec025aefd26ff5b44408f14d6e250defe7d0ae2444f0f8e106",
      "b225cf5dcf266f3ff0b32319a72cf23fca7c53c98cb4af1a7bbfe413415407f1",
      "ac5f446d55d0807d211e05fd5482534b0dc99d7b9f255174f9dba30b9ebc01ac"
    }
  },
  {
    "http://a.b.c.d.e.f.g/1.html",
    10,
    {
      "8c39d0c311331cfae87867aa52a98ef3c995b121c0f7bc750164996a4b3ab43f",
      "ce385c58c19493d2e4ac23fbb1d4faccde65b73bfcc4f3b6ba62addf905fbf41",
      "37a343cf5d2e00eeb103175c8e4b0adddbef6348f6c60e732a4952fc0a053d89",
      "f1930a298cf214f0459049ad655838b080a9ba886dd0c759e21c8af005528d14",
      "0285b5d5ad2aa12ff24d0fc9ac820725061a659fdd369857a422cfe4cbb04e4e",
      "4fd37f62520c129f29525fd3d1eb9b04511b632e4aef190dbc23f8519d7ccd7e",
      "a5a5563280f2da618e8a6b14060d909679446767c7d3bbcc23c9b02419b12289",
      "4e378632a186388136b13689a85bf63d2f8fcf50c93b1468c4e20cd12423f2f8",
      "e42d99efd820eeb6fad77109534a6af1b5cb6bd7755958fead91e0790850a303",
      "9401530ee6371f3f1cb82e463223e7bf5fd3ab8b85872d477509110467b4c9e1"
    }
  },
  {
    "http://1.2.3.4/1/",
    2,
    {
      "5c9f354119e8d3f82e1bc01545ec7a656da70453e6bfc053ac8b257bdd4d8ef6",
      "3f008b863ca6e954c31859665454f9cbcb10760acb7ebc536d6da1ccac94618d"
    }
  }
};

static char *
bytes_to_hex (const guint8 *bytes,
              gsize         length)
{
  const char *hex_digits = "0123456789abcdef";
  char *hex;

  hex = g_malloc (length * 2 + 1);
  for (gsize i = 0; i < length; i++) {
    hex[2 * i] = hex_digits[bytes[i] >> 4];
    hex[2 * i + 1] = hex_digits[bytes[i] & 0xf];
  }
  hex[length * 2] = 0;

  return hex;
}

static void
test_ephy_gsb_utils_compute_hashes (void)
{
  for (guint i = 0; i < G_N_ELEMENTS (compute_hashes_tests); i++) {
    ComputeHashesTest test = compute_hashes_tests[i];
    GList *hashes, *h;

    h = hashes = ephy_gsb_utils_compute_hashes (test.url);
    g_assert_cmpuint (g_list_length (hashes), ==, test.num_hashes);

    for (guint k = 0; k < test.num_hashes; k++, h = h->next) {
      char *hash_hex = bytes_to_hex (g_bytes_get_data (h->data, NULL),
                                     g_bytes_get_size (h->data));
      g_assert_cmpstr (hash_hex, ==, test.hashes_hex[k]);
      g_free (hash_hex);
    }

    g_list_free_full (hashes, (GDestroyNotify)g_bytes_unref);
  }
}

typedef struct {
  const char *url;
  gboolean is_threat;
} VerifyURLTest;

static const VerifyURLTest verify_url_tests[] = {
  {"https://testsafebrowsing.appspot.com/apiv4/LINUX/MALWARE/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/LINUX/SOCIAL_ENGINEERING/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/LINUX/UNWANTED_SOFTWARE/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/ANY_PLATFORM/MALWARE/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/ANY_PLATFORM/SOCIAL_ENGINEERING/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/ANY_PLATFORM/UNWANTED_SOFTWARE/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/WINDOWS/MALWARE/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/WINDOWS/SOCIAL_ENGINEERING/URL/", TRUE},
  {"https://testsafebrowsing.appspot.com/apiv4/WINDOWS/UNWANTED_SOFTWARE/URL/", TRUE},
};

static GMainLoop *test_verify_url_loop;
static int test_verify_url_counter;

static void
test_verify_url_cb (EphyGSBService *service,
                    GAsyncResult   *result,
                    gpointer        user_data)
{
  gboolean is_threat = GPOINTER_TO_UINT (user_data);
  GList *threats = ephy_gsb_service_verify_url_finish (service, result);

  g_assert_true ((threats != NULL) == is_threat);

  g_list_free_full (threats, g_free);

  if (g_atomic_int_dec_and_test (&test_verify_url_counter))
    g_main_loop_quit (test_verify_url_loop);
}

static void
gsb_service_update_finished_cb (EphyGSBService *service,
                                gpointer        user_data)
{
  for (guint i = 0; i < G_N_ELEMENTS (verify_url_tests); i++) {
    VerifyURLTest test = verify_url_tests[i];

    ephy_gsb_service_verify_url (service,
                                 test.url,
                                 (GAsyncReadyCallback)test_verify_url_cb,
                                 GUINT_TO_POINTER (test.is_threat));
  }
}

static void
test_ephy_gsb_service_verify_url (void)
{
  EphyGSBService *service;
  char *db_path;

  db_path = g_build_filename (g_get_tmp_dir (), "gsb-threats-test.db", NULL);
  if (g_file_test (db_path, G_FILE_TEST_IS_REGULAR))
    g_unlink (db_path);

  /* Note that this test takes a bit longer to execute because we have to wait
   * for the temporary threats database to be populated with data from the server.
   */
  service = ephy_gsb_service_new ("AIzaSyAtuURrRblYXvwCyDC5ZFq0mEw1x4VN6KA", db_path);
  g_signal_connect (service, "update-finished",
                    G_CALLBACK (gsb_service_update_finished_cb), NULL);

  test_verify_url_counter = G_N_ELEMENTS (verify_url_tests);
  test_verify_url_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (test_verify_url_loop);

  g_assert_cmpint (g_unlink (db_path), ==, 0);

  g_free (db_path);
  g_object_unref (service);
  g_main_loop_unref (test_verify_url_loop);
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  GError *error = NULL;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_TESTING_MODE, &error);
  if (error) {
    g_debug ("ephy_file_helpers_init() failed: %s\n", error->message);
    g_error_free (error);
    return -1;
  }

  g_test_add_func ("/lib/safe-browsing/test_ephy_gsb_utils_canonicalize",
                   test_ephy_gsb_utils_canonicalize);
  g_test_add_func ("/lib/safe-browsing/test_ephy_gsb_utils_compute_hashes",
                   test_ephy_gsb_utils_compute_hashes);
  g_test_add_func ("/lib/safe-browsing/test_ephy_gsb_service_verify_url",
                   test_ephy_gsb_service_verify_url);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  return ret;
}
