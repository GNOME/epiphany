/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2004 Adam Hooper
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

#include "ephy-extensions-manager.h"

#include "ephy-loader.h"
#include "ephy-shlib-loader.h"

#include "ephy-node-db.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlschemas.h>

#include <gmodule.h>
#include <dirent.h>
#include <string.h>

#define CONF_LOADED_EXTENSIONS	"/apps/epiphany/general/active_extensions"
#define SCHEMA_FILE		"/epiphany-extension.xsd"

#define EPHY_EXTENSIONS_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerPrivate))

struct _EphyExtensionsManagerPrivate
{
	gboolean initialised;

	GList *data;
	GList *factories;
	GList *extensions;
	GList *windows;
	guint active_extensions_notifier_id;

	xmlSchemaPtr schema;
	xmlSchemaValidCtxtPtr schema_ctxt;
};

typedef struct
{
	EphyExtensionInfo info;
	guint version;
	gboolean load_deferred;
	gboolean load_failed;

	xmlChar *gettext_domain;
	xmlChar *locale_directory;
	xmlChar *loader_type;
	GData *loader_attributes;

	EphyLoader *loader; /* NULL if never loaded */
	GObject *extension; /* NULL if unloaded */
} ExtensionInfo;

typedef struct
{
	char *type;
	EphyLoader *loader;
} LoaderInfo;

enum
{
	CHANGED,
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

	if (type == 0)
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

	LOG ("Adding '%s' to extensions", identifier)

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

	LOG ("Removing '%s' from extensions", identifier)

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

