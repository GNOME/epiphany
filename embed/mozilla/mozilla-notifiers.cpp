/*
 *  Copyright (C) 2000 Nate Case
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, OR (AT YOUR OPTION)
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

#include "ephy-embed-single.h"
#include "ephy-embed-shell.h"
#include "mozilla-notifiers.h"
#include "eel-gconf-extensions.h"
#include "MozRegisterComponents.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-langs.h"
#include "ephy-debug.h"

#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <stdlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtksettings.h>

#include <nsCOMPtr.h>
#include <nsEmbedString.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
/* we don't use glib/gi18n.h here because we need gnome_i18n_get_language_list() */
#include <libgnome/gnome-i18n.h>

#ifdef ALLOW_PRIVATE_API
#include <nsIProtocolProxyService.h>
#endif
 
#define MOZILLA_PREF_NO_PROXIES "network.proxy.no_proxies_on"
#define MIGRATE_PIXEL_SIZE

static void
mozilla_cache_size_notifier (GConfClient *client,
		             guint cnxn_id,
		             GConfEntry *entry,
		             char *pref);
static void
mozilla_own_colors_notifier(GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyEmbedSingle *single);
static void
mozilla_own_fonts_notifier(GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   EphyEmbedSingle *single);

static void
generic_mozilla_string_notifier(GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				const char *pref_name);
static void
generic_mozilla_int_notifier(GConfClient *client,
			     guint cnxn_id,
			     GConfEntry *entry,
			     const char *pref_name);
static void
generic_mozilla_bool_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      const char *pref_name);
static void
mozilla_allow_popups_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      EphyEmbedSingle *single);

static void
mozilla_language_notifier(GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  EphyEmbedSingle *single);

static void
mozilla_proxy_mode_notifier (GConfClient *client,
			     guint cnxn_id,
			     GConfEntry *entry,
			     EphyEmbedSingle *single);
static void
mozilla_proxy_autoconfig_notifier (GConfClient *client,
			           guint cnxn_id,
			           GConfEntry *entry,
			           EphyEmbedSingle *single);
static void
mozilla_proxy_mode_notifier (GConfClient *client,
		             guint cnxn_id,
		             GConfEntry *entry,
		             EphyEmbedSingle *single);
static void
mozilla_cookies_accept_notifier (GConfClient *client,
		                 guint cnxn_id,
		                 GConfEntry *entry,
		                 char *pref);
static void
mozilla_proxy_ignore_notifier (GConfClient *client,
		               guint cnxn_id,
		               GConfEntry *entry,
		               EphyEmbedSingle *single);

/* Keeps the list of the notifiers we installed for mozilla prefs */
/* to be able to remove them when exiting */
GList *mozilla_notifiers = NULL;
GList *font_infos = NULL;

enum
{
	BOOL_PREF,
	INT_PREF,
	STRING_PREF
};

static const struct 
{
	char *gconf_key;
	guint pref_type;
	const char *mozilla_key;
}
conversion_table [] =
{
	{ CONF_SECURITY_JAVA_ENABLED, BOOL_PREF, "security.enable_java" },
	{ CONF_SECURITY_JAVASCRIPT_ENABLED, BOOL_PREF, "javascript.enabled" },
	{ CONF_NETWORK_PROXY_AUTO_URL, STRING_PREF, "network.proxy.autoconfig_url" },
	{ CONF_NETWORK_HTTP_PROXY, STRING_PREF, "network.proxy.http" },
	{ CONF_NETWORK_FTP_PROXY, STRING_PREF, "network.proxy.ftp" },
	{ CONF_NETWORK_SSL_PROXY, STRING_PREF, "network.proxy.ssl" },
	{ CONF_NETWORK_SOCKS_PROXY, STRING_PREF, "network.proxy.socks" },
	{ CONF_NETWORK_HTTP_PROXY_PORT, INT_PREF, "network.proxy.http_port" },
	{ CONF_NETWORK_FTP_PROXY_PORT, INT_PREF, "network.proxy.ftp_port" },
	{ CONF_NETWORK_SSL_PROXY_PORT, INT_PREF, "network.proxy.ssl_port" },
	{ CONF_NETWORK_SOCKS_PROXY_PORT, INT_PREF, "network.proxy.socks_port" },
	{ CONF_LANGUAGE_DEFAULT_ENCODING, STRING_PREF, "intl.charset.default" },
	{ CONF_LANGUAGE_AUTODETECT_ENCODING, STRING_PREF, "intl.charset.detector" },
	{ CONF_BROWSE_WITH_CARET, BOOL_PREF, "accessibility.browsewithcaret" },

	{ NULL, 0, NULL }
};
 
