/*
 *  Copyright (C) 2000 Nate Case 
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
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
#include <libgnome/gnome-i18n.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include "nsBuildID.h"
#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"

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
mozilla_default_font_notifier(GConfClient *client,
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
mozilla_user_agent_notifier(GConfClient *client,
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
	{ CONF_SECURITY_JAVA_ENABLED, BOOL_PREF, "security.enable_java"},
	{ CONF_SECURITY_JAVASCRIPT_ENABLED, BOOL_PREF, "javascript.enabled"},
	{ CONF_RENDERING_UNDERLINE_LINKS, BOOL_PREF, "browser.underline_anchors"},
	{ CONF_NETWORK_PROXY_AUTO_URL, STRING_PREF, "network.proxy.autoconfig_url"},
	{ CONF_NETWORK_HTTP_PROXY, STRING_PREF, "network.proxy.http"},
	{ CONF_NETWORK_FTP_PROXY, STRING_PREF, "network.proxy.ftp"},
	{ CONF_NETWORK_SSL_PROXY, STRING_PREF, "network.proxy.ssl"},
	{ CONF_NETWORK_HTTP_PROXY_PORT, INT_PREF, "network.proxy.http_port"},
	{ CONF_NETWORK_FTP_PROXY_PORT, INT_PREF, "network.proxy.ftp_port"},
	{ CONF_NETWORK_SSL_PROXY_PORT, INT_PREF, "network.proxy.ssl_port"},
	{ CONF_NETWORK_CACHE_COMPARE, INT_PREF, "browser.cache.check_doc_frequency"},
	{ CONF_SECURITY_COOKIES_ACCEPT, BOOL_PREF, "network.cookie.warnAboutCookies"},
	{ CONF_LANGUAGE_DEFAULT_ENCODING, STRING_PREF, "intl.charset.default" },
	{ CONF_LANGUAGE_AUTODETECT_ENCODING, STRING_PREF, "intl.charset.detector" },
	{ NULL, 0, NULL }
};
 
static const struct 
{
	const char *gconf_key;
	GConfClientNotifyFunc func;
}
custom_notifiers [] =
{
	{ CONF_NETWORK_USER_AGENT, 
	  (GConfClientNotifyFunc) mozilla_user_agent_notifier },
	{ CONF_RENDERING_USE_OWN_COLORS, 
	  (GConfClientNotifyFunc) mozilla_own_colors_notifier },
	{ CONF_RENDERING_USE_OWN_FONTS, 
	  (GConfClientNotifyFunc) mozilla_own_fonts_notifier },
	{ CONF_SECURITY_ALLOW_POPUPS, 
	  (GConfClientNotifyFunc) mozilla_allow_popups_notifier },
	{ CONF_RENDERING_LANGUAGE, 
	  (GConfClientNotifyFunc) mozilla_language_notifier },
	{ CONF_RENDERING_DEFAULT_FONT, 
	  (GConfClientNotifyFunc) mozilla_default_font_notifier },
	{ CONF_NETWORK_PROXY_MODE,
	  (GConfClientNotifyFunc) mozilla_proxy_mode_notifier },
	{ CONF_NETWORK_PROXY_AUTO_URL,
	  (GConfClientNotifyFunc) mozilla_proxy_autoconfig_notifier },
	{ CONF_NETWORK_CACHE_SIZE,
          (GConfClientNotifyFunc) mozilla_cache_size_notifier },
	{NULL, NULL}
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

	cache_size = gconf_value_get_int(entry->value) * 1024;

	mozilla_prefs_set_int ("browser.cache.disk.capacity", cache_size);
}

static void
mozilla_font_size_notifier (GConfClient *client,
		            guint cnxn_id,
		            GConfEntry *entry,
		            char *pref)
{
	char key[255];
	
	sprintf (key, "font.%s", pref);

	mozilla_prefs_set_int (key, gconf_value_get_int(entry->value));
}

static void
mozilla_proxy_mode_notifier (GConfClient *client,
		             guint cnxn_id,
		             GConfEntry *entry,
		             EphyEmbedSingle *single)
{
	const char *mode;
	int mozilla_mode = 0;
	
	mode = gconf_value_get_string(entry->value);
	
	if (strcmp (mode, "manual") == 0)
	{
		mozilla_mode = 1;
	}
	else if (strcmp (mode, "auto") == 0)
	{
		mozilla_mode = 2;
	}

	mozilla_prefs_set_int ("network.proxy.type", mozilla_mode);
}

static void
mozilla_font_notifier (GConfClient *client,
		       guint cnxn_id,
		       GConfEntry *entry,
		       char *pref)
{
	char key[255];
	
	sprintf (key, "font.name.%s", pref);

	mozilla_prefs_set_string (key, gconf_value_get_string(entry->value));
}

static void
mozilla_proxy_autoconfig_notifier (GConfClient *client,
		       	           guint cnxn_id,
		       		   GConfEntry *entry,
		       		   EphyEmbedSingle *single)
{
	ephy_embed_single_load_proxy_autoconf 
		(single, gconf_value_get_string(entry->value));
}

static void
add_notification_and_notify (GConfClient		*client,
			     const char 		*key,
			     GConfClientNotifyFunc	 func,
			     gpointer			 user_data)
{
	GConfEntry *entry;
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

	entry = gconf_client_get_entry (client, key, NULL, TRUE, &error);
	if (eel_gconf_handle_error (&error))
	{
		return;
	}
	g_return_if_fail (entry != NULL);

	if (entry->value != NULL)
	{
		func (client, cnxn_id, entry, user_data);
	}
	gconf_entry_free (entry);
}

void 
mozilla_notifiers_init(EphyEmbedSingle *single) 
{
	GConfClient *client = eel_gconf_client_get_global ();
	guint i;
	guint n_fonts_languages;
	const FontsLanguageInfo *fonts_language;

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
	n_fonts_languages = ephy_langs_get_n_font_languages ();
	fonts_language = ephy_langs_get_font_languages ();
	for (i = 0; i < n_fonts_languages; i++)
	{
		guint k;
		char *types [] = { "serif", "sans-serif", "cursive", "fantasy", "monospace" };
		char key[255];
		char *info;
		
		for (k = 0; k < G_N_ELEMENTS (types); k++)
		{
			info = g_strconcat (types[k], ".", fonts_language[i].code, NULL);
			
			snprintf (key, 255, "%s_%s_%s", CONF_RENDERING_FONT, 
			 	 types[k], 
                 	 	 fonts_language[i].code);
			add_notification_and_notify (client, key,
						     (GConfClientNotifyFunc)mozilla_font_notifier,
						     info);
			font_infos = g_list_append (font_infos, info);
		}

		snprintf (key, 255, "%s_%s", CONF_RENDERING_FONT_MIN_SIZE, fonts_language[i].code);
		info = g_strconcat ("minimum-size", ".", fonts_language[i].code, NULL);
		add_notification_and_notify (client, key,
					     (GConfClientNotifyFunc)mozilla_font_size_notifier,
					     info);
		font_infos = g_list_append (font_infos, info);

		snprintf (key, 255, "%s_%s", CONF_RENDERING_FONT_FIXED_SIZE, fonts_language[i].code);
		info = g_strconcat ("size.fixed", ".", fonts_language[i].code, NULL);
		add_notification_and_notify (client, key,
					     (GConfClientNotifyFunc)mozilla_font_size_notifier,
					     info);
		font_infos = g_list_append (font_infos, info);

		snprintf (key, 255, "%s_%s", CONF_RENDERING_FONT_VAR_SIZE, fonts_language[i].code);
		info = g_strconcat ("size.variable", ".", fonts_language[i].code, NULL);
		add_notification_and_notify (client, key,
					     (GConfClientNotifyFunc)mozilla_font_size_notifier,
					     info);
		font_infos = g_list_append (font_infos, info);		
	}
}

void 
mozilla_notifiers_free (void)
{
	GList *l;
	
	ephy_notification_remove (&mozilla_notifiers);

	for (l = font_infos; l != NULL; l = l->next)
	{
		g_free (l->data);
	}
	
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
	const gchar *value;

	/* determine type of gconf key, in order of likelyhood */
	switch (entry->value->type)
	{
		case GCONF_VALUE_STRING:
			value = gconf_value_get_string(entry->value);
			if (value)
			{
				mozilla_prefs_set_string
					(pref_name,
					 gconf_value_get_string(entry->value));
			}
			break;

		default:	
			g_warning("Unsupported variable type");
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
	/* determine type of gconf key, in order of likelyhood */
	switch (entry->value->type)
	{
		case GCONF_VALUE_INT:	mozilla_prefs_set_int(pref_name,
						gconf_value_get_int(entry->value));
					break;
		case GCONF_VALUE_BOOL:	mozilla_prefs_set_boolean(pref_name,
						gconf_value_get_bool(entry->value));
					break;
		case GCONF_VALUE_STRING:	mozilla_prefs_set_string(pref_name,
						gconf_value_get_string(entry->value));
					break;
		default:	g_warning("Unsupported variable type");
	}
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
	/* determine type of gconf key, in order of likelyhood */
	switch (entry->value->type)
	{
		case GCONF_VALUE_BOOL:	mozilla_prefs_set_boolean(pref_name,
						gconf_value_get_bool(entry->value));
					break;
		case GCONF_VALUE_INT:	mozilla_prefs_set_int(pref_name,
						gconf_value_get_int(entry->value));
					break;
		default:	g_warning("Unsupported variable type");
	}
}

