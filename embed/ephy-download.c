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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-download.h"

#include <libgnomevfs/gnome-vfs-uri.h>

static void
ephy_download_class_init (EphyDownloadClass *klass);
static void
ephy_download_init (EphyDownload *dv);

enum
{
	CHANGED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_download_signals[LAST_SIGNAL] = { 0 };

GType
ephy_download_get_type (void)
{
       static GType ephy_download_type = 0;

        if (ephy_download_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyDownloadClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_download_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (EphyDownload),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) ephy_download_init
                };

                ephy_download_type = g_type_register_static (G_TYPE_OBJECT,
                                                             "EphyDownload",
                                                             &our_info, 0);
        }

        return ephy_download_type;
}

static void
ephy_download_class_init (EphyDownloadClass *klass)
{
        parent_class = g_type_class_peek_parent (klass);

	ephy_download_signals[CHANGED] =
                g_signal_new ("changed",
                              EPHY_TYPE_DOWNLOAD,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyDownloadClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
}

static void
ephy_download_init (EphyDownload *dv)
{
}

EphyDownload *
ephy_download_new (void)
{
	return EPHY_DOWNLOAD (g_object_new (EPHY_TYPE_DOWNLOAD, NULL));
}

char *
ephy_download_get_name (EphyDownload *download)
{
	GnomeVFSURI *uri;
	char *target;
	char *result;

	target = ephy_download_get_target (download);

	uri = gnome_vfs_uri_new (target);
	if (uri)
	{
		result = gnome_vfs_uri_extract_short_name (uri);
		gnome_vfs_uri_unref (uri);
	}
	else
	{
		result = g_strdup ("Unknown");
	}

	return result;
}

long
ephy_download_get_remaining_time (EphyDownload *download)
{
	long elapsed_time, remaining_time = 0;
	long total, cur;

	total = ephy_download_get_total_progress (download);
	cur = ephy_download_get_current_progress (download);
	elapsed_time = ephy_download_get_elapsed_time (download);

	if (cur > 0)
	{
		remaining_time = elapsed_time * (total - cur) / cur;
	}

	return remaining_time;
}

char *
ephy_download_get_source (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_source (download);
}

char *
ephy_download_get_target (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_target (download);
}

long
ephy_download_get_current_progress (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_current_progress (download);
}

long
ephy_download_get_total_progress (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_total_progress (download);
}

int
ephy_download_get_percent (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_percent (download);
}

long
ephy_download_get_elapsed_time (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_elapsed_time (download);
}

EphyDownloadState
ephy_download_get_state (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_state (download);
}

void
ephy_download_cancel (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	klass->cancel (download);
}

void
ephy_download_pause (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	klass->pause (download);
}

void
ephy_download_resume (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	klass->resume (download);
}
