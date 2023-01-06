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

#include "ephy-file-dialog-utils.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

static const char *webpage_types[] = {
  "text/html",
  "application/xhtml+xml",
  "text/xml",
  "message/rfc822",                                     /* MHTML */
  "multipart/related",                                  /* MHTML */
  "application/x-mimearchive",                          /* MHTML */
  NULL
};

static const char *image_types[] = {
  "image/png",
  "image/jpeg",
  "image/gif",
  "image/webp",
  "image/avif",
  NULL
};

void
ephy_file_dialog_add_filters (GtkFileDialog *dialog)
{
  g_autoptr (GListStore) filters = NULL;
  g_autoptr (GtkFileFilter) supported_filter = NULL;
  g_autoptr (GtkFileFilter) webpages_filter = NULL;
  g_autoptr (GtkFileFilter) images_filter = NULL;
  g_autoptr (GtkFileFilter) all_filter = NULL;
  int i;

  g_assert (GTK_IS_FILE_DIALOG (dialog));

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

  supported_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (supported_filter, _("All supported types"));
  g_list_store_append (filters, supported_filter);

  webpages_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (webpages_filter, _("Web pages"));
  g_list_store_append (filters, webpages_filter);

  images_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (images_filter, _("Images"));
  g_list_store_append (filters, images_filter);

  all_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, _("All files"));
  gtk_file_filter_add_pattern (all_filter, "*");
  g_list_store_append (filters, all_filter);

  for (i = 0; webpage_types[i]; i++) {
    gtk_file_filter_add_mime_type (supported_filter, webpage_types[i]);
    gtk_file_filter_add_mime_type (webpages_filter, webpage_types[i]);
  }

  for (i = 0; image_types[i]; i++) {
    gtk_file_filter_add_mime_type (supported_filter, image_types[i]);
    gtk_file_filter_add_mime_type (images_filter, image_types[i]);
  }

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_set_default_filter (dialog, supported_filter);
}
