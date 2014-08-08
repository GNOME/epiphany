/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_CERTIFICATE_POPOVER_H
#define EPHY_CERTIFICATE_POPOVER_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_CERTIFICATE_POPOVER            (ephy_certificate_popover_get_type())
#define EPHY_CERTIFICATE_POPOVER(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_CERTIFICATE_POPOVER, EphyCertificatePopover))
#define EPHY_IS_CERTIFICATE_POPOVER(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_CERTIFICATE_POPOVER))
#define EPHY_CERTIFICATE_POPOVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_CERTIFICATE_POPOVER, EphyCertificatePopoverClass))
#define EPHY_IS_CERTIFICATE_POPOVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_CERTIFICATE_POPOVER))
#define EPHY_CERTIFICATE_POPOVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_CERTIFICATE_POPOVER, EphyCertificatePopoverClass))

typedef struct _EphyCertificatePopover        EphyCertificatePopover;
typedef struct _EphyCertificatePopoverClass   EphyCertificatePopoverClass;
typedef struct _EphyCertificatePopoverPrivate EphyCertificatePopoverPrivate;

struct _EphyCertificatePopover
{
  GtkPopover parent_object;

  /*< private >*/
  EphyCertificatePopoverPrivate *priv;
};

struct _EphyCertificatePopoverClass
{
  GtkPopoverClass parent_class;
};

GType      ephy_certificate_popover_get_type (void);

GtkWidget *ephy_certificate_popover_new      (GtkWidget *relative_to,
                                              const char *address,
                                              GTlsCertificate *certificate,
                                              GTlsCertificateFlags tls_errors,
                                              EphySecurityLevel security_level);

G_END_DECLS

#endif
