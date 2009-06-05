/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2007 Christian Persch
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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>

#include "eel-gconf-extensions.h"
#include "ephy-base-embed.h"
#include "ephy-debug.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-permission-manager.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-string.h"
#include "ephy-zoom.h"

G_DEFINE_TYPE (EphyBaseEmbed, ephy_base_embed, GTK_TYPE_BIN)

static void
ephy_base_embed_size_request (GtkWidget *widget,
                              GtkRequisition *requisition)
{
  GtkWidget *child;

  GTK_WIDGET_CLASS (ephy_base_embed_parent_class)->size_request (widget, requisition);

  child = GTK_BIN (widget)->child;

  if (child && GTK_WIDGET_VISIBLE (child)) {
    GtkRequisition child_requisition;
    gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
  }
}

static void
ephy_base_embed_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
  GtkWidget *child;

  widget->allocation = *allocation;

  child = GTK_BIN (widget)->child;
  g_return_if_fail (child != NULL);

  gtk_widget_size_allocate (child, allocation);
}

static void
ephy_base_embed_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child)
    gtk_widget_grab_focus (child);
}

static void
ephy_base_embed_class_init (EphyBaseEmbedClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  widget_class->size_request = ephy_base_embed_size_request;
  widget_class->size_allocate = ephy_base_embed_size_allocate;
  widget_class->grab_focus = ephy_base_embed_grab_focus;
}

static void
ephy_base_embed_init (EphyBaseEmbed *self)
{
}
