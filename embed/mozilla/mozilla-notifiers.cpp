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

#include "config.h"

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "mozilla-notifiers.h"
#include "eel-gconf-extensions.h"
#include "mozilla-prefs.h"
#include "MozRegisterComponents.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "mozilla-i18n.h"

#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <libgnome/gnome-i18n.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include "nsBuildID.h"

static void
mozilla_own_colors_notifier(GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    gpointer user_data);
static void
mozilla_own_fonts_notifier(GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   gpointer user_data);

static void
mozilla_animate_notifier(GConfClient *client,
			 guint cnxn_id,
			 GConfEntry *entry,
			 gpointer user_data);
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
			      gpointer user_data);

static void
mozilla_language_notifier(GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  gpointer user_data);

static void
mozilla_autodetect_charset_notifier(GConfClient *client,
				    guint cnxn_id,
				    GConfEntry *entry,
				    EphyEmbedSingle *single);

static void
mozilla_default_font_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer user_data);

static void
mozilla_proxy_mode_notifier (GConfClient *client,
			     guint cnxn_id,
			     GConfEntry *entry,
			     char *pref);
static void
mozilla_proxy_autoconfig_notifier (GConfClient *client,
			           guint cnxn_id,
			           GConfEntry *entry,
			           char *pref);

static void
mozilla_user_agent_notifier(GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    gpointer user_data);

static void 
mozilla_default_charset_notifier (GConfClient *client,
				  guint cnxn_id,
				  GConfEntry *entry,
				  EphyEmbedSingle *single);
static void
mozilla_socks_version_notifier (GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data);

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
	{ CONF_FILTERING_JAVA_ENABLED, BOOL_PREF, "security.enable_java"},
	{ CONF_FILTERING_JAVASCRIPT_ENABLED, BOOL_PREF, "javascript.enabled"},
	{ CONF_FILTERING_IMAGE_LOADING_TYPE, INT_PREF, "network.image.imageBehavior"},
	{ CONF_RENDERING_BG_COLOR, STRING_PREF, "browser.display.background_color"},
	{ CONF_RENDERING_TEXT_COLOR, STRING_PREF, "browser.display.foreground_color"},
	{ CONF_RENDERING_UNVISITED_LINKS, STRING_PREF, "browser.anchor_color"},
	{ CONF_RENDERING_VISITED_LINKS, STRING_PREF, "browser.visited_color"},
	{ CONF_RENDERING_UNDERLINE_LINKS, BOOL_PREF, "browser.underline_anchors"},
	{ CONF_NETWORK_PROXY_AUTO_URL, STRING_PREF, "network.proxy.autoconfig_url"},
	{ CONF_NETWORK_HTTP_PROXY, STRING_PREF, "network.proxy.http"},
	{ CONF_NETWORK_FTP_PROXY, STRING_PREF, "network.proxy.ftp"},
	{ CONF_NETWORK_SSL_PROXY, STRING_PREF, "network.proxy.ssl"},
	{ CONF_NETWORK_SOCKS_PROXY, STRING_PREF, "network.proxy.socks"},
	{ CONF_NETWORK_HTTP_PROXY_PORT, INT_PREF, "network.proxy.http_port"},
	{ CONF_NETWORK_FTP_PROXY_PORT, INT_PREF, "network.proxy.ftp_port"},
	{ CONF_NETWORK_SSL_PROXY_PORT, INT_PREF, "network.proxy.ssl_port"},
	{ CONF_NETWORK_SOCKS_PROXY_PORT, INT_PREF, "network.proxy.socks_port"},
	{ CONF_NETWORK_NO_PROXIES_FOR, STRING_PREF, "network.proxy.no_proxies_on"},
	{ CONF_NETWORK_MEMORY_CACHE, INT_PREF, "browser.cache.memory.capacity"},
	{ CONF_NETWORK_DISK_CACHE, INT_PREF, "browser.cache.disk.capacity"},
	{ CONF_NETWORK_CACHE_COMPARE, INT_PREF, "browser.cache.check_doc_frequency"},
	{ CONF_PERSISTENT_COOKIE_WARN, BOOL_PREF, "network.cookie.warnAboutCookies"},
	{ CONF_PERSISTENT_COOKIES_BEHAVIOR, INT_PREF, "network.cookie.cookieBehavior"},
	{ CONF_PERSISTENT_COOKIE_LIFETIME, BOOL_PREF, "network.cookie.lifetime.enabled"},
	{ CONF_PERSISTENT_PASSWORDS_SAVE, BOOL_PREF, "signon.rememberSignons"},
	{ CONF_RENDERING_USE_SYSTEM_COLORS, BOOL_PREF, "browser.display.use_system_colors"},
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
	{ CONF_FILTERING_ANIMATE_TYPE, 
	  (GConfClientNotifyFunc) mozilla_animate_notifier },
	{ CONF_RENDERING_USE_OWN_COLORS, 
	  (GConfClientNotifyFunc) mozilla_own_colors_notifier },
	{ CONF_RENDERING_USE_OWN_FONTS, 
	  (GConfClientNotifyFunc) mozilla_own_fonts_notifier },
	{ CONF_FILTERING_ALLOW_POPUPS, 
	  (GConfClientNotifyFunc) mozilla_allow_popups_notifier },
	{ CONF_LANGUAGE_DEFAULT_CHARSET, 
	  (GConfClientNotifyFunc) mozilla_default_charset_notifier },
	{ CONF_RENDERING_LANGUAGE, 
	  (GConfClientNotifyFunc) mozilla_language_notifier },
	{ CONF_LANGUAGE_AUTODETECT_CHARSET, 
	  (GConfClientNotifyFunc) mozilla_autodetect_charset_notifier },
	{ CONF_RENDERING_DEFAULT_FONT, 
	  (GConfClientNotifyFunc) mozilla_default_font_notifier },
	{ CONF_NETWORK_SOCKS_PROXY_VERSION, 
	  (GConfClientNotifyFunc) mozilla_socks_version_notifier },
	{ CONF_NETWORK_PROXY_MODE,
	  (GConfClientNotifyFunc) mozilla_proxy_mode_notifier },
	{ CONF_NETWORK_PROXY_AUTO_URL,
	  (GConfClientNotifyFunc) mozilla_proxy_autoconfig_notifier },
	{NULL, NULL}
};

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
		             char *pref)
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
		       		   char *pref)
{
	EphyEmbedSingle *single;
	single = ephy_embed_shell_get_embed_single (embed_shell);
	ephy_embed_single_load_proxy_autoconf 
		(single, gconf_value_get_string(entry->value));
}

