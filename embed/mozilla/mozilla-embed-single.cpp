/*  vim:set ts=8 noet sw=8:
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003 Robert Marcano
 *  Copyright (C) 2003, 2004, 2005, 2006 Christian Persch
 *  Copyright (C) 2005 Crispin Flowerday
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

#include "config.h"

#include "mozilla-embed-single.h"

#include "ephy-cookie-manager.h"
#include "ephy-password-manager.h"
#include "ephy-permission-manager.h"
#include "ephy-certificate-manager.h"
#include "ephy-embed-shell.h"

#include "glib.h"
#include "ephy-debug.h"
#include "gtkmozembed.h"
#include "gtkmozembed_internal.h"
#include "mozilla-embed.h"
#include "ephy-file-helpers.h"
#include "mozilla-notifiers.h"
#include "ephy-langs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "MozRegisterComponents.h"
#include "EphySingle.h"
#include "EphyBrowser.h"
#include "EphyUtils.h"
#include "MozillaPrivate.h"
#include "mozilla-x509-cert.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <nsCOMPtr.h>
#include <nsMemory.h>
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsIWindowWatcher.h>
#include <nsIIOService.h>
#include <nsISupportsPrimitives.h>
#include <nsICookieManager.h>
#include <nsICookie2.h>
#include <nsICookieManager.h>
#include <nsIPasswordManager.h>
#include <nsCPasswordManager.h>
#include <nsIPermission.h>
#include <nsIPermissionManager.h>
#include <nsILocalFile.h>
#include <nsIURI.h>

#if defined(HAVE_MOZILLA_TOOLKIT) && defined(HAVE_GECKO_1_8)
#include "EphyDirectoryProvider.h"
#endif

#ifdef HAVE_MOZILLA_PSM
#include <nsIX509Cert.h>
#include <nsIX509CertDB.h>
#endif

#ifdef HAVE_NSIPASSWORD_H
#include <nsIPassword.h>
#endif

#if defined (HAVE_CHROME_NSICHROMEREGISTRYSEA_H)
#include <chrome/nsIChromeRegistrySea.h>
#elif defined(MOZ_NSIXULCHROMEREGISTRY_SELECTSKIN)
#include <nsIChromeRegistry.h>
#endif

#ifdef ALLOW_PRIVATE_API
// FIXME: For setting the locale. hopefully gtkmozembed will do itself soon
#include <nsILocaleService.h>
#include <nsIHttpAuthManager.h>
#include <nsICacheService.h>
#include <nsIFontEnumerator.h>
#include <nsNetCID.h>
#include <nsIIDNService.h>
#endif /* ALLOW_PRIVATE_API */

#ifdef HAVE_GECKO_1_8
#include <nsIStyleSheetService.h>
#include "EphyUtils.h"
#endif

#include <stdlib.h>

#ifdef ENABLE_NETWORK_MANAGER
#include <libnm_glib.h>
#endif

#define MOZILLA_PROFILE_DIR  "/mozilla"
#define MOZILLA_PROFILE_NAME "epiphany"
#define MOZILLA_PROFILE_FILE "prefs.js"
#define DEFAULT_PROFILE_FILE SHARE_DIR"/default-prefs.js"

#define MOZILLA_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSinglePrivate))

struct MozillaEmbedSinglePrivate
{
	char *user_prefs;
	
	/* monitor this widget for theme changes*/
	GtkWidget *theme_window;

	EphySingle *mSingleObserver;

#ifdef ENABLE_NETWORK_MANAGER
	libnm_glib_ctx *nm_context;
	guint nm_callback_id;
#endif

	guint online : 1;
};

enum
{
	PROP_0,
	PROP_NETWORK_STATUS
};

static void mozilla_embed_single_class_init	(MozillaEmbedSingleClass *klass);
static void ephy_embed_single_iface_init	(EphyEmbedSingleIface *iface);
static void ephy_cookie_manager_iface_init	(EphyCookieManagerIface *iface);
static void ephy_password_manager_iface_init	(EphyPasswordManagerIface *iface);
static void ephy_permission_manager_iface_init	(EphyPermissionManagerIface *iface);
static void mozilla_embed_single_init		(MozillaEmbedSingle *ges);

#ifdef ENABLE_CERTIFICATE_MANAGER
static void ephy_certificate_manager_iface_init	(EphyCertificateManagerIface *iface);
#endif

static GObjectClass *parent_class = NULL;

