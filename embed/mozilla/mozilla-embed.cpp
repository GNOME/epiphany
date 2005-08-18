/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "mozilla-config.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mozilla-embed.h"
#include "mozilla-embed-event.h"
#include "ephy-embed-shell.h"
#include "ephy-command-manager.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include "EphyBrowser.h"
#include "EventContext.h"
#include "EphyUtils.h"

#include <gtkmozembed.h>
#define MOZILLA_STRICT_API
#include <nsEmbedString.h>
#undef MOZILLA_STRICT_API
#include <nsMemory.h>
#include <nsIURI.h>
#include <nsIRequest.h>
#include <nsIWebProgressListener.h>
#include <nsGfxCIID.h>

static void	mozilla_embed_class_init	(MozillaEmbedClass *klass);
static void	mozilla_embed_init		(MozillaEmbed *gs);
static void	mozilla_embed_destroy		(GtkObject *object);
static void	mozilla_embed_finalize		(GObject *object);
static void	ephy_embed_iface_init		(EphyEmbedIface *iface);

static void mozilla_embed_location_changed_cb	(GtkMozEmbed *embed,
						 MozillaEmbed *membed);
static void mozilla_embed_net_state_all_cb	(GtkMozEmbed *embed,
						 const char *aURI,
						 gint state,
						 guint status,
						 MozillaEmbed *membed);
static gint mozilla_embed_dom_key_down_cb	(GtkMozEmbed *embed,
						 gpointer dom_event,
						 MozillaEmbed *membed);
static gint mozilla_embed_dom_mouse_click_cb	(GtkMozEmbed *embed,
						 gpointer dom_event,
						 MozillaEmbed *membed);
static gint mozilla_embed_dom_mouse_down_cb	(GtkMozEmbed *embed,
						 gpointer dom_event, 
						 MozillaEmbed *membed);
static void mozilla_embed_new_window_cb		(GtkMozEmbed *embed, 
						 GtkMozEmbed **newEmbed,
						 guint chromemask,
						 MozillaEmbed *membed);
static void mozilla_embed_security_change_cb	(GtkMozEmbed *embed, 
						 gpointer request,
						 PRUint32 state,
						 MozillaEmbed *membed);
static EmbedSecurityLevel mozilla_embed_security_level (PRUint32 state);

#define MOZILLA_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED, MozillaEmbedPrivate))

typedef enum
{
	MOZILLA_EMBED_LOAD_STARTED,
	MOZILLA_EMBED_LOAD_REDIRECTING,
	MOZILLA_EMBED_LOAD_LOADING,
	MOZILLA_EMBED_LOAD_STOPPED
} MozillaEmbedLoadState;

struct MozillaEmbedPrivate
{
	EphyBrowser *browser;
	MozillaEmbedLoadState load_state;
#ifdef GTKMOZEMBED_BROKEN_FOCUS
	guint focus_connected : 1;
#endif /* GTKMOZEMBED_BROKEN_FOCUS */
};

#define WINDOWWATCHER_CONTRACTID "@mozilla.org/embedcomp/window-watcher;1"

static GObjectClass *parent_class = NULL;

#ifdef GTKMOZEMBED_BROKEN_FOCUS
static guint fiesid = 0;
static guint foesid = 0;
#endif /* GTKMOZEMBED_BROKEN_FOCUS */

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
	nsresult rv;
	PRBool enabled;

        rv = mpriv->browser->GetCommandState (command, &enabled);

	return NS_SUCCEEDED (rv) ? enabled : FALSE;
}

static void
ephy_command_manager_iface_init (EphyCommandManagerIface *iface)
{
	iface->do_command = impl_manager_do_command;
	iface->can_do_command = impl_manager_can_do_command;
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
			(GInterfaceInitFunc) ephy_embed_iface_init,
        		NULL,
        		NULL
     		};

		static const GInterfaceInfo ephy_command_manager_info =
		{
			(GInterfaceInitFunc) ephy_command_manager_iface_init,
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
	nsresult rv;
        PRBool didFind;

        rv = mpriv->browser->Find (backwards, &didFind);
	
	return NS_SUCCEEDED (rv) ? didFind : FALSE;
}