static void
mozilla_own_colors_notifier(GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyEmbedSingle *single)
{
	mozilla_prefs_set_boolean("browser.display.use_document_colors",
				       !gconf_value_get_bool(entry->value));
}

static void
mozilla_own_fonts_notifier(GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   EphyEmbedSingle *single)
{
	mozilla_prefs_set_int("browser.display.use_document_fonts",
				   !gconf_value_get_bool(entry->value));
}

static void
mozilla_allow_popups_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      EphyEmbedSingle *single)
{
	gboolean new_val = eel_gconf_get_boolean(CONF_SECURITY_ALLOW_POPUPS);
	mozilla_prefs_set_boolean ("dom.disable_open_during_load", 
					!new_val);
}

static char *
get_system_language ()
{
	const GList *sys_langs;

	sys_langs = gnome_i18n_get_language_list ("LC_MESSAGES");

	if (sys_langs)
	{
		char *lang = (char *)sys_langs->data;

		/* FIXME this probably need to be smarter */
		if (strcmp (lang, "C") != 0)
		{
			return g_strndup (lang, 2);
		}
	}

	return NULL;
}

static void
mozilla_language_notifier(GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  EphyEmbedSingle *single)
{
	GSList *languages, *l;
	GString *result;

	languages = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);
	g_return_if_fail (languages != NULL);

	result = g_string_new ("");

	for (l = languages; l != NULL; l = l->next)
	{
		char *lang = (char *)l->data;

		if (strcmp (lang, "system") == 0)
		{
			char *sys_lang;
			
			sys_lang = get_system_language ();
			if (sys_lang)
			{
				g_string_append (result, sys_lang);
				g_free (sys_lang);
			}
		}
		else
		{
			g_string_append (result, (char *)l->data);
		}

		if (l->next) g_string_append (result, ",");
	}
	
	mozilla_prefs_set_string ("intl.accept_languages", result->str);

	g_string_free (result, TRUE);
	
	g_slist_foreach (languages, (GFunc) g_free, NULL);
	g_slist_free (languages);
}

