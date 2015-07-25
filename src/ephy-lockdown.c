/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010, 2012 Igalia S.L.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-lockdown.h"

#include "ephy-action-helper.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-settings.h"

#include <gtk/gtk.h>

#define LOCKDOWN_FLAG 1 << 8

static int
find_name (GtkActionGroup *action_group,
           const char *name)
{
  return g_strcmp0 (gtk_action_group_get_name (action_group), name);
}

static GtkActionGroup *
find_action_group (GtkUIManager *manager,
                   const char *name)
{
  GList *list, *element;

  list = gtk_ui_manager_get_action_groups (manager);
  element = g_list_find_custom (list, name, (GCompareFunc) find_name);
  g_return_val_if_fail (element != NULL, NULL);

  return GTK_ACTION_GROUP (element->data);
}

static void
arbitrary_url_cb (GSettings *settings,
                  char *key,
                  EphyWindow *window)
{
  EphyEmbed *embed;
  const char *address;

  /* Restore the real web page address when disabling entry */
  if (g_settings_get_boolean (settings, key)) {
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    /* embed is NULL on startup */
    if (embed == NULL)
      return;

    address = ephy_web_view_get_display_address (ephy_embed_get_web_view (embed));
    ephy_window_set_location (window, address);
    ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), NULL);
  }
}

static void
fullscreen_cb (GSettings *settings,
               char *key,
               EphyWindow *window)
{
  if (g_settings_get_boolean (settings, key))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

typedef struct {
  char *key;
  char *action;
  char *prop;
} BindAction;

static const BindAction window_actions[] = {
  { EPHY_PREFS_LOCKDOWN_PRINTING, "FilePrint", "sensitive" },

  { EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING, "FileBookmarkPage", "sensitive" },

  { EPHY_PREFS_LOCKDOWN_ARBITRARY_URL, "GoLocation", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "FileSaveAs", "sensitive" },

  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "ViewFullscreen", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "TabsDetach", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "FileNewWindow", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "FileNewWindowIncognito", "sensitive" }
};

static const BindAction popup_actions[] = {
  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "DownloadLinkAs", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "SaveImageAs", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING, "ContextBookmarkPage", "sensitive" },

  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "OpenLinkInNewWindow", "sensitive" }
};

static const BindAction special_toolbar_actions[] = {
  { EPHY_PREFS_LOCKDOWN_HISTORY, "NavigationBack", "visible" },
  { EPHY_PREFS_LOCKDOWN_HISTORY, "NavigationBack", "sensitive" },
  { EPHY_PREFS_LOCKDOWN_HISTORY, "NavigationForward", "visible" },
  { EPHY_PREFS_LOCKDOWN_HISTORY, "NavigationForward", "sensitive" },
};

static gboolean
sensitive_get_mapping (GValue *value,
                       GVariant *variant,
                       gpointer data)
{
  GtkAction *action;
  gboolean active, before, after;

  action = GTK_ACTION (data);
  active = g_variant_get_boolean (variant);

  before = gtk_action_get_sensitive (action);
  ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, active);
  after = gtk_action_get_sensitive (action);

  /* Set (GtkAction::sensitive) to the value in GSettings _only if_
   * the LOCKDOWN_FLAG had some real effect in the GtkAction */
  g_value_set_boolean (value, (before != after) ? after : before);

  return TRUE;
}

static void
bind_settings_and_actions (GSettings *settings,
                           GtkActionGroup *action_group,
                           const BindAction *actions,
                           int actions_n)
{
  int i;

  for (i = 0; i < actions_n; i++) {
    GtkAction *action;

    action = gtk_action_group_get_action (action_group,
                  actions[i].action);

    if (g_strcmp0 (actions[i].prop, "visible") == 0) {
      g_settings_bind (settings, actions[i].key,
                       action, actions[i].prop,
                       G_SETTINGS_BIND_GET |
                       G_SETTINGS_BIND_INVERT_BOOLEAN);
    } else {
      /* We need a custom get_mapping for 'sensitive'
       * properties, see usage of
       * ephy_action_change_sensitivity_flags in
       * ephy-window.c. */
      g_settings_bind_with_mapping (settings, actions[i].key,
                                    action, actions[i].prop,
                                    G_SETTINGS_BIND_GET,
                                    sensitive_get_mapping,
                                    NULL,
                                    action, NULL);
    }
  }
}

static void
bind_location_controller (GSettings *settings,
                          EphyLocationController *controller)
{
  g_settings_bind (settings, EPHY_PREFS_LOCKDOWN_ARBITRARY_URL,
                   controller, "editable",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static void
window_added_cb (GtkApplication *application,
                 GtkWindow *window,
                 EphyLockdown *lockdown)
{
  GtkUIManager *manager;
  GtkActionGroup *action_group;
  GtkAction *action;
  GSettings *settings;
  EphyLocationController *location_controller;

  if (!EPHY_IS_WINDOW (window))
    return;

  g_signal_connect (EPHY_SETTINGS_LOCKDOWN,
                    "changed::" EPHY_PREFS_LOCKDOWN_FULLSCREEN,
                    G_CALLBACK (fullscreen_cb), window);
  g_signal_connect (EPHY_SETTINGS_LOCKDOWN,
                    "changed::" EPHY_PREFS_LOCKDOWN_ARBITRARY_URL,
                    G_CALLBACK (arbitrary_url_cb), window);

  /* Trigger an initial state on these elements. */
  fullscreen_cb (EPHY_SETTINGS_LOCKDOWN,
                 EPHY_PREFS_LOCKDOWN_FULLSCREEN, EPHY_WINDOW (window));
  arbitrary_url_cb (EPHY_SETTINGS_LOCKDOWN,
                    EPHY_PREFS_LOCKDOWN_ARBITRARY_URL, EPHY_WINDOW (window));

  manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (EPHY_WINDOW (window)));

  action_group = find_action_group (manager, "WindowActions");
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group, window_actions,
                             G_N_ELEMENTS (window_actions));

  action_group = find_action_group (manager, "PopupsActions");
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group, popup_actions,
                             G_N_ELEMENTS (popup_actions));

  action = gtk_action_group_get_action (action_group,
                                        "SetImageAsBackground");
  settings = ephy_settings_get ("org.gnome.desktop.background");
  g_settings_bind_writable (settings, "picture-filename",
                            action, "sensitive", FALSE);

  action_group = find_action_group (manager, "SpecialToolbarActions");
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group, special_toolbar_actions,
                             G_N_ELEMENTS (special_toolbar_actions));
  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION) {
    location_controller = ephy_window_get_location_controller (EPHY_WINDOW (window));
    bind_location_controller (EPHY_SETTINGS_LOCKDOWN, location_controller);
  }
}

G_DEFINE_TYPE (EphyLockdown, ephy_lockdown, G_TYPE_OBJECT)

static void
ephy_lockdown_init (EphyLockdown *lockdown)
{
  EphyShell *shell;

  LOG ("EphyLockdown initialising");

  shell = ephy_shell_get_default ();

  g_signal_connect (shell, "window-added",
                    G_CALLBACK (window_added_cb), lockdown);
}

static void
ephy_lockdown_class_init (EphyLockdownClass *klass)
{
}
