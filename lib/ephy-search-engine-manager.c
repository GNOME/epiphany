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

#include "ephy-settings.h"
#include "ephy-prefs.h"

#define FALLBACK_ADDRESS "https://duckduckgo.com/?q=%s&t=epiphany"

enum {
  SEARCH_ENGINES_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _EphySearchEngineManager {
  GObject parent_instance;
  GHashTable *search_engines;
};

typedef struct {
  char *address;
  char *bang;
} EphySearchEngineInfo;

G_DEFINE_TYPE (EphySearchEngineManager, ephy_search_engine_manager, G_TYPE_OBJECT)

static void
ephy_search_engine_info_free (EphySearchEngineInfo *info)
{
  g_free (info->address);
  g_free (info->bang);
  g_free (info);
}

static EphySearchEngineInfo *
ephy_search_engine_info_new (const char *address,
                             const char *bang)
{
  EphySearchEngineInfo *info;
  info = g_malloc (sizeof (EphySearchEngineInfo));
  info->address = g_strdup (address);
  info->bang = g_strdup (bang);
  return info;
}

static void
search_engines_changed_cb (GSettings *settings,
                           char      *key,
                           gpointer   user_data)
{
  g_signal_emit (EPHY_SEARCH_ENGINE_MANAGER (user_data),
                 signals[SEARCH_ENGINES_CHANGED], 0);
}

static void
ephy_search_engine_manager_init (EphySearchEngineManager *manager)
{
  const char *address;
  const char *bang;
  char *name;
  GVariantIter *iter = NULL;

  manager->search_engines = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   (GDestroyNotify)ephy_search_engine_info_free);

  g_settings_get (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES, "a(sss)", &iter);

  while (g_variant_iter_next (iter, "(s&s&s)", &name, &address, &bang)) {
    g_hash_table_insert (manager->search_engines,
                         name,
                         ephy_search_engine_info_new (address,
                                                      bang));
  }

  g_signal_connect (EPHY_SETTINGS_MAIN,
                    "changed::search-engines",
                    G_CALLBACK (search_engines_changed_cb), manager);
}

static void
ephy_search_engine_manager_dispose (GObject *object)
{
  EphySearchEngineManager *manager = EPHY_SEARCH_ENGINE_MANAGER (object);

  g_clear_pointer (&manager->search_engines, g_hash_table_destroy);

  G_OBJECT_CLASS (ephy_search_engine_manager_parent_class)->dispose (object);
}

static void
ephy_search_engine_manager_class_init (EphySearchEngineManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_search_engine_manager_dispose;

  signals[SEARCH_ENGINES_CHANGED] = g_signal_new ("changed",
                                                  EPHY_TYPE_SEARCH_ENGINE_MANAGER,
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL, NULL, NULL,
                                                  G_TYPE_NONE, 0);
}

EphySearchEngineManager *
ephy_search_engine_manager_new (void)
{
  return EPHY_SEARCH_ENGINE_MANAGER (g_object_new (EPHY_TYPE_SEARCH_ENGINE_MANAGER, NULL));
}

const char *
ephy_search_engine_manager_get_address (EphySearchEngineManager *manager,
                                        const char              *name)
{
  EphySearchEngineInfo *info;

  info = (EphySearchEngineInfo *)g_hash_table_lookup (manager->search_engines, name);

  if (info)
    return info->address;

  return NULL;
}

const char *
ephy_search_engine_manager_get_default_search_address (EphySearchEngineManager *manager)
{
  char *name;
  const char *address;

  name = ephy_search_engine_manager_get_default_engine (manager);
  address = ephy_search_engine_manager_get_address (manager, name);
  g_free (name);

  return address ? address : FALLBACK_ADDRESS;
}

const char *
ephy_search_engine_manager_get_bang (EphySearchEngineManager *manager,
                                     const char              *name)
{
  EphySearchEngineInfo *info;

  info = (EphySearchEngineInfo *)g_hash_table_lookup (manager->search_engines, name);

  if (info)
    return info->bang;

  return NULL;
}

char *
ephy_search_engine_manager_get_default_engine (EphySearchEngineManager *manager)
{
  return g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_DEFAULT_SEARCH_ENGINE);
}

gboolean
ephy_search_engine_manager_set_default_engine (EphySearchEngineManager *manager,
                                               const char              *name)
{
  if (!g_hash_table_contains (manager->search_engines, name))
    return FALSE;

  return g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_DEFAULT_SEARCH_ENGINE, name);
}

