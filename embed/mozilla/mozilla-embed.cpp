/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#include "mozilla-embed.h"

#include "ephy-command-manager.h"
#include "ephy-string.h"
#include "ephy-embed.h"
#include "ephy-debug.h"

#include "MozillaPrivate.h"
#include "EphyBrowser.h"
#include "EventContext.h"

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

static void	mozilla_embed_class_init	(MozillaEmbedClass *klass);
static void	mozilla_embed_init		(MozillaEmbed *gs);
static void	mozilla_embed_destroy		(GtkObject *object);
static void	ephy_embed_init			(EphyEmbedClass *embed_class);

static void 
mozilla_embed_connect_signals (MozillaEmbed *membed);
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
        { "net_state_all",   (void *) mozilla_embed_net_state_all_cb     },
        { "link_message",    (void *) mozilla_embed_link_message_cb      },
        { "js_status",       (void *) mozilla_embed_js_status_cb         },
        { "dom_mouse_click", (void *) mozilla_embed_dom_mouse_click_cb   },
	{ "dom_mouse_down",  (void *) mozilla_embed_dom_mouse_down_cb    },
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

static void
impl_manager_do_command (EphyCommandManager *manager,
			 const char *command) 
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(manager)->priv;

	mpriv->browser->DoCommand (command);
}

static gboolean
impl_manager_can_do_command (EphyCommandManager *manager,
			     const char *command) 
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(manager)->priv;
	nsresult result;
	PRBool enabled;

        result = mpriv->browser->GetCommandState (command, &enabled);

	return NS_SUCCEEDED (result) ? enabled : FALSE;
}

static void
ephy_command_manager_init (EphyCommandManagerClass *manager_class)
{
	manager_class->do_command = impl_manager_do_command;
	manager_class->can_do_command = impl_manager_can_do_command;
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

static gboolean
impl_find_next (EphyEmbed *embed, 
                gboolean backwards)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult result;
        PRBool didFind;

        result = mpriv->browser->Find (backwards, &didFind);
	
	return NS_SUCCEEDED (result) ? didFind : FALSE;
}

static void
impl_activate (EphyEmbed *embed) 
{
	gtk_widget_grab_focus (GTK_BIN (embed)->child);
}

static void
impl_find_set_properties (EphyEmbed *embed, 
                          char *search_string,
	                  gboolean case_sensitive,
			  gboolean wrap_around)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->FindSetProperties
		((NS_ConvertUTF8toUCS2(search_string)).get(),
		 case_sensitive, wrap_around); 
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
               	g_warning ("EphyBrowser initialization failed for %p\n", widget);
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
_mozilla_embed_get_ephy_browser (MozillaEmbed *embed)
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
                g_signal_connect (G_OBJECT (embed),
                                  signal_connections[i].event,
                                  G_CALLBACK (signal_connections[i].func), 
                                  embed);
        }
}

