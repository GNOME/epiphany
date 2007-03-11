/*
*  Copyright © 2000 Nate Case
*  Copyright © 2000-2004 Marco Pesenti Gritti
*  Copyright © 2003, 2004 Christian Persch
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
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*  $Id$
*/

#include "mozilla-config.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtksettings.h>

#include <nsCOMPtr.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-encodings.h"
#include "ephy-langs.h"
#include "ephy-node.h"
#include "ephy-prefs.h"

#include "mozilla-notifiers.h"

/* define to migrate epiphany 1.0 font preferences */
#define MIGRATE_PIXEL_SIZE

#define MAX_FONT_SIZE	128

/* Keeps the list of the notifiers we installed for mozilla prefs */
/* to be able to remove them when exiting */
static GList *notifiers = NULL;
static nsIPrefBranch *gPrefBranch;

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
transform_accept_languages_list (GConfEntry *gcentry,
				 GValue *value,
				 gpointer user_data)
{
	GConfValue *gcvalue;
	GArray *array;
	GSList *languages, *l;
	char **langs;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_LIST ||
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
transform_cache_size (GConfEntry *gcentry,
		      GValue *value,
		      gpointer user_data)
{
	GConfValue *gcvalue;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_INT) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, gconf_value_get_int (gcvalue) * 1024);

	return TRUE;
}

static gboolean
transform_cookies_accept_mode (GConfEntry *gcentry,
			       GValue *value,
			       gpointer user_data)

{
	GConfValue *gcvalue;
	const char *mode;
	int mozilla_mode = 0;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	     gcvalue->type != GCONF_VALUE_STRING) return FALSE;

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
transform_encoding (GConfEntry *gcentry,
		    GValue *value,
		    gpointer user_data)
{
	GConfValue *gcvalue;
	EphyEncodings *encodings;
	EphyNode *node;
	const char *code;
	gboolean is_autodetector;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_STRING) return FALSE;

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
transform_image_animation_mode (GConfEntry *gcentry,
			        GValue *value,
			        gpointer user_data)

{
	GConfValue *gcvalue;
	const char *mode;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_STRING) return FALSE;

	mode = gconf_value_get_string (gcvalue);
	if (mode == NULL) return FALSE;

	if (strcmp (mode, "disabled") == 0)
	{
		mode = "none";
	}

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, mode);

	return TRUE;
}

static gboolean
transform_proxy_ignore_list (GConfEntry *gcentry,
			     GValue *value,
			     gpointer user_data)
{
	GConfValue *gcvalue;
	GArray *array;
	GSList *hosts, *l;
	char **strings;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_LIST ||
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
transform_proxy_mode (GConfEntry *gcentry,
		      GValue *value,
		      gpointer user_data)
{
	GConfValue *gcvalue;
	const char *mode;
	int mozilla_mode = 0;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_STRING) return FALSE;

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
transform_use_own_fonts (GConfEntry *gcentry,
			 GValue *value,
			 gpointer user_data)
{
	GConfValue *gcvalue;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_BOOL) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, gconf_value_get_bool (gcvalue) ? 0 : 1);

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_bool (GConfEntry *gcentry,
				 GValue *value,
				 gpointer user_data)
{
	GConfValue *gcvalue;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_BOOL) return FALSE;

	g_value_init (value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, gconf_value_get_bool (gcvalue));

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_bool_invert (GConfEntry *gcentry,
					GValue *value,
					gpointer user_data)
{
	GConfValue *gcvalue;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_BOOL) return FALSE;

	g_value_init (value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, !gconf_value_get_bool (gcvalue));

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_int (GConfEntry *gcentry,
				GValue *value,
				gpointer user_data)
{
	GConfValue *gcvalue;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_INT) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, gconf_value_get_int (gcvalue));

	return TRUE;
}

extern "C" gboolean
mozilla_notifier_transform_string (GConfEntry *gcentry,
				   GValue *value,
				   gpointer user_data)
{
	GConfValue *gcvalue;
	const char *str;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_STRING) return FALSE;

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
	{ CONF_IMAGE_ANIMATION_MODE,
	  "image.animation_mode",
	  transform_image_animation_mode },
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
	{ CONF_PRIVACY_REMEMBER_PASSWORDS,
	  "signon.rememberSignons",
	  mozilla_notifier_transform_bool }
};

