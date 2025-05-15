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
#include <libportal/portal-helpers.h>

extern GQuark ephy_file_helpers_error_quark;
#define EPHY_FILE_HELPERS_ERROR_QUARK        (ephy_file_helpers_error_quark)

G_BEGIN_DECLS

typedef enum
{
  EPHY_FILE_HELPERS_NONE             = 0,
  EPHY_FILE_HELPERS_KEEP_DIR         = 1 << 0,
  EPHY_FILE_HELPERS_PRIVATE_PROFILE  = 1 << 1,
  EPHY_FILE_HELPERS_ENSURE_EXISTS    = 1 << 2,
  EPHY_FILE_HELPERS_STEAL_DATA       = 1 << 3,
  EPHY_FILE_HELPERS_TESTING_MODE     = 1 << 4
} EphyFileHelpersFlags;

typedef enum {
  EPHY_FILE_LAUNCH_URI_HANDLER_FILE,
  EPHY_FILE_LAUNCH_URI_HANDLER_DIRECTORY
} EphyFileLaunchUriHandlerType;

gboolean           ephy_file_helpers_init                   (const char            *profile_dir,
                                                             EphyFileHelpersFlags   flags,
                                                             GError               **error);
const char *       ephy_profile_dir                         (void);
gboolean           ephy_profile_dir_is_default              (void);
gboolean           ephy_profile_dir_is_web_application      (void);
const char *       ephy_cache_dir                           (void);
const char *       ephy_config_dir                          (void);
char       *       ephy_default_profile_dir                 (void);
char       *       ephy_default_cache_dir                   (void);
char       *       ephy_default_config_dir                  (void);
void               ephy_file_helpers_shutdown               (void);
char       *       ephy_file_get_downloads_dir              (void);
char       *       ephy_file_desktop_dir                    (void);
char       *       ephy_file_get_display_name               (GFile                 *file);
const char *       ephy_file_tmp_dir                        (void);
char       *       ephy_file_tmp_filename                   (const char            *base,
                                                             const char            *extension);
gboolean           ephy_ensure_dir_exists                   (const char            *dir,
                                                             GError               **error);
gboolean           ephy_file_launch_uri_handler             (GFile                         *file,
                                                             const char                    *mime_type,
                                                             GdkDisplay                    *display,
                                                             EphyFileLaunchUriHandlerType   type);
gboolean           ephy_file_delete_dir_recursively         (const char            *directory,
                                                             GError               **error);
char       *       ephy_sanitize_filename                   (char                  *filename);
void               ephy_open_default_instance_window        (void);
void               ephy_open_incognito_window               (const char            *uri);
gboolean           ephy_file_open_uri_in_default_browser    (const char            *uri,
                                                             GdkDisplay            *display);
gboolean           ephy_file_browse_to                      (GFile                 *file,
                                                             GdkDisplay            *display);
void               ephy_copy_directory                      (const char            *source,
                                                             const char            *target);
XdpPortal  *       ephy_get_portal                          (void);

G_END_DECLS
