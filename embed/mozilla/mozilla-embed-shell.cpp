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

#include "glib.h"
#include "ephy-string.h"
#include "gtkmozembed.h"
#include "mozilla-embed-shell.h"
#include "mozilla-prefs.h"
#include "ephy-prefs.h"
#include "ephy-file-helpers.h"
#include "mozilla-notifiers.h"
#include "mozilla-i18n.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "MozRegisterComponents.h"
#include "FilePicker.h"

#include <time.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>
#include <nsICacheService.h>
#include <nsCOMPtr.h>
#include <nsNetCID.h>
#include <nsIServiceManager.h>
#include <nsIIOService.h>
#include <nsIProtocolProxyService.h>
#include <nsIJVMManager.h>
#include <nsIAtom.h>
#include <nsICharsetConverterManager.h>
#include <nsICharsetConverterManager2.h>
#include <nsIFontList.h>
#include <nsISupportsPrimitives.h>
#include <nsReadableUtils.h>
#include <nsICookieManager.h>
#include <nsIPasswordManager.h>
#include <nsIPassword.h>
#include <nsICookie.h>
#include <nsCCookieManager.h>
#include <nsCPasswordManager.h>

#define MOZILLA_PROFILE_DIR  "/mozilla"
#define MOZILLA_PROFILE_NAME "epiphany"
#define MOZILLA_PROFILE_FILE "prefs.js"

static void
mozilla_embed_shell_class_init (MozillaEmbedShellClass *klass);
static void
mozilla_embed_shell_init (MozillaEmbedShell *ges);
static void
mozilla_embed_shell_finalize (GObject *object);

static void              
impl_get_capabilities (EphyEmbedShell *shell,
		       EmbedShellCapabilities *caps);
static gresult      
impl_clear_cache (EphyEmbedShell *shell,
		  CacheType type);
static gresult          
impl_set_offline_mode (EphyEmbedShell *shell,
		       gboolean offline);
static gresult           
impl_load_proxy_autoconf (EphyEmbedShell *shell,
			  const char* url);
static gresult           
impl_get_charset_titles (EphyEmbedShell *shell,
			 const char *group,
			 GList **charsets);
static gresult           
impl_get_charset_groups (EphyEmbedShell *shell,
		         GList **groups);
static gresult
impl_get_font_list (EphyEmbedShell *shell,
		    const char *langGroup,
		    const char *fontType,
		    GList **fontList,
		    char **default_font);
static gresult           
impl_list_cookies (EphyEmbedShell *shell,
		   GList **cookies);
static gresult           
impl_remove_cookies (EphyEmbedShell *shell,
		     GList *cookies);
static gresult           
impl_list_passwords (EphyEmbedShell *shell,
		     PasswordType type, 
		     GList **passwords);
static gresult           
impl_remove_passwords (EphyEmbedShell *shell,
		       GList *passwords,
		       PasswordType type);
static gresult 
impl_show_file_picker (EphyEmbedShell *shell,
		       GtkWidget *parentWidget, 
		       const char *title,
		       const char *directory,
		       const char *file, 
		       FilePickerMode mode,
                       char **ret_fullpath, 
		       gboolean *ret_save_content, 
                       FileFormat *file_formats, 
		       int *ret_file_format);

static void mozilla_embed_shell_new_window_orphan_cb (GtkMozEmbedSingle *embed,
            	           		              GtkMozEmbed **retval, 
					              guint chrome_mask,
                           		              EphyEmbedShell *shell);

struct MozillaEmbedShellPrivate
{
	GHashTable *charsets_hash;
	GList *sorted_charsets_titles;
};

static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);

static GObjectClass *parent_class = NULL;

GType
mozilla_embed_shell_get_type (void)
{
       static GType mozilla_embed_shell_type = 0;

        if (mozilla_embed_shell_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (MozillaEmbedShellClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) mozilla_embed_shell_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (MozillaEmbedShell),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) mozilla_embed_shell_init
                };

                mozilla_embed_shell_type = g_type_register_static (EPHY_EMBED_SHELL_TYPE,
								   "MozillaEmbedShell",
								   &our_info, (GTypeFlags)0);
        }

        return mozilla_embed_shell_type;
}