	LOG ("Registering internal extension of type %s",
	     g_type_name (((GTypeClass *) object)->g_type))

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
	xmlFree ((xmlChar *) einfo->name);
	xmlFree ((xmlChar *) einfo->description);
	g_list_foreach (einfo->authors, (GFunc) xmlFree, NULL);
	g_list_free (einfo->authors);
	xmlFree ((xmlChar *) einfo->url);
	xmlFree ((xmlChar *) info->gettext_domain);
	xmlFree ((xmlChar *) info->locale_directory);
	xmlFree ((xmlChar *) info->loader_type);
	g_datalist_clear (&info->loader_attributes);

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

typedef enum
{
	STATE_START,
	STATE_STOP,
	STATE_ERROR,
	STATE_EXTENSION,
	STATE_NAME,
	STATE_DESCRIPTION,
	STATE_VERSION,
	STATE_AUTHOR,
	STATE_URL,
	STATE_GETTEXT_DOMAIN,
	STATE_LOCALE_DIRECTORY,
	STATE_LOADER,
	STATE_LOADER_ATTRIBUTE,
	STATE_LOAD_DEFERRED,
} ParserState;

static void
ephy_extensions_manager_load_file (EphyExtensionsManager *manager,
				   const char *dir,
				   const char *filename)
{
	xmlDocPtr doc;
	xmlTextReaderPtr reader;
	ParserState state = STATE_START;
	GQuark attr_quark = 0;
	EphyExtensionInfo *einfo;
	ExtensionInfo *info;
	int ret;
	char *identifier, *dot, *path;

	LOG ("Loading description file '%s'", filename)

	identifier = g_path_get_basename (filename);
	dot = strstr (identifier, ".xml");
	g_return_if_fail (dot != NULL);
	*dot = '\0';

	if (g_list_find_custom (manager->priv->data, identifier,
				(GCompareFunc) find_extension_info) != NULL)
	{
		g_warning ("Extension description for '%s' already read!\n",
			   identifier);
		g_free (identifier);
		return;
	}

	path = g_build_filename (dir, filename, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_warning ("'%s' doesn't exist\n", filename);
		g_free (identifier);
		g_free (path);
		return;
	}

	/* FIXME: Ideally we'd put the schema validator in the reader. libxml2
	 * doesn't seem to support that at this point in time, so we've got to
	 * put the schema validation on the Doc Tree and then pass that to the
	 * reader. (maybe switch to RelaxNG?)
	 */
	doc = xmlParseFile (path);
	g_free (path);

	if (doc == NULL)
	{
		g_warning ("Couldn't read '%s'\n", filename);
		g_free (identifier);
		return;
	}

	if (manager->priv->schema_ctxt)
	{
		if (xmlSchemaValidateDoc (manager->priv->schema_ctxt, doc))
		{
			g_warning ("Validation errors in '%s'\n", filename);
			xmlFreeDoc (doc);
			return;
		}
	}
	
	reader = xmlReaderWalker (doc);
	g_return_if_fail (reader != NULL);

	info = g_new0 (ExtensionInfo, 1);
	einfo = (EphyExtensionInfo *) info;
	einfo->identifier = identifier;
	g_datalist_init (&info->loader_attributes);

	ret = xmlTextReaderRead (reader);

	while (ret == 1)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (state == STATE_LOADER &&
		    type == XML_READER_TYPE_ELEMENT &&
		    xmlStrEqual (tag, (const xmlChar *) "attribute"))
		{
			xmlChar *name;

			state = STATE_LOADER_ATTRIBUTE;

			name = xmlTextReaderGetAttribute (reader, (const xmlChar *) "name");
			attr_quark = g_quark_from_string ((const char *) name);
			xmlFree (name);
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "author"))
		{
			state = STATE_AUTHOR;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "description"))
		{
			state = STATE_DESCRIPTION;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "gettext-domain"))
		{
			state = STATE_GETTEXT_DOMAIN;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "load-deferred"))
		{
			state = STATE_LOAD_DEFERRED;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "locale-directory"))
		{
			state = STATE_LOCALE_DIRECTORY;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "name"))
		{
			state = STATE_NAME;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "url"))
		{
			state = STATE_URL;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "version"))
		{
			state = STATE_VERSION;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "loader"))
		{
			state = STATE_LOADER;

			info->loader_type = xmlTextReaderGetAttribute (reader, (const xmlChar *) "type");
		}
		else if (state == STATE_LOADER_ATTRIBUTE &&
			 type == XML_READER_TYPE_TEXT &&
			 attr_quark != 0)
		{
			xmlChar *value;

			value = xmlTextReaderValue (reader);

			g_datalist_id_set_data_full (&info->loader_attributes,
						     attr_quark, value,
						     (GDestroyNotify) xmlFree);
			attr_quark = 0;
		}
		else if (state == STATE_LOADER_ATTRIBUTE &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "attribute"))
		{
			state = STATE_LOADER;
		}
		else if (state == STATE_AUTHOR &&
			 type == XML_READER_TYPE_TEXT)
		{
			einfo->authors = g_list_prepend
				(einfo->authors, xmlTextReaderValue (reader));
		}
		else if (state == STATE_DESCRIPTION &&
			 type == XML_READER_TYPE_TEXT)
		{
			
			einfo->description = xmlTextReaderValue (reader);
		}
		else if (state == STATE_GETTEXT_DOMAIN &&
			 type == XML_READER_TYPE_TEXT)
		{
			info->gettext_domain = xmlTextReaderValue (reader);
		}
		else if (state == STATE_LOAD_DEFERRED &&
			 type == XML_READER_TYPE_TEXT)
		{
			const xmlChar *value;

			value = xmlTextReaderConstValue (reader);
			info->load_deferred =
				(value != NULL && xmlStrEqual (value, (const xmlChar *) "true"));
		}
		else if (state == STATE_LOCALE_DIRECTORY &&
			 type == XML_READER_TYPE_TEXT)
		{
			info->locale_directory = xmlTextReaderValue (reader);
		}
		else if (state == STATE_NAME &&
			 type == XML_READER_TYPE_TEXT)
		{
			einfo->name = xmlTextReaderValue (reader);
		}
		else if (state == STATE_VERSION &&
			 type == XML_READER_TYPE_TEXT)
		{
			info->version = (guint) strtol ((const char *) xmlTextReaderConstValue (reader), NULL, 10);
		}
		else if (state == STATE_URL &&
			 type == XML_READER_TYPE_TEXT)
		{
			einfo->url = xmlTextReaderValue (reader);
		}
		else if (state == STATE_AUTHOR &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "author"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_DESCRIPTION &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "description"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_GETTEXT_DOMAIN &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "gettext-domain"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_LOCALE_DIRECTORY &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "locale-directory"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_LOADER &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "loader"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_NAME &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "name"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_URL &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "url"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_VERSION &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "version"))
		{
			state = STATE_EXTENSION;
		}
		else if (type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE ||
			 type == XML_READER_TYPE_WHITESPACE ||
			 type == XML_READER_TYPE_TEXT)
		{
			/* eat it */
		}
		else if (state == STATE_START &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "extension"))
		{
			state = STATE_EXTENSION;
		}
		else if (state == STATE_EXTENSION &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, (const xmlChar *) "extension"))
		{
			state = STATE_STOP;
		}
		else
		{
			const xmlChar *content;

			content = xmlTextReaderConstValue (reader);
			g_warning ("tag '%s' of type %d in state %d with content '%s' was unexpected!",
				   tag, type, state, content ? (char *) content : "(null)");

			state = STATE_ERROR;
			break;
		}

		ret = xmlTextReaderRead (reader);
	}

	xmlFreeTextReader (reader);
	xmlFreeDoc (doc);

	/* sanity check */
	if (ret < 0 || state != STATE_STOP ||
	    einfo->name == NULL || einfo->description == NULL ||
	    info->loader_type == NULL || info->loader_type[0] == '\0')
	{
		free_extension_info (info);
		return;
	}

	manager->priv->data = g_list_prepend (manager->priv->data, info);
}

