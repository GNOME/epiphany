/*
 *  Copyright © 2007 Xan Lopez
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

#ifndef EPHY_EMBED_CONTAINER_H
#define EPHY_EMBED_CONTAINER_H

#include "ephy-embed.h"
#include "ephy-web-view.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_CONTAINER               (ephy_embed_container_get_type ())
#define EPHY_EMBED_CONTAINER(o)                 (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_CONTAINER, EphyEmbedContainer))
#define EPHY_EMBED_CONTAINER_IFACE(k)           (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_CONTAINER, EphyEmbedContainerIface))
#define EPHY_IS_EMBED_CONTAINER(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_CONTAINER))
#define EPHY_IS_EMBED_CONTAINER_IFACE(k)        (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_CONTAINER))
#define EPHY_EMBED_CONTAINER_GET_IFACE(inst)    (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_EMBED_CONTAINER, EphyEmbedContainerIface))

typedef struct _EphyEmbedContainer      EphyEmbedContainer;
typedef struct _EphyEmbedContainerIface EphyEmbedContainerIface;

struct _EphyEmbedContainerIface
{
  GTypeInterface parent_iface;

  gint (* add_child)               (EphyEmbedContainer *container,
                                    EphyEmbed *child,
                                    gint position,
                                    gboolean set_active);

  void (* set_active_child)        (EphyEmbedContainer *container,
                                    EphyEmbed *child);

  void (* remove_child)            (EphyEmbedContainer *container,
                                    EphyEmbed *child);

  EphyEmbed * (* get_active_child) (EphyEmbedContainer *container);

  GList * (* get_children)         (EphyEmbedContainer *container);

  gboolean (* get_is_popup)        (EphyEmbedContainer *container);
};

GType             ephy_embed_container_get_type         (void);
gint              ephy_embed_container_add_child        (EphyEmbedContainer *container,
                                                         EphyEmbed          *child,
                                                         gint                position,
                                                         gboolean            set_active);
void              ephy_embed_container_set_active_child (EphyEmbedContainer *container,
                                                         EphyEmbed          *child);
void              ephy_embed_container_remove_child     (EphyEmbedContainer *container,
                                                         EphyEmbed          *child);
EphyEmbed *       ephy_embed_container_get_active_child (EphyEmbedContainer *container);
GList *           ephy_embed_container_get_children     (EphyEmbedContainer *container);
gboolean          ephy_embed_container_get_is_popup     (EphyEmbedContainer *container);

G_END_DECLS

#endif