GType
mozilla_embed_single_get_type (void)
{
       static GType type = 0;

        if (G_UNLIKELY (type == 0))
        {
                static const GTypeInfo our_info =
                {
                        sizeof (MozillaEmbedSingleClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) mozilla_embed_single_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (MozillaEmbedSingle),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) mozilla_embed_single_init
                };

		static const GInterfaceInfo embed_single_info =
		{
			(GInterfaceInitFunc) ephy_embed_single_iface_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo cookie_manager_info =
		{
			(GInterfaceInitFunc) ephy_cookie_manager_iface_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo password_manager_info =
		{
			(GInterfaceInitFunc) ephy_password_manager_iface_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo permission_manager_info =
		{
			(GInterfaceInitFunc) ephy_permission_manager_iface_init,
			NULL,
			NULL
		};

#ifdef ENABLE_CERTIFICATE_MANAGER
		static const GInterfaceInfo certificate_manager_info =
		{
			(GInterfaceInitFunc) ephy_certificate_manager_iface_init,
			NULL,
			NULL
		};
#endif

		type = g_type_register_static (G_TYPE_OBJECT,
					       "MozillaEmbedSingle",
					       &our_info,
					       (GTypeFlags)0);

		g_type_add_interface_static (type,
					     EPHY_TYPE_EMBED_SINGLE,
					     &embed_single_info);

		g_type_add_interface_static (type,
					     EPHY_TYPE_COOKIE_MANAGER,
					     &cookie_manager_info);

		g_type_add_interface_static (type,
					     EPHY_TYPE_PASSWORD_MANAGER,
					     &password_manager_info);

		g_type_add_interface_static (type,
					     EPHY_TYPE_PERMISSION_MANAGER,
					     &permission_manager_info);
#ifdef ENABLE_CERTIFICATE_MANAGER
		g_type_add_interface_static (type,
					     EPHY_TYPE_CERTIFICATE_MANAGER,
					     &certificate_manager_info);
#endif
	}

        return type;
}

static gboolean
mozilla_set_default_prefs (MozillaEmbedSingle *mes)
{
	nsCOMPtr<nsIPrefService> prefService;

        prefService = do_GetService (NS_PREFSERVICE_CONTRACTID);
	NS_ENSURE_TRUE (prefService, FALSE);

        /* read our predefined default prefs */
        nsresult rv;
        nsCOMPtr<nsILocalFile> file;
        NS_NewNativeLocalFile(nsEmbedCString(DEFAULT_PROFILE_FILE),
			      PR_TRUE, getter_AddRefs(file));
	if (!file) return FALSE;

        rv = prefService->ReadUserPrefs (file);                                                                              
        if (NS_FAILED(rv))
        {
                g_warning ("failed to read default preferences, error: %x", rv);
		return FALSE;
        }
                                                                                                                             
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs(pref));
	NS_ENSURE_TRUE (pref, FALSE);

	/* We do this before reading the user pref file so that the user
	 * still can overwrite this pref.
	 * We don't use the default-prefs.js file since that cannot be
	 * localised (see bug #144909).
	 */
	/* translators: this is the URL that searches from the location
	 * entry get directed to. The search terms will be _appended_ to it,
	 * in url-escaped UTF-8; that means that if you're choosing google,
	 * the 'q=' part needs to come last.
	 */
	pref->SetCharPref ("keyword.URL", _("http://www.google.com/search?ie=UTF-8&oe=UTF-8&q="));

        /* Load the default user preferences as well.  This also makes the
           prefs to be saved in the user's prefs.js file, instead of messing up
           our global defaults file. */
        rv = prefService->ReadUserPrefs (nsnull);
        if (NS_FAILED(rv))
        {
                g_warning ("failed to read user preferences, error: %x", rv);
        }

#ifdef HAVE_GECKO_1_8
	/* FIXME: maybe only set the major version ("1.6", "1.8") here? */
	pref->SetCharPref ("general.useragent.extra.epiphany", "Epiphany/" VERSION);

	/* Unset old prefs, otherwise they end up in the user agent string too */
	pref->ClearUserPref ("general.useragent.vendor");
	pref->ClearUserPref ("general.useragent.vendorSub");
#else
	pref->SetCharPref ("general.useragent.vendor", "Epiphany");
	pref->SetCharPref ("general.useragent.vendorSub", VERSION);
#endif

	/* Open ftp uris with an external handler if one is setup */
	pref->SetBoolPref ("network.protocol-handler.external.ftp", PR_FALSE);	

	return TRUE;
}

static char *
color_to_string (GdkColor color)
{
	return g_strdup_printf ("#%.2x%.2x%.2x",
				color.red >> 8,
				color.green >> 8,
				color.blue >> 8);
}

static void
mozilla_update_colors (GtkWidget *window,
                       GtkStyle *previous_style,
                       MozillaEmbedSingle *mes)
{
	nsCOMPtr<nsIPrefService> prefService;
	GdkColor color;
	char *str;

        prefService = do_GetService (NS_PREFSERVICE_CONTRACTID);
	if (!prefService) return;

        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));
	if (!pref) return;

	/* Set the bg color to the text bg color*/
	color = window->style->base[GTK_STATE_NORMAL];
	str = color_to_string (color);
	pref->SetCharPref ("browser.display.background_color", str);		
	g_free (str);

	/* Set the text color */
	color = window->style->text[GTK_STATE_NORMAL];
	str = color_to_string (color);	
	pref->SetCharPref ("browser.display.foreground_color", str);	
	g_free (str);

	/* FIXME: We should probably monitor and set link color here too,
	 * but i'm not really sure what to do about that yet
	 */
}

