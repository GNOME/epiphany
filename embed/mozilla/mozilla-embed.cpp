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

#include "gtkmozembed.h"
#include "gtkmozembed_internal.h"
#include "ephy-string.h"
#include "ephy-embed.h"
#include "ephy-debug.h"
#include "mozilla-embed.h"
#include "MozillaPrivate.h"
#include "EphyWrapper.h"
#include "EventContext.h"
#include "ephy-debug.h"

#include <nsIWindowWatcher.h>
#include <nsIURI.h>
#include <nsIURL.h>
#include <nsNetUtil.h>
#include <nsString.h>
#include <nsIRequest.h>
#include <nsIWebProgressListener.h>
#include <nsITransportSecurityInfo.h>
#include <nsIPrintOptions.h>
#include <nsGfxCIID.h>

static void
mozilla_embed_class_init (MozillaEmbedClass *klass);
static void
mozilla_embed_init (MozillaEmbed *gs);
static void
mozilla_embed_finalize (GObject *object);
static void
mozilla_embed_destroy (GtkObject *object);
static void
ephy_embed_init (EphyEmbedClass *embed_class);

static void
impl_get_capabilities (EphyEmbed *embed,
                       EmbedCapabilities *caps);
static gresult 
impl_load_url (EphyEmbed *embed, 
               const char *url);
static gresult 
impl_stop_load (EphyEmbed *embed);
static gresult 
impl_can_go_back (EphyEmbed *embed);
static gresult 
impl_can_go_forward (EphyEmbed *embed);
static gresult 
impl_can_go_up (EphyEmbed *embed);
static gresult
impl_get_go_up_list (EphyEmbed *embed, GSList **l);
static gresult 
impl_go_back (EphyEmbed *embed);
static gresult  
impl_go_forward (EphyEmbed *embed);
static gresult
impl_go_up (EphyEmbed *embed);
static gresult 
impl_render_data (EphyEmbed *embed, 
                  const char *data,
                  guint32 len,
                  const char *base_uri, 
                  const char *mime_type);
static gresult
impl_open_stream (EphyEmbed *embed,
                  const char *base_uri,
                  const char *mime_type);
static gresult
impl_append_data (EphyEmbed *embed,
                  const char *data, 
                  guint32 len);
static gresult
impl_close_stream (EphyEmbed *embed);
static gresult
impl_get_title (EphyEmbed *embed,
                char **title);
static gresult 
impl_get_location (EphyEmbed *embed, 
                   gboolean toplevel,
                   char **location);
static gresult 
impl_reload (EphyEmbed *embed, 
             EmbedReloadFlags flags);
static gresult
impl_copy_page (EphyEmbed *dest,
		EphyEmbed *source,
		EmbedDisplayType display_type);
static gresult
impl_zoom_set (EphyEmbed *embed, 
               float zoom, 
               gboolean reflow);
static gresult
impl_zoom_get (EphyEmbed *embed,
               float *zoom);
static gresult 
impl_selection_can_cut (EphyEmbed *embed);
static gresult 
impl_selection_can_copy (EphyEmbed *embed);
static gresult 
impl_can_paste (EphyEmbed *embed);
static gresult 
impl_select_all (EphyEmbed *embed);
static gresult
impl_selection_cut (EphyEmbed *embed);
static gresult
impl_selection_copy (EphyEmbed *embed);
static gresult
impl_paste (EphyEmbed *embed);
static gresult
impl_shistory_count  (EphyEmbed *embed,
                      int *count);
static gresult
impl_shistory_get_nth (EphyEmbed *embed, 
                       int nth,
                       gboolean is_relative,
                       char **url,
                       char **title);
static gresult
impl_shistory_get_pos (EphyEmbed *embed,
                       int *pos);
static gresult
impl_shistory_go_nth (EphyEmbed *embed, 
                      int nth);
static gboolean
impl_shistory_copy (EphyEmbed *source,
                    EphyEmbed *dest);
static gresult
impl_get_security_level (EphyEmbed *embed, 
                         EmbedSecurityLevel *level,
                         char **description);
static gresult
impl_find (EphyEmbed *embed,
           EmbedFindInfo *info);

static gresult
impl_set_encoding (EphyEmbed *embed,
                   const char *encoding);

static gresult
impl_print (EphyEmbed *embed, 
            EmbedPrintInfo *info);

static gresult
impl_print_preview_close (EphyEmbed *embed);

static gresult
impl_print_preview_num_pages (EphyEmbed *embed,
			      gint *retNum);
static gresult
impl_print_preview_navigate (EphyEmbed *embed,
			     EmbedPrintPreviewNavType navType,
			     gint pageNum);

static void 
mozilla_embed_connect_signals (MozillaEmbed *membed);
static char *
mozilla_embed_get_uri_parent (const char *uri);
static void
mozilla_embed_location_changed_cb (GtkMozEmbed *embed, MozillaEmbed *membed);
static void
mozilla_embed_title_changed_cb (GtkMozEmbed *embed, MozillaEmbed *membed);
static void
mozilla_embed_net_state_all_cb (GtkMozEmbed *embed, const char *aURI,
                                gint state, guint status, MozillaEmbed *membed);
static void
mozilla_embed_progress_cb (GtkMozEmbed *embed, const char *aURI,
                           gint curprogress, gint maxprogress, MozillaEmbed *membed);