static int
find_loader (const LoaderInfo *info,
	      const char *type)
{
	return strcmp (info->type, type);
}

static EphyLoader *
get_loader_for_type (EphyExtensionsManager *manager,
		      const char *type)
{
	LoaderInfo *info;
	GList *l;

	LOG ("Looking for loader for type '%s'", type)

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

	/* try to load a loader */
	g_return_val_if_reached (NULL);

	return NULL;
}


static void
attach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	ephy_extension_attach_window (extension, window);
}

static void
load_extension (EphyExtensionsManager *manager,
		ExtensionInfo *info)
{
	EphyLoader *loader;

	g_return_if_fail (info->extension == NULL);

	LOG ("Loading extension '%s'", info->info.identifier)

	/* don't try again */
	if (info->load_failed) return;

	/* get a loader */
	loader = get_loader_for_type (manager, info->loader_type);
	if (loader == NULL)
	{
		g_warning ("No loader found for extension '%s' of type '%s'\n",
			   info->info.identifier, info->loader_type);
		return;
	}

	info->loader = loader;

	info->extension = ephy_loader_get_object (loader,
						   &info->loader_attributes);

	if (info->extension != NULL)
	{
		manager->priv->extensions =
			g_list_prepend (manager->priv->extensions,
					g_object_ref (info->extension));

		g_list_foreach (manager->priv->windows, (GFunc) attach_window,
				info->extension);

		info->info.active = TRUE;

		g_signal_emit (manager, signals[CHANGED], 0, info);
	}
	else
	{
		info->load_failed = TRUE;
	}
}

static void
detach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	ephy_extension_detach_window (extension, window);
}

static void
unload_extension (EphyExtensionsManager *manager,
		  ExtensionInfo *info)
{
	g_return_if_fail (info->loader != NULL);
	g_return_if_fail (info->extension != NULL || info->load_failed);

	LOG ("Unloading extension '%s'", info->info.identifier)

	if (info->load_failed) return;

	g_list_foreach (manager->priv->windows, (GFunc) detach_window,
			info->extension);

	manager->priv->extensions =
		g_list_remove (manager->priv->extensions, info->extension);

	ephy_loader_release_object (info->loader, G_OBJECT (info->extension));
	g_object_unref (info->extension);

	info->info.active = FALSE;
	info->extension = NULL;

	g_signal_emit (manager, signals[CHANGED], 0, info);
}

static void
ephy_extensions_manager_load_dir (EphyExtensionsManager *manager,
				  const char *path)
{
	DIR *d;
	struct dirent *e;

	LOG ("Scanning directory '%s'", path)

	START_PROFILER ("Scanning directory")

	d = opendir (path);
	if (d == NULL)
	{
		return;
	}
	while ((e = readdir (d)) != NULL)
	{
		if (g_str_has_suffix (e->d_name, ".xml"))
		{
			ephy_extensions_manager_load_file (manager, path, e->d_name);
		}
	}
	closedir (d);

	STOP_PROFILER ("Scanning directory")
}

