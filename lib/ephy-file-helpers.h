/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2005, 2006 Christian Persch
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

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

extern GQuark ephy_file_helpers_error_quark;
#define EPHY_FILE_HELPERS_ERROR_QUARK        (ephy_file_helpers_error_quark)

G_BEGIN_DECLS

typedef enum
{
  EPHY_MIME_PERMISSION_SAFE    = 1,
  EPHY_MIME_PERMISSION_UNSAFE  = 2,
  EPHY_MIME_PERMISSION_UNKNOWN = 3
} EphyMimePermission;

typedef enum
{
  EPHY_FILE_HELPERS_NONE             = 0,
  EPHY_FILE_HELPERS_KEEP_DIR         = 1 << 1,
  EPHY_FILE_HELPERS_PRIVATE_PROFILE  = 1 << 2,
  EPHY_FILE_HELPERS_ENSURE_EXISTS    = 1 << 3,
  EPHY_FILE_HELPERS_STEAL_DATA       = 1 << 4,
  EPHY_FILE_HELPERS_TESTING_MODE     = 1 << 5
} EphyFileHelpersFlags;

gboolean           ephy_file_helpers_init                   (const char            *profile_dir,
                                                             EphyFileHelpersFlags   flags,
                                                             GError               **error);
const char *       ephy_dot_dir                             (void);
gboolean           ephy_dot_dir_is_default                  (void);
gboolean           ephy_dot_dir_is_web_application          (void);
char       *       ephy_default_dot_dir                     (void);
void               ephy_file_helpers_shutdown               (void);
char       *       ephy_file_get_downloads_dir              (void);
char       *       ephy_file_desktop_dir                    (void);
const char *       ephy_file_tmp_dir                        (void);
char       *       ephy_file_tmp_filename                   (const char            *base,
                                                             const char            *extension);
gboolean           ephy_ensure_dir_exists                   (const char            *dir,
                                                             GError **);
GSList     *       ephy_file_find                           (const char            *path,
                                                             const char            *fname,
                                                             gint                   maxdepth);
void               ephy_file_delete_on_exit                 (GFile                 *file);
gboolean           ephy_file_launch_desktop_file            (const char            *filename,
                                                             const char            *parameter,
                                                             guint32                user_time,
                                                             GtkWidget             *widget);
gboolean           ephy_file_launch_application             (GAppInfo              *app,
                                                             GList                 *files,
                                                             guint32                user_time,
                                                             GtkWidget             *widget);
gboolean           ephy_file_launch_handler                 (const char            *mime_type,
                                                             GFile                 *file,
                                                             guint32                user_time);
gboolean           ephy_file_open_uri_in_default_browser    (const char            *uri,
                                                             guint32                timestamp,
                                                             GdkScreen             *screen);
gboolean           ephy_file_browse_to                      (GFile                 *file,
                                                             guint32                user_time);
gboolean           ephy_file_delete_dir_recursively         (const char            *directory,
                                                             GError               **error);
void               ephy_file_delete_uri                     (const char            *uri);
gboolean           ephy_file_move_uri                       (const char            *source_uri,
                                                             const char            *dest_uri);
char       *       ephy_file_create_data_uri_for_filename   (const char            *filename,
                                                             const char            *mime_type);
char       *       ephy_sanitize_filename                   (char                  *filename);
GAppInfo   *       ephy_file_launcher_get_app_info_for_file (GFile                 *file,
                                                             const char            *mime_type);
void               ephy_open_default_instance_window        (void);
void               ephy_open_incognito_window               (const char            *uri);
gboolean           ephy_file_launch_via_uri_handler         (const char            *uri);
gboolean           ephy_file_launch_file_via_uri_handler    (GFile                 *file);

G_END_DECLS
