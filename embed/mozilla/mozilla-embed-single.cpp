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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "glib.h"
#include "ephy-debug.h"
#include "gtkmozembed.h"
#include "mozilla-embed.h"
#include "mozilla-embed-single.h"
#include "ephy-file-helpers.h"
#include "mozilla-notifiers.h"
#include "ephy-langs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "MozRegisterComponents.h"

#include <time.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <nsICacheService.h>
#include <nsCOMPtr.h>
#include <nsIPrefService.h>
#include <nsNetCID.h>
#include <nsIServiceManager.h>
#include <nsIIOService.h>
#include <nsIProtocolProxyService.h>
#include <nsIAtom.h>
#include <nsIFontEnumerator.h>
#include <nsISupportsPrimitives.h>
#include <nsReadableUtils.h>
#include <nsICookieManager.h>
#include <nsIPasswordManager.h>
#include <nsIPassword.h>
#include <nsICookie.h>
#include <nsCCookieManager.h>
#include <nsCPasswordManager.h>
#include <nsString.h>
#include <nsILocalFile.h>

// FIXME: For setting the locale. hopefully gtkmozembed will do itself soon
#include <nsIChromeRegistry.h>
#include <nsILocaleService.h>

#define MOZILLA_PROFILE_DIR  "/mozilla"
#define MOZILLA_PROFILE_NAME "epiphany"
#define MOZILLA_PROFILE_FILE "prefs.js"
#define DEFAULT_PROFILE_FILE SHARE_DIR"/default-prefs.js"

static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass);
static void
mozilla_embed_single_init (MozillaEmbedSingle *ges);
static void
mozilla_embed_single_finalize (GObject *object);

#define MOZILLA_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED_SINGLE, MozillaEmbedSinglePrivate))

struct MozillaEmbedSinglePrivate
{
	char *user_prefs;
	
	/* monitor this widget for theme changes*/
	GtkWidget *theme_window;
};

static GObjectClass *parent_class = NULL;

GType
mozilla_embed_single_get_type (void)
{
       static GType mozilla_embed_single_type = 0;

        if (mozilla_embed_single_type == 0)
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

                mozilla_embed_single_type = g_type_register_static (EPHY_TYPE_EMBED_SINGLE,
								   "MozillaEmbedSingle",
								   &our_info, (GTypeFlags)0);
        }

        return mozilla_embed_single_type;
}

EphyEmbedSingle *
mozilla_embed_single_new (void)
{
	return EPHY_EMBED_SINGLE (g_object_new (MOZILLA_TYPE_EMBED_SINGLE, NULL));
}

static gboolean
mozilla_set_default_prefs (MozillaEmbedSingle *mes)
{
	nsCOMPtr<nsIPrefService> prefService;

        prefService = do_GetService (NS_PREFSERVICE_CONTRACTID);
	if (prefService == NULL) return FALSE;

        /* read our predefined default prefs */
        nsresult rv;
        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewNativeLocalFile(
                NS_LITERAL_CSTRING(DEFAULT_PROFILE_FILE),
                PR_TRUE, getter_AddRefs(file));
        if (NS_FAILED (rv)) return FALSE;

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
	if (pref == NULL) return FALSE;

	/* FIXME We need to do this because mozilla doesnt set product
	sub for embedding apps */
	pref->SetCharPref ("general.useragent.vendor", "Epiphany");
	pref->SetCharPref ("general.useragent.vendorSub", VERSION);

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
	g_return_if_fail (prefService != NULL);

        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));
	g_return_if_fail (pref != NULL);

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
 	g_assert (chrome_mask && GTK_MOZ_EMBED_FLAG_OPENASCHROME);

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

