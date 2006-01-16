/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2004 Adam Hooper
 *  Copyright (C) 2005 Crispin Flowerday
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

#include "config.h"

#include "ephy-extensions-manager.h"

#include "ephy-loader.h"
#include "ephy-shlib-loader.h"

#include "ephy-node-db.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"
#include "ephy-object-helpers.h"
#include "ephy-debug.h"

#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/globals.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <gconf/gconf-client.h>

#include <gmodule.h>
#include <dirent.h>
#include <string.h>

#ifdef ENABLE_PYTHON
#include "ephy-python-extension.h"
#include "ephy-python-loader.h"
#endif

#define CONF_LOADED_EXTENSIONS	"/apps/epiphany/general/active_extensions"
#define EE_GROUP		"Epiphany Extension"
#define DOT_INI			".ephy-extension"
#define RELOAD_DELAY		333 /* ms */
#define RELOAD_SYNC_DELAY	1000 /* ms */

#define ENABLE_LEGACY_FORMAT

#define EPHY_EXTENSIONS_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerPrivate))

struct _EphyExtensionsManagerPrivate
{
	gboolean initialised;

	GList *data;
	GList *factories;
	GList *extensions;
	GList *dir_monitors;
	GList *windows;
	guint active_extensions_notifier_id;
	guint sync_timeout_id;
	GHashTable *reload_hash;

#ifdef ENABLE_LEGACY_FORMAT
	xsltStylesheetPtr xml2ini_xsl;
#endif
};

typedef struct
{
	EphyExtensionInfo info;
	gboolean load_failed;

	char *loader_type;

	EphyLoader *loader; /* NULL if never loaded */
	GObject *extension; /* NULL if unloaded */

#ifdef ENABLE_LEGACY_FORMAT
	guint is_legacy_format : 1;
#endif
} ExtensionInfo;

typedef struct
{
	char *type;
	EphyLoader *loader;
} LoaderInfo;

typedef enum
{
	FORMAT_UNKNOWN,
	FORMAT_INI
#ifdef ENABLE_LEGACY_FORMAT
	, FORMAT_XML
#endif
} ExtensionFormat;

enum
{
	CHANGED,
	ADDED,
	REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void ephy_extensions_manager_class_init	(EphyExtensionsManagerClass *klass);
static void ephy_extensions_manager_iface_init	(EphyExtensionIface *iface);
static void ephy_extensions_manager_init	(EphyExtensionsManager *manager);

GType
ephy_extensions_manager_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyExtensionsManagerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_extensions_manager_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyExtensionsManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_extensions_manager_init
		};

		static const GInterfaceInfo extension_info =
		{
			(GInterfaceInitFunc) ephy_extensions_manager_iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyExtensionsManager",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     EPHY_TYPE_EXTENSION,
					     &extension_info);
	}

	return type;
}

/**
 * ephy_extensions_manager_load:
 * @manager: an #EphyExtensionsManager
 * @name: identifier of the extension to load
 *
 * Loads the @name extension.
 **/
void
ephy_extensions_manager_load (EphyExtensionsManager *manager,
			      const char *identifier)
{
	GSList *gconf_exts;

	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));
	g_return_if_fail (identifier != NULL);

	LOG ("Adding '%s' to extensions", identifier);

	gconf_exts = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);

	if (!g_slist_find_custom (gconf_exts, identifier, (GCompareFunc) strcmp))
	{
		gconf_exts = g_slist_prepend (gconf_exts, g_strdup (identifier));

		eel_gconf_set_string_list (CONF_LOADED_EXTENSIONS, gconf_exts);
	}

	g_slist_foreach (gconf_exts, (GFunc) g_free, NULL);
	g_slist_free (gconf_exts);
}

/**
 * ephy_extensions_manager_unload:
 * @manager: an #EphyExtensionsManager
 * @name: filename of extension to unload, minus "lib" and "extension.so"
 *
 * Unloads the extension specified by @name.
 *
 * The extension with the same filename can afterwards be reloaded. However,
 * if any GTypes within the extension have changed parent types, Epiphany must
 * be restarted.
 **/