static void
impl_activate (EphyEmbed *embed) 
{
	gtk_widget_grab_focus (GTK_BIN (embed)->child);
}

static void
impl_find_set_properties (EphyEmbed *embed, 
                          const char *search_string,
	                  gboolean case_sensitive,
			  gboolean wrap_around)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	nsEmbedString searchString;
	NS_CStringToUTF16 (nsEmbedCString(search_string),
			   NS_CSTRING_ENCODING_UTF8, searchString);

	mpriv->browser->FindSetProperties (searchString.get(), case_sensitive,
					   wrap_around); 
}

#ifdef GTKMOZEMBED_BROKEN_FOCUS
static gboolean
child_focus_in_event_cb (GtkWidget *child,
			 GdkEventFocus *event,
			 MozillaEmbed *embed)
{
	embed->priv->browser->FocusActivate ();

	return FALSE;
}

static gboolean
child_focus_out_event_cb (GtkWidget *child,
			  GdkEventFocus *event,
			  MozillaEmbed *embed)
{
	embed->priv->browser->FocusDeactivate ();

	return FALSE;
}
#endif /* GTKMOZEMBED_BROKEN_FOCUS */

static void
mozilla_embed_realize (GtkWidget *widget)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (widget)->priv;
	GtkBin *bin = GTK_BIN (widget);

	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	/* Initialise our helper class */
	nsresult rv;
	rv = mpriv->browser->Init (GTK_MOZ_EMBED (widget));
	if (NS_FAILED (rv))
	{
		g_warning ("EphyBrowser initialization failed for %p\n", widget);
		return;
	}

