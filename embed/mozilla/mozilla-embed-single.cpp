/*  vim:set ts=8 noet sw=8:
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#include "mozilla-embed-single.h"

#include "ephy-cookie-manager.h"
#include "ephy-password-manager.h"
#include "ephy-permission-manager.h"

#include "glib.h"
#include "ephy-debug.h"
#include "gtkmozembed.h"
#include "mozilla-embed.h"
#include "ephy-file-helpers.h"
#include "mozilla-notifiers.h"
#include "ephy-langs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "MozRegisterComponents.h"
#include "EphySingle.h"
#include "EphyUtils.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <nsCOMPtr.h>
#include <nsMemory.h>
#include <nsEmbedString.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
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

#ifdef HAVE_NSIPASSWORD_H
#include <nsIPassword.h>
#endif

#ifdef ALLOW_PRIVATE_API
// FIXME: For setting the locale. hopefully gtkmozembed will do itself soon
#include <nsIChromeRegistry.h>
#include <nsILocaleService.h>
#include <nsIProtocolProxyService.h>
#include <nsIHttpAuthManager.h>
#include <nsICacheService.h>
#include <nsIFontEnumerator.h>
#include <nsNetCID.h>
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
};

static void mozilla_embed_single_class_init	(MozillaEmbedSingleClass *klass);
static void ephy_embed_single_iface_init	(EphyEmbedSingleIface *iface);
static void ephy_cookie_manager_iface_init	(EphyCookieManagerIface *iface);
static void ephy_password_manager_iface_init	(EphyPasswordManagerIface *iface);
static void ephy_permission_manager_iface_init	(EphyPermissionManagerIface *iface);
static void mozilla_embed_single_init		(MozillaEmbedSingle *ges);
static gboolean have_gnome_url_handler		(const gchar *protocol);

static GObjectClass *parent_class = NULL;

GType
mozilla_embed_single_get_type (void)
{
       static GType type = 0;

        if (type == 0)
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
                                                                                                                             
        /* Load the default user preferences as well.  This also makes the
           prefs to be saved in the user's prefs.js file, instead of messing up
           our global defaults file. */
        rv = prefService->ReadUserPrefs (nsnull);
        if (NS_FAILED(rv))
        {
                g_warning ("failed to read user preferences, error: %x", rv);
        }

        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));
	NS_ENSURE_TRUE (pref, FALSE);

	/* FIXME We need to do this because mozilla doesnt set product
	sub for embedding apps */
	pref->SetCharPref ("general.useragent.vendor", "Epiphany");
	pref->SetCharPref ("general.useragent.vendorSub", VERSION);

	/* Make sure that Mozilla does not handle these protocols internally */
	pref->SetBoolPref("network.protocol-handler.external.news", TRUE);
	pref->SetBoolPref("network.protocol-handler.external.mailto", TRUE);
	pref->SetBoolPref("network.protocol-handler.external.irc", TRUE);
	if (have_gnome_url_handler ("ftp"))
	{
		pref->SetBoolPref("network.protocol-handler.external.ftp", TRUE);
	}
	else
	{
		pref->SetBoolPref("network.protocol-handler.external.ftp", FALSE);
	}

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
mozilla_embed_single_new_window_orphan_cb (GtkMozEmbedSingle *embed,
                      		          GtkMozEmbed **retval, 
					  guint chrome_mask,
                           		  EphyEmbedSingle *shell)
{
 	g_assert (chrome_mask & GTK_MOZ_EMBED_FLAG_OPENASCHROME);

	*retval = _mozilla_embed_new_xul_dialog ();
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

static gboolean
have_gnome_url_handler (const gchar *protocol)
{
	gchar *key, *cmd;
	gboolean rv;

	key = g_strdup_printf ("/desktop/gnome/url-handlers/%s/command", 
			       protocol);
	cmd = eel_gconf_get_string (key);
	g_free (key);

	rv = (cmd != NULL && strstr (cmd, "epiphany") == NULL);
	g_free (cmd);

	if (!rv) return rv;

	key = g_strdup_printf ("/desktop/gnome/url-handlers/%s/enabled", 
			       protocol);
	rv = eel_gconf_get_boolean (key);
	g_free (key);

	return rv;
}

static nsresult
getUILang (nsAString& aUILang)
{
	nsresult result;

	nsCOMPtr<nsILocaleService> localeService = do_GetService (NS_LOCALESERVICE_CONTRACTID, &result);
	if (NS_FAILED (result) || !localeService)
	{
		g_warning ("Could not get locale service!\n");
		return NS_ERROR_FAILURE;
	}

	result = localeService->GetLocaleComponentForUserAgent (aUILang);

	if (NS_FAILED (result))
	{
		g_warning ("Could not determine locale!\n");
		return NS_ERROR_FAILURE;
	}

	return NS_OK;
}

static nsresult
mozilla_init_chrome (void)
{
	nsresult result;
	nsEmbedString uiLang;

	nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService (NS_CHROMEREGISTRY_CONTRACTID);
	NS_ENSURE_TRUE (chromeRegistry, NS_ERROR_FAILURE);

	// Set skin to 'classic' so we get native scrollbars.
	result = chromeRegistry->SelectSkin (nsEmbedCString("classic/1.0"), PR_FALSE);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);

	// set locale
	result = chromeRegistry->SetRuntimeProvider(PR_TRUE);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);

	result = getUILang(uiLang);
	NS_ENSURE_SUCCESS (result, NS_ERROR_FAILURE);

	nsEmbedCString cUILang;
	NS_UTF16ToCString (uiLang, NS_CSTRING_ENCODING_UTF8, cUILang);

	return chromeRegistry->SelectLocale (cUILang, PR_FALSE);
}