void
ephy_extensions_manager_unload (EphyExtensionsManager *manager,
				const char *identifier)
{
	GSList *gconf_exts;
	GSList *l;

	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));
	g_return_if_fail (identifier != NULL);

	LOG ("Removing '%s' from extensions", identifier);

	gconf_exts = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);

	l = g_slist_find_custom (gconf_exts, identifier, (GCompareFunc) strcmp);

	if (l != NULL)
	{
		gconf_exts = g_slist_remove_link (gconf_exts, l);
		g_free (l->data);
		g_slist_free_1 (l);

		eel_gconf_set_string_list (CONF_LOADED_EXTENSIONS, gconf_exts);
	}

	g_slist_foreach (gconf_exts, (GFunc) g_free, NULL);
	g_slist_free (gconf_exts);
}

/**
 * ephy_extensions_manager_register:
 * @manager: an #EphyExtensionsManager
 * @object: an Extension
 *
 * Registers @object with the extensions manager. @object must implement the
 * #EphyExtension interface.
 **/
void
ephy_extensions_manager_register (EphyExtensionsManager *manager,
				  GObject *object)
{
	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));
	g_return_if_fail (EPHY_IS_EXTENSION (object));

	manager->priv->extensions = g_list_prepend (manager->priv->extensions,
						    g_object_ref (object));
}


/**
 * ephy_extensions_manager_get_extensions:
 * @manager: an #EphyExtensionsManager
 *
 * Returns the list of known extensions.
 *
 * Returns: a list of #EphyExtensionInfo
 **/
GList *
ephy_extensions_manager_get_extensions (EphyExtensionsManager *manager)
{
	return g_list_copy (manager->priv->data);
}

static void
free_extension_info (ExtensionInfo *info)
{
	EphyExtensionInfo *einfo = (EphyExtensionInfo *) info;

	g_free (einfo->identifier);
	g_key_file_free (einfo->keyfile);
	g_free (info->loader_type);

	if (info->extension != NULL)
	{
		g_return_if_fail (info->loader != NULL);

		ephy_loader_release_object (info->loader, info->extension);
	}
	if (info->loader != NULL)
	{
		g_object_unref (info->loader);
	}

	g_free (info);
}

static void
free_loader_info (LoaderInfo *info)
{
	g_free (info->type);
	g_object_unref (info->loader);
	g_free (info);
}

static int
find_extension_info (const ExtensionInfo *info,
		     const char *identifier)
{
	return strcmp (info->info.identifier, identifier);
}

static ExtensionInfo *
ephy_extensions_manager_parse_keyfile (EphyExtensionsManager *manager,
				       GKeyFile *key_file,
				       const char *identifier)
{
	ExtensionInfo *info;
	EphyExtensionInfo *einfo;
	char *start_group;

	LOG ("Parsing INI description file for '%s'", identifier);

	start_group = g_key_file_get_start_group (key_file);
	if (start_group == NULL ||
	    strcmp (start_group, EE_GROUP) != 0 ||
	    !g_key_file_has_group (key_file, "Loader"))
	{
		g_warning ("Invalid extension description file for '%s'; "
			   "missing 'Epiphany Extension' or 'Loader' group",
			   identifier);
		
		g_key_file_free (key_file);
		g_free (start_group);
		return NULL;
	}
	g_free (start_group);

	if (!g_key_file_has_key (key_file, EE_GROUP, "Name", NULL) ||
	    !g_key_file_has_key (key_file, EE_GROUP, "Description", NULL))
	{
		g_warning ("Invalid extension description file for '%s'; "
			   "missing 'Name' or 'Description' keys.",
			   identifier);
		
		g_key_file_free (key_file);
		return NULL;
	}

	info = g_new0 (ExtensionInfo, 1);
	einfo = (EphyExtensionInfo *) info;
	einfo->identifier = g_strdup (identifier);
	einfo->keyfile = key_file;

	info->loader_type = g_key_file_get_string (key_file, "Loader", "Type", NULL);

	/* sanity check */
	if (info->loader_type == NULL || info->loader_type[0] == '\0')
	{
		free_extension_info (info);
		return NULL;
	}

	manager->priv->data = g_list_prepend (manager->priv->data, info);

	g_signal_emit (manager, signals[ADDED], 0, info);

	return info;
}