void 
mozilla_notifiers_init(EphyEmbedSingle *single) 
{
	int i;

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
		
		ephy_notification_add(
			conversion_table[i].gconf_key,
			func,
			(gpointer)conversion_table[i].mozilla_key,
			&mozilla_notifiers);
	}

	for (i = 0; custom_notifiers[i].gconf_key != NULL; i++)
	{
			ephy_notification_add(
				custom_notifiers[i].gconf_key,
				custom_notifiers[i].func,
				(gpointer)single,
				&mozilla_notifiers);
	}

	/* fonts notifiers */
	for (i = 0; i < LANG_ENC_NUM; i++)
	{
		int k;
		char *types [] = { "serif", "sans-serif", "cursive", "fantasy", "monospace" };
		char key[255];
		char *info;
		
		for (k = 0; k < 5; k++)
		{
			info = g_strconcat (types[k], ".", lang_encode_item[i], NULL);
			
			sprintf (key, "%s_%s_%s", CONF_RENDERING_FONT, 
			 	 types[k], 
                 	 	 lang_encode_item[i]);
			ephy_notification_add (key,
						 (GConfClientNotifyFunc)mozilla_font_notifier,
						  info,
						  &mozilla_notifiers);
			font_infos = g_list_append (font_infos, info);
		}

		sprintf (key, "%s_%s", CONF_RENDERING_FONT_MIN_SIZE, lang_encode_item[i]);
		info = g_strconcat ("minimum-size", ".", lang_encode_item[i], NULL);
		ephy_notification_add (key,
					 (GConfClientNotifyFunc)mozilla_font_size_notifier,
					 info,
				         &mozilla_notifiers);
		font_infos = g_list_append (font_infos, info);

		sprintf (key, "%s_%s", CONF_RENDERING_FONT_FIXED_SIZE, lang_encode_item[i]);
		info = g_strconcat ("size.fixed", ".", lang_encode_item[i], NULL);
		ephy_notification_add (key,
					 (GConfClientNotifyFunc)mozilla_font_size_notifier,
					 info,
				         &mozilla_notifiers);
		font_infos = g_list_append (font_infos, info);

		sprintf (key, "%s_%s", CONF_RENDERING_FONT_VAR_SIZE, lang_encode_item[i]);
		info = g_strconcat ("size.variable", ".", lang_encode_item[i], NULL);
		ephy_notification_add (key,
					 (GConfClientNotifyFunc)mozilla_font_size_notifier,
					 info,
				         &mozilla_notifiers);
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

void 
mozilla_notifiers_set_defaults(void) 
{
	GConfClient* client = eel_gconf_client_get_global();
	GConfValue* value;
	int i;

	for (i = 0; conversion_table[i].gconf_key != NULL; i++)
	{
		value = gconf_client_get 
			(client, conversion_table[i].gconf_key, NULL);
		if (value)
		{
			gconf_client_set (client, 
					  conversion_table[i].gconf_key,
					  value, NULL);
			gconf_value_free (value);
		}
	}

	for (i = 0; custom_notifiers[i].gconf_key != NULL; i++)
	{
		value = gconf_client_get 
			(client, custom_notifiers[i].gconf_key, NULL);
		if (value)
		{
			gconf_client_set (client, 
					  custom_notifiers[i].gconf_key,
					  value, NULL);
			gconf_value_free (value);
		}
	}
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
mozilla_default_charset_notifier(GConfClient *client,
				 guint cnxn_id,
				 GConfEntry *entry,
				 EphyEmbedSingle *single)
{
	/* FIXME */
}


static void
mozilla_own_colors_notifier(GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    gpointer user_data)
{
	mozilla_prefs_set_boolean("browser.display.use_document_colors",
				       !gconf_value_get_bool(entry->value));
}

static void
mozilla_own_fonts_notifier(GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   gpointer user_data)
{
	mozilla_prefs_set_int("browser.display.use_document_fonts",
				   !gconf_value_get_bool(entry->value));
}

static void
mozilla_animate_notifier(GConfClient *client,
			 guint cnxn_id,
			 GConfEntry *entry,
			 gpointer user_data)
{
	static const gchar *type[] =
	{
		"normal",
		"once",
		"none"
	};

	mozilla_prefs_set_string ("image.animation_mode",
				  type[gconf_value_get_int(entry->value)]);
}

static void
mozilla_allow_popups_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer user_data)
{
	gboolean new_val = eel_gconf_get_boolean(CONF_FILTERING_ALLOW_POPUPS);
	mozilla_prefs_set_boolean ("dom.disable_open_during_load", 
					!new_val);
}

static void
mozilla_language_notifier(GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  gpointer user_data)
{
	gchar *languages;
	GSList *language_list ,*cur_lang_list;
	
	language_list = eel_gconf_get_string_list (CONF_RENDERING_LANGUAGE);

	languages = NULL;
	cur_lang_list = language_list;
	while (cur_lang_list != NULL) {
		char *lang, *tmp;

		lang = g_strdup((char *)cur_lang_list->data);
		
		if (languages == NULL)
			languages = lang;
		else {
			tmp = languages;
			languages = g_strconcat(languages, ",", lang, NULL);
			g_free(lang);
			g_free(tmp);
		}
		g_free(cur_lang_list->data);
		cur_lang_list = cur_lang_list->next;
	}

	if (languages == NULL)
	{
		languages = g_strdup ("");
	}

	mozilla_prefs_set_string ("intl.accept_languages", languages);
	g_free (languages);

	g_slist_free(language_list);
}

static char *autodetect_charset_prefs[] =
{
        "",
        "zh_parallel_state_machine",
        "cjk_parallel_state_machine",
        "ja_parallel_state_machine",
        "ko_parallel_state_machine",
        "ruprob",
        "zhcn_parallel_state_machine",
        "zhtw_parallel_state_machine",
        "ukprob"
};

static void
mozilla_autodetect_charset_notifier(GConfClient *client,
				    guint cnxn_id,
				    GConfEntry *entry,
				    EphyEmbedSingle *single)
{
	int charset = eel_gconf_get_integer (CONF_LANGUAGE_AUTODETECT_CHARSET);
	if (charset < 0 || 
	    charset >= (int)(sizeof(autodetect_charset_prefs)
		             / sizeof(autodetect_charset_prefs[0])))
	{
		g_warning ("mozilla_autodetect_charset_notifier: "
			   "unsupported value: %d", charset);
		return;
	}
	mozilla_prefs_set_string ("intl.charset.detector", 
				  autodetect_charset_prefs[charset]);
}

static void
mozilla_default_font_notifier(GConfClient *client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer user_data)
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
			     gpointer user_data)
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

static void
mozilla_socks_version_notifier (GConfClient *client,
				guint cnxn_id,
				GConfEntry *entry,
				gpointer user_data)
{
	int version;
	version = gconf_value_get_int(entry->value) + 4;
	mozilla_prefs_set_int ("network.proxy.socks_version", 
			       version);
}
