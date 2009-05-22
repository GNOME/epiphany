/*
 *  Copyright © 2000, 2001, 2002 Marco Pesenti Gritti
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

#ifndef DOWNLOADER_VIEW_H
#define DOWNLOADER_VIEW_H

#include "ephy-dialog.h"

#include <webkit/webkit.h>
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOADER_VIEW		(downloader_view_get_type ())
#define EPHY_DOWNLOADER_VIEW(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderView))
#define EPHY_DOWNLOADER_VIEW_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderViewClass))
#define EPHY_IS_DOWNLOADER_VIEW(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_DOWNLOADER_VIEW))
#define EPHY_IS_DOWNLOADER_VIEW_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_DOWNLOADER_VIEW))
#define EPHY_DOWNLOADER_VIEW_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderViewClass))

typedef struct _DownloaderView		DownloaderView;
typedef struct _DownloaderViewPrivate	DownloaderViewPrivate;
typedef struct _DownloaderViewClass	DownloaderViewClass;

struct _DownloaderView
{
        EphyDialog parent;

	/*< private >*/
        DownloaderViewPrivate *priv;
};

struct _DownloaderViewClass
{
        EphyDialogClass parent_class;
};

/* These represent actions to be performed after the download is
 * successfully completed; NONE means no download will happen,
 * DOWNLOAD is just a way to tell the mime content handler that a file
 * chooser should be displayed so that the user can select where to
 * download to, and is usually turned into OPEN_LOCATION after that
 * happens (in other words, DOWNLOAD will never be an action when the
 * download is finished). OPEN will try to run the default application
 * that handles that file type.
 */
typedef enum
{
        DOWNLOAD_ACTION_NONE,
        DOWNLOAD_ACTION_DOWNLOAD,
        DOWNLOAD_ACTION_OPEN,
        DOWNLOAD_ACTION_OPEN_LOCATION
} DownloadAction;

GType           downloader_view_get_type              (void);

DownloaderView *downloader_view_new                   (void);

void            downloader_view_add_download          (DownloaderView *dv,
						       WebKitDownload *download);

void            downloader_view_remove_download       (DownloaderView *dv,
						       WebKitDownload *download);

G_END_DECLS

#endif
