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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glib.h"
#include "ephy-string.h"
#include "ephy-debug.h"
#include "gtkmozembed.h"
#include "mozilla-embed-single.h"
#include "ephy-prefs.h"
#include "ephy-file-helpers.h"
#include "mozilla-notifiers.h"
#include "ephy-langs.h"
#include "eel-gconf-extensions.h"
#include "ephy-embed-prefs.h"
#include "MozRegisterComponents.h"
#include "FilePicker.h"

#include <time.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>
#include <nsICacheService.h>
#include <nsCOMPtr.h>
#include <nsIPrefService.h>
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

/* language groups */
typedef enum
{
	LG_ARABIC,
	LG_BALTIC,
	LG_CENTRAL_EUROPEAN,
	LG_CHINESE,
	LG_CYRILLIC,
	LG_GREEK,
	LG_HEBREW,
	LG_INDIAN,
	LG_JAPANESE, 
	LG_KOREAN,
	LG_TURKISH,
	LG_UNICODE,
	LG_VIETNAMESE,
	LG_WESTERN,
	LG_OTHER,
} LanguageGroup;

/* language groups names */
static const gchar *lgroups[] =
{
        N_("Arabic"),
        N_("Baltic"),
        N_("Central European"),
        N_("Chinese"),
        N_("Cyrillic"),
        N_("Greek"),
        N_("Hebrew"),
        N_("Indian"),
        N_("Japanese"), 
        N_("Korean"),
        N_("Turkish"),
        N_("Unicode"),
        N_("Vietnamese"),
        N_("Western"),
        N_("Other")
};
static const guint n_lgroups = G_N_ELEMENTS (lgroups);

