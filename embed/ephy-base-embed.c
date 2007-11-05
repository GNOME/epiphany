/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2007  Xan Lopez
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
#include "ephy-embed.h"
#include "ephy-base-embed.h"

struct _EphyBaseEmbedPrivate
{
  /* Flags */
  guint is_blank : 1;

  char *address;
  char *typed_address;
  char *title;
  char *loading_title;
};

static void ephy_base_embed_dispose (GObject *object);
static void ephy_base_embed_finalize (GObject *object);
static void ephy_embed_iface_init (EphyEmbedIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (EphyBaseEmbed, ephy_base_embed, GTK_TYPE_BIN,
                                  G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED,
                                                         ephy_embed_iface_init))

static void
ephy_base_embed_size_request (GtkWidget *widget,
                              GtkRequisition *requisition)
{
  GtkWidget *child;

  GTK_WIDGET_CLASS (ephy_base_embed_parent_class)->size_request (widget, requisition);

  child = GTK_BIN (widget)->child;
	
  if (child && GTK_WIDGET_VISIBLE (child))
    {
      GtkRequisition child_requisition;
      gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
    }
}

static void
ephy_base_embed_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
  GtkWidget *child;
  GtkAllocation invalid = { -1, -1, 1, 1 };

  widget->allocation = *allocation;

  child = GTK_BIN (widget)->child;
  g_return_if_fail (child != NULL);

  gtk_widget_size_allocate (child, allocation);
}

static void
ephy_base_embed_class_init (EphyBaseEmbedClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  gobject_class->dispose = ephy_base_embed_dispose;
  gobject_class->finalize = ephy_base_embed_finalize;

  widget_class->size_request = ephy_base_embed_size_request;
  widget_class->size_allocate = ephy_base_embed_size_allocate;
  g_type_class_add_private (gobject_class, sizeof (EphyBaseEmbedPrivate));
}

static void
ephy_base_embed_init (EphyBaseEmbed *self)
{
}

static void
ephy_base_embed_dispose (GObject *object)
{
  EphyBaseEmbed *self = (EphyBaseEmbed *)object;

  G_OBJECT_CLASS (ephy_base_embed_parent_class)->dispose (object);
}

static void
ephy_base_embed_finalize (GObject *object)
{
  EphyBaseEmbed *self = (EphyBaseEmbed *)object;

  G_OBJECT_CLASS (ephy_base_embed_parent_class)->finalize (object);
}

static void
ephy_embed_iface_init (EphyEmbedIface *iface)
{
}
