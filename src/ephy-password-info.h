/*
 *  Copyright © 2009 Holger Hans Peter Freyther
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

#ifndef EPHY_PASSWORD_INFO_H
#define EPHY_PASSWORD_INFO_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PASSWORD_INFO    (ephy_password_info_get_type ())

/*
 * Password Data for Gnome Keyring. We keep track of the
 * key_id and the password in secured memory.
 */
typedef struct _EphyPasswordInfo EphyPasswordInfo;

struct _EphyPasswordInfo {
  guint32 keyring_id;
  char *secret;
};

GType ephy_password_info_get_type (void) G_GNUC_CONST;
EphyPasswordInfo *ephy_password_info_new (guint32);

G_END_DECLS

#endif
