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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef MOZILLA_DOWNLOAD_H
#define MOZILLA_DOWNLOAD_H

#include <glib.h>
#include <glib-object.h>

#include "ephy-download.h"

G_BEGIN_DECLS

#define MOZILLA_TYPE_DOWNLOAD		(mozilla_download_get_type ())
#define MOZILLA_DOWNLOAD(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MOZILLA_TYPE_DOWNLOAD, MozillaDownload))
#define MOZILLA_DOWNLOAD_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MOZILLA_TYPE_DOWNLOAD, MozillaDownloadClass))
#define MOZILLA_IS_DOWNLOAD(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MOZILLA_TYPE_DOWNLOAD))
#define MOZILLA_IS_DOWNLOAD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MOZILLA_TYPE_DOWNLOAD))
#define MOZILLA_DOWNLOAD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MOZILLA_TYPE_DOWNLOAD, MozillaDownloadClass))

typedef struct _MozillaDownloadClass	MozillaDownloadClass;
typedef struct _MozillaDownload		MozillaDownload;
typedef struct _MozillaDownloadPrivate	MozillaDownloadPrivate;

class MozDownload;

struct _MozillaDownload
{
	EphyDownload parent;
	MozillaDownloadPrivate *priv;
};

struct _MozillaDownloadClass
{
	EphyDownloadClass parent_class;
};

GType		 mozilla_download_get_type	(void);

EphyDownload	*mozilla_download_new		(MozDownload *download);

G_END_DECLS

#endif
