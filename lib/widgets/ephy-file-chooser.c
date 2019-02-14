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

#define MAX_PREVIEW_SIZE 180
#define MAX_PREVIEW_SOURCE_SIZE 4096

static GtkFileFilter *
ephy_file_chooser_add_pattern_filter (GtkFileChooser *dialog,
                                      const char     *title,
                                      const char     *first_pattern,
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

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filth);

  return filth;
}

static GtkFileFilter *
ephy_file_chooser_add_mime_filter (GtkFileChooser *dialog,
                                   const char     *title,
                                   const char     *first_mimetype,
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

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filth);

  return filth;
}

static void
update_preview_cb (GtkFileChooser *file_chooser,
                   gpointer        data)
{
  GtkImage *preview = GTK_IMAGE (data);
  g_autofree char *filename = gtk_file_chooser_get_preview_filename (file_chooser);
  gint preview_width = 0;
  gint preview_height = 0;
  struct g_stat st_buf;
  g_autoptr(GdkPixbuf) pixbuf = NULL;

  GdkPixbufFormat *preview_format = gdk_pixbuf_get_file_info (filename,
                                                              &preview_width,
                                                              &preview_height);

  if (!filename || g_stat (filename, &st_buf) || (!S_ISREG (st_buf.st_mode))) {
    gtk_file_chooser_set_preview_widget_active (file_chooser, FALSE);
    return; // stat failed or file is not regular
  }

  if (!preview_format ||
      preview_width <= 0 || preview_height <= 0 ||
      preview_width > MAX_PREVIEW_SOURCE_SIZE ||
      preview_height > MAX_PREVIEW_SOURCE_SIZE) {
    gtk_file_chooser_set_preview_widget_active (file_chooser, FALSE);
    return; // unpreviewable, 0px, or unsafely large
  }

  if (preview_width > MAX_PREVIEW_SIZE || preview_height > MAX_PREVIEW_SIZE) {
    pixbuf = gdk_pixbuf_new_from_file_at_size (filename,
                                               MAX_PREVIEW_SIZE,
                                               MAX_PREVIEW_SIZE,
                                               NULL);
  } else {
    pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
  }

  pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);

  gtk_widget_set_size_request (GTK_WIDGET (preview),
                               gdk_pixbuf_get_width (pixbuf) + 6,
                               gdk_pixbuf_get_height (pixbuf) + 6);

  gtk_image_set_from_pixbuf (preview, pixbuf);
  gtk_file_chooser_set_preview_widget_active (file_chooser, pixbuf != NULL);
}

GtkFileChooser *
ephy_create_file_chooser (const char           *title,
                          GtkWidget            *parent,
                          GtkFileChooserAction  action,
                          EphyFileFilterDefault default_filter)
{
  GtkFileChooser *dialog;
  GtkFileFilter *filter[EPHY_FILE_FILTER_LAST];
  g_autofree char *downloads_dir = NULL;
  GtkWidget *preview = gtk_image_new ();

  g_assert (GTK_IS_WINDOW (parent));
  g_assert (default_filter >= 0 && default_filter <= EPHY_FILE_FILTER_LAST);

  dialog = GTK_FILE_CHOOSER (gtk_file_chooser_native_new (title,
                                                          GTK_WINDOW (parent),
                                                          action,
                                                          NULL,
                                                          _("_Cancel")));
  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);

  downloads_dir = ephy_file_get_downloads_dir ();
  gtk_file_chooser_add_shortcut_folder (dialog, downloads_dir, NULL);

  if (action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER) {
    gtk_file_chooser_native_set_accept_label (GTK_FILE_CHOOSER_NATIVE (dialog), _("_Open"));
  } else if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
    gtk_file_chooser_native_set_accept_label (GTK_FILE_CHOOSER_NATIVE (dialog), _("_Save"));
  }

  gtk_file_chooser_set_preview_widget (dialog, preview);
  gtk_file_chooser_set_use_preview_label (dialog, FALSE);
  g_signal_connect (dialog, "update-preview",
                    G_CALLBACK (update_preview_cb),
                    preview);

  if (default_filter != EPHY_FILE_FILTER_NONE) {
    filter[EPHY_FILE_FILTER_ALL_SUPPORTED] =
      ephy_file_chooser_add_mime_filter
        (dialog,
        _("All supported types"),
        "text/html",
        "application/xhtml+xml",
        "text/xml",
        "message/rfc822",                                     /* MHTML */
        "multipart/related",                                  /* MHTML */
        "application/x-mimearchive",                          /* MHTML */
        "image/png",
        "image/jpeg",
        "image/gif",
        "image/webp",
        NULL);

    filter[EPHY_FILE_FILTER_WEBPAGES] =
      ephy_file_chooser_add_mime_filter
        (dialog, _("Web pages"),
        "text/html",
        "application/xhtml+xml",
        "text/xml",
        "message/rfc822",                                     /* MHTML */
        "multipart/related",                                  /* MHTML */
        "application/x-mimearchive",                          /* MHTML */
        NULL);

    filter[EPHY_FILE_FILTER_IMAGES] =
      ephy_file_chooser_add_mime_filter
        (dialog, _("Images"),
        "image/png",
        "image/jpeg",
        "image/gif",
        "image/webp",
        NULL);

    filter[EPHY_FILE_FILTER_ALL] =
      ephy_file_chooser_add_pattern_filter
        (dialog, _("All files"), "*", NULL);

    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog),
                                 filter[default_filter]);
  }

  return dialog;
}
