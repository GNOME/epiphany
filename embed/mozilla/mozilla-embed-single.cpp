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
 *
 *  $Id$
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
#include <libgnomevfs/gnome-vfs-utils.h>
#include <string.h>
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

// FIXME: For setting the locale. hopefully gtkmozembed will do itself soon
#include <nsIChromeRegistry.h>
#include <nsILocaleService.h>

#define MOZILLA_PROFILE_DIR  "/mozilla"
#define MOZILLA_PROFILE_NAME "epiphany"
#define MOZILLA_PROFILE_FILE "prefs.js"
#define DEFAULT_PROFILE_FILE SHARE_DIR"/default-prefs.js"

/* language groups names */
static const
struct
{
	gchar *title;
	LanguageGroup group;
}
lang_groups[] =
{
        { N_("_Arabic"),		LG_ARABIC },
        { N_("_Baltic"),		LG_BALTIC },
        { N_("Central _European"),	LG_CENTRAL_EUROPEAN },
        { N_("Chi_nese"),		LG_CHINESE },
        { N_("_Cyrillic"),		LG_CYRILLIC },
        { N_("_Greek"),			LG_GREEK },
        { N_("_Hebrew"),		LG_HEBREW },
        { N_("_Indian"),		LG_INDIAN },
        { N_("_Japanese"),		LG_JAPANESE },
        { N_("_Korean"),		LG_KOREAN },
        { N_("_Turkish"),		LG_TURKISH },
        { N_("_Unicode"),		LG_UNICODE },
        { N_("_Vietnamese"),		LG_VIETNAMESE },
        { N_("_Western"),		LG_WESTERN },
        { N_("_Other"),			LG_OTHER }
};
static const guint n_lang_groups = G_N_ELEMENTS (lang_groups);

/**
 * translatable encodings titles
 * NOTE: if you add /change encodings, please also update the schema file
 * epiphany.schemas.in
 */