static void
mozilla_embed_shell_class_init (MozillaEmbedShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEmbedShellClass *shell_class;
	
	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
	shell_class = EPHY_EMBED_SHELL_CLASS (klass);
	
        object_class->finalize = mozilla_embed_shell_finalize;

	shell_class->get_capabilities = impl_get_capabilities;
	shell_class->clear_cache = impl_clear_cache;
	shell_class->set_offline_mode = impl_set_offline_mode;
	shell_class->load_proxy_autoconf = impl_load_proxy_autoconf;
	shell_class->get_charset_titles = impl_get_charset_titles;
	shell_class->get_charset_groups = impl_get_charset_groups;
	shell_class->get_font_list = impl_get_font_list;
	shell_class->list_cookies = impl_list_cookies;
	shell_class->remove_cookies = impl_remove_cookies;
	shell_class->list_passwords = impl_list_passwords;
	shell_class->remove_passwords = impl_remove_passwords;
	shell_class->show_file_picker = impl_show_file_picker;
}

static void
mozilla_load_proxy_prefs (MozillaEmbedShell *shell)
{
	char *tmp;
	int i, mozilla_mode = 0;
	
	/* Proxy mode */
	tmp = eel_gconf_get_string (CONF_NETWORK_PROXY_MODE);
	g_return_if_fail (tmp != NULL);
	
	if (strcmp (tmp, "manual") == 0)
	{
		mozilla_mode = 1;
	}
	else if (strcmp (tmp, "auto") == 0)
	{
		mozilla_mode = 2;
	}

	mozilla_prefs_set_int ("network.proxy.type", mozilla_mode);
	g_free (tmp);

	/* Http proxy */
	tmp = eel_gconf_get_string (CONF_NETWORK_HTTP_PROXY);
	g_return_if_fail (tmp != NULL);
	mozilla_prefs_set_string ("network.proxy.http", tmp);
	g_free (tmp);

	i = eel_gconf_get_integer (CONF_NETWORK_HTTP_PROXY_PORT);
	mozilla_prefs_set_int ("network.proxy.http_port", i);
	
	/* Ftp proxy */
	tmp = eel_gconf_get_string (CONF_NETWORK_FTP_PROXY);
	g_return_if_fail (tmp != NULL);
	mozilla_prefs_set_string ("network.proxy.ftp", tmp);
	g_free (tmp);

	i = eel_gconf_get_integer (CONF_NETWORK_FTP_PROXY_PORT);
	mozilla_prefs_set_int ("network.proxy.ftp_port", i);

	/* Secure proxy */
	tmp = eel_gconf_get_string (CONF_NETWORK_SSL_PROXY);
	g_return_if_fail (tmp != NULL);
	mozilla_prefs_set_string ("network.proxy.ssl", tmp);
	g_free (tmp);

	i = eel_gconf_get_integer (CONF_NETWORK_SSL_PROXY_PORT);
	mozilla_prefs_set_int ("network.proxy.ssl_port", i);

	/* Socks proxy */
	tmp = eel_gconf_get_string (CONF_NETWORK_SOCKS_PROXY);
	g_return_if_fail (tmp != NULL);
	mozilla_prefs_set_string ("network.proxy.socks", tmp);
	g_free (tmp);

	i = eel_gconf_get_integer (CONF_NETWORK_SOCKS_PROXY_PORT);
	mozilla_prefs_set_int ("network.proxy.socks_port", i);

	/* Autoconfiguration */
	tmp = eel_gconf_get_string (CONF_NETWORK_PROXY_AUTO_URL);
	g_return_if_fail (tmp != NULL);
	ephy_embed_shell_load_proxy_autoconf 
		(EPHY_EMBED_SHELL (shell), tmp);
	g_free (tmp);
}

static void
mozilla_set_default_prefs (void)
{
        mozilla_prefs_set_boolean ("mozilla.widget.raise-on-setfocus",
                                   FALSE);
        mozilla_prefs_set_boolean ("browser.display.use_system_colors",
                                   FALSE);

        /* set default search engine */
        mozilla_prefs_set_string ("keyword.URL",_("http://www.google.com/search?q="));
        mozilla_prefs_set_boolean ("keyword.enabled", TRUE);
        mozilla_prefs_set_boolean ("security.checkloaduri", FALSE); 

        /* while we have no UI */
        mozilla_prefs_set_boolean ("wallet.captureForms", FALSE);

        /* deactivate mailcap and mime.types support */
        mozilla_prefs_set_string ("helpers.global_mime_types_file", "");
        mozilla_prefs_set_string ("helpers.global_mailcap_file", "");
        mozilla_prefs_set_string ("helpers.private_mime_types_file", "");
        mozilla_prefs_set_string ("helpers.private_mailcap_file", "");
        
        /* dont allow xpi installs from epiphany, there are crashes */
        mozilla_prefs_set_boolean ("xpinstall.enabled", FALSE);

        /* disable sucky XUL ftp view, have nice ns4-like html page instead */
        mozilla_prefs_set_boolean ("network.dir.generate_html", TRUE);
        
        /* set the right accept encoding flags */
        mozilla_prefs_set_string ("network.http.accept-encoding" ,
                                  "gzip, deflate, compress;q=0.9");
	
        mozilla_prefs_save ();
}

