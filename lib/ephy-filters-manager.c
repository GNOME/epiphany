/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2017 Igalia S.L.
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
#include "ephy-filters-manager.h"

#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-uri-tester-shared.h"

#include <gio/gio.h>

#define ADBLOCK_FILTER_UPDATE_FREQUENCY 24 * 60 * 60 /* In seconds */

struct _EphyFiltersManager {
  GObject parent_instance;

  char *filters_dir;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (EphyFiltersManager, ephy_filters_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FILTERS_DIR,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static gboolean
adblock_filter_file_is_valid (GFile *file)
{
  GFileInfo *file_info;
  gboolean result = FALSE;

  /* Now check if the local file is too old. */
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_TIME_MODIFIED","G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 NULL);
  if (file_info) {
    if (g_file_info_get_size (file_info) > 0) {
      GTimeVal current_time;
      GTimeVal mod_time;

      g_get_current_time (&current_time);
      g_file_info_get_modification_time (file_info, &mod_time);

      if (current_time.tv_sec > mod_time.tv_sec) {
        gint64 expire_time = mod_time.tv_sec + ADBLOCK_FILTER_UPDATE_FREQUENCY;

        result = current_time.tv_sec < expire_time;
      }
    }
    g_object_unref (file_info);
  }

  return result;
}

typedef struct {
  EphyFiltersManager *manager;

  char *src_uri;
  GFile *filter_file;
  GFile *tmp_file;
} AdblockFilterRetrieveData;

static AdblockFilterRetrieveData *
adblock_filter_retrieve_data_new (EphyFiltersManager *manager,
                                  GFile              *src_file,
                                  GFile              *filter_file)
{
  AdblockFilterRetrieveData* data;
  char *path, *tmp_path;

  data = g_slice_new (AdblockFilterRetrieveData);
  data->manager = g_object_ref (manager);
  data->src_uri = g_file_get_uri (src_file);
  data->filter_file = g_object_ref (filter_file);

  path = g_file_get_path (filter_file);
  tmp_path = g_strdup_printf ("%s.tmp", path);
  g_free (path);
  data->tmp_file = g_file_new_for_path (tmp_path);
  g_free (tmp_path);

  return data;
}

static void
adblock_filter_retrieve_data_free (AdblockFilterRetrieveData *data)
{
  g_object_unref (data->manager);
  g_object_unref (data->filter_file);
  g_object_unref (data->tmp_file);

  g_free (data->src_uri);

  g_slice_free (AdblockFilterRetrieveData, data);
}

static void
retrieve_filter_file_finished (GFile                     *src,
                               GAsyncResult              *result,
                               AdblockFilterRetrieveData *data)
{
  GError *error = NULL;

  if (!g_file_copy_finish (src, result, &error) ||
      !g_file_move (data->tmp_file, data->filter_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
    GFileOutputStream *stream;

    /* If failed to retrieve, create an empty file if it doesn't exist to unblock extensions */
    stream = g_file_create (data->filter_file, G_FILE_CREATE_NONE, NULL, NULL);
    if (stream)
      g_object_unref (stream);

    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error retrieving filter %s: %s\n", data->src_uri, error->message);
    g_error_free (error);
  }

  adblock_filter_retrieve_data_free (data);
}

static void
retrieve_filter_file (EphyFiltersManager *manager,
                      const char         *filter_url,
                      GFile              *file)
{
  GFile *src = g_file_new_for_uri (filter_url);
  AdblockFilterRetrieveData *data;

  data = adblock_filter_retrieve_data_new (manager, src, file);

  g_file_copy_async (src, data->tmp_file,
                     G_FILE_COPY_OVERWRITE,
                     G_PRIORITY_DEFAULT,
                     manager->cancellable,
                     NULL, NULL,
                     (GAsyncReadyCallback)retrieve_filter_file_finished,
                     data);

  g_object_unref (src);
}

static void
remove_old_adblock_filters (EphyFiltersManager *manager,
                            GList              *current_files)
{
  GFile *file;
  GFile *filters_dir;
  GFileEnumerator *enumerator;
  gboolean current_filter;
  char *path;
  GError *error = NULL;

  filters_dir = g_file_new_for_path (manager->filters_dir);
  enumerator = g_file_enumerate_children (filters_dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL,
                                          &error);
  if (error != NULL) {
    g_warning ("Failed to enumerate children of %s: %s", manager->filters_dir, error->message);
    g_error_free (error);
    g_object_unref (filters_dir);
    return;
  }

  /* For each file in the adblock directory, check if it is a currently-enabled
   * and remove it if not, since filter files can be quite large. */
  for (;;) {
    g_file_enumerator_iterate (enumerator, NULL, &file, NULL, &error);
    if (error != NULL) {
      g_warning ("Failed to iterate file enumerator for %s: %s", manager->filters_dir, error->message);
      g_clear_error (&error);
      continue;
    }

    /* Success: no more files left to iterate. */
    if (file == NULL)
      break;

    current_filter = FALSE;
    for (GList *l = current_files; l != NULL; l = l->next) {
      if (g_file_equal (l->data, file)) {
        current_filter = TRUE;
        break;
      }
    }

    if (!current_filter) {
      g_file_delete (file, NULL, &error);
      if (error != NULL) {
        path = g_file_get_path (file);
        g_warning ("Failed to remove %s: %s", path, error->message);
        g_free (path);
        g_clear_error (&error);
      }
    }
  }

  g_object_unref (filters_dir);
  g_object_unref (enumerator);
}

static void
update_adblock_filter_files (EphyFiltersManager *manager)
{
  char **filters;
  GList *files = NULL;

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK))
    return;

