/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright (c) 2021 Matthew Leeds <mwleeds@protonmail.com>
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

#include "ephy-webapp-provider-generated.h"

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_APP_PROVIDER_SERVICE (ephy_web_app_provider_service_get_type ())

G_DECLARE_FINAL_TYPE (EphyWebAppProviderService, ephy_web_app_provider_service, EPHY, WEB_APP_PROVIDER_SERVICE, GApplication)

EphyWebAppProviderService *ephy_web_app_provider_service_new (void);

G_END_DECLS