static const struct 
{
	const char *gconf_key;
	GConfClientNotifyFunc func;
}
custom_notifiers [] =
{
	{ CONF_RENDERING_USE_OWN_COLORS, 
	  (GConfClientNotifyFunc) mozilla_own_colors_notifier },
	{ CONF_RENDERING_USE_OWN_FONTS, 
	  (GConfClientNotifyFunc) mozilla_own_fonts_notifier },
	{ CONF_SECURITY_ALLOW_POPUPS, 
	  (GConfClientNotifyFunc) mozilla_allow_popups_notifier },
	{ CONF_RENDERING_LANGUAGE, 
	  (GConfClientNotifyFunc) mozilla_language_notifier },
	{ CONF_NETWORK_PROXY_MODE,
	  (GConfClientNotifyFunc) mozilla_proxy_mode_notifier },
	{ CONF_NETWORK_PROXY_AUTO_URL,
	  (GConfClientNotifyFunc) mozilla_proxy_autoconfig_notifier },
	{ CONF_NETWORK_CACHE_SIZE,
          (GConfClientNotifyFunc) mozilla_cache_size_notifier },
	{ CONF_SECURITY_COOKIES_ACCEPT,
	  (GConfClientNotifyFunc) mozilla_cookies_accept_notifier },
 	{ CONF_NETWORK_PROXY_IGNORE_HOSTS,
 	  (GConfClientNotifyFunc) mozilla_proxy_ignore_notifier },

	{ NULL, NULL }
};

static gboolean
mozilla_prefs_set_string(const char *preference_name, const char *new_value)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);
        g_return_val_if_fail (new_value != NULL, FALSE);
        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
		nsresult rv = pref->SetCharPref (preference_name, new_value);            
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }

        return FALSE;
}

static gboolean
mozilla_prefs_set_boolean (const char *preference_name,
                           gboolean new_boolean_value)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);
  
        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));
  
        if (pref)
        {
                nsresult rv = pref->SetBoolPref (preference_name,
                                new_boolean_value ? PR_TRUE : PR_FALSE);
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }
        return FALSE;
}

static gboolean
mozilla_prefs_set_int (const char *preference_name, int new_int_value)
{
        g_return_val_if_fail (preference_name != NULL, FALSE);

        nsCOMPtr<nsIPrefService> prefService = 
                                do_GetService (NS_PREFSERVICE_CONTRACTID);
        nsCOMPtr<nsIPrefBranch> pref;
        prefService->GetBranch ("", getter_AddRefs(pref));

        if (pref)
        {
                nsresult rv = pref->SetIntPref (preference_name, new_int_value);
                return NS_SUCCEEDED (rv) ? TRUE : FALSE;
        }

        return FALSE;
}

static void
mozilla_cache_size_notifier (GConfClient *client,
		             guint cnxn_id,
		             GConfEntry *entry,
		             char *pref)
{
	int cache_size;

	cache_size = eel_gconf_get_integer (entry->key) * 1024;

	mozilla_prefs_set_int ("browser.cache.disk.capacity", cache_size);
}

static void
mozilla_font_size_notifier (GConfClient *client,
		            guint cnxn_id,
		            GConfEntry *entry,
		            char *pref)
{
	char key[255];
	
	if (entry->value == NULL) return;

	sprintf (key, "font.%s", pref);

	mozilla_prefs_set_int (key, MAX (eel_gconf_get_integer (entry->key), 1));
}

