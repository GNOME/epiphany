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
#include "config.h"
#endif

#include "mozilla-embed-persist.h"
#include "mozilla-embed.h"
#include "EphyBrowser.h"
#include "EphyHeaderSniffer.h"
#include "MozDownload.h"

#include <stddef.h>
#include <nsIWebBrowserPersist.h>
#include <nsString.h>
#include <nsCWebBrowserPersist.h>
#include <nsNetUtil.h>
#include <nsIHistoryEntry.h>
#include <nsISHEntry.h>

static void
mozilla_embed_persist_class_init (MozillaEmbedPersistClass *klass);
static void
mozilla_embed_persist_init (MozillaEmbedPersist *ges);
static void
mozilla_embed_persist_finalize (GObject *object);

static gboolean
impl_save (EphyEmbedPersist *persist);
static void
impl_cancel (EphyEmbedPersist *persist);

#define MOZILLA_EMBED_PERSIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_PERSIST, MozillaEmbedPersistPrivate))

struct MozillaEmbedPersistPrivate
{
	nsCOMPtr<nsIWebBrowserPersist> mPersist;
};

static GObjectClass *parent_class = NULL;

GType
mozilla_embed_persist_get_type (void)
{
       static GType mozilla_embed_persist_type = 0;

        if (mozilla_embed_persist_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (MozillaEmbedPersistClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) mozilla_embed_persist_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (MozillaEmbedPersist),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) mozilla_embed_persist_init
                };

                mozilla_embed_persist_type = 
				g_type_register_static (EPHY_TYPE_EMBED_PERSIST,
                                                        "MozillaEmbedPersist",
                                                        &our_info, (GTypeFlags)0);
        }

        return mozilla_embed_persist_type;
}

static void
mozilla_embed_persist_class_init (MozillaEmbedPersistClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEmbedPersistClass *persist_class = EPHY_EMBED_PERSIST_CLASS (klass);
	
        parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
	
        object_class->finalize = mozilla_embed_persist_finalize;

	persist_class->save = impl_save;
	persist_class->cancel = impl_cancel;

	g_type_class_add_private (object_class, sizeof(MozillaEmbedPersistPrivate));
}

static void
mozilla_embed_persist_init (MozillaEmbedPersist *persist)
{
        persist->priv = MOZILLA_EMBED_PERSIST_GET_PRIVATE (persist);

      	persist->priv->mPersist = do_CreateInstance (NS_WEBBROWSERPERSIST_CONTRACTID);
}

