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
#include "ephy-bookmarks.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-profile-utils.h"
#include "ephy-shell.h"

const char* bookmarks_paths[] = { EPHY_BOOKMARKS_FILE, EPHY_BOOKMARKS_FILE_RDF };

static void
clear_bookmark_files (void)
{
  GFile *file;
  char *path;
  int i;

  for (i = 0; i < G_N_ELEMENTS (bookmarks_paths); i++) {

    path = g_build_filename (ephy_dot_dir (),
                             bookmarks_paths[i],
                             NULL);
    file = g_file_new_for_path (path);
    g_file_delete (file, NULL, NULL);
    g_object_unref (file);
    g_free (path);
  }
}

static void
test_ephy_bookmarks_create (void)
{
  EphyBookmarks *bookmarks;

  bookmarks = ephy_bookmarks_new ();
  g_assert (bookmarks);
  g_object_unref (bookmarks);

  clear_bookmark_files ();
}

static void
test_ephy_bookmarks_add (void)
{
  EphyBookmarks *bookmarks;
  EphyNode *node, *result;

  bookmarks = ephy_bookmarks_new ();
  g_assert (bookmarks);
  
  node = ephy_bookmarks_add (bookmarks, "GNOME", "http://www.gnome.org");
  g_assert (node);
  result = ephy_bookmarks_find_bookmark (bookmarks, "http://www.gnome.org");
  g_assert (node == result);
  
  g_object_unref (bookmarks);

  clear_bookmark_files ();
}

static void
test_ephy_bookmarks_set_address (void)
{
  EphyBookmarks *bookmarks;
  EphyNode *node;

  bookmarks = ephy_bookmarks_new ();
  g_assert (bookmarks);
  node = ephy_bookmarks_add (bookmarks, "GNOME", "http://www.gnome.org");
  g_assert (node);
  ephy_bookmarks_set_address (bookmarks, node, "http://www.google.com");
  node = ephy_bookmarks_find_bookmark (bookmarks, "http://www.gnome.org");
  g_assert (node == NULL);
  node = ephy_bookmarks_find_bookmark (bookmarks, "http://www.google.com");
  g_assert (node);

  g_object_unref (bookmarks);
  clear_bookmark_files ();
}

int
main (int argc, char *argv[])
{
  gboolean ret;

  gtk_test_init (&argc, &argv);
  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);

  g_test_add_func ("/src/bookmarks/ephy-bookmarks/create",
                   test_ephy_bookmarks_create);

  g_test_add_func ("/src/bookmarks/ephy-bookmarks/add",
                   test_ephy_bookmarks_add);

  g_test_add_func ("/src/bookmarks/ephy-bookmarks/set_address",
                   test_ephy_bookmarks_set_address);

  ret = g_test_run ();

  return ret;
}
