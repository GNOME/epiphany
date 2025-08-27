/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010, 2012 Igalia S.L.
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
#include "ephy-lockdown.h"

#include "ephy-action-helper.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-location-controller.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include <gtk/gtk.h>

#define LOCKDOWN_FLAG 1 << 8

struct _EphyLockdown {
  GObject parent_instance;
};

G_DEFINE_FINAL_TYPE (EphyLockdown, ephy_lockdown, G_TYPE_OBJECT)

static void
arbitrary_url_cb (GSettings  *settings,
                  const char *key,
                  EphyWindow *window)
{
  EphyEmbed *embed;
  const char *address;

  /* Restore the real web page address when disabling entry */
  if (g_settings_get_boolean (settings, key)) {
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    /* embed is NULL on startup */
    if (!embed)
      return;

    address = ephy_web_view_get_display_address (ephy_embed_get_web_view (embed));
    ephy_window_set_location (window, address);
    ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), NULL);
  }
}

static void
fullscreen_cb (GSettings  *settings,
               const char *key,
               EphyWindow *window)
{
  if (g_settings_get_boolean (settings, key))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

typedef struct {
  const char *key;
  const char *action;
  const char *prop;
} BindAction;

static const BindAction app_actions[] = {
  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "new-window", "enabled" },
  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "new-incognito", "enabled" },
};

static const BindAction app_mode_app_actions[] = {
  { EPHY_PREFS_LOCKDOWN_HISTORY, "history", "enabled" }
};

static const BindAction window_actions[] = {
  { EPHY_PREFS_LOCKDOWN_ARBITRARY_URL, "location", "enabled"},

  { EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING, "bookmark-page", "enabled" },

  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "fullscreen", "enabled" },

  { EPHY_PREFS_LOCKDOWN_PRINTING, "print", "enabled" },

  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "save-as", "enabled" },

  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "screenshot", "enabled" }
};

static const BindAction popup_actions[] = {
  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "download-link-as", "enabled" },
  { EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK, "save-image-as", "enabled" },
  { EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING, "context-bookmark-page", "enabled" },

  { EPHY_PREFS_LOCKDOWN_FULLSCREEN, "open-link-in-new-window", "enabled" }
};

static const BindAction toolbar_actions[] = {
  { EPHY_PREFS_LOCKDOWN_HISTORY, "navigation-back", "enabled" },
  { EPHY_PREFS_LOCKDOWN_HISTORY, "navigation-forward", "enabled" }
};

static gboolean
sensitive_get_mapping (GValue   *value,
                       GVariant *variant,
                       gpointer  data)
{
  GAction *action;
  gboolean active, before, after;

  action = G_ACTION (data);
  active = g_variant_get_boolean (variant);

  before = g_action_get_enabled (action);
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), LOCKDOWN_FLAG, active);
  after = g_action_get_enabled (action);

  /* Set (GAction::enabled) to the value in GSettings _only if_
   * the LOCKDOWN_FLAG had some real effect in the GAction */
  g_value_set_boolean (value, (before != after) ? after : before);

  return TRUE;
}

static void
bind_settings_and_actions (GSettings        *settings,
                           GActionGroup     *action_group,
                           const BindAction *actions,
                           int               actions_n)
{
  int i;

  for (i = 0; i < actions_n; i++) {
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         actions[i].action);
    g_assert (action);

    /* We need a custom get_mapping for 'enabled'
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

static void
bind_location_controller (GSettings              *settings,
                          EphyLocationController *controller)
{
  g_settings_bind (settings, EPHY_PREFS_LOCKDOWN_ARBITRARY_URL,
                   controller, "editable",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static void
window_added_cb (GtkApplication *application,
                 GtkWindow      *window,
                 EphyLockdown   *lockdown)
{
  GActionGroup *action_group;
  EphyEmbedShellMode mode;
  GAction *action;
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

  mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (application));
  action_group = G_ACTION_GROUP (G_APPLICATION (application));
  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION) {
    /* These actions do not exist in application mode. */
    bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                               action_group, app_actions,
                               G_N_ELEMENTS (app_actions));
  }
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group, app_mode_app_actions,
                             G_N_ELEMENTS (app_mode_app_actions));

  action_group = ephy_window_get_action_group (EPHY_WINDOW (window), "win");
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group,
                             window_actions,
                             G_N_ELEMENTS (window_actions));

  action_group = ephy_window_get_action_group (EPHY_WINDOW (window), "toolbar");
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group,
                             toolbar_actions,
                             G_N_ELEMENTS (toolbar_actions));

  action_group = ephy_window_get_action_group (EPHY_WINDOW (window), "popup");
  bind_settings_and_actions (EPHY_SETTINGS_LOCKDOWN,
                             action_group, popup_actions,
                             G_N_ELEMENTS (popup_actions));

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "set-image-as-background");
  settings = ephy_settings_get ("org.gnome.desktop.background");
  g_settings_bind_writable (settings, "picture-filename",
                            action, "enabled", FALSE);

  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION &&
      mode != EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    location_controller = ephy_window_get_location_controller (EPHY_WINDOW (window));
    bind_location_controller (EPHY_SETTINGS_LOCKDOWN, location_controller);
  }
}

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