static void
mozilla_setup_colors (MozillaEmbedSingle *mes)
{
	GtkWidget *window;

	/* Create a random window to monitor for theme changes */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	mes->priv->theme_window = window;
	gtk_widget_realize (window);
	gtk_widget_ensure_style (window);

	/* monitor theme changes*/
	g_signal_connect (G_OBJECT (window), "style-set",
			  G_CALLBACK (mozilla_update_colors), mes);
	
	/* Initialize the colors */
	mozilla_update_colors (window, NULL, mes);

	mes->priv->theme_window = window;
}			

static void 
mozilla_embed_single_new_window_orphan_cb (GtkMozEmbedSingle *moz_single,
					   GtkMozEmbed **newEmbed,
					   guint chrome_mask,
					   EphyEmbedSingle *single)
{
	GtkMozEmbedChromeFlags chrome = (GtkMozEmbedChromeFlags) chrome_mask;
	EphyEmbed *new_embed = NULL;
	EphyEmbedChrome mask;

	if (chrome_mask & GTK_MOZ_EMBED_FLAG_OPENASCHROME)
	{
		*newEmbed = _mozilla_embed_new_xul_dialog ();
		return;
	}

	mask = _mozilla_embed_translate_chrome (chrome);

	g_signal_emit_by_name (single, "new-window", NULL, mask,
			       &new_embed);

	/* it's okay not to have a new embed */
	if (new_embed != NULL)
	{
		gtk_moz_embed_set_chrome_mask (GTK_MOZ_EMBED (new_embed), chrome);

		*newEmbed = GTK_MOZ_EMBED (new_embed);
	}
}

static void
mozilla_init_plugin_path ()
{
	const char *user_path;
	char *new_path;

	user_path = g_getenv ("MOZ_PLUGIN_PATH");
	new_path = g_strconcat (user_path ? user_path : "",
				user_path ? ":" : "",
				MOZILLA_PREFIX "/lib/mozilla/plugins"
				":" MOZILLA_HOME "/plugins",
#ifdef HAVE_PRIVATE_PLUGINS
				":" PLUGINDIR,
#endif
				NULL);

	g_setenv ("MOZ_PLUGIN_PATH", new_path, TRUE);
	g_free (new_path);
}

static void
mozilla_init_single (MozillaEmbedSingle *mes)
{	
	GtkMozEmbedSingle *single;
	
	/* get single */
        single = gtk_moz_embed_single_get ();
        if (single == NULL)
        {
                g_warning ("Failed to get singleton embed object!\n");
        }

        /* allow creation of orphan windows */
        g_signal_connect (G_OBJECT (single), "new_window_orphan",
                          G_CALLBACK (mozilla_embed_single_new_window_orphan_cb),
			  mes);
}

static void
mozilla_init_home (void)
{
	char *mozilla_five_home;
        mozilla_five_home = g_strdup (g_getenv ("MOZILLA_FIVE_HOME"));
        gtk_moz_embed_set_comp_path (mozilla_five_home);
        g_free (mozilla_five_home);
}

void
mozilla_init_profile (void)
{
	char *profile_path;
	profile_path = g_build_filename (ephy_dot_dir (), 
					 MOZILLA_PROFILE_DIR, 
					 NULL);
        gtk_moz_embed_set_profile_path (profile_path, MOZILLA_PROFILE_NAME);
        g_free (profile_path);
}

#if defined(MOZ_NSIXULCHROMEREGISTRY_SELECTSKIN) || defined(HAVE_CHROME_NSICHROMEREGISTRYSEA_H)
static nsresult
getUILang (nsAString& aUILang)
{
	nsresult rv;

	nsCOMPtr<nsILocaleService> localeService = do_GetService (NS_LOCALESERVICE_CONTRACTID);
	if (!localeService)
	{
		g_warning ("Could not get locale service!\n");
		return NS_ERROR_FAILURE;
	}

	rv = localeService->GetLocaleComponentForUserAgent (aUILang);

	if (NS_FAILED (rv))
	{
		g_warning ("Could not determine locale!\n");
		return NS_ERROR_FAILURE;
	}

	return NS_OK;
}
#endif

static nsresult
mozilla_init_chrome (void)
{
/* FIXME: can we just omit this on new-toolkit ? */
#if defined(MOZ_NSIXULCHROMEREGISTRY_SELECTSKIN) || defined(HAVE_CHROME_NSICHROMEREGISTRYSEA_H)
	nsresult rv;
	nsEmbedString uiLang;

#ifdef HAVE_CHROME_NSICHROMEREGISTRYSEA_H
	nsCOMPtr<nsIChromeRegistrySea> chromeRegistry = do_GetService (NS_CHROMEREGISTRY_CONTRACTID);
#else
	nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService (NS_CHROMEREGISTRY_CONTRACTID);
#endif
	NS_ENSURE_TRUE (chromeRegistry, NS_ERROR_FAILURE);

	// Set skin to 'classic' so we get native scrollbars.
	rv = chromeRegistry->SelectSkin (nsEmbedCString("classic/1.0"), PR_FALSE);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	// set locale
	rv = chromeRegistry->SetRuntimeProvider(PR_TRUE);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	rv = getUILang(uiLang);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsEmbedCString cUILang;
	NS_UTF16ToCString (uiLang, NS_CSTRING_ENCODING_UTF8, cUILang);

	return chromeRegistry->SelectLocale (cUILang, PR_FALSE);
#else
	return NS_OK;
#endif
}

