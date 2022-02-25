/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2004 Christian Persch
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

#include "ephy-embed.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LINK (ephy_link_get_type ())

G_DECLARE_INTERFACE (EphyLink, ephy_link, EPHY, LINK, GObject)

typedef enum
{
  EPHY_LINK_NEW_WINDOW             = 1 << 0,
  EPHY_LINK_NEW_TAB                = 1 << 1,
  EPHY_LINK_JUMP_TO                = 1 << 2,
  EPHY_LINK_NEW_TAB_APPEND_AFTER   = 1 << 3,
  EPHY_LINK_HOME_PAGE              = 1 << 4,
  EPHY_LINK_TYPED                  = 1 << 5,
  EPHY_LINK_BOOKMARK               = 1 << 6
} EphyLinkFlags;

struct _EphyLinkInterface
{
  GTypeInterface base_iface;

  /* Signals */
  EphyEmbed * (* open_link) (EphyLink *link,
                             const char *address,
                             EphyEmbed *embed,
                             EphyLinkFlags flags);
};

EphyEmbed *ephy_link_open (EphyLink *link,
                           const char *address,
                           EphyEmbed *embed,
                           EphyLinkFlags flags);

EphyLinkFlags ephy_link_flags_from_modifiers (GdkModifierType modifiers,
                                              gboolean        middle_click);

G_END_DECLS
