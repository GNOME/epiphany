/*  vim:set ts=8 noet sw=8:
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003 Robert Marcano
 *  Copyright © 2003, 2004, 2005, 2006 Christian Persch
 *  Copyright © 2005 Crispin Flowerday
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

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <nsStringAPI.h>

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include <nsComponentManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsCPasswordManager.h>
#include <nsICookie2.h>
#include <nsICookieManager.h>
#include <nsICookieManager.h>
#include <nsIFile.h>
#include <nsIIOService.h>
#include <nsILocalFile.h>
#include <nsIPassword.h>
#include <nsIPasswordManager.h>
#include <nsIPermission.h>
#include <nsIPermissionManager.h>
#include <nsIPrefService.h>
#include <nsIStyleSheetService.h>
#include <nsISupportsPrimitives.h>
#include <nsIURI.h>
#include <nsIWindowWatcher.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>

#ifdef HAVE_MOZILLA_PSM
#include <nsIX509Cert.h>
#include <nsIX509CertDB.h>
#endif

#ifdef ALLOW_PRIVATE_API
#include <nsICacheService.h>
#include <nsIFontEnumerator.h>
#include <nsIHttpAuthManager.h>
#include <nsIIDNService.h>
#include <nsNetCID.h>
#endif /* ALLOW_PRIVATE_API */

#include "ephy-file-helpers.h"
#include "eel-gconf-extensions.h"
#include "ephy-certificate-manager.h"
#include "ephy-cookie-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-langs.h"
#include "ephy-password-manager.h"
#include "ephy-permission-manager.h"
#include "mozilla-embed.h"
#include "mozilla-notifiers.h"
#include "mozilla-x509-cert.h"

#include "EphyBrowser.h"
#include "EphyDirectoryProvider.h"
#include "EphySingle.h"
#include "EphyUtils.h"
#include "MozRegisterComponents.h"

#include "mozilla-embed-single.h"

#include "AutoJSContextStack.h"

#define MOZILLA_PROFILE_DIR  "/mozilla"
#define MOZILLA_PROFILE_NAME "epiphany"
#define MOZILLA_PROFILE_FILE "prefs.js"
#define DEFAULT_PROFILE_FILE SHARE_DIR"/default-prefs.js"

#define USER_CSS_LOAD_DELAY 500 /* ms */

#define MOZILLA_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSinglePrivate))

struct MozillaEmbedSinglePrivate
{
	char *user_prefs;
	
	EphySingle *mSingleObserver;

	char *user_css_file;
        guint user_css_enabled_notifier_id;
        EphyFileMonitor *user_css_file_monitor;
        guint user_css_enabled : 1;

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
                const GTypeInfo our_info =
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

		const GInterfaceInfo embed_single_info =
		{
			(GInterfaceInitFunc) ephy_embed_single_iface_init,
			NULL,
			NULL
		};

		const GInterfaceInfo cookie_manager_info =
		{
			(GInterfaceInitFunc) ephy_cookie_manager_iface_init,
			NULL,
			NULL
		};

		const GInterfaceInfo password_manager_info =
		{
			(GInterfaceInitFunc) ephy_password_manager_iface_init,
			NULL,
			NULL
		};

