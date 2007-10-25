/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "mozilla-config.h"
#include "config.h"

#include <nsStringAPI.h>

#include <gtkmozembed.h>
#include <nsIDOMKeyEvent.h>
#include <nsIDOMMouseEvent.h>
#include <nsIRequest.h>
#include <nsIURI.h>
#include <nsIWebNavigation.h>
#include <nsIWebProgressListener.h>
#include <nsMemory.h>

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#include "EphyBrowser.h"
#include "EphyUtils.h"
#include "EventContext.h"

#include "ephy-command-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-history.h"
#include "ephy-string.h"
#include "mozilla-embed-event.h"

#include "mozilla-embed.h"

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
static gboolean mozilla_embed_dom_mouse_click_cb(GtkMozEmbed *embed,
						 gpointer dom_event,
						 MozillaEmbed *membed);
static gboolean mozilla_embed_dom_mouse_down_cb	(GtkMozEmbed *embed,
						 gpointer dom_event, 
						 MozillaEmbed *membed);
static gboolean mozilla_embed_dom_key_press_cb	(GtkMozEmbed *embed,
						 gpointer dom_event, 
						 MozillaEmbed *membed);
static void mozilla_embed_new_window_cb		(GtkMozEmbed *embed, 
						 GtkMozEmbed **newEmbed,
						 guint chrome_mask,
						 MozillaEmbed *membed);
static void mozilla_embed_security_change_cb	(GtkMozEmbed *embed, 
						 gpointer request,
						 PRUint32 state,
						 MozillaEmbed *membed);
static void mozilla_embed_document_type_cb	(EphyEmbed *embed,
						 EphyEmbedDocumentType type,
						 MozillaEmbed *membed);
static void mozilla_embed_zoom_change_cb	(EphyEmbed *embed,
						 float zoom,
						 MozillaEmbed *membed);
static void mozilla_embed_title_change_cb       (EphyEmbed *embed,
                                                 MozillaEmbed *membed);
static void mozilla_embed_link_message_cb       (EphyEmbed *embed,
                                                 MozillaEmbed *membed);
static void mozilla_embed_set_title             (MozillaEmbed *embed,
                                                 char *title);
static void mozilla_embed_set_loading_title     (MozillaEmbed *embed,
                                                 const char *title,
                                                 gboolean is_address);
static void impl_set_typed_address		(EphyEmbed *embed,
						 const char *address,
						 EphyEmbedAddressExpire expire);

static EphyEmbedSecurityLevel mozilla_embed_security_level (PRUint32 state);

#define MOZILLA_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED, MozillaEmbedPrivate))

typedef enum
{
	MOZILLA_EMBED_LOAD_STARTED,
	MOZILLA_EMBED_LOAD_REDIRECTING,
	MOZILLA_EMBED_LOAD_LOADING,
	MOZILLA_EMBED_LOAD_STOPPED
} MozillaEmbedLoadState;

#define MAX_TITLE_LENGTH	512 /* characters */
#define RELOAD_DELAY		250 /* ms */
#define RELOAD_DELAY_MAX_TICKS	40  /* RELOAD_DELAY * RELOAD_DELAY_MAX_TICKS = 10 s */

struct MozillaEmbedPrivate
{
	EphyBrowser *browser;
	MozillaEmbedLoadState load_state;

	EphyEmbedAddressExpire address_expire;
	/* guint address_expire : 2; ? */
	EphyEmbedSecurityLevel security_level;
	/* guint security_level : 3; ? */
	EphyEmbedDocumentType document_type;
	EphyEmbedNavigationFlags nav_flags;
	float zoom;

	/* Flags */
	guint is_blank : 1;
	guint is_loading : 1;
	guint is_setting_zoom : 1;

	gint8 load_percent;
	char *address;
	char *typed_address;
	char *title;
	char *loading_title;
	int cur_requests;
	int total_requests;
	char *status_message;
	char *link_message;

	/* File watch */
	GnomeVFSMonitorHandle *monitor;
	guint reload_scheduled_id;
	guint reload_delay_ticks;	
};

#define WINDOWWATCHER_CONTRACTID "@mozilla.org/embedcomp/window-watcher;1"

enum
{
	PROP_0,
	PROP_ADDRESS,
	PROP_DOCUMENT_TYPE,
        PROP_LINK_MESSAGE,
	PROP_LOAD_PROGRESS,
	PROP_LOAD_STATUS,
	PROP_NAVIGATION,
	PROP_SECURITY,
	PROP_STATUS_MESSAGE,
        PROP_TITLE,
	PROP_TYPED_ADDRESS,
	PROP_ZOOM
};

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

G_DEFINE_TYPE_WITH_CODE (MozillaEmbed, mozilla_embed, GTK_TYPE_MOZ_EMBED,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED,
						ephy_embed_iface_init)
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_COMMAND_MANAGER,
						ephy_command_manager_iface_init))
	
static void
mozilla_embed_grab_focus (GtkWidget *widget)
{
	GtkWidget *child;

	child = gtk_bin_get_child (GTK_BIN (widget));

	if (child != NULL)
	{
		gtk_widget_grab_focus (child);
	}
	else
	{
		g_warning ("Need to realize the embed before grabbing focus!\n");
	}
}

static void
impl_close (EphyEmbed *embed) 
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	mpriv->browser->Close ();
}

static void
mozilla_embed_realize (GtkWidget *widget)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (widget)->priv;

	GTK_WIDGET_CLASS (mozilla_embed_parent_class)->realize (widget);

	/* Initialise our helper class */
	nsresult rv;
	rv = mpriv->browser->Init (GTK_MOZ_EMBED (widget));
	if (NS_FAILED (rv))
	{
		g_warning ("EphyBrowser initialization failed for %p\n", widget);
		return;
	}
}

static GObject *
mozilla_embed_constructor (GType type, guint n_construct_properties,
			   GObjectConstructParam *construct_params)
{
	g_object_ref (embed_shell);

	/* we depend on single because of mozilla initialization */
	ephy_embed_shell_get_embed_single (embed_shell);

	return G_OBJECT_CLASS (mozilla_embed_parent_class)->constructor (type, n_construct_properties,
									 construct_params);
}