static void
mozilla_proxy_mode_notifier (GConfClient *client,
		             guint cnxn_id,
		             GConfEntry *entry,
		             EphyEmbedSingle *single)
{
	char *mode;
	int mozilla_mode = 0;
	
	mode = eel_gconf_get_string (entry->key);
	if (mode == NULL) return;
	
	if (strcmp (mode, "manual") == 0)
	{
		mozilla_mode = 1;
	}
	else if (strcmp (mode, "auto") == 0)
	{
		mozilla_mode = 2;
	}

	mozilla_prefs_set_int ("network.proxy.type", mozilla_mode);

	g_free (mode);
}

static void
mozilla_cookies_accept_notifier (GConfClient *client,
		                 guint cnxn_id,
		                 GConfEntry *entry,
		                 char *pref)
{
	char *mode;
	int mozilla_mode = 0;
	
	mode = eel_gconf_get_string (entry->key);
	if (mode == NULL) return;
	
	if (strcmp (mode, "anywhere") == 0)
	{
		mozilla_mode = 0;
	}
	else if (strcmp (mode, "current site") == 0)
	{
		mozilla_mode = 1;
	}
	else if (strcmp (mode, "nowhere") == 0)
	{
		mozilla_mode = 2;
	}

	mozilla_prefs_set_int ("network.cookie.cookieBehavior", mozilla_mode);

	g_free (mode);
}

static void
mozilla_font_notifier (GConfClient *client,
		       guint cnxn_id,
		       GConfEntry *entry,
		       char *pref)
{
	char key[255];
	char *name;

	if (entry->value == NULL) return;
	
	sprintf (key, "font.name.%s", pref);

	name = eel_gconf_get_string (entry->key);
	if (name)
	{
		mozilla_prefs_set_string (key, name);
		g_free (name);
	}
}

static void
mozilla_proxy_autoconfig_notifier (GConfClient *client,
		       	           guint cnxn_id,
		       		   GConfEntry *entry,
		       		   EphyEmbedSingle *single)
{
        nsCOMPtr<nsIProtocolProxyService> pps =
                do_GetService ("@mozilla.org/network/protocol-proxy-service;1");
        if (!pps) return;

	char *url = eel_gconf_get_string (entry->key);
	if (url && url[0] != '\0')
	{
#ifdef MOZ_NSIPROTOCOLPROXYSERVICE_NSACSTRING_
	        pps->ConfigureFromPAC (nsEmbedCString (url));
#else
	        pps->ConfigureFromPAC (url);
#endif
	}

	g_free (url);
}

static void
add_notification_and_notify (GConfClient		*client,
			     const char 		*key,
			     GConfClientNotifyFunc	 func,
			     gpointer			 user_data)
{
	GError *error = NULL;
	guint cnxn_id;

	cnxn_id = gconf_client_notify_add (client, key, func, user_data, NULL, &error);
	if (eel_gconf_handle_error (&error))
	{
		if (cnxn_id != EEL_GCONF_UNDEFINED_CONNECTION)
		{
			gconf_client_notify_remove (client, cnxn_id);
		}
		return;
	}

	mozilla_notifiers = g_list_append (mozilla_notifiers,
			                   GUINT_TO_POINTER (cnxn_id));

	gconf_client_notify (client, key);
}

#ifdef MIGRATE_PIXEL_SIZE

#define INT_ROUND(a) gint((a) + 0.5f)

/**
 *  This function gets the dpi in the same way that mozilla gets the dpi, 
 *  this allows us to convert from pixels to points easily
 */
static gint
mozilla_get_dpi ()
{
	GtkSettings* settings = gtk_settings_get_default ();
	gint dpi = 0;
 
	/* Use the gdk-xft-dpi setting if it is set */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (settings)),
					  "gtk-xft-dpi"))
	{
		g_object_get (G_OBJECT (settings), "gtk-xft-dpi", &dpi, NULL);
		if (dpi) return INT_ROUND (dpi / PANGO_SCALE);
	}

	/* Fall back to what xft thinks it is */
	char *val = XGetDefault (GDK_DISPLAY (), "Xft", "dpi");
	if (val)
	{
		char *e;
		double d = strtod(val, &e);
		if (e != val) return INT_ROUND (d);
	}
	
	/* Fall back to calculating manually from the gdk screen settings */
	float screenWidthIn = float (gdk_screen_width_mm()) / 25.4f;
	return INT_ROUND (gdk_screen_width() / screenWidthIn);
}

