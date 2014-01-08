/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-download.h
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

#ifndef _EPHY_DOWNLOAD_H
#define _EPHY_DOWNLOAD_H

#include <glib-object.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOAD              ephy_download_get_type()
#define EPHY_DOWNLOAD(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_DOWNLOAD, EphyDownload))
#define EPHY_DOWNLOAD_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_DOWNLOAD, EphyDownloadClass))
#define EPHY_IS_DOWNLOAD(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_DOWNLOAD))
#define EPHY_IS_DOWNLOAD_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_DOWNLOAD))
#define EPHY_DOWNLOAD_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_DOWNLOAD, EphyDownloadClass))

typedef struct _EphyDownload EphyDownload;
typedef struct _EphyDownloadClass EphyDownloadClass;
typedef struct _EphyDownloadPrivate EphyDownloadPrivate;

struct _EphyDownload
{
  GObject parent;

  EphyDownloadPrivate *priv;
};

struct _EphyDownloadClass
{
  GObjectClass parent_class;

  void (* filename_suggested) (EphyDownload *download,
                               char *suggested_filename);
  void (* completed)  (EphyDownload *download);
  void (* error)      (EphyDownload *download,
                       gint error_code,
                       gint error_detail,
                       char *reason);
};

typedef enum
{
  EPHY_DOWNLOAD_ACTION_NONE,
  EPHY_DOWNLOAD_ACTION_AUTO,
  EPHY_DOWNLOAD_ACTION_BROWSE_TO,
  EPHY_DOWNLOAD_ACTION_OPEN,
  EPHY_DOWNLOAD_ACTION_DO_NOTHING
} EphyDownloadActionType;

GType         ephy_download_get_type              (void) G_GNUC_CONST;

EphyDownload *ephy_download_new                   (WebKitDownload *download,
                                                   GtkWindow *parent);
EphyDownload *ephy_download_new_for_uri           (const char *uri,
                                                   GtkWindow *parent);

void          ephy_download_cancel                (EphyDownload *download);

void          ephy_download_set_destination_uri   (EphyDownload *download,
                                                   const char *destination);

WebKitDownload *ephy_download_get_webkit_download (EphyDownload *download);

const char   *ephy_download_get_destination_uri   (EphyDownload *download);
char         *ephy_download_get_content_type      (EphyDownload *download);

guint32       ephy_download_get_start_time        (EphyDownload *download);

GtkWindow    *ephy_download_get_window            (EphyDownload *download);

EphyDownloadActionType ephy_download_get_action   (EphyDownload *download);
void          ephy_download_set_action            (EphyDownload *download,
                                                   EphyDownloadActionType action);
gboolean      ephy_download_do_download_action    (EphyDownload *download,
                                                   EphyDownloadActionType action);

GtkWidget    *ephy_download_get_widget            (EphyDownload *download);
void          ephy_download_set_widget            (EphyDownload *download,
                                                   GtkWidget *widget);

G_END_DECLS

#endif /* _EPHY_DOWNLOAD_H */