/* translated charset titles */
static const
struct
{
	gchar *charset_title;
	gchar *charset_name;
	LanguageGroup lgroup;
}
charset_trans_array[] =
{ 
	{ N_("Arabic (IBM-864)"),                  "IBM864",                LG_ARABIC },
	{ N_("Arabic (IBM-864-I)"),                "IBM864i",               LG_ARABIC },
	{ N_("Arabic (ISO-8859-6)"),               "ISO-8859-6",            LG_ARABIC },
	{ N_("Arabic (ISO-8859-6-E)"),             "ISO-8859-6-E",          LG_ARABIC },
	{ N_("Arabic (ISO-8859-6-I)"),             "ISO-8859-6-I",          LG_ARABIC },
	{ N_("Arabic (MacArabic)"),                "x-mac-arabic",          LG_ARABIC },
	{ N_("Arabic (Windows-1256)"),             "windows-1256",          LG_ARABIC },
	{ N_("Armenian (ARMSCII-8)"),              "armscii-8",             LG_OTHER },
	{ N_("Baltic (ISO-8859-13)"),              "ISO-8859-13",           LG_BALTIC },
	{ N_("Baltic (ISO-8859-4)"),               "ISO-8859-4",            LG_BALTIC },
	{ N_("Baltic (Windows-1257)"),             "windows-1257",          LG_BALTIC },
	{ N_("Celtic (ISO-8859-14)"),              "ISO-8859-14",           LG_OTHER },
	{ N_("Central European (IBM-852)"),        "IBM852",                LG_CENTRAL_EUROPEAN },
	{ N_("Central European (ISO-8859-2)"),     "ISO-8859-2",	    LG_CENTRAL_EUROPEAN },
	{ N_("Central European (MacCE)"),          "x-mac-ce",              LG_CENTRAL_EUROPEAN },
	{ N_("Central European (Windows-1250)"),   "windows-1250",          LG_CENTRAL_EUROPEAN },
	{ N_("Chinese Simplified (GB18030)"),      "gb18030",               LG_CHINESE },
	{ N_("Chinese Simplified (GB2312)"),       "GB2312",                LG_CHINESE },
	{ N_("Chinese Simplified (GBK)"),          "x-gbk",                 LG_CHINESE },
	{ N_("Chinese Simplified (HZ)"),           "HZ-GB-2312",	    LG_CHINESE },
	{ N_("Chinese Simplified (ISO-2022-CN)"),  "ISO-2022-CN",           LG_CHINESE },
	{ N_("Chinese Simplified (Windows-936)"),  "windows-936",           LG_CHINESE },
	{ N_("Chinese Traditional (Big5)"),        "Big5",                  LG_CHINESE },
	{ N_("Chinese Traditional (Big5-HKSCS)"),  "Big5-HKSCS",	    LG_CHINESE },
	{ N_("Chinese Traditional (EUC-TW)"),      "x-euc-tw",              LG_CHINESE },
	{ N_("Croatian (MacCroatian)"),            "x-mac-croatian",        LG_CENTRAL_EUROPEAN },
	{ N_("Cyrillic (IBM-855)"),                "IBM855",                LG_CYRILLIC },
	{ N_("Cyrillic (ISO-8859-5)"),             "ISO-8859-5",	    LG_CYRILLIC },
	{ N_("Cyrillic (ISO-IR-111)"),             "ISO-IR-111",	    LG_CYRILLIC },
	{ N_("Cyrillic (KOI8-R)"),                 "KOI8-R",                LG_CYRILLIC },
	{ N_("Cyrillic (MacCyrillic)"),            "x-mac-cyrillic",        LG_CYRILLIC },
	{ N_("Cyrillic (Windows-1251)"),           "windows-1251",          LG_CYRILLIC },
	{ N_("Cyrillic/Russian (CP-866)"),         "IBM866",                LG_CYRILLIC },
	{ N_("Cyrillic/Ukrainian (KOI8-U)"),       "KOI8-U",                LG_CYRILLIC },
	{ N_("Cyrillic/Ukrainian (MacUkrainian)"), "x-mac-ukrainian",       LG_CYRILLIC },
	{ N_("English (US-ASCII)"),                "us-ascii",              LG_WESTERN },
	{ N_("Farsi (MacFarsi)"),                  "x-mac-farsi",           LG_OTHER },
	{ N_("Georgian (GEOSTD8)"),                "geostd8",               LG_OTHER },
	{ N_("Greek (ISO-8859-7)"),                "ISO-8859-7",            LG_GREEK },
	{ N_("Greek (MacGreek)"),                  "x-mac-greek",           LG_GREEK },
	{ N_("Greek (Windows-1253)"),              "windows-1253",          LG_GREEK },
	{ N_("Gujarati (MacGujarati)"),            "x-mac-gujarati",        LG_INDIAN },
	{ N_("Gurmukhi (MacGurmukhi)"),            "x-mac-gurmukhi",        LG_INDIAN },
	{ N_("Hebrew (IBM-862)"),                  "IBM862",                LG_HEBREW },
	{ N_("Hebrew (ISO-8859-8-E)"),             "ISO-8859-8-E",          LG_HEBREW },
	{ N_("Hebrew (ISO-8859-8-I)"),             "ISO-8859-8-I",          LG_HEBREW },
	{ N_("Hebrew (MacHebrew)"),                "x-mac-hebrew",          LG_HEBREW },
	{ N_("Hebrew (Windows-1255)"),             "windows-1255",          LG_HEBREW },
	{ N_("Hindi (MacDevanagari)"),             "x-mac-devanagari",      LG_INDIAN },
	{ N_("Icelandic (MacIcelandic)"),          "x-mac-icelandic",       LG_OTHER },
	{ N_("Japanese (EUC-JP)"),                 "EUC-JP",                LG_JAPANESE },
	{ N_("Japanese (ISO-2022-JP)"),            "ISO-2022-JP",           LG_JAPANESE },
	{ N_("Japanese (Shift_JIS)"),              "Shift_JIS",             LG_JAPANESE },
	{ N_("Korean (EUC-KR)"),                   "EUC-KR",                LG_KOREAN },
	{ N_("Korean (ISO-2022-KR)"),              "ISO-2022-KR",           LG_KOREAN },
	{ N_("Korean (JOHAB)"),                    "x-johab",               LG_KOREAN },
	{ N_("Korean (UHC)"),                      "x-windows-949",         LG_KOREAN },
	{ N_("Nordic (ISO-8859-10)"),              "ISO-8859-10",           LG_OTHER },
	{ N_("Romanian (MacRomanian)"),            "x-mac-romanian",        LG_OTHER },
	{ N_("Romanian (ISO-8859-16)"),            "ISO-8859-16",           LG_OTHER },
	{ N_("South European (ISO-8859-3)"),       "ISO-8859-3",            LG_OTHER },
	{ N_("Thai (TIS-620)"),                    "TIS-620",               LG_OTHER },
	{ N_("Turkish (IBM-857)"),                 "IBM857",                LG_TURKISH },
	{ N_("Turkish (ISO-8859-9)"),              "ISO-8859-9",            LG_TURKISH },
	{ N_("Turkish (MacTurkish)"),              "x-mac-turkish",         LG_TURKISH },
	{ N_("Turkish (Windows-1254)"),            "windows-1254",          LG_TURKISH },
	{ N_("Unicode (UTF-7)"),                   "UTF-7",                 LG_UNICODE },
	{ N_("Unicode (UTF-8)"),                   "UTF-8",                 LG_UNICODE },
	{ N_("Unicode (UTF-16BE)"),                "UTF-16BE",              LG_UNICODE },
	{ N_("Unicode (UTF-16LE)"),                "UTF-16LE",              LG_UNICODE },
	{ N_("Unicode (UTF-32BE)"),                "UTF-32BE",              LG_UNICODE },
	{ N_("Unicode (UTF-32LE)"),                "UTF-32LE",              LG_UNICODE },
	{ N_("User Defined"),                      "x-user-defined",        LG_OTHER },
	{ N_("Vietnamese (TCVN)"),                 "x-viet-tcvn5712",       LG_VIETNAMESE },
	{ N_("Vietnamese (VISCII)"),               "VISCII",                LG_VIETNAMESE },
	{ N_("Vietnamese (VPS)"),                  "x-viet-vps",            LG_VIETNAMESE },
	{ N_("Vietnamese (Windows-1258)"),         "windows-1258",          LG_VIETNAMESE },
	{ N_("Visual Hebrew (ISO-8859-8)"),        "ISO-8859-8",            LG_HEBREW },
	{ N_("Western (IBM-850)"),                 "IBM850",                LG_WESTERN },
	{ N_("Western (ISO-8859-1)"),              "ISO-8859-1",            LG_WESTERN },
	{ N_("Western (ISO-8859-15)"),             "ISO-8859-15",           LG_WESTERN },
	{ N_("Western (MacRoman)"),                "x-mac-roman",           LG_WESTERN },
	{ N_("Western (Windows-1252)"),            "windows-1252",          LG_WESTERN },
	/* charsets whithout posibly translatable names */
	{ "T.61-8bit",                             "T.61-8bit",             LG_OTHER },
	{ "x-imap4-modified-utf7",                 "x-imap4-modified-utf7", LG_UNICODE },
	{ "x-u-escaped",                           "x-u-escaped",           LG_OTHER }
};
static const guint n_charset_trans_array = G_N_ELEMENTS (charset_trans_array);

