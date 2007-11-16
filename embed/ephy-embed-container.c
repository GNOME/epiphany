/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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

#include "config.h"

#include "ephy-embed-container.h"
#include "ephy-embed-type-builtins.h"

static void
ephy_embed_container_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (!initialized) {
    initialized = TRUE;

    g_object_interface_install_property (g_class,
                                         g_param_spec_flags ("chrome", NULL, NULL,
                                                             EPHY_TYPE_EMBED_CHROME,
                                                             EPHY_EMBED_CHROME_ALL,
                                                             G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_interface_install_property (g_class,
                                         g_param_spec_boolean ("is-popup", NULL, NULL,
                                                               FALSE,
                                                               G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
                                                               G_PARAM_CONSTRUCT_ONLY));
  }
}

GType
ephy_embed_container_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0)) {
    const GTypeInfo our_info =
      {
        sizeof (EphyEmbedContainerIface),
        ephy_embed_container_base_init,
        NULL,
      };

    type = g_type_register_static (G_TYPE_INTERFACE,
                                   "EphyEmbedContainer",
                                   &our_info, (GTypeFlags)0);
  }

  return type;
}

/**
 * ephy_embed_container_add_child:
 * @container: an #EphyEmbedContainer
 * @child: an #EphyEmbed
 * @position: the position in @container's
 * @jump_to: %TRUE to switch to @container's new child after insertion
 *
 * Inserts @child into @container.
 * Return value: @child's new position inside @container.
 **/
gint
ephy_embed_container_add_child (EphyEmbedContainer *container,
                                EphyEmbed *child,
                                gint position,
                                gboolean jump_to)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  return iface->add_child (container, child, position, jump_to);
}

/**
 * ephy_embed_container_set_active_child:
 * @container: an #EphyEmbedContainer
 * @child: an #EphyEmbed inside @container
 *
 * Sets @child as @container's active child.
 **/
void
ephy_embed_container_set_active_child (EphyEmbedContainer *container,
                                       EphyEmbed *child)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  iface->set_active_child (container, child);
}

/**
 * ephy_embed_container_remove_child:
 * @container: an #EphyEmbedContainer
 * @child: an #EphyEmbed
 *
 * Removes @child from @container.
 **/
void
ephy_embed_container_remove_child (EphyEmbedContainer *container,
                                   EphyEmbed *child)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  iface->remove_child (container, child);
}

/**
 * ephy_embed_container_get_active_child:
 * @window: an #EphyEmbedContainer
 *
 * Returns @container's active #EphyEmbed.
 *
 * Return value: @container's active child
 **/
EphyEmbed *
ephy_embed_container_get_active_child (EphyEmbedContainer *container)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  return iface->get_active_child (container);
}

/**
 * ephy_embed_container_get_children:
 * @container: a #EphyEmbedContainer
 *
 * Returns the list of #EphyEmbed:s in the container.
 *
 * Return value: a newly-allocated list of #EphyEmbed:s
 */
GList *
ephy_embed_container_get_children (EphyEmbedContainer *container)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  return iface->get_children (container);
}

/**
 * ephy_embed_container_get_is_popup:
 * @container: an #EphyEmbedContainer
 *
 * Returns whether this embed container is a popup.
 *
 * Return value: %TRUE if it is a popup
 **/
gboolean
ephy_embed_container_get_is_popup (EphyEmbedContainer *container)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  return iface->get_is_popup (container);
}

/**
 * ephy_embed_container_get_chrome:
 * @container: an #EphyEmbedContainer
 *
 * Returns the #EphyEmbedChrome flags indicating the visibility of several parts
 * of the UI.
 *
 * Return value: #EphyEmbedChrome flags.
 **/
EphyEmbedChrome
ephy_embed_container_get_chrome (EphyEmbedContainer *container)
{
  EphyEmbedContainerIface *iface = EPHY_EMBED_CONTAINER_GET_IFACE (container);
  return iface->get_chrome (container);
}

