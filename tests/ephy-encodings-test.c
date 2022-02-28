/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 - Igalia S.L.
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
#include "ephy-embed-prefs.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-settings.h"
#include "ephy-shell.h"

#include <gtk/gtk.h>
#include <locale.h>

/* TODO: don't hardcode the number of encodings in ephy-encodings.c here! */
#define NUM_ENCODINGS 78

static void
test_ephy_encodings_create (void)
{
  EphyEncoding *encoding;

  encoding = ephy_encoding_new ("UTF-8", "Unicode (UTF-8)",
                                LG_UNICODE);
  g_assert_nonnull (encoding);
  g_assert_cmpstr (ephy_encoding_get_encoding (encoding), ==, "UTF-8");
  g_assert_cmpstr (ephy_encoding_get_title (encoding), ==, "Unicode (UTF-8)");
  g_assert_cmpstr (ephy_encoding_get_title_elided (encoding), ==, "Unicode (UTF-8)");
  g_assert_cmpstr (ephy_encoding_get_collation_key (encoding), ==, "\311\263\307\276\307\205\305\271\310\215\306\202\306\216\311\263\311\250\306\245\305\237\001\035\035\035\035\035\035\035\035\035\035\035\001\007\002\002\002\002\002\002\007\007\007\002\001\001\346\244\273\001\346\215\264\001\346\211\243\001\346\204\235\001\346\216\257\001\346\205\217\001\346\206\212\001\343\265\233\001\343\265\242\001\346\244\273\001\346\244\231\001\346\234\257\001\343\265\247\001\345\277\241\001\343\265\243");
  g_assert_cmpint (ephy_encoding_get_language_groups (encoding), ==, LG_UNICODE);

  g_object_unref (encoding);
}

static void
test_ephy_encodings_get (void)
{
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();
  EphyEncodings *encodings;
  GList *all, *p;

  encodings = ephy_embed_shell_get_encodings (embed_shell);
  g_assert_nonnull (encodings);

  all = ephy_encodings_get_all (encodings);
  g_assert_nonnull (all);
  g_assert_cmpint (g_list_length (all), ==, NUM_ENCODINGS);

  for (p = all; p; p = p->next) {
    EphyEncoding *encoding = EPHY_ENCODING (p->data);
    g_assert_nonnull (encoding);
    g_assert_true (EPHY_IS_ENCODING (encoding));
  }

  g_list_free (all);
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);
  setlocale (LC_ALL, "");

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_assert_nonnull (ephy_shell_get_default ());

  g_test_add_func ("/src/ephy-encodings/create",
                   test_ephy_encodings_create);

  g_test_add_func ("/src/ephy-encodings/get",
                   test_ephy_encodings_get);

  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}
