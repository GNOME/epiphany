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
  char *icon_url;
  char *url;
  char *desktop_file;
  char install_date[128];
} EphyWebApplication;

#define EPHY_WEB_APP_PREFIX "app-"
#define EPHY_WEB_APP_ICON_NAME "app-icon.png"

char    *ephy_web_application_get_app_id_from_name (const char *name);

char    *ephy_web_application_create (const char *id, const char *address, const char *name, GdkPixbuf *icon);

char    *ephy_web_application_ensure_for_app_info (GAppInfo *app_info);

gboolean ephy_web_application_delete (const char *id);

void     ephy_web_application_setup_from_profile_directory (const char *profile_directory);

void     ephy_web_application_setup_from_desktop_file (GDesktopAppInfo *desktop_info);

char    *ephy_web_application_get_profile_directory (const char *id);

GList   *ephy_web_application_get_application_list (void);

void     ephy_web_application_free_application_list (GList *list);

gboolean ephy_web_application_exists (const char *id);

void     ephy_web_application_initialize_settings (const char *profile_directory);

G_END_DECLS