static void
ephy_extensions_manager_load_ini_file (EphyExtensionsManager *manager,
				       const char *identifier,
				       const char *path)
{
	GKeyFile *keyfile;
	GError *err = NULL;

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &err))
	{
		g_warning ("Could load key file for '%s': '%s'",
			   identifier, err->message);
		g_error_free (err);
		g_key_file_free (keyfile);
		return;
	}

	ephy_extensions_manager_parse_keyfile (manager, keyfile, identifier);
}

#ifdef ENABLE_LEGACY_FORMAT

static void
ephy_extensions_manager_load_xml_file (EphyExtensionsManager *manager,
				       const char *identifier,
				       const char *path)
{
	EphyExtensionsManagerPrivate *priv = manager->priv;
	ExtensionInfo *info;
	xmlDocPtr doc, res;
	const xmlChar *xsl_file;
	xmlChar *output = NULL;
	int outlen = -1, ret = -1;
	
	START_PROFILER ("Transforming .xml -> " DOT_INI)

	doc = xmlParseFile (path);
	if (!doc) goto out;

	if (priv->xml2ini_xsl == NULL)
	{
		xsl_file = (const xmlChar *) ephy_file ("ephy-xml2ini.xsl");
		if (!xsl_file) return;

		priv->xml2ini_xsl = xsltParseStylesheetFile (xsl_file);
		if (priv->xml2ini_xsl == NULL)
		{
			g_warning ("Couldn't parse the XSL to transform .xml extension descriptions!\n");
			goto out;
		}
	}

	res = xsltApplyStylesheet (priv->xml2ini_xsl, doc, NULL);
	if (!res) goto out;

	ret = xsltSaveResultToString (&output, &outlen, res, priv->xml2ini_xsl);

	if (ret >= 0 && output != NULL && outlen > -1)
	{
		GKeyFile *keyfile;
		GError *err = NULL;

		keyfile = g_key_file_new ();
		if (!g_key_file_load_from_data (keyfile, (char *) output, outlen,
					        G_KEY_FILE_NONE, &err))
		{
			g_warning ("Could load converted key file for '%s': '%s'",
				   identifier, err->message);
			g_error_free (err);
			g_key_file_free (keyfile);
			goto out;
		}

		info = ephy_extensions_manager_parse_keyfile (manager, keyfile, identifier);
		if (info != NULL)
		{
			info->is_legacy_format = TRUE;
		}
	}

	xmlFreeDoc (res);
	xmlFreeDoc (doc);

out:
	xmlFree (output);

	STOP_PROFILER ("Transforming .xml -> " DOT_INI)
}

#endif /* ENABLE_LEGACY_FORMAT */

static char *
path_to_identifier (const char *path)
{
	char *identifier, *dot;

	identifier = g_path_get_basename (path);
	dot = strstr (identifier, DOT_INI);

#ifdef ENABLE_LEGACY_FORMAT
	if (!dot)
	{
		dot = strstr (identifier, ".xml");
	}
#endif

	g_return_val_if_fail (dot != NULL, NULL);

	*dot = '\0';

	return identifier;
}

static ExtensionFormat
format_from_path (const char *path)
{
	ExtensionFormat format = FORMAT_UNKNOWN;

	if (g_str_has_suffix (path, DOT_INI))
	{
		format = FORMAT_INI;
	}
#ifdef ENABLE_LEGACY_FORMAT
	else if (g_str_has_suffix (path, ".xml"))
	{
		format = FORMAT_XML;
	}
#endif

	return format;
}