static void
mozilla_init_observer (MozillaEmbedSingle *single)
{
	EphySingle *es;

	es = new EphySingle ();
	NS_ADDREF (single->priv->mSingleObserver = es);

	nsresult rv;
	rv = es->Init (EPHY_EMBED_SINGLE (single));
	if (NS_FAILED (rv))
	{
		g_warning ("Failed to initialise EphySingle!\n");
		return;
	}
}

#ifdef ENABLE_NETWORK_MANAGER

static void
network_state_cb (libnm_glib_ctx *context,
		  gpointer data)
{
	EphyEmbedSingle *single = EPHY_EMBED_SINGLE (data);
	libnm_glib_state state;

	state = libnm_glib_get_network_state (context);

	LOG ("Network state: %d\n", state);

	switch (state)
	{
		case LIBNM_NO_DBUS:
		case LIBNM_NO_NETWORKMANAGER:
		case LIBNM_INVALID_CONTEXT:
			/* do nothing */
			break;
		case LIBNM_NO_NETWORK_CONNECTION:
			ephy_embed_single_set_network_status (single, FALSE);
			break;
		case LIBNM_ACTIVE_NETWORK_CONNECTION:
			ephy_embed_single_set_network_status (single, TRUE);
			break;
	}
}

static void
mozilla_init_network_manager (MozillaEmbedSingle *single)
{
	MozillaEmbedSinglePrivate *priv = single->priv;

	priv->nm_context = libnm_glib_init ();
	if (priv->nm_context == NULL)
	{
		g_warning ("Could not initialise NetworkManager, connection status will not be managed!\n");
		return;
	}

	priv->nm_callback_id = libnm_glib_register_callback (priv->nm_context,
							     network_state_cb,
							     single, NULL);
	if (priv->nm_callback_id == 0)
	{
		libnm_glib_shutdown (priv->nm_context);
		priv->nm_context = NULL;

		g_warning ("Could not connect to NetworkManager, connection status will not be managed!\n");
		return;
	}
}

#endif /* ENABLE_NETWORK_MANAGER */

static gboolean
init_services (MozillaEmbedSingle *single)
{
	/* Pre initialization */
	mozilla_init_plugin_path ();
	mozilla_init_home ();
	mozilla_init_profile ();

	/* Set mozilla binary path */
	gtk_moz_embed_set_comp_path (MOZILLA_HOME);

#if defined(HAVE_MOZILLA_TOOLKIT) && defined(HAVE_GECKO_1_8)

	nsCOMPtr<nsIDirectoryServiceProvider> dp = new EphyDirectoryProvider ();
	if (!dp) return FALSE;

	gtk_moz_embed_set_directory_service_provider (dp);
#endif

	/* Fire up the beast */
	gtk_moz_embed_push_startup ();

	mozilla_register_components ();

	/* Until gtkmozembed does this itself */
	mozilla_init_chrome ();

	mozilla_init_single (single);

	if (!mozilla_set_default_prefs (single))
	{
		return FALSE;
	}

	/* FIXME: This should be removed when mozilla
	 * bugs 207000 and 207001 are fixed.
	 */
	mozilla_setup_colors (single);

	START_PROFILER ("Mozilla prefs notifiers")
	mozilla_notifiers_init ();
	STOP_PROFILER ("Mozilla prefs notifiers")

	mozilla_init_observer (single);

#ifdef ENABLE_NETWORK_MANAGER
	mozilla_init_network_manager (single);
#endif

	return TRUE;
}

static void
prepare_close_cb (EphyEmbedShell *shell)
{
	GValue value = { 0, };

	/* To avoid evil web sites posing an alert and thus inhibiting
	 * shutdown, we just turn off javascript! :)
	 */
	g_value_init (&value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&value, FALSE);
	mozilla_pref_set ("javascript.enabled", &value);
	g_value_unset (&value);
}

static void
mozilla_embed_single_init (MozillaEmbedSingle *mes)
{
 	mes->priv = MOZILLA_EMBED_SINGLE_GET_PRIVATE (mes);

	mes->priv->user_prefs =
		g_build_filename (ephy_dot_dir (), 
				  MOZILLA_PROFILE_DIR,
				  MOZILLA_PROFILE_NAME,
				  MOZILLA_PROFILE_FILE,
				  NULL);

	if (!init_services (mes))
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new
			(NULL,
                         GTK_DIALOG_MODAL,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_CLOSE,
                         _("Epiphany can't be used now. "
                         "Mozilla initialization failed."));
		gtk_dialog_run (GTK_DIALOG (dialog));

		exit (0);
	}

	g_signal_connect_object (embed_shell, "prepare-close",
				 G_CALLBACK (prepare_close_cb), mes,
				 (GConnectFlags) 0);
}

