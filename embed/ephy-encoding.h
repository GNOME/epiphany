/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ENCODING (ephy_encoding_get_type ())

G_DECLARE_FINAL_TYPE (EphyEncoding, ephy_encoding, EPHY, ENCODING, GObject)

typedef enum
{
  LG_NONE     = 0,
  LG_ARABIC   = 1 << 0,
  LG_BALTIC   = 1 << 1,
  LG_CAUCASIAN    = 1 << 2,
  LG_C_EUROPEAN   = 1 << 3,
  LG_CHINESE_TRAD   = 1 << 4,
  LG_CHINESE_SIMP   = 1 << 5,
  LG_CYRILLIC   = 1 << 6,
  LG_GREEK    = 1 << 7,
  LG_HEBREW   = 1 << 8,
  LG_INDIAN   = 1 << 9,
  LG_JAPANESE   = 1 << 10,
  LG_KOREAN   = 1 << 12,
  LG_NORDIC   = 1 << 13,
  LG_PERSIAN    = 1 << 14,
  LG_SE_EUROPEAN    = 1 << 15,
  LG_THAI     = 1 << 16,
  LG_TURKISH    = 1 << 17,
  LG_UKRAINIAN    = 1 << 18,
  LG_UNICODE    = 1 << 19,
  LG_VIETNAMESE   = 1 << 20,
  LG_WESTERN    = 1 << 21,
  LG_ALL      = 0x3fffff,
} EphyLanguageGroup;

EphyEncoding * ephy_encoding_new                  (const char *encoding,
                                                   const char *title,
                                                   int language_groups);
const char    * ephy_encoding_get_title           (EphyEncoding *encoding);
const char    * ephy_encoding_get_title_elided    (EphyEncoding *encoding);
const char    * ephy_encoding_get_encoding        (EphyEncoding *encoding);
const char    * ephy_encoding_get_collation_key   (EphyEncoding *encoding);
int             ephy_encoding_get_language_groups (EphyEncoding *encoding);

G_END_DECLS
