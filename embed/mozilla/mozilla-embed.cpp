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

#include "ephy-command-manager.h"
#include "ephy-string.h"
#include "ephy-embed.h"
#include "ephy-debug.h"
#include "mozilla-embed.h"
#include "MozillaPrivate.h"
#include "EphyBrowser.h"
#include "EventContext.h"
#include "ephy-debug.h"

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
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
impl_zoom_set (EphyEmbed *embed, 
               float zoom, 
               gboolean reflow);
static gresult
impl_zoom_get (EphyEmbed *embed,
               float *zoom);
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
static gresult
impl_get_security_level (EphyEmbed *embed, 
                         EmbedSecurityLevel *level,
                         char **description);
static gresult
impl_set_encoding (EphyEmbed *embed,
                   const char *encoding);
static gresult
impl_get_encoding_info (EphyEmbed *embed,
			EphyEncodingInfo **info);
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

#define MOZILLA_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED, MozillaEmbedPrivate))

struct MozillaEmbedPrivate
{
	EphyBrowser *browser;
	nsCOMPtr<nsIRequest> request;
	gint security_state;
};

#define WINDOWWATCHER_CONTRACTID "@mozilla.org/embedcomp/window-watcher;1"

static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

static GObjectClass *parent_class = NULL;

static gresult
impl_manager_do_command (EphyCommandManager *manager,
			 const char *command) 
{
	nsresult result = NS_OK;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(manager)->priv;

        result = mpriv->browser->DoCommand (command);
	
	return result ? G_OK : G_FAILED;
}

static gresult
impl_manager_can_do_command (EphyCommandManager *manager,
			     const char *command) 
{
	return G_NOT_IMPLEMENTED;
}

static gresult
impl_manager_observe_command (EphyCommandManager *manager,
			      const char *command) 
{
	return G_NOT_IMPLEMENTED;
}

static void
ephy_command_manager_init (EphyCommandManagerClass *manager_class)
{
	manager_class->do_command = impl_manager_do_command;
	manager_class->can_do_command = impl_manager_can_do_command;
	manager_class->observe_command = impl_manager_observe_command;
}
	
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
			(GInterfaceInitFunc) ephy_embed_init,
        		NULL,
        		NULL
     		};

		static const GInterfaceInfo ephy_command_manager_info =
		{
			(GInterfaceInitFunc) ephy_command_manager_init,
        		NULL,
        		NULL
     		 };
	
                mozilla_embed_type = g_type_register_static (GTK_TYPE_MOZ_EMBED,
							     "MozillaEmbed",
							     &our_info, 
							     (GTypeFlags)0);
		g_type_add_interface_static (mozilla_embed_type,
                                   	     EPHY_TYPE_EMBED,
                                   	     &embed_info);
		g_type_add_interface_static (mozilla_embed_type,
                                   	     EPHY_TYPE_COMMAND_MANAGER,
                                   	     &ephy_command_manager_info);
        }

        return mozilla_embed_type;
}

static gresult
impl_find_next (EphyEmbed *embed, 
                gboolean backwards)
{
	nsresult result = NS_OK;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

        PRBool didFind;

        result = mpriv->browser->Find (backwards, &didFind);
	
	return didFind ? G_OK : G_FAILED;
}

static gresult
impl_activate (EphyEmbed *embed) 
{
	g_return_val_if_fail (EPHY_IS_EMBED (embed), G_FAILED);

	gtk_widget_grab_focus (GTK_BIN (embed)->child);
	
	return G_OK;
}

static gresult
impl_find_set_properties (EphyEmbed *embed, 
                          char *search_string,
	                  gboolean case_sensitive,
			  gboolean wrap_around)
{
	nsresult result = NS_OK;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

        result = mpriv->browser->FindSetProperties
		((NS_ConvertUTF8toUCS2(search_string)).get(),
		 case_sensitive, wrap_around); 
	
	return result ? G_OK : G_FAILED;
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
	embed_class->zoom_set = impl_zoom_set;
	embed_class->zoom_get = impl_zoom_get;
	embed_class->shistory_count = impl_shistory_count;
	embed_class->shistory_get_nth = impl_shistory_get_nth;
	embed_class->shistory_get_pos = impl_shistory_get_pos;
	embed_class->shistory_go_nth = impl_shistory_go_nth;
	embed_class->get_security_level = impl_get_security_level;
	embed_class->find_next = impl_find_next;
	embed_class->activate = impl_activate;
	embed_class->find_set_properties = impl_find_set_properties;
	embed_class->set_encoding = impl_set_encoding;
	embed_class->get_encoding_info = impl_get_encoding_info;
	embed_class->print = impl_print;
	embed_class->print_preview_close = impl_print_preview_close;
	embed_class->print_preview_num_pages = impl_print_preview_num_pages;
	embed_class->print_preview_navigate = impl_print_preview_navigate;
}