static void
mozilla_embed_single_dispose (GObject *object)
{
	MozillaEmbedSingle *single = MOZILLA_EMBED_SINGLE (object);
	MozillaEmbedSinglePrivate *priv = single->priv;

	if (priv->mSingleObserver)
	{
		priv->mSingleObserver->Detach ();
		NS_RELEASE (priv->mSingleObserver);
		priv->mSingleObserver = nsnull;
	}

#ifdef ENABLE_NETWORK_MANAGER
        if (priv->nm_context != NULL)
	{
		if (priv->nm_callback_id != 0)
		{
			libnm_glib_unregister_callback (priv->nm_context,
							priv->nm_callback_id);
			priv->nm_callback_id = 0;
		}
	
		libnm_glib_shutdown (priv->nm_context);
		priv->nm_context = NULL;
	}
#endif

	parent_class->dispose (object);
}

static void
mozilla_embed_single_finalize (GObject *object)
{
	MozillaEmbedSingle *mes = MOZILLA_EMBED_SINGLE (object);

	/* Destroy EphyEmbedSingle before because some
	 * services depend on xpcom */
	G_OBJECT_CLASS (parent_class)->finalize (object);

	mozilla_notifiers_shutdown ();

	gtk_moz_embed_pop_startup ();

	g_free (mes->priv->user_prefs);

	if (mes->priv->theme_window)
	{
		gtk_widget_destroy (mes->priv->theme_window);
	}
}

static void
impl_clear_cache (EphyEmbedSingle *shell)
{
	nsCOMPtr<nsICacheService> cacheService =
                        do_GetService (NS_CACHESERVICE_CONTRACTID);
	if (!cacheService) return;

	cacheService->EvictEntries (nsICache::STORE_ANYWHERE);
}

static void
impl_clear_auth_cache (EphyEmbedSingle *shell)
{
	nsCOMPtr<nsIHttpAuthManager> authManager =
			do_GetService (NS_HTTPAUTHMANAGER_CONTRACTID);
	if (!authManager) return;

	authManager->ClearAll();
}

static void
impl_set_network_status (EphyEmbedSingle *single,
			 gboolean online)
{
	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID);
	if (!io) return;

	io->SetOffline (!online);
}

static gboolean
impl_get_network_status (EphyEmbedSingle *esingle)
{
	MozillaEmbedSingle *single = MOZILLA_EMBED_SINGLE (esingle);
	MozillaEmbedSinglePrivate *priv = single->priv;

	NS_ENSURE_TRUE (priv->mSingleObserver, TRUE);

	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID);
	if (!io) return FALSE; /* no way to check the state, assume offline */

	PRBool isOffline;
	nsresult rv;
	rv = io->GetOffline(&isOffline);
	NS_ENSURE_SUCCESS (rv, FALSE);

	PRBool isOnline = !isOffline;
	PRBool reallyOnline = priv->mSingleObserver->IsOnline ();

	g_return_val_if_fail (reallyOnline == isOnline, TRUE);

	return !isOffline;
}

static GList *
impl_get_font_list (EphyEmbedSingle *shell,
		    const char *langGroup)
{
	nsresult rv;
	PRUint32 fontCount;
	PRUnichar **fontArray;
	GList *l = NULL;

	nsCOMPtr<nsIFontEnumerator> mozFontEnumerator;
	mozFontEnumerator = do_CreateInstance("@mozilla.org/gfx/fontenumerator;1");
	NS_ENSURE_TRUE (mozFontEnumerator, NULL);

	rv = mozFontEnumerator->EnumerateFonts (langGroup, nsnull,
					        &fontCount, &fontArray);
	NS_ENSURE_SUCCESS (rv, NULL);

	for (PRUint32 i = 0; i < fontCount; i++)
	{
		char *gFontString;

		nsEmbedCString tmp;
		NS_UTF16ToCString (nsEmbedString(fontArray[i]),
				   NS_CSTRING_ENCODING_UTF8, tmp);
		gFontString = g_strdup (tmp.get());
		l = g_list_prepend (l, gFontString);
		nsMemory::Free (fontArray[i]);
	}

	nsMemory::Free (fontArray);

	return g_list_reverse (l);
}

static GList *
impl_list_cookies (EphyCookieManager *manager)
{
	nsresult rv;
	GList *cookies = NULL;
	
	nsCOMPtr<nsICookieManager> cookieManager = 
			do_GetService (NS_COOKIEMANAGER_CONTRACTID);
	if (!cookieManager) return NULL;

	nsCOMPtr<nsISimpleEnumerator> cookieEnumerator;
	cookieManager->GetEnumerator (getter_AddRefs(cookieEnumerator));
	NS_ENSURE_TRUE (cookieEnumerator, NULL);

	PRBool enumResult;
	for (cookieEnumerator->HasMoreElements(&enumResult) ;
	     enumResult == PR_TRUE ;
	     cookieEnumerator->HasMoreElements(&enumResult))
	{
		nsCOMPtr<nsICookie> keks;
		rv = cookieEnumerator->GetNext (getter_AddRefs(keks));
		if (NS_FAILED (rv) || !keks) continue;

		EphyCookie *cookie = mozilla_cookie_to_ephy_cookie (keks);
		if (!cookie) continue;

		cookies = g_list_prepend (cookies, cookie);
	}       

	return cookies;
}

