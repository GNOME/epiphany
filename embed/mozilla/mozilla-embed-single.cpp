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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"

#include "config.h"

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <nsStringAPI.h>

#ifdef XPCOM_GLUE
#include <nsXPCOMGlue.h>
#include <gtkmozembed_glue.cpp>
#endif

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
#include <nsIPermission.h>
#include <nsIPermissionManager.h>
#include <nsIPrefService.h>
#include <nsIStyleSheetService.h>
#include <nsISupportsPrimitives.h>
#include <nsIURI.h>
#include <nsIWindowWatcher.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>

#ifdef ALLOW_PRIVATE_API
#include <nsICacheService.h>
#include <nsIFontEnumerator.h>
#include <nsIHttpAuthManager.h>
#include <nsIIDNService.h>
#include <nsNetCID.h>
#endif /* ALLOW_PRIVATE_API */

#ifdef HAVE_GECKO_1_9
#include <nsILoginInfo.h>
#include <nsILoginManager.h>
#else
#include <nsIPassword.h>
#include <nsIPasswordManager.h>
#endif /* !HAVE_GECKO_1_9 */

#include "ephy-file-helpers.h"
#include "eel-gconf-extensions.h"
#include "ephy-cookie-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-langs.h"
#include "ephy-password-manager.h"
#include "ephy-permission-manager.h"
#include "ephy-string.h"
#include "mozilla-embed.h"
#include "mozilla-notifiers.h"

#include "EphyBrowser.h"
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

#ifndef HAVE_GECKO_1_9
#include "EphyDirectoryProvider.h"
#endif /* HAVE_GECKO_1_9 */

struct MozillaEmbedSinglePrivate
{
	char *user_prefs;
	
	EphySingle *mSingleObserver;

	char *user_css_file;
        guint user_css_enabled_notifier_id;
        GFileMonitor *user_css_file_monitor;
        guint user_css_enabled : 1;
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

G_DEFINE_TYPE_WITH_CODE (MozillaEmbedSingle, mozilla_embed_single, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_SINGLE,
                                                ephy_embed_single_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_COOKIE_MANAGER,
                                                ephy_cookie_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PASSWORD_MANAGER,
                                                ephy_password_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_PERMISSION_MANAGER,
                                                ephy_permission_manager_iface_init))

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

static GList *
mozilla_init_plugin_add_unique_path (GList *list,
				     const char *path)
{
	GList *l;
	char *canon;

	if (path == NULL)
		return list;

	canon = ephy_string_canonicalize_pathname (path);

	for (l = list; l != NULL; l = l->next) {
		if (g_str_equal (list->data, canon) != FALSE) {
			/* The path is already in the list */
			g_free (canon);
			return list;
		}
	}
	return g_list_prepend (list, canon);
}

static GList *
mozilla_init_plugin_add_unique_paths (GList *list,
				      const char *path)
{
	char **paths;
	guint i;

	if (path == NULL)
		return list;

	paths = g_strsplit (path, ":", -1);
	if (paths == NULL)
		return list;
	for (i = 0; paths[i] != NULL; i++) {
		list = mozilla_init_plugin_add_unique_path (list, paths[i]);
	}
	g_strfreev (paths);
	return list;
}