static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass);
static void
mozilla_embed_single_init (MozillaEmbedSingle *ges);
static void
mozilla_embed_single_finalize (GObject *object);

static gresult      
impl_clear_cache (EphyEmbedSingle *shell,
		  CacheType type);
static gresult          
impl_set_offline_mode (EphyEmbedSingle *shell,
		       gboolean offline);
static gresult           
impl_load_proxy_autoconf (EphyEmbedSingle *shell,
			  const char* url);
static gresult           
impl_get_charset_titles (EphyEmbedSingle *shell,
			 const char *group,
			 GList **charsets);
static gresult           
impl_get_charset_groups (EphyEmbedSingle *shell,
		         GList **groups);
static gresult
impl_get_font_list (EphyEmbedSingle *shell,
		    const char *langGroup,
		    const char *fontType,
		    GList **fontList,
		    char **default_font);
static gresult           
impl_list_cookies (EphyEmbedSingle *shell,
		   GList **cookies);
static gresult           
impl_remove_cookies (EphyEmbedSingle *shell,
		     GList *cookies);
static gresult           
impl_list_passwords (EphyEmbedSingle *shell,
		     PasswordType type, 
		     GList **passwords);
static gresult           
impl_remove_passwords (EphyEmbedSingle *shell,
		       GList *passwords,
		       PasswordType type);
