/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#include "ephy-shell.h"
#include "ephy-session.h" /* Weird (session is an extension) but it works */
#include "ephy-module-loader.h"
#include "ephy-debug.h"

#include "eel-gconf-extensions.h"

#include <gmodule.h>
#include <dirent.h>
#include <string.h>

#define CONF_LOADED_EXTENSIONS "/apps/epiphany/general/active_extensions"

#define EPHY_EXTENSIONS_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerPrivate))

struct EphyExtensionsManagerPrivate
{
	GHashTable *extensions;
	GSList *internal_extensions;
	guint active_extensions_notifier_id;
};

typedef struct
{
	EphyModuleLoader *loader; /* NULL if never loaded */
	EphyExtension *extension; /* NULL if unloaded */
} ExtInfo;

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

static void
free_ext_info (ExtInfo *info)
{
	if (info->extension)
	{
		g_object_unref (info->extension);
	}
	g_free (info);
}

static void
windows_foreach (GFunc func, EphyExtension *extension)
{
	EphySession *session;
	GList *windows;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));

	windows = ephy_session_get_windows (session);

	g_list_foreach (windows, func, extension);

	g_list_free (windows);
}

static void
attach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	ephy_extension_attach_window (extension, window);
}

static void
detach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	ephy_extension_detach_window (extension, window);
}

static EphyExtension *
instantiate_extension (EphyModuleLoader *loader)
{
	EphyExtension *extension;

	extension = EPHY_EXTENSION (ephy_module_loader_factory (loader));

	if (EPHY_IS_EXTENSION (extension))
	{
		windows_foreach ((GFunc) attach_window, extension);

		return extension;
	}

	return NULL;
}

static void
real_load (ExtInfo *info)
{
	if (info->extension != NULL) return;

	if (g_type_module_use (G_TYPE_MODULE (info->loader)) == FALSE)
	{
		g_warning ("Could not load extension file at %s\n",
			   ephy_module_loader_get_path (info->loader));
		return;
	}

	info->extension = instantiate_extension (info->loader);

	if (info->extension == NULL)
	{
		g_warning ("Could not load extension at %s\n",
			   ephy_module_loader_get_path (info->loader));
	}

	g_type_module_unuse (G_TYPE_MODULE (info->loader));
}

/**
 * ephy_extensions_manager_load:
 * @manager: an #EphyExtensionsManager
 * @filename: filename of an extension to load, minus "lib" and "extension.so"
 *
 * Loads the @filename extension.
 **/
void
ephy_extensions_manager_load (EphyExtensionsManager *manager,
			      const char *filename)
{
	GSList *gconf_exts;

	gconf_exts = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);

	if (!g_slist_find_custom (gconf_exts, filename, (GCompareFunc) strcmp))
	{
		gconf_exts = g_slist_prepend (gconf_exts, g_strdup (filename));

		eel_gconf_set_string_list (CONF_LOADED_EXTENSIONS, gconf_exts);
	}

	g_slist_foreach (gconf_exts, (GFunc) g_free, NULL);
	g_slist_free (gconf_exts);
}

static void
real_unload (ExtInfo *info)
{
	if (info->extension == NULL) return; /* not loaded */

	windows_foreach ((GFunc) detach_window, info->extension);

	/*
	 * Only unref the extension in the idle loop; if the extension has its
	 * own functions queued in the idle loop, the functions must exist in
	 * memory before being called.
	 */
	g_idle_add ((GSourceFunc) g_object_unref, info->extension);
	info->extension = NULL;
}

/**
 * ephy_extensions_manager_unload:
 * @manager: an #EphyExtensionsManager
 * @filename: filename of extension to unload, minus "lib" and "extension.so"
 *
 * Unloads the extension specified by @filename.
 *
 * The extension with the same filename can afterwards be reloaded. However,
 * if any GTypes within the extension have changed parent types, Epiphany must
 * be restarted.
 **/