static void
mozilla_init_single (MozillaEmbedShell *mes)
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
                          GTK_SIGNAL_FUNC (mozilla_embed_shell_new_window_orphan_cb),
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
is_new_build (void)
{
	gboolean new_build = FALSE;
	char *mozprefs, *build_test;
	
	mozprefs = g_build_filename (ephy_dot_dir (), 
				     MOZILLA_PROFILE_DIR,
				     MOZILLA_PROFILE_NAME,
				     MOZILLA_PROFILE_FILE,
				     NULL);

	/* no mozilla prefs ? or new epiphany build */
        build_test = eel_gconf_get_string ("/apps/epiphany/gconf_test");
        if (!g_file_test(mozprefs, G_FILE_TEST_EXISTS) || 
            build_test == NULL ||
            strncmp (build_test, __TIME__, 8) != 0)
        {
                new_build = TRUE;
                eel_gconf_set_string ("/apps/epiphany/gconf_test", __TIME__);
        }
	
        g_free (mozprefs);
        g_free (build_test);

	return new_build;
}

static void
mozilla_init_prefs (void)
{
	mozilla_set_default_prefs ();
	mozilla_notifiers_set_defaults ();
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

	rv = (cmd != NULL);
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
mozilla_embed_shell_init (MozillaEmbedShell *mes)
{
	gboolean new_build;

        mes->priv = g_new0 (MozillaEmbedShellPrivate, 1);

	mes->priv->charsets_hash = NULL;
	mes->priv->sorted_charsets_titles = NULL;

	new_build = is_new_build ();
	
	/* Pre initialization */
	mozilla_notifiers_init (mes);
	mozilla_init_home ();
	mozilla_init_profile ();
	
	/* Fire up the best */
	gtk_moz_embed_push_startup ();

	/* Post initialization */
	if (new_build)
	{
		mozilla_init_prefs ();
	}

	mozilla_load_proxy_prefs (mes);

	mozilla_init_single (mes);
	
	mozilla_register_components ();

	mozilla_register_external_protocols ();

	/* FIXME alert if fails */
}

static void 
mozilla_embed_shell_new_window_orphan_cb (GtkMozEmbedSingle *embed,
                      		          GtkMozEmbed **retval, 
					  guint chrome_mask,
                           		  EphyEmbedShell *shell)
{
	/* FIXME conversion duped in mozilla_embed */
	EphyEmbed *new_embed;
	int i;
        EmbedChromeMask mask = EMBED_CHROME_OPENASPOPUP;
        
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
                if (chrome_mask & conversion_map[i].chromemask)
                {
                        mask = (EmbedChromeMask) (mask | conversion_map[i].embed_mask); 
                }
        }
       
	g_signal_emit_by_name (shell, "new_window_orphan", &new_embed, mask);

	g_assert (new_embed != NULL);
	
	*retval = GTK_MOZ_EMBED(EPHY_EMBED(new_embed));
}

static void
mozilla_embed_shell_finalize (GObject *object)
{
	MozillaEmbedShell *mes;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_MOZILLA_EMBED_SHELL (object));

        mes = MOZILLA_EMBED_SHELL (object);

        g_return_if_fail (mes->priv != NULL);

	mozilla_notifiers_free ();

	mozilla_prefs_save ();
	
	gtk_moz_embed_pop_startup ();
	
        g_free (mes->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void              
impl_get_capabilities (EphyEmbedShell *shell,
		       EmbedShellCapabilities *caps)
{
	EmbedShellCapabilities mycaps;
	
	mycaps = (EmbedShellCapabilities)
	 	 (CACHE_CLEAR_CAP |
	          OFFLINE_CAP |
        	  PROXY_AUTOCONF_CAP |
	          JAVA_CONSOLE_CAP |
	          JS_CONSOLE_CAP |
	          CHARSETS_CAP |
	          COOKIES_CAP |
	          PASSWORDS_CAP);
	
	*caps = mycaps;
}

static gresult      
impl_clear_cache (EphyEmbedShell *shell,
		  CacheType type)
{
	nsresult rv;
	
	nsCOMPtr<nsICacheService> CacheService =
                        do_GetService (NS_CACHESERVICE_CONTRACTID, &rv);
	if (NS_FAILED(rv)) return G_FAILED;

	CacheService->EvictEntries((guint)type);

	return G_OK;
}

static gresult          
impl_set_offline_mode (EphyEmbedShell *shell,
		       gboolean offline)
{
	nsresult rv;

	nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv))
                return G_FAILED;

        rv = io->SetOffline(offline);
	if (NS_SUCCEEDED(rv)) return G_FAILED;
	
	return G_OK;
}

