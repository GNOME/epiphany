/* vim: set foldmethod=marker sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-download.c
 * This file is part of Epiphany
 *
 * Copyright © 2011 - Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef _EPHY_DOWNLOAD_WIDGET_H
#define _EPHY_DOWNLOAD_WIDGET_H

#include <glib-object.h>
#include "ephy-download.h"

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOAD_WIDGET              ephy_download_widget_get_type()
#define EPHY_DOWNLOAD_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_DOWNLOAD_WIDGET, EphyDownloadWidget))
#define EPHY_DOWNLOAD_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_DOWNLOAD_WIDGET, EphyDownloadWidgetClass))
#define EPHY_IS_DOWNLOAD_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_DOWNLOAD_WIDGET))
#define EPHY_IS_DOWNLOAD_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_DOWNLOAD_WIDGET))
#define EPHY_DOWNLOAD_WIDGET_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_DOWNLOAD_WIDGET, EphyDownloadWidgetClass))

typedef struct _EphyDownloadWidget EphyDownloadWidget;
typedef struct _EphyDownloadWidgetClass EphyDownloadWidgetClass;
typedef struct _EphyDownloadWidgetPrivate EphyDownloadWidgetPrivate;

struct _EphyDownloadWidget
{
  GtkBox parent;

  EphyDownloadWidgetPrivate *priv;
};

struct _EphyDownloadWidgetClass
{
  GtkBoxClass parent_class;
};

GType          ephy_download_widget_get_type             (void) G_GNUC_CONST;

GtkWidget     *ephy_download_widget_new                  (EphyDownload *ephy_download);

EphyDownload  *ephy_download_widget_get_download         (EphyDownloadWidget *widget);

gboolean       ephy_download_widget_download_is_finished (EphyDownloadWidget *widget);

G_END_DECLS

#endif /* _EPHY_DOWNLOAD_WIDGET_H */
