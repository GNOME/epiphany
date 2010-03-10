/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 * testephyembedpersist.c
 * This file is part of Epiphany
 *
 * Copyright © 2010 - Igalia S.L.
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
#include "ephy-embed-single.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-stock-icons.h"
#include <gtk/gtk.h>

static void
test_embed_single_new ()
{
  EphyEmbedSingle *single;

  single = EPHY_EMBED_SINGLE (g_object_new (EPHY_TYPE_EMBED_SINGLE, NULL));
  g_assert (EPHY_IS_EMBED_SINGLE (single));

  g_object_unref (single);
}

static void
test_embed_single_get_from_shell ()
{
  EphyEmbedSingle *single;

  single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
  g_assert (EPHY_IS_EMBED_SINGLE (single));
}

static void
test_embed_single_network_status ()
{
  EphyEmbedSingle *single;

  single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
  g_assert (EPHY_IS_EMBED_SINGLE (single));

  /* Defaults to TRUE */
  g_assert (ephy_embed_single_get_network_status (single));

  ephy_embed_single_set_network_status (single, FALSE);
  g_assert (ephy_embed_single_get_network_status (single) == FALSE);
}

static void
test_embed_single_form_auth ()
{
  EphyEmbedSingle *single;
  GSList *results = NULL;

  single = EPHY_EMBED_SINGLE (g_object_new (EPHY_TYPE_EMBED_SINGLE, NULL));
  g_assert (EPHY_IS_EMBED_SINGLE (single));

  results = ephy_embed_single_get_form_auth (single, "gnome.org");
  g_assert_cmpint (g_slist_length (results), ==, 0);

  ephy_embed_single_add_form_auth (single, "gnome.org",
                                   "form_username_field", "form_password_field",
                                   "username");

  results = ephy_embed_single_get_form_auth (single, "gnome.org");
  g_assert_cmpint (g_slist_length (results), ==, 1);

  results = ephy_embed_single_get_form_auth (single, "www.gnome.org");
  g_assert_cmpint (g_slist_length (results), ==, 0);

  g_object_unref (single);
}

int
main (int argc, char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);
  g_thread_init (NULL);

  ephy_debug_init ();
  ephy_embed_prefs_init ();
  _ephy_shell_create_instance ();

  if (!ephy_file_helpers_init (NULL, TRUE, FALSE, NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/embed/ephy-embed-single/new",
                   test_embed_single_new);
  g_test_add_func ("/embed/ephy-embed-single/get_from_shell",
                   test_embed_single_get_from_shell);
  g_test_add_func ("/embed/ephy-embed-single/network_status",
                   test_embed_single_network_status);
  g_test_add_func ("/embed/ephy-embed-single/form_auth",
                   test_embed_single_form_auth);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  return ret;
}