		const GInterfaceInfo permission_manager_info =
		{
			(GInterfaceInitFunc) ephy_permission_manager_iface_init,
			NULL,
			NULL
		};

#ifdef ENABLE_CERTIFICATE_MANAGER
		const GInterfaceInfo certificate_manager_info =
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
        NS_NewNativeLocalFile(nsCString(DEFAULT_PROFILE_FILE),
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

	pref->SetCharPref ("general.useragent.extra.epiphany", "Epiphany/" UA_VERSION);

	/* Unset old prefs, otherwise they end up in the user agent string too */
	pref->ClearUserPref ("general.useragent.vendor");
	pref->ClearUserPref ("general.useragent.vendorSub");

	/* Don't open ftp uris with an external handler if one is setup */
	pref->SetBoolPref ("network.protocol-handler.external.ftp", PR_FALSE);	

	return TRUE;
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
				(char *) NULL);

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

void
mozilla_init_profile (void)
{
	char *profile_path;
	profile_path = g_build_filename (ephy_dot_dir (), 
					 MOZILLA_PROFILE_DIR,
					 (char *) NULL);
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

static void
user_css_register (MozillaEmbedSingle *single)
{
	MozillaEmbedSinglePrivate *priv = single->priv;

	nsresult rv;
	nsCOMPtr<nsILocalFile> file;
	rv = NS_NewNativeLocalFile (nsDependentCString (priv->user_css_file),
				    PR_TRUE, getter_AddRefs (file));
	NS_ENSURE_SUCCESS (rv, );

	PRBool exists = PR_FALSE;
	rv = file->Exists (&exists);
	if (NS_FAILED (rv) || !exists) return;

	nsCOMPtr<nsIURI> uri;
	rv = EphyUtils::NewFileURI (getter_AddRefs (uri), file);
	NS_ENSURE_SUCCESS (rv, );

	nsCOMPtr<nsIStyleSheetService> service
			(do_GetService ("@mozilla.org/content/style-sheet-service;1", &rv));
	NS_ENSURE_SUCCESS (rv, );

	PRBool isRegistered = PR_FALSE;
	rv = service->SheetRegistered (uri, nsIStyleSheetService::USER_SHEET,
				       &isRegistered);
	if (NS_SUCCEEDED (rv) && isRegistered)
	{
		rv = service->UnregisterSheet (uri, nsIStyleSheetService::USER_SHEET);
	}

	rv = service->LoadAndRegisterSheet (uri, nsIStyleSheetService::AGENT_SHEET);
	if (NS_FAILED (rv))
	{
		g_warning ("Registering the user stylesheet failed (rv=%x)!\n", rv);
	}
}

static void
user_css_unregister (MozillaEmbedSingle *single)
{
	MozillaEmbedSinglePrivate *priv = single->priv;

	nsresult rv;
	nsCOMPtr<nsILocalFile> file;
	rv = NS_NewNativeLocalFile (nsDependentCString (priv->user_css_file),
				    PR_TRUE, getter_AddRefs (file));
	NS_ENSURE_SUCCESS (rv, );

	nsCOMPtr<nsIURI> uri;
	rv = EphyUtils::NewFileURI (getter_AddRefs (uri), file);
	NS_ENSURE_SUCCESS (rv, );

	nsCOMPtr<nsIStyleSheetService> service
			(do_GetService ("@mozilla.org/content/style-sheet-service;1", &rv));
	NS_ENSURE_SUCCESS (rv, );

	PRBool isRegistered = PR_FALSE;
	rv = service->SheetRegistered (uri, nsIStyleSheetService::USER_SHEET,
				       &isRegistered);
	if (NS_SUCCEEDED (rv) && isRegistered)
	{
		rv = service->UnregisterSheet (uri, nsIStyleSheetService::USER_SHEET);
	}
	if (NS_FAILED (rv))
	{
		g_warning ("Unregistering the user stylesheet failed (rv=%x)!\n", rv);
	}
}

static void
user_css_file_monitor_func (EphyFileMonitor *,
			    const char *,
			    GnomeVFSMonitorEventType event_type,
			    MozillaEmbedSingle *single)
{
	LOG ("Reregistering the user style sheet");

	if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
	{
		user_css_unregister (single);
	}
	else
	{
		user_css_register (single);
	}
}

static void
user_css_enabled_notify (GConfClient *client,
			 guint cnxn_id,
			 GConfEntry *entry,
			 MozillaEmbedSingle *single)
{
	MozillaEmbedSinglePrivate *priv = single->priv;
	guint enabled;

	enabled = eel_gconf_get_boolean (CONF_USER_CSS_ENABLED) != FALSE;
	if (priv->user_css_enabled == enabled) return;

	LOG ("User stylesheet enabled: %s", enabled ? "t" : "f");

	priv->user_css_enabled = enabled;

        if (enabled)
	{
		char *uri;

		user_css_register (single);

		uri = gnome_vfs_get_uri_from_local_path (priv->user_css_file);

		g_assert (priv->user_css_file_monitor == NULL);
		priv->user_css_file_monitor =
			ephy_file_monitor_add (uri,
					       GNOME_VFS_MONITOR_FILE,
					       USER_CSS_LOAD_DELAY,
					       (EphyFileMonitorFunc) user_css_file_monitor_func,
					       NULL,
					       single);
		g_free (uri);
	}
        else
	{
		if (priv->user_css_file_monitor != NULL)
		{
			ephy_file_monitor_cancel (priv->user_css_file_monitor);
			priv->user_css_file_monitor = NULL;
		}

		user_css_unregister (single);
	}
}

static void
mozilla_stylesheet_init (MozillaEmbedSingle *single)
{
	MozillaEmbedSinglePrivate *priv = single->priv;

	priv->user_css_file = g_build_filename (ephy_dot_dir (),
						USER_STYLESHEET_FILENAME,
						(char *) NULL);

	user_css_enabled_notify (NULL, 0, NULL, single);
	priv->user_css_enabled_notifier_id =
		eel_gconf_notification_add
			(CONF_USER_CSS_ENABLED,
			 (GConfClientNotifyFunc) user_css_enabled_notify,
			 single);
}

static void
mozilla_stylesheet_shutdown (MozillaEmbedSingle *single)
{
	MozillaEmbedSinglePrivate *priv = single->priv;

	if (priv->user_css_enabled_notifier_id != 0)
	{
		eel_gconf_notification_remove (priv->user_css_enabled_notifier_id);
		priv->user_css_enabled_notifier_id = 0;
	}

	if (priv->user_css_file_monitor != NULL)
	{
		ephy_file_monitor_cancel (priv->user_css_file_monitor);
		priv->user_css_file_monitor = NULL;
	}

	if (priv->user_css_file != NULL)
	{
		g_free (priv->user_css_file);
		priv->user_css_file = NULL;
	}
}

static gboolean
impl_init (EphyEmbedSingle *esingle)
{
	MozillaEmbedSingle *single = MOZILLA_EMBED_SINGLE (esingle);

#ifdef MOZ_ENABLE_XPRINT
	/* XPrint? No, thanks! */
	g_unsetenv ("XPSERVERLIST");
#endif

#ifdef HAVE_GECKO_1_9
	NS_LogInit ();
#endif

	/* Pre initialization */
	mozilla_init_plugin_path ();

	mozilla_init_profile ();

#ifdef HAVE_GECKO_1_9
	gtk_moz_embed_set_path (MOZILLA_HOME);
#else
	/* Set mozilla binary path */
	gtk_moz_embed_set_comp_path (MOZILLA_HOME);
#endif

	nsCOMPtr<nsIDirectoryServiceProvider> dp = new EphyDirectoryProvider ();
	if (!dp) return FALSE;

	gtk_moz_embed_set_directory_service_provider (dp);

	/* Fire up the beast */
	gtk_moz_embed_push_startup ();
	/* FIXME check that it succeeded! */

	mozilla_register_components ();

	mozilla_init_single (single);

	if (!mozilla_set_default_prefs (single))
	{
		return FALSE;
	}

	START_PROFILER ("Mozilla prefs notifiers")
	mozilla_notifiers_init ();
	STOP_PROFILER ("Mozilla prefs notifiers")

	mozilla_init_observer (single);

        mozilla_stylesheet_init (single);

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
				  (char *) NULL);

	g_signal_connect_object (embed_shell, "prepare-close",
				 G_CALLBACK (prepare_close_cb), mes,
				 (GConnectFlags) 0);
}

static void
mozilla_embed_single_dispose (GObject *object)
{
	MozillaEmbedSingle *single = MOZILLA_EMBED_SINGLE (object);
	MozillaEmbedSinglePrivate *priv = single->priv;

        mozilla_stylesheet_shutdown (single);

	if (priv->mSingleObserver)
	{
		priv->mSingleObserver->Detach ();
		NS_RELEASE (priv->mSingleObserver);
		priv->mSingleObserver = nsnull;
	}

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

#ifdef HAVE_GECKO_1_9
	NS_LogTerm ();
#endif

	g_free (mes->priv->user_prefs);
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

static const char*
impl_get_backend_name (EphyEmbedSingle *esingle)
{
	/* If you alter the return values here, remember to update
	 * the docs in ephy-embed-single.c */
#if defined (HAVE_GECKO_1_10)
# error "Need to add version string for gecko 1.10"
#elif defined(HAVE_GECKO_1_9)
	return "gecko-1.9";
#elif defined(HAVE_GECKO_1_8)
	return "gecko-1.8";
#else
# error "Undefined/unsupported gecko version!"
#endif
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

		nsCString tmp;
		NS_UTF16ToCString (nsString(fontArray[i]),
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
	nsCString host;
	rv = idnService->ConvertUTF8toACE (nsCString(cookie->domain), host);
	NS_ENSURE_SUCCESS (rv, );

	cookieManager->Remove (host,
			       nsCString(cookie->name),
			       nsCString(cookie->path),
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

		nsCString transfer;
		rv = nsPassword->GetHost (transfer);
		if (NS_FAILED (rv)) continue;

		nsCString host;
		idnService->ConvertACEtoUTF8 (transfer, host);

		nsString unicodeName;
		rv = nsPassword->GetUser (unicodeName);
		if (NS_FAILED (rv)) continue;

		nsCString userName;
		NS_UTF16ToCString (unicodeName,
				   NS_CSTRING_ENCODING_UTF8, userName);

		rv = nsPassword->GetPassword (unicodeName);
		if (NS_FAILED (rv)) continue;

		nsCString userPassword;
		NS_UTF16ToCString (unicodeName,
				   NS_CSTRING_ENCODING_UTF8, userPassword);

		EphyPasswordInfo *p = g_new0 (EphyPasswordInfo, 1);

		p->host = g_strdup (host.get());
		p->username = g_strdup (userName.get());
		p->password = g_strdup (userPassword.get());

		passwords = g_list_prepend (passwords, p);
	}

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
	nsCString host;
	rv = idnService->ConvertUTF8toACE (nsCString(info->host), host);
	NS_ENSURE_SUCCESS (rv, );

	nsString userName;
	NS_CStringToUTF16 (nsCString(info->username),
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
        EphyUtils::NewURI(getter_AddRefs(uri), nsCString(host));
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

	pm->Remove (nsCString (host), type);
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
        EphyUtils::NewURI(getter_AddRefs(uri), nsCString (host));
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
		nsCString str;
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
	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return NULL;

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
		nsCString full_string;
		NS_UTF16ToCString (nsString(certNameList[i]),
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
	nsString path;
	NS_CStringToUTF16 (nsCString(file),
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
	iface->init = impl_init;
	iface->clear_cache = impl_clear_cache;
	iface->clear_auth_cache = impl_clear_auth_cache;
	iface->set_network_status = impl_set_network_status;
	iface->get_network_status = impl_get_network_status;
	iface->get_font_list = impl_get_font_list;
	iface->open_window = impl_open_window;
	iface->get_backend_name = impl_get_backend_name;
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
