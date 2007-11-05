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
#include "ephy-embed-type-builtins.h"
#include "ephy-zoom.h"

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

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_DOCUMENT_TYPE,
  PROP_HIDDEN_POPUP_COUNT,
  PROP_ICON,
  PROP_ICON_ADDRESS,
  PROP_LINK_MESSAGE,
  PROP_LOAD_PROGRESS,
  PROP_LOAD_STATUS,
  PROP_NAVIGATION,
  PROP_POPUPS_ALLOWED,
  PROP_SECURITY,
  PROP_STATUS_MESSAGE,
  PROP_TITLE,
  PROP_TYPED_ADDRESS,
  PROP_ZOOM
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

  widget->allocation = *allocation;

  child = GTK_BIN (widget)->child;
  g_return_if_fail (child != NULL);

  gtk_widget_size_allocate (child, allocation);
}

static void
ephy_base_embed_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
}

static void
ephy_base_embed_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
}

static void
ephy_base_embed_class_init (EphyBaseEmbedClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  gobject_class->dispose = ephy_base_embed_dispose;
  gobject_class->finalize = ephy_base_embed_finalize;
  gobject_class->get_property = ephy_base_embed_get_property;
  gobject_class->set_property = ephy_base_embed_set_property;

  widget_class->size_request = ephy_base_embed_size_request;
  widget_class->size_allocate = ephy_base_embed_size_allocate;

  g_object_class_install_property (gobject_class,
                                   PROP_SECURITY,
                                   g_param_spec_enum ("security-level",
                                                      "Security Level",
                                                      "The embed's security level",
                                                      EPHY_TYPE_EMBED_SECURITY_LEVEL,
                                                      EPHY_EMBED_STATE_IS_UNKNOWN,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_DOCUMENT_TYPE,
                                   g_param_spec_enum ("document-type",
                                                      "Document Type",
                                                      "The embed's documen type",
                                                      EPHY_TYPE_EMBED_DOCUMENT_TYPE,
                                                      EPHY_EMBED_DOCUMENT_HTML,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_ZOOM,
                                   g_param_spec_float ("zoom",
                                                       "Zoom",
                                                       "The embed's zoom",
                                                       ZOOM_MINIMAL,
                                                       ZOOM_MAXIMAL,
                                                       1.0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LOAD_PROGRESS,
                                   g_param_spec_int ("load-progress",
                                                     "Load progress",
                                                     "The embed's load progress in percent",
                                                     0,
                                                     100,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LOAD_STATUS,
                                   g_param_spec_boolean ("load-status",
                                                         "Load status",
                                                         "The embed's load status",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_NAVIGATION,
                                   g_param_spec_flags ("navigation",
                                                       "Navigation flags",
                                                       "The embed's navigation flags",
                                                       EPHY_TYPE_EMBED_NAVIGATION_FLAGS,
                                                       0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        "Address",
                                                        "The embed's address",
                                                        "",
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_TYPED_ADDRESS,
                                   g_param_spec_string ("typed-address",
                                                        "Typed Address",
                                                        "The typed address",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The embed's title",
                                                        _("Blank page"),
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_STATUS_MESSAGE,
                                   g_param_spec_string ("status-message",
                                                        "Status Message",
                                                        "The embed's statusbar message",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LINK_MESSAGE,
                                   g_param_spec_string ("link-message",
                                                        "Link Message",
                                                        "The embed's link message",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        "Icon",
                                                        "The embed icon's",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_ADDRESS,
                                   g_param_spec_string ("icon-address",
                                                        "Icon address",
                                                        "The embed icon's address",
                                                        NULL,
                                                        (G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)));
  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN_POPUP_COUNT,
                                   g_param_spec_int ("hidden-popup-count",
                                                     "Number of Blocked Popups",
                                                     "The embed's number of blocked popup windows",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_POPUPS_ALLOWED,
                                   g_param_spec_boolean ("popups-allowed",
                                                         "Popups Allowed",
                                                         "Whether popup windows are to be displayed",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

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
