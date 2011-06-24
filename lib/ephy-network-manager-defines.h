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

#ifndef EPHY_NETWORK_MANAGER_DEFINES_H
#define EPHY_NETWORK_MANAGER_DEFINES_H

#define NM_DBUS_SERVICE                     "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH                        "/org/freedesktop/NetworkManager"

/**
 * NMState:
 * @NM_STATE_UNKNOWN: networking state is unknown
 * @NM_STATE_ASLEEP: networking is not enabled
 * @NM_STATE_DISCONNECTED: there is no active network connection
 * @NM_STATE_DISCONNECTING: network connections are being cleaned up
 * @NM_STATE_CONNECTING: a network connection is being started
 * @NM_STATE_CONNECTED_LOCAL: there is only local IPv4 and/or IPv6 connectivity
 * @NM_STATE_CONNECTED_SITE: there is only site-wide IPv4 and/or IPv6 connectivity
 * @NM_STATE_CONNECTED_GLOBAL: there is global IPv4 and/or IPv6 Internet connectivity
 *
 * #NMState values indicate the current overall networking state.
 */
typedef enum {
        NM_STATE_UNKNOWN          = 0,
        NM_STATE_ASLEEP           = 10,
        NM_STATE_DISCONNECTED     = 20,
        NM_STATE_DISCONNECTING    = 30,
        NM_STATE_CONNECTING       = 40,
        NM_STATE_CONNECTED_LOCAL  = 50,
        NM_STATE_CONNECTED_SITE   = 60,
        NM_STATE_CONNECTED_GLOBAL = 70
} NMState;

#endif
