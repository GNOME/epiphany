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

#include "ProgressListener.h"
#include "EphyWrapper.h"
#include "mozilla-embed.h"
#include "mozilla-embed-persist.h"

#include <stddef.h>
#include <nsIWebBrowserPersist.h>
#include <nsString.h>
#include <nsCWebBrowserPersist.h>
#include <nsNetUtil.h>

static void
mozilla_embed_persist_class_init (MozillaEmbedPersistClass *klass);
static void
mozilla_embed_persist_init (MozillaEmbedPersist *ges);
static void
mozilla_embed_persist_finalize (GObject *object);

static gresult 
impl_save (EphyEmbedPersist *persist);
static gresult 
impl_cancel (EphyEmbedPersist *persist);

#define MOZILLA_EMBED_PERSIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_PERSIST, MozillaEmbedPersistPrivate))

struct MozillaEmbedPersistPrivate
{
	nsCOMPtr<nsIWebBrowserPersist> mPersist;
	GProgressListener *mProgress;
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

	persist->priv->mPersist->SetProgressListener (nsnull);
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

static gresult 
impl_cancel (EphyEmbedPersist *persist)
{
	nsCOMPtr<nsIWebBrowserPersist> bpersist =
	MOZILLA_EMBED_PERSIST (persist)->priv->mPersist;
	if (!bpersist) return G_FAILED;

	bpersist->CancelSave ();

	g_object_unref (persist);

	return G_OK;
}

static gresult 
impl_save (EphyEmbedPersist *persist)
{
	nsresult rv;
	nsAutoString s;
	char *filename;
	char *uri;
	int max_size;
	EphyEmbed *embed;
	EmbedPersistFlags flags;
	EphyWrapper *wrapper = NULL;
	PRUint32 persistFlags = 0;

	g_object_ref (persist);
	
	g_object_get (persist,
		      "source", &uri,        
		      "dest", &filename,
		      "flags", &flags,
		      "embed", &embed,
		      "max_size", &max_size,
		      NULL);
	
	g_return_val_if_fail (filename != NULL, G_FAILED);
	
	nsCOMPtr<nsIWebBrowserPersist> bpersist =
		MOZILLA_EMBED_PERSIST (persist)->priv->mPersist;
	if (!bpersist) return G_FAILED;

	nsCOMPtr<nsIURI> linkURI;
	linkURI = nsnull;
	if (uri)
	{
        	s.AssignWithConversion(uri);
	      	rv = NS_NewURI(getter_AddRefs(linkURI), s);
      		if (NS_FAILED(rv) || !linkURI) return G_FAILED;
	}

        nsCOMPtr<nsILocalFile> file;
       	rv = NS_NewLocalFile(NS_ConvertUTF8toUCS2(filename), PR_TRUE, getter_AddRefs(file)); 
        if (NS_FAILED(rv) || !file) return G_FAILED;

	nsCOMPtr<nsILocalFile> path;
        if (flags & EMBED_PERSIST_SAVE_CONTENT)
	{
		char *datapath;
		datapath = g_strconcat (filename, ".content", NULL);
                NS_NewLocalFile(NS_ConvertUTF8toUCS2(datapath), PR_TRUE, getter_AddRefs(path));
		g_free (datapath);
	}
        else
	{
                path = nsnull;
	}
	
	nsCOMPtr<nsIDOMWindow> parent;
	parent = nsnull;
	
	if (embed)
	{
	        wrapper = (EphyWrapper *) mozilla_embed_get_ephy_wrapper (MOZILLA_EMBED(embed));
		g_return_val_if_fail (wrapper != NULL, G_FAILED);	
		wrapper->GetDOMWindow (getter_AddRefs (parent));
	}

	persistFlags = nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;

	size_t len = strlen(filename);
	if((filename[len-1] == 'z' && filename[len-2] == 'g') ||
	   (filename[len-1] == 'Z' && filename[len-2] == 'G'))
	{
                persistFlags |= nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION;
	}

	if (flags & EMBED_PERSIST_BYPASSCACHE)
	{
		persistFlags |= nsIWebBrowserPersist::PERSIST_FLAGS_BYPASS_CACHE;
	}

	if (flags & EMBED_PERSIST_FROMCACHE)
	{
		persistFlags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;
	}

	bpersist->SetPersistFlags (persistFlags);

	GProgressListener *aProgress = new GProgressListener ();
	MOZILLA_EMBED_PERSIST (persist)->priv->mProgress = aProgress;

	if (uri == NULL)
	{
		g_return_val_if_fail (wrapper != NULL, G_FAILED);
		
		nsCOMPtr<nsIDOMDocument> DOMDocument;

		if (flags & EMBED_PERSIST_MAINDOC)
		{
                	rv = wrapper->GetMainDOMDocument (getter_AddRefs(DOMDocument));
		}
        	else
		{
                	rv = wrapper->GetDOMDocument (getter_AddRefs(DOMDocument));
		}
        	if (NS_FAILED(rv) || !DOMDocument) return G_FAILED;

		nsCOMPtr<nsIDocument> document =
                                do_QueryInterface (DOMDocument, &rv);
        	if (NS_FAILED(rv) || !document) return G_FAILED;

	        nsCOMPtr<nsIURI> uri;
        	rv = document->GetDocumentURL (getter_AddRefs(uri));
        	if (NS_FAILED(rv) || !uri) return G_FAILED;

        	aProgress->InitForPersist (bpersist, parent,
                   	                   uri, file, 
                           	           ACTION_OBJECT_NOTIFY,
					   persist,
					   (flags & EMBED_PERSIST_SHOW_PROGRESS));

		rv = bpersist->SaveDocument (DOMDocument, file, path, nsnull, 0, 0);
		if (NS_FAILED(rv)) return G_FAILED;
	}
	else
	{
        	aProgress->InitForPersist (bpersist, parent,
                 	                   linkURI, file,
                         	           ACTION_OBJECT_NOTIFY,
                                 	   persist, 
					   (flags & EMBED_PERSIST_SHOW_PROGRESS));

		rv = bpersist->SaveURI (linkURI, nsnull, nsnull, nsnull, nsnull, file);
		if (NS_FAILED(rv)) return G_FAILED;
	}
	
	return G_OK;
}