static void
ephy_extensions_manager_load_file (EphyExtensionsManager *manager,
				   const char *path)
{
	GList *element;
	char *identifier;
	ExtensionFormat format;

	identifier = path_to_identifier (path);
	g_return_if_fail (identifier != NULL);
	if (identifier == NULL) return;

	format = format_from_path (path);
	g_return_if_fail (format != FORMAT_UNKNOWN);

	element = g_list_find_custom (manager->priv->data, identifier,
				      (GCompareFunc) find_extension_info);
	if (element != NULL)
	{
#ifdef ENABLE_LEGACY_FORMAT
		ExtensionInfo *info = (ExtensionInfo *) element->data;

		/* If this is the legacy format and we already have the info
		 * read for this type from a non-legacy format file, don't
		 * warn.
		 */
		if (format == FORMAT_XML && !info->is_legacy_format)
#endif
		{
			g_warning ("Extension description for '%s' already read!",
				   identifier);
		}

		g_free (identifier);
		return;
	}

	if (format == FORMAT_INI)
	{
		ephy_extensions_manager_load_ini_file (manager, identifier,
						       path);
	}
#ifdef ENABLE_LEGACY_FORMAT
	else if (format == FORMAT_XML)
	{
		ephy_extensions_manager_load_xml_file (manager, identifier,
						       path);
	}
#endif

	g_free (identifier);
}


static int
find_loader (const LoaderInfo *info,
	      const char *type)
{
	return strcmp (info->type, type);
}

static char *
sanitise_type (const char *string)
{
	char *str, *p;

	str = g_strdup (string);
	for (p = str; *p != '\0'; p++)
	{
		if (!g_ascii_isalpha (*p)) *p = '-';
	}

	return str;
}

static EphyLoader *
get_loader_for_type (EphyExtensionsManager *manager,
		     const char *type)
{
	LoaderInfo *info;
	GList *l;
	char *path, *name, *stype, *data;
	GKeyFile *keyfile;
	EphyLoader *shlib_loader;
	GObject *loader;

	LOG ("Looking for loader for type '%s'", type);

	l = g_list_find_custom (manager->priv->factories, type,
				(GCompareFunc) find_loader);
	if (l != NULL)
	{
		info = (LoaderInfo *) l->data;
		return g_object_ref (info->loader);
	}

	if (strcmp (type, "shlib") == 0)
	{
		info = g_new (LoaderInfo, 1);
		info->type = g_strdup (type);
		info->loader = g_object_new (EPHY_TYPE_SHLIB_LOADER, NULL);

		manager->priv->factories =
			g_list_append (manager->priv->factories, info);

		return g_object_ref (info->loader);
	}
	if (strcmp (type, "python") == 0)
	{
#ifdef ENABLE_PYTHON
		info = g_new (LoaderInfo, 1);
		info->type = g_strdup (type);
		info->loader = g_object_new (EPHY_TYPE_PYTHON_LOADER, NULL);

		manager->priv->factories =
				g_list_append (manager->priv->factories, info);

		return g_object_ref (info->loader);
#else
		return NULL;
#endif
	}

	shlib_loader = get_loader_for_type (manager, "shlib");
	g_return_val_if_fail (shlib_loader != NULL, NULL);

	stype = sanitise_type (type);
	name = g_strconcat ("lib", stype, "loader.", G_MODULE_SUFFIX, NULL);
	path = g_build_filename (LOADER_DIR, name, NULL);
	data = g_strconcat ("[Loader]\nType=shlib\nLibrary=", path, "\n", NULL);
	g_free (stype);
	g_free (name);
	g_free (path);

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_data (keyfile, data, strlen (data), 0, NULL))
	{
		g_free (data);
		return NULL;
	}

	loader = ephy_loader_get_object (shlib_loader, keyfile);
	g_key_file_free (keyfile);

	if (EPHY_IS_LOADER (loader))
	{
		info = g_new (LoaderInfo, 1);
		info->type = g_strdup (type);
		info->loader = EPHY_LOADER (loader);

		manager->priv->factories =
			g_list_append (manager->priv->factories, info);

		return g_object_ref (info->loader);
	}

	g_return_val_if_reached (NULL);

	return NULL;
}