static void
mozilla_embed_destroy (GtkObject *object)
{
	MozillaEmbed *embed = MOZILLA_EMBED (object);
	int i;
	
	for (i = 0; signal_connections[i].event != NULL; i++)
        {
                g_signal_handlers_disconnect_by_func
			(G_OBJECT (object),
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
impl_load_url (EphyEmbed *embed, 
               const char *url)
{
        gtk_moz_embed_load_url (GTK_MOZ_EMBED(embed), url);
}

static void
impl_stop_load (EphyEmbed *embed)
{
	gtk_moz_embed_stop_load (GTK_MOZ_EMBED(embed));	
}

static gboolean
impl_can_go_back (EphyEmbed *embed)
{
	return gtk_moz_embed_can_go_back (GTK_MOZ_EMBED(embed));
}

static gboolean
impl_can_go_forward (EphyEmbed *embed)
{
	return gtk_moz_embed_can_go_forward (GTK_MOZ_EMBED(embed));
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

static gboolean
impl_can_go_up (EphyEmbed *embed)
{
	char *address, *s;
	gboolean result;

	address = ephy_embed_get_location (embed, TRUE);
	if (address == NULL)
	{
		return FALSE;
	}

	if ((s = mozilla_embed_get_uri_parent (address)) != NULL)
	{
		g_free (s);
		result = TRUE;
	}
	else
	{
		result = FALSE;
	}

	g_free (address);

	return result;
}

static GSList *
impl_get_go_up_list (EphyEmbed *embed)
{
	GSList *l = NULL;
	char *address, *s;

	address = ephy_embed_get_location (embed, TRUE);
	if (address == NULL)
	{
		return NULL;
	}
	
	s = address;
	while ((s = mozilla_embed_get_uri_parent (s)) != NULL)
	{
		l = g_slist_prepend (l, s);
	}				

	g_free (address);

	return g_slist_reverse (l);
}

static void
impl_go_back (EphyEmbed *embed)
{
	gtk_moz_embed_go_back (GTK_MOZ_EMBED(embed));
}
		
static void
impl_go_forward (EphyEmbed *embed)
{
	gtk_moz_embed_go_forward (GTK_MOZ_EMBED(embed));
}

static void
impl_go_up (EphyEmbed *embed)
{
	char *uri;
	char *parent_uri;

	uri = ephy_embed_get_location (embed, TRUE);
	g_return_if_fail (uri != NULL);
	
	parent_uri = mozilla_embed_get_uri_parent (uri);
	g_free (uri);
	g_return_if_fail (parent_uri != NULL);

	ephy_embed_load_url (embed, parent_uri);

	g_free (parent_uri);
}

static char *
impl_get_title (EphyEmbed *embed)
{
	nsXPIDLString uTitle;

	*getter_Copies(uTitle) =
		gtk_moz_embed_get_title_unichar (GTK_MOZ_EMBED(embed));

	return g_strdup (NS_ConvertUCS2toUTF8(uTitle).get());
}

static char *
impl_get_location (EphyEmbed *embed, 
                   gboolean toplevel)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	char *l;
	nsresult rv;
	nsCAutoString url;
	
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

	return l;
}

static void
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
}

static void
impl_zoom_set (EphyEmbed *embed, 
               float zoom, 
               gboolean reflow)
{
	EphyBrowser *browser;
	nsresult result;

	g_return_if_fail (zoom > 0.0);

	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_if_fail (browser != NULL);

	result = browser->SetZoom (zoom, reflow);

	if (NS_SUCCEEDED (result))
	{
		g_signal_emit_by_name (embed, "ge_zoom_change", zoom);
	}
}

static float
impl_zoom_get (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	float f;
	
	nsresult result = mpriv->browser->GetZoom (&f);
	
	if (NS_SUCCEEDED (result))
	{
		return f;
	}

	return 1.0;
}

static int
impl_shistory_n_items (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	int count, index;

	rv = mpriv->browser->GetSHInfo (&count, &index);

	return NS_SUCCEEDED(rv) ? count : 0;
}

static void
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
		nth += ephy_embed_shistory_get_pos (embed);
	}
	
        rv = mpriv->browser->GetSHUrlAtIndex(nth, url);

        *aUrl = (NS_SUCCEEDED (rv) && !url.IsEmpty()) ? g_strdup(url.get()) : NULL;

	rv = mpriv->browser->GetSHTitleAtIndex(nth, &title);

	*aTitle = g_strdup (NS_ConvertUCS2toUTF8(title).get());

	nsMemory::Free (title);
}

static int
impl_shistory_get_pos (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	int count, index;

	rv = mpriv->browser->GetSHInfo (&count, &index);

	return NS_SUCCEEDED(rv) ? index : 0;
}

static void
impl_shistory_go_nth (EphyEmbed *embed, 
                      int nth)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->GoToHistoryIndex (nth);
}

static void
impl_get_security_level (EphyEmbed *embed, 
                         EmbedSecurityLevel *level,
                         char **description)
{
	nsresult result;

	g_return_if_fail (description != NULL || level != NULL);
	*description = NULL;
	*level = STATE_IS_UNKNOWN;

        nsCOMPtr<nsIChannel> channel;
	channel = do_QueryInterface (MOZILLA_EMBED(embed)->priv->request, 
				     &result);
        if (NS_FAILED (result)) return;

        nsCOMPtr<nsISupports> info;
        result = channel->GetSecurityInfo(getter_AddRefs(info));
        if (NS_FAILED (result)) return;

	if (info)
	{
		nsCOMPtr<nsITransportSecurityInfo> secInfo(do_QueryInterface(info));
		if (!secInfo) return;

		nsXPIDLString tooltip;
		result = secInfo->GetShortSecurityDescription(getter_Copies(tooltip));
		if (NS_FAILED (result)) return;

		if (tooltip)
		{
			*description = g_strdup (NS_ConvertUCS2toUTF8(tooltip).get());
		}
	}
	
	*level = mozilla_embed_security_level (MOZILLA_EMBED (embed));
}

