/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2017 Igalia S.L.
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

#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-string.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <sys/stat.h>
#include <sys/types.h>

static GtkFileFilter *
ephy_file_dialog_create_pattern_filter (const char *title,
                                        const char *first_pattern,
                                        ...)
{
  GtkFileFilter *filth;
  va_list args;
  const char *pattern;

  filth = gtk_file_filter_new ();

  va_start (args, first_pattern);

  pattern = first_pattern;
  while (pattern != NULL) {
    gtk_file_filter_add_pattern (filth, pattern);
    pattern = va_arg (args, const char *);
  }
  va_end (args);

  gtk_file_filter_set_name (filth, title);

  return filth;
}

static GtkFileFilter *
ephy_file_dialog_create_mime_filter (const char *title,
                                     const char *first_mimetype,
                                     ...)
{
  GtkFileFilter *filth;
  va_list args;
  const char *mimetype;

  filth = gtk_file_filter_new ();

  va_start (args, first_mimetype);

  mimetype = first_mimetype;
  while (mimetype != NULL) {
    gtk_file_filter_add_mime_type (filth, mimetype);
    mimetype = va_arg (args, const char *);
  }
  va_end (args);

  gtk_file_filter_set_name (filth, title);

  return filth;
}

void
ephy_file_dialog_add_shortcuts (GtkFileDialog *dialog)
{
  g_autofree char *downloads_dir_path = NULL;
  g_autoptr (GFile) downloads_dir = NULL;
  g_autoptr (GListStore) shortcuts = NULL;

  g_assert (GTK_IS_FILE_DIALOG (dialog));

  downloads_dir_path = ephy_file_get_downloads_dir ();
  downloads_dir = g_file_new_for_path (downloads_dir_path);

  shortcuts = g_list_store_new (G_TYPE_FILE);
  g_list_store_append (shortcuts, downloads_dir);

  gtk_file_dialog_set_shortcut_folders (dialog, G_LIST_MODEL (shortcuts));
}

void
ephy_file_dialog_add_filters (GtkFileDialog         *dialog,
                              EphyFileFilterDefault  default_filter)
{
  GtkFileFilter *filter[EPHY_FILE_FILTER_LAST];
  g_autoptr (GListStore) filters = NULL;
  int i;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (default_filter >= 0 && default_filter < EPHY_FILE_FILTER_LAST);

  filter[EPHY_FILE_FILTER_ALL_SUPPORTED] =
    ephy_file_dialog_create_mime_filter
      (_("All supported types"),
      "text/html",
      "application/xhtml+xml",
      "text/xml",
      "message/rfc822",                                     /* MHTML */
      "multipart/related",                                  /* MHTML */
      "application/x-mimearchive",                          /* MHTML */
      "application/pdf",
      "image/png",
      "image/jpeg",
      "image/gif",
      "image/webp",
      NULL);

  filter[EPHY_FILE_FILTER_WEBPAGES] =
    ephy_file_dialog_create_mime_filter
      (_("Web pages"),
      "text/html",
      "application/xhtml+xml",
      "text/xml",
      "message/rfc822",                                     /* MHTML */
      "multipart/related",                                  /* MHTML */
      "application/x-mimearchive",                          /* MHTML */
      NULL);

  filter[EPHY_FILE_FILTER_IMAGES] =
    ephy_file_dialog_create_mime_filter
      (_("Images"),
      "image/png",
      "image/jpeg",
      "image/gif",
      "image/webp",
      NULL);

  filter[EPHY_FILE_FILTER_ALL] =
    ephy_file_dialog_create_pattern_filter
      (_("All files"), "*", NULL);

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  for (i = 0; i < EPHY_FILE_FILTER_LAST; i++)
    g_list_store_append (filters, filter[i]);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_set_current_filter (dialog, filter[default_filter]);

  for (i = 0; i < EPHY_FILE_FILTER_LAST; i++)
    g_object_unref (filter[i]);
}
