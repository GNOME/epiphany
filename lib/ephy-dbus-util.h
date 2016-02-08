/*
 *  Copyright © 2016 Igalia S.L.
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

#ifndef EPHY_DBUS_UTIL_H
#define EPHY_DBUS_UTIL_H

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean ephy_dbus_peer_is_authorized (GCredentials *peer_credentials);

G_END_DECLS

#endif