static void
mozilla_embed_realize (GtkWidget *widget)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (widget)->priv;

	(* GTK_WIDGET_CLASS(parent_class)->realize) (widget);

	nsresult result;
	result = mpriv->browser->Init (GTK_MOZ_EMBED (widget));

	if (NS_FAILED(result))
	{
               	g_warning ("Browser initialization failed");
	}
}

static void
mozilla_embed_class_init (MozillaEmbedClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
     	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass); 
     	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass); 

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	gtk_object_class->destroy = mozilla_embed_destroy;
	widget_class->realize = mozilla_embed_realize;

	g_type_class_add_private (object_class, sizeof(MozillaEmbedPrivate));
}

static void
mozilla_embed_init (MozillaEmbed *embed)
{
        embed->priv = MOZILLA_EMBED_GET_PRIVATE (embed);

	embed->priv->browser = new EphyBrowser ();
	embed->priv->security_state = -1;

	mozilla_embed_connect_signals (embed);
}

gpointer
mozilla_embed_get_ephy_browser (MozillaEmbed *embed)
{
	g_return_val_if_fail (embed->priv->browser != NULL, NULL);
	
	return embed->priv->browser;
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

	if (embed->priv->browser)
	{
		embed->priv->browser->Destroy();
        	delete embed->priv->browser;
        	embed->priv->browser = NULL;
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
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
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	
	if (toplevel)
	{
		rv = mpriv->browser->GetDocumentUrl (url);
		l = (NS_SUCCEEDED (rv) && !url.IsEmpty()) ?
		     g_strdup (url.get()) : NULL;	   	
	}
	else
	{
		rv = mpriv->browser->GetTargetDocumentUrl (url);
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
impl_zoom_set (EphyEmbed *embed, 
               float zoom, 
               gboolean reflow)
{
	EphyBrowser *browser;
	nsresult result;

	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_val_if_fail (browser != NULL, G_FAILED);

	result = browser->SetZoom (zoom, reflow);

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
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	
	nsresult result = mpriv->browser->GetZoom (&f);
	
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
impl_shistory_count  (EphyEmbed *embed,
                      int *count)
{
	nsresult rv;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	int c, index;
	
	rv = mpriv->browser->GetSHInfo (&c, &index);

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
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

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
	
        rv = mpriv->browser->GetSHUrlAtIndex(nth, url);

        *aUrl = (NS_SUCCEEDED (rv) && !url.IsEmpty()) ? g_strdup(url.get()) : NULL;

	rv = mpriv->browser->GetSHTitleAtIndex(nth, &title);

	*aTitle = g_strdup (NS_ConvertUCS2toUTF8(title).get());

	nsMemory::Free (title);

	return G_OK;
}

static gresult
impl_shistory_get_pos (EphyEmbed *embed,
                       int *pos)
{
	nsresult rv;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	int count, index;
	
	rv = mpriv->browser->GetSHInfo (&count, &index);

	*pos = index;
	
	return NS_SUCCEEDED(rv) ? G_OK : G_FAILED;
}

static gresult
impl_shistory_go_nth (EphyEmbed *embed, 
                      int nth)
{
	nsresult rv;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	rv = mpriv->browser->GoToHistoryIndex (nth);

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
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

        nsCOMPtr<nsIPrintSettings> options;
        result = mpriv->browser->GetPrintSettings(getter_AddRefs(options));
        if (!NS_SUCCEEDED (result)) return G_FAILED;

	MozillaCollatePrintSettings(info, options);

        options->SetPrintSilent (PR_TRUE);

	result = mpriv->browser->Print(options, info->preview);

	/* Workaround for bug 125984 */
        options->SetPrintSilent (PR_FALSE);

	return NS_SUCCEEDED (result) ? G_OK : G_FAILED;
}

static gresult
impl_print_preview_close (EphyEmbed *embed)
{
	nsresult result = NS_OK;
	EphyBrowser *browser;
	
	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_val_if_fail (browser != NULL, G_FAILED);

	result = browser->PrintPreviewClose();
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_print_preview_num_pages (EphyEmbed *embed,
			      gint *retNum)
{
	nsresult result = NS_OK;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	
	result = mpriv->browser->PrintPreviewNumPages(retNum);
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_print_preview_navigate (EphyEmbed *embed,
			     EmbedPrintPreviewNavType navType,
			     gint pageNum)
{
	nsresult result = NS_OK;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	result = mpriv->browser->PrintPreviewNavigate(navType, pageNum);
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_set_encoding (EphyEmbed *embed,
		   const char *encoding)
{
	nsresult result;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	result = mpriv->browser->ForceEncoding (encoding);
	if (NS_FAILED (result)) return G_FAILED;
	
	gtk_moz_embed_reload (GTK_MOZ_EMBED (embed),
			      GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE);
	
	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static gresult
impl_get_encoding_info (EphyEmbed *embed,
			EphyEncodingInfo **info)
{
	nsresult result;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	g_return_val_if_fail (info != NULL, G_FAILED);
	*info = NULL;

	result = mpriv->browser->GetEncodingInfo (info);

	return NS_SUCCEEDED(result) ? G_OK : G_FAILED;
}

static void
mozilla_embed_location_changed_cb (GtkMozEmbed *embed, 
				   MozillaEmbed *membed)
{
	char *location;

	location = gtk_moz_embed_get_location (embed);
	g_signal_emit_by_name (membed, "ge_location", location);
	g_free (location);
}

static void
mozilla_embed_title_changed_cb (GtkMozEmbed *embed, 
				MozillaEmbed *membed)
{
	g_return_if_fail (MOZILLA_IS_EMBED (membed));
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
}

static gint
mozilla_embed_dom_key_down_cb (GtkMozEmbed *embed, gpointer dom_event,
		               MozillaEmbed *membed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

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

	EphyEmbedEvent *info;
	info = ephy_embed_event_new ();

	gboolean ret = FALSE;

	nsresult rv;
	EventContext ctx;
	ctx.Init (mpriv->browser);
	rv = ctx.GetKeyEventInfo (ev, info);
	if (NS_FAILED (rv)) return G_FAILED;

	if (info->keycode == nsIDOMKeyEvent::DOM_VK_F10 &&
	    (info->modifier == GDK_SHIFT_MASK ||
	     info->modifier == GDK_CONTROL_MASK))
	{
		/* Translate relative coordinates to absolute values, and try
		   to avoid covering links by adding a little offset. */

		int x, y;
		gdk_window_get_origin (GTK_WIDGET(membed)->window, &x, &y);
		info->x += x + 6;	
		info->y += y + 6;

		if (info->modifier == GDK_CONTROL_MASK)
		{
			info->context = EMBED_CONTEXT_DOCUMENT;	
		}

		nsCOMPtr<nsIDOMDocument> doc;
		rv = ctx.GetTargetDocument (getter_AddRefs(doc));
		if (NS_SUCCEEDED(rv))
		{
			rv = mpriv->browser->PushTargetDocument (doc);
			if (NS_SUCCEEDED(rv))
			{
				g_signal_emit_by_name (membed, "ge_context_menu", info, &ret);
				mpriv->browser->PopTargetDocument ();
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
	nsresult result;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_mouse_click_cb: domevent NULL");
		return FALSE;
	}

	info = ephy_embed_event_new ();
	
	event_context.Init (mpriv->browser);
        result = event_context.GetMouseEventInfo (static_cast<nsIDOMMouseEvent*>(dom_event), info);

	if (NS_SUCCEEDED(result))
	{
		nsCOMPtr<nsIDOMDocument> domDoc;
		result = event_context.GetTargetDocument (getter_AddRefs(domDoc));
		if (NS_SUCCEEDED(result))
		{
			result = mpriv->browser->PushTargetDocument (domDoc);
			if (NS_SUCCEEDED(result))
			{
				g_signal_emit_by_name (membed, "ge_dom_mouse_click", 
						       info, &return_value); 
				mpriv->browser->PopTargetDocument ();
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
	gint return_value = FALSE;
	nsresult result;
	EphyEmbedEventType type;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_mouse_down_cb: domevent NULL");
		return FALSE;
	}

	info = ephy_embed_event_new ();
	
	event_context.Init (mpriv->browser);
        result = event_context.GetMouseEventInfo (static_cast<nsIDOMMouseEvent*>(dom_event), info);
	if (NS_FAILED (result)) return FALSE;

	ephy_embed_event_get_event_type (info, &type);
		
	nsCOMPtr<nsIDOMDocument> domDoc;
	result = event_context.GetTargetDocument (getter_AddRefs(domDoc));
	if (NS_SUCCEEDED(result))
	{
		result = mpriv->browser->PushTargetDocument (domDoc);

		if (NS_SUCCEEDED(result))
		{
			g_signal_emit_by_name (membed, "ge_dom_mouse_down", 
					       info, &return_value); 

			if (return_value == FALSE &&
			    type == EPHY_EMBED_EVENT_MOUSE_BUTTON3)
			{
				g_signal_emit_by_name (membed, "ge_context_menu", 
						       info, &return_value);
			}

			mpriv->browser->PopTargetDocument ();
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

