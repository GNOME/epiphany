/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_FILE_MONITOR_H
#define EPHY_FILE_MONITOR_H

#include "ephy-web-view.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FILE_MONITOR (ephy_file_monitor_get_type ())

G_DECLARE_FINAL_TYPE (EphyFileMonitor, ephy_file_monitor, EPHY, FILE_MONITOR, GObject)

EphyFileMonitor * ephy_file_monitor_new             (EphyWebView *view);
void              ephy_file_monitor_update_location (EphyFileMonitor *monitor,
                                                     const char *address);

G_END_DECLS

#endif