static void
active_extensions_notifier (GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyExtensionsManager *manager)
{
	GSList *active_extensions;
	GList *l;
	gboolean active;
	ExtensionInfo *info;

	LOG ("Synching changed list of active extensions")

	active_extensions = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);

	for (l = manager->priv->data; l != NULL; l = l->next)
	{
		info = (ExtensionInfo *) l->data;

		active = (g_slist_find_custom (active_extensions,
					       info->info.identifier,
					       (GCompareFunc) strcmp) != NULL);

		LOG ("Extension '%s' is %sactive and %sloaded",
		     info->info.identifier,
		     active ? "" : "not ",
		     info->info.active ? "" : "not ")

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
}

static void
xml_error_cb (EphyExtensionsManager *manager,
	      const char *msg,
	      ...)

{
	va_list args;

	va_start (args, msg);
	g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, msg, args);
	va_end(args);
}

static void
init_schema_ctxt (EphyExtensionsManager *manager)
{
	xmlSchemaParserCtxtPtr parse_ctxt;
	const char *filename;

	manager->priv->schema = NULL;
	manager->priv->schema_ctxt = NULL;

	filename = ephy_file (SCHEMA_FILE);
	g_return_if_fail (filename != NULL);

	parse_ctxt = xmlSchemaNewParserCtxt (filename);
	if (parse_ctxt == NULL)
	{
		g_warning ("Error opening extensions description schema file "
			   "\"" SCHEMA_FILE "\"");
		return;
	}

	manager->priv->schema = xmlSchemaParse (parse_ctxt);
	xmlSchemaFreeParserCtxt (parse_ctxt);
	if (manager->priv->schema == NULL)
	{
		g_warning ("Error parsing extensions description schema file "
			   "\"" SCHEMA_FILE "\"");
		return;
	}

	manager->priv->schema_ctxt = xmlSchemaNewValidCtxt
		(manager->priv->schema);
	if (manager->priv->schema == NULL)
	{
		g_warning ("Error creating extensions description schema "
			   "validation context for \"" SCHEMA_FILE "\"");
		return;
	}

	xmlSchemaSetValidErrors (manager->priv->schema_ctxt,
				 (xmlSchemaValidityErrorFunc) xml_error_cb,
				 (xmlSchemaValidityWarningFunc) xml_error_cb,
				 manager);
}

static void
ephy_extensions_manager_init (EphyExtensionsManager *manager)
{
	char *path;

	manager->priv = EPHY_EXTENSIONS_MANAGER_GET_PRIVATE (manager);

	LOG ("EphyExtensionsManager initialising")

	init_schema_ctxt (manager);

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
ephy_extensions_manager_finalize (GObject *object)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (object);
	EphyExtensionsManagerPrivate *priv = manager->priv;

	LOG ("EphyExtensionsManager finalising")

	eel_gconf_notification_remove (manager->priv->active_extensions_notifier_id);

	g_list_foreach (priv->extensions, (GFunc) g_object_unref, NULL);
	g_list_free (priv->extensions);

	g_list_foreach (priv->factories, (GFunc) free_loader_info, NULL);
	g_list_free (priv->factories);

	g_list_foreach (priv->data, (GFunc) free_extension_info, NULL);
	g_list_free (priv->data);

	g_list_free (priv->windows);

	if (priv->schema)
	{
		xmlSchemaFree (priv->schema);
	}
	if (priv->schema_ctxt)
	{
		xmlSchemaFreeValidCtxt (priv->schema_ctxt);
	}

	parent_class->finalize (object);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("Attach")

	g_list_foreach (manager->priv->extensions,
			(GFunc) ephy_extension_attach_window, window);

	manager->priv->windows = g_list_prepend (manager->priv->windows, window);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("Detach")

	manager->priv->windows = g_list_remove (manager->priv->windows, window);

	g_object_ref (window);

	g_list_foreach (manager->priv->extensions,
			(GFunc) ephy_extension_detach_window, window);

	g_object_unref (window);
}

static void
ephy_extensions_manager_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
}

static void
ephy_extensions_manager_class_init (EphyExtensionsManagerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class->finalize = ephy_extensions_manager_finalize;

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
	
	g_type_class_add_private (object_class, sizeof (EphyExtensionsManagerPrivate));
}