gboolean
mozilla_pref_set (const char *pref,
		  const GValue *value)
{
	NS_ENSURE_TRUE (gPrefBranch, FALSE);

	g_return_val_if_fail (pref != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	nsresult rv;
	switch (G_VALUE_TYPE (value))
	{
		case G_TYPE_INT:
		{
			PRInt32 old_value = 0;
			PRInt32 new_value = g_value_get_int (value);
			rv = gPrefBranch->GetIntPref (pref, &old_value);
			if (NS_FAILED (rv) || old_value != new_value)
			{
				rv = gPrefBranch->SetIntPref (pref, new_value);
			}
			break;
		}
		case G_TYPE_BOOLEAN:
		{
			PRBool old_value = PR_FALSE;
			PRBool new_value = g_value_get_boolean (value);
			rv = gPrefBranch->GetBoolPref (pref, &old_value);
			if (NS_FAILED (rv) || old_value != new_value)
			{
				rv = gPrefBranch->SetBoolPref (pref, new_value);
			}
			break;
		}
		case G_TYPE_STRING:
		{
			const char *new_value = g_value_get_string (value);
			if (new_value == NULL)
			{
				rv = gPrefBranch->ClearUserPref (pref);
			}
			else
			{
				char *old_value = nsnull;

				rv = gPrefBranch->GetCharPref (pref, &old_value);
				if (NS_FAILED (rv) ||
				    old_value == nsnull ||
				    strcmp (old_value, new_value) != 0)
				{
					rv = gPrefBranch->SetCharPref (pref, new_value);
				}

				if (old_value)
				{
					nsMemory::Free (old_value);
				}
			}
			break;
		}
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
	   GConfEntry *gcentry,
	   PrefData *data)
{
	GValue value = { 0, };

	g_return_if_fail (gcentry != NULL);
	g_assert (data != NULL);

	if (data->func (gcentry, &value, data->user_data))
	{
		mozilla_pref_set (data->mozilla_pref, &value);
		g_value_unset (&value);
	}
	else
	{
		/* Reset the pref */
		NS_ENSURE_TRUE (gPrefBranch, );
		gPrefBranch->ClearUserPref (data->mozilla_pref);
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

	/* FIXME: use slice allocator */
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
		g_object_get (G_OBJECT (settings), "gtk-xft-dpi", &dpi, (char *) NULL);
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

static gboolean
parse_pango_font (const char *font,
		  char **name,
		  int *size)
{
	PangoFontMask mask = (PangoFontMask) (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_SIZE);

	PangoFontDescription *desc = pango_font_description_from_string (font);
	if (desc == NULL) return FALSE;

	if ((pango_font_description_get_set_fields (desc) & mask) != mask)
	{
		pango_font_description_free (desc);
		return FALSE;
	}
				
	*size = PANGO_PIXELS (pango_font_description_get_size (desc));
	*name = g_strdup (pango_font_description_get_family (desc));

	pango_font_description_free (desc);

	LOG ("Parsed pango font description '%s' -> name:%s, size:%d",
	     font, *name ? *name : "-", *size);

	return *name != NULL && *size > 0;
}

typedef struct
{
	guint cnxn_id;
	char **font_name_prefs;
	char **font_size_prefs;
	char *font_name;
	int font_size;
} DesktopFontData;

static gboolean
transform_font_name (GConfEntry *gcentry,
		     GValue *value,
		     gpointer user_data)
{
	DesktopFontData *data = (DesktopFontData *) user_data;
	GConfValue *gcvalue;
	const char *font_name;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gconf_entry_get_is_default (gcentry) ||
	    gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_STRING)
	{
		font_name = data->font_name;
	}
	else
	{
		font_name = gconf_value_get_string (gcvalue);
		if ((font_name == NULL || font_name[0] == '\0'))
		{
			font_name = data->font_name;
		}
	}

	LOG ("%s value for key '%s'",
	     gconf_entry_get_is_default (gcentry) ? "default" : "NON-default",
	     gconf_entry_get_key (gcentry));

	if (font_name == NULL) return FALSE;

	LOG ("Inferred font name '%s' for key '%s'",
	     font_name, gconf_entry_get_key (gcentry));

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, font_name);

	return TRUE;
}

static gboolean
transform_font_size (GConfEntry *gcentry,
		     GValue *value,
		     gpointer user_data)
{
	DesktopFontData *data = (DesktopFontData *) user_data;
	GConfValue *gcvalue;
	int size = 0;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gconf_entry_get_is_default (gcentry) ||
	    gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_INT)
	{
		size = data->font_size;
	}
	else
	{
		size = gconf_value_get_int (gcvalue);
	}

	if (size <= 0 || size > MAX_FONT_SIZE) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, size);

	return TRUE;
}

