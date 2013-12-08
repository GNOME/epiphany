/*
 *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_WEB_APP_UTILS_H
#define EPHY_WEB_APP_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

typedef struct {
    char *name;
    char *icon_url;
    char *url;
    char *desktop_file;
    char install_date[128];
} EphyWebApplication;

#define EPHY_WEB_APP_PREFIX "app-"
#define EPHY_WEB_APP_ICON_NAME "app-icon.png"

char    *ephy_web_application_create (const char *address, const char *name, GdkPixbuf *icon);

gboolean ephy_web_application_delete (const char *name);

char    *ephy_web_application_get_profile_directory (const char *name);

GList   *ephy_web_application_get_application_list (void);

void     ephy_web_application_free_application_list (GList *list);

gboolean ephy_web_application_exists (const char *name);

G_END_DECLS

#endif