static void
mozilla_init_plugin_path ()
{
	GList *list, *l;
	GString *path;

	list = NULL;
	list = mozilla_init_plugin_add_unique_paths (list,
						     g_getenv ("MOZ_PLUGIN_PATH"));
	list = mozilla_init_plugin_add_unique_path (list,
						    MOZILLA_PREFIX "/lib/mozilla/plugins");
	list = mozilla_init_plugin_add_unique_path (list,
						    MOZILLA_HOME "/plugins");
	list = mozilla_init_plugin_add_unique_path (list,
						    MOZILLA_NATIVE_PLUGINSDIR);
#ifdef HAVE_PRIVATE_PLUGINS
	list = mozilla_init_plugin_add_unique_path (list, PLUGINDIR);
#endif

	list = g_list_reverse (list);
	path = g_string_new ((const char *) list->data);
	g_free (list->data);
	l = list->next;
	for (; l != NULL; l = l->next) {
		path = g_string_append_c (path, ':');
		path = g_string_append (path, (const char *) l->data);
		g_free (l->data);
	}
	g_list_free (list);

	g_setenv ("MOZ_PLUGIN_PATH", path->str, TRUE);
	g_string_free (path, TRUE);
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
mozilla_init_login_manager (MozillaEmbedSingle *single)
{
#ifdef HAVE_GECKO_1_9
        nsCOMPtr<nsILoginManager> loginManager =
                        do_GetService (NS_LOGINMANAGER_CONTRACTID);
	if (!loginManager)
		g_warning ("Failed to instantiate LoginManager");
#endif /* HAVE_GECKO_1_9 */
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
user_css_file_monitor_changed_cb (GFileMonitor *file_monitor,
				  GFile *file,
				  GFile *other_file,
				  gint event_type,
				  MozillaEmbedSingle *single)
{
	LOG ("Reregistering the user style sheet");

	if (event_type == G_FILE_MONITOR_EVENT_DELETED)
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
		GFile *file;

		user_css_register (single);

		file = g_file_new_for_path (priv->user_css_file);

		g_assert (priv->user_css_file_monitor == NULL);
		priv->user_css_file_monitor =
			g_file_monitor_file (file,
					     G_FILE_MONITOR_NONE,
					     NULL, NULL);
		g_file_monitor_set_rate_limit (priv->user_css_file_monitor,
					       USER_CSS_LOAD_DELAY);
		g_signal_connect (priv->user_css_file_monitor, "changed",
				  G_CALLBACK (user_css_file_monitor_changed_cb),
				  single);
		g_object_unref (file);
	}
        else
	{
		if (priv->user_css_file_monitor != NULL)
		{
			g_file_monitor_cancel (priv->user_css_file_monitor);
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
		g_file_monitor_cancel (priv->user_css_file_monitor);
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

	g_setenv ("MOZILLA_POSTSCRIPT_ENABLED", "1", TRUE);
	g_unsetenv ("MOZILLA_POSTSCRIPT_PRINTER_LIST");

#ifdef MOZ_ENABLE_XPRINT
	/* XPrint? No, thanks! */
	g_unsetenv ("XPSERVERLIST");
#endif

#ifdef HAVE_GECKO_1_9
	NS_LogInit ();
#endif

	nsresult rv;
#ifdef XPCOM_GLUE
	static const GREVersionRange greVersion = {
	  "1.9a", PR_TRUE,
	  "2", PR_TRUE
	};
	char xpcomLocation[4096];
	rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0, xpcomLocation, sizeof (xpcomLocation));
	if (NS_FAILED (rv))
	{
	  g_warning ("Could not find a suitable GRE!\n");
	  return FALSE;
	}

	// Startup the XPCOM Glue that links us up with XPCOM.
	rv = XPCOMGlueStartup(xpcomLocation);
	if (NS_FAILED (rv))
	{
	  g_warning ("Could not startup XPCOM glue!\n");
	  return FALSE;
	}

	rv = GTKEmbedGlueStartup();
	if (NS_FAILED (rv))
	{
	  g_warning ("Could not startup embed glue!\n");
	  return FALSE;
	}

	rv = GTKEmbedGlueStartupInternal();
	if (NS_FAILED (rv))
	{
	  g_warning ("Could not startup internal glue!\n");
	  return FALSE;
	}

	char *lastSlash = strrchr(xpcomLocation, '/');
	if (lastSlash)
	  *lastSlash = '\0';

	gtk_moz_embed_set_path(xpcomLocation);
	gtk_moz_embed_set_comp_path (SHARE_DIR);
#else
#ifdef HAVE_GECKO_1_9
        gtk_moz_embed_set_path (MOZILLA_HOME);
	gtk_moz_embed_set_comp_path (SHARE_DIR);
#else
        gtk_moz_embed_set_comp_path (MOZILLA_HOME);
#endif
#endif // XPCOM_GLUE

	/* Pre initialization */
	mozilla_init_plugin_path ();

	mozilla_init_profile ();

#ifndef HAVE_GECKO_1_9
	nsCOMPtr<nsIDirectoryServiceProvider> dp = new EphyDirectoryProvider ();
	if (!dp) return FALSE;

	gtk_moz_embed_set_directory_service_provider (dp);
#endif

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

	mozilla_init_login_manager (single);

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

	G_OBJECT_CLASS (mozilla_embed_single_parent_class)->dispose (object);
}

static void
mozilla_embed_single_finalize (GObject *object)
{
	MozillaEmbedSingle *mes = MOZILLA_EMBED_SINGLE (object);

	/* Destroy EphyEmbedSingle before because some
	 * services depend on xpcom */
	G_OBJECT_CLASS (mozilla_embed_single_parent_class)->finalize (object);

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

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, NULL);

#ifdef HAVE_GECKO_1_9
	nsILoginInfo **logins = nsnull;
	PRUint32 count,i;
	nsresult rv;

	nsCOMPtr<nsILoginManager> loginManager =
			do_GetService (NS_LOGINMANAGER_CONTRACTID);
	NS_ENSURE_TRUE (loginManager, NULL);

	loginManager -> GetAllLogins(&count, &logins);

	for (i=0; i < count; i++) {
		nsString transfer;
		nsString unicodeName;
		rv = logins[i]->GetHostname (transfer);
		if (NS_FAILED (rv)) continue;

		nsCString host;
		if (transfer.IsVoid())
                  host.SetIsVoid(PR_TRUE);
                else
                  idnService->ConvertACEtoUTF8 (NS_ConvertUTF16toUTF8(transfer), host);

		rv = logins[i]->GetHttpRealm (unicodeName);
		if (NS_FAILED (rv)) continue;
		nsCString httpRealm;
		if (unicodeName.IsVoid())
                  httpRealm.SetIsVoid(PR_TRUE);
                else
                  NS_UTF16ToCString (unicodeName,
                                    NS_CSTRING_ENCODING_UTF8, httpRealm);

		rv = logins[i]->GetUsername (unicodeName);
		if (NS_FAILED (rv)) continue;
		nsCString userName;
		if (unicodeName.IsVoid())
                  userName.SetIsVoid(PR_TRUE);
                else
                  NS_UTF16ToCString (unicodeName,
                                    NS_CSTRING_ENCODING_UTF8, userName);

		rv = logins[i]->GetUsernameField (unicodeName);
		if (NS_FAILED (rv)) continue;
		nsCString usernameField;
		if (unicodeName.IsVoid())
                  usernameField.SetIsVoid(PR_TRUE);
                else
                  NS_UTF16ToCString (unicodeName,
                                    NS_CSTRING_ENCODING_UTF8, usernameField);

		rv = logins[i]->GetPassword (unicodeName);
		if (NS_FAILED (rv)) continue;
		nsCString userPassword;
		if (unicodeName.IsVoid())
                  userPassword.SetIsVoid(PR_TRUE);
                else
                  NS_UTF16ToCString (unicodeName,
                                    NS_CSTRING_ENCODING_UTF8, userPassword);

		rv = logins[i]->GetPasswordField (unicodeName);
		if (NS_FAILED (rv)) continue;
		nsCString passwordField;
		if (unicodeName.IsVoid())
                  passwordField.SetIsVoid(PR_TRUE);
                else
                  NS_UTF16ToCString (unicodeName,
                                    NS_CSTRING_ENCODING_UTF8, passwordField);

		rv = logins[i]->GetFormSubmitURL (unicodeName);
		if (NS_FAILED (rv)) continue;
		nsCString formSubmitURL;
		if (unicodeName.IsVoid())
                  formSubmitURL.SetIsVoid(PR_TRUE);
                else
                  NS_UTF16ToCString (unicodeName,
                                    NS_CSTRING_ENCODING_UTF8, formSubmitURL);

		EphyPasswordInfo *p = ephy_password_info_new (host.IsVoid() ? NULL : host.get(),
                                                              userName.IsVoid() ? NULL : userName.get(),
                                                              userPassword.IsVoid() ? NULL : userPassword.get());
		p->httpRealm = httpRealm.IsVoid() ? NULL : g_strdup(httpRealm.get());
		p->usernameField = usernameField.IsVoid() ? NULL : g_strdup(usernameField.get());
		p->passwordField = passwordField.IsVoid() ? NULL : g_strdup(passwordField.get());
		p->formSubmitURL = formSubmitURL.IsVoid() ? NULL : g_strdup(formSubmitURL.get());

		passwords = g_list_prepend (passwords, p);
	}

	NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY (count, logins);

#else // HAVE_GECKO_1_9

	nsresult rv;
	nsCOMPtr<nsIPasswordManager> passwordManager =
			do_GetService (NS_PASSWORDMANAGER_CONTRACTID);
	if (!passwordManager) return NULL;

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
#endif /* !HAVE_GECKO_1_9 */

	return passwords;
}

static void
impl_remove_password (EphyPasswordManager *manager,
		      EphyPasswordInfo *info)
{
	nsString userName;

        if (info->username)
          NS_CStringToUTF16 (nsCString(info->username),
                            NS_CSTRING_ENCODING_UTF8, userName);
#ifdef HAVE_GECKO_1_9
        else
          userName.SetIsVoid (PR_TRUE);

	nsString host;
	nsString userNameField;
	nsString password;
	nsString passwordField;
	nsString httpRealm;
	nsString formSubmitURL;

        if (info->host)
          NS_CStringToUTF16 (nsCString(info->host),
                            NS_CSTRING_ENCODING_UTF8, userName);
        else
          host.SetIsVoid (PR_TRUE);

        if (info->usernameField)
          NS_CStringToUTF16 (nsCString(info->usernameField),
                            NS_CSTRING_ENCODING_UTF8, userNameField);
        else
          userNameField.SetIsVoid (PR_TRUE);

        if (info->httpRealm)
          NS_CStringToUTF16 (nsCString(info->httpRealm),
                            NS_CSTRING_ENCODING_UTF8, httpRealm);
        else
          userName.SetIsVoid (PR_TRUE);

        if (info->password)
          NS_CStringToUTF16 (nsCString(info->password),
                            NS_CSTRING_ENCODING_UTF8, password);
        else
          password.SetIsVoid (PR_TRUE);

        if (info->passwordField)
          NS_CStringToUTF16 (nsCString(info->passwordField),
                            NS_CSTRING_ENCODING_UTF8, passwordField);
        else
          passwordField.SetIsVoid (PR_TRUE);

        if (info->formSubmitURL)
          NS_CStringToUTF16 (nsCString(info->formSubmitURL),
                            NS_CSTRING_ENCODING_UTF8, formSubmitURL);
        else
          formSubmitURL.SetIsVoid (PR_TRUE);

        nsCOMPtr<nsILoginManager> loginManager =
			do_GetService (NS_LOGINMANAGER_CONTRACTID);
	NS_ENSURE_TRUE (loginManager, );

	nsCOMPtr<nsILoginInfo> login
		(do_CreateInstance(NS_LOGININFO_CONTRACTID));

	login->SetUsername(userName);
	login->SetUsernameField(userNameField);
	login->SetHostname(host);
	login->SetHttpRealm(httpRealm);
	login->SetFormSubmitURL(formSubmitURL);
	login->SetPassword(password);
	login->SetPasswordField(passwordField);

	loginManager->RemoveLogin(login);

#else /* !HAVE_GECKO_1_9 */
        if (!info->host)
          return;

        nsCOMPtr<nsIPasswordManager> pm =
                        do_GetService (NS_PASSWORDMANAGER_CONTRACTID);
	if (!pm) return;

        nsCString host (info->host);
	pm->RemoveUser (host, userName);
#endif /* HAVE_GECKO_1_9 */
}

static void
impl_remove_all_passwords (EphyPasswordManager *manager)
{
#ifdef HAVE_GECKO_1_9
	nsCOMPtr<nsILoginManager> loginManager =
			do_GetService (NS_LOGINMANAGER_CONTRACTID);
	NS_ENSURE_TRUE (loginManager, );

	loginManager->RemoveAllLogins();

#else /* HAVE_GECKO_1_9 */
	nsresult rv;
	nsCOMPtr<nsIPasswordManager> passwordManager =
			do_GetService (NS_PASSWORDMANAGER_CONTRACTID);
	if (!passwordManager) return;

	nsCOMPtr<nsIIDNService> idnService
		(do_GetService ("@mozilla.org/network/idn-service;1"));
	NS_ENSURE_TRUE (idnService, );

	nsCOMPtr<nsISimpleEnumerator> passwordEnumerator;
	passwordManager->GetEnumerator (getter_AddRefs(passwordEnumerator));
	NS_ENSURE_TRUE (passwordEnumerator, );

	PRBool enumResult;
	for (passwordEnumerator->HasMoreElements(&enumResult) ;
	     enumResult == PR_TRUE ;
	     passwordEnumerator->HasMoreElements(&enumResult))
	{
		nsCOMPtr<nsIPassword> nsPassword;
		passwordEnumerator->GetNext (getter_AddRefs(nsPassword));
		if (!nsPassword) continue;

		nsCString host;
		rv = nsPassword->GetHost (host);
		if (NS_FAILED (rv)) continue;

		nsString userName;
		rv = nsPassword->GetUser (userName);
		if (NS_FAILED (rv)) continue;

		passwordManager->RemoveUser (host, userName);
	}
#endif /* !HAVE_GECKO_1_9 */
}

static void
impl_add_password (EphyPasswordManager *manager,
                  EphyPasswordInfo *info)
{
#ifndef HAVE_GECKO_1_9
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

       nsString username;
       NS_CStringToUTF16 (nsCString(info->username),
                          NS_CSTRING_ENCODING_UTF8, username);

       nsString password;
       NS_CStringToUTF16 (nsCString(info->password),
                          NS_CSTRING_ENCODING_UTF8, password);

       pm->AddUser(host, username, password);
#endif /* !HAVE_GECKO_1_9 */
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
	iface->add = impl_add_password;
	iface->remove = impl_remove_password;
	iface->remove_all = impl_remove_all_passwords;
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
