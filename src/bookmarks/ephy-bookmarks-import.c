/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gnome.org>
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

#include "ephy-bookmarks-import.h"

#include "config.h"

#include "gvdb-builder.h"
#include "gvdb-reader.h"

#include <glib/gi18n.h>

GQuark bookmarks_import_error_quark (void);
G_DEFINE_QUARK (bookmarks-import-error-quark, bookmarks_import_error)
#define BOOKMARKS_IMPORT_ERROR bookmarks_import_error_quark ()

typedef enum {
  BOOKMARKS_IMPORT_ERROR_TAGS = 1001,
  BOOKMARKS_IMPORT_ERROR_BOOKMARKS = 1002
} BookmarksImportErrorCode;

static GSequence *
get_bookmarks_from_table (GvdbTable *table)
{
  GSequence *bookmarks = NULL;
  char **list = NULL;
  int length;
  int i;

  bookmarks = g_sequence_new (g_object_unref);

  /* Iterate over all keys (url's) in the table. */
  list = gvdb_table_get_names (table, &length);
  for (i = 0; i < length; i++) {
    EphyBookmark *bookmark;
    GVariant *value;
    GVariantIter *iter;
    GSequence *tags;
    char *tag;
    const char *title;
    gint64 time_added;
    char *id;
    double modified;
    gboolean uploaded;

    /* Obtain the correspoding GVariant. */
    value = gvdb_table_get_value (table, list[i]);

    g_variant_get (value, "(x&s&sdbas)", &time_added, &title, &id, &modified, &uploaded, &iter);

    /* Add all stored tags in a GSequence. */
    tags = g_sequence_new (g_free);
    while (g_variant_iter_next (iter, "s", &tag)) {
      g_sequence_insert_sorted (tags, tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);
    }
    g_variant_iter_free (iter);

    /* Create the new bookmark. */
    bookmark = ephy_bookmark_new (list[i], title, tags);
    ephy_bookmark_set_time_added (bookmark, time_added);
    ephy_bookmark_set_id (bookmark, id);
    ephy_bookmark_set_modification_time (bookmark, modified);
    ephy_bookmark_set_is_uploaded (bookmark, uploaded);
    g_sequence_prepend (bookmarks, bookmark);

    g_variant_unref (value);
  }

  g_strfreev (list);

  return bookmarks;
}

gboolean
ephy_bookmarks_import (EphyBookmarksManager  *manager,
                       const char            *filename,
                       GError               **error)
{
  GvdbTable *root_table = NULL;
  GvdbTable *table = NULL;
  GSequence *bookmarks = NULL;
  char **list = NULL;
  gboolean res = TRUE;
  int length;
  int i;

  /* Create a new table to hold data stored in file. */
  root_table = gvdb_table_new (filename, TRUE, error);
  if (!root_table) {
    res = FALSE;
    goto out;
  }

  /* Add tags to the bookmark manager's sequence. */
  table = gvdb_table_get_table (root_table, "tags");
  if (!table) {
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_TAGS,
                 _("File is not a valid Epiphany bookmarks file: missing tags table"));
    res = FALSE;
    goto out;
  }

  /* Iterate over all keys (url's) in the table. */
  list = gvdb_table_get_names (table, &length);
  for (i = 0; i < length; i++)
    ephy_bookmarks_manager_create_tag (manager, list[i]);
  g_strfreev (list);
  gvdb_table_free (table);

  /* Get bookmarks table */
  table = gvdb_table_get_table (root_table, "bookmarks");
  if (!table) {
    g_set_error (error,
                 BOOKMARKS_IMPORT_ERROR,
                 BOOKMARKS_IMPORT_ERROR_BOOKMARKS,
                 _("File is not a valid Epiphany bookmarks file: missing bookmarks table"));
    res = FALSE;
    goto out;
  }

  bookmarks = get_bookmarks_from_table (table);
  ephy_bookmarks_manager_add_bookmarks (manager, bookmarks);

  out:
    if (table)
      gvdb_table_free (table);
    if (bookmarks)
      g_sequence_free (bookmarks);
    if (root_table)
      gvdb_table_free (root_table);

  return res;
}
