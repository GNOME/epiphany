/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Christian Persch
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_ENCODINGS_H
#define EPHY_ENCODINGS_H

#include <glib-object.h>
#include <glib.h>

#include "ephy-encoding.h"

G_BEGIN_DECLS

#define EPHY_TYPE_ENCODINGS (ephy_encodings_get_type ())

G_DECLARE_FINAL_TYPE (EphyEncodings, ephy_encodings, EPHY, ENCODINGS, GObject)

EphyEncodings *ephy_encodings_new           (void);
EphyEncoding  *ephy_encodings_get_encoding  (EphyEncodings     *encodings,
                                             const char        *code,
                                             gboolean           add_if_not_found);
GList         *ephy_encodings_get_encodings (EphyEncodings     *encodings,
                                             EphyLanguageGroup  group_mask);
GList         *ephy_encodings_get_all       (EphyEncodings     *encodings);
void           ephy_encodings_add_recent    (EphyEncodings     *encodings,
                                             const char        *code);
GList         *ephy_encodings_get_recent    (EphyEncodings     *encodings);

G_END_DECLS

#endif
