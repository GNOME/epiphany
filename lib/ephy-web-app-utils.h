/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#include <gio/gdesktopappinfo.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct {
  char *id;
  char *name;
  char *icon_path;
  char *tmp_icon_path;
  char *url;
  char *desktop_file; /* only used for legacy apps */
  char *desktop_path;
  guint64 install_date_uint64;
} EphyWebApplication;

/**
 * EphyWebApplicationOptions:
 * @EPHY_WEB_APPLICATION_NONE: A default web application installed in the
 *    user's home directory.
 * @EPHY_WEB_APPLICATION_MOBILE_CAPABLE: Set when the meta tag
 *    "apple-mobile-web-app-capable" is set. Causes back/forward navigation
 *    buttons to be hidden.
 * @EPHY_WEB_APPLICATION_SYSTEM: Set when the web application was installed
 *    under /usr via deb/rpm rather than in the user's home directory.
 */
typedef enum {
  EPHY_WEB_APPLICATION_NONE,
  EPHY_WEB_APPLICATION_MOBILE_CAPABLE,
  EPHY_WEB_APPLICATION_SYSTEM,
} EphyWebApplicationOptions;

typedef enum {
  EPHY_WEB_APP_FOUND,
  EPHY_WEB_APP_NOT_FOUND,
} EphyWebAppFound;

typedef enum {
  EPHY_WEB_APP_NEED_TMP_ICON,
  EPHY_WEB_APP_NO_TMP_ICON, /* avoid sync I/O, don't initialize app->tmp_icon_path */
} EphyWebAppNeedTmpIcon;

#define EPHY_WEB_APP_ICON_NAME "app-icon.png"

/* The GApplication ID must begin with the app ID for the dynamic launcher portal to work */
#define EPHY_WEB_APP_GAPPLICATION_ID_PREFIX APPLICATION_ID ".WebApp_"

char               *ephy_web_application_get_app_id_from_name (const char *name);

const char         *ephy_web_application_get_gapplication_id_from_profile_directory (const char *profile_dir);

gboolean            ephy_web_application_create (const char                 *id,
                                                 const char                 *address,
                                                 const char                 *install_token,
                                                 EphyWebApplicationOptions   options,
                                                 GError                    **error);

char               *ephy_web_application_ensure_for_app_info (GAppInfo *app_info);

void                ephy_web_application_launch (const char *id);

gboolean            ephy_web_application_delete (const char      *id,
                                                 EphyWebAppFound *out_app_found);

gboolean            ephy_web_application_delete_by_desktop_file_id (const char      *desktop_file_id,
                                                                    EphyWebAppFound *out_app_found);

void                ephy_web_application_setup_from_profile_directory (const char *profile_directory);

void                ephy_web_application_setup_from_desktop_file (GDesktopAppInfo *desktop_info);

char               *ephy_web_application_get_profile_directory (const char *id);

/* legacy variant for profile migration */
char               *ephy_legacy_web_application_get_profile_directory (const char *id);

GKeyFile           *ephy_web_application_get_desktop_keyfile (const char  *id,
                                                              GError     **error);

char               *ephy_web_application_get_desktop_path (const char *id);

EphyWebApplication *ephy_web_application_for_profile_directory (const char            *profile_dir,
                                                                EphyWebAppNeedTmpIcon  need_tmp_icon);

void                ephy_web_application_free (EphyWebApplication *app);

gboolean            ephy_web_application_exists (const char *id);

GList              *ephy_web_application_get_application_list (void);

/* legacy variant for profile migration */
GList              *ephy_web_application_get_legacy_application_list (void);

char              **ephy_web_application_get_desktop_id_list (void);

void                ephy_web_application_free_application_list (GList *list);

void                ephy_web_application_initialize_settings (const char *profile_directory, EphyWebApplicationOptions options);

gboolean            ephy_web_application_is_uri_allowed (const char *uri);

gboolean            ephy_web_application_save (EphyWebApplication *app);

gboolean            ephy_web_application_is_system (EphyWebApplication *app);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EphyWebApplication, ephy_web_application_free)

G_END_DECLS
