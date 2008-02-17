/*
 *  Copyright © 2003 Xan Lopez
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
 * $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <nsStringAPI.h>

#include <nsILocalFile.h>
#include <nsIMIMEInfo.h>
#include <nsIURI.h>
#include <nsMemory.h>

#include "ephy-debug.h"

#include "MozDownload.h"

#include "mozilla-download.h"

static void mozilla_download_class_init	(MozillaDownloadClass *klass);
static void mozilla_download_init	(MozillaDownload *ges);
static void mozilla_download_finalize	(GObject *object);

enum
{
	PROP_0,
	PROP_MOZDOWNLOAD
};

struct _MozillaDownloadPrivate
{
	MozDownload *moz_download;
};

#define MOZILLA_DOWNLOAD_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_DOWNLOAD, MozillaDownloadPrivate))

G_DEFINE_TYPE (MozillaDownload, mozilla_download, EPHY_TYPE_DOWNLOAD)

static char *
impl_get_target (EphyDownload *download)
{
	nsCOMPtr<nsILocalFile> targetFile;
	MozDownload *mozDownload;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetTargetFile (getter_AddRefs (targetFile));

	nsCString tempPathStr;
	targetFile->GetNativePath (tempPathStr);

	return g_strdup (tempPathStr.get ());
}

static char *
impl_get_source (EphyDownload *download)
{
	nsCOMPtr<nsIURI> uri;
	MozDownload *mozDownload;
	nsCString spec;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetSource (getter_AddRefs (uri));
	uri->GetSpec (spec);

	return g_strdup (spec.get());
}

static gint64
impl_get_current_progress (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt64 progress;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetCurrentProgress (&progress);

	return progress;
}

static EphyDownloadState
impl_get_state (EphyDownload *download)
{
	MozDownload *mozDownload;
	EphyDownloadState state;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetState (&state);

	return state;
}

static gint64
impl_get_total_progress (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt64 progress;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetTotalProgress (&progress);

	return progress;
}

static int
impl_get_percent (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt32 percent;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetPercentComplete (&percent);

	return percent;
}

static gint64
impl_get_elapsed_time (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt64 elapsed;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetElapsedTime (&elapsed);

	return elapsed / 1000000;
}

static char*
impl_get_mime (EphyDownload *download)
{
	MozDownload *mozDownload;
	nsCOMPtr<nsIMIMEInfo> mime;
	nsCString mimeType;

	mozDownload = MOZILLA_DOWNLOAD (download)->priv->moz_download;

	mozDownload->GetMIMEInfo (getter_AddRefs(mime));
        if (!mime) return g_strdup ("application/octet-stream");

        mime->GetMIMEType(mimeType);

	return g_strdup (mimeType.get());
}

static void 
impl_cancel (EphyDownload *download)
{
	MOZILLA_DOWNLOAD (download)->priv->moz_download->Cancel ();
}

static void
impl_pause (EphyDownload *download)
{
	MOZILLA_DOWNLOAD (download)->priv->moz_download->Pause ();
}

static void
impl_resume (EphyDownload *download)
{
	MOZILLA_DOWNLOAD (download)->priv->moz_download->Resume ();
}

static void
mozilla_download_finalize (GObject *object)
{
        MozillaDownload *download = MOZILLA_DOWNLOAD (object);

	NS_RELEASE (download->priv->moz_download);

	LOG ("MozillaDownload %p finalised", object);

        G_OBJECT_CLASS (mozilla_download_parent_class)->finalize (object);
}

static void
mozilla_download_set_property (GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
        MozillaDownload *download = MOZILLA_DOWNLOAD (object);

        switch (prop_id)
        {
                case PROP_MOZDOWNLOAD:
			MozDownload *moz_download;

			moz_download = (MozDownload *)g_value_get_pointer (value);
			NS_ADDREF (moz_download);
			download->priv->moz_download = moz_download;
                        break;
        }
}

static void
mozilla_download_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
        MozillaDownload *download = MOZILLA_DOWNLOAD (object);

        switch (prop_id)
        {
                case PROP_MOZDOWNLOAD:
                        g_value_set_pointer (value, download->priv->moz_download);
                        break;
        }
}

static void
mozilla_download_class_init (MozillaDownloadClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyDownloadClass *download_class = EPHY_DOWNLOAD_CLASS (klass);
	
	object_class->finalize = mozilla_download_finalize;
	object_class->set_property = mozilla_download_set_property;
	object_class->get_property = mozilla_download_get_property;

	download_class->get_elapsed_time = impl_get_elapsed_time;
	download_class->get_current_progress = impl_get_current_progress;
	download_class->get_total_progress = impl_get_total_progress;
	download_class->get_percent = impl_get_percent;
	download_class->get_target = impl_get_target;
	download_class->get_source = impl_get_source;
	download_class->get_state = impl_get_state;
        download_class->get_mime = impl_get_mime;
	download_class->cancel = impl_cancel;
	download_class->pause = impl_pause;
	download_class->resume = impl_resume;

	g_type_class_add_private (klass, sizeof (MozillaDownloadPrivate));

	g_object_class_install_property (object_class,
					 PROP_MOZDOWNLOAD,
					 g_param_spec_pointer ("mozilla-download",
							       "Mozilla Download",
							       "Mozilla Download",
							       (GParamFlags)
							       (G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							       G_PARAM_CONSTRUCT_ONLY)));
}

static void
mozilla_download_init (MozillaDownload *download)
{
	LOG ("MozillaDownload %p initialising", download);

	download->priv = MOZILLA_DOWNLOAD_GET_PRIVATE (download);
}

EphyDownload *
mozilla_download_new (MozDownload *download)
{
	return EPHY_DOWNLOAD (g_object_new (MOZILLA_TYPE_DOWNLOAD,
					    "mozilla-download", download,
					    NULL));
}