static void
attach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	GList *tabs, *l;

	ephy_extension_attach_window (extension, window);

	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l; l = l->next)
	{
		ephy_extension_attach_tab (extension, window,
					   EPHY_TAB (l->data));
	}
	g_list_free (tabs);
}

static void
load_extension (EphyExtensionsManager *manager,
		ExtensionInfo *info)
{
	EphyLoader *loader;

	g_return_if_fail (info->extension == NULL);

	LOG ("Loading extension '%s'", info->info.identifier);

	/* don't try again */
	if (info->load_failed) return;

	/* get a loader */
	loader = get_loader_for_type (manager, info->loader_type);
	if (loader == NULL)
	{
		g_message ("No loader found for extension '%s' of type '%s'\n",
			   info->info.identifier, info->loader_type);
		return;
	}

	info->loader = loader;

	info->extension = ephy_loader_get_object (loader, info->info.keyfile);

	/* attach if the extension implements EphyExtensionIface */
	if (EPHY_IS_EXTENSION (info->extension))
	{
		manager->priv->extensions =
			g_list_prepend (manager->priv->extensions,
					g_object_ref (info->extension));

		g_list_foreach (manager->priv->windows, (GFunc) attach_window,
				info->extension);
	}

	if (info->extension != NULL)
	{
		info->info.active = TRUE;

		g_signal_emit (manager, signals[CHANGED], 0, info);
	}
	else
	{
		info->info.active = FALSE;
		info->load_failed = TRUE;
	}
}

static void
detach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	GList *tabs, *l;

	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l; l = l->next)
	{
		ephy_extension_detach_tab (extension, window,
					   EPHY_TAB (l->data));
	}
	g_list_free (tabs);

	ephy_extension_detach_window (extension, window);
}

static void
unload_extension (EphyExtensionsManager *manager,
		  ExtensionInfo *info)
{
	g_return_if_fail (info->loader != NULL);
	g_return_if_fail (info->extension != NULL || info->load_failed);

	LOG ("Unloading extension '%s'", info->info.identifier);

	if (info->load_failed) return;

	/* detach if the extension implements EphyExtensionIface */
	if (EPHY_IS_EXTENSION (info->extension))
	{
		g_list_foreach (manager->priv->windows, (GFunc) detach_window,
				info->extension);

		manager->priv->extensions =
			g_list_remove (manager->priv->extensions, info->extension);

		/* we own two refs to the extension, the one we added when
		 * we added it to the priv->extensions list, and the one returned
		 * from get_object. Release object, and queue a unref, since if the
		 * extension has its own functions queued in the idle loop, the
		 * functions must exist in memory before being called.
		 */
		ephy_object_idle_unref (info->extension);
	}

	ephy_loader_release_object (info->loader, G_OBJECT (info->extension));

	info->info.active = FALSE;
	info->extension = NULL;

	g_signal_emit (manager, signals[CHANGED], 0, info);
}

static void
sync_loaded_extensions (EphyExtensionsManager *manager)
{
	GConfClient *client;
	GConfValue *value;
	GSList *active_extensions = NULL;
	GList *l;
	gboolean active;
	ExtensionInfo *info;

	LOG ("Synching changed list of active extensions");

	client = gconf_client_get_default ();
	g_return_if_fail (client != NULL);

	value = gconf_client_get (client, CONF_LOADED_EXTENSIONS, NULL);

	/* make sure the extensions-manager-ui is loaded */
	if (value == NULL ||
	    value->type != GCONF_VALUE_LIST ||
	    gconf_value_get_list_type (value) != GCONF_VALUE_STRING)
	{
		active_extensions = g_slist_prepend (active_extensions,
						     g_strdup ("extensions-manager-ui"));
		eel_gconf_set_string_list (CONF_LOADED_EXTENSIONS, active_extensions);
	}
	else
	{
		active_extensions = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);
	}

	for (l = manager->priv->data; l != NULL; l = l->next)
	{
		info = (ExtensionInfo *) l->data;

		active = (g_slist_find_custom (active_extensions,
					       info->info.identifier,
					       (GCompareFunc) strcmp) != NULL);

		LOG ("Extension '%s' is %sactive and %sloaded",
		     info->info.identifier,
		     active ? "" : "not ",
		     info->info.active ? "" : "not ");

		if (active != info->info.active)
		{
			if (active)
			{
				load_extension (manager, info);
			}
			else
			{
				unload_extension (manager, info);
			}
		}
	}

	g_slist_foreach (active_extensions, (GFunc) g_free, NULL);
	g_slist_free (active_extensions);

	if (value != NULL)
	{
		gconf_value_free (value);
	}
	g_object_unref (client);
}

