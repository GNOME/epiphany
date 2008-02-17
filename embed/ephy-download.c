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
 *  $Id$
 */

#include "config.h"

#include "ephy-download.h"

#include <glib/gi18n.h>

#define EPHY_DOWNLOAD_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DOWNLOAD, EphyDownloadPrivate))

#define REMAINING_TIME_UPDATE_SECS 2

static void
ephy_download_class_init (EphyDownloadClass *klass);
static void
ephy_download_init (EphyDownload *dv);

enum
{
	CHANGED,
	LAST_SIGNAL
};

struct _EphyDownloadPrivate
{
	gint64 remaining_time_last_update;
	gint64 remaining_time;
};

static guint ephy_download_signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyDownload, ephy_download, G_TYPE_OBJECT)

static void
ephy_download_class_init (EphyDownloadClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

	ephy_download_signals[CHANGED] =
                g_signal_new ("changed",
                              EPHY_TYPE_DOWNLOAD,
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyDownloadClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);

	g_type_class_add_private (object_class, sizeof(EphyDownloadPrivate));
}

static void
ephy_download_init (EphyDownload *download)
{
	download->priv = EPHY_DOWNLOAD_GET_PRIVATE (download);

	download->priv->remaining_time = 0;
	download->priv->remaining_time_last_update = 0;
}

EphyDownload *
ephy_download_new (void)
{
	return EPHY_DOWNLOAD (g_object_new (EPHY_TYPE_DOWNLOAD, NULL));
}

char *
ephy_download_get_name (EphyDownload *download)
{
	char *target;
	char *result;

	target = ephy_download_get_target (download);

	if (target)
	{
		result = g_path_get_basename (target);
	}
	else
	{
		result = g_strdup (_("Unknown"));
	}

	g_free (target);

	return result;
}

static void
update_remaining_time (EphyDownload *download)
{
	gint64 elapsed_time, total, cur;

	total = ephy_download_get_total_progress (download);
	cur = ephy_download_get_current_progress (download);
	elapsed_time = ephy_download_get_elapsed_time (download);

	if (cur > 0)
	{
		float per_byte_time;

		per_byte_time = (float)elapsed_time / (float)cur;
		download->priv->remaining_time = per_byte_time * (total - cur);
	}
}

gint64
ephy_download_get_remaining_time (EphyDownload *download)
{
	gint64 elapsed_time;

	elapsed_time = ephy_download_get_elapsed_time (download);
	if (elapsed_time - download->priv->remaining_time_last_update >=
	    REMAINING_TIME_UPDATE_SECS)
	{
		update_remaining_time (download);
		download->priv->remaining_time_last_update = elapsed_time;
	}

	return download->priv->remaining_time;
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

gint64
ephy_download_get_current_progress (EphyDownload *download)
{
	EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
	return klass->get_current_progress (download);
}

gint64
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

gint64
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

char *
ephy_download_get_mime (EphyDownload *download)
{
        EphyDownloadClass *klass = EPHY_DOWNLOAD_GET_CLASS (download);
        return klass->get_mime (download);
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