static void
impl_remove_cookie (EphyCookieManager *manager,
		    const EphyCookie *cookie)
{
	nsCOMPtr<nsICookieManager> cookieManager =
		do_GetService (NS_COOKIEMANAGER_CONTRACTID);
	if (!cookieManager) return;

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, );

	nsresult rv;
	nsEmbedCString host;
	rv = idnService->ConvertUTF8toACE (nsEmbedCString(cookie->domain), host);
	NS_ENSURE_SUCCESS (rv, );

	cookieManager->Remove (host,
			       nsEmbedCString(cookie->name),
			       nsEmbedCString(cookie->path),
			       PR_FALSE /* block */);
}

static void
impl_clear_cookies (EphyCookieManager *manager)
{
	nsCOMPtr<nsICookieManager> cookieManager =
		do_GetService (NS_COOKIEMANAGER_CONTRACTID);
	if (!cookieManager) return;

	cookieManager->RemoveAll ();
}
	
static GList *
impl_list_passwords (EphyPasswordManager *manager)
{
	GList *passwords = NULL;

#ifdef HAVE_NSIPASSWORD_H
	nsresult rv;

	nsCOMPtr<nsIPasswordManager> passwordManager =
			do_GetService (NS_PASSWORDMANAGER_CONTRACTID);
	if (!passwordManager) return NULL;

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, NULL);

	nsCOMPtr<nsISimpleEnumerator> passwordEnumerator;
	passwordManager->GetEnumerator (getter_AddRefs(passwordEnumerator));
	NS_ENSURE_TRUE (passwordEnumerator, NULL);

	PRBool enumResult;
	for (passwordEnumerator->HasMoreElements(&enumResult) ;
	     enumResult == PR_TRUE ;
	     passwordEnumerator->HasMoreElements(&enumResult))
	{
		nsCOMPtr<nsIPassword> nsPassword;
		passwordEnumerator->GetNext (getter_AddRefs(nsPassword));
		if (!nsPassword) continue;

		nsEmbedCString transfer;
		rv = nsPassword->GetHost (transfer);
		if (NS_FAILED (rv)) continue;

		nsEmbedCString host;
		idnService->ConvertACEtoUTF8 (transfer, host);

		nsEmbedString unicodeName;
		rv = nsPassword->GetUser (unicodeName);
		if (NS_FAILED (rv)) continue;

		nsEmbedCString userName;
		NS_UTF16ToCString (unicodeName,
				   NS_CSTRING_ENCODING_UTF8, userName);

		rv = nsPassword->GetPassword (unicodeName);
		if (NS_FAILED (rv)) continue;

		nsEmbedCString userPassword;
		NS_UTF16ToCString (unicodeName,
				   NS_CSTRING_ENCODING_UTF8, userPassword);

		EphyPasswordInfo *p = g_new0 (EphyPasswordInfo, 1);

		p->host = g_strdup (host.get());
		p->username = g_strdup (userName.get());
		p->password = g_strdup (userPassword.get());

		passwords = g_list_prepend (passwords, p);
	}
#endif

	return passwords;
}

static void
impl_remove_password (EphyPasswordManager *manager,
		      EphyPasswordInfo *info)
{
        nsCOMPtr<nsIPasswordManager> pm =
                        do_GetService (NS_PASSWORDMANAGER_CONTRACTID);
	if (!pm) return;

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, );

	nsresult rv;
	nsEmbedCString host;
	rv = idnService->ConvertUTF8toACE (nsEmbedCString(info->host), host);
	NS_ENSURE_SUCCESS (rv, );

	nsEmbedString userName;
	NS_CStringToUTF16 (nsEmbedCString(info->username),
			   NS_CSTRING_ENCODING_UTF8, userName);
	pm->RemoveUser (host, userName);
}

static void
impl_permission_manager_add (EphyPermissionManager *manager,
			     const char *host,
			     const char *type,
			     EphyPermission permission)
{
	/* can only set allow or deny */
	g_return_if_fail (permission != EPHY_PERMISSION_DEFAULT);
	g_return_if_fail (type != NULL && type[0] != '\0');

        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return;

	nsCOMPtr<nsIURI> uri;
        EphyUtils::NewURI(getter_AddRefs(uri), nsEmbedCString(host));
	if (!uri) return;

	gboolean allow = (permission == EPHY_PERMISSION_ALLOWED);

	pm->Add (uri, type,
		 allow ? (PRUint32) nsIPermissionManager::ALLOW_ACTION :
			 (PRUint32) nsIPermissionManager::DENY_ACTION);
}