  /* One at a time please! */
  g_cancellable_cancel (manager->cancellable);

  filters = g_settings_get_strv (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ADBLOCK_FILTERS);
  for (guint i = 0; filters[i]; i++) {
    GFile *filter_file;

    filter_file = ephy_uri_tester_get_adblock_filter_file (manager->filters_dir, filters[i]);
    if (!adblock_filter_file_is_valid (filter_file))
      retrieve_filter_file (manager, filters[i], filter_file);
    files = g_list_prepend (files, filter_file);
  }

  remove_old_adblock_filters (manager, files);

  g_strfreev (filters);
  g_list_free_full (files, g_object_unref);
}

static void
adblock_filters_changed_cb (GSettings          *settings,
                            char               *key,
                            EphyFiltersManager *manager)
{
  update_adblock_filter_files (manager);
}

static void
enable_adblock_changed_cb (GSettings          *settings,
                           char               *key,
                           EphyFiltersManager *manager)
{
  update_adblock_filter_files (manager);
}

static void
ephy_filters_manager_dispose (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  if (manager->cancellable) {
    g_cancellable_cancel (manager->cancellable);
    g_clear_object (&manager->cancellable);
  }

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->dispose (object);
}

static void
ephy_filters_manager_finalize (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  g_free (manager->filters_dir);

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->finalize (object);
}

static void
ephy_filters_manager_constructed (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->constructed (object);

  /* Note: up here because we must connect *before* reading the settings. */
  g_signal_connect (EPHY_SETTINGS_WEB, "changed::" EPHY_PREFS_WEB_ADBLOCK_FILTERS,
                    G_CALLBACK (adblock_filters_changed_cb), manager);
  g_signal_connect (EPHY_SETTINGS_WEB, "changed::" EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                    G_CALLBACK (enable_adblock_changed_cb), manager);

  g_mkdir_with_parents (manager->filters_dir, 0700);
  update_adblock_filter_files (manager);
}

static void
ephy_filters_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  switch (prop_id) {
    case PROP_FILTERS_DIR:
      manager->filters_dir = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_filters_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  switch (prop_id) {
    case PROP_FILTERS_DIR:
      g_value_set_string (value, manager->filters_dir);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_filters_manager_class_init (EphyFiltersManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_filters_manager_constructed;
  object_class->dispose = ephy_filters_manager_dispose;
  object_class->finalize = ephy_filters_manager_finalize;
  object_class->set_property = ephy_filters_manager_set_property;
  object_class->get_property = ephy_filters_manager_get_property;

  object_properties[PROP_FILTERS_DIR] =
    g_param_spec_string ("filters-dir",
                         "Filters directory",
                         "The directory in which adblock filters are saved",
                         "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);
}

static void
ephy_filters_manager_init (EphyFiltersManager *manager)
{
  manager->cancellable = g_cancellable_new ();
}

EphyFiltersManager *
ephy_filters_manager_new (const char *filters_dir)
{
  return EPHY_FILTERS_MANAGER (g_object_new (EPHY_TYPE_FILTERS_MANAGER,
                                            "filters-dir", filters_dir,
                                            NULL));
}

const char *
ephy_filters_manager_get_adblock_filters_dir (EphyFiltersManager *manager)
{
  return manager->filters_dir;
}
