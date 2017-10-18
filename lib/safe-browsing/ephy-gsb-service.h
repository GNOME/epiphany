/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_GSB_SERVICE (ephy_gsb_service_get_type ())

G_DECLARE_FINAL_TYPE (EphyGSBService, ephy_gsb_service, EPHY, GSB_SERVICE, GObject)

EphyGSBService *ephy_gsb_service_new                (const char *api_key,
                                                     const char *db_path);
void            ephy_gsb_service_verify_url         (EphyGSBService      *self,
                                                     const char          *url,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);
GList          *ephy_gsb_service_verify_url_finish  (EphyGSBService  *self,
                                                     GAsyncResult    *result);

G_END_DECLS
