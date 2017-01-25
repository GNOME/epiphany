/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Cedric Le Moigne <cedlemo@gmx.com>
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-search-engine-manager.h"

#include "ephy-file-helpers.h"
#include "ephy-string.h"

#define G_SETTINGS_ENABLE_BACKEND 1
#include <gio/gsettingsbackend.h>

struct _EphySearchEngineManager
{
  GObject parent_instance;
  GSettingsBackend *backend;
  GSettings *settings;
  GHashTable *search_engines;
};

G_DEFINE_TYPE (EphySearchEngineManager, ephy_search_engine_manager, G_TYPE_OBJECT)

static void
ephy_search_engine_manager_init (EphySearchEngineManager *manager)
{
  char *key_file = NULL;
  char **search_engine_urls;
  char **search_engine_names;
  uint n_engines;

  manager->search_engines = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  key_file = g_build_filename (ephy_dot_dir (), G_DIR_SEPARATOR_S "search-engines.ini", NULL);

  manager->backend = g_keyfile_settings_backend_new (key_file, "/", "SearchEngines");
  g_free (key_file);

  manager->settings = g_settings_new_with_backend_and_path ("org.gnome.Epiphany.search-engines",
                                                            manager->backend,
                                                            "/org/gnome/epiphany/search-engines/");

  search_engine_names = g_settings_get_strv (manager->settings,
                                             "search-engines-names");
  search_engine_urls = g_settings_get_strv (manager->settings,
                                            "search-engines-urls");
  n_engines = g_strv_length (search_engine_names);

  for (uint i = 0; i < n_engines; ++i) {
    const char *name = search_engine_names[i];
    const char *url = search_engine_urls[i];
    g_hash_table_insert (manager->search_engines,
                         g_strdup (name),
                         g_strdup (url));
  }

  g_strfreev (search_engine_names);
  g_strfreev (search_engine_urls);
}

static void
ephy_search_engine_manager_dispose (GObject *object)
{
  EphySearchEngineManager *manager = EPHY_SEARCH_ENGINE_MANAGER (object);

  g_clear_pointer (&manager->search_engines, g_hash_table_destroy);
  g_clear_object (&manager->backend);
  g_clear_object (&manager->settings);

  G_OBJECT_CLASS (ephy_search_engine_manager_parent_class)->dispose (object);
}

static void
ephy_search_engine_manager_class_init (EphySearchEngineManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_search_engine_manager_dispose;
}

EphySearchEngineManager *
ephy_search_engine_manager_new (void)
{
  return EPHY_SEARCH_ENGINE_MANAGER (g_object_new (EPHY_TYPE_SEARCH_ENGINE_MANAGER, NULL));
}

const char *
ephy_search_engine_manager_get_url (EphySearchEngineManager *manager,
                                    const char              *name)
{
  if (!manager->search_engines)
    return NULL;

  return g_hash_table_lookup (manager->search_engines, name);
}

char *
ephy_search_engine_manager_get_default_engine (EphySearchEngineManager *manager)
{
  if (!manager->settings)
    return NULL;

  return g_settings_get_string (manager->settings, "default-search-engine");
}

char **
ephy_search_engine_manager_get_names (EphySearchEngineManager *manager)
{
  if (!manager->settings)
    return NULL;

  return g_settings_get_strv (manager->settings, "search-engines-names");
}

GSettings *
ephy_search_engine_manager_get_settings (EphySearchEngineManager *manager)
{
  return g_settings_new_with_backend_and_path ("org.gnome.Epiphany.search-engines",
                                               manager->backend,
                                               "/org/gnome/epiphany/search-engines/");
}

static void
ephy_search_engine_manager_apply_settings (EphySearchEngineManager *manager)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  int size;
  int i = 0;
  char **search_engine_names;
  char **search_engine_urls;

  if (!manager->search_engines)
    return;

  size = g_hash_table_size (manager->search_engines);

  search_engine_names = g_malloc(size + 1);
  search_engine_urls = g_malloc(size + 1);

  g_hash_table_iter_init (&iter, manager->search_engines);

  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    search_engine_names[i] = key;
    search_engine_urls[i] = value;
    i++;
  }

  search_engine_names[size] = NULL;
  search_engine_urls[size] = NULL;

  if (!manager->settings)
    return;

  g_settings_set_strv (manager->settings,
                       "search-engines-names",
                       (const char * const*) search_engine_names);
  g_settings_set_strv (manager->settings,
                       "search-engines-urls",
                       (const char * const*) search_engine_urls);

  g_free (search_engine_names);
  g_free (search_engine_urls);
}

void
ephy_search_engine_manager_add_engine (EphySearchEngineManager *manager,
                                       const char              *name,
                                       const char              *url)
{
  if (!manager->search_engines)
    return;

  g_hash_table_insert (manager->search_engines, g_strdup (name), g_strdup (url));
  ephy_search_engine_manager_apply_settings (manager);
}

void
ephy_search_engine_manager_delete_engine (EphySearchEngineManager *manager,
                                          const char              *name)
{
  if (!manager->search_engines)
    return;

  g_hash_table_remove (manager->search_engines, name);
  ephy_search_engine_manager_apply_settings (manager);
}

void
ephy_search_engine_manager_modify_engine (EphySearchEngineManager *manager,
                                          const char              *name,
                                          const char              *url)
{
  if (!manager->search_engines)
    return;

  g_hash_table_replace (manager->search_engines,
                        g_strdup (name),
                        g_strdup (url));
  ephy_search_engine_manager_apply_settings (manager);
}