static void
impl_print (EphyEmbed *embed,
            EmbedPrintInfo *info)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult result;

        nsCOMPtr<nsIPrintSettings> options;
        result = mpriv->browser->GetPrintSettings(getter_AddRefs(options));
        if (!NS_SUCCEEDED (result)) return;

	MozillaCollatePrintSettings(info, options);

        options->SetPrintSilent (PR_TRUE);

	result = mpriv->browser->Print(options, info->preview);

	/* Workaround for bug 125984 */
        options->SetPrintSilent (PR_FALSE);
}

static void
impl_print_preview_close (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->PrintPreviewClose();
}

static int
impl_print_preview_n_pages (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult result;
	int num;

	result = mpriv->browser->PrintPreviewNumPages(&num);

	return NS_SUCCEEDED(result) ? num : 0;
}

static void
impl_print_preview_navigate (EphyEmbed *embed,
			     EmbedPrintPreviewNavType type,
			     int page)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult result;

	result = mpriv->browser->PrintPreviewNavigate(type, page);
}

static void
impl_set_encoding (EphyEmbed *embed,
		   const char *encoding)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult result;

	result = mpriv->browser->ForceEncoding (encoding);
	if (NS_FAILED (result)) return;

	gtk_moz_embed_reload (GTK_MOZ_EMBED (embed),
			      GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE);
}

static EphyEncodingInfo *
impl_get_encoding_info (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult result;
	EphyEncodingInfo *info = NULL;

	result = mpriv->browser->GetEncodingInfo (&info);

	if (NS_FAILED (result))
	{
		ephy_encoding_info_free (info);

		return NULL;
	}

	return info;
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
	if (NS_FAILED (rv)) return ret;

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

static gint
mozilla_embed_dom_mouse_click_cb (GtkMozEmbed *embed, gpointer dom_event, 
				  MozillaEmbed *membed)
{
	EphyEmbedEvent *info;
	EventContext event_context;
	gint return_value = FALSE;
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

	type = ephy_embed_event_get_event_type (info);
		
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
		{ 0, EMBED_CHROME_NONE }
	};

	if (chromemask & GTK_MOZ_EMBED_FLAG_OPENASCHROME)
	{
		*newEmbed = _mozilla_embed_new_xul_dialog ();
		return;
	}

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

static void
ephy_embed_init (EphyEmbedClass *embed_class)
{
	embed_class->load_url = impl_load_url; 
	embed_class->stop_load = impl_stop_load;
	embed_class->can_go_back = impl_can_go_back;
	embed_class->can_go_forward =impl_can_go_forward;
	embed_class->can_go_up = impl_can_go_up;
	embed_class->get_go_up_list = impl_get_go_up_list;
	embed_class->go_back = impl_go_back;
	embed_class->go_forward = impl_go_forward;
	embed_class->go_up = impl_go_up;
	embed_class->get_title = impl_get_title;
	embed_class->get_location = impl_get_location;
	embed_class->reload = impl_reload;
	embed_class->zoom_set = impl_zoom_set;
	embed_class->zoom_get = impl_zoom_get;
	embed_class->shistory_n_items = impl_shistory_n_items;
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
	embed_class->print_preview_n_pages = impl_print_preview_n_pages;
	embed_class->print_preview_navigate = impl_print_preview_navigate;
}

void
xul_visibility_cb (GtkWidget *embed, gboolean visibility, GtkWidget *window)
{
	if (visibility)
	{
		gtk_widget_show (window);
	}
	else
	{
		gtk_widget_hide (window);
	}
}

void
xul_size_to_cb (GtkWidget *embed, gint width, gint height)
{
	gtk_widget_set_size_request (embed, width, height);
}

GtkMozEmbed *
_mozilla_embed_new_xul_dialog (void)
{
	GtkWidget *window, *embed;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	embed = gtk_moz_embed_new ();
	gtk_widget_show (embed);
	gtk_container_add (GTK_CONTAINER (window), embed);

	g_signal_connect_swapped (embed, "destroy_browser",
				  G_CALLBACK (gtk_widget_destroy), window);
	g_signal_connect (embed, "visibility",
			  G_CALLBACK (xul_visibility_cb), window);
	g_signal_connect (embed, "size_to",
			  G_CALLBACK (xul_size_to_cb), NULL);

	return GTK_MOZ_EMBED (embed);
}
