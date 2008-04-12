/*
 *  Copyright © 2007 Chris Wilson
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 *  $Id: ephy-node-db.h 6588 2006-09-13 11:34:25Z chpe $
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_GLIB_COMPAT_H
#define EPHY_GLIB_COMPAT_H

#include <glib.h>

G_BEGIN_DECLS

#if !GLIB_CHECK_VERSION(2,13,0)
#define g_timeout_add_seconds(I, F, D) g_timeout_add((I)*1000, (F), (D))
#endif

G_END_DECLS

#endif /* EPHY_GLIB_COMPAT_H */