static void
mozilla_init_observer (MozillaEmbedSingle *single)
{
	single->priv->mSingleObserver = new EphySingle ();

	if (single->priv->mSingleObserver)
	{
		single->priv->mSingleObserver->Init (EPHY_EMBED_SINGLE (single));
	}
}

static gboolean
init_services (MozillaEmbedSingle *single)
{
	/* Pre initialization */
	mozilla_init_home ();
	mozilla_init_profile ();

	/* Fire up the beast */
	gtk_moz_embed_push_startup ();

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
	mozilla_notifiers_init (EPHY_EMBED_SINGLE (single));
	STOP_PROFILER ("Mozilla prefs notifiers")

	mozilla_register_components ();

	mozilla_init_observer (single);

	return TRUE;
}

static void
mozilla_embed_single_init (MozillaEmbedSingle *mes)
{
 	mes->priv = MOZILLA_EMBED_SINGLE_GET_PRIVATE (mes);

	mes->priv->theme_window = NULL;
	mes->priv->user_prefs =
		g_build_filename (ephy_dot_dir (), 
				  MOZILLA_PROFILE_DIR,
				  MOZILLA_PROFILE_NAME,
				  MOZILLA_PROFILE_FILE,
				  NULL);

	mes->priv->mSingleObserver = nsnull;

	if (!init_services (mes))
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new
			(NULL,
                         GTK_DIALOG_MODAL,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_CLOSE,
                         _("Epiphany can't be used now. "
                         "Mozilla initialization failed. Check your "
                         "MOZILLA_FIVE_HOME environmental variable."));
		gtk_dialog_run (GTK_DIALOG (dialog));

		exit (0);
	}
}

static void
mozilla_embed_single_finalize (GObject *object)
{
	MozillaEmbedSingle *mes = MOZILLA_EMBED_SINGLE (object);

	/* Destroy EphyEmbedSingle before because some
	 * services depend on xpcom */
	G_OBJECT_CLASS (parent_class)->finalize (object);

	mozilla_notifiers_free ();

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
impl_set_offline_mode (EphyEmbedSingle *shell,
		       gboolean offline)
{
	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID);
	if (!io) return;

	io->SetOffline(offline);
}

