/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2007 Xan Lopez
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

#include "ephy-embed.h"
#include "ephy-web-view.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_CONTAINER (ephy_embed_container_get_type ())

G_DECLARE_INTERFACE (EphyEmbedContainer, ephy_embed_container, EPHY, EMBED_CONTAINER, GObject)

struct _EphyEmbedContainerInterface
{
  GTypeInterface parent_iface;

  gint (* add_child)               (EphyEmbedContainer *container,
                                    EphyEmbed *child,
                                    EphyEmbed *parent,
                                    int position,
                                    gboolean set_active);

  void (* set_active_child)        (EphyEmbedContainer *container,
                                    EphyEmbed *child);

  void (* remove_child)            (EphyEmbedContainer *container,
                                    EphyEmbed *child);

  EphyEmbed * (* get_active_child) (EphyEmbedContainer *container);

  GList * (* get_children)         (EphyEmbedContainer *container);

  gboolean (* get_is_popup)        (EphyEmbedContainer *container);

  guint (* get_n_children)         (EphyEmbedContainer *container);
};

gint              ephy_embed_container_add_child        (EphyEmbedContainer *container,
                                                         EphyEmbed          *child,
                                                         EphyEmbed          *parent,
                                                         int                 position,
                                                         gboolean            set_active);
void              ephy_embed_container_set_active_child (EphyEmbedContainer *container,
                                                         EphyEmbed          *child);
void              ephy_embed_container_remove_child     (EphyEmbedContainer *container,
                                                         EphyEmbed          *child);
EphyEmbed *       ephy_embed_container_get_active_child (EphyEmbedContainer *container);
GList *           ephy_embed_container_get_children     (EphyEmbedContainer *container);
gboolean          ephy_embed_container_get_is_popup     (EphyEmbedContainer *container);
guint             ephy_embed_container_get_n_children   (EphyEmbedContainer *container);

G_END_DECLS