static void
mozilla_embed_link_message_cb (GtkMozEmbed *embed, MozillaEmbed *membed);
static void
mozilla_embed_js_status_cb (GtkMozEmbed *embed, MozillaEmbed *membed);
static void
mozilla_embed_visibility_cb (GtkMozEmbed *embed, gboolean visibility, MozillaEmbed *membed);
static void
mozilla_embed_destroy_brsr_cb (GtkMozEmbed *embed, MozillaEmbed *embed);
static gint
mozilla_embed_dom_mouse_click_cb (GtkMozEmbed *embed, gpointer dom_event, MozillaEmbed *membed);
static gint
mozilla_embed_dom_mouse_down_cb (GtkMozEmbed *embed, gpointer dom_event, 
				 MozillaEmbed *membed);
static void
mozilla_embed_size_to_cb (GtkMozEmbed *embed, gint width, gint height, MozillaEmbed *membed);
static void
mozilla_embed_new_window_cb (GtkMozEmbed *embed, 
			     GtkMozEmbed **newEmbed,
                             guint chromemask, MozillaEmbed *membed);
static void
mozilla_embed_security_change_cb (GtkMozEmbed *embed, 
				  gpointer request,
                                  guint state, MozillaEmbed *membed);
static EmbedSecurityLevel
mozilla_embed_security_level (MozillaEmbed *membed);
static gint
mozilla_embed_dom_key_down_cb (GtkMozEmbed *embed, gpointer dom_event,
		               MozillaEmbed *membed);

/* signals to connect on each embed widget */
static const struct
{ 
	char *event; 
        void *func; /* should be a GtkSignalFunc or similar */
}
signal_connections[] =
{
	{ "location",        (void *) mozilla_embed_location_changed_cb  },
        { "title",           (void *) mozilla_embed_title_changed_cb     },
        { "net_state_all",   (void *) mozilla_embed_net_state_all_cb     },
        { "progress_all",    (void *) mozilla_embed_progress_cb          },
        { "link_message",    (void *) mozilla_embed_link_message_cb      },
        { "js_status",       (void *) mozilla_embed_js_status_cb         },
        { "visibility",      (void *) mozilla_embed_visibility_cb        },
        { "destroy_browser", (void *) mozilla_embed_destroy_brsr_cb      },
        { "dom_mouse_click", (void *) mozilla_embed_dom_mouse_click_cb   },
	{ "dom_mouse_down",  (void *) mozilla_embed_dom_mouse_down_cb    },
        { "size_to",         (void *) mozilla_embed_size_to_cb           },
        { "new_window",      (void *) mozilla_embed_new_window_cb        },
        { "security_change", (void *) mozilla_embed_security_change_cb   },
	{ "dom_key_down",    (void *) mozilla_embed_dom_key_down_cb      },

        /* terminator -- must be last in the list! */
        { NULL, NULL } 
};

struct MozillaEmbedPrivate
{
	MozillaEmbedPrivate() : wrapper(NULL), security_state(-1), no_page(1)
	{ /* nothing */ }

	EphyWrapper *wrapper;
	nsCOMPtr<nsIRequest> request;
	gint security_state;

	/* HACK 1: No page loaded, 0: Loading an empty page, -1: Page loaded */ 
	gint no_page;
};

#define WINDOWWATCHER_CONTRACTID "@mozilla.org/embedcomp/window-watcher;1"

static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

static GObjectClass *parent_class = NULL;

GType 
mozilla_embed_get_type (void)
{
        static GType mozilla_embed_type = 0;

        if (mozilla_embed_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (MozillaEmbedClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) mozilla_embed_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (MozillaEmbed),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) mozilla_embed_init
                };

		static const GInterfaceInfo embed_info =
		{
			(GInterfaceInitFunc) ephy_embed_init,      /* interface_init */
        		NULL,                                      /* interface_finalize */
        		NULL                                       /* interface_data */
     		 };
		
                mozilla_embed_type = g_type_register_static (GTK_TYPE_MOZ_EMBED,
							     "MozillaEmbed",
							     &our_info, 
							     (GTypeFlags)0);
		g_type_add_interface_static (mozilla_embed_type,
                                   	     EPHY_EMBED_TYPE,
                                   	     &embed_info);
        }

        return mozilla_embed_type;
}

static void
ephy_embed_init (EphyEmbedClass *embed_class)
{
	embed_class->get_capabilities = impl_get_capabilities;
	embed_class->load_url = impl_load_url; 
	embed_class->stop_load = impl_stop_load;
	embed_class->can_go_back = impl_can_go_back;
	embed_class->can_go_forward =impl_can_go_forward;
	embed_class->can_go_up = impl_can_go_up;
	embed_class->get_go_up_list = impl_get_go_up_list;
	embed_class->go_back = impl_go_back;
	embed_class->go_forward = impl_go_forward;
	embed_class->go_up = impl_go_up;
	embed_class->render_data = impl_render_data;
	embed_class->open_stream = impl_open_stream;
	embed_class->append_data = impl_append_data;
	embed_class->close_stream = impl_close_stream;
	embed_class->get_title = impl_get_title;
	embed_class->get_location = impl_get_location;
	embed_class->reload = impl_reload;
	embed_class->copy_page = impl_copy_page;
	embed_class->zoom_set = impl_zoom_set;
	embed_class->zoom_get = impl_zoom_get;
	embed_class->selection_can_cut = impl_selection_can_cut;
	embed_class->selection_can_copy = impl_selection_can_copy;
	embed_class->can_paste = impl_can_paste;
	embed_class->selection_cut = impl_selection_cut;
	embed_class->selection_copy = impl_selection_copy;
	embed_class->paste = impl_paste;
	embed_class->shistory_count = impl_shistory_count;
	embed_class->shistory_get_nth = impl_shistory_get_nth;
	embed_class->shistory_get_pos = impl_shistory_get_pos;
	embed_class->shistory_go_nth = impl_shistory_go_nth;
	embed_class->shistory_copy = impl_shistory_copy;
	embed_class->get_security_level = impl_get_security_level;
	embed_class->find = impl_find;
	embed_class->set_encoding = impl_set_encoding;
	embed_class->select_all = impl_select_all;
	embed_class->print = impl_print;
	embed_class->print_preview_close = impl_print_preview_close;
	embed_class->print_preview_num_pages = impl_print_preview_num_pages;
	embed_class->print_preview_navigate = impl_print_preview_navigate;
}