static void
impl_permission_manager_remove (EphyPermissionManager *manager,
				const char *host,
				const char *type)
{
        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return;

	pm->Remove (nsEmbedCString (host), type);
}

static void
impl_permission_manager_clear (EphyPermissionManager *manager)
{
        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return;

	pm->RemoveAll ();
}

EphyPermission
impl_permission_manager_test (EphyPermissionManager *manager,
			      const char *host,
			      const char *type)
{
	g_return_val_if_fail (type != NULL && type[0] != '\0', EPHY_PERMISSION_DEFAULT);

        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return EPHY_PERMISSION_DEFAULT;

	nsCOMPtr<nsIURI> uri;
        EphyUtils::NewURI(getter_AddRefs(uri), nsEmbedCString (host));
        if (!uri) return EPHY_PERMISSION_DEFAULT;

	nsresult rv;
	PRUint32 action;
	rv = pm->TestPermission (uri, type, &action);
	NS_ENSURE_SUCCESS (rv, EPHY_PERMISSION_DEFAULT);

	EphyPermission permission;

	switch (action)
	{
		case nsIPermissionManager::ALLOW_ACTION:
			permission = EPHY_PERMISSION_ALLOWED;
			break;
		case nsIPermissionManager::DENY_ACTION:
			permission = EPHY_PERMISSION_DENIED;
			break;
		case nsIPermissionManager::UNKNOWN_ACTION:
		default:
			permission = EPHY_PERMISSION_DEFAULT;
			break;
	}

	return permission;
}

GList *
impl_permission_manager_list (EphyPermissionManager *manager,
			      const char *type)
{
	GList *list = NULL;

        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return NULL;

	nsCOMPtr<nsISimpleEnumerator> pe;
	pm->GetEnumerator(getter_AddRefs(pe));
	NS_ENSURE_TRUE (pe, NULL);
	
	PRBool hasMore;
	while (NS_SUCCEEDED (pe->HasMoreElements (&hasMore)) && hasMore)
	{
		nsCOMPtr<nsISupports> element;
		pe->GetNext (getter_AddRefs (element));
		
		nsCOMPtr<nsIPermission> perm (do_QueryInterface (element));
		if (!perm) continue;

		nsresult rv;
		nsEmbedCString str;
		rv = perm->GetType(str);
		if (NS_FAILED (rv)) continue;

		if (strcmp (str.get(), type) == 0)
		{
			EphyPermissionInfo *info = 
				mozilla_permission_to_ephy_permission (perm);

			if (info != NULL)
			{
				list = g_list_prepend (list, info);
			}
		}
	}

	return list;
}

static GtkWidget * 
impl_open_window (EphyEmbedSingle *single,
		  EphyEmbed *parent,
		  const char *address,
		  const char *name,
		  const char *features)
{
	nsCOMPtr<nsIDOMWindow> domWindow;

	if (parent)
	{
		EphyBrowser *browser;

		browser = (EphyBrowser *) _mozilla_embed_get_ephy_browser (MOZILLA_EMBED(parent));
		g_return_val_if_fail (browser != NULL, NULL);

		browser->GetDOMWindow (getter_AddRefs (domWindow));
	}

	nsCOMPtr<nsIWindowWatcher> wWatch(do_GetService ("@mozilla.org/embedcomp/window-watcher;1"));
	nsCOMPtr<nsIDOMWindow> newWindow;
	wWatch->OpenWindow (domWindow, address, name, features, nsnull,
			    getter_AddRefs (newWindow));

	return EphyUtils::FindEmbed (newWindow);
}

#ifdef ENABLE_CERTIFICATE_MANAGER

static gboolean
impl_remove_certificate (EphyCertificateManager *manager,
			 EphyX509Cert *cert)
{
	nsresult rv;

	nsCOMPtr<nsIX509CertDB> certDB;
	certDB = do_GetService (NS_X509CERTDB_CONTRACTID);
	if (!certDB) return FALSE;

        nsCOMPtr<nsIX509Cert> mozCert;
        rv = mozilla_x509_cert_get_mozilla_cert (MOZILLA_X509_CERT (cert),
						 getter_AddRefs (mozCert));
	if (NS_FAILED (rv)) return FALSE;

	rv = certDB->DeleteCertificate (mozCert);
	if (NS_FAILED (rv)) return FALSE;

        return TRUE;
}

#define NICK_DELIMITER PRUnichar('\001')

