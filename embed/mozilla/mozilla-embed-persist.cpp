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
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "EphyBrowser.h"
#include "EphyHeaderSniffer.h"
#include "MozDownload.h"
#include "EphyUtils.h"

#include <stddef.h>

#include <nsIWebBrowserPersist.h>
#include <nsCWebBrowserPersist.h>
#include <nsIHistoryEntry.h>
#include <nsISHEntry.h>
#include <nsIDOMSerializer.h>
#include <nsIIOService.h>
#include <nsNetCID.h>

static void
mozilla_embed_persist_class_init (MozillaEmbedPersistClass *klass);
static void
mozilla_embed_persist_init (MozillaEmbedPersist *ges);
static void
mozilla_embed_persist_finalize (GObject *object);

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
	g_signal_emit_by_name (persist, "cancelled");
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
	long max_size;
	EphyEmbed *embed;
	EmbedPersistFlags flags;

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

	EphyBrowser *browser = NULL;
	if (embed)
	{
		browser = (EphyBrowser *) _mozilla_embed_get_ephy_browser (MOZILLA_EMBED(embed));

		g_object_unref (embed);

		NS_ENSURE_TRUE (browser, FALSE);
	}
	/* we must have one of uri or browser */
	g_assert (browser != NULL || uri != NULL);

	/* Get a temp filename to save to */
	char *tmp_filename, *base;
	base = g_build_filename (g_get_tmp_dir (), "sav-XXXXXX", NULL);
	tmp_filename = ephy_file_tmp_filename (base, "html");
	g_free (base);
	if (tmp_filename == NULL) return FALSE;

	nsCOMPtr<nsILocalFile> tmpFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
	tmpFile->InitWithNativePath (nsEmbedCString (tmp_filename));
	g_free (tmp_filename);

	/* Get the uri to save to */
	nsCOMPtr<nsIURI> inURI;
	nsEmbedCString sURI;
	if (uri)
	{
		sURI.Assign (uri);
	}
	else
	{
		rv = browser->GetDocumentUrl (sURI);
		NS_ENSURE_SUCCESS (rv, FALSE);
	}

	rv = EphyUtils::NewURI (getter_AddRefs(inURI), sURI);
	NS_ENSURE_SUCCESS (rv, FALSE);

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
                	browser->GetDocument (getter_AddRefs(DOMDocument));
		}
        	else
		{
                	browser->GetTargetDocument (getter_AddRefs(DOMDocument));
		}
        	NS_ENSURE_TRUE (DOMDocument, FALSE);
	}


	/* Get the current page descriptor */
	nsCOMPtr<nsISupports> pageDescriptor;
	if (browser)
	{
	        browser->GetPageDescriptor(getter_AddRefs(pageDescriptor));
	}

	/* if we have COPY_PAGE, we *need* to have a page descriptor, else we'll re-fetch
	 * the page, which will possibly give a different page than the original which we
	 * need for view source
	 */
	NS_ENSURE_TRUE (!(flags & EMBED_PERSIST_COPY_PAGE) || pageDescriptor, FALSE);

	if (filename == NULL)
	{
		/* Create an header sniffer and do the save */
		nsCOMPtr<nsIWebBrowserPersist> webPersist =
			MOZILLA_EMBED_PERSIST (persist)->priv->mPersist;
		NS_ENSURE_TRUE (webPersist, FALSE);

		EphyHeaderSniffer* sniffer = new EphyHeaderSniffer
			(webPersist, MOZILLA_EMBED_PERSIST (persist),
			 tmpFile, inURI, DOMDocument, postData);
		if (!sniffer) return FALSE;
 
		webPersist->SetProgressListener(sniffer);
		rv = webPersist->SaveURI(inURI, pageDescriptor, nsnull, nsnull, nsnull, tmpFile);
		if (NS_FAILED (rv)) return FALSE;
	}
	else
	{
		/* Filename to save to */
		nsCOMPtr<nsILocalFile> destFile;
		NS_NewNativeLocalFile (nsEmbedCString(filename),
				       PR_TRUE, getter_AddRefs(destFile));
	        NS_ENSURE_TRUE (destFile, FALSE);

		rv =  InitiateMozillaDownload (DOMDocument, inURI, destFile,
					       nsnull, inURI, MOZILLA_EMBED_PERSIST (persist),
					       postData, pageDescriptor, max_size);
		if (NS_FAILED (rv)) return FALSE;
	}

	g_free (uri);
	g_free (filename);

	return TRUE;
}

static char *
impl_to_string (EphyEmbedPersist *persist)
{
	EphyEmbed *embed;
	nsCOMPtr<nsIDOMDocument> DOMDocument;
	EmbedPersistFlags flags;
	EphyBrowser *browser;
	nsresult rv = NS_OK;

	g_object_ref (persist);
	
	g_object_get (persist,
	              "flags", &flags,
	              "embed", &embed,
	              NULL);
	g_object_unref (persist);
	g_assert (embed != NULL);

	browser = (EphyBrowser *) _mozilla_embed_get_ephy_browser (MOZILLA_EMBED(embed));
	g_assert (browser != NULL);

	if (flags & EMBED_PERSIST_MAINDOC)
	{
		rv = browser->GetDocument (getter_AddRefs(DOMDocument));
	}
       	else
	{
		rv = browser->GetTargetDocument (getter_AddRefs(DOMDocument));
	}
	if (NS_FAILED(rv) || !DOMDocument) return NULL;

	nsCOMPtr<nsIDOMNode> node = do_QueryInterface(DOMDocument);
	if (!node) return NULL;

	nsCOMPtr<nsIDOMSerializer> serializer;
	serializer = do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsEmbedString outString;
	serializer->SerializeToString(node, outString);

	nsEmbedCString cOutString;
	NS_UTF16ToCString (outString, NS_CSTRING_ENCODING_UTF8, cOutString);

	return g_strdup (cOutString.get());
}

static GObject *
mozilla_embed_persist_constructor (GType type, guint n_construct_properties,
			           GObjectConstructParam *construct_params)
{
	/* we depend on single because of mozilla initialization */
	ephy_embed_shell_get_embed_single (embed_shell);

	return parent_class->constructor (type, n_construct_properties,
					  construct_params);
}

static void
mozilla_embed_persist_class_init (MozillaEmbedPersistClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEmbedPersistClass *persist_class = EPHY_EMBED_PERSIST_CLASS (klass);
	
        parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
	
        object_class->finalize = mozilla_embed_persist_finalize;
	object_class->constructor = mozilla_embed_persist_constructor;

	persist_class->save = impl_save;
	persist_class->cancel = impl_cancel;
	persist_class->to_string = impl_to_string;

	g_type_class_add_private (object_class, sizeof(MozillaEmbedPersistPrivate));
}