static void
mozilla_migrate_font_gconf_key (const char *pixel_key, const char *point_key)
{
	int size;

	size = eel_gconf_get_integer (pixel_key);

	if (size > 0)
	{
		/* Use doubles to get more accurate arithmetic */
		double dpi   = (double)mozilla_get_dpi ();
		double value = (double)eel_gconf_get_integer (pixel_key);
		gint point = INT_ROUND ((value * 72) / dpi);

		eel_gconf_set_integer (point_key, point);
	}
}

#endif

void 
mozilla_notifiers_init (EphyEmbedSingle *single) 
{
	GConfClient *client = eel_gconf_client_get_global ();
	guint i;
	const EphyFontsLanguageInfo *font_languages;
	guint n_font_languages;

	eel_gconf_monitor_add ("/apps/epiphany/web");
	eel_gconf_monitor_add ("/system/proxy");
	eel_gconf_monitor_add ("/system/http_proxy");

#ifdef MIGRATE_PIXEL_SIZE
	gboolean migrate_size;

	migrate_size = (eel_gconf_get_integer (CONF_SCHEMA_VERSION)
			< EPIPHANY_SCHEMA_VERSION);
	if (migrate_size)
	{
		eel_gconf_set_integer (CONF_SCHEMA_VERSION, EPIPHANY_SCHEMA_VERSION);
	}
#endif

	for (i = 0; conversion_table[i].gconf_key != NULL; i++)
	{
		GConfClientNotifyFunc func = NULL;

		switch (conversion_table[i].pref_type)
		{
		case INT_PREF:
			func = (GConfClientNotifyFunc) generic_mozilla_int_notifier;
			break;
		case BOOL_PREF:
			func = (GConfClientNotifyFunc) generic_mozilla_bool_notifier;
			break;
		case STRING_PREF:
			func = (GConfClientNotifyFunc) generic_mozilla_string_notifier;
			break;
		}

		g_assert (func != NULL);
		
		add_notification_and_notify
			(client,
			 conversion_table[i].gconf_key,
			 func,
			 (gpointer)conversion_table[i].mozilla_key);
	}

	for (i = 0; custom_notifiers[i].gconf_key != NULL; i++)
	{
		add_notification_and_notify
			(client,
			 custom_notifiers[i].gconf_key,
			 custom_notifiers[i].func,
			 (gpointer)single);
	}

	/* fonts notifiers */
	font_languages = ephy_font_languages ();
	n_font_languages = ephy_font_n_languages ();

	for (i=0; i < n_font_languages; i++)
	{
		const char *code = font_languages[i].code;
		guint k;
		char *types [] = { "variable", "monospace" };
		char key[255];
		char *info;
#ifdef MIGRATE_PIXEL_SIZE
		char old_key[255];
#endif
		
		for (k = 0; k < G_N_ELEMENTS (types); k++)
		{
			info = g_strconcat (types[k], ".", code, NULL);
			
			g_snprintf (key, 255, "%s_%s_%s", CONF_RENDERING_FONT, 
			 	    types[k], code);
			add_notification_and_notify (client, key,
						     (GConfClientNotifyFunc)mozilla_font_notifier,
						     info);
			font_infos = g_list_prepend (font_infos, info);
		}

#ifdef MIGRATE_PIXEL_SIZE
                if (migrate_size)
                {
			char *type;

			type = eel_gconf_get_string (CONF_RENDERING_FONT_TYPE_OLD);
			if (type && (strcmp (type, "serif") == 0 ||
				     strcmp (type, "sans-serif") == 0))
			{
				char *family;

				g_snprintf (old_key, 255, "%s_%s_%s",
					    CONF_RENDERING_FONT, type, code);
				g_snprintf (key, 255, "%s_%s_%s",
					    CONF_RENDERING_FONT, "variable", code);

				family = eel_gconf_get_string (old_key);
				if (family)
				{
					eel_gconf_set_string (key, family);
					g_free (family);
				}
			}

			g_free (type);
		}
#endif

		g_snprintf (key, 255, "%s_%s", CONF_RENDERING_FONT_MIN_SIZE, code);
		info = g_strconcat ("minimum-size", ".", code, NULL);
		add_notification_and_notify (client, key,
					     (GConfClientNotifyFunc)mozilla_font_size_notifier,
					     info);
		font_infos = g_list_prepend (font_infos, info);

#ifdef MIGRATE_PIXEL_SIZE
		if (migrate_size)
		{
			g_snprintf (old_key, 255, "%s_%s",
				    CONF_RENDERING_FONT_MIN_SIZE_OLD, code);
			mozilla_migrate_font_gconf_key (old_key, key);
		}
#endif

		g_snprintf (key, 255, "%s_%s", CONF_RENDERING_FONT_FIXED_SIZE, code);
		info = g_strconcat ("size.fixed", ".", code, NULL);
		add_notification_and_notify (client, key,
					     (GConfClientNotifyFunc)mozilla_font_size_notifier,
					     info);
		font_infos = g_list_prepend (font_infos, info);

#ifdef MIGRATE_PIXEL_SIZE
		if (migrate_size)
		{
			g_snprintf (old_key, 255, "%s_%s",
				    CONF_RENDERING_FONT_FIXED_SIZE_OLD, code);
			mozilla_migrate_font_gconf_key (old_key, key);
		}
#endif

		g_snprintf (key, 255, "%s_%s", CONF_RENDERING_FONT_VAR_SIZE, code);
		info = g_strconcat ("size.variable", ".", code, NULL);
		add_notification_and_notify (client, key,
					     (GConfClientNotifyFunc)mozilla_font_size_notifier,
					     info);
		font_infos = g_list_prepend (font_infos, info);

#ifdef MIGRATE_PIXEL_SIZE
		if (migrate_size)
		{
			g_snprintf (old_key, 255, "%s_%s",
				    CONF_RENDERING_FONT_VAR_SIZE_OLD, code);
			mozilla_migrate_font_gconf_key (old_key, key);
		}
#endif
	}
}

