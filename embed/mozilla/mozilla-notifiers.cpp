/*
*  Copyright (C) 2000 Nate Case
*  Copyright (C) 2000-2004 Marco Pesenti Gritti
*  Copyright (C) 2003, 2004 Christian Persch
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

#include "config.h"

#include "mozilla-notifiers.h"

#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-langs.h"
#include "ephy-node.h"
#include "ephy-encodings.h"
#include "ephy-embed-shell.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtksettings.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <nsCOMPtr.h>
#include <nsIServiceManager.h>
#include <nsIPrefService.h>

/* define to migrate epiphany 1.0 font preferences */
#define MIGRATE_PIXEL_SIZE

/* Keeps the list of the notifiers we installed for mozilla prefs */
/* to be able to remove them when exiting */
GList *notifiers = NULL;

typedef struct
{
	char *gconf_key;
	char *mozilla_pref;
	PrefValueTransformFunc func;
	gpointer user_data;
	guint cnxn_id;
} PrefData;

static void
free_pref_data (PrefData *data)
{
	g_free (data->gconf_key);
	g_free (data->mozilla_pref);
	g_free (data);
}

static gboolean
transform_accept_languages_list (GConfValue *gcvalue,
				 GValue *value,
				 gpointer user_data)
{
	GArray *array;
	GSList *languages, *l;
	char **langs;

	if (gcvalue->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (gcvalue) != GCONF_VALUE_STRING) return FALSE;

	languages = gconf_value_get_list (gcvalue);

	array = g_array_new (TRUE, FALSE, sizeof (char *));

	for (l = languages; l != NULL; l = l->next)
	{
		const char *lang;

		lang = gconf_value_get_string ((GConfValue *) l->data);

		if (lang != NULL && strcmp (lang, "system") == 0)
		{
			ephy_langs_append_languages (array);
		}
		else if (lang != NULL && lang[0] != '\0')
		{
			char *str;
			str = g_ascii_strdown (lang, -1);
			g_array_append_val (array, str);
		}
	}

	ephy_langs_sanitise (array);

	langs = (char **) g_array_free (array, FALSE);

	g_value_init (value, G_TYPE_STRING);
	g_value_take_string (value, g_strjoinv (",", langs));

	g_strfreev (langs);

	return TRUE;
}

static gboolean
transform_cache_size (GConfValue *gcvalue,
		      GValue *value,
		      gpointer user_data)
{
	if (gcvalue->type != GCONF_VALUE_INT) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, gconf_value_get_int (gcvalue) * 1024);

	return TRUE;
}

static gboolean
transform_cookies_accept_mode (GConfValue *gcvalue,
			       GValue *value,
			       gpointer user_data)

{
	const char *mode;
	int mozilla_mode = 0;

	if (gcvalue->type != GCONF_VALUE_STRING) return FALSE;

	mode = gconf_value_get_string (gcvalue);
	if (mode == NULL) return FALSE;

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

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, mozilla_mode);

	return TRUE;
}

static gboolean
transform_encoding (GConfValue *gcvalue,
		    GValue *value,
		    gpointer user_data)
{
	EphyEncodings *encodings;
	EphyNode *node;
	const char *code;
	gboolean is_autodetector;

	if (gcvalue->type != GCONF_VALUE_STRING) return FALSE;

	code = gconf_value_get_string (gcvalue);
	if (code == NULL) return FALSE;

	encodings = EPHY_ENCODINGS (ephy_embed_shell_get_encodings (embed_shell));
	node = ephy_encodings_get_node (encodings, code, FALSE);
	if (node == NULL) return FALSE;

	is_autodetector = ephy_node_get_property_boolean (node, EPHY_NODE_ENCODING_PROP_IS_AUTODETECTOR);
	if (is_autodetector != GPOINTER_TO_INT (user_data)) return FALSE;

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, code);

	return TRUE;
}

static gboolean
transform_font_size (GConfValue *gcvalue,
		     GValue *value,
		     gpointer user_data)
{
	if (gcvalue->type != GCONF_VALUE_INT) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, MAX (1, gconf_value_get_int (gcvalue)));

	return TRUE;
}

static gboolean
transform_proxy_ignore_list (GConfValue *gcvalue,
			     GValue *value,
			     gpointer user_data)
{
	GArray *array;
	GSList *hosts, *l;
	char **strings;

	if (gcvalue->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (gcvalue) != GCONF_VALUE_STRING) return FALSE;

	hosts = gconf_value_get_list (gcvalue);

	array = g_array_new (TRUE, FALSE, sizeof (char *));

	for (l = hosts; l != NULL; l = l->next)
	{
		const char *host;

		host = gconf_value_get_string ((GConfValue *) l->data);

		if (host != NULL && host[0] != '\0')
		{
			g_array_append_val (array, host);
		}
	}

	strings = (char **) g_array_free (array, FALSE);

	g_value_init (value, G_TYPE_STRING);
	g_value_take_string (value, g_strjoinv (",", strings));

	/* the strings themselves are const */
	g_free (strings);

	return TRUE;
}

