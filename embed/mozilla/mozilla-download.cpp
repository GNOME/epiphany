/*
 *  Copyright (C) 2003 Xan Lopez
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
 * $Id$
 */

#include "mozilla-download.h"

#include "nsString.h"

static void
mozilla_download_class_init (MozillaDownloadClass *klass);
static void
mozilla_download_init (MozillaDownload *ges);
static void
mozilla_download_finalize (GObject *object);

static GObjectClass *parent_class = NULL;

GType
mozilla_download_get_type (void)
{
       static GType mozilla_download_type = 0;

        if (mozilla_download_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (MozillaDownloadClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) mozilla_download_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (MozillaDownload),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) mozilla_download_init
                };

                mozilla_download_type = 
				g_type_register_static (EPHY_TYPE_DOWNLOAD,
                                                        "MozillaDownload",
                                                        &our_info, (GTypeFlags)0);
        }

        return mozilla_download_type;
}

static char *
impl_get_source (EphyDownload *download)
{
	nsCOMPtr<nsILocalFile> targetFile;
	MozDownload *mozDownload;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetTarget (getter_AddRefs (targetFile));

	nsCAutoString tempPathStr;
	targetFile->GetNativePath (tempPathStr);

	return g_strdup (tempPathStr.get ());
}

static char *
impl_get_target (EphyDownload *download)
{
	nsCOMPtr<nsIURI> uri;
	MozDownload *mozDownload;
	nsCString spec;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetSource (getter_AddRefs (uri));
	uri->GetSpec (spec);

	return g_strdup (spec.get());
}

static long
impl_get_current_progress (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt32 progress;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetCurrentProgress (&progress);

	return progress;
}

static EphyDownloadState
impl_get_state (EphyDownload *download)
{
	MozDownload *mozDownload;
	EphyDownloadState state;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetState (&state);

	return state;
}

static long
impl_get_total_progress (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt32 progress;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetTotalProgress (&progress);

	return progress;
}

static int
impl_get_percent (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt32 percent;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetPercentComplete (&percent);

	return percent;
}

static long
impl_get_elapsed_time (EphyDownload *download)
{
	MozDownload *mozDownload;
	PRInt64 elapsed;

	mozDownload = MOZILLA_DOWNLOAD (download)->moz_download;

	mozDownload->GetElapsedTime (&elapsed);

	return elapsed / 1000000;
}

static void 
impl_cancel (EphyDownload *download)
{
	MOZILLA_DOWNLOAD (download)->moz_download->Cancel ();
}

static void
impl_pause (EphyDownload *download)
{
}

static void
impl_resume (EphyDownload *download)
{
}

static void
mozilla_download_class_init (MozillaDownloadClass *klass)
{
	EphyDownloadClass *download_class = EPHY_DOWNLOAD_CLASS (klass);
	
        parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	download_class->get_elapsed_time = impl_get_elapsed_time;
	download_class->get_current_progress = impl_get_current_progress;
	download_class->get_total_progress = impl_get_total_progress;
	download_class->get_percent = impl_get_percent;
	download_class->get_target = impl_get_target;
	download_class->get_source = impl_get_source;
	download_class->get_state = impl_get_state;
	download_class->cancel = impl_cancel;
	download_class->pause = impl_pause;
	download_class->resume = impl_resume;
}

static void
mozilla_download_init (MozillaDownload *view)
{
}

EphyDownload *
mozilla_download_new (void)
{
	return EPHY_DOWNLOAD (g_object_new (MOZILLA_TYPE_DOWNLOAD, NULL));
}