static void
mozilla_default_font_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      EphyEmbedSingle *single)
{
	const gchar *font_types [] = {"serif","sans-serif"};
	int default_font;

	default_font = eel_gconf_get_integer (CONF_RENDERING_DEFAULT_FONT);
	if (default_font < 0 || 
	    default_font >= (int)(sizeof(font_types) / sizeof(font_types[0])))
	{
		g_warning ("mozilla_default_font_notifier: "
			   "unsupported value: %d", default_font);
		return;
	}
	mozilla_prefs_set_string ("font.default", font_types[default_font]);
}

static void
mozilla_prefs_set_user_agent ()
{
        static gchar *default_user_agent = NULL;
        gchar *value;
        gchar *user_agent = NULL;
        struct utsname name;
        gchar *system;

        if (!default_user_agent)
        {
                if (uname (&name) == 0)
                {
                        system = g_strdup_printf ("%s %s",
                                                  name.sysname, 
                                                  name.machine);
                }
                else
                {
                        system = g_strdup ("Unknown");
                }
                
                default_user_agent = g_strdup_printf
                        ("Mozilla/5.0 (X11; U; %s) Gecko/%d Epiphany/" VERSION, 
                         system,
                         NS_BUILD_ID/100);

                g_free (system);
        }

        value = eel_gconf_get_string (CONF_NETWORK_USER_AGENT);
        
        /* now, build a valid user agent string */
        if (!value || !strcmp ("", value) 
                   || !strcmp ("default", value) 
                   || !strcmp ("Ephy", value)
                   || !strcmp (_("Default (recommended)"), value)
                   || !strcmp ("Default (recommended)", value))
        {
                user_agent = g_strdup (default_user_agent);
        }
        else
        {
                user_agent = g_strdup (value);
        }

	mozilla_prefs_set_string ("general.useragent.override", user_agent);
	g_free (user_agent);
}

static void
mozilla_user_agent_notifier (GConfClient *client,
			     guint cnxn_id,
			     GConfEntry *entry,
			     EphyEmbedSingle *single)
{
	switch (entry->value->type)
	{
		case GCONF_VALUE_STRING:
			mozilla_prefs_set_user_agent ();
			break;

		default:	
			g_warning ("Unsupported variable type");
			break;
	}
}