static gboolean
transform_proxy_mode (GConfValue *gcvalue,
		      GValue *value,
		      gpointer user_data)
{
	const char *mode;
	int mozilla_mode = 0;

	if (gcvalue->type != GCONF_VALUE_STRING) return FALSE;

	mode = gconf_value_get_string (gcvalue);
	if (mode == NULL) return FALSE;

	if (strcmp (mode, "manual") == 0)
	{
		mozilla_mode = 1;
	}
	else if (strcmp (mode, "auto") == 0)
	{
		mozilla_mode = 2;
	}

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, mozilla_mode);

	return TRUE;
}

static gboolean
transform_use_own_fonts (GConfValue *gcvalue,
			 GValue *value,
			 gpointer user_data)
{
	if (gcvalue->type != GCONF_VALUE_BOOL) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, gconf_value_get_bool (gcvalue) ? 0 : 1);

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_bool (GConfValue *gcvalue,
				 GValue *value,
				 gpointer user_data)
{
	if (gcvalue->type != GCONF_VALUE_BOOL) return FALSE;

	g_value_init (value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, gconf_value_get_bool (gcvalue));

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_bool_invert (GConfValue *gcvalue,
					GValue *value,
					gpointer user_data)
{
	if (gcvalue->type != GCONF_VALUE_BOOL) return FALSE;

	g_value_init (value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, !gconf_value_get_bool (gcvalue));

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_int (GConfValue *gcvalue,
				GValue *value,
				gpointer user_data)
{
	if (gcvalue->type != GCONF_VALUE_INT) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, gconf_value_get_int (gcvalue));

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_string (GConfValue *gcvalue,
				   GValue *value,
				   gpointer user_data)
{
	const char *str;

	if (gcvalue->type != GCONF_VALUE_STRING) return FALSE;

	str = gconf_value_get_string (gcvalue);
	if (str == NULL) return FALSE;

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, str);

	return TRUE;
}

static const PrefData notifier_entries[] =
{
	{ CONF_BROWSE_WITH_CARET,
	  "accessibility.browsewithcaret",
	  mozilla_notifier_transform_bool },
	{ CONF_NETWORK_CACHE_SIZE,
	  "browser.cache.disk.capacity",
	  transform_cache_size },
	{ CONF_RENDERING_USE_OWN_COLORS,
	  "browser.display.use_document_colors",
	  mozilla_notifier_transform_bool_invert },
	{ CONF_RENDERING_USE_OWN_FONTS,
	  "browser.display.use_document_fonts",
	  transform_use_own_fonts },
	{ CONF_SECURITY_ALLOW_POPUPS,
	  "dom.disable_open_during_load",
	  mozilla_notifier_transform_bool_invert },
	{ CONF_RENDERING_LANGUAGE,
	  "intl.accept_languages",
	  transform_accept_languages_list },
	{ CONF_LANGUAGE_DEFAULT_ENCODING,
	  "intl.charset.default",
	  transform_encoding,
	  GINT_TO_POINTER (FALSE) },
	{ CONF_LANGUAGE_AUTODETECT_ENCODING,
	  "intl.charset.detector",
	  transform_encoding,
	  GINT_TO_POINTER (TRUE) },
	{ CONF_SECURITY_JAVA_ENABLED,
	  "security.enable_java",
	  mozilla_notifier_transform_bool },
	{ CONF_SECURITY_JAVASCRIPT_ENABLED,
	  "javascript.enabled",
	  mozilla_notifier_transform_bool },
	{ CONF_NETWORK_PROXY_AUTO_URL,
	  "network.proxy.autoconfig_url",
	  mozilla_notifier_transform_string },
	{ CONF_NETWORK_HTTP_PROXY,
	  "network.proxy.http",
	  mozilla_notifier_transform_string },
	{ CONF_NETWORK_HTTP_PROXY_PORT,
	  "network.proxy.http_port",
	  mozilla_notifier_transform_int },
	{ CONF_NETWORK_FTP_PROXY,
	  "network.proxy.ftp",
	  mozilla_notifier_transform_string },
	{ CONF_NETWORK_FTP_PROXY_PORT,
	  "network.proxy.ftp_port",
	  mozilla_notifier_transform_int },
	{ CONF_NETWORK_SSL_PROXY,
	  "network.proxy.ssl",
	  mozilla_notifier_transform_string },
	{ CONF_NETWORK_SSL_PROXY_PORT,
	  "network.proxy.ssl_port",
	  mozilla_notifier_transform_int },
	{ CONF_NETWORK_SOCKS_PROXY,
	  "network.proxy.socks",
	  mozilla_notifier_transform_string },
	{ CONF_NETWORK_SOCKS_PROXY_PORT,
	  "network.proxy.socks_port",
	  mozilla_notifier_transform_int },
	{ CONF_NETWORK_PROXY_IGNORE_HOSTS,
	  "network.proxy.no_proxies_on",
	  transform_proxy_ignore_list },
	{ CONF_NETWORK_PROXY_MODE,
	  "network.proxy.type",
	  transform_proxy_mode },
	{ CONF_SECURITY_COOKIES_ACCEPT,
	  "network.cookie.cookieBehavior",
	  transform_cookies_accept_mode },
};

gboolean
mozilla_pref_set (const char *pref,
		  const GValue *value)
{
	g_return_val_if_fail (pref != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	nsCOMPtr<nsIPrefService> prefService
		(do_GetService (NS_PREFSERVICE_CONTRACTID));
	NS_ENSURE_TRUE (prefService, FALSE);

	nsCOMPtr<nsIPrefBranch> prefBranch;
	prefService->GetBranch ("", getter_AddRefs (prefBranch));
	NS_ENSURE_TRUE (prefBranch, FALSE);

	nsresult rv;
	switch (G_VALUE_TYPE (value))
	{
		case G_TYPE_INT:
			rv = prefBranch->SetIntPref (pref, g_value_get_int (value));
			break;
		case G_TYPE_BOOLEAN:
			rv = prefBranch->SetBoolPref (pref, g_value_get_boolean (value));
			break;
		case G_TYPE_STRING:
			rv = prefBranch->SetCharPref (pref, g_value_get_string (value));
			break;
		default:
			g_return_val_if_reached (FALSE);
			rv = NS_ERROR_FAILURE;
			break;
	}

	return NS_SUCCEEDED (rv) != PR_FALSE;
}

static void
notify_cb (GConfClient *client,
	   guint cnxn_id,
	   GConfEntry *entry,
	   PrefData *data)
{
	GConfValue *gcvalue;
	GValue value = { 0, };

	g_return_if_fail (entry != NULL);
	g_return_if_fail (data != NULL);

	gcvalue = gconf_entry_get_value (entry);
	/* happens on initial notify if the key doesn't exist */
	if (gcvalue == NULL) return;

	if (data->func (gcvalue, &value, data->user_data))
	{
		mozilla_pref_set (data->mozilla_pref, &value);
		g_value_unset (&value);
	}
}

extern "C" guint
mozilla_notifier_add (const char *gconf_key,
		      const char *mozilla_pref,
		      PrefValueTransformFunc func,
		      gpointer user_data)
{
	GConfClient *client;
	PrefData *data;
	GError *error = NULL;
	guint cnxn_id;

	g_return_val_if_fail (gconf_key != NULL, 0);
	g_return_val_if_fail (mozilla_pref != NULL, 0);
	g_return_val_if_fail (func, 0);

	client = eel_gconf_client_get_global ();
	g_return_val_if_fail (client != NULL, 0);

	data = g_new (PrefData, 1);
	data->gconf_key = g_strdup (gconf_key);
	data->mozilla_pref = g_strdup (mozilla_pref);
	data->func = func;
	data->user_data = user_data;

	cnxn_id = gconf_client_notify_add (client, gconf_key,
					   (GConfClientNotifyFunc) notify_cb,
					   data, (GFreeFunc) free_pref_data,
					   &error);
	if (eel_gconf_handle_error (&error))
	{
		if (cnxn_id != EEL_GCONF_UNDEFINED_CONNECTION)
		{
			gconf_client_notify_remove (client, cnxn_id);
		}

		return 0;
	}

	data->cnxn_id = cnxn_id;
	notifiers = g_list_prepend (notifiers, data);

	gconf_client_notify (client, gconf_key);

	return cnxn_id;
}

static int
find_data (const PrefData *a,
	   gconstpointer idptr)
{
	return a->cnxn_id != GPOINTER_TO_UINT (idptr);
}

extern "C" void
mozilla_notifier_remove	(guint id)
{
	GList *l;

	g_return_if_fail (id != 0);

	l = g_list_find_custom (notifiers, GUINT_TO_POINTER (id),
				(GCompareFunc) find_data);
	g_return_if_fail (l != NULL);

	notifiers = g_list_delete_link (notifiers, l);
	eel_gconf_notification_remove (id);
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
migrate_font_gconf_key (const char *pixel_key,
			const char *point_key)
{
	int size;

	size = eel_gconf_get_integer (pixel_key);

	if (size > 0)
	{
		/* Use doubles to get more accurate arithmetic */
		double dpi   = (double) mozilla_get_dpi ();
		double value = (double) eel_gconf_get_integer (pixel_key);
		gint point = INT_ROUND ((value * 72) / dpi);

		eel_gconf_set_integer (point_key, point);
	}
}

#endif

extern "C" void
mozilla_notifiers_init (void)
{
	const EphyFontsLanguageInfo *font_languages;
	guint n_font_languages, i;

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

	for (i = 0; i < G_N_ELEMENTS (notifier_entries); i++)
	{
		mozilla_notifier_add (notifier_entries[i].gconf_key,
				      notifier_entries[i].mozilla_pref,
				      notifier_entries[i].func,
				      notifier_entries[i].user_data);
	}

	/* fonts notifiers */
	font_languages = ephy_font_languages ();
	n_font_languages = ephy_font_n_languages ();

	for (i=0; i < n_font_languages; i++)
	{
		const char *code = font_languages[i].code;
		guint k;
		char *types [] = { "variable", "monospace" };
		char key[255], pref[255];
#ifdef MIGRATE_PIXEL_SIZE
		char old_key[255];
#endif

		for (k = 0; k < G_N_ELEMENTS (types); k++)
		{
			g_snprintf (key, sizeof (key), "%s_%s_%s",
				    CONF_RENDERING_FONT, types[k], code);
			g_snprintf (pref, sizeof (pref), "font.name.%s.%s",
				    types[k], code);

			mozilla_notifier_add (key, pref,
					      mozilla_notifier_transform_string, NULL);
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

				g_snprintf (old_key, sizeof (old_key), "%s_%s_%s",
					    CONF_RENDERING_FONT, type, code);
				g_snprintf (key, sizeof (key), "%s_%s_%s",
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

		/* FIXME is it "minimum-size" or "min-size" !!? */
		g_snprintf (key, sizeof (key), "%s_%s",
			    CONF_RENDERING_FONT_MIN_SIZE, code);
		g_snprintf (pref, sizeof (pref), "font.minimum-size.%s", code);
		mozilla_notifier_add (key, pref, transform_font_size, NULL);

#ifdef MIGRATE_PIXEL_SIZE
		if (migrate_size)
		{
			g_snprintf (old_key, sizeof (key), "%s_%s",
				    CONF_RENDERING_FONT_MIN_SIZE_OLD, code);
			migrate_font_gconf_key (old_key, key);
		}
#endif

		g_snprintf (key, sizeof (key), "%s_%s",
			    CONF_RENDERING_FONT_FIXED_SIZE, code);
		g_snprintf (pref, sizeof (pref), "font.size.fixed.%s", code);
		mozilla_notifier_add (key, pref, transform_font_size, NULL);

#ifdef MIGRATE_PIXEL_SIZE
		if (migrate_size)
		{
			g_snprintf (old_key, sizeof (old_key), "%s_%s",
				    CONF_RENDERING_FONT_FIXED_SIZE_OLD, code);
			migrate_font_gconf_key (old_key, key);
		}
#endif

		g_snprintf (key, sizeof (key), "%s_%s",
			    CONF_RENDERING_FONT_VAR_SIZE, code);
		g_snprintf (pref, sizeof (pref), "font.size.variable.%s", code);
		mozilla_notifier_add (key, pref, transform_font_size, NULL);

#ifdef MIGRATE_PIXEL_SIZE
		if (migrate_size)
		{
			g_snprintf (old_key, sizeof (old_key), "%s_%s",
				    CONF_RENDERING_FONT_VAR_SIZE_OLD, code);
			migrate_font_gconf_key (old_key, key);
		}
#endif
	}
}

static void
remove_notification (PrefData *data)
{
	eel_gconf_notification_remove (data->cnxn_id);
}

extern "C" void
mozilla_notifiers_shutdown (void)
{
	eel_gconf_monitor_remove ("/apps/epiphany/web");
	eel_gconf_monitor_remove ("/system/proxy");
	eel_gconf_monitor_remove ("/system/http_proxy");

	g_list_foreach (notifiers, (GFunc) remove_notification, NULL);
	g_list_free (notifiers);
}