static void
mozilla_embed_get_property (GObject *object,
			    guint prop_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	MozillaEmbed *embed = MOZILLA_EMBED (object);
	MozillaEmbedPrivate *priv = embed->priv;
	
	switch (prop_id)
	{
        case PROP_ADDRESS:
                g_value_set_string (value, priv->address);
                break;
	case PROP_DOCUMENT_TYPE:
		g_value_set_enum (value, priv->document_type);
		break;
	case PROP_SECURITY:
		g_value_set_enum (value, priv->security_level);
		break;
	case PROP_ZOOM:
		g_value_set_float (value, priv->zoom);
		break;
        case PROP_LINK_MESSAGE:
                g_value_set_string (value, priv->link_message);
                break;
	case PROP_LOAD_PROGRESS:
		g_value_set_int (value, priv->load_percent);
		break;
	case PROP_LOAD_STATUS:
		g_value_set_boolean (value, priv->is_loading);
		break;
	case PROP_NAVIGATION:
		g_value_set_flags (value, priv->nav_flags);
		break;
        case PROP_STATUS_MESSAGE:
                g_value_set_string (value, priv->status_message);
                break;
        case PROP_TITLE:
                g_value_set_string (value, priv->title);
                break;
	case PROP_TYPED_ADDRESS:
		g_value_set_string (value, priv->typed_address);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

}

static void
mozilla_embed_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	switch (prop_id)
	{
	case PROP_TYPED_ADDRESS:
		impl_set_typed_address (EPHY_EMBED (object), g_value_get_string (value),
					EPHY_EMBED_ADDRESS_EXPIRE_NOW);
		break;
        case PROP_ADDRESS:
	case PROP_DOCUMENT_TYPE:
	case PROP_LOAD_PROGRESS:
	case PROP_LOAD_STATUS:
        case PROP_LINK_MESSAGE:
	case PROP_NAVIGATION:
	case PROP_SECURITY:
        case PROP_STATUS_MESSAGE:
	case PROP_ZOOM:
		/* read only */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
mozilla_embed_class_init (MozillaEmbedClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass); 
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass); 

	mozilla_embed_parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->constructor = mozilla_embed_constructor;
	object_class->finalize = mozilla_embed_finalize;
	object_class->get_property = mozilla_embed_get_property;
	object_class->set_property = mozilla_embed_set_property;

	gtk_object_class->destroy = mozilla_embed_destroy;

	widget_class->grab_focus = mozilla_embed_grab_focus;
	widget_class->realize = mozilla_embed_realize;

	g_object_class_override_property (object_class, PROP_DOCUMENT_TYPE, "document-type");
	g_object_class_override_property (object_class, PROP_SECURITY, "security-level");
	g_object_class_override_property (object_class, PROP_ZOOM, "zoom");
	g_object_class_override_property (object_class, PROP_LOAD_PROGRESS, "load-progress");
	g_object_class_override_property (object_class, PROP_LOAD_STATUS, "load-status");
	g_object_class_override_property (object_class, PROP_NAVIGATION, "navigation");
	g_object_class_override_property (object_class, PROP_ADDRESS, "address");
	g_object_class_override_property (object_class, PROP_TYPED_ADDRESS, "typed-address");
	g_object_class_override_property (object_class, PROP_TITLE, "title");
	g_object_class_override_property (object_class, PROP_STATUS_MESSAGE, "message");
	g_object_class_override_property (object_class, PROP_LINK_MESSAGE, "link-message");

	g_type_class_add_private (object_class, sizeof(MozillaEmbedPrivate));
}