static void
impl_load_proxy_autoconf (EphyEmbedSingle *shell,
			  const char* url)
{
	g_assert (url != NULL);

        nsCOMPtr<nsIProtocolProxyService> pps =
                do_GetService ("@mozilla.org/network/protocol-proxy-service;1");
	if (!pps) return;

	pps->ConfigureFromPAC (url);
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
	nsresult result;
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
		result = cookieEnumerator->GetNext (getter_AddRefs(keks));
		if (NS_FAILED (result) || !keks) continue;

		EphyCookie *cookie = mozilla_cookie_to_ephy_cookie (keks);

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

	cookieManager->Remove (nsEmbedCString(cookie->domain),
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

		nsEmbedString unicodeName;
		rv = nsPassword->GetUser (unicodeName);
		if (NS_FAILED (rv)) continue;

		nsEmbedCString userName;
		NS_UTF16ToCString (unicodeName,
				   NS_CSTRING_ENCODING_UTF8, userName);

		EphyPasswordInfo *p = g_new0 (EphyPasswordInfo, 1);

		p->host = g_strdup (transfer.get());
		p->username = g_strdup (userName.get());
		p->password = NULL;

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

	nsEmbedString userName;
	NS_CStringToUTF16 (nsEmbedCString(info->username),
			   NS_CSTRING_ENCODING_UTF8, userName);
	pm->RemoveUser (nsEmbedCString(info->host), userName);
}

static const char *permission_type_string [] =
{
	"cookie",
	"image",
	"popup"
};

void
impl_permission_manager_add (EphyPermissionManager *manager,
			     const char *host,
			     EphyPermissionType type,
			     EphyPermission permission)
{
	/* can only set allow or deny */
	g_return_if_fail (permission != EPHY_PERMISSION_DEFAULT);

        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return;

	nsCOMPtr<nsIURI> uri;
        EphyUtils::NewURI(getter_AddRefs(uri), nsEmbedCString(host));
	if (!uri) return;

	gboolean allow = (permission == EPHY_PERMISSION_ALLOWED);

	pm->Add (uri,
		 permission_type_string [type],
		 allow ? (PRUint32) nsIPermissionManager::ALLOW_ACTION :
			 (PRUint32) nsIPermissionManager::DENY_ACTION);
}

void
impl_permission_manager_remove (EphyPermissionManager *manager,
				const char *host,
				EphyPermissionType type)
{
        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return;

	pm->Remove (nsEmbedCString (host), permission_type_string [type]);
}

void
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
			      EphyPermissionType type)
{
        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return EPHY_PERMISSION_DEFAULT;

	nsCOMPtr<nsIURI> uri;
        EphyUtils::NewURI(getter_AddRefs(uri), nsEmbedCString (host));
        if (!uri) return EPHY_PERMISSION_DEFAULT;

	nsresult rv;
	PRUint32 action;
	rv = pm->TestPermission (uri, permission_type_string [type], &action);
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
			      EphyPermissionType type)
{
	GList *list = NULL;

        nsCOMPtr<nsIPermissionManager> pm
		(do_GetService (NS_PERMISSIONMANAGER_CONTRACTID));
	if (!pm) return NULL;

	nsCOMPtr<nsISimpleEnumerator> pe;
	pm->GetEnumerator(getter_AddRefs(pe));
	NS_ENSURE_TRUE (pe, NULL);
	
	PRBool more;
	for (pe->HasMoreElements (&more); more == PR_TRUE; pe->HasMoreElements (&more))
	{
		nsCOMPtr<nsIPermission> perm;
		pe->GetNext(getter_AddRefs(perm));
		if (!perm) continue;

		nsresult rv;
		nsEmbedCString str;
		rv = perm->GetType(str);
		if (NS_FAILED (rv)) continue;

		if (strcmp (str.get(), permission_type_string[type]) == 0)
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

static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->finalize = mozilla_embed_single_finalize;

	g_type_class_add_private (object_class, sizeof (MozillaEmbedSinglePrivate));
}

static void
ephy_embed_single_iface_init (EphyEmbedSingleIface *iface)
{
	iface->clear_cache = impl_clear_cache;
	iface->clear_auth_cache = impl_clear_auth_cache;
	iface->set_offline_mode = impl_set_offline_mode;
	iface->load_proxy_autoconf = impl_load_proxy_autoconf;
	iface->get_font_list = impl_get_font_list;
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