static const 
struct
{
	gchar *title;
	gchar *name;
	LanguageGroup group;
}
encodings[] =
{ 
	/* translators: access keys need only be unique within the same LG_group */
	{ N_("Arabic (_IBM-864)"),                  "IBM864",                LG_ARABIC },
	{ N_("Arabic (ISO-_8859-6)"),               "ISO-8859-6",            LG_ARABIC },
	{ N_("Arabic (_MacArabic)"),                "x-mac-arabic",          LG_ARABIC },
	{ N_("Arabic (_Windows-1256)"),             "windows-1256",          LG_ARABIC },
	{ N_("Baltic (_ISO-8859-13)"),              "ISO-8859-13",           LG_BALTIC },
	{ N_("Baltic (I_SO-8859-4)"),               "ISO-8859-4",            LG_BALTIC },
	{ N_("Baltic (_Windows-1257)"),             "windows-1257",          LG_BALTIC },
	{ N_("Central European (_IBM-852)"),        "IBM852",                LG_CENTRAL_EUROPEAN },
	{ N_("Central European (I_SO-8859-2)"),     "ISO-8859-2",	     LG_CENTRAL_EUROPEAN },
	{ N_("Central European (_MacCE)"),          "x-mac-ce",              LG_CENTRAL_EUROPEAN },
	{ N_("Central European (_Windows-1250)"),   "windows-1250",          LG_CENTRAL_EUROPEAN },
	{ N_("Croatian (Mac_Croatian)"),            "x-mac-croatian",        LG_CENTRAL_EUROPEAN },
	{ N_("Chinese Simplified (_GB18030)"),      "gb18030",               LG_CHINESE },
	{ N_("Chinese Simplified (G_B2312)"),       "GB2312",                LG_CHINESE },
	{ N_("Chinese Simplified (GB_K)"),          "x-gbk",                 LG_CHINESE },
	{ N_("Chinese Simplified (_HZ)"),           "HZ-GB-2312",	     LG_CHINESE },
	{ N_("Chinese Simplified (_ISO-2022-CN)"),  "ISO-2022-CN",           LG_CHINESE },
	{ N_("Chinese Traditional (Big_5)"),        "Big5",                  LG_CHINESE },
	{ N_("Chinese Traditional (Big5-HK_SCS)"),  "Big5-HKSCS",	     LG_CHINESE },
	{ N_("Chinese Traditional (_EUC-TW)"),      "x-euc-tw",              LG_CHINESE },
	{ N_("Cyrillic (_IBM-855)"),                "IBM855",                LG_CYRILLIC },
	{ N_("Cyrillic (I_SO-8859-5)"),             "ISO-8859-5",	     LG_CYRILLIC },
	{ N_("Cyrillic (IS_O-IR-111)"),             "ISO-IR-111",	     LG_CYRILLIC },
	{ N_("Cyrillic (_KOI8-R)"),                 "KOI8-R",                LG_CYRILLIC },
	{ N_("Cyrillic (_MacCyrillic)"),            "x-mac-cyrillic",        LG_CYRILLIC },
	{ N_("Cyrillic (_Windows-1251)"),           "windows-1251",          LG_CYRILLIC },
	{ N_("Cyrillic/Russian (_CP-866)"),         "IBM866",                LG_CYRILLIC },
	{ N_("Cyrillic/Ukrainian (_KOI8-U)"),       "KOI8-U",                LG_CYRILLIC },
	{ N_("Cyrillic/Ukrainian (Mac_Ukrainian)"), "x-mac-ukrainian",       LG_CYRILLIC },
	{ N_("Greek (_ISO-8859-7)"),                "ISO-8859-7",            LG_GREEK },
	{ N_("Greek (_MacGreek)"),                  "x-mac-greek",           LG_GREEK },
	{ N_("Greek (_Windows-1253)"),              "windows-1253",          LG_GREEK },
	{ N_("Gujarati (_MacGujarati)"),            "x-mac-gujarati",        LG_INDIAN },
	{ N_("Gurmukhi (Mac_Gurmukhi)"),            "x-mac-gurmukhi",        LG_INDIAN },
	{ N_("Hindi (Mac_Devanagari)"),             "x-mac-devanagari",      LG_INDIAN },
	{ N_("Hebrew (_IBM-862)"),                  "IBM862",                LG_HEBREW },
	{ N_("Hebrew (IS_O-8859-8-I)"),             "ISO-8859-8-I",          LG_HEBREW },
	{ N_("Hebrew (_MacHebrew)"),                "x-mac-hebrew",          LG_HEBREW },
	{ N_("Hebrew (_Windows-1255)"),             "windows-1255",          LG_HEBREW },
	{ N_("_Visual Hebrew (ISO-8859-8)"),        "ISO-8859-8",            LG_HEBREW },
	{ N_("Japanese (_EUC-JP)"),                 "EUC-JP",                LG_JAPANESE },
	{ N_("Japanese (_ISO-2022-JP)"),            "ISO-2022-JP",           LG_JAPANESE },
	{ N_("Japanese (_Shift-JIS)"),              "Shift_JIS",             LG_JAPANESE },
	{ N_("Korean (_EUC-KR)"),                   "EUC-KR",                LG_KOREAN },
	{ N_("Korean (_ISO-2022-KR)"),              "ISO-2022-KR",           LG_KOREAN },
	{ N_("Korean (_JOHAB)"),                    "x-johab",               LG_KOREAN },
	{ N_("Korean (_UHC)"),                      "x-windows-949",         LG_KOREAN },
	{ N_("Turkish (_IBM-857)"),                 "IBM857",                LG_TURKISH },
	{ N_("Turkish (I_SO-8859-9)"),              "ISO-8859-9",            LG_TURKISH },
	{ N_("Turkish (_MacTurkish)"),              "x-mac-turkish",         LG_TURKISH },
	{ N_("Turkish (_Windows-1254)"),            "windows-1254",          LG_TURKISH },
	{ N_("Unicode (UTF-_7)"),                   "UTF-7",                 LG_UNICODE },
	{ N_("Unicode (UTF-_8)"),                   "UTF-8",                 LG_UNICODE },
	{ N_("Vietnamese (_TCVN)"),                 "x-viet-tcvn5712",       LG_VIETNAMESE },
	{ N_("Vietnamese (_VISCII)"),               "VISCII",                LG_VIETNAMESE },
	{ N_("Vietnamese (V_PS)"),                  "x-viet-vps",            LG_VIETNAMESE },
	{ N_("Vietnamese (_Windows-1258)"),         "windows-1258",          LG_VIETNAMESE },
	{ N_("Western (_IBM-850)"),                 "IBM850",                LG_WESTERN },
	{ N_("Western (I_SO-8859-1)"),              "ISO-8859-1",            LG_WESTERN },
	{ N_("Western (IS_O-8859-15)"),             "ISO-8859-15",           LG_WESTERN },
	{ N_("Western (_MacRoman)"),                "x-mac-roman",           LG_WESTERN },
	{ N_("Western (_Windows-1252)"),            "windows-1252",          LG_WESTERN },
	{ N_("_Armenian (ARMSCII-8)"),              "armscii-8",             LG_OTHER },
	{ N_("_Celtic (ISO-8859-14)"),              "ISO-8859-14",           LG_OTHER },
	{ N_("_Farsi (MacFarsi)"),                  "x-mac-farsi",           LG_OTHER },
	{ N_("_Georgian (GEOSTD8)"),                "geostd8",               LG_OTHER },
	{ N_("_Icelandic (MacIcelandic)"),          "x-mac-icelandic",       LG_OTHER },
	{ N_("_Nordic (ISO-8859-10)"),              "ISO-8859-10",           LG_OTHER },
	{ N_("_Romanian (MacRomanian)"),            "x-mac-romanian",        LG_OTHER },
	{ N_("R_omanian (ISO-8859-16)"),            "ISO-8859-16",           LG_OTHER },
	{ N_("South _European (ISO-8859-3)"),       "ISO-8859-3",            LG_OTHER },
	{ N_("Thai (TIS-_620)"),                    "TIS-620",               LG_OTHER },
#if MOZILLA_SNAPSHOT >= 10 
	{ N_("Thai (IS_O-8859-11)"),                "iso-8859-11",           LG_OTHER },
	{ N_("_Thai (Windows-874)"),                "windows-874",           LG_OTHER },
#endif	
	{ N_("_User Defined"),                      "x-user-defined",        LG_OTHER },
};
static const guint n_encodings = G_N_ELEMENTS (encodings);