static void
ephy_extensions_manager_unload_file (EphyExtensionsManager *manager,
				     const char *path)
{
	GList *l;
	ExtensionInfo *info;
	char *identifier;

	identifier = path_to_identifier (path);

	l = g_list_find_custom (manager->priv->data, identifier,
				(GCompareFunc) find_extension_info);

	if (l != NULL)
	{
		info = (ExtensionInfo *) l->data;

		manager->priv->data = g_list_remove (manager->priv->data, info);

		if (info->info.active == TRUE)
		{
			unload_extension (manager, info);
		}

		g_signal_emit (manager, signals[REMOVED], 0, info);

		free_extension_info (info);
	}

	g_free (identifier);
}

static gboolean
reload_sync_cb (EphyExtensionsManager *manager)
{
	EphyExtensionsManagerPrivate *priv = manager->priv;

	if (priv->sync_timeout_id != 0)
	{
		g_source_remove (priv->sync_timeout_id);
		priv->sync_timeout_id = 0;
	}

	sync_loaded_extensions (manager);

	return FALSE;
}

static gboolean
reload_cb (gpointer *data)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (data[0]);
	EphyExtensionsManagerPrivate *priv = manager->priv;
	char *path = data[1];

	LOG ("Reloading %s", path);

	/* We still need path and don't want to remove the timeout
	 * which will be removed automatically when we return, so 
	 * just use _steal instead of _remove.
	 */
	g_hash_table_steal (priv->reload_hash, path);

	ephy_extensions_manager_load_file (manager, path);
	g_free (path);

	/* Schedule a sync of active extensions */
	/* FIXME: just look if we need to activate *this* extension? */

	if (priv->sync_timeout_id != 0)
	{
		g_source_remove (priv->sync_timeout_id);
	}

	priv->sync_timeout_id = g_timeout_add (RELOAD_SYNC_DELAY,
					       (GSourceFunc) reload_sync_cb,
					       manager);
	return FALSE;
}

static void
schedule_load_from_monitor (EphyExtensionsManager *manager,
			    const char *path)
{
	EphyExtensionsManagerPrivate *priv = manager->priv;
	char *identifier, *copy;
	gpointer *data;
	guint timeout_id;

	/* When a file is installed, it sometimes gets CREATED empty and then
	 * gets its contents filled later (for a CHANGED signal). Theoretically
	 * I suppose we could get a CHANGED signal when the file is half-full,
	 * but I doubt that'll happen much (the files are <1000 bytes). We
	 * don't want warnings all over the place, so we just wait a bit before
	 * actually reloading the file. (We're assuming that if a file is
	 * empty it'll be filled soon and this function will be called again.)
	 *
	 * Oh, and we return if the extension is already loaded, too.
	 */

	identifier = path_to_identifier (path);
	g_return_if_fail (identifier != NULL);
	if (identifier == NULL) return;

	if (g_list_find_custom (manager->priv->data, identifier,
				(GCompareFunc) find_extension_info) != NULL)
	{
		g_free (identifier);
		return;
	}
	g_free (identifier);

	g_return_if_fail (priv->reload_hash != NULL);

	data = g_new (gpointer, 2);
	data[0] = (gpointer) manager;
	data[1] = copy = g_strdup (path);
	timeout_id = g_timeout_add_full (G_PRIORITY_LOW, RELOAD_DELAY,
					 (GSourceFunc) reload_cb,
					 data, (GDestroyNotify) g_free);
	g_hash_table_replace (priv->reload_hash, copy /* owns it */,
			      GUINT_TO_POINTER (timeout_id));
}