#ifdef GTKMOZEMBED_BROKEN_FOCUS
	/* HACK ALERT! This depends highly on undocumented interna of
	 * GtkMozEmbed!
	 *
	 * GtkMozEmbed::realize installs focus-[in|out]-event handlers to
	 * toplevel, and, on the first realize only, to the child.
	 * GtkMozEmbed disconnects its focus-[in|out]-event handler
	 * to the toplevel on unrealize, and leaves the ones to the child
	 * in place. So we don't need to unblock the blocked handlers
	 * and therefore need no ::unrealize handler.
	*/

	GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
	gpointer data = ((GtkMozEmbed *) widget)->data;

	guint n;

	n = g_signal_handlers_block_matched (toplevel,
					     (GSignalMatchType) (G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
					     fiesid, 0, NULL, NULL, data);
	n += g_signal_handlers_block_matched (toplevel,
					      (GSignalMatchType) (G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
					      foesid, 0, NULL, NULL, data);
	if (n != 2)
	{
		g_warning ("Unexpected number (n=%d) of toplevel focus handlers found!\n", n);
	}

	if (mpriv->focus_connected) return;

	GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
	g_return_if_fail (child != NULL);

	n = g_signal_handlers_block_matched (child,
					     (GSignalMatchType) (G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
					     fiesid, 0, NULL, NULL, widget);
	n += g_signal_handlers_block_matched (child,
					      (GSignalMatchType) (G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
					      foesid, 0, NULL, NULL, widget);
	if (n != 2)
	{
		g_warning ("Unexpected number (n=%d) of child focus handlers found!\n", n);
	}

	g_signal_connect_object (child, "focus-in-event",
				 G_CALLBACK (child_focus_in_event_cb), widget,
				 G_CONNECT_AFTER);
	g_signal_connect_object (child, "focus-out-event",
				 G_CALLBACK (child_focus_out_event_cb), widget,
				 G_CONNECT_AFTER);

	mpriv->focus_connected = TRUE;
#endif /* GTKMOZEMBED_BROKEN_FOCUS */
}

static GObject *
mozilla_embed_constructor (GType type, guint n_construct_properties,
			   GObjectConstructParam *construct_params)
{
	/* we depend on single because of mozilla initialization */
	ephy_embed_shell_get_embed_single (embed_shell);

	return parent_class->constructor (type, n_construct_properties,
					  construct_params);
}

static void
mozilla_embed_class_init (MozillaEmbedClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
     	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass); 
     	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass); 

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->constructor = mozilla_embed_constructor;
	object_class->finalize = mozilla_embed_finalize;

	gtk_object_class->destroy = mozilla_embed_destroy;

	widget_class->realize = mozilla_embed_realize;

#ifdef GTKMOZEMBED_BROKEN_FOCUS
	fiesid = g_signal_lookup ("focus-in-event", GTK_TYPE_WIDGET);
	foesid = g_signal_lookup ("focus-out-event", GTK_TYPE_WIDGET);
#endif /* GTKMOZEMBED_BROKEN_FOCUS */

	g_type_class_add_private (object_class, sizeof(MozillaEmbedPrivate));
}

static void
mozilla_embed_init (MozillaEmbed *embed)
{
        embed->priv = MOZILLA_EMBED_GET_PRIVATE (embed);
	embed->priv->browser = new EphyBrowser ();

	g_signal_connect_object (G_OBJECT (embed), "location",
				 G_CALLBACK (mozilla_embed_location_changed_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (G_OBJECT (embed), "net_state_all",
				 G_CALLBACK (mozilla_embed_net_state_all_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (G_OBJECT (embed), "dom_mouse_click",
				 G_CALLBACK (mozilla_embed_dom_mouse_click_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (G_OBJECT (embed), "dom_mouse_down",
				 G_CALLBACK (mozilla_embed_dom_mouse_down_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (G_OBJECT (embed), "new_window",
				 G_CALLBACK (mozilla_embed_new_window_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (G_OBJECT (embed), "security_change",
				 G_CALLBACK (mozilla_embed_security_change_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (G_OBJECT (embed), "dom_key_down",
				 G_CALLBACK (mozilla_embed_dom_key_down_cb),
				 embed, (GConnectFlags) 0);
}

gpointer
_mozilla_embed_get_ephy_browser (MozillaEmbed *embed)
{
	g_return_val_if_fail (embed->priv->browser != NULL, NULL);
	
	return embed->priv->browser;
}

static void
mozilla_embed_destroy (GtkObject *object)
{
	MozillaEmbed *embed = MOZILLA_EMBED (object);

	if (embed->priv->browser)
	{
		embed->priv->browser->Destroy();
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
mozilla_embed_finalize (GObject *object)
{
	MozillaEmbed *embed = MOZILLA_EMBED (object);

	if (embed->priv->browser)
	{
        	delete embed->priv->browser;
	       	embed->priv->browser = nsnull;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
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
        rv = EphyUtils::NewURI (getter_AddRefs(uri), nsEmbedCString(aUri));
        if (NS_FAILED(rv) || !uri) return NULL;

	nsEmbedCString path;
	rv = uri->GetPath(path);
	if (NS_FAILED(rv)) return NULL;

	if (!path.Length() || strcmp (path.get(), "/") == 0 ||
	    !strchr (path.get(), '/'))
	{
		return NULL;
	}

	nsEmbedCString parent;
	rv = uri->Resolve (nsEmbedCString(".."), parent);
	if (NS_FAILED(rv)) return NULL;

	return g_strdup (parent.get()); 
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
	if (uri == NULL)
	{
		return;
	}
	
	parent_uri = mozilla_embed_get_uri_parent (uri);
	g_free (uri);

	if (parent_uri)
	{
		ephy_embed_load_url (embed, parent_uri);
		g_free (parent_uri);
	}
}

static char *
impl_get_title (EphyEmbed *embed)
{
	return gtk_moz_embed_get_title (GTK_MOZ_EMBED (embed));
}

static char *
impl_get_link_message (EphyEmbed *embed)
{
	return gtk_moz_embed_get_link_message (GTK_MOZ_EMBED (embed));
}

static char *
impl_get_js_status (EphyEmbed *embed)
{
	return gtk_moz_embed_get_js_status (GTK_MOZ_EMBED (embed));
}

static char *
impl_get_location (EphyEmbed *embed, 
                   gboolean toplevel)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	char *l;
	nsresult rv;
	nsEmbedCString url;

	if (toplevel)
	{
		rv = mpriv->browser->GetDocumentUrl (url);
		l = (NS_SUCCEEDED (rv) && url.Length()) ?
		     g_strdup (url.get()) : NULL;	   	
	}
	else
	{
		rv = mpriv->browser->GetTargetDocumentUrl (url);
		l = (NS_SUCCEEDED (rv) && url.Length()) ?
		     g_strdup (url.get()) : NULL;	   	
	}

	return l;
}

static void
impl_reload (EphyEmbed *embed, 
             gboolean force)
{
#ifndef GTKMOZEMBED_BROKEN_RELOAD
	guint32 mflags;

	mflags = GTK_MOZ_EMBED_FLAG_RELOADNORMAL;

	if (force)
	{
		mflags = GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE;
	}

	gtk_moz_embed_reload (GTK_MOZ_EMBED(embed), mflags);
#else
	/* Workaround for broken reload with frames, see mozilla bug
	 * http://bugzilla.mozilla.org/show_bug.cgi?id=246392
	 */
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	mpriv->browser->Reload (force ? EphyBrowser::RELOAD_FORCE :
					EphyBrowser::RELOAD_NORMAL);
#endif
}

static void
impl_set_zoom (EphyEmbed *embed, 
               float zoom) 
{
	EphyBrowser *browser;
	nsresult rv;

	g_return_if_fail (zoom > 0.0);

	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_if_fail (browser != NULL);

	rv = browser->SetZoom (zoom);

	if (NS_SUCCEEDED (rv))
	{
		g_signal_emit_by_name (embed, "ge_zoom_change", zoom);
	}
}

static float
impl_get_zoom (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	float f;

	nsresult rv;	
	rv = mpriv->browser->GetZoom (&f);
	
	if (NS_SUCCEEDED (rv))
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
        nsEmbedCString url;
	PRUnichar *title;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	if (is_relative)
	{
		nth += ephy_embed_shistory_get_pos (embed);
	}
	
        rv = mpriv->browser->GetSHUrlAtIndex(nth, url);

        *aUrl = (NS_SUCCEEDED (rv) && url.Length()) ? g_strdup(url.get()) : NULL;

	rv = mpriv->browser->GetSHTitleAtIndex(nth, &title);

	if (title)
	{
		nsEmbedCString cTitle;
		NS_UTF16ToCString (nsEmbedString(title),
				   NS_CSTRING_ENCODING_UTF8, cTitle);
		*aTitle = g_strdup (cTitle.get());
		nsMemory::Free (title);
	}
	else
	{
		*aTitle = NULL;
	}
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
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	if (level) *level = STATE_IS_UNKNOWN;
	if (description) *description = NULL;

	nsresult rv;
	PRUint32 state;
	nsEmbedCString desc;
	rv = mpriv->browser->GetSecurityInfo (&state, desc);
	if (NS_FAILED (rv)) return;

	if (level) *level = mozilla_embed_security_level (state);
	if (description) *description = g_strdup (desc.get());
}

static void
impl_show_page_certificate (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	mpriv->browser->ShowCertificate ();
}
	
static void
impl_print (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
 
	mpriv->browser->Print ();
}

static void
impl_set_print_preview_mode (EphyEmbed *embed, gboolean preview_mode)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->SetPrintPreviewMode (preview_mode);
}

static int
impl_print_preview_n_pages (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	int num;

	rv = mpriv->browser->PrintPreviewNumPages(&num);

	return NS_SUCCEEDED (rv) ? num : 0;
}

static void
impl_print_preview_navigate (EphyEmbed *embed,
			     EmbedPrintPreviewNavType type,
			     int page)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->PrintPreviewNavigate(type, page);
}

static void
impl_set_encoding (EphyEmbed *embed,
		   const char *encoding)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsEmbedCString currEnc;

	g_return_if_fail (encoding != NULL);

	rv = mpriv->browser->GetEncoding (currEnc);
	if (NS_FAILED (rv)) return;

	if (strcmp (currEnc.get(), encoding) != 0 ||
	    encoding[0] == '\0' && !ephy_embed_has_automatic_encoding (embed))
	{
		rv = mpriv->browser->ForceEncoding (encoding);
		if (NS_FAILED (rv)) return;
	}

#ifndef GTKMOZEMBED_BROKEN_RELOAD
	gtk_moz_embed_reload (GTK_MOZ_EMBED (embed),
			      GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE);
#else
	/* Workaround for broken reload with frames, see mozilla bug
	 * http://bugzilla.mozilla.org/show_bug.cgi?id=246392
	 */
	mpriv->browser->Reload (EphyBrowser::RELOAD_ENCODING_CHANGE);
#endif
}

static char *
impl_get_encoding (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsEmbedCString encoding;

	rv = mpriv->browser->GetEncoding (encoding);

	if (NS_FAILED (rv) || !encoding.Length())
	{
		return NULL;
	}

	return g_strdup (encoding.get());
}

static gboolean
impl_has_automatic_encoding (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsEmbedCString encoding;

	rv = mpriv->browser->GetForcedEncoding (encoding);

	if (NS_FAILED (rv) || !encoding.Length())
	{
		return TRUE;
	}

	return FALSE;
}

static gboolean
impl_has_modified_forms (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;

	PRBool modified;
	rv = mpriv->browser->GetHasModifiedForms (&modified);

	return NS_SUCCEEDED (rv) ? modified : FALSE;
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
update_load_state (MozillaEmbed *membed, gint state)
{
	MozillaEmbedPrivate *priv = membed->priv;

	if (state & GTK_MOZ_EMBED_FLAG_IS_NETWORK)
	{
		if (state & GTK_MOZ_EMBED_FLAG_START)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_STARTED;
		}
		else if (state & GTK_MOZ_EMBED_FLAG_STOP)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_STOPPED;
		}
	}
	else if (state & GTK_MOZ_EMBED_FLAG_START &&
	         state & GTK_MOZ_EMBED_FLAG_IS_REQUEST)
	{
		if (priv->load_state == MOZILLA_EMBED_LOAD_REDIRECTING)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_STARTED;
		}
		else if (priv->load_state != MOZILLA_EMBED_LOAD_LOADING)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_LOADING;

			char *address;
			address = gtk_moz_embed_get_location (GTK_MOZ_EMBED (membed));
			g_signal_emit_by_name (membed, "ge_content_change", address);
			g_free (address);
		}
	}
	else if (state & GTK_MOZ_EMBED_FLAG_REDIRECTING &&
	         priv->load_state == MOZILLA_EMBED_LOAD_STARTED)
	{
		priv->load_state = MOZILLA_EMBED_LOAD_REDIRECTING;
	}
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

	update_load_state (membed, state);
	
	g_signal_emit_by_name (membed, "ge_net_state", aURI, estate);
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
	NS_ENSURE_TRUE (ev, FALSE);
	nsCOMPtr<nsIDOMEvent> dev = do_QueryInterface (ev);
	NS_ENSURE_TRUE (dev, FALSE);

	MozillaEmbedEvent *info;
	info = mozilla_embed_event_new (NS_STATIC_CAST (gpointer, dev));

	gboolean ret = FALSE;

	nsresult rv;
	EventContext ctx;
	ctx.Init (mpriv->browser);
	rv = ctx.GetKeyEventInfo (ev, info);
	if (NS_FAILED (rv))
	{
		g_object_unref (info);
		return ret;
	}

	if ((info->keycode == nsIDOMKeyEvent::DOM_VK_F10 &&
	    (info->modifier == GDK_SHIFT_MASK ||
	     info->modifier == GDK_CONTROL_MASK))
	    || (info->keycode == nsIDOMKeyEvent::DOM_VK_CONTEXT_MENU &&
		!info->modifier)
	   )
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
	MozillaEmbedEvent *info;
	EventContext event_context;
	gint return_value = FALSE;
	nsresult rv;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_mouse_click_cb: domevent NULL");
		return FALSE;
	}

	nsCOMPtr<nsIDOMMouseEvent> ev = static_cast<nsIDOMMouseEvent*>(dom_event);
	NS_ENSURE_TRUE (ev, FALSE);
	nsCOMPtr<nsIDOMEvent> dev = do_QueryInterface (ev);
	NS_ENSURE_TRUE (dev, FALSE);

	info = mozilla_embed_event_new (NS_STATIC_CAST (gpointer, dev));

	event_context.Init (mpriv->browser);
        rv = event_context.GetMouseEventInfo (ev, MOZILLA_EMBED_EVENT (info));

	if (NS_SUCCEEDED (rv))
	{
		nsCOMPtr<nsIDOMDocument> domDoc;
		rv = event_context.GetTargetDocument (getter_AddRefs(domDoc));
		if (NS_SUCCEEDED (rv))
		{
			rv = mpriv->browser->PushTargetDocument (domDoc);
			if (NS_SUCCEEDED (rv))
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
	MozillaEmbedEvent *info;
	EventContext event_context;
	gint return_value = FALSE;
	nsresult rv;
	EphyEmbedEventType type;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	if (dom_event == NULL)
	{
		g_warning ("mozilla_embed_dom_mouse_down_cb: domevent NULL");
		return FALSE;
	}

	nsCOMPtr<nsIDOMMouseEvent> ev = static_cast<nsIDOMMouseEvent*>(dom_event);
	NS_ENSURE_TRUE (ev, FALSE);
	nsCOMPtr<nsIDOMEvent> dev = do_QueryInterface (ev);
	NS_ENSURE_TRUE (dev, FALSE);

	info = mozilla_embed_event_new (NS_STATIC_CAST (gpointer, dev));

	event_context.Init (mpriv->browser);
        rv = event_context.GetMouseEventInfo (ev, MOZILLA_EMBED_EVENT (info));
	if (NS_FAILED (rv)) return FALSE;

	type = ephy_embed_event_get_event_type ((EphyEmbedEvent *) info);
		
	nsCOMPtr<nsIDOMDocument> domDoc;
	rv = event_context.GetTargetDocument (getter_AddRefs(domDoc));
	if (NS_SUCCEEDED (rv))
	{
		rv = mpriv->browser->PushTargetDocument (domDoc);

		if (NS_SUCCEEDED (rv))
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
                             guint chrome_mask, 
			     MozillaEmbed *membed)
{
	guint i;
	guint mask = 0;
	EphyEmbed *new_embed = NULL;

	struct
	{
		guint mozilla_mask;
		guint embed_mask;
	}
	conversion_map [] =
	{
		{ GTK_MOZ_EMBED_FLAG_MENUBARON, EPHY_EMBED_CHROME_MENUBAR },
		{ GTK_MOZ_EMBED_FLAG_TOOLBARON, EPHY_EMBED_CHROME_TOOLBAR },
		{ GTK_MOZ_EMBED_FLAG_STATUSBARON, EPHY_EMBED_CHROME_STATUSBAR },
		{ GTK_MOZ_EMBED_FLAG_PERSONALTOOLBARON, EPHY_EMBED_CHROME_BOOKMARKSBAR },
		{ 0, 0 }
	};

	if (chrome_mask & GTK_MOZ_EMBED_FLAG_OPENASCHROME)
	{
		*newEmbed = _mozilla_embed_new_xul_dialog ();
		return;
	}

	for (i = 0; conversion_map[i].mozilla_mask != 0; i++)
	{
		if (chrome_mask & conversion_map[i].mozilla_mask)
		{
			mask |= conversion_map[i].embed_mask;
		}
	}

	g_signal_emit_by_name (membed, "ge_new_window", &new_embed, mask);

	g_assert (new_embed != NULL);
	
	*newEmbed = GTK_MOZ_EMBED(new_embed);
}

static void
mozilla_embed_security_change_cb (GtkMozEmbed *embed, 
				  gpointer requestptr,
				  PRUint32 state,
				  MozillaEmbed *membed)
{
	g_signal_emit_by_name (membed, "ge_security_change",
			       mozilla_embed_security_level (state));
}

static EmbedSecurityLevel
mozilla_embed_security_level (PRUint32 state)
{
	EmbedSecurityLevel level;

	switch (state)
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
ephy_embed_iface_init (EphyEmbedIface *iface)
{
	iface->load_url = impl_load_url; 
	iface->stop_load = impl_stop_load;
	iface->can_go_back = impl_can_go_back;
	iface->can_go_forward =impl_can_go_forward;
	iface->can_go_up = impl_can_go_up;
	iface->get_go_up_list = impl_get_go_up_list;
	iface->go_back = impl_go_back;
	iface->go_forward = impl_go_forward;
	iface->go_up = impl_go_up;
	iface->get_title = impl_get_title;
	iface->get_location = impl_get_location;
	iface->get_link_message = impl_get_link_message;
	iface->get_js_status = impl_get_js_status;
	iface->reload = impl_reload;
	iface->set_zoom = impl_set_zoom;
	iface->get_zoom = impl_get_zoom;
	iface->shistory_n_items = impl_shistory_n_items;
	iface->shistory_get_nth = impl_shistory_get_nth;
	iface->shistory_get_pos = impl_shistory_get_pos;
	iface->shistory_go_nth = impl_shistory_go_nth;
	iface->get_security_level = impl_get_security_level;
	iface->show_page_certificate = impl_show_page_certificate;
	iface->find_next = impl_find_next;
	iface->activate = impl_activate;
	iface->find_set_properties = impl_find_set_properties;
	iface->set_encoding = impl_set_encoding;
	iface->get_encoding = impl_get_encoding;
	iface->has_automatic_encoding = impl_has_automatic_encoding;
	iface->print = impl_print;
	iface->set_print_preview_mode = impl_set_print_preview_mode;
	iface->print_preview_n_pages = impl_print_preview_n_pages;
	iface->print_preview_navigate = impl_print_preview_navigate;
	iface->has_modified_forms = impl_has_modified_forms;
}

static void
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

static void
xul_size_to_cb (GtkWidget *embed, gint width, gint height, gpointer dummy)
{
	gtk_widget_set_size_request (embed, width, height);
}

static void
xul_new_window_cb (GtkMozEmbed *embed,
		   GtkMozEmbed **retval, 
		   guint chrome_mask,
		   gpointer dummy)
{
        g_assert (chrome_mask & GTK_MOZ_EMBED_FLAG_OPENASCHROME);

        *retval = _mozilla_embed_new_xul_dialog ();
}

GtkMozEmbed *
_mozilla_embed_new_xul_dialog (void)
{
	GtkWidget *window, *embed;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	embed = gtk_moz_embed_new ();
	gtk_widget_show (embed);
	gtk_container_add (GTK_CONTAINER (window), embed);

	g_signal_connect_object (embed, "destroy_browser",
				 G_CALLBACK (gtk_widget_destroy),
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (embed, "visibility",
				 G_CALLBACK (xul_visibility_cb),
				 window, (GConnectFlags) 0);
	g_signal_connect_object (embed, "size_to",
				 G_CALLBACK (xul_size_to_cb),
				 NULL, (GConnectFlags) 0);
	g_signal_connect_object (embed, "new_window",
				 G_CALLBACK (xul_new_window_cb),
				 NULL, (GConnectFlags) 0);

	return GTK_MOZ_EMBED (embed);
}