static void
notify_desktop_font_cb (GConfClient *client,
			guint cnxn_id,
			GConfEntry *gcentry,
			DesktopFontData *data)
{
	GConfValue *gcvalue;
	char *name = NULL;
	int size = 0, i;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue != NULL &&
	    gcvalue->type == GCONF_VALUE_STRING &&
	    parse_pango_font (gconf_value_get_string (gcvalue), &name, &size))
	{
		LOG ("Desktop font %s -> name=%s, size=%d", 
		     gconf_entry_get_key (gcentry), name, size);

		g_free (data->font_name);
		data->font_name = name;
		data->font_size = size;
	}
	else
	{
		g_free (name);

		g_free (data->font_name);
		data->font_name = NULL;
		data->font_size = 0;
	}

	for (i = 0; data->font_name_prefs[i] != NULL; ++i)
	{
		gconf_client_notify (client, data->font_name_prefs[i]);
	}
	for (i = 0; data->font_size_prefs[i] != NULL; ++i)
	{
		gconf_client_notify (client, data->font_size_prefs[i]);
	}
}

typedef struct
{
	guint cnxn_id;
	char **prefs;
	int size;
} MinimumFontSizeData;

static gboolean
transform_minimum_font_size (GConfEntry *gcentry,
			     GValue *value,
			     gpointer user_data)
{
	MinimumFontSizeData *data = (MinimumFontSizeData *) user_data;
	GConfValue *gcvalue;
	int size = 0;

	gcvalue = gconf_entry_get_value (gcentry);
	if (gcvalue == NULL ||
	    gcvalue->type != GCONF_VALUE_INT)
	{
		size = data->size;
	}
	else
	{
		size = MAX (gconf_value_get_int (gcvalue),
			    data->size);
	}

	if (size <= 0) return FALSE;

	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, size);

	return TRUE;
}

static void
notify_minimum_size_cb (GConfClient *client,
			guint cnxn_id,
			GConfEntry *entry,
			MinimumFontSizeData *data)
{
	GConfValue *gcvalue;
	int i;

	data->size = 0;

	gcvalue = gconf_entry_get_value (entry);
	/* happens on initial notify if the key doesn't exist */
	if (gcvalue != NULL &&
	    gcvalue->type == GCONF_VALUE_INT)
	{
		data->size = gconf_value_get_int (gcvalue);
		data->size = MAX (data->size, 0);
	}

	LOG ("Minimum font size now %d", data->size);

	for (i = 0; data->prefs[i] != NULL; ++i)
	{
		gconf_client_notify (client, data->prefs[i]);
	}
}

static DesktopFontData *desktop_font_data;
static MinimumFontSizeData *minimum_font_size_data;

