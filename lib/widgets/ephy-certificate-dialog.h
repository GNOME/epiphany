/*
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

#ifndef EPHY_CERTIFICATE_DIALOG_H
#define EPHY_CERTIFICATE_DIALOG_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_CERTIFICATE_DIALOG            (ephy_certificate_dialog_get_type())
#define EPHY_CERTIFICATE_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_CERTIFICATE_DIALOG, EphyCertificateDialog))
#define EPHY_IS_CERTIFICATE_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_CERTIFICATE_DIALOG))
#define EPHY_CERTIFICATE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_CERTIFICATE_DIALOG, EphyCertificateDialogClass))
#define EPHY_IS_CERTIFICATE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_CERTIFICATE_DIALOG))
#define EPHY_CERTIFICATE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_CERTIFICATE_DIALOG, EphyCertificateDialogClass))

typedef struct _EphyCertificateDialog        EphyCertificateDialog;
typedef struct _EphyCertificateDialogClass   EphyCertificateDialogClass;
typedef struct _EphyCertificateDialogPrivate EphyCertificateDialogPrivate;

struct _EphyCertificateDialog
{
        GtkDialog parent_object;

        /*< private >*/
        EphyCertificateDialogPrivate *priv;
};

struct _EphyCertificateDialogClass
{
        GtkDialogClass parent_class;
};

GType      ephy_certificate_dialog_get_type (void);

GtkWidget *ephy_certificate_dialog_new      (GtkWindow           *parent,
                                             const char          *address,
                                             GTlsCertificate     *certificate,
                                             GTlsCertificateFlags tls_errors);

G_END_DECLS

#endif
