/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DOWNLOADER_VIEW_H
#define DOWNLOADER_VIEW_H

#include "ephy-dialog.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOADER_VIEW		(downloader_view_get_type ())
#define EPHY_DOWNLOADER_VIEW(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderView))
#define EPHY_DOWNLOADER_VIEW_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderViewClass))
#define EPHY_IS_DOWNLOADER_VIEW(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_DOWNLOADER_VIEW))
#define EPHY_IS_DOWNLOADER_VIEW_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_DOWNLOADER_VIEW))
#define EPHY_DOWNLOADER_VIEW_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderViewClass))

typedef struct DownloaderView DownloaderView;
typedef struct DownloaderViewClass DownloaderViewClass;
typedef struct DownloaderViewPrivate DownloaderViewPrivate;

typedef enum
{
	DOWNLOAD_STATUS_DOWNLOADING,
	DOWNLOAD_STATUS_PAUSED,
	DOWNLOAD_STATUS_RESUMING,
	DOWNLOAD_STATUS_COMPLETED
} DownloadStatus;

struct DownloaderView
{
        EphyDialog parent;
        DownloaderViewPrivate *priv;
};

struct DownloaderViewClass
{
        EphyDialogClass parent_class;

	void (*download_remove)    (DownloaderView *dv, gpointer persist_object);
	void (*download_pause)     (DownloaderView *dv, gpointer persist_object);
	void (*download_resume)    (DownloaderView *dv, gpointer persist_object);
};

GType           downloader_view_get_type              (void);

DownloaderView *downloader_view_new                   (void);

void            downloader_view_add_download          (DownloaderView *dv,
						       gchar *filename,
						       gchar *source,
						       gchar *dest,
						       gpointer persist_object);

void            downloader_view_remove_download       (DownloaderView *dv,
						       gpointer persist_object);

void            downloader_view_set_download_status   (DownloaderView *dv,
						       DownloadStatus status,
						       gpointer persist_object);

void            downloader_view_set_download_progress (DownloaderView *dv,
						       glong elapsed,
						       glong remaining,
						       gfloat speed,
						       gint size_total,
						       gint size_done,
						       gfloat progress,
						       gpointer persist_object);

G_END_DECLS

#endif
