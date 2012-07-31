/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2012 - Igalia S.L.
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
#include "ephy-embed-prefs.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-shell.h"

#include <gtk/gtk.h>

/* TODO: don't hardcode the number of encodings in ephy-encodings.c here! */
#define NUM_ENCODINGS 78

static void
test_ephy_encodings_get ()
{
  EphyEncodings *encodings;
  GList *all, *p;

  encodings = EPHY_ENCODINGS (ephy_embed_shell_get_encodings (embed_shell));
  g_assert (encodings);

  all = ephy_encodings_get_all (encodings);
  g_assert (all);
  g_assert_cmpint (g_list_length (all), ==, NUM_ENCODINGS);

  for (p = all; p; p = p->next) {
    EphyEncoding *encoding = EPHY_ENCODING (p->data);
    g_assert (encoding);
    g_assert (EPHY_IS_ENCODING (encoding));
  }

  g_list_free (all);
}

int
main (int argc, char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();
  ephy_embed_prefs_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_assert (ephy_shell);

  g_test_add_func ("/src/ephy-encodings/get",
                   test_ephy_encodings_get);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();
  g_object_unref (ephy_shell);

  return ret;
}