void
ephy_extensions_manager_unload (EphyExtensionsManager *manager,
				const char *filename)
{
	GSList *gconf_exts;
	GSList *l;
	
	gconf_exts = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);

	l = g_slist_find_custom (gconf_exts, filename, (GCompareFunc) strcmp);

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
 * ephy_extensions_manager_add:
 * @manager: an #EphyExtensionsManager
 * @type: GType of the extension to add
 *
 * Creates a new instance of @type (which must be an #EphyExtension) and adds
 * it to @manager. This is only used to load internal Epiphany extensions.
 *
 * Return value: a new instance of @type
 **/
EphyExtension *
ephy_extensions_manager_add (EphyExtensionsManager *manager,
			     GType type)
{
	EphyExtension *extension;

	LOG ("adding extensions of type %s", g_type_name (type))

	extension = EPHY_EXTENSION (g_object_new (type, NULL));
	if (!EPHY_IS_EXTENSION (extension))
	{
		g_object_unref (extension);

		return NULL;
	}

	manager->priv->internal_extensions =
		g_slist_append (manager->priv->internal_extensions, extension);

	return extension;
}

static void
sync_one_extension (const char *name,
		    ExtInfo *info,
		    GSList *wanted_exts)
{
	if (g_slist_find_custom (wanted_exts, name, (GCompareFunc) strcmp))
	{
		real_load (info);
	}
	else
	{
		real_unload (info);
	}
}

static void
ephy_extensions_manager_sync_gconf (EphyExtensionsManager *manager)
{
	GSList *wanted_exts;

	wanted_exts = eel_gconf_get_string_list (CONF_LOADED_EXTENSIONS);

	g_hash_table_foreach (manager->priv->extensions,
			      (GHFunc) sync_one_extension,
			      wanted_exts);

	g_slist_foreach (wanted_exts, (GFunc) g_free, NULL);
	g_slist_free (wanted_exts);
}

static void
ephy_extensions_manager_load_file (EphyExtensionsManager *manager,
				   const char *dir,
				   const char *filename)
{
	ExtInfo *info;
	char *name;
	char *path;

	/* Must match "libBLAHextension.so" */
	if (!g_str_has_prefix (filename, "lib")
		|| !g_str_has_suffix (filename, "extension." G_MODULE_SUFFIX))
	{
		return;
	}

	name = g_strndup (filename + 3,
			  strlen(filename) - 13 - strlen(G_MODULE_SUFFIX));

	if (g_hash_table_lookup (manager->priv->extensions, name) != NULL)
	{
		/* We already have another version stored */
		g_free (name);
		return;
	}

	path = g_build_filename (dir, filename, NULL);

	info = g_new0 (ExtInfo, 1);
	info->loader = ephy_module_loader_new (path);

	g_free (path);

	g_hash_table_insert (manager->priv->extensions, name, info);
}

/**
 * ephy_extensions_manager_load_dir:
 * @manager: an #EphyExtensionsManager
 * @path: directory to load
 *
 * Searches @path for all files matching the pattern
 * &quot;libEXTextension.so&quot; and stores them in @manager, ready to be
 * loaded.
 **/
void
ephy_extensions_manager_load_dir (EphyExtensionsManager *manager,
				  const char *path)
{
	DIR *d;
	struct dirent *e;

	d = opendir (path);
	if (d == NULL)
	{
		return;
	}
	while ((e = readdir (d)) != NULL)
	{
		ephy_extensions_manager_load_file (manager, path, e->d_name);
	}
	closedir (d);

	ephy_extensions_manager_sync_gconf (manager);
}

static void
active_extensions_notifier (GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyExtensionsManager *manager)
{
	ephy_extensions_manager_sync_gconf (manager);
}

static void
ephy_extensions_manager_init (EphyExtensionsManager *manager)
{
	manager->priv = EPHY_EXTENSIONS_MANAGER_GET_PRIVATE (manager);

	LOG ("EphyExtensionsManager initialising")

	manager->priv->extensions = g_hash_table_new_full
		(g_str_hash, g_str_equal,
		 (GDestroyNotify) g_free, (GDestroyNotify) free_ext_info);

	manager->priv->internal_extensions = NULL;

	manager->priv->active_extensions_notifier_id =
		eel_gconf_notification_add (CONF_LOADED_EXTENSIONS,
					    (GConfClientNotifyFunc)
					     active_extensions_notifier,
					    manager);
}

static void
ephy_extensions_manager_finalize (GObject *object)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (object);

	LOG ("EphyExtensionsManager finalising")

	g_hash_table_destroy (manager->priv->extensions);

	g_slist_foreach (manager->priv->internal_extensions,
			 (GFunc) g_object_unref, NULL);
	g_slist_free (manager->priv->internal_extensions);

	eel_gconf_notification_remove
		(manager->priv->active_extensions_notifier_id);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
attach_window_to_info (const char *key,
		       ExtInfo *info,
		       EphyWindow *window)
{
	if (info->extension)
	{
		ephy_extension_attach_window (info->extension, window);
	}
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("multiplexing attach_window")

	g_slist_foreach (manager->priv->internal_extensions,
			 (GFunc) ephy_extension_attach_window, window);

	g_hash_table_foreach (manager->priv->extensions,
			      (GHFunc) attach_window_to_info,
			      window);
}

static void
detach_window_from_info (const char *key,
			 ExtInfo *info,
			 EphyWindow *window)
{
	if (info->extension)
	{
		ephy_extension_detach_window (info->extension, window);
	}
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("multiplexing detach_window")

	g_object_ref (window);

	g_slist_foreach (manager->priv->internal_extensions,
			 (GFunc) ephy_extension_detach_window, window);

	g_hash_table_foreach (manager->priv->extensions,
			      (GHFunc) detach_window_from_info,
			      window);

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

	g_type_class_add_private (object_class, sizeof (EphyExtensionsManagerPrivate));
}

EphyExtensionsManager *
ephy_extensions_manager_new (void)
{
	return EPHY_EXTENSIONS_MANAGER (g_object_new (EPHY_TYPE_EXTENSIONS_MANAGER, NULL));
}