static void
dir_changed_cb (GnomeVFSMonitorHandle *handle,
		const char *monitor_uri,
		const char *info_uri,
		GnomeVFSMonitorEventType event_type,
		EphyExtensionsManager *manager)
{
	char *path;

	/*
	 * We only deal with XML and INI files:
	 * Add them to the manager when created, remove them when deleted.
	 */
	if (format_from_path (info_uri) == FORMAT_UNKNOWN) return;

	path = gnome_vfs_get_local_path_from_uri (info_uri);

	switch (event_type)
	{
		case GNOME_VFS_MONITOR_EVENT_CREATED:
		case GNOME_VFS_MONITOR_EVENT_CHANGED:
			schedule_load_from_monitor (manager, path);
			break;
		case GNOME_VFS_MONITOR_EVENT_DELETED:
			ephy_extensions_manager_unload_file (manager, path);
			break;
		default:
			break;
	}

	g_free (path);
}

static void
ephy_extensions_manager_load_dir (EphyExtensionsManager *manager,
				  const char *path)
{
	DIR *d;
	struct dirent *e;
	char *file_path;
	char *file_uri;
	GnomeVFSMonitorHandle *monitor;
	GnomeVFSResult res;

	LOG ("Scanning directory '%s'", path);

	START_PROFILER ("Scanning directory")

	d = opendir (path);
	if (d == NULL)
	{
		return;
	}
	while ((e = readdir (d)) != NULL)
	{
		if (format_from_path (e->d_name) != FORMAT_UNKNOWN)
		{
			file_path = g_build_filename (path, e->d_name, NULL);
			ephy_extensions_manager_load_file (manager, file_path);
			g_free (file_path);
		}
	}
	closedir (d);

	file_uri = gnome_vfs_get_uri_from_local_path (path);
	res = gnome_vfs_monitor_add (&monitor,
				     path,
				     GNOME_VFS_MONITOR_DIRECTORY,
				     (GnomeVFSMonitorCallback) dir_changed_cb,
				     manager);
	g_free (file_uri);

	if (res == GNOME_VFS_OK)
	{
		manager->priv->dir_monitors = g_list_prepend
			(manager->priv->dir_monitors, monitor);
	}

	STOP_PROFILER ("Scanning directory")
}

static void
active_extensions_notifier (GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyExtensionsManager *manager)
{
	sync_loaded_extensions (manager);
}

static void
cancel_timeout (gpointer data)
{
	guint id = GPOINTER_TO_UINT (data);

	g_source_remove (id);
}

static void
ephy_extensions_manager_init (EphyExtensionsManager *manager)
{
	EphyExtensionsManagerPrivate *priv;

	priv = manager->priv = EPHY_EXTENSIONS_MANAGER_GET_PRIVATE (manager);

	priv->reload_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) cancel_timeout);
}

void
ephy_extensions_manager_startup (EphyExtensionsManager *manager)
{
	char *path;

	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));

	LOG ("EphyExtensionsManager startup");

	/* load the extensions descriptions */
	path = g_build_filename (ephy_dot_dir (), "extensions", NULL);
	ephy_extensions_manager_load_dir (manager, path);
	g_free (path);

	ephy_extensions_manager_load_dir (manager, EXTENSIONS_DIR);

	active_extensions_notifier (NULL, 0, NULL, manager);
	manager->priv->active_extensions_notifier_id =
		eel_gconf_notification_add
			(CONF_LOADED_EXTENSIONS,
			 (GConfClientNotifyFunc) active_extensions_notifier,
			 manager);
}

static void
ephy_extensions_manager_dispose (GObject *object)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (object);
	EphyExtensionsManagerPrivate *priv = manager->priv;