static void
mozilla_register_external_protocols (void)
{
	/* FIXME register only when needed */
	if (have_gnome_url_handler ("ftp"))
	{
		mozilla_register_FtpProtocolHandler ();
	}
	else
	{
		mozilla_unregister_FtpProtocolHandler ();
	}

	mozilla_register_MailtoProtocolHandler ();
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

#if MOZILLA_SNAPSHOT >= 12
	result = localeService->GetLocaleComponentForUserAgent (aUILang);
#else
	nsXPIDLString uiLang;
	result = localeService->GetLocaleComponentForUserAgent (getter_Copies(uiLang));
	aUILang = uiLang;
#endif

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
	nsAutoString uiLang;

	nsCOMPtr<nsIXULChromeRegistry> chromeRegistry = do_GetService (NS_CHROMEREGISTRY_CONTRACTID, &result);
	if (NS_FAILED (result) || !chromeRegistry)
	{
		g_warning ("Could not get the chrome registry!\n");
		return NS_ERROR_FAILURE;
	}

	// Set skin to 'classic' so we get native scrollbars.
	result = chromeRegistry->SelectSkin (NS_LITERAL_CSTRING("classic/1.0"), PR_FALSE);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	// set locale
	chromeRegistry->SetRuntimeProvider(PR_TRUE);

	result = getUILang(uiLang);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	result = chromeRegistry->SelectLocale (NS_ConvertUCS2toUTF8(uiLang), PR_FALSE);
	if (NS_FAILED (result)) return NS_ERROR_FAILURE;

	return NS_OK;
}

gboolean
mozilla_embed_single_init_services (MozillaEmbedSingle *single)
{
	/* Pre initialization */
	mozilla_init_home ();
	mozilla_init_profile ();
	
	/* Fire up the best */
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

	mozilla_register_external_protocols ();

	return TRUE;
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
	nsresult rv;
	
	nsCOMPtr<nsICacheService> CacheService =
                        do_GetService (NS_CACHESERVICE_CONTRACTID, &rv);
	if (NS_SUCCEEDED (rv))
	{
		CacheService->EvictEntries (nsICache::STORE_ANYWHERE);
	}
}

static void
impl_set_offline_mode (EphyEmbedSingle *shell,
		       gboolean offline)
{
	nsresult rv;

	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
	if (NS_SUCCEEDED (rv))
	{
		io->SetOffline(offline);
	}
}

static void
impl_load_proxy_autoconf (EphyEmbedSingle *shell,
			  const char* url)
{
	nsresult rv;

        nsCOMPtr<nsIProtocolProxyService> pps =
                do_GetService ("@mozilla.org/network/protocol-proxy-service;1",
                               &rv);
	if (NS_SUCCEEDED (rv))
	{
		pps->ConfigureFromPAC (url);
	}
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
	mozFontEnumerator = do_CreateInstance("@mozilla.org/gfx/fontenumerator;1", &rv);
	if(NS_FAILED(rv)) return NULL;

	rv = mozFontEnumerator->EnumerateFonts (nsnull, nsnull,
					        &fontCount, &fontArray);
	if (NS_FAILED (rv)) return NULL;

	for (PRUint32 i = 0; i < fontCount; i++)
	{
		char *gFontString;

		gFontString = g_strdup(NS_ConvertUCS2toUTF8 (fontArray[i]).get());
		l = g_list_prepend (l, gFontString);
		nsMemory::Free (fontArray[i]);
	}

	nsMemory::Free (fontArray);

	return g_list_reverse (l);
}

static GList *
impl_list_cookies (EphyEmbedSingle *shell)
{
        nsresult result;
	GList *cookies = NULL;

        nsCOMPtr<nsICookieManager> cookieManager = 
                        do_CreateInstance (NS_COOKIEMANAGER_CONTRACTID);
        nsCOMPtr<nsISimpleEnumerator> cookieEnumerator;
        result = 
            cookieManager->GetEnumerator (getter_AddRefs(cookieEnumerator));
        if (NS_FAILED(result)) return NULL;
	
        PRBool enumResult;
        for (cookieEnumerator->HasMoreElements(&enumResult) ;
             enumResult == PR_TRUE ;
             cookieEnumerator->HasMoreElements(&enumResult))
        {
                CookieInfo *c;
        
                nsCOMPtr<nsICookie> nsCookie;
                result = cookieEnumerator->GetNext (getter_AddRefs(nsCookie));
                if (NS_FAILED(result)) return NULL;

                c = g_new0 (CookieInfo, 1);

                nsCAutoString transfer;

                nsCookie->GetHost (transfer);
                c->domain = g_strdup (transfer.get());
                nsCookie->GetName (transfer);
                c->name = g_strdup (transfer.get());
                nsCookie->GetValue (transfer);
                c->value = g_strdup (transfer.get());
                nsCookie->GetPath (transfer);
                c->path = g_strdup (transfer.get());

		PRBool isSecure;
                nsCookie->GetIsSecure (&isSecure);
                if (isSecure == PR_TRUE) 
                        c->secure = g_strdup (_("Yes"));
                else 
                        c->secure = g_strdup (_("No"));

                PRUint64 dateTime;
                nsCookie->GetExpires (&dateTime);
		if(dateTime == 0)
			c->expire = g_strdup (_("End of current session"));
		else
	                c->expire = g_strdup_printf ("%s",ctime((time_t*)&dateTime));
                
                cookies = g_list_prepend (cookies, c);
        }       

	return g_list_reverse (cookies);
}