static gresult 
impl_show_file_picker (EphyEmbedSingle *shell,
		       GtkWidget *parentWidget, 
		       const char *title,
		       const char *directory,
		       const char *file, 
		       FilePickerMode mode,
                       char **ret_fullpath, 
                       FileFormat *file_formats, 
		       int *ret_file_format);

static void mozilla_embed_single_new_window_orphan_cb (GtkMozEmbedSingle *embed,
            	           		              GtkMozEmbed **retval, 
					              guint chrome_mask,
                           		              EphyEmbedSingle *shell);

struct MozillaEmbedSinglePrivate
{
	char *user_prefs;
	GHashTable *charsets_hash;
	GList *sorted_charsets_titles;
};

static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);

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

                mozilla_embed_single_type = g_type_register_static (EPHY_EMBED_SINGLE_TYPE,
								   "MozillaEmbedSingle",
								   &our_info, (GTypeFlags)0);
        }

        return mozilla_embed_single_type;
}

static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEmbedSingleClass *shell_class;
	
	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);
	shell_class = EPHY_EMBED_SINGLE_CLASS (klass);
	
        object_class->finalize = mozilla_embed_single_finalize;

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
mozilla_set_default_prefs (MozillaEmbedSingle *mes)
{
	nsCOMPtr<nsIPrefService> prefService;

        prefService = do_GetService (NS_PREFSERVICE_CONTRACTID);
	g_return_if_fail (prefService != NULL);

        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));
	g_return_if_fail (pref != NULL);

	/* Don't allow mozilla to raise window when setting focus (work around bugs) */
	pref->SetBoolPref ("mozilla.widget.raise-on-setfocus", PR_FALSE);

	/* set default search engine */
	pref->SetCharPref ("keyword.URL", "http://www.google.com/search?btnI=I%27m+Feeling+Lucky&q=");
	pref->SetBoolPref ("keyword.enabled", PR_TRUE);
	pref->SetBoolPref ("security.checkloaduri", PR_FALSE);

	/* dont allow xpi installs from epiphany, there are crashes */
	pref->SetBoolPref ("xpinstall.enabled", PR_FALSE);

	/* deactivate mailcap and mime.types support */
	pref->SetCharPref ("helpers.global_mailcap_file", "");
	pref->SetCharPref ("helpers.global_mime_types_file", "");
	pref->SetCharPref ("helpers.private_mailcap_file", "");
	pref->SetCharPref ("helpers.private_mime_types_file", "");

	/* disable sucky XUL ftp view, have nice ns4-like html page instead */
	pref->SetBoolPref ("network.dir.generate_html", PR_TRUE);

	/* disable usless security warnings */
	pref->SetBoolPref ("security.warn_entering_secure", PR_FALSE);
	pref->SetBoolPref ("security.warn_leaving_secure", PR_FALSE);
	pref->SetBoolPref ("security.warn_submit_insecure", PR_FALSE);

	/* Always use the system colors if a page doesn't specify its own. */
	pref->SetBoolPref ("browser.display.use_system_colors", PR_TRUE);

	/* Smooth scrolling on */
	pref->SetBoolPref ("general.smoothScroll", PR_TRUE);

	/* Disable blinking text and marquee, its non-standard and annoying */
	pref->SetBoolPref ("browser.blink_allowed", PR_FALSE);
	pref->SetBoolPref ("browser.display.enable_marquee", PR_FALSE);

	/* Enable Browsing with the Caret */
	pref->SetBoolPref ("accessibility.browsewithcaret", PR_TRUE);

	/* Don't Fetch the Sidebar whats related information, since we don't use it */
	pref->SetBoolPref ("browser.related.enabled", PR_FALSE);

	/* Line Wrap View->Source */
	pref->SetBoolPref ("view_source.wrap_long_lines", PR_TRUE);

	/* CTRL-Mousewheel scrolls by one page */
	pref->SetIntPref ("mousewheel.withcontrolkey.action", 1);
	pref->SetIntPref ("mousewheel.withcontrolkey.numlines", 1);
	pref->SetBoolPref ("mousewheel.withcontrolkey.sysnumlines", PR_FALSE);

	/* Enable Image Auto-Resizing */
	pref->SetBoolPref ("browser.enable_automatic_image_resizing", PR_TRUE);
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
                          GTK_SIGNAL_FUNC (mozilla_embed_single_new_window_orphan_cb),
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
 	mes->priv = g_new0 (MozillaEmbedSinglePrivate, 1);
	mes->priv->charsets_hash = NULL;
	mes->priv->sorted_charsets_titles = NULL;

	mes->priv->user_prefs =
		g_build_filename (ephy_dot_dir (), 
				  MOZILLA_PROFILE_DIR,
				  MOZILLA_PROFILE_NAME,
				  MOZILLA_PROFILE_FILE,
				  NULL);

	/* Pre initialization */
	mozilla_init_home ();
	mozilla_init_profile ();
	
	/* Fire up the best */
	gtk_moz_embed_push_startup ();

	mozilla_set_default_prefs (mes);

	START_PROFILER ("Mozilla prefs notifiers")
	mozilla_notifiers_init (EPHY_EMBED_SINGLE (mes));
	STOP_PROFILER ("Mozilla prefs notifiers")

	mozilla_init_single (mes);
	
	mozilla_register_components ();

	mozilla_register_external_protocols ();

	/* FIXME alert if fails */
}