static GList *
retrieveCerts (PRUint32 type)
{
	nsresult rv;

	nsCOMPtr<nsIX509CertDB> certDB;
	certDB = do_GetService (NS_X509CERTDB_CONTRACTID);
	if (!certDB) return NULL;

	PRUint32 count;
	PRUnichar **certNameList = NULL;

	rv = certDB->FindCertNicknames (NULL, type, &count, &certNameList);
	if (NS_FAILED (rv)) return NULL;

	LOG("Certificates found: %i", count);

	GList *list = NULL;
	for (PRUint32 i = 0; i < count; i++)
	{
		/* HACK HACK, this is EVIL, the string for each cert is:
		     <DELIMITER>nicknameOrEmailAddress<DELIMITER>dbKey
		   So we need to chop off the dbKey to look it up in the database.
		   
		   https://bugzilla.mozilla.org/show_bug.cgi?id=214742
		*/
		nsEmbedCString full_string;
		NS_UTF16ToCString (nsEmbedString(certNameList[i]),
				   NS_CSTRING_ENCODING_UTF8, full_string);

		const char *key = full_string.get();
		char *pos = strrchr (key, NICK_DELIMITER);
		if (!pos) continue;

		nsCOMPtr<nsIX509Cert> mozilla_cert;
		rv = certDB->FindCertByDBKey (pos, NULL, getter_AddRefs (mozilla_cert));
		if (NS_FAILED (rv)) continue;

		MozillaX509Cert *cert = mozilla_x509_cert_new (mozilla_cert);
		list = g_list_prepend (list, cert);
	}

	NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY (count, certNameList);
	return list;
}

static GList *
impl_get_certificates (EphyCertificateManager *manager,
		       EphyX509CertType type)
{
	int moz_type = nsIX509Cert::USER_CERT;
	switch (type)
	{
		case PERSONAL_CERTIFICATE:
			moz_type = nsIX509Cert::USER_CERT;
			break;
		case SERVER_CERTIFICATE:
			moz_type = nsIX509Cert::SERVER_CERT;
			break;
		case CA_CERTIFICATE:
			moz_type = nsIX509Cert::CA_CERT;
			break;
	}
	return retrieveCerts (moz_type);
}

static gboolean
impl_import (EphyCertificateManager *manager,
	     const gchar *file)
{
	nsresult rv;
	nsCOMPtr<nsIX509CertDB> certDB;
	certDB = do_GetService (NS_X509CERTDB_CONTRACTID);
	if (!certDB) return FALSE;

	nsCOMPtr<nsILocalFile> localFile;
	localFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);

	// TODO Is this correct ?
	nsEmbedString path;
	NS_CStringToUTF16 (nsEmbedCString(file),
			   NS_CSTRING_ENCODING_UTF8, path);


	localFile->InitWithPath (path);
	rv = certDB->ImportPKCS12File(NULL, localFile);
	if (NS_FAILED (rv)) return FALSE;

	return TRUE;
}

#endif /* ENABLE_CERTIFICATE_MANAGER */

static void
mozilla_embed_single_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	EphyEmbedSingle *single = EPHY_EMBED_SINGLE (object);

	switch (prop_id)
	{
		case PROP_NETWORK_STATUS:
			g_value_set_boolean (value, ephy_embed_single_get_network_status (single));
			break;
	}
}

static void
mozilla_embed_single_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyEmbedSingle *single = EPHY_EMBED_SINGLE (object);

	switch (prop_id)
	{
		case PROP_NETWORK_STATUS:
			ephy_embed_single_set_network_status (single, g_value_get_boolean (value));
			break;
	}
}
static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->dispose = mozilla_embed_single_dispose;
	object_class->finalize = mozilla_embed_single_finalize;
	object_class->get_property = mozilla_embed_single_get_property;
	object_class->set_property = mozilla_embed_single_set_property;

	g_object_class_override_property (object_class, PROP_NETWORK_STATUS, "network-status");

	g_type_class_add_private (object_class, sizeof (MozillaEmbedSinglePrivate));
}

static void
ephy_embed_single_iface_init (EphyEmbedSingleIface *iface)
{
	iface->clear_cache = impl_clear_cache;
	iface->clear_auth_cache = impl_clear_auth_cache;
	iface->set_network_status = impl_set_network_status;
	iface->get_network_status = impl_get_network_status;
	iface->get_font_list = impl_get_font_list;
	iface->open_window = impl_open_window;
}

static void
ephy_cookie_manager_iface_init (EphyCookieManagerIface *iface)
{
	iface->list = impl_list_cookies;
	iface->remove = impl_remove_cookie;
	iface->clear = impl_clear_cookies;
}

static void
ephy_password_manager_iface_init (EphyPasswordManagerIface *iface)
{
	iface->add = NULL; /* not implemented yet */
	iface->remove = impl_remove_password;
	iface->list = impl_list_passwords;
}

static void
ephy_permission_manager_iface_init (EphyPermissionManagerIface *iface)
{
	iface->add = impl_permission_manager_add;
	iface->remove = impl_permission_manager_remove;
	iface->clear = impl_permission_manager_clear;
	iface->test = impl_permission_manager_test;
	iface->list = impl_permission_manager_list;
}

#ifdef ENABLE_CERTIFICATE_MANAGER

static void
ephy_certificate_manager_iface_init (EphyCertificateManagerIface *iface)
{
	iface->get_certificates = impl_get_certificates;
	iface->remove_certificate = impl_remove_certificate;
	iface->import = impl_import;
}

#endif /* ENABLE_CERTIFICATE_MANAGER */
