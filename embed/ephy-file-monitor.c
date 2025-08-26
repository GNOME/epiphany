/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-file-monitor.h"

#include "ephy-debug.h"

#include <string.h>

#define RELOAD_DELAY            250 /* ms */
#define RELOAD_DELAY_MAX_TICKS  40  /* RELOAD_DELAY * RELOAD_DELAY_MAX_TICKS = 10 s */

struct _EphyFileMonitor {
  GObject parent;

  GFileMonitor *monitor;
  gboolean monitor_directory;
  guint reload_scheduled_id;
  guint reload_delay_ticks;

  EphyWebView *view;
};

G_DEFINE_FINAL_TYPE (EphyFileMonitor, ephy_file_monitor, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_file_monitor_cancel (EphyFileMonitor *monitor)
{
  g_assert (EPHY_IS_FILE_MONITOR (monitor));

  if (monitor->monitor) {
    LOG ("Cancelling file monitor");

    g_file_monitor_cancel (G_FILE_MONITOR (monitor->monitor));
    g_object_unref (monitor->monitor);
    monitor->monitor = NULL;
  }

  if (monitor->reload_scheduled_id != 0) {
    LOG ("Cancelling scheduled reload");

    g_source_remove (monitor->reload_scheduled_id);
    monitor->reload_scheduled_id = 0;
  }

  monitor->reload_delay_ticks = 0;
}

static gboolean
ephy_file_monitor_reload_cb (EphyFileMonitor *monitor)
{
  if (monitor->reload_delay_ticks > 0) {
    monitor->reload_delay_ticks--;

    /* Run again. */
    return TRUE;
  }

  if (ephy_web_view_is_loading (monitor->view)) {
    /* Wait a bit to reload if we're still loading! */
    monitor->reload_delay_ticks = RELOAD_DELAY_MAX_TICKS / 2;

    /* Run again. */
    return TRUE;
  }

  monitor->reload_scheduled_id = 0;

  LOG ("Reloading file '%s'", ephy_web_view_get_address (monitor->view));
  webkit_web_view_reload (WEBKIT_WEB_VIEW (monitor->view));

  /* Don't run again. */
  return FALSE;
}

static void
ephy_file_monitor_changed_cb (GFileMonitor      *monitor,
                              GFile             *file,
                              GFile             *other_file,
                              GFileMonitorEvent  event_type,
                              EphyFileMonitor   *file_monitor)
{
  gboolean should_reload;

  switch (event_type) {
    /* These events will always trigger a reload: */
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CREATED:
      should_reload = TRUE;
      break;

    /* These events will only trigger a reload for directories: */
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      should_reload = file_monitor->monitor_directory;
      break;

    /* These events don't trigger a reload: */
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    default:
      should_reload = FALSE;
      break;

    /* These events will never be sent: */
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
      g_assert_not_reached ();
  }

  if (should_reload) {
    /* We make a lot of assumptions here, but basically we know
     * that we just have to reload, by construction.
     * Delay the reload a little bit so we don't endlessly
     * reload while a file is written.
     */
    if (file_monitor->reload_delay_ticks == 0)
      file_monitor->reload_delay_ticks = 1;
    else {
      /* Exponential backoff. */
      file_monitor->reload_delay_ticks = MIN (file_monitor->reload_delay_ticks * 2,
                                              RELOAD_DELAY_MAX_TICKS);
    }

    if (file_monitor->reload_scheduled_id == 0) {
      file_monitor->reload_scheduled_id =
        g_timeout_add (RELOAD_DELAY,
                       (GSourceFunc)ephy_file_monitor_reload_cb, file_monitor);
      g_source_set_name_by_id (file_monitor->reload_scheduled_id, "[epiphany] file_monitor");
    }
  }
}

void
ephy_file_monitor_update_location (EphyFileMonitor *file_monitor,
                                   const char      *address)
{
  gboolean local;
  char *anchor;
  char *url;
  GFile *file;
  GFileType file_type;
  GFileInfo *file_info;

  g_assert (EPHY_IS_FILE_MONITOR (file_monitor));
  g_assert (address);

  ephy_file_monitor_cancel (file_monitor);

  local = g_str_has_prefix (address, "file://");
  if (!local)
    return;

  /* strip off anchors */
  anchor = strchr (address, '#');
  if (anchor)
    url = g_strndup (address, anchor - address);
  else
    url = g_strdup (address);

  file = g_file_new_for_uri (url);
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 0, NULL, NULL);
  if (!file_info) {
    g_object_unref (file);
    g_free (url);
    return;
  }

  file_type = g_file_info_get_file_type (file_info);
  g_object_unref (file_info);

  if (file_type == G_FILE_TYPE_DIRECTORY) {
    file_monitor->monitor = g_file_monitor_directory (file, 0, NULL, NULL);
    g_signal_connect (file_monitor->monitor, "changed",
                      G_CALLBACK (ephy_file_monitor_changed_cb),
                      file_monitor);
    file_monitor->monitor_directory = TRUE;
    LOG ("Installed monitor for directory '%s'", url);
  } else if (file_type == G_FILE_TYPE_REGULAR) {
    file_monitor->monitor = g_file_monitor_file (file, 0, NULL, NULL);
    g_signal_connect (file_monitor->monitor, "changed",
                      G_CALLBACK (ephy_file_monitor_changed_cb),
                      file_monitor);
    file_monitor->monitor_directory = FALSE;
    LOG ("Installed monitor for file '%s'", url);
  }

  g_object_unref (file);
  g_free (url);
}

static void
ephy_file_monitor_dispose (GObject *object)
{
  ephy_file_monitor_cancel (EPHY_FILE_MONITOR (object));

  G_OBJECT_CLASS (ephy_file_monitor_parent_class)->dispose (object);
}

static void
ephy_file_monitor_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EphyFileMonitor *monitor = EPHY_FILE_MONITOR (object);

  switch (prop_id) {
    case PROP_VIEW:
      g_value_set_object (value, monitor->view);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_file_monitor_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphyFileMonitor *monitor = EPHY_FILE_MONITOR (object);

  switch (prop_id) {
    case PROP_VIEW:
      monitor->view = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_file_monitor_class_init (EphyFileMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = ephy_file_monitor_dispose;
  gobject_class->get_property = ephy_file_monitor_get_property;
  gobject_class->set_property = ephy_file_monitor_set_property;

  obj_properties[PROP_VIEW] =
    g_param_spec_object ("view",
                         NULL, NULL,
                         EPHY_TYPE_WEB_VIEW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);
}

static void
ephy_file_monitor_init (EphyFileMonitor *monitor)
{
}

EphyFileMonitor *
ephy_file_monitor_new (EphyWebView *view)
{
  return g_object_new (EPHY_TYPE_FILE_MONITOR,
                       "view", view,
                       NULL);
}