static void
mozilla_embed_single_class_init (MozillaEmbedSingleClass *klass);
static void
mozilla_embed_single_init (MozillaEmbedSingle *ges);
static void
mozilla_embed_single_finalize (GObject *object);

static gresult      
impl_clear_cache (EphyEmbedSingle *shell);
static gresult          
impl_set_offline_mode (EphyEmbedSingle *shell,
		       gboolean offline);
static gresult           
impl_load_proxy_autoconf (EphyEmbedSingle *shell,
			  const char* url);
static gresult           
impl_get_encodings (EphyEmbedSingle *shell,
		    LanguageGroup group,
		    gboolean elide_underscores,
		    GList **encodings_list);
static gresult           
impl_get_language_groups (EphyEmbedSingle *shell,
		          GList **groups);
static gresult
impl_get_font_list (EphyEmbedSingle *shell,
		    const char *langGroup,
		    GList **fontList);
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
	shell_class->get_encodings = impl_get_encodings;
	shell_class->get_language_groups = impl_get_language_groups;
	shell_class->get_font_list = impl_get_font_list;
	shell_class->list_cookies = impl_list_cookies;
	shell_class->remove_cookies = impl_remove_cookies;
	shell_class->list_passwords = impl_list_passwords;
	shell_class->remove_passwords = impl_remove_passwords;
	shell_class->show_file_picker = impl_show_file_picker;

	g_type_class_add_private (object_class, sizeof(MozillaEmbedSinglePrivate));
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

	nsXPIDLString uiLang;
	result = localeService->GetLocaleComponentForUserAgent (getter_Copies(uiLang));
	aUILang = uiLang;
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