static void
mozilla_embed_class_init (MozillaEmbedClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
     	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass); 
	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
	
	gtk_object_class->destroy = mozilla_embed_destroy;
        object_class->finalize = mozilla_embed_finalize;
}

static void
mozilla_embed_init (MozillaEmbed *embed)
{
        embed->priv = g_new0 (MozillaEmbedPrivate, 1);

	mozilla_embed_connect_signals (embed);
}

gpointer
mozilla_embed_get_ephy_wrapper (MozillaEmbed *embed)
{
	g_return_val_if_fail (embed->priv->wrapper != NULL, NULL);
	
	return embed->priv->wrapper;
}

static void 
mozilla_embed_connect_signals (MozillaEmbed *embed)
{
        gint i;
	EphyEmbed *gembed;

	gembed = EPHY_EMBED (embed);
	       
        /* connect signals */
        for (i = 0; signal_connections[i].event != NULL; i++)
        {
                g_signal_connect (G_OBJECT(embed),
                                  signal_connections[i].event,
                                  G_CALLBACK(signal_connections[i].func), 
                                  embed);
        }
}

static void
mozilla_embed_destroy (GtkObject *object)
{
	int i;
	MozillaEmbed *embed = MOZILLA_EMBED (object);
	
	for (i = 0; signal_connections[i].event != NULL; i++)
        {
                g_signal_handlers_disconnect_by_func
			(G_OBJECT(object),
                         (gpointer)signal_connections[i].func, 
                         (void *)object);
        }

	if (embed->priv->wrapper)
	{
		embed->priv->wrapper->Destroy();
        	delete embed->priv->wrapper;
        	embed->priv->wrapper = NULL;
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
mozilla_embed_finalize (GObject *object)
{
        MozillaEmbed *embed;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_MOZILLA_EMBED (object));

	embed = MOZILLA_EMBED (object);
	
	g_return_if_fail (embed->priv != NULL);
	
	g_free (embed->priv);
	
        G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("MozillaEmbed finalized %p", embed)
}

static void
impl_get_capabilities (EphyEmbed *embed,
                       EmbedCapabilities *caps)
{
	EmbedCapabilities mozilla_caps;
	
	mozilla_caps = (EmbedCapabilities) ( 
		EMBED_CLIPBOARD_CAP |
	        EMBED_COOKIES_CAP |
	        EMBED_LINKS_CAP |
	        EMBED_ZOOM_CAP |
	        EMBED_PRINT_CAP |
	        EMBED_FIND_CAP |
	        EMBED_SECURITY_CAP |
	        EMBED_ENCODING_CAP |
	        EMBED_SHISTORY_CAP );
	
	*caps = mozilla_caps;
}

static gresult 
impl_load_url (EphyEmbed *embed, 
               const char *url)
{
        gtk_moz_embed_load_url (GTK_MOZ_EMBED(embed),
                                url);

	return G_OK;
}

static gresult 
impl_stop_load (EphyEmbed *embed)
{
	gtk_moz_embed_stop_load (GTK_MOZ_EMBED(embed));	
	
	return G_OK;
}

static gresult 
impl_can_go_back (EphyEmbed *embed)
{
	if (gtk_moz_embed_can_go_back (GTK_MOZ_EMBED(embed)))
	{
		return G_OK;
	}
	else
	{
		return G_FAILED;
	}
}

static gresult 
impl_can_go_forward (EphyEmbed *embed)
{
	if (gtk_moz_embed_can_go_forward (GTK_MOZ_EMBED(embed)))
	{
		return G_OK;
	}
	else
	{
		return G_FAILED;
	}

}

static gresult 
impl_can_go_up (EphyEmbed *embed)
{
	char *location;
	char *s;
	gresult result;

	if (ephy_embed_get_location (embed, TRUE, &location) != G_OK)
	  return G_FAILED;
	g_return_val_if_fail (location != NULL, G_FAILED);
	if ((s = mozilla_embed_get_uri_parent (location)) != NULL)
	{
		g_free (s);
		result = G_OK;
	}
	else
	{
		result = G_FAILED;
	}

	g_free (location);

	return result;
}

static gresult
impl_get_go_up_list (EphyEmbed *embed, GSList **l)
{
	char *location;
	char *s;
	
	if (ephy_embed_get_location (embed, TRUE, &location) != G_OK)
		return G_FAILED;
	g_return_val_if_fail (location != NULL, G_FAILED);
	
	*l = NULL;
	s = location;
	while ((s = mozilla_embed_get_uri_parent (s)) != NULL)
	{
		*l = g_slist_prepend (*l, s);
	}				

	g_free (location);
	*l = g_slist_reverse (*l);

	return G_OK;
}

static gresult 
impl_go_back (EphyEmbed *embed)
{
	gtk_moz_embed_go_back (GTK_MOZ_EMBED(embed));

	return G_OK;
}
		
static gresult  
impl_go_forward (EphyEmbed *embed)
{
	gtk_moz_embed_go_forward (GTK_MOZ_EMBED(embed));

	return G_OK;
}

static gresult
impl_go_up (EphyEmbed *embed)
{
	char *uri;
	char *parent_uri;
	
	ephy_embed_get_location (embed, TRUE, &uri);
	g_return_val_if_fail (uri != NULL, G_FAILED);
	
	parent_uri = mozilla_embed_get_uri_parent (uri);
	g_free (uri);
	g_return_val_if_fail (parent_uri != NULL, G_FAILED);
	
	ephy_embed_load_url (embed, parent_uri);

	g_free (parent_uri);

	return G_OK;
}

static char *
mozilla_embed_get_uri_parent (const char *aUri)
{
        nsresult rv;

        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI (getter_AddRefs(uri), aUri);
        if (NS_FAILED(rv) || !uri) return NULL;

        nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
        if (NS_FAILED(rv) || !url) return NULL;

        nsCAutoString dirPath;
        rv = url->GetDirectory (dirPath);
        if (NS_FAILED(rv) || !dirPath.Length()) return NULL;

        nsCAutoString filePath;
        rv = url->GetFilePath (filePath);
        if (NS_FAILED(rv) || !filePath.Length()) return NULL;

        PRInt32 pathLength = filePath.Length();
        PRInt32 trailingSlash = filePath.RFind("/");

        if(pathLength < 2 || trailingSlash == -1)
        {
                return NULL;
        }

        if(trailingSlash != (pathLength-1))
        {
                uri->SetPath(dirPath);
        }
        else
        {
                PRInt32 nextSlash = filePath.RFind("/",PR_FALSE,trailingSlash-1);
                nsCAutoString parentPath;
                filePath.Left(parentPath, nextSlash);
                uri->SetPath(parentPath);
        }

        nsCAutoString spec;
        uri->GetSpec(spec);

        return !spec.IsEmpty() ? g_strdup(spec.get()) : NULL;
}

static gresult
impl_render_data (EphyEmbed *embed, 
                  const char *data,
                  guint32 len,
                  const char *base_uri, 
                  const char *mime_type)
{
	gtk_moz_embed_render_data (GTK_MOZ_EMBED(embed),
				   data,
				   len,
				   base_uri,
				   mime_type);
	
	return G_OK;
}

static gresult
impl_open_stream (EphyEmbed *embed,
                  const char *base_uri,
                  const char *mime_type)
{
	gtk_moz_embed_open_stream (GTK_MOZ_EMBED(embed),
				   base_uri, mime_type);
	
	return G_OK;
}

static gresult
impl_append_data (EphyEmbed *embed,
                  const char *data, 
                  guint32 len)
{
	gtk_moz_embed_append_data (GTK_MOZ_EMBED(embed),
				   data, len);
	
	return G_OK;
}

static gresult
impl_close_stream (EphyEmbed *embed)
{
	gtk_moz_embed_close_stream (GTK_MOZ_EMBED(embed));
	
	return G_OK;
}

static gresult
impl_get_title (EphyEmbed *embed,
                char **title)
{
	nsXPIDLString uTitle;

	*getter_Copies(uTitle) =
		gtk_moz_embed_get_title_unichar (GTK_MOZ_EMBED(embed));

	*title = g_strdup (NS_ConvertUCS2toUTF8(uTitle).get());
	
	return G_OK;
}

static gresult 
impl_get_location (EphyEmbed *embed, 
                   gboolean toplevel,
                   char **location)
{
	char *l;
	nsresult rv;
	nsCAutoString url;
	EphyWrapper *wrapper;

	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;

	/* if the wrapper is NULL than we have no location,
	 * in fact the wrapper is initialized on net start */
	if (!wrapper) 
	{
		*location = NULL;
		return G_FAILED;
	}
	
	if (toplevel)
	{
		rv = wrapper->GetMainDocumentUrl (url);
		l = (NS_SUCCEEDED (rv) && !url.IsEmpty()) ?
		     g_strdup (url.get()) : NULL;	   	
	}
	else
	{
		rv = wrapper->GetDocumentUrl (url);
		l = (NS_SUCCEEDED (rv) && !url.IsEmpty()) ?
		     g_strdup (url.get()) : NULL;	   	
	}

	*location = l;

	if (l == NULL) return G_FAILED;
	
	return G_OK;
}

static gresult 
impl_reload (EphyEmbed *embed, 
             EmbedReloadFlags flags)
{
	guint32 mflags;

	mflags = GTK_MOZ_EMBED_FLAG_RELOADNORMAL;
	
	if ((flags & EMBED_RELOAD_BYPASSCACHE) &&
	    (flags & EMBED_RELOAD_BYPASSPROXY))
	{
		mflags = GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE;
	}
	else if (flags & EMBED_RELOAD_BYPASSCACHE)
	{
		mflags = GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE;
	}
	else if (flags & EMBED_RELOAD_BYPASSPROXY)
	{
		mflags = GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXY;
	}
	
	gtk_moz_embed_reload (GTK_MOZ_EMBED(embed),
			      mflags);
	
	return G_OK;
}

static gresult
impl_copy_page (EphyEmbed *dest,
		EphyEmbed *source,
		EmbedDisplayType display_type)
{
	EphyWrapper *dWrapper;
	dWrapper = MOZILLA_EMBED(dest)->priv->wrapper;
	g_return_val_if_fail (dWrapper != NULL, G_FAILED);

	EphyWrapper *sWrapper;
	sWrapper = MOZILLA_EMBED(source)->priv->wrapper;
	g_return_val_if_fail (sWrapper != NULL, G_FAILED);

        nsresult rv;

        nsCOMPtr<nsISupports> pageDescriptor;
        rv = sWrapper->GetPageDescriptor(getter_AddRefs(pageDescriptor));
        if (!pageDescriptor || NS_FAILED(rv)) return G_FAILED;

        rv = dWrapper->LoadDocument(pageDescriptor, static_cast<PRUint32>(display_type));
        if (NS_FAILED(rv)) return G_FAILED;

        return G_OK;
}

static gresult
impl_zoom_set (EphyEmbed *embed, 
               float zoom, 
               gboolean reflow)
{
	EphyWrapper *wrapper;
	nsresult result;

	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	result = wrapper->SetZoom (zoom, reflow);

	if (NS_SUCCEEDED (result))
	{
		g_signal_emit_by_name (embed, "ge_zoom_change", zoom);
	}

	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_zoom_get (EphyEmbed *embed,
               float *zoom)
{
	float f;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	if (!wrapper)
	{
		LOG ("impl_zoom_get: wrapper == NULL")
		
		*zoom = 1.0;
		return G_FAILED;
	}
	
	nsresult result = wrapper->GetZoom (&f);
	
	if (NS_SUCCEEDED (result))
	{
		*zoom = f;

		return G_OK;
	}
	else
	{
		return G_FAILED;
	}
}

static gresult 
impl_selection_can_cut (EphyEmbed *embed)
{
	gboolean result;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	wrapper->CanCutSelection (&result);
	
	return result ? G_OK : G_FAILED;
}

static gresult 
impl_selection_can_copy (EphyEmbed *embed)
{
	gboolean result;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	wrapper->CanCopySelection (&result);
	
	return result ? G_OK : G_FAILED;
}

static gresult 
impl_can_paste (EphyEmbed *embed)
{
	gboolean result;
	EphyWrapper *wrapper;

	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	wrapper->CanPaste (&result);
	
	return result ? G_OK : G_FAILED;
}

static gresult 
impl_select_all (EphyEmbed *embed)
{
	nsresult result;
	EphyWrapper *wrapper;

	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	result = wrapper->SelectAll ();
	
	return result ? G_OK : G_FAILED;
}

static gresult
impl_selection_cut (EphyEmbed *embed)
{
	nsresult rv;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	rv = wrapper->CutSelection ();
	
	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_selection_copy (EphyEmbed *embed)
{
	nsresult rv;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	rv = wrapper->CopySelection ();
	
	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_paste (EphyEmbed *embed)
{
	nsresult rv;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	rv = wrapper->Paste ();
	
	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_shistory_count  (EphyEmbed *embed,
                      int *count)
{
	nsresult rv;
	EphyWrapper *wrapper;
	int c, index;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	rv = wrapper->GetSHInfo (&c, &index);

	*count = c;
	
	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_shistory_get_nth (EphyEmbed *embed, 
                       int nth,
                       gboolean is_relative,
                       char **aUrl,
                       char **aTitle)
{
	nsresult rv;
        nsCAutoString url;
	PRUnichar *title;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	if (is_relative)
	{
		int pos;

		if (ephy_embed_shistory_get_pos 
		    (EPHY_EMBED(embed), &pos) == G_OK)
		{
			pos += nth;
		}
		else
		{
			return G_FAILED;
		}
	}
	
        rv = wrapper->GetSHUrlAtIndex(nth, url);

        *aUrl = (NS_SUCCEEDED (rv) && !url.IsEmpty()) ? g_strdup(url.get()) : NULL;

	rv = wrapper->GetSHTitleAtIndex(nth, &title);

	*aTitle = g_strdup (NS_ConvertUCS2toUTF8(title).get());

	nsMemory::Free (title);

	return G_OK;
}

static gresult
impl_shistory_get_pos (EphyEmbed *embed,
                       int *pos)
{
	nsresult rv;
	EphyWrapper *wrapper;
	int count, index;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	rv = wrapper->GetSHInfo (&count, &index);

	*pos = index;
	
	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_shistory_go_nth (EphyEmbed *embed, 
                      int nth)
{
	nsresult rv;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	rv = wrapper->GoToHistoryIndex (nth);

	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gboolean
impl_shistory_copy (EphyEmbed *source,
                    EphyEmbed *dest)
{
	nsresult rv;
	EphyWrapper *s_wrapper;
	EphyWrapper *d_wrapper;
	
	s_wrapper = MOZILLA_EMBED(source)->priv->wrapper;
	g_return_val_if_fail (s_wrapper != NULL, G_FAILED);

	d_wrapper = MOZILLA_EMBED(dest)->priv->wrapper;
	g_return_val_if_fail (d_wrapper != NULL, G_FAILED);

	rv = s_wrapper->CopyHistoryTo (d_wrapper);

	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_get_security_level (EphyEmbed *embed, 
                         EmbedSecurityLevel *level,
                         char **description)
{
	nsresult result;

        nsCOMPtr<nsIChannel> channel;
	channel = do_QueryInterface (MOZILLA_EMBED(embed)->priv->request, 
				     &result);
        if (NS_FAILED (result)) return G_FAILED;

        nsCOMPtr<nsISupports> info;
        result = channel->GetSecurityInfo(getter_AddRefs(info));
        if (NS_FAILED (result)) return G_FAILED;

	*description = NULL;
	if (info)
	{
		nsCOMPtr<nsITransportSecurityInfo> secInfo(do_QueryInterface(info));
		if (!secInfo) return G_FAILED;

		nsXPIDLString tooltip;
		result = secInfo->GetShortSecurityDescription(getter_Copies(tooltip));
		if (NS_FAILED (result)) return G_FAILED;

		if (tooltip)
		{
			*description = g_strdup (NS_ConvertUCS2toUTF8(tooltip).get());
		}
	}
	
	*level = mozilla_embed_security_level (MOZILLA_EMBED (embed));
	return G_OK;
}

static gresult
impl_print (EphyEmbed *embed,
            EmbedPrintInfo *info)
{
	nsresult result = NS_OK;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

        nsCOMPtr<nsIPrintSettings> options;
        result = wrapper->GetPrintSettings(getter_AddRefs(options));
        if (!NS_SUCCEEDED (result)) return G_FAILED;

	MozillaCollatePrintSettings(info, options);

        options->SetPrintSilent (PR_TRUE);

	result = wrapper->Print(options, info->preview);

	return NS_SUCCEEDED (result) ? G_OK : G_FAILED;
}

static gresult
impl_print_preview_close (EphyEmbed *embed)
{
	nsresult result = NS_OK;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	result = wrapper->PrintPreviewClose();
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_print_preview_num_pages (EphyEmbed *embed,
			      gint *retNum)
{
	nsresult result = NS_OK;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	result = wrapper->PrintPreviewNumPages(retNum);
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_print_preview_navigate (EphyEmbed *embed,
			     EmbedPrintPreviewNavType navType,
			     gint pageNum)
{
	nsresult result = NS_OK;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	result = wrapper->PrintPreviewNavigate(navType, pageNum);
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_find (EphyEmbed *embed, 
           EmbedFindInfo *info)
{
	nsresult result = NS_OK;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
        PRBool didFind;

        result = wrapper->Find ((NS_ConvertUTF8toUCS2(info->search_string)).get(),
				info->interactive,
				info->match_case, 
                 	        info->backwards, info->wrap,
                       	        info->entire_word, info->search_frames,
                                &didFind);
	
	return didFind ? G_OK : G_FAILED;
}

static gresult
impl_set_encoding (EphyEmbed *embed,
		   const char *encoding)
{
	nsresult result = NS_OK;
	EphyWrapper *wrapper;
	
	wrapper = MOZILLA_EMBED(embed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	result = wrapper->ForceEncoding (encoding);
	if (NS_FAILED (result)) return G_FAILED;
	
	gtk_moz_embed_reload (GTK_MOZ_EMBED (embed),
			      GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE);
	
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static void
mozilla_embed_location_changed_cb (GtkMozEmbed *embed, 
				   MozillaEmbed *membed)
{
	/* Do not emit signal if we are loading the
	 * fallback about:blank. We dont want the user
	 * to know about it. */
	if (membed->priv->no_page != 0)
	{
		char *location;

		location = gtk_moz_embed_get_location (embed);
		g_signal_emit_by_name (membed, "ge_location", location);
		g_free (location);
	}

	membed->priv->no_page = -1;
}

static void
mozilla_embed_title_changed_cb (GtkMozEmbed *embed, 
				MozillaEmbed *membed)
{
	g_return_if_fail (IS_MOZILLA_EMBED (membed));
	g_return_if_fail (GTK_IS_WIDGET (embed));
	g_signal_emit_by_name (membed, "ge_title");
}

static void
mozilla_embed_net_state_all_cb (GtkMozEmbed *embed, const char *aURI,
                                gint state, guint status, 
				MozillaEmbed *membed)
{
	EmbedState estate = EMBED_STATE_UNKNOWN;
	int i;
	
	struct
	{
		guint state;
		EmbedState embed_state;
	}
	conversion_map [] =
	{
		{ GTK_MOZ_EMBED_FLAG_START, EMBED_STATE_START },
		{ GTK_MOZ_EMBED_FLAG_STOP, EMBED_STATE_STOP },
		{ GTK_MOZ_EMBED_FLAG_REDIRECTING, EMBED_STATE_REDIRECTING },
		{ GTK_MOZ_EMBED_FLAG_TRANSFERRING, EMBED_STATE_TRANSFERRING },
		{ GTK_MOZ_EMBED_FLAG_NEGOTIATING, EMBED_STATE_NEGOTIATING },
		{ GTK_MOZ_EMBED_FLAG_IS_REQUEST, EMBED_STATE_IS_REQUEST },
		{ GTK_MOZ_EMBED_FLAG_IS_DOCUMENT, EMBED_STATE_IS_DOCUMENT },
		{ GTK_MOZ_EMBED_FLAG_IS_NETWORK, EMBED_STATE_IS_NETWORK },
		{ 0, EMBED_STATE_UNKNOWN }
	};

	/* No page loaded, default to about:blank */
	if (membed->priv->no_page > 0 && 
	    (state &  GTK_MOZ_EMBED_FLAG_STOP) &&
	    (state &  GTK_MOZ_EMBED_FLAG_IS_DOCUMENT))
	{
		ephy_embed_load_url (EPHY_EMBED(membed), "about:blank");
		membed->priv->no_page = 0;
	}
	
	if (!membed->priv->wrapper)
	{
		membed->priv->wrapper = new EphyWrapper ();

		nsresult result;
		result = membed->priv->wrapper->Init (GTK_MOZ_EMBED(embed));
		if (NS_FAILED(result))
		{
                	g_warning ("Wrapper initialization failed");
		}
	}

	for (i = 0; conversion_map[i].state != 0; i++)
	{
		if (state & conversion_map[i].state)
		{
			estate = (EmbedState) (estate | conversion_map[i].embed_state);	
		}
	}
	
	g_signal_emit_by_name (membed, "ge_net_state", aURI, estate);
}

static void
mozilla_embed_progress_cb (GtkMozEmbed *embed, const char *aURI,
                           gint curprogress, gint maxprogress, 
			   MozillaEmbed *membed)
{
	g_signal_emit_by_name (membed, "ge_progress", aURI, 
			       curprogress, maxprogress);
}

static void
mozilla_embed_link_message_cb (GtkMozEmbed *embed, 
			       MozillaEmbed *membed)
{
	nsXPIDLString message;

	*getter_Copies(message) = gtk_moz_embed_get_link_message_unichar (embed);
	
	g_signal_emit_by_name (membed, "ge_link_message",
			       NS_ConvertUCS2toUTF8(message).get());
}

static void
mozilla_embed_js_status_cb (GtkMozEmbed *embed, 
			    MozillaEmbed *membed)
{
	nsXPIDLString status;

	*getter_Copies(status) = gtk_moz_embed_get_js_status_unichar (embed);
	
	g_signal_emit_by_name (membed, "ge_js_status",
			       NS_ConvertUCS2toUTF8(status).get());
}

static void
mozilla_embed_visibility_cb (GtkMozEmbed *embed, gboolean visibility, 
			     MozillaEmbed *membed)
{
	g_signal_emit_by_name (membed, "ge_visibility", visibility); 

	nsresult rv;
	nsCOMPtr<nsIWindowWatcher> wwatch
		(do_GetService(WINDOWWATCHER_CONTRACTID, &rv));
	if (NS_FAILED(rv) || !wwatch) return;

	EphyWrapper *wrapper = membed->priv->wrapper;

	if (wrapper)
	{
		nsCOMPtr<nsIDOMWindow> domWindow;
		rv = wrapper->GetDOMWindow(getter_AddRefs(domWindow));
		if(NS_FAILED(rv) || !domWindow) return;

		rv = wwatch->SetActiveWindow(domWindow);
	}
}

static gint
mozilla_embed_dom_key_down_cb (GtkMozEmbed *embed, gpointer dom_event,
		               MozillaEmbed *membed)
{
	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_key_down_cb: domevent NULL");
		return FALSE;
	}

	nsCOMPtr<nsIDOMKeyEvent> ev = static_cast<nsIDOMKeyEvent*>(dom_event);
	if (!ev)
	{
		return FALSE;
	}

	EphyWrapper *wrapper = MOZILLA_EMBED(membed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	EphyEmbedEvent *info;
	info = ephy_embed_event_new ();

	gboolean ret = FALSE;

	nsresult rv;
	EventContext ctx;
	ctx.Init (wrapper);
	rv = ctx.GetKeyEventInfo (ev, info);
	if (NS_SUCCEEDED(rv) &&
	    (info->keycode == nsIDOMKeyEvent::DOM_VK_F10 &&
	     info->modifier == GDK_SHIFT_MASK))
	{
		/* Translate relative coordinates to absolute values, and try
		   to avoid covering links by adding a little offset. */

		int x, y;
		gdk_window_get_origin (GTK_WIDGET(membed)->window, &x, &y);
		info->x += x + 6;	
		info->y += y + 6;

		nsCOMPtr<nsIDOMDocument> doc;
		rv = ctx.GetTargetDocument (getter_AddRefs(doc));
		if (NS_SUCCEEDED(rv))
		{
			rv = wrapper->PushTargetDocument (doc);
			if (NS_SUCCEEDED(rv))
			{
				g_signal_emit_by_name (membed, "ge_context_menu", info, &ret);
				wrapper->PopTargetDocument ();
			}
		}
	}

	g_object_unref (info);
	return ret;
}

static void
mozilla_embed_destroy_brsr_cb (GtkMozEmbed *embed, 
			       MozillaEmbed *membed)
{
	g_signal_emit_by_name (membed, "ge_destroy_brsr"); 
}

static gint
mozilla_embed_dom_mouse_click_cb (GtkMozEmbed *embed, gpointer dom_event, 
				  MozillaEmbed *membed)
{
	EphyEmbedEvent *info;
	EventContext event_context;
	gint return_value = 0;
	EphyWrapper *wrapper;
	nsresult result;

	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_mouse_click_cb: domevent NULL");
		return FALSE;
	}

	info = ephy_embed_event_new ();
	
	wrapper = MOZILLA_EMBED(membed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);
	
	event_context.Init (wrapper);
        result = event_context.GetMouseEventInfo (static_cast<nsIDOMMouseEvent*>(dom_event), info);

	if (NS_SUCCEEDED(result))
	{
		nsCOMPtr<nsIDOMDocument> domDoc;
		result = event_context.GetTargetDocument (getter_AddRefs(domDoc));
		if (NS_SUCCEEDED(result))
		{
			result = wrapper->PushTargetDocument (domDoc);
			if (NS_SUCCEEDED(result))
			{
				g_signal_emit_by_name (membed, "ge_dom_mouse_click", 
						       info, &return_value); 
				wrapper->PopTargetDocument ();
			}
		}

	}

	g_object_unref (info);

	return return_value;
}

static gint
mozilla_embed_dom_mouse_down_cb (GtkMozEmbed *embed, gpointer dom_event, 
				 MozillaEmbed *membed)
{
	EphyEmbedEvent *info;
	EventContext event_context;
	gint return_value = 0;
	EphyWrapper *wrapper;
	nsresult result;
	EphyEmbedEventType type;

	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_mouse_down_cb: domevent NULL");
		return FALSE;
	}

	info = ephy_embed_event_new ();
	
	wrapper = MOZILLA_EMBED(membed)->priv->wrapper;
	g_return_val_if_fail (wrapper != NULL, G_FAILED);

	event_context.Init (wrapper);
        result = event_context.GetMouseEventInfo (static_cast<nsIDOMMouseEvent*>(dom_event), info);

	ephy_embed_event_get_event_type (info, &type);

	if (NS_SUCCEEDED(result) && (type == EPHY_EMBED_EVENT_MOUSE_BUTTON3))
	{
		nsCOMPtr<nsIDOMDocument> domDoc;
		result = event_context.GetTargetDocument (getter_AddRefs(domDoc));
		if (NS_SUCCEEDED(result))
		{
			result = wrapper->PushTargetDocument (domDoc);
			if (NS_SUCCEEDED(result))
			{
				g_signal_emit_by_name (membed, "ge_context_menu", 
						       info, &return_value); 
				wrapper->PopTargetDocument ();
			}
		}

	}

	g_object_unref (info);

	return return_value;
}

static void
mozilla_embed_size_to_cb (GtkMozEmbed *embed, gint width, gint height, 
			  MozillaEmbed *membed)
{
	g_signal_emit_by_name (membed, "ge_size_to", width, height);
}

static void
mozilla_embed_new_window_cb (GtkMozEmbed *embed, 
			     GtkMozEmbed **newEmbed,
                             guint chromemask, 
			     MozillaEmbed *membed)
{
	int i;
	EmbedChromeMask mask = EMBED_CHROME_OPENASPOPUP;
	EphyEmbed *new_embed = NULL;

	struct
	{
		guint chromemask;
		EmbedChromeMask embed_mask;
	}
	conversion_map [] =
	{
		{ GTK_MOZ_EMBED_FLAG_DEFAULTCHROME, EMBED_CHROME_DEFAULT },
		{ GTK_MOZ_EMBED_FLAG_MENUBARON, EMBED_CHROME_MENUBARON },
		{ GTK_MOZ_EMBED_FLAG_TOOLBARON, EMBED_CHROME_TOOLBARON },
		{ GTK_MOZ_EMBED_FLAG_PERSONALTOOLBARON, EMBED_CHROME_BOOKMARKSBAR_DEFAULT },
		{ GTK_MOZ_EMBED_FLAG_STATUSBARON, EMBED_CHROME_STATUSBARON },
		{ GTK_MOZ_EMBED_FLAG_WINDOWRAISED, EMBED_CHROME_WINDOWRAISED },
		{ GTK_MOZ_EMBED_FLAG_WINDOWLOWERED, EMBED_CHROME_WINDOWLOWERED },
		{ GTK_MOZ_EMBED_FLAG_CENTERSCREEN, EMBED_CHROME_CENTERSCREEN },
		{ GTK_MOZ_EMBED_FLAG_OPENASDIALOG, EMBED_CHROME_OPENASDIALOG },
		{ GTK_MOZ_EMBED_FLAG_OPENASCHROME, EMBED_CHROME_OPENASCHROME },
		{ 0, EMBED_CHROME_NONE }
	};

	for (i = 0; conversion_map[i].chromemask != 0; i++)
	{
		if (chromemask & conversion_map[i].chromemask)
		{
			mask = (EmbedChromeMask) (mask | conversion_map[i].embed_mask);	
		}
	}
	
	g_signal_emit_by_name (membed, "ge_new_window", &new_embed, mask);

	g_assert (new_embed != NULL);
	
	*newEmbed = GTK_MOZ_EMBED(new_embed);
}

static void
mozilla_embed_security_change_cb (GtkMozEmbed *embed, 
				  gpointer request,
                                  guint state, 
				  MozillaEmbed *membed)
{
	EmbedSecurityLevel level;

	membed->priv->request = (nsIRequest *) request;
	membed->priv->security_state = state;
	level = mozilla_embed_security_level (membed);

	g_signal_emit_by_name (membed, "ge_security_change", level);
}

static EmbedSecurityLevel
mozilla_embed_security_level (MozillaEmbed *membed)
{
	EmbedSecurityLevel level;

	switch (membed->priv->security_state)
        {
        case nsIWebProgressListener::STATE_IS_INSECURE:
                level = STATE_IS_INSECURE;
                break;
        case nsIWebProgressListener::STATE_IS_BROKEN:
                level = STATE_IS_BROKEN;
                break;
        case nsIWebProgressListener::STATE_IS_SECURE|
             nsIWebProgressListener::STATE_SECURE_HIGH:
                level = STATE_IS_SECURE_HIGH;
                break;
        case nsIWebProgressListener::STATE_IS_SECURE|
             nsIWebProgressListener::STATE_SECURE_MED:
                level = STATE_IS_SECURE_MED;
                break;
        case nsIWebProgressListener::STATE_IS_SECURE|
             nsIWebProgressListener::STATE_SECURE_LOW:
                level = STATE_IS_SECURE_LOW;
                break;
        default:
                level = STATE_IS_UNKNOWN;
                break;
        }
	return level;
}