void 
mozilla_notifiers_free (void)
{
	eel_gconf_monitor_remove ("/apps/epiphany/web");
	eel_gconf_monitor_remove ("/system/proxy");
	eel_gconf_monitor_remove ("/system/http_proxy");

	g_list_foreach (mozilla_notifiers, 
		        (GFunc)eel_gconf_notification_remove, 
		        NULL);
	g_list_free(mozilla_notifiers);

	g_list_foreach (font_infos, (GFunc) g_free, NULL);
	g_list_free (font_infos);
}

/**
 * generic_mozilla_string_notify: update mozilla pref to match epiphany prefs.
 * 	user_data should match the mozilla key
 */
static void
generic_mozilla_string_notifier(GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				const char *pref_name)
{
	char *value;

	value = eel_gconf_get_string (entry->key);
	if (value)
	{
		mozilla_prefs_set_string (pref_name, value);
		g_free (value);
	}
}


/**
 * generic_mozilla_int_notify: update mozilla pref to match epiphany prefs.
 * 	user_data should match the mozilla key
 */
static void
generic_mozilla_int_notifier(GConfClient *client,
			     guint cnxn_id,
			     GConfEntry *entry,
			     const char *pref_name)
{
	int value;

	value = eel_gconf_get_integer (entry->key);
	mozilla_prefs_set_int (pref_name, value);
}


/**
 * generic_mozilla_bool_notify: update mozilla pref to match epiphany prefs.
 * 	user_data should match the mozilla key
 */
static void
generic_mozilla_bool_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      const char *pref_name)
{
	gboolean value;

	value = eel_gconf_get_boolean (entry->key);
	mozilla_prefs_set_boolean (pref_name, value);
}

static void
mozilla_own_colors_notifier(GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyEmbedSingle *single)
{
	gboolean value;

	value = eel_gconf_get_boolean (entry->key);
	mozilla_prefs_set_boolean ("browser.display.use_document_colors", !value);
}

static void
mozilla_own_fonts_notifier(GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   EphyEmbedSingle *single)
{
	int value;

	value = eel_gconf_get_boolean (entry->key) ? 0 : 1;
	mozilla_prefs_set_int("browser.display.use_document_fonts", value);
}