static gresult           
impl_load_proxy_autoconf (EphyEmbedShell *shell,
			  const char* url)
{
	nsresult rv;

        nsCOMPtr<nsIProtocolProxyService> pps =
                do_GetService ("@mozilla.org/network/protocol-proxy-service;1",
                               &rv);
        if (NS_FAILED(rv) || !pps) return G_FAILED;

        rv = pps->ConfigureFromPAC (url);
	if (NS_FAILED(rv)) return G_FAILED;
	
	return G_OK;
}

static gresult
fill_charsets_lists (MozillaEmbedShellPrivate *priv)
{
	nsresult rv;
	char *tmp;
        PRUint32 cscount;
        PRUint32 translated_cscount = get_translated_cscount ();
        char *charset_str, *charset_title_str;

        nsCOMPtr<nsIAtom> docCharsetAtom;
        nsCOMPtr<nsICharsetConverterManager2> ccm2 =
                do_GetService (NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
        if (!NS_SUCCEEDED(rv)) return G_FAILED;

        nsCOMPtr <nsISupportsArray> cs_list;
        rv = ccm2->GetDecoderList (getter_AddRefs(cs_list));
        if (!NS_SUCCEEDED(rv)) return G_FAILED;

        rv = cs_list->Count(&cscount);
        priv->charsets_hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (PRUint32 i = 0; i < cscount; i++)
        {
                nsCOMPtr<nsISupports> cssupports =
                                        (dont_AddRef)(cs_list->ElementAt(i));
                nsCOMPtr<nsIAtom> csatom ( do_QueryInterface(cssupports) );
                nsAutoString charset_ns, charset_title_ns;

                /* charset name */
		rv = csatom->ToString(charset_ns);
                tmp = ToNewCString (charset_ns);
                if (tmp == NULL || strlen (tmp) == 0)
                {
                        continue;
                }
		charset_str = g_strdup (tmp);
		nsMemory::Free (tmp);
		tmp = nsnull;

                /* charset readable title */
                rv = ccm2->GetCharsetTitle2(csatom, &charset_title_ns);
                tmp = ToNewCString (charset_title_ns);
                if (tmp == NULL || 
                    strlen (tmp) == 0)
                {
                        if (tmp) nsMemory::Free (tmp);
                        charset_title_str = g_strdup (charset_str);
                }
		else
		{
			charset_title_str = g_strdup (tmp);
			nsMemory::Free (tmp);
			tmp = nsnull;
		}

		for (PRUint32 j = 0; j < translated_cscount; j++)
                {
                        if (g_ascii_strcasecmp (
                                charset_str, 
                                charset_trans_array[j].charset_name) == 0)
                        {
                                g_free (charset_title_str);
                                charset_title_str = (char *) 
                                        _(charset_trans_array[j].charset_title);
                                break;
                        }
                }

		/* fill the hash and the sorted list */
		g_hash_table_insert (priv->charsets_hash, charset_title_str, charset_str);
                priv->sorted_charsets_titles = 
			g_list_insert_sorted (priv->sorted_charsets_titles,
                                              (gpointer)charset_title_str,
                                              (GCompareFunc)g_ascii_strcasecmp); 
        }

	return G_OK;
}

static void
ensure_charsets_tables (MozillaEmbedShell *shell)
{
	if (!shell->priv->charsets_hash)
	{
		fill_charsets_lists (shell->priv);
	}
}

static gresult           
impl_get_charset_titles (EphyEmbedShell *shell,
		         const char *group,
			 GList **charsets)
{
	MozillaEmbedShell *mshell = MOZILLA_EMBED_SHELL(shell);
	int count = get_translated_cscount ();
	GList *l = NULL;
	int j;
	
	ensure_charsets_tables (mshell);
	g_return_val_if_fail (mshell->priv->charsets_hash != NULL, G_FAILED);

	for (j = 0; j < count; j++)
	{	
		if (group == NULL ||
		    strcmp (group, lgroups[charset_trans_array[j].lgroup]) == 0) 
		{
			CharsetInfo *info;
			info = g_new0 (CharsetInfo, 1);
			info->name = charset_trans_array[j].charset_name;
			info->title = charset_trans_array[j].charset_title;
			l = g_list_append (l, info);

			/* FIXME check that the encoding exists in mozilla before
			 * adding it */
		}
	}

	*charsets = l;

	return G_OK;
}

static gresult           
impl_get_charset_groups (EphyEmbedShell *shell,
		         GList **groups)
{
	GList *l = NULL;
	int i;
	
	for (i = 0; lgroups[i] != NULL; i++)
	{
		l = g_list_append (l, (gpointer)lgroups[i]);
	}
	
	*groups = l;
	
	return G_OK;
}

static gresult
impl_get_font_list (EphyEmbedShell *shell,
		    const char *langGroup,
		    const char *fontType,
		    GList **fontList,
		    char **default_font)
{
	nsresult rv;

	nsCOMPtr<nsIFontList> mozFontList;
	mozFontList = do_CreateInstance("@mozilla.org/gfx/fontlist;1", &rv);
	if(NS_FAILED(rv)) return G_FAILED;

	nsCOMPtr<nsISimpleEnumerator> fontEnum;
	mozFontList->AvailableFonts(NS_ConvertUTF8toUCS2(langGroup).get(),
				    NS_ConvertUTF8toUCS2(fontType).get(),
				    getter_AddRefs(fontEnum));
	if(NS_FAILED(rv)) return G_FAILED;

	GList *l = NULL;
	PRBool enumResult;
	for(fontEnum->HasMoreElements(&enumResult) ;
	    enumResult == PR_TRUE;
	    fontEnum->HasMoreElements(&enumResult))
	{
		nsCOMPtr<nsISupportsString> fontName;
		fontEnum->GetNext(getter_AddRefs(fontName));
		if(NS_FAILED(rv)) return G_FAILED;

		nsString fontString;
		fontName->GetData(fontString);

		char *gFontString;
		gFontString = g_strdup(NS_ConvertUCS2toUTF8(fontString).get());
		l = g_list_append(l, gFontString);
	}
	*fontList = l;

	if (default_font != NULL)
	{
		char key [255];

		sprintf (key, "font.name.%s.%s", fontType, langGroup);
		
		*default_font = mozilla_prefs_get_string (key);
	}

	return G_OK;
}

static gresult           
impl_list_cookies (EphyEmbedShell *shell,
		   GList **cookies)
{
        nsresult result;

        nsCOMPtr<nsICookieManager> cookieManager = 
                        do_CreateInstance (NS_COOKIEMANAGER_CONTRACTID);
        nsCOMPtr<nsISimpleEnumerator> cookieEnumerator;
        result = 
            cookieManager->GetEnumerator (getter_AddRefs(cookieEnumerator));
        if (NS_FAILED(result)) return G_FAILED;
	
        PRBool enumResult;
        for (cookieEnumerator->HasMoreElements(&enumResult) ;
             enumResult == PR_TRUE ;
             cookieEnumerator->HasMoreElements(&enumResult))
        {
                CookieInfo *c;
        
                nsCOMPtr<nsICookie> nsCookie;
                result = cookieEnumerator->GetNext (getter_AddRefs(nsCookie));
                if (NS_FAILED(result)) return G_FAILED;

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
                
                *cookies = g_list_prepend (*cookies, c);
        }       

	*cookies = g_list_reverse (*cookies);
		
	return G_OK;
}

static gresult           
impl_remove_cookies (EphyEmbedShell *shell,
		     GList *cookies)
{
	nsresult result;
	GList *cl;
        nsCOMPtr<nsICookieManager> cookieManager =
                        do_CreateInstance (NS_COOKIEMANAGER_CONTRACTID);
	
        for (cl = g_list_first(cookies) ; cl != NULL ; 
             cl = g_list_next (cl))
        {
                CookieInfo *c = (CookieInfo *)cl->data;

                result = cookieManager->Remove (NS_LITERAL_CSTRING(c->domain),
                                                NS_LITERAL_CSTRING(c->name),
                                                NS_LITERAL_CSTRING(c->path),
                                                PR_FALSE);
                if (NS_FAILED(result)) return G_FAILED;
        };

	return G_OK;
}
	
static gresult           
impl_list_passwords (EphyEmbedShell *shell,
		     PasswordType type, 
		     GList **passwords)
{
        nsresult result = NS_ERROR_FAILURE;

        nsCOMPtr<nsIPasswordManager> passwordManager =
                        do_CreateInstance (NS_PASSWORDMANAGER_CONTRACTID);
        nsCOMPtr<nsISimpleEnumerator> passwordEnumerator;
        if (type == PASSWORD_PASSWORD)
                result = passwordManager->GetEnumerator 
                                (getter_AddRefs(passwordEnumerator));
        else if (type == PASSWORD_REJECT)
                result = passwordManager->GetRejectEnumerator 
                                (getter_AddRefs(passwordEnumerator));
        if (NS_FAILED(result)) return G_FAILED;      

        PRBool enumResult;
        for (passwordEnumerator->HasMoreElements(&enumResult) ;
             enumResult == PR_TRUE ;
             passwordEnumerator->HasMoreElements(&enumResult))
        {
                nsCOMPtr<nsIPassword> nsPassword;
                result = passwordEnumerator->GetNext 
                                        (getter_AddRefs(nsPassword));
                if (NS_FAILED(result)) return G_FAILED;

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

		*passwords = g_list_prepend (*passwords, p);
        }       

	*passwords = g_list_reverse (*passwords);

	return G_OK;
}

static gresult           
impl_remove_passwords (EphyEmbedShell *shell,
		       GList *passwords, 
		       PasswordType type)
{
	nsresult result = NS_ERROR_FAILURE;
        nsCOMPtr<nsIPasswordManager> passwordManager =
                        do_CreateInstance (NS_PASSWORDMANAGER_CONTRACTID);

        for (GList *l = g_list_first(passwords) ; l !=NULL ; 
             l = g_list_next(l))
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

                if (NS_FAILED(result)) return G_FAILED;
        };
	
        return G_OK;
}