char **
ephy_search_engine_manager_get_names (EphySearchEngineManager *manager)
{
  GHashTableIter iter;
  gpointer key;
  char **search_engine_names;
  guint size;
  guint i = 0;

  size = g_hash_table_size (manager->search_engines);
  search_engine_names = g_new0 (char *, size + 1);

  g_hash_table_iter_init (&iter, manager->search_engines);

  while (g_hash_table_iter_next (&iter, &key, NULL))
    search_engine_names[i++] = g_strdup ((char *)key);

  return search_engine_names;
}

char **
ephy_search_engine_manager_get_bangs (EphySearchEngineManager *manager)
{
  GHashTableIter iter;
  gpointer value;
  char **search_engine_bangs;
  guint size;
  guint i = 0;

  size = g_hash_table_size (manager->search_engines);
  search_engine_bangs = g_new0 (char *, size + 1);

  g_hash_table_iter_init (&iter, manager->search_engines);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    search_engine_bangs[i++] = ((EphySearchEngineInfo *)value)->bang;

  return search_engine_bangs;
}

static void
ephy_search_engine_manager_apply_settings (EphySearchEngineManager *manager)
{
  GHashTableIter iter;
  EphySearchEngineInfo *info;
  gpointer key;
  gpointer value;
  GVariantBuilder builder;
  GVariant *variant;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));
  g_hash_table_iter_init (&iter, manager->search_engines);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    info = (EphySearchEngineInfo *)value;
    g_variant_builder_add (&builder, "(sss)", key, info->address, info->bang);
  }
  variant = g_variant_builder_end (&builder);
  g_settings_set_value (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES, variant);
}

void
ephy_search_engine_manager_add_engine (EphySearchEngineManager *manager,
                                       const char              *name,
                                       const char              *address,
                                       const char              *bang)
{
  EphySearchEngineInfo *info;

  info = ephy_search_engine_info_new (address, bang);
  g_hash_table_insert (manager->search_engines, g_strdup (name), info);
  ephy_search_engine_manager_apply_settings (manager);
}

void
ephy_search_engine_manager_delete_engine (EphySearchEngineManager *manager,
                                          const char              *name)
{
  g_hash_table_remove (manager->search_engines, name);
  ephy_search_engine_manager_apply_settings (manager);
}

void
ephy_search_engine_manager_modify_engine (EphySearchEngineManager *manager,
                                          const char              *name,
                                          const char              *address,
                                          const char              *bang)
{
  EphySearchEngineInfo *info;

  info = ephy_search_engine_info_new (address, bang);
  g_hash_table_replace (manager->search_engines,
                        g_strdup (name),
                        info);
  ephy_search_engine_manager_apply_settings (manager);
}

const char *
ephy_search_engine_manager_engine_from_bang (EphySearchEngineManager *manager,
                                             const char              *bang)
{
  GHashTableIter iter;
  EphySearchEngineInfo *info;
  gpointer key;
  gpointer value;

  g_hash_table_iter_init (&iter, manager->search_engines);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    info = (EphySearchEngineInfo *)value;
    if (g_strcmp0 (bang, info->bang) == 0)
      return (const char *)key;
  }

  return NULL;
}

static char *
ephy_search_engine_manager_replace_pattern (const char *string,
                                            const char *pattern,
                                            const char *replace)
{
  gchar **strings;
  GString *buffer;

  strings = g_strsplit (string, pattern, -1);

  buffer = g_string_new (NULL);

  for (guint i = 0; strings[i] != NULL; i++) {
    if (i > 0)
      g_string_append (buffer, replace);

    g_string_append (buffer, strings[i]);
  }

  g_strfreev (strings);

  return g_string_free (buffer, FALSE);
}

char *
ephy_search_engine_manager_build_search_address (EphySearchEngineManager *manager,
                                                 const char              *name,
                                                 const char              *search)
{
  EphySearchEngineInfo *info;

  info = (EphySearchEngineInfo *)g_hash_table_lookup (manager->search_engines, name);

  if (info == NULL)
    return NULL;

  return ephy_search_engine_manager_replace_pattern (info->address, "%s", search);
}

char *
ephy_search_engine_manager_parse_bang_search (EphySearchEngineManager *manager,
                                              const char              *search)
{
  GHashTableIter iter;
  EphySearchEngineInfo *info;
  gpointer value;
  GString *buffer;
  char *search_address = NULL;

  g_hash_table_iter_init (&iter, manager->search_engines);

  while (g_hash_table_iter_next (&iter, NULL, &value)) {
    info = (EphySearchEngineInfo *)value;
    buffer = g_string_new (info->bang);
    g_string_append (buffer, " ");
    if (strstr (search, buffer->str) == search) {
      search_address = ephy_search_engine_manager_replace_pattern (info->address,
                                                                   "%s",
                                                                   (search + buffer->len));
      g_string_free (buffer, TRUE);
      return search_address;
    }
    g_string_free (buffer, TRUE);
  }

  return search_address;
}