static void
impl_remove_cookies (EphyEmbedSingle *shell,
		     GList *cookies)
{
	nsresult result;
	GList *cl;
        nsCOMPtr<nsICookieManager> cookieManager =
                        do_CreateInstance (NS_COOKIEMANAGER_CONTRACTID);

	for (cl = cookies; cl != NULL; cl = cl->next)
        {
                CookieInfo *c = (CookieInfo *)cl->data;

                result = cookieManager->Remove (nsDependentCString(c->domain),
                                                nsDependentCString(c->name),
                                                nsDependentCString(c->path),
                                                PR_FALSE);
        };
}
	
static GList *
impl_list_passwords (EphyEmbedSingle *shell,
		     PasswordType type)
{
        nsresult result = NS_ERROR_FAILURE;
	GList *passwords = NULL;

        nsCOMPtr<nsIPasswordManager> passwordManager =
                        do_CreateInstance (NS_PASSWORDMANAGER_CONTRACTID);
        nsCOMPtr<nsISimpleEnumerator> passwordEnumerator;
        if (type == PASSWORD_PASSWORD)
                result = passwordManager->GetEnumerator 
                                (getter_AddRefs(passwordEnumerator));
        else if (type == PASSWORD_REJECT)
                result = passwordManager->GetRejectEnumerator 
                                (getter_AddRefs(passwordEnumerator));
        if (NS_FAILED(result)) return NULL;      

        PRBool enumResult;
        for (passwordEnumerator->HasMoreElements(&enumResult) ;
             enumResult == PR_TRUE ;
             passwordEnumerator->HasMoreElements(&enumResult))
        {
                nsCOMPtr<nsIPassword> nsPassword;
                result = passwordEnumerator->GetNext 
                                        (getter_AddRefs(nsPassword));
                if (NS_FAILED(result)) return NULL;

                PasswordInfo *p = g_new0 (PasswordInfo, 1);

                nsCAutoString transfer;
                nsPassword->GetHost (transfer);
                p->host = g_strdup (transfer.get());

                if (type == PASSWORD_PASSWORD)
                {
                        nsAutoString unicodeName;
                        nsPassword->GetUser (unicodeName);
                        p->username = g_strdup(NS_ConvertUCS2toUTF8(unicodeName).get());
                }

		passwords = g_list_prepend (passwords, p);
        }       

	return g_list_reverse (passwords);
}

static void
impl_remove_passwords (EphyEmbedSingle *shell,
		       GList *passwords, 
		       PasswordType type)
{
	nsresult result = NS_ERROR_FAILURE;
        nsCOMPtr<nsIPasswordManager> passwordManager =
                        do_CreateInstance (NS_PASSWORDMANAGER_CONTRACTID);
	GList *l;

	for (l = passwords; l != NULL; l = l->next)
        {
                PasswordInfo *p = (PasswordInfo *)l->data;
                if (type == PASSWORD_PASSWORD)
                {
                        result = passwordManager->RemoveUser (NS_LITERAL_CSTRING(p->host),
                                                              NS_ConvertUTF8toUCS2(nsDependentCString(p->username)));
                }
                else if (type == PASSWORD_REJECT)
                {
                        result = passwordManager->RemoveReject
                                        (nsDependentCString(p->host));
                };
        };
}

static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEmbedSingleClass *shell_class = EPHY_EMBED_SINGLE_CLASS (klass);
	
	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
	
        object_class->finalize = mozilla_embed_single_finalize;

	shell_class->clear_cache = impl_clear_cache;
	shell_class->set_offline_mode = impl_set_offline_mode;
	shell_class->load_proxy_autoconf = impl_load_proxy_autoconf;
	shell_class->get_font_list = impl_get_font_list;
	shell_class->list_cookies = impl_list_cookies;
	shell_class->remove_cookies = impl_remove_cookies;
	shell_class->list_passwords = impl_list_passwords;
	shell_class->remove_passwords = impl_remove_passwords;

	g_type_class_add_private (object_class, sizeof(MozillaEmbedSinglePrivate));
}