#ifdef ENABLE_LEGACY_FORMAT
	if (priv->xml2ini_xsl != NULL)
	{
		xsltFreeStylesheet (priv->xml2ini_xsl);
		priv->xml2ini_xsl = NULL;
	}
#endif

	if (priv->active_extensions_notifier_id != 0)
	{
		eel_gconf_notification_remove (priv->active_extensions_notifier_id);
		priv->active_extensions_notifier_id = 0;
	}

	if (priv->reload_hash != NULL)
	{
		g_hash_table_destroy (priv->reload_hash);
		priv->reload_hash = NULL;
	}

	if (priv->sync_timeout_id != 0)
	{
		g_source_remove (priv->sync_timeout_id);
		priv->sync_timeout_id = 0;
	}

	if (priv->dir_monitors != NULL)
	{
		g_list_foreach (priv->dir_monitors, (GFunc) gnome_vfs_monitor_cancel, NULL);
		g_list_free (priv->dir_monitors);
		priv->dir_monitors = NULL;
	}

	if (priv->extensions != NULL)
	{
		g_list_foreach (priv->extensions, (GFunc) g_object_unref, NULL);
		g_list_free (priv->extensions);
		priv->extensions = NULL;
	}

	if (priv->factories != NULL)
	{
		/* FIXME release loaded loaders */
		g_list_foreach (priv->factories, (GFunc) free_loader_info, NULL);
		g_list_free (priv->factories);
		priv->factories = NULL;
	}

	if (priv->data != NULL)
	{
		g_list_foreach (priv->data, (GFunc) free_extension_info, NULL);
		g_list_free (priv->data);
		priv->data = NULL;
	}

	if (priv->windows != NULL)
	{
		g_list_free (priv->windows);
		priv->windows = NULL;
	}

	parent_class->dispose (object);
}

static void
attach_extension_to_window (EphyExtension *extension,
			    EphyWindow *window)
{
	attach_window (window, extension);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("Attach window %p", window);

	g_list_foreach (manager->priv->extensions,
			(GFunc) attach_extension_to_window, window);

	manager->priv->windows = g_list_prepend (manager->priv->windows, window);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);
	GList *tabs, *l;

	LOG ("Detach window %p", window);

	manager->priv->windows = g_list_remove (manager->priv->windows, window);

	g_object_ref (window);

	/* Detach tabs (uses impl_detach_tab) */
	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l; l = l->next)
	{
		ephy_extension_detach_tab (extension, window,
					   EPHY_TAB (l->data));
	}
	g_list_free (tabs);

	/* Then detach the window */
	g_list_foreach (manager->priv->extensions,
			(GFunc) ephy_extension_detach_window, window);

	g_object_unref (window);
}

static void
impl_attach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyTab *tab)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);
	GList *l;

	LOG ("Attach window %p tab %p", window, tab);

	for (l = manager->priv->extensions; l; l = l->next)
	{
		ephy_extension_attach_tab (EPHY_EXTENSION (l->data),
					   window, tab);
	}
}

static void
impl_detach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyTab *tab)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);
	GList *l;

	LOG ("Detach window %p tab %p", window, tab);

	g_object_ref (window);
	g_object_ref (tab);

	for (l = manager->priv->extensions; l; l = l->next)
	{
		ephy_extension_detach_tab (EPHY_EXTENSION (l->data),
					   window, tab);
	}

	g_object_unref (tab);
	g_object_unref (window);
}

static void
ephy_extensions_manager_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
	iface->attach_tab    = impl_attach_tab;
	iface->detach_tab    = impl_detach_tab;
}

static void
ephy_extensions_manager_class_init (EphyExtensionsManagerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class->dispose = ephy_extensions_manager_dispose;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyExtensionsManagerClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	signals[ADDED] =
		g_signal_new ("added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyExtensionsManagerClass, added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	signals[REMOVED] =
		g_signal_new ("removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyExtensionsManagerClass, removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	
	g_type_class_add_private (object_class, sizeof (EphyExtensionsManagerPrivate));
}