static void
mozilla_allow_popups_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      EphyEmbedSingle *single)
{
	gboolean value;

	value = eel_gconf_get_boolean (entry->key);
	mozilla_prefs_set_boolean ("dom.disable_open_during_load", !value);
}

static char *
get_system_language ()
{
	const GList *sys_langs;
	const char *lang;

	/**
	 * This is a comma separated list of language ranges, as specified
	 * by RFC 2616, 14.4.
	 * Always include the basic language code last.
	 *
	 * Examples:
	 * "pt"    translation: "pt"
	 * "pt_BR" translation: "pt-br,pt"
	 * "zh_CN" translation: "zh-cn,zh"
	 * "zh_HK" translation: "zh-hk,zh" or maybe "zh-hk,zh-tw,zh"
	 */
	lang = _("system-language");
	
	if (strncmp (lang, "system-language", 15) != 0)
	{
		/* the l10n has it */
		return g_strdup (lang);
	}
	
	sys_langs = gnome_i18n_get_language_list ("LC_MESSAGES");

	if (sys_langs)
	{
		lang = (char *)sys_langs->data;

		/* FIXME this probably need to be smarter */
		/* FIXME this can be up to 8 chars, not just 2 */
		if (strcmp (lang, "C") != 0)
		{
			return g_strndup (lang, 2);
		}
	}

	/* fallback to english */
	return g_strdup ("en");
}

static void
mozilla_language_notifier(GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  EphyEmbedSingle *single)
{
	GSList *languages, *l, *ulist = NULL;
	GString *result;

	languages = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	result = g_string_new ("");

	/* substitute the system language */
	l = g_slist_find_custom (languages, "system", (GCompareFunc) strcmp);
	if (l != NULL)
	{
		char *sys_lang;
		int index;

		index = g_slist_position (languages, l);
		g_free (l->data);
		languages = g_slist_delete_link (languages, l);

		sys_lang = get_system_language ();

		if (sys_lang)
		{
			char **s;
			int i = 0;

			s = g_strsplit (sys_lang, ",", -1);
			while (s[i] != NULL)
			{
				languages = g_slist_insert (languages, g_strdup (s[i]), index);

				index ++;
				i++;
			}

			g_strfreev (s);
			g_free (sys_lang);
		}		
	}

	/* now make a list of unique entries */
	for (l = languages; l != NULL; l = l->next)
	{
		if (g_slist_find_custom (ulist, l->data, (GCompareFunc) strcmp) == NULL)
		{
			ulist = g_slist_prepend (ulist, l->data);
		}
	}
	ulist = g_slist_reverse (ulist);

	for (l = ulist; l != NULL; l = l->next)
	{
		g_string_append (result, (char *)l->data);

		if (l->next) g_string_append (result, ",");
	}

	mozilla_prefs_set_string ("intl.accept_languages", result->str);

	g_string_free (result, TRUE);

	g_slist_foreach (languages, (GFunc) g_free, NULL);
	g_slist_free (languages);
	g_slist_free (ulist);
}

static void
mozilla_proxy_ignore_notifier (GConfClient *client,
		               guint cnxn_id,
		               GConfEntry *entry,
		               EphyEmbedSingle *single)
{
	GSList *hosts, *l;
	char **strings, *mozilla_ignore_list;
	int i = 0;

	hosts = eel_gconf_get_string_list (entry->key);

	strings = g_new (gchar*, g_slist_length (hosts) + 1);
	for (l = hosts; l != NULL; l = l->next)
	{
		char *item = (char *) l->data;

		if (item && item[0] != '\0')
		{
			strings[i] = item;
			i++;
		}
	}
	strings[i] = NULL;

	mozilla_ignore_list = g_strjoinv (", ", strings);
	mozilla_prefs_set_string (MOZILLA_PREF_NO_PROXIES,
				  mozilla_ignore_list);

	g_free (mozilla_ignore_list);
	g_free (strings);
	g_slist_foreach (hosts, (GFunc) g_free, NULL);
	g_slist_free (hosts);
}