static void
mozilla_embed_persist_finalize (GObject *object)
{
        MozillaEmbedPersist *persist = MOZILLA_EMBED_PERSIST (object);

	persist->priv->mPersist = nsnull;

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
mozilla_embed_persist_completed (MozillaEmbedPersist *persist)
{
	g_signal_emit_by_name (persist, "completed");
	g_object_unref (persist);
}

void
mozilla_embed_persist_cancelled (MozillaEmbedPersist *persist)
{
	g_object_unref (persist);
}

static void
impl_cancel (EphyEmbedPersist *persist)
{
	nsCOMPtr<nsIWebBrowserPersist> bpersist =
	MOZILLA_EMBED_PERSIST (persist)->priv->mPersist;
	if (bpersist)
	{
		bpersist->CancelSave ();
	}

	g_object_unref (persist);
}

static gboolean
impl_save (EphyEmbedPersist *persist)
{
	nsresult rv;
	char *filename;
	char *uri;
	int max_size;
	EphyEmbed *embed;
	EmbedPersistFlags flags;
	PRUint32 persistFlags = 0;

	/* FIXME implement max size */

	g_object_ref (persist);
	
	g_object_get (persist,
		      "source", &uri,        
		      "dest", &filename,
		      "flags", &flags,
		      "embed", &embed,
		      "max_size", &max_size,
		      NULL);

	g_return_val_if_fail (!(flags & EMBED_PERSIST_COPY_PAGE)
			      || embed != NULL, FALSE);	
	g_return_val_if_fail (filename != NULL, FALSE);

	EphyBrowser *browser = NULL;
	if (embed)
	{
	        browser = (EphyBrowser *) mozilla_embed_get_ephy_browser (MOZILLA_EMBED(embed));
		g_return_val_if_fail (browser != NULL, FALSE);
	}

	/* we must have one of uri or browser */
	g_assert (browser != NULL || uri != NULL);

	/* Get a temp filename to save to */
	nsCOMPtr<nsIProperties> dirService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
	if (!dirService) return FALSE;
	nsCOMPtr<nsIFile> tmpFile;
	dirService->Get("TmpD", NS_GET_IID(nsIFile), getter_AddRefs(tmpFile));
	static short unsigned int tmpRandom = 0;
	nsAutoString tmpNo;
	tmpNo.AppendInt(tmpRandom++);
	nsAutoString saveFile(NS_LITERAL_STRING("-sav"));
	saveFile += tmpNo;
	saveFile += NS_LITERAL_STRING("tmp");
	tmpFile->Append(saveFile);

	/* Get the uri to save to */
	nsCOMPtr<nsIURI> inURI;
	nsCAutoString sURI;
	if (uri)
	{
		sURI.Assign (uri);
	}
	else
	{
		rv = browser->GetDocumentUrl (sURI);
		if (NS_FAILED(rv)) return FALSE;
	}
      	rv = NS_NewURI(getter_AddRefs(inURI), sURI);
	if (NS_FAILED(rv) || !inURI) return FALSE;

	/* Get post data */
	nsCOMPtr<nsIInputStream> postData;
	if (browser)
	{
		PRInt32 sindex;

		nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(browser->mWebBrowser));
		nsCOMPtr<nsISHistory> sessionHistory;
		webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
		nsCOMPtr<nsIHistoryEntry> entry;
		sessionHistory->GetIndex(&sindex);
		sessionHistory->GetEntryAtIndex(sindex, PR_FALSE, getter_AddRefs(entry));
		nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(entry));
		if (shEntry)
		{
			shEntry->GetPostData(getter_AddRefs(postData));
		}
	}

	/* Get the DOM document if a uri is not specified */
	nsCOMPtr<nsIDOMDocument> DOMDocument;
	if (!uri)
	{		
		if (flags & EMBED_PERSIST_MAINDOC)
		{
                	rv = browser->GetDocument (getter_AddRefs(DOMDocument));
		}
        	else
		{
                	rv = browser->GetTargetDocument (getter_AddRefs(DOMDocument));
		}
        	if (NS_FAILED(rv) || !DOMDocument) return FALSE;
	}


	/* Get the current page descriptor */
	nsCOMPtr<nsISupports> pageDescriptor;
	if (flags & EMBED_PERSIST_COPY_PAGE)
	{
	        rv = browser->GetPageDescriptor(getter_AddRefs(pageDescriptor));
	}

	if (filename == NULL)
	{
		/* Create an header sniffer and do the save */
		nsCOMPtr<nsIWebBrowserPersist> webPersist =
			MOZILLA_EMBED_PERSIST (persist)->priv->mPersist;
		if (!webPersist) return FALSE;

		EphyHeaderSniffer* sniffer = new EphyHeaderSniffer
			(webPersist, MOZILLA_EMBED_PERSIST (persist),
			 tmpFile, inURI, DOMDocument, postData);
		if (!sniffer) return FALSE;
 
		webPersist->SetProgressListener(sniffer);
		rv = webPersist->SaveURI(inURI, nsnull, nsnull, nsnull, nsnull, tmpFile);
		if (NS_FAILED (rv)) return FALSE;
	}
	else
	{
		/* Filename to save to */
		nsCOMPtr<nsILocalFile> destFile;
		rv = NS_NewNativeLocalFile (nsDependentCString(filename),
				            PR_TRUE, getter_AddRefs(destFile));
	        if (NS_FAILED(rv) || !destFile) return FALSE;

		rv =  InitiateMozillaDownload (DOMDocument, inURI, destFile,
					       nsnull, inURI, MOZILLA_EMBED_PERSIST (persist),
					       postData, pageDescriptor);
		if (NS_FAILED (rv)) return FALSE;
	}

	return TRUE;
}