static void 
mozilla_embed_single_new_window_orphan_cb (GtkMozEmbedSingle *embed,
                      		          GtkMozEmbed **retval, 
					  guint chrome_mask,
                           		  EphyEmbedSingle *shell)
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
mozilla_embed_single_finalize (GObject *object)
{
	MozillaEmbedSingle *mes;

	/* Destroy EphyEmbedSingle before because some
	 * services depend on xpcom */
        G_OBJECT_CLASS (parent_class)->finalize (object);

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_MOZILLA_EMBED_SINGLE (object));

        mes = MOZILLA_EMBED_SINGLE (object);

        g_return_if_fail (mes->priv != NULL);

	mozilla_notifiers_free ();

	gtk_moz_embed_pop_startup ();

	g_free (mes->priv->user_prefs);
	
        g_free (mes->priv);
}

static gresult      
impl_clear_cache (EphyEmbedSingle *shell,
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
impl_set_offline_mode (EphyEmbedSingle *shell,
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
impl_load_proxy_autoconf (EphyEmbedSingle *shell,
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
fill_charsets_lists (MozillaEmbedSinglePrivate *priv)
{
	nsresult rv;
	char *tmp;
        PRUint32 cscount;
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
                        charset_title_str = g_strdup (charset_str);
                }
		else
		{
			charset_title_str = g_strdup (tmp);
		}

		if (tmp) nsMemory::Free (tmp);
		tmp = nsnull;

#ifdef GNOME_ENABLE_DEBUG
		gboolean found = FALSE;
#endif
		for (PRUint32 j = 0; j < n_charset_trans_array; j++)
                {
                        if (g_ascii_strcasecmp (
                                charset_str, 
                                charset_trans_array[j].charset_name) == 0)
                        {
                                g_free (charset_title_str);
                                charset_title_str = (char *) 
                                        _(charset_trans_array[j].charset_title);
#ifdef GNOME_ENABLE_DEBUG
				found = TRUE;
#endif
                                break;
                        }
                }

#ifdef GNOME_ENABLE_DEBUG
		if (found == FALSE)
		{
			g_warning ("Charset %s with title %s not in list!",
			           charset_str, charset_title_str);
		}
#endif

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
ensure_charsets_tables (MozillaEmbedSingle *shell)
{
	if (!shell->priv->charsets_hash)
	{
		fill_charsets_lists (shell->priv);
	}
}

static gresult           
impl_get_charset_titles (EphyEmbedSingle *shell,
		         const char *group,
			 GList **charsets)
{
	MozillaEmbedSingle *mshell = MOZILLA_EMBED_SINGLE(shell);
	GList *l = NULL;
	guint j;

	ensure_charsets_tables (mshell);
	g_return_val_if_fail (mshell->priv->charsets_hash != NULL, G_FAILED);

	for (j = 0; j < n_charset_trans_array; j++)
	{	
		if (group == NULL ||
		    strcmp (group, lgroups[charset_trans_array[j].lgroup]) == 0) 
		{
			CharsetInfo *info;
			info = g_new0 (CharsetInfo, 1);
			info->name = charset_trans_array[j].charset_name;
			info->title = charset_trans_array[j].charset_title;
			l = g_list_prepend (l, info);
		}
	}

	*charsets = g_list_reverse (l);

	return G_OK;
}

static gresult           
impl_get_charset_groups (EphyEmbedSingle *shell,
		         GList **groups)
{
	GList *l = NULL;
	guint i;
	
	for (i = 0; i < n_lgroups; i++)
	{
		l = g_list_prepend (l, (gpointer)lgroups[i]);
	}
	
	*groups = g_list_reverse (l);
	
	return G_OK;
}

static gresult
impl_get_font_list (EphyEmbedSingle *shell,
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
		l = g_list_prepend (l, gFontString);
	}
	*fontList = g_list_reverse (l);

	if (default_font != NULL)
	{
		char key[255];
		char *value = NULL;
		nsCOMPtr<nsIPrefService> prefService;

	        prefService = do_GetService (NS_PREFSERVICE_CONTRACTID);
		g_return_val_if_fail (prefService != NULL, G_FAILED);
	
	        nsCOMPtr<nsIPrefBranch> pref;
	        prefService->GetBranch ("", getter_AddRefs(pref));
		g_return_val_if_fail (pref != NULL, G_FAILED);

		sprintf (key, "font.name.%s.%s", fontType, langGroup);
		
		pref->GetCharPref (key, &value);
		*default_font = g_strdup (value);
		nsMemory::Free (value);
	}

	return G_OK;
}

static gresult           
impl_list_cookies (EphyEmbedSingle *shell,
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
impl_remove_cookies (EphyEmbedSingle *shell,
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
impl_list_passwords (EphyEmbedSingle *shell,
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
impl_remove_passwords (EphyEmbedSingle *shell,
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
impl_show_file_picker (EphyEmbedSingle *shell,
		       GtkWidget *parentWidget, 
		       const char *title,
		       const char *directory,
		       const char *file, 
		       FilePickerMode mode,
                       char **ret_fullpath, 
                       FileFormat *file_formats, 
		       int *ret_file_format)
{
	gchar *expanded_directory;

        GFilePicker *filePicker = new GFilePicker (file_formats);

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
                nsCOMPtr<nsILocalFile> file;
                filePicker->GetFile (getter_AddRefs(file));
		nsCAutoString tempFullPathStr;
                file->GetNativePath (tempFullPathStr);
                *ret_fullpath = g_strdup (tempFullPathStr.get());
                delete filePicker;
                return G_OK;
        }
}