static gresult 
impl_show_file_picker (EphyEmbedShell *shell,
		       GtkWidget *parentWidget, 
		       const char *title,
		       const char *directory,
		       const char *file, 
		       FilePickerMode mode,
                       char **ret_fullpath, 
		       gboolean *ret_save_content, 
                       FileFormat *file_formats, 
		       int *ret_file_format)
{
	PRBool showContentCheck;
        gchar *expanded_directory;

        if (ret_save_content == NULL)
                showContentCheck = PR_FALSE;
        else
                showContentCheck = PR_TRUE;

        GFilePicker *filePicker = new GFilePicker (showContentCheck, 
                                                   file_formats);

	/* FIXME sane path: expand tilde ... */
        expanded_directory = g_strdup (directory);

        /* make sure the directory exists, and use the home directory
         * otherwise */
        if (!expanded_directory ||
            !g_file_test (expanded_directory, G_FILE_TEST_IS_DIR))
        {
                if (expanded_directory) g_free (expanded_directory);
                expanded_directory = g_strdup (g_get_home_dir());
        }

        nsCOMPtr<nsILocalFile> dir = 
                                do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
        dir->InitWithNativePath (nsDependentCString(expanded_directory));
        g_free (expanded_directory);

        filePicker->InitWithGtkWidget (parentWidget, title, mode);
        filePicker->SetDefaultString (NS_ConvertUTF8toUCS2(file).get());
        filePicker->SetDisplayDirectory (dir);
        
        PRInt16 retval;
        filePicker->Show (&retval);

        if (ret_save_content != NULL)
        {
                if (retval == GFilePicker::returnOKSaveContent)
                        *ret_save_content = TRUE;
                else
                        *ret_save_content = FALSE;
        }
        if (ret_file_format != NULL)
        {
                *ret_file_format = filePicker->mSelectedFileFormat;
        }

        if (retval == nsIFilePicker::returnCancel)
        {
                delete filePicker;
                return G_FAILED;
        }
        else
        {
                if (*ret_fullpath)
                        g_free (*ret_fullpath);
                nsCOMPtr<nsILocalFile> file;
                filePicker->GetFile (getter_AddRefs(file));
		nsCAutoString tempFullPathStr;
                file->GetNativePath (tempFullPathStr);
                *ret_fullpath = g_strdup (tempFullPathStr.get());
                delete filePicker;
                return G_OK;
        }
}