static gresult      
impl_clear_cache (EphyEmbedSingle *shell)
{
	nsresult rv;
	
	nsCOMPtr<nsICacheService> CacheService =
                        do_GetService (NS_CACHESERVICE_CONTRACTID, &rv);
	if (NS_FAILED(rv)) return G_FAILED;

	CacheService->EvictEntries (nsICache::STORE_ANYWHERE);

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

static gint
encoding_info_cmp (const EncodingInfo *i1, const EncodingInfo *i2)
{
	return strcmp (i1->key, i2->key);
}

static gresult           
impl_get_encodings (EphyEmbedSingle *shell,
		   LanguageGroup group,
		   gboolean elide_underscores,
		   GList **encodings_list)
{
	GList *l = NULL;
	guint i;

	for (i = 0; i < n_encodings; i++)
	{
		if (group == LG_ALL || group == encodings[i].group)
		{
			EncodingInfo *info;
			gchar *elided = NULL;

			info = g_new0 (EncodingInfo, 1);

			info->encoding = g_strdup (encodings[i].name);
			
			elided = ephy_string_elide_underscores (_(encodings[i].title));

			if (elide_underscores)
			{
				info->title = g_strdup (elided);
			}
			else
			{
				info->title = g_strdup (_(encodings[i].title));
			}

			/* collate without underscores */
			info->key = g_utf8_collate_key (elided, -1);

			info->group = encodings[i].group;

			l = g_list_prepend (l, info);
			g_free (elided);
		}
	}

	*encodings_list = g_list_sort (l, (GCompareFunc) encoding_info_cmp);

	return G_OK;
}

static gint
language_group_info_cmp (const LanguageGroupInfo *i1, const LanguageGroupInfo *i2)
{
	return strcmp (i1->key, i2->key);
}

static gresult           
impl_get_language_groups (EphyEmbedSingle *shell,
		          GList **groups)
{
	GList *l = NULL;
	guint i;
	
	for (i = 0; i < n_lang_groups; i++)
	{
		LanguageGroupInfo *info;
		gchar *elided = NULL;
		
		info = g_new0 (LanguageGroupInfo, 1);
		
		info->title = g_strdup (_(lang_groups[i].title));
		info->group = lang_groups[i].group;
		
		/* collate without underscores */
		elided = ephy_string_elide_underscores (info->title);
		info->key = g_utf8_collate_key (elided, -1);
		g_free (elided);

		l = g_list_prepend (l, info);
	}
	
	*groups = g_list_sort (l, (GCompareFunc) language_group_info_cmp);
	
	return G_OK;
}

static gresult
impl_get_font_list (EphyEmbedSingle *shell,
		    const char *langGroup,
		    GList **fontList)
{
	nsresult rv;
	PRUint32 fontCount;
	PRUnichar **fontArray;
	GList *l = NULL;

	nsCOMPtr<nsIFontEnumerator> mozFontEnumerator;
	mozFontEnumerator = do_CreateInstance("@mozilla.org/gfx/fontenumerator;1", &rv);
	if(NS_FAILED(rv)) return G_FAILED;

	rv = mozFontEnumerator->EnumerateFonts (nsnull, nsnull,
					        &fontCount, &fontArray);
	if (NS_FAILED (rv)) return G_FAILED;

	for (PRUint32 i = 0; i < fontCount; i++)
	{
		char *gFontString;

		gFontString = g_strdup(NS_ConvertUCS2toUTF8 (fontArray[i]).get());
		l = g_list_prepend (l, gFontString);
		nsMemory::Free (fontArray[i]);
	}

	nsMemory::Free (fontArray);

	*fontList = g_list_reverse (l);

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

                result = cookieManager->Remove (nsDependentCString(c->domain),
                                                nsDependentCString(c->name),
                                                nsDependentCString(c->path),
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
	char *expanded_directory = NULL;
	gresult result;

        GFilePicker *filePicker = new GFilePicker ();

	if (directory != NULL)
	{
	        expanded_directory = gnome_vfs_expand_initial_tilde (directory);
	}

        /* make sure the directory exists, and use the home directory
         * otherwise */
        if (!expanded_directory ||
            !g_file_test (expanded_directory, G_FILE_TEST_IS_DIR))
        {
                g_free (expanded_directory);
                expanded_directory = g_strdup (g_get_home_dir());
        }

        nsCOMPtr<nsILocalFile> dir = 
                                do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
        dir->InitWithNativePath (nsDependentCString(expanded_directory));
        g_free (expanded_directory);

        filePicker->InitWithGtkWidget (parentWidget, title, mode);
	if (file)
	{
	        filePicker->SetDefaultString (NS_ConvertUTF8toUCS2(file).get());
	}
        filePicker->SetDisplayDirectory (dir);
        
        PRInt16 retval;
        filePicker->Show (&retval);

        nsCOMPtr<nsILocalFile> local_file;
	filePicker->GetFile (getter_AddRefs(local_file));
	nsCAutoString tempFullPathStr;
	local_file->GetNativePath (tempFullPathStr);
	*ret_fullpath = g_strdup (tempFullPathStr.get());

        result = (retval == nsIFilePicker::returnCancel) ? G_FAILED : G_OK;

	delete filePicker;

	return result;
}