static void
mozilla_font_notifiers_init (void)
{
	static const char *types [] = { "variable", "monospace" };
	const EphyFontsLanguageInfo *font_languages;
	guint n_font_languages, i;

	eel_gconf_monitor_add ("/desktop/gnome/interface");

	font_languages = ephy_font_languages ();
	n_font_languages = ephy_font_n_languages ();

	desktop_font_data = g_new0 (DesktopFontData, 2);
	desktop_font_data[0].font_name_prefs = g_new0 (char*, n_font_languages + 1);
	desktop_font_data[0].font_size_prefs = g_new0 (char*, n_font_languages + 1);
	desktop_font_data[1].font_name_prefs = g_new0 (char*, n_font_languages + 1);
	desktop_font_data[1].font_size_prefs = g_new0 (char*, n_font_languages + 1);

	desktop_font_data[0].cnxn_id =
		eel_gconf_notification_add (CONF_DESKTOP_FONT_VARIABLE,
					    (GConfClientNotifyFunc) notify_desktop_font_cb,
					    &desktop_font_data[0]);
	eel_gconf_notify (CONF_DESKTOP_FONT_VARIABLE);

	desktop_font_data[1].cnxn_id =
		eel_gconf_notification_add (CONF_DESKTOP_FONT_MONOSPACE,
					    (GConfClientNotifyFunc) notify_desktop_font_cb,
					    &desktop_font_data[1]);
	eel_gconf_notify (CONF_DESKTOP_FONT_MONOSPACE);

	minimum_font_size_data = g_new0 (MinimumFontSizeData, 1);
	minimum_font_size_data->prefs = g_new0 (char*, n_font_languages + 1);

	minimum_font_size_data->cnxn_id =
		eel_gconf_notification_add (CONF_RENDERING_FONT_MIN_SIZE,
					    (GConfClientNotifyFunc) notify_minimum_size_cb,
					    minimum_font_size_data);
	eel_gconf_notify (CONF_RENDERING_FONT_MIN_SIZE);

#ifdef MIGRATE_PIXEL_SIZE
	gboolean migrate_size;

	migrate_size = (eel_gconf_get_integer (CONF_SCHEMA_VERSION)
			< EPIPHANY_SCHEMA_VERSION);
	if (migrate_size)
	{
		eel_gconf_set_integer (CONF_SCHEMA_VERSION, EPIPHANY_SCHEMA_VERSION);
	}
#endif

	for (i=0; i < n_font_languages; i++)
	{
		const char *code = font_languages[i].code;
		guint k;
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

			desktop_font_data[k].font_name_prefs[i] = g_strdup (key);

			mozilla_notifier_add (key, pref,
					      transform_font_name,
					      &desktop_font_data[k]);
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

		minimum_font_size_data->prefs[i] = g_strdup (key);

		mozilla_notifier_add (key, pref, transform_minimum_font_size,
				      minimum_font_size_data);

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

		desktop_font_data[1].font_size_prefs[i] = g_strdup (key);
		mozilla_notifier_add (key, pref, transform_font_size,
				      &desktop_font_data[1]);

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
	
		desktop_font_data[0].font_size_prefs[i] = g_strdup (key);
		mozilla_notifier_add (key, pref, transform_font_size,
				      &desktop_font_data[0]);

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
mozilla_font_notifiers_shutdown (void)
{
	eel_gconf_monitor_remove ("/desktop/gnome/interface");

	eel_gconf_notification_remove (desktop_font_data[0].cnxn_id);
	eel_gconf_notification_remove (desktop_font_data[1].cnxn_id);
	eel_gconf_notification_remove (minimum_font_size_data->cnxn_id);

	g_free (desktop_font_data[0].font_name);
	g_free (desktop_font_data[1].font_name);

	g_strfreev (desktop_font_data[0].font_name_prefs);
	g_strfreev (desktop_font_data[0].font_size_prefs);
	g_strfreev (desktop_font_data[1].font_name_prefs);
	g_strfreev (desktop_font_data[1].font_size_prefs);
	g_strfreev (minimum_font_size_data->prefs);

	g_free (desktop_font_data);
	g_free (minimum_font_size_data);	
}

extern "C" gboolean
mozilla_notifiers_init (void)
{
	guint i;

	eel_gconf_monitor_add ("/apps/epiphany/web");
	eel_gconf_monitor_add ("/system/proxy");
	eel_gconf_monitor_add ("/system/http_proxy");

	/* the pref service conveniently implements the root pref branch */
	gPrefBranch = nsnull;
	nsresult rv = CallGetService (NS_PREFSERVICE_CONTRACTID, &gPrefBranch);
	if (NS_FAILED (rv) || !gPrefBranch)
	{
		g_warning ("Failed to get the pref service!\n");
		return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS (notifier_entries); i++)
	{
		mozilla_notifier_add (notifier_entries[i].gconf_key,
				      notifier_entries[i].mozilla_pref,
				      notifier_entries[i].func,
				      notifier_entries[i].user_data);
	}

	mozilla_font_notifiers_init ();

	return TRUE;
}

static void
remove_notification (PrefData *data)
{
	eel_gconf_notification_remove (data->cnxn_id);
}

extern "C" void
mozilla_notifiers_shutdown (void)
{
	NS_IF_RELEASE (gPrefBranch);
	gPrefBranch = nsnull;

	mozilla_font_notifiers_shutdown ();

	eel_gconf_monitor_remove ("/apps/epiphany/web");
	eel_gconf_monitor_remove ("/system/proxy");
	eel_gconf_monitor_remove ("/system/http_proxy");

	g_list_foreach (notifiers, (GFunc) remove_notification, NULL);
	g_list_free (notifiers);
}
