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

GtkFileChooser *
ephy_create_file_chooser (const char            *title,
                          GtkWidget             *parent,
                          GtkFileChooserAction   action,
                          EphyFileFilterDefault  default_filter)
{
  GtkWidget *toplevel_window = gtk_widget_get_toplevel (parent);
  GtkFileChooser *dialog;
  GtkFileFilter *filter[EPHY_FILE_FILTER_LAST];
  g_autofree char *downloads_dir = NULL;

  g_assert (GTK_IS_WINDOW (toplevel_window));
  g_assert (default_filter >= 0 && default_filter <= EPHY_FILE_FILTER_LAST);

  dialog = GTK_FILE_CHOOSER (gtk_file_chooser_native_new (title,
                                                          GTK_WINDOW (toplevel_window),
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
        "application/pdf",
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