static void
mozilla_embed_init (MozillaEmbed *embed)
{
	MozillaEmbedPrivate *priv = embed->priv;
	priv = MOZILLA_EMBED_GET_PRIVATE (embed);
	priv->browser = new EphyBrowser ();

	g_signal_connect_object (embed, "location",
				 G_CALLBACK (mozilla_embed_location_changed_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "net_state_all",
				 G_CALLBACK (mozilla_embed_net_state_all_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "dom_mouse_click",
				 G_CALLBACK (mozilla_embed_dom_mouse_click_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "dom_mouse_down",
				 G_CALLBACK (mozilla_embed_dom_mouse_down_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "dom-key-press",
				 G_CALLBACK (mozilla_embed_dom_key_press_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "new_window",
				 G_CALLBACK (mozilla_embed_new_window_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "security_change",
				 G_CALLBACK (mozilla_embed_security_change_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "ge_document_type",
				 G_CALLBACK (mozilla_embed_document_type_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "ge_zoom_change",
				 G_CALLBACK (mozilla_embed_zoom_change_cb),
				 embed, (GConnectFlags) 0);
        g_signal_connect_object (embed, "title",
                                 G_CALLBACK (mozilla_embed_title_change_cb),
                                 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "link_message",
				 G_CALLBACK (mozilla_embed_link_message_cb),
				 embed, (GConnectFlags)0);

	priv->document_type = EPHY_EMBED_DOCUMENT_HTML;
	priv->security_level = EPHY_EMBED_STATE_IS_UNKNOWN;
	priv->zoom = 1.0;
	priv->is_setting_zoom = FALSE;
	priv->load_percent = 0;
	priv->is_loading = FALSE;
	priv->typed_address = NULL;
        priv->address = NULL;
	priv->address_expire = EPHY_EMBED_ADDRESS_EXPIRE_NOW;
	priv->title = NULL;
        priv->loading_title = NULL;
	priv->is_blank = TRUE;
	priv->total_requests = 0;
	priv->cur_requests = 0;
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
	
	GTK_OBJECT_CLASS (mozilla_embed_parent_class)->destroy (object);
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

	g_free (embed->priv->address);
	g_free (embed->priv->typed_address);
	g_free (embed->priv->title);
	g_free (embed->priv->loading_title);
	g_free (embed->priv->status_message);
	g_free (embed->priv->link_message);

	G_OBJECT_CLASS (mozilla_embed_parent_class)->finalize (object);

	g_object_unref (embed_shell);
}

static void
impl_load_url (EphyEmbed *embed, 
	       const char *url)
{
	gtk_moz_embed_load_url (GTK_MOZ_EMBED(embed), url);
}

static char * impl_get_location (EphyEmbed *embed, gboolean toplevel);

static void
impl_load (EphyEmbed *embed, 
	   const char *url,
	   EphyEmbedLoadFlags flags,
	   EphyEmbed *preview_embed)
{
	EphyBrowser *browser;

	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_if_fail (browser != NULL);

	nsCOMPtr<nsIURI> uri;
	if (preview_embed != NULL)
	{
		EphyBrowser *pbrowser;

		pbrowser = MOZILLA_EMBED(preview_embed)->priv->browser;
		if (pbrowser != NULL)
		{
			pbrowser->GetDocumentURI (getter_AddRefs (uri));
		}
	}

#ifdef HAVE_GECKO_1_8_1
	if (flags & EPHY_EMBED_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP)
	{
		browser->LoadURI (url, nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP, uri);	
	}
	else
#endif /* HAVE_GECKO_1_8_1 */
	{
		browser->LoadURI (url, nsIWebNavigation::LOAD_FLAGS_NONE, uri);	
	}
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

static gboolean
mozilla_embed_get_uri_parent (MozillaEmbed *membed,
			      const char *aUri,
			      nsCString &aParent)
{
	nsresult rv;
	nsCString encoding;
	rv = membed->priv->browser->GetEncoding (encoding);
	if (NS_FAILED (rv)) return FALSE;

	nsCOMPtr<nsIURI> uri;
	rv = EphyUtils::NewURI (getter_AddRefs(uri), nsCString(aUri), encoding.get());
	if (NS_FAILED(rv) || !uri) return FALSE;

	/* Don't support going 'up' with chrome url's, mozilla handily
	 * fixes them up for us, so it doesn't work properly, see
	 * rdf/chrome/src/nsChromeProtocolHandler.cpp::NewURI()
	 * (the Canonify() call)
	 */
	nsCString scheme;
	rv = uri->GetScheme (scheme);
	if (NS_FAILED(rv) || !scheme.Length()) return FALSE;
	if (strcmp (scheme.get(), "chrome") == 0) return FALSE;

	nsCString path;
	rv = uri->GetPath(path);
	if (NS_FAILED(rv) || !path.Length()) return FALSE;
	if (strcmp (path.get (), "/") == 0) return FALSE;

	const char *slash = strrchr (path.BeginReading(), '/');
	if (!slash) return FALSE;

	if (slash[1] == '\0')
	{
		/* ends with a slash - a directory, go to parent */
		rv = uri->Resolve (nsCString(".."), aParent);
	}
	else
	{
		/* it's a file, go to the directory */
		rv = uri->Resolve (nsCString("."), aParent);
	}

	return NS_SUCCEEDED (rv);
}

static gboolean
impl_can_go_up (EphyEmbed *embed)
{
	MozillaEmbed *membed = MOZILLA_EMBED (embed);
	char *address;
	gboolean result;

	address = ephy_embed_get_location (embed, TRUE);
	if (address == NULL) return FALSE;

	nsCString parent;
	result = mozilla_embed_get_uri_parent (membed, address, parent);
	g_free (address);

	return result;
}

static GSList *
impl_get_go_up_list (EphyEmbed *embed)
{
	MozillaEmbed *membed = MOZILLA_EMBED (embed);
	GSList *l = NULL;
	char *address, *s;

	address = ephy_embed_get_location (embed, TRUE);
	if (address == NULL) return NULL;

	s = address;
	nsCString parent;
	while (mozilla_embed_get_uri_parent (membed, s, parent))
	{
		s = g_strdup (parent.get());
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
	MozillaEmbed *membed = MOZILLA_EMBED (embed);
	char *uri;

	uri = ephy_embed_get_location (embed, TRUE);
	if (uri == NULL) return;

	gboolean rv;
	nsCString parent_uri;
	rv = mozilla_embed_get_uri_parent (membed, uri, parent_uri);
	g_free (uri);

	g_return_if_fail (rv != FALSE);

	ephy_embed_load_url (embed, parent_uri.get ());
}

static const char *
impl_get_title (EphyEmbed *embed)
{
        return MOZILLA_EMBED (embed)->priv->title;
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
	nsresult rv;

	nsCOMPtr<nsIURI> uri;
	if (toplevel)
	{
		rv = mpriv->browser->GetDocumentURI (getter_AddRefs (uri));
	}
	else
	{
		rv = mpriv->browser->GetTargetDocumentURI (getter_AddRefs (uri));
	}

	if (NS_FAILED (rv)) return NULL;

	nsCOMPtr<nsIURI> furi;
	rv = uri->Clone (getter_AddRefs (furi));
	/* Some nsIURI impls return NS_OK even though they didn't put anything in the outparam!! */
	if (NS_FAILED (rv) || !furi) furi.swap(uri);

	/* Hide password part */
	nsCString user;
	furi->GetUsername (user);
	furi->SetUserPass (user);

	nsCString url;
	furi->GetSpec (url);

	return url.Length() ? g_strdup (url.get()) : NULL;
}

static void
impl_reload (EphyEmbed *embed, 
	     gboolean force)
{
	guint32 mflags = GTK_MOZ_EMBED_FLAG_RELOADNORMAL;

	if (force)
	{
		mflags = GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE;
	}

	gtk_moz_embed_reload (GTK_MOZ_EMBED(embed), mflags);
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

static void
impl_scroll_lines (EphyEmbed *embed,
		   int num_lines)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->ScrollLines (num_lines);
}

static void
impl_scroll_pages (EphyEmbed *embed,
		   int num_pages)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->ScrollPages (num_pages);
}

static void
impl_scroll_pixels (EphyEmbed *embed,
		    int dx,
		    int dy)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->ScrollPixels (dx, dy);
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
	nsCString url;
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
		nsCString cTitle;
		NS_UTF16ToCString (nsString(title),
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
impl_shistory_copy (EphyEmbed *source,
		    EphyEmbed *dest,
		    gboolean copy_back,
		    gboolean copy_forward,
		    gboolean copy_current)
{
	MozillaEmbedPrivate *spriv = MOZILLA_EMBED(source)->priv;
	MozillaEmbedPrivate *dpriv = MOZILLA_EMBED(dest)->priv;

	spriv->browser->CopySHistory(dpriv->browser, copy_back,
				     copy_forward, copy_current);
}

static void
impl_get_security_level (EphyEmbed *embed,
			 EphyEmbedSecurityLevel *level,
			 char **description)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	if (level) *level = EPHY_EMBED_STATE_IS_UNKNOWN;
	if (description) *description = NULL;

	nsresult rv;
	PRUint32 state;
	nsCString desc;
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
			     EphyEmbedPrintPreviewNavType type,
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
	nsCString currEnc;

	g_return_if_fail (encoding != NULL);

	rv = mpriv->browser->GetEncoding (currEnc);
	if (NS_FAILED (rv)) return;

	if (strcmp (currEnc.get(), encoding) != 0 ||
	    encoding[0] == '\0' && !ephy_embed_has_automatic_encoding (embed))
	{
		rv = mpriv->browser->ForceEncoding (encoding);
		if (NS_FAILED (rv)) return;
	}

	gtk_moz_embed_reload (GTK_MOZ_EMBED (embed),
			      GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE);
}

static char *
impl_get_encoding (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsCString encoding;

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
	nsCString encoding;

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

static EphyEmbedDocumentType
impl_get_document_type (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	return mpriv->document_type;
}

static int
impl_get_load_percent (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	return mpriv->load_percent;
}

static void
mozilla_embed_set_load_percent (MozillaEmbed *embed, int percent)
{
       MozillaEmbedPrivate *mpriv = embed->priv;

       if (percent != mpriv->load_percent)
       {
	       mpriv->load_percent = percent;

	       g_object_notify (G_OBJECT (embed), "load-progress");
       }
}

static gboolean
impl_get_load_status (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	return mpriv->is_loading;
}

static void
mozilla_embed_set_load_status (MozillaEmbed *embed, gboolean status)
{
       MozillaEmbedPrivate *mpriv = embed->priv;
       guint is_loading;

       is_loading = status != FALSE;

       if (is_loading != mpriv->is_loading)
       {
	       mpriv->is_loading = is_loading;

	       g_object_notify (G_OBJECT (embed), "load-status");
       }
}

static void
mozilla_embed_update_navigation_flags (MozillaEmbed *membed)
{
	MozillaEmbedPrivate *priv = membed->priv;
        EphyEmbed *embed = EPHY_EMBED (membed);
	guint flags = 0;

	if (impl_can_go_up (embed))
	{
		flags |= EPHY_EMBED_NAV_UP;
	}

        if (impl_can_go_back (embed))
	{
		flags |= EPHY_EMBED_NAV_BACK;
	}

	if (impl_can_go_forward (embed))
	{
		flags |= EPHY_EMBED_NAV_FORWARD;
	}

	if (priv->nav_flags != (EphyEmbedNavigationFlags)flags)
	{
		priv->nav_flags = (EphyEmbedNavigationFlags)flags;

		g_object_notify (G_OBJECT (embed), "navigation");
	}
}

static EphyEmbedNavigationFlags
impl_get_navigation_flags (EphyEmbed *embed)
{
	MozillaEmbedPrivate *priv = MOZILLA_EMBED (embed)->priv;
	return priv->nav_flags;
}

static const char*
impl_get_typed_address (EphyEmbed *embed)
{
	return MOZILLA_EMBED (embed)->priv->typed_address;
}

static void
impl_set_typed_address (EphyEmbed *embed,
			const char *address,
			EphyEmbedAddressExpire expire)
{
       MozillaEmbedPrivate *priv = MOZILLA_EMBED (embed)->priv;
 
       g_free (priv->typed_address);
       priv->typed_address = g_strdup (address);

       if (expire == EPHY_EMBED_ADDRESS_EXPIRE_CURRENT &&
	   !priv->is_loading)
       {
	       priv->address_expire = EPHY_EMBED_ADDRESS_EXPIRE_NOW;
       }
       else
       {
	       priv->address_expire = expire;
       }

       g_object_notify (G_OBJECT (embed), "typed-address");
}

static const char*
impl_get_address (EphyEmbed *embed)
{
        MozillaEmbedPrivate *priv = MOZILLA_EMBED (embed)->priv;
  
	return priv->address ? priv->address : "about:blank";
}

static const char*
impl_get_status_message (EphyEmbed *embed)
{
        MozillaEmbedPrivate *priv = MOZILLA_EMBED (embed)->priv;

	if (priv->link_message && priv->link_message[0] != '\0')
	{
		return priv->link_message;
	}
	else if (priv->status_message)
	{
		return priv->status_message;
	}
	else
	{
		return NULL;
	}
}

static const char*
impl_get_link_message (EphyEmbed *embed)
{
        MozillaEmbedPrivate *priv = MOZILLA_EMBED (embed)->priv;

        return priv->link_message;
}

static void
mozilla_embed_set_address (MozillaEmbed *embed, char *address)
{
        MozillaEmbedPrivate *priv = embed->priv;
	GObject *object = G_OBJECT (embed);
	
	g_free (priv->address);
	priv->address = address;

	priv->is_blank = address == NULL ||
			 strcmp (address, "about:blank") == 0;

	if (priv->is_loading &&
	    priv->address_expire == EPHY_EMBED_ADDRESS_EXPIRE_NOW &&
	    priv->typed_address != NULL)
	{
		g_free (priv->typed_address);
		priv->typed_address = NULL;

		g_object_notify (object, "typed-address");
	}

	g_object_notify (object, "address");
}

static void
mozilla_embed_file_monitor_cancel (MozillaEmbed *embed)
{
	MozillaEmbedPrivate *priv = embed->priv;

	if (priv->monitor != NULL)
	{
		LOG ("Cancelling file monitor");

		gnome_vfs_monitor_cancel (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->reload_scheduled_id != 0)
	{
		LOG ("Cancelling scheduled reload");

		g_source_remove (priv->reload_scheduled_id);
		priv->reload_scheduled_id = 0;
	}

	priv->reload_delay_ticks = 0;
}

static gboolean
ephy_file_monitor_reload_cb (MozillaEmbed *embed)
{
	MozillaEmbedPrivate *priv = embed->priv;

	if (priv->reload_delay_ticks > 0)
	{
		priv->reload_delay_ticks--;

		/* Run again */
		return TRUE;
	}

	if (priv->is_loading)
	{
		/* Wait a bit to reload if we're still loading! */
		priv->reload_delay_ticks = RELOAD_DELAY_MAX_TICKS / 2;

		/* Run again */
		return TRUE;
	}

	priv->reload_scheduled_id = 0;

	LOG ("Reloading file '%s'", impl_get_address (embed));

	impl_reload (EPHY_EMBED (embed), TRUE);

	/* don't run again */
	return FALSE;
}

static void
mozilla_embed_file_monitor_cb (GnomeVFSMonitorHandle *handle,
                               const gchar *monitor_uri,
                               const gchar *info_uri,
                               GnomeVFSMonitorEventType event_type,
                               MozillaEmbed *embed)
{
	gboolean uri_is_directory;
	gboolean should_reload;
	char* local_path;
	MozillaEmbedPrivate *priv = embed->priv;

	LOG ("File '%s' has changed, scheduling reload", monitor_uri);

	local_path = gnome_vfs_get_local_path_from_uri(monitor_uri);
	uri_is_directory = g_file_test(local_path, G_FILE_TEST_IS_DIR);
	g_free(local_path);

	switch (event_type)
	{
		/* These events will always trigger a reload: */
		case GNOME_VFS_MONITOR_EVENT_CHANGED:
		case GNOME_VFS_MONITOR_EVENT_CREATED:
			should_reload = TRUE;
			break;

		/* These events will only trigger a reload for directories: */
		case GNOME_VFS_MONITOR_EVENT_DELETED:
		case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
			should_reload = uri_is_directory;
			break;

		/* These events don't trigger a reload: */
		case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
		case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
		default:
			should_reload = FALSE;
			break;
	}

	if (should_reload) {
		/* We make a lot of assumptions here, but basically we know
		 * that we just have to reload, by construction.
		 * Delay the reload a little bit so we don't endlessly
		 * reload while a file is written.
		 */
		if (priv->reload_delay_ticks == 0)
		{
			priv->reload_delay_ticks = 1;
		}
		else
		{
			/* Exponential backoff */
			priv->reload_delay_ticks = MIN (priv->reload_delay_ticks * 2,
					RELOAD_DELAY_MAX_TICKS);
		}

		if (priv->reload_scheduled_id == 0)
		{
			priv->reload_scheduled_id =
				g_timeout_add (RELOAD_DELAY,
						(GSourceFunc) ephy_file_monitor_reload_cb, embed);
		}
	}
}

static void
mozilla_embed_update_file_monitor (MozillaEmbed *embed,
                                   const gchar *address)
{
	MozillaEmbedPrivate *priv = embed->priv;
	GnomeVFSMonitorHandle *handle = NULL;
	gboolean local;
	char* local_path;
	GnomeVFSMonitorType monitor_type;

	if (priv->monitor != NULL &&
	    priv->address != NULL && address != NULL &&
	    strcmp (priv->address, address) == 0)
	
	{
		/* same address, no change needed */
		return;
	}

	mozilla_embed_file_monitor_cancel (embed);

	local = g_str_has_prefix (address, "file://");
	if (local == FALSE) return;
	
	local_path = gnome_vfs_get_local_path_from_uri(address);
	monitor_type = g_file_test(local_path, G_FILE_TEST_IS_DIR)
		? GNOME_VFS_MONITOR_DIRECTORY
		: GNOME_VFS_MONITOR_FILE;
	g_free(local_path);

	if (gnome_vfs_monitor_add (&handle, address,
				   monitor_type,
				   (GnomeVFSMonitorCallback) mozilla_embed_file_monitor_cb,
				   embed) == GNOME_VFS_OK)
	{
		LOG ("Installed monitor for file '%s'", address);

		priv->monitor = handle;
	}
}

static void
mozilla_embed_set_link_message (MozillaEmbed *embed,
                                char *link_message)
{
	char *status_message;
	char **splitted_message;
        MozillaEmbedPrivate *priv = embed->priv;

	g_free (priv->link_message);
	status_message = ephy_string_blank_chr (link_message);
	
	if (status_message && g_str_has_prefix (status_message, "mailto:"))
	{
		int i = 1;
		char *p;
		GString *tmp;
		
		/* We first want to eliminate all the things after "?", like
		 * cc, subject and alike.
		 */
		
		p = strchr (status_message, '?');
		if (p != NULL) *p = '\0';
		
		/* Then we also want to check if there is more than an email address
		 * in the mailto: list.
		 */
		
		splitted_message = g_strsplit_set (status_message, ";", -1);
		tmp = g_string_new (g_strdup_printf (_("Send an email message to “%s”"),
						     (splitted_message[0] + 7)));
		
		while (splitted_message [i] != NULL)
		{
			g_string_append_printf (tmp, ", “%s”", splitted_message[i]);
			i++;
		}
		
		priv->link_message = g_string_free (tmp, FALSE);
		
		g_free (status_message);
		g_strfreev (splitted_message);
	}
	else
	{
		priv->link_message = status_message;
	}

	g_object_notify (G_OBJECT (embed), "status-message");
	g_object_notify (G_OBJECT (embed), "link-message");
}

static void
mozilla_embed_location_changed_cb (GtkMozEmbed *embed, 
				   MozillaEmbed *membed)
{
	char *location;
        GObject *object = G_OBJECT (embed);

	location = gtk_moz_embed_get_location (embed);
	g_signal_emit_by_name (membed, "ge_location", location);
	g_free (location);

	g_object_freeze_notify (object);

	/* do this up here so we still have the old address around */
	mozilla_embed_update_file_monitor (membed, location);

	/* Do not expose about:blank to the user, an empty address
	   bar will do better */
	if (location == NULL || location[0] == '\0' ||
	    strcmp (location, "about:blank") == 0)
	{
		mozilla_embed_set_address (membed, NULL);
		mozilla_embed_set_title (membed, NULL);
	}
	else
	{
		char *embed_address;

		/* we do this to get rid of an eventual password in the URL */
		embed_address = impl_get_location (EPHY_EMBED (embed), TRUE);
		mozilla_embed_set_address (membed, embed_address);
		mozilla_embed_set_loading_title (membed, embed_address, TRUE);
	}

	mozilla_embed_set_link_message (membed, NULL);
#if 0
	mozilla_embed_set_icon_address (embed, NULL);
#endif
	mozilla_embed_update_navigation_flags (membed);

	g_object_notify (object, "title");

	g_object_thaw_notify (object);
}

static void
mozilla_embed_link_message_cb       (EphyEmbed *embed,
                                     MozillaEmbed *membed)
{
  char *link_message = gtk_moz_embed_get_link_message (GTK_MOZ_EMBED (membed));
  mozilla_embed_set_link_message (membed, link_message);
  g_free (link_message);
}

static gboolean
address_has_web_scheme (const char *address)
{
	gboolean has_web_scheme;

	if (address == NULL) return FALSE;

	has_web_scheme = (g_str_has_prefix (address, "http:") ||
			  g_str_has_prefix (address, "https:") ||
			  g_str_has_prefix (address, "ftp:") ||
			  g_str_has_prefix (address, "file:") ||
			  g_str_has_prefix (address, "data:") ||
			  g_str_has_prefix (address, "about:") ||
			  g_str_has_prefix (address, "gopher:"));

	return has_web_scheme;
}

static void
mozilla_embed_restore_zoom_level (MozillaEmbed *membed, const char *address)
{
	MozillaEmbedPrivate *priv = membed->priv;

	/* restore zoom level */
	if (address_has_web_scheme (address))
	{
		EphyHistory *history;
		EphyNode *host;
		GValue value = { 0, };
		float zoom = 1.0, current_zoom;

		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		host = ephy_history_get_host (history, address);

		if (host != NULL && ephy_node_get_property
				     (host, EPHY_NODE_HOST_PROP_ZOOM, &value))
		{
			zoom = g_value_get_float (&value);
			g_value_unset (&value);
		}

		current_zoom = ephy_embed_get_zoom (EPHY_EMBED (membed));
		if (zoom != current_zoom)
		{
			priv->is_setting_zoom = TRUE;
			ephy_embed_set_zoom (EPHY_EMBED (membed), zoom);
			priv->is_setting_zoom = FALSE;
		}
	}
}

static void
update_load_state (MozillaEmbed *membed, gint state)
{
	MozillaEmbedPrivate *priv = membed->priv;

	if (state & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT &&
	    state & (GTK_MOZ_EMBED_FLAG_START | GTK_MOZ_EMBED_FLAG_STOP))
	{
		g_signal_emit_by_name (membed, "ge-document-type",
				       priv->browser->GetDocumentType ());
	}

	if (state & GTK_MOZ_EMBED_FLAG_RESTORING &&
	    priv->load_state == MOZILLA_EMBED_LOAD_STARTED)
	{
		priv->load_state = MOZILLA_EMBED_LOAD_LOADING;

		char *address;
		address = gtk_moz_embed_get_location (GTK_MOZ_EMBED (membed));
		g_signal_emit_by_name (membed, "ge-content-change", address);
		mozilla_embed_restore_zoom_level (membed, address);
		g_free (address);
	}

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
			mozilla_embed_restore_zoom_level (membed, address);
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
update_net_state_message (MozillaEmbed *embed, const char *uri, EphyEmbedNetState flags)
{
	GnomeVFSURI *vfs_uri = NULL;
	const char *msg = NULL;
	const char *host = NULL;

	if (uri != NULL)
	{
		vfs_uri = gnome_vfs_uri_new (uri);
	}

	if (vfs_uri != NULL)
	{
		host = gnome_vfs_uri_get_host_name (vfs_uri);
	}

	if (host == NULL || host[0] == '\0') goto out;

	/* IS_REQUEST and IS_NETWORK can be both set */
	if (flags & EPHY_EMBED_STATE_IS_REQUEST)
	{
		if (flags & EPHY_EMBED_STATE_REDIRECTING)
		{
			msg = _("Redirecting to “%s”…");
		}
		else if (flags & EPHY_EMBED_STATE_TRANSFERRING)
		{
			msg = _("Transferring data from “%s”…");
		}
		else if (flags & EPHY_EMBED_STATE_NEGOTIATING)
		{
			msg = _("Waiting for authorization from “%s”…");
		}
	}

	if (flags & EPHY_EMBED_STATE_IS_NETWORK)
	{
		if (flags & EPHY_EMBED_STATE_START)
		{
			msg = _("Loading “%s”…");
		}
	}

	if ((flags & EPHY_EMBED_STATE_IS_NETWORK) &&
	    (flags & EPHY_EMBED_STATE_STOP))
	{
		g_free (embed->priv->status_message);
		embed->priv->status_message = NULL;
		g_object_notify (G_OBJECT (embed), "message");

	}
	else if (msg != NULL)
	{
		g_free (embed->priv->status_message);
		g_free (embed->priv->loading_title);
		embed->priv->status_message = g_strdup_printf (msg, host);
		embed->priv->loading_title = g_strdup_printf (msg, host);
		g_object_notify (G_OBJECT (embed), "message");
		g_object_notify (G_OBJECT (embed), "title");
	}

out:
	if (vfs_uri != NULL)
	{
		gnome_vfs_uri_unref (vfs_uri);
	}
}

static int
build_load_percent (int requests_done, int requests_total)
{
	int percent= 0;

	if (requests_total > 0)
	{
		percent = (requests_done * 100) / requests_total;

		/* Mozilla sometimes report more done requests than
		   total requests. Their progress widget clamp the value */
		percent = CLAMP (percent, 0, 100);
	}

	return percent;
}

static void
build_progress_from_requests (MozillaEmbed *embed, EphyEmbedNetState state)
{
	int load_percent;

	if (state & EPHY_EMBED_STATE_IS_REQUEST)
        {
                if (state & EPHY_EMBED_STATE_START)
                {
			embed->priv->total_requests ++;
		}
		else if (state & EPHY_EMBED_STATE_STOP)
		{
			embed->priv->cur_requests ++;
		}

		load_percent = build_load_percent (embed->priv->cur_requests,
						   embed->priv->total_requests);

		mozilla_embed_set_load_percent (embed, load_percent);
	}
}

static void
ensure_page_info (MozillaEmbed *embed, const char *address)
{
	MozillaEmbedPrivate *priv = embed->priv;

	if ((priv->address == NULL || priv->address[0] == '\0') &&
	    priv->address_expire == EPHY_EMBED_ADDRESS_EXPIRE_NOW)
        {
                mozilla_embed_set_address (embed, g_strdup (address));
	}

	/* FIXME huh?? */
	if (priv->title == NULL || priv->title[0] == '\0')
	{
		mozilla_embed_set_title (embed, NULL);
	}
}

static void
update_embed_from_net_state (MozillaEmbed *embed,
                             const char *uri,
                             EphyEmbedNetState state)
{
	MozillaEmbedPrivate *priv = embed->priv;

	update_net_state_message (embed, uri, state);

	if (state & EPHY_EMBED_STATE_IS_NETWORK)
	{
		if (state & EPHY_EMBED_STATE_START)
		{
			GObject *object = G_OBJECT (embed);

			g_object_freeze_notify (object);

			priv->total_requests = 0;
			priv->cur_requests = 0;

			mozilla_embed_set_load_percent (embed, 0);
			mozilla_embed_set_load_status (embed, TRUE);

			ensure_page_info (embed, uri);

			g_object_notify (object, "title");

			g_object_thaw_notify (object);
		}
		else if (state & EPHY_EMBED_STATE_STOP)
		{
			GObject *object = G_OBJECT (embed);

			g_object_freeze_notify (object);

			mozilla_embed_set_load_percent (embed, 100);
			mozilla_embed_set_load_status (embed, FALSE);

			g_free (priv->loading_title);
			priv->loading_title = NULL;

			priv->address_expire = EPHY_EMBED_ADDRESS_EXPIRE_NOW;

			g_object_notify (object, "title");

			g_object_thaw_notify (object);
		}

                mozilla_embed_update_navigation_flags (embed);
	}

	build_progress_from_requests (embed, state);
}

static void
mozilla_embed_net_state_all_cb (GtkMozEmbed *embed, const char *aURI,
				gint state, guint status, 
				MozillaEmbed *membed)
{
	EphyEmbedNetState estate = EPHY_EMBED_STATE_UNKNOWN;
	int i;

	struct
	{
		guint state;
		EphyEmbedNetState embed_state;
	}
	conversion_map [] =
	{
		{ GTK_MOZ_EMBED_FLAG_START, EPHY_EMBED_STATE_START },
		{ GTK_MOZ_EMBED_FLAG_STOP, EPHY_EMBED_STATE_STOP },
		{ GTK_MOZ_EMBED_FLAG_REDIRECTING, EPHY_EMBED_STATE_REDIRECTING },
		{ GTK_MOZ_EMBED_FLAG_TRANSFERRING, EPHY_EMBED_STATE_TRANSFERRING },
		{ GTK_MOZ_EMBED_FLAG_NEGOTIATING, EPHY_EMBED_STATE_NEGOTIATING },
		{ GTK_MOZ_EMBED_FLAG_IS_REQUEST, EPHY_EMBED_STATE_IS_REQUEST },
		{ GTK_MOZ_EMBED_FLAG_IS_DOCUMENT, EPHY_EMBED_STATE_IS_DOCUMENT },
		{ GTK_MOZ_EMBED_FLAG_IS_NETWORK, EPHY_EMBED_STATE_IS_NETWORK },
		{ GTK_MOZ_EMBED_FLAG_RESTORING, EPHY_EMBED_STATE_RESTORING },
		{ 0, EPHY_EMBED_STATE_UNKNOWN }
	};

	for (i = 0; conversion_map[i].state != 0; i++)
	{
		if (state & conversion_map[i].state)
		{
			estate = (EphyEmbedNetState) (estate | conversion_map[i].embed_state);	
		}
	}

	update_load_state (membed, state);
        update_embed_from_net_state (membed, aURI, (EphyEmbedNetState)state);
	
	g_signal_emit_by_name (membed, "ge_net_state", aURI, /* FIXME: (gulong) */ estate);
}

static gboolean
mozilla_embed_emit_mouse_signal (MozillaEmbed *embed,
				 gpointer dom_event, 
				 const char *signal_name)
{
	MozillaEmbedPrivate *mpriv = embed->priv;
	MozillaEmbedEvent *info;
	EventContext event_context;
	gint return_value = FALSE;
	nsresult rv;

	if (dom_event == NULL) return FALSE;

	nsCOMPtr<nsIDOMMouseEvent> ev = static_cast<nsIDOMMouseEvent*>(dom_event);
	NS_ENSURE_TRUE (ev, FALSE);
	nsCOMPtr<nsIDOMEvent> dev = do_QueryInterface (ev);
	NS_ENSURE_TRUE (dev, FALSE);

	info = mozilla_embed_event_new (static_cast<gpointer>(dev));

	event_context.Init (mpriv->browser);
	rv = event_context.GetMouseEventInfo (ev, MOZILLA_EMBED_EVENT (info));
	if (NS_FAILED (rv))
	{
		g_object_unref (info);
		return FALSE;
	}

	nsCOMPtr<nsIDOMDocument> domDoc;
	rv = event_context.GetTargetDocument (getter_AddRefs(domDoc));
	if (NS_SUCCEEDED (rv))
	{
		mpriv->browser->PushTargetDocument (domDoc);

		g_signal_emit_by_name (embed, signal_name, 
				       info, &return_value); 
		mpriv->browser->PopTargetDocument ();
	}

	g_object_unref (info);

	return return_value;
}

static gboolean
mozilla_embed_dom_mouse_click_cb (GtkMozEmbed *embed,
				  gpointer dom_event, 
				  MozillaEmbed *membed)
{
	return mozilla_embed_emit_mouse_signal (membed, dom_event,
						"ge_dom_mouse_click");
}

static gboolean
mozilla_embed_dom_mouse_down_cb (GtkMozEmbed *embed, gpointer dom_event, 
				 MozillaEmbed *membed)
{
	return mozilla_embed_emit_mouse_signal (membed, dom_event,
						"ge_dom_mouse_down");
}

static gint
mozilla_embed_dom_key_press_cb (GtkMozEmbed *embed,
				gpointer dom_event, 
				MozillaEmbed *membed)
{
	gint retval = FALSE;

	if (dom_event == NULL) return FALSE;

	nsCOMPtr<nsIDOMKeyEvent> ev = static_cast<nsIDOMKeyEvent*>(dom_event);
	NS_ENSURE_TRUE (ev, FALSE);

	if (!EventContext::CheckKeyPress (ev)) return FALSE;

	GdkEvent *event = gtk_get_current_event ();
	if (event == NULL) return FALSE; /* shouldn't happen! */

	g_return_val_if_fail (GDK_KEY_PRESS == event->type, FALSE);

	g_signal_emit_by_name (embed, "ge-search-key-press", event, &retval);

	gdk_event_free (event);

	return retval;
}

EphyEmbedChrome
_mozilla_embed_translate_chrome (GtkMozEmbedChromeFlags flags)
{
	static const struct
	{
		guint mozilla_flag;
		guint ephy_flag;
	}
	conversion_map [] =
	{
		{ GTK_MOZ_EMBED_FLAG_MENUBARON, EPHY_EMBED_CHROME_MENUBAR },
		{ GTK_MOZ_EMBED_FLAG_TOOLBARON, EPHY_EMBED_CHROME_TOOLBAR },
		{ GTK_MOZ_EMBED_FLAG_STATUSBARON, EPHY_EMBED_CHROME_STATUSBAR },
		{ GTK_MOZ_EMBED_FLAG_PERSONALTOOLBARON, EPHY_EMBED_CHROME_BOOKMARKSBAR },
	};

	guint mask = 0, i;

	for (i = 0; i < G_N_ELEMENTS (conversion_map); i++)
	{
		if (flags & conversion_map[i].mozilla_flag)
		{
			mask |= conversion_map[i].ephy_flag;
		}
	}

	return (EphyEmbedChrome) mask;
}

static void
mozilla_embed_new_window_cb (GtkMozEmbed *embed, 
			     GtkMozEmbed **newEmbed,
			     guint chrome_mask, 
			     MozillaEmbed *membed)
{
	GtkMozEmbedChromeFlags chrome = (GtkMozEmbedChromeFlags) chrome_mask;
	EphyEmbed *new_embed = NULL;
	GObject *single;
	EphyEmbedChrome mask;

	if (chrome & GTK_MOZ_EMBED_FLAG_OPENASCHROME)
	{
		*newEmbed = _mozilla_embed_new_xul_dialog ();
		return;
	}

	mask = _mozilla_embed_translate_chrome (chrome);

	single = ephy_embed_shell_get_embed_single (embed_shell);
	g_signal_emit_by_name (single, "new-window", embed, mask,
			       &new_embed);

	g_assert (new_embed != NULL);

	gtk_moz_embed_set_chrome_mask (GTK_MOZ_EMBED (new_embed), chrome);

	g_signal_emit_by_name (membed, "ge-new-window", new_embed);

	*newEmbed = GTK_MOZ_EMBED (new_embed);
}

static void
mozilla_embed_set_security_level (MozillaEmbed *embed, EphyEmbedSecurityLevel level)
{
	MozillaEmbedPrivate *priv = embed->priv;

	if (priv->security_level != level)
	{
		priv->security_level = level;

		g_object_notify (G_OBJECT (embed), "security-level");
	}
}

static void
mozilla_embed_security_change_cb (GtkMozEmbed *embed, 
				  gpointer requestptr,
				  PRUint32 state,
				  MozillaEmbed *membed)
{
	mozilla_embed_set_security_level (membed, mozilla_embed_security_level (state));
}

static void
mozilla_embed_document_type_cb (EphyEmbed *embed,
				EphyEmbedDocumentType type,
				MozillaEmbed *membed)
{
	if (membed->priv->document_type != type)
	{
		membed->priv->document_type = type;

		g_object_notify (G_OBJECT (membed), "document-type");
	}
}

static void
mozilla_embed_zoom_change_cb (EphyEmbed *embed,
			      float zoom,
			      MozillaEmbed *membed)
{
	char *address;

	if (membed->priv->zoom != zoom)
	{
		if (membed->priv->is_setting_zoom)
		  {
		    return;
		  }

		address = ephy_embed_get_location (embed, TRUE);
		if (address_has_web_scheme (address))
		  {
		    EphyHistory *history;
		    EphyNode *host;
		    history = EPHY_HISTORY
		      (ephy_embed_shell_get_global_history (embed_shell));
		    host = ephy_history_get_host (history, address);

		    if (host != NULL)
		      {
			ephy_node_set_property_float (host,
						      EPHY_NODE_HOST_PROP_ZOOM,
						      zoom);
		      }
		  }

		g_free (address);

		membed->priv->zoom = zoom;

		g_object_notify (G_OBJECT (membed), "zoom");
	}
}

static char *
get_title_from_address (const char *address)
{
	GnomeVFSURI *uri;
	char *title;

	if (address == NULL) return NULL;
		
	uri = gnome_vfs_uri_new (address);
	if (uri == NULL) return g_strdup (address);
		
	title = gnome_vfs_uri_to_string (uri,
                                         (GnomeVFSURIHideOptions)
                                         (GNOME_VFS_URI_HIDE_USER_NAME |
                                         GNOME_VFS_URI_HIDE_PASSWORD |
                                         GNOME_VFS_URI_HIDE_HOST_PORT |
                                         GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD |
                                          GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER));
	gnome_vfs_uri_unref (uri);

	return title;
}

static void
mozilla_embed_set_loading_title (MozillaEmbed *embed,
                                 const char *title,
                                 gboolean is_address)
{
        MozillaEmbedPrivate *priv = embed->priv;
	char *freeme = NULL;

	g_free (priv->loading_title);
	priv->loading_title = NULL;

	if (is_address)
	{
		title = freeme = get_title_from_address (title);	
	}

	if (title != NULL && title[0] != '\0')
	{
		/* translators: %s here is the address of the web page */
		priv->loading_title = g_strdup_printf (_("Loading “%s”…"), title);
	}
	else
	{
		priv->loading_title = g_strdup (_("Loading…"));
	}

	g_free (freeme);
}

static void
mozilla_embed_set_title (MozillaEmbed *embed,
                         char *title)
{
        MozillaEmbedPrivate *priv = embed->priv;

	if (!priv->is_blank && (title == NULL || g_strstrip (title)[0] == '\0'))
	{

		g_free (title);
		title = get_title_from_address (priv->address);

		/* Fallback */
		if (title == NULL || title[0] == '\0')
		{
			g_free (title);
			title = NULL;
			priv->is_blank = TRUE;
		}
	}
	else if (priv->is_blank && title != NULL)
	{
		g_free (title);
		title = NULL;
	}

	g_free (priv->title);
	priv->title = ephy_string_shorten (title, MAX_TITLE_LENGTH);

	g_object_notify (G_OBJECT (embed), "title");
}

static void
mozilla_embed_title_change_cb       (EphyEmbed *embed,
                                     MozillaEmbed *membed)
{
	GObject *object = G_OBJECT (embed);
        char *title;

        title = gtk_moz_embed_get_title (GTK_MOZ_EMBED (embed));

	g_object_freeze_notify (object);

	mozilla_embed_set_title (membed, title);
	mozilla_embed_set_loading_title (membed, title, FALSE);

	g_object_thaw_notify (object);
}

static EphyEmbedSecurityLevel
mozilla_embed_security_level (PRUint32 state)
{
	EphyEmbedSecurityLevel level;

	switch (state)
	{
	case nsIWebProgressListener::STATE_IS_INSECURE:
		level = EPHY_EMBED_STATE_IS_INSECURE;
		break;
	case nsIWebProgressListener::STATE_IS_BROKEN:
		level = EPHY_EMBED_STATE_IS_BROKEN;
		break;
	case nsIWebProgressListener::STATE_IS_SECURE|
	     nsIWebProgressListener::STATE_SECURE_HIGH:
		level = EPHY_EMBED_STATE_IS_SECURE_HIGH;
		break;
	case nsIWebProgressListener::STATE_IS_SECURE|
	     nsIWebProgressListener::STATE_SECURE_MED:
		level = EPHY_EMBED_STATE_IS_SECURE_MED;
		break;
	case nsIWebProgressListener::STATE_IS_SECURE|
	     nsIWebProgressListener::STATE_SECURE_LOW:
		level = EPHY_EMBED_STATE_IS_SECURE_LOW;
		break;
	default:
		level = EPHY_EMBED_STATE_IS_UNKNOWN;
		break;
	}
	return level;
}

static void
ephy_embed_iface_init (EphyEmbedIface *iface)
{
	iface->load_url = impl_load_url; 
	iface->load = impl_load; 
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
	iface->scroll_lines = impl_scroll_lines;
	iface->scroll_pages = impl_scroll_pages;
	iface->scroll_pixels = impl_scroll_pixels;
	iface->shistory_n_items = impl_shistory_n_items;
	iface->shistory_get_nth = impl_shistory_get_nth;
	iface->shistory_get_pos = impl_shistory_get_pos;
	iface->shistory_go_nth = impl_shistory_go_nth;
	iface->shistory_copy = impl_shistory_copy;
	iface->get_security_level = impl_get_security_level;
	iface->show_page_certificate = impl_show_page_certificate;
	iface->close = impl_close;
	iface->set_encoding = impl_set_encoding;
	iface->get_encoding = impl_get_encoding;
	iface->has_automatic_encoding = impl_has_automatic_encoding;
	iface->print = impl_print;
	iface->set_print_preview_mode = impl_set_print_preview_mode;
	iface->print_preview_n_pages = impl_print_preview_n_pages;
	iface->print_preview_navigate = impl_print_preview_navigate;
	iface->has_modified_forms = impl_has_modified_forms;
	iface->get_document_type = impl_get_document_type;
	iface->get_load_percent = impl_get_load_percent;
	iface->get_load_status = impl_get_load_status;
	iface->get_navigation_flags = impl_get_navigation_flags;
	iface->get_typed_address = impl_get_typed_address;
	iface->set_typed_address = impl_set_typed_address;
	iface->get_address = impl_get_address;
        iface->get_status_message = impl_get_status_message;
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

static void
xul_title_cb (GtkMozEmbed *embed,
	      GtkWindow *window)
{
	char *title;

	title = gtk_moz_embed_get_title (embed);
	gtk_window_set_title (window, title);
	g_free (title);
}

GtkMozEmbed *
_mozilla_embed_new_xul_dialog (void)
{
	GtkWidget *window, *embed;

	g_object_ref (embed_shell);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_object_set_data_full (G_OBJECT (window), "EmbedShellRef",
				embed_shell,
				(GDestroyNotify) g_object_unref);
	g_signal_connect_object (embed_shell, "prepare_close",
				 G_CALLBACK (gtk_widget_destroy), window,
				 (GConnectFlags) G_CONNECT_SWAPPED);

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
	g_signal_connect_object (embed, "title",
				 G_CALLBACK (xul_title_cb),
				 window, (GConnectFlags) 0);

	return GTK_MOZ_EMBED (embed);
}
