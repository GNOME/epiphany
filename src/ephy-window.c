/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2011 Igalia S.L.
 *  Copyright © 2016 Iulian-Gabriel Radu
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
#include "ephy-window.h"

#include "context-menu-commands.h"
#include "ephy-action-bar.h"
#include "ephy-action-helper.h"
#include "ephy-add-opensearch-engine-button.h"
#include "ephy-bookmarks-dialog.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-desktop-utils.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-filters-manager.h"
#include "ephy-find-toolbar.h"
#include "ephy-flatpak-utils.h"
#include "ephy-fullscreen-box.h"
#include "ephy-header-bar.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-mouse-gesture-controller.h"
#include "ephy-permissions-manager.h"
#include "ephy-prefs.h"
#include "ephy-reader-handler.h"
#include "ephy-security-dialog.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-site-menu-button.h"
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"
#include "ephy-uri-helpers.h"
#include "ephy-view-source-handler.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-view.h"
#include "ephy-zoom.h"
#include "window-commands.h"

#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libportal-gtk4/portal-gtk4.h>
#include <stdlib.h>

#include <webkit/webkit.h>

/**
 * SECTION:ephy-window
 * @short_description: Epiphany's main #GtkWindow widget
 *
 * #EphyWindow is Epiphany's main widget.
 */

const struct {
  const char *action_and_target;
  const char *accelerators[9];
} accels [] = {
  /* Page Menu accels */
  { "win.page-menu", { "F10", NULL } },
  { "win.new-tab", { "<Primary>T", NULL } },
  { "win.open", { "<Primary>O", NULL } },
  { "win.save-as", { "<Primary>S", NULL } },
  { "win.save-as-application", { "<shift><Primary>A", NULL } },
  { "win.screenshot", { "<shift><Primary>S", NULL } },
  { "win.undo", { "<Primary>Z", NULL } },
  { "win.redo", { "<shift><Primary>Z", NULL } },
  { "win.copy", { "<Primary>C", NULL } },
  { "win.cut", { "<Primary>X", NULL } },
  { "win.paste", { "<Primary>V", NULL } },
  { "win.paste-as-plain-text", { "<shift><Primary>V", NULL } },
  { "win.zoom-in", { "<Primary>plus", "<Primary>KP_Add", "<Primary>equal", "ZoomIn", NULL } },
  { "win.zoom-out", { "<Primary>minus", "<Primary>KP_Subtract", "ZoomOut", NULL } },
  { "win.zoom-normal", { "<Primary>0", "<Primary>KP_0", NULL } },
  { "win.print", { "<Primary>P", NULL } },
  { "win.find", { "<Primary>F", NULL } },
  { "win.find-prev", { "<shift><Primary>G", NULL } },
  { "win.find-next", { "<Primary>G", NULL } },
  { "win.bookmark-page", { "<Primary>D", "AddFavorite", NULL } },
  { "win.bookmarks", { "<alt><Primary>D", NULL } },
  { "win.show-downloads", { "<shift><Primary>Y", NULL } },
  { "win.encoding", { NULL } },
  { "win.page-source", { "<Primary>U", NULL } },
  { "win.toggle-inspector", { "<shift><Primary>I", "F12", NULL } },
  { "win.toggle-reader-mode", { "<alt><Primary>r", NULL } },

  { "win.select-all", { "<Primary>A", NULL } },

  { "win.send-to", { "Send", NULL } },
  { "win.location", { "<Primary>L", "<alt>D", "F6", "Go", "OpenURL", NULL } },
  { "win.location-search", {"<Primary>K", NULL} },
  { "win.home", { "<alt>Home", NULL } },
  { "win.tabs-view", { "<shift><Primary>O", NULL } },

  /* Toggle actions */
  { "win.browse-with-caret", { "F7", NULL } },
  { "win.fullscreen", { "F11", NULL } },

  /* Navigation */
  { "toolbar.stop", { "Escape", "Stop", NULL } },
  { "toolbar.reload", { "<Primary>R", "F5", "Refresh", "Reload", NULL } },
  { "toolbar.reload-bypass-cache", { "<shift><Primary>R", "<shift>F5" } },
  { "toolbar.combined-stop-reload", { NULL } },

  /* Tabs */
  { "tab.duplicate", { "<shift><Primary>K", NULL } },
  { "tab.close", { "<Primary>W", NULL } },
  { "tab.mute", { "<Primary>M", NULL } },
  { "tab.pin", { NULL } }
}, accels_navigation_ltr [] = {
  { "toolbar.navigation-back", { "<alt>Left", "<alt>KP_Left", "<alt>KP_4", "Back", NULL } },
  { "toolbar.navigation-forward", { "<alt>Right", "<alt>KP_Right", "<alt>KP_6", "Forward", NULL } }
}, accels_navigation_rtl [] = {
  { "toolbar.navigation-back", { "<alt>Right", "<alt>KP_Right", "<alt>KP_6", "Back", NULL } },
  { "toolbar.navigation-forward", { "<alt>Left", "<alt>KP_Left", "<alt>KP_4", "Forward", NULL } }
}, *accels_navigation_ltr_rtl;

#define SETTINGS_CONNECTION_DATA_KEY    "EphyWindowSettings"

static guint64 window_uid = 1;

struct _EphyWindow {
  AdwApplicationWindow parent_instance;

  GtkWidget *overview;
  EphyFullscreenBox *fullscreen_box;
  GtkWidget *header_bar;
  GHashTable *action_labels;
  EphyTabView *tab_view;
  AdwTabBar *tab_bar;
  GtkWidget *action_bar_revealer;
  GtkWidget *action_bar;
  GtkWidget *overlay_split_view;
  GtkWidget *bottom_sheet;
  GtkWidget *bookmarks_dialog;
  EphyEmbed *active_embed;
  EphyWindowChrome chrome;
  WebKitHitTestResult *context_event;
  WebKitHitTestResult *hit_test_result;
  guint idle_worker;
  EphyLocationController *location_controller;
  guint modified_forms_timeout_id;
  EphyMouseGestureController *mouse_gesture_controller;
  EphyAdaptiveMode adaptive_mode;
  int last_opened_pos;
  gboolean show_fullscreen_header_bar;
  guint64 uid;
  GtkWidget *toast_overlay;
  GtkWidget *switch_to_tab;
  AdwToast *switch_toast;
  AdwToast *download_start_toast;
  GtkWidget *header_bin_top;
  GtkWidget *header_bin_bottom;
  GCancellable *cancellable;

  GList *pending_decisions;
  gulong filters_initialized_id;

  gint current_width;
  gint current_height;

  guint has_default_size : 1;
  guint is_maximized : 1;
  guint is_fullscreen : 1;
  guint closing : 1;
  guint is_popup : 1;
  guint updating_address : 1;
  guint force_close : 1;
  guint checking_modified_forms : 1;
  guint has_modified_forms : 1;
  guint confirmed_close_with_multiple_tabs : 1;
  guint present_on_insert : 1;

  GHashTable *action_groups;
};

enum {
  PROP_0,
  PROP_ACTIVE_CHILD,
  PROP_CHROME,
  PROP_SINGLE_TAB_MODE,
  PROP_ADAPTIVE_MODE,
};

/* Make sure not to overlap with those in ephy-lockdown.c */
enum {
  SENS_FLAG_CHROME        = 1 << 0,
  SENS_FLAG_CONTEXT       = 1 << 1,
  SENS_FLAG_DOCUMENT      = 1 << 2,
  SENS_FLAG_LOADING       = 1 << 3,
  SENS_FLAG_NAVIGATION    = 1 << 4,
  SENS_FLAG_IS_BLANK      = 1 << 5,
  SENS_FLAG_IS_INTERNAL_PAGE = 1 << 6,
  SENS_FLAG_IS_OVERVIEW   = 1 << 7,
  SENS_FLAG_IS_SIDEBAR    = 1 << 9,
};

static gint
impl_add_child (EphyEmbedContainer *container,
                EphyEmbed          *child,
                EphyEmbed          *parent,
                int                 position,
                gboolean            jump_to)
{
  EphyWindow *window = EPHY_WINDOW (container);
  int ret;

  g_assert (!window->is_popup || ephy_tab_view_get_n_pages (window->tab_view) < 1);

  ret = ephy_tab_view_add_tab (window->tab_view, child, parent, position, jump_to);

  if (jump_to)
    ephy_window_update_entry_focus (window, ephy_embed_get_web_view (child));

  return ret;
}

static void
impl_set_active_child (EphyEmbedContainer *container,
                       EphyEmbed          *child)
{
  EphyWindow *window = EPHY_WINDOW (container);

  ephy_tab_view_select_page (window->tab_view, GTK_WIDGET (child));
}

static AdwDialog *
construct_confirm_close_dialog (EphyWindow *window,
                                const char *title,
                                const char *info,
                                const char *action)
{
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (title, info);

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "accept", action,
                                  NULL);

  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "accept",
                                            ADW_RESPONSE_DESTRUCTIVE);

  return dialog;
}

static void
impl_remove_child (EphyEmbedContainer *container,
                   EphyEmbed          *child)
{
  EphyWindow *window = EPHY_WINDOW (container);

  ephy_tab_view_close (window->tab_view, GTK_WIDGET (child));
}

static EphyEmbed *
impl_get_active_child (EphyEmbedContainer *container)
{
  return EPHY_WINDOW (container)->active_embed;
}

static GList *
impl_get_children (EphyEmbedContainer *container)
{
  EphyWindow *window = EPHY_WINDOW (container);

  return ephy_tab_view_get_pages (window->tab_view);
}

static gboolean
impl_get_is_popup (EphyEmbedContainer *container)
{
  return EPHY_WINDOW (container)->is_popup;
}

static guint
impl_get_n_children (EphyEmbedContainer *container)
{
  EphyWindow *window = EPHY_WINDOW (container);

  return ephy_tab_view_get_n_pages (window->tab_view);
}

static void
ephy_window_embed_container_iface_init (EphyEmbedContainerInterface *iface)
{
  iface->add_child = impl_add_child;
  iface->set_active_child = impl_set_active_child;
  iface->remove_child = impl_remove_child;
  iface->get_active_child = impl_get_active_child;
  iface->get_children = impl_get_children;
  iface->get_is_popup = impl_get_is_popup;
  iface->get_n_children = impl_get_n_children;
}

static EphyEmbed *
ephy_window_open_link (EphyLink      *link,
                       const char    *address,
                       EphyEmbed     *embed,
                       EphyLinkFlags  flags)
{
  EphyWindow *window = EPHY_WINDOW (link);
  EphyEmbed *new_embed;
  EphyWebView *web_view;

  g_assert (address || (flags & (EPHY_LINK_NEW_WINDOW | EPHY_LINK_NEW_TAB | EPHY_LINK_HOME_PAGE)));

  if (!embed)
    embed = window->active_embed;

  if (flags & EPHY_LINK_BOOKMARK)
    ephy_web_view_set_visit_type (ephy_embed_get_web_view (embed),
                                  EPHY_PAGE_VISIT_BOOKMARK);
  else if (flags & EPHY_LINK_TYPED)
    ephy_web_view_set_visit_type (ephy_embed_get_web_view (embed),
                                  EPHY_PAGE_VISIT_TYPED);

  if (!embed ||
      (flags & (EPHY_LINK_JUMP_TO |
                EPHY_LINK_NEW_TAB |
                EPHY_LINK_NEW_WINDOW))) {
    EphyNewTabFlags ntflags = 0;
    EphyWindow *target_window;

    if (!embed)
      target_window = window;
    else
      target_window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed)));

    if (flags & EPHY_LINK_JUMP_TO)
      ntflags |= EPHY_NEW_TAB_JUMP;

    if (flags & EPHY_LINK_NEW_WINDOW ||
        (flags & EPHY_LINK_NEW_TAB && window->is_popup))
      target_window = ephy_window_new ();

    if (flags & EPHY_LINK_NEW_TAB_APPEND_AFTER)
      ntflags |= EPHY_NEW_TAB_APPEND_AFTER;

    new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                    target_window,
                                    embed, ntflags);
  } else if (!embed) {
    new_embed = ephy_shell_new_tab (ephy_shell_get_default (), window, NULL, 0);
  } else {
    new_embed = embed;
  }

  web_view = ephy_embed_get_web_view (new_embed);

  if (address) {
    ephy_web_view_load_url (web_view, address);
  } else if (flags & EPHY_LINK_NEW_TAB) {
    ephy_web_view_load_new_tab_page (web_view);
  } else if (flags & (EPHY_LINK_NEW_WINDOW | EPHY_LINK_HOME_PAGE)) {
    EphyShell *shell = ephy_shell_get_default ();
    EphyWebApplication *webapp = ephy_shell_get_webapp (shell);
    if (webapp)
      ephy_web_view_load_url (web_view, webapp->url);
    else
      ephy_web_view_load_homepage (web_view);
  }

  if (ephy_web_view_get_is_blank (web_view))
    ephy_window_focus_location_entry (window);
  else
    gtk_widget_grab_focus (GTK_WIDGET (new_embed));

  return new_embed;
}

static void
ephy_window_link_iface_init (EphyLinkInterface *iface)
{
  iface->open_link = ephy_window_open_link;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyWindow, ephy_window, ADW_TYPE_APPLICATION_WINDOW,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                      ephy_window_link_iface_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_CONTAINER,
                                                      ephy_window_embed_container_iface_init))

static void
sync_chromes_visibility (EphyWindow *window)
{
  gboolean show_tabsbar, is_wide, fullscreen, fullscreen_lockdown;

  if (window->closing)
    return;

  show_tabsbar = (window->chrome & EPHY_WINDOW_CHROME_TABSBAR);
  is_wide = window->adaptive_mode == EPHY_ADAPTIVE_MODE_NORMAL;
  fullscreen = gtk_window_is_fullscreen (GTK_WINDOW (window));

  fullscreen_lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN) ||
                        ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_KIOSK;

  gtk_widget_set_visible (GTK_WIDGET (window->header_bar),
                          !fullscreen_lockdown &&
                          (!fullscreen || window->show_fullscreen_header_bar));
  gtk_widget_set_visible (GTK_WIDGET (window->tab_bar),
                          !fullscreen_lockdown &&
                          (show_tabsbar && is_wide && !(window->is_popup) &&
                           (!fullscreen || window->show_fullscreen_header_bar)));
  gtk_widget_set_visible (GTK_WIDGET (window->action_bar),
                          !fullscreen_lockdown && !is_wide &&
                          (!fullscreen || window->show_fullscreen_header_bar));
}

static void
ephy_window_set_chrome (EphyWindow       *window,
                        EphyWindowChrome  chrome)
{
  if (window->chrome == chrome)
    return;

  window->chrome = chrome;
  if (window->closing)
    return;

  g_object_notify (G_OBJECT (window), "chrome");
  sync_chromes_visibility (window);
}

static void
sync_tab_load_status (EphyWebView     *view,
                      WebKitLoadEvent  load_event,
                      EphyWindow      *window)
{
  GActionGroup *action_group;
  GAction *action;
  gboolean loading;

  if (window->closing)
    return;

  loading = ephy_web_view_is_loading (view);

  action_group = ephy_window_get_action_group (window, "win");

  /* disable print while loading, see bug #116344 */
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "print");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        SENS_FLAG_LOADING, loading);

  if (!loading) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "toggle-reader-mode");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
    g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (FALSE));
  }

  action_group = ephy_window_get_action_group (window, "toolbar");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "stop");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), loading);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "combined-stop-reload");
  g_action_change_state (action, g_variant_new_boolean (loading));
}

static void
sync_tab_security (EphyWebView *view,
                   GParamSpec  *pspec,
                   EphyWindow  *window)
{
  EphyTitleWidget *title_widget;
  EphySecurityLevel security_level;

  if (window->closing)
    return;

  ephy_web_view_get_security_level (view, &security_level, NULL, NULL, NULL);

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));
  ephy_title_widget_set_security_level (title_widget, security_level);
}

static void
ephy_window_set_adaptive_mode (EphyWindow       *window,
                               EphyAdaptiveMode  adaptive_mode)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));

  if (window->adaptive_mode == adaptive_mode)
    return;

  window->adaptive_mode = adaptive_mode;

  ephy_header_bar_set_adaptive_mode (header_bar, adaptive_mode);

  sync_chromes_visibility (window);

  if (adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW) {
    g_object_ref (window->header_bar);
    adw_bin_set_child (ADW_BIN (window->header_bin_top), NULL);
    adw_bin_set_child (ADW_BIN (window->header_bin_bottom), window->header_bar);
    g_object_unref (window->header_bar);
    gtk_widget_add_css_class (GTK_WIDGET (window), "narrow");
    gtk_widget_set_visible (window->action_bar_revealer, TRUE);
  } else {
    g_object_ref (window->header_bar);
    adw_bin_set_child (ADW_BIN (window->header_bin_bottom), NULL);
    adw_bin_set_child (ADW_BIN (window->header_bin_top), window->header_bar);
    g_object_unref (window->header_bar);
    gtk_widget_remove_css_class (GTK_WIDGET (window), "narrow");
    gtk_widget_set_visible (window->action_bar_revealer, FALSE);
  }
}

static void
notify_fullscreen_cb (EphyWindow *window)
{
  EphyEmbed *embed;
  gboolean fullscreen = gtk_window_is_fullscreen (GTK_WINDOW (window));
  GAction *action;
  GActionGroup *action_group;

  window->is_fullscreen = fullscreen;

  embed = window->active_embed;

  if (embed && fullscreen) {
    /* sync status */
    sync_tab_load_status (ephy_embed_get_web_view (embed), WEBKIT_LOAD_STARTED, window);
    sync_tab_security (ephy_embed_get_web_view (embed), NULL, window);
  }

  if (embed) {
    if (fullscreen)
      ephy_embed_entering_fullscreen (embed);
    else
      ephy_embed_leaving_fullscreen (embed);
  }

  ephy_fullscreen_box_set_fullscreen (window->fullscreen_box,
                                      fullscreen && window->show_fullscreen_header_bar);

  adw_tab_overview_set_show_start_title_buttons (ADW_TAB_OVERVIEW (window->overview),
                                                 !fullscreen);
  adw_tab_overview_set_show_end_title_buttons (ADW_TAB_OVERVIEW (window->overview),
                                               !fullscreen);

  sync_chromes_visibility (window);

  action_group = ephy_window_get_action_group (window, "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "fullscreen");

  g_simple_action_set_state (G_SIMPLE_ACTION (action),
                             g_variant_new_boolean (fullscreen));

  action_group = ephy_window_get_action_group (window, "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "tabs-view");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               !fullscreen || window->show_fullscreen_header_bar);

  if (!fullscreen)
    window->show_fullscreen_header_bar = FALSE;
}

#if 0
/* Disabled due to https://gitlab.gnome.org/GNOME/epiphany/-/issues/1915 */
static gboolean
ephy_window_should_view_receive_key_press_event (EphyWindow      *window,
                                                 guint            keyval,
                                                 GdkModifierType  state)
{
  state &= (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK);

  /* Focus location entry */
  if (keyval == GDK_KEY_F6)
    return FALSE;

  /* Websites are allowed to override most Epiphany accelerators, but not
   * window or tab management accelerators. */
  if (state == GDK_CONTROL_MASK)
    return keyval != GDK_KEY_n &&            /* New Window */
           keyval != GDK_KEY_q &&            /* Quit */
           keyval != GDK_KEY_T &&            /* Reopen Closed Tab */
           keyval != GDK_KEY_t &&            /* New Tab */
           keyval != GDK_KEY_w &&            /* Close Tab */
           keyval != GDK_KEY_Page_Up &&      /* Previous Tab */
           keyval != GDK_KEY_KP_Page_Up &&   /* Previous Tab */
           keyval != GDK_KEY_Page_Down &&    /* Next Tab */
           keyval != GDK_KEY_KP_Page_Down && /* Next Tab */
           keyval != GDK_KEY_Tab &&          /* Next Tab */
           keyval != GDK_KEY_KP_Tab &&       /* Next Tab */
           keyval != GDK_KEY_ISO_Left_Tab;   /* Previous Tab (Shift+Tab -> ISO Left Tab) */

  if (state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    return keyval != GDK_KEY_n &&            /* New Incognito Window */
           keyval != GDK_KEY_Page_Up &&      /* Move Tab Left */
           keyval != GDK_KEY_KP_Page_Up &&   /* Move Tab Left */
           keyval != GDK_KEY_Page_Down &&    /* Move Tab Right */
           keyval != GDK_KEY_KP_Page_Down;   /* Move Tab Right */

  if (state == GDK_ALT_MASK)
    return keyval != GDK_KEY_Left &&      /* Back */
           keyval != GDK_KEY_Right &&     /* Forward */
           keyval != GDK_KEY_Home &&      /* Homepage */
           keyval != GDK_KEY_1 &&         /* Switch To Tab 1 */
           keyval != GDK_KEY_2 &&         /* Switch To Tab 2 */
           keyval != GDK_KEY_3 &&         /* Switch To Tab 3 */
           keyval != GDK_KEY_4 &&         /* Switch To Tab 4 */
           keyval != GDK_KEY_5 &&         /* Switch To Tab 5 */
           keyval != GDK_KEY_6 &&         /* Switch To Tab 6 */
           keyval != GDK_KEY_7 &&         /* Switch To Tab 7 */
           keyval != GDK_KEY_8 &&         /* Switch To Tab 8 */
           keyval != GDK_KEY_9 &&         /* Switch To Tab 9 */
           keyval != GDK_KEY_0;           /* Switch To Tab 10 */

  return TRUE;
}

static gboolean
handle_key_cb (EphyWindow            *window,
               guint                  keyval,
               guint                  keycode,
               GdkModifierType        state,
               GtkEventControllerKey *controller)
{
  EphyWebView *view;

  if (!window->active_embed)
    return GDK_EVENT_PROPAGATE;

  view = ephy_embed_get_web_view (window->active_embed);
  if (gtk_window_get_focus (GTK_WINDOW (window)) != GTK_WIDGET (view))
    return GDK_EVENT_PROPAGATE;

  if (ephy_window_should_view_receive_key_press_event (window, keyval, state))
    return gtk_event_controller_key_forward (controller, GTK_WIDGET (view));

  return GDK_EVENT_PROPAGATE;
}
#endif

static void
on_set_background_status (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (source_object);
  g_autoptr (GError) error = NULL;

  if (!xdp_portal_set_background_status_finish (portal, res, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not set background status: %s", error->message);
    return;
  }
}

static void
on_request_background (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (source_object);
  g_autoptr (GError) error = NULL;
  GtkWidget *window;
  EphyWindow *ephy_window;
  EphyEmbed *embed;
  g_autofree char *status = NULL;

  if (!xdp_portal_request_background_finish (portal, res, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not request background: %s", error->message);
    return;
  }

  window = GTK_WIDGET (user_data);
  gtk_widget_set_visible (window, FALSE);

  ephy_window = EPHY_WINDOW (window);
  embed = ephy_window_get_active_embed (ephy_window);

  status = g_strdup (ephy_embed_get_title (embed));

  /* 96 is a limit by freedesktop portal */
  if (strlen (status) > 96)
    status[95] = '\0';

  xdp_portal_set_background_status (portal, status, ephy_window->cancellable, on_set_background_status, NULL);
}

static gboolean
ephy_window_close_request (GtkWindow *window)
{
  if ((ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION) &&
      g_settings_get_boolean (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_RUN_IN_BACKGROUND)) {
    g_autoptr (XdpPortal) portal = xdp_portal_new ();
    g_autoptr (XdpParent) parent_window = xdp_parent_new_gtk (GTK_WINDOW (window));
    EphyWindow *ephy_window = EPHY_WINDOW (window);

    xdp_portal_request_background (portal, parent_window, NULL, NULL, XDP_BACKGROUND_FLAG_NONE, ephy_window->cancellable, on_request_background, window);
    return TRUE;
  }

  if (!ephy_window_close (EPHY_WINDOW (window)))
    return TRUE;

  return FALSE;
}

#define MAX_SPELL_CHECK_GUESSES 4

static void
update_link_actions_sensitivity (EphyWindow *window,
                                 gboolean    link_has_web_scheme)
{
  GAction *action;
  GActionGroup *action_group;

  action_group = ephy_window_get_action_group (window, "popup");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "open-link-in-new-window");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), link_has_web_scheme);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "open-link-in-new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_CONTEXT, !link_has_web_scheme);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "open-link-in-incognito-window");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), link_has_web_scheme);
}

static void
update_edit_action_sensitivity (EphyWindow *window,
                                const char *action_name,
                                gboolean    sensitive,
                                gboolean    hide)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = ephy_window_get_action_group (window, "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       action_name);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive || hide);
}

static void
update_edit_actions_sensitivity (EphyWindow *window,
                                 gboolean    hide)
{
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));
  gboolean can_cut, can_copy, can_undo, can_redo, can_paste;

  if (GTK_IS_EDITABLE (widget)) {
    EphyTitleWidget *title_widget;
    gboolean has_selection;

    title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

    has_selection = gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), NULL, NULL);

    can_cut = has_selection;
    can_copy = has_selection;
    can_paste = TRUE;
    can_undo = EPHY_IS_LOCATION_ENTRY (title_widget);
    can_redo = EPHY_IS_LOCATION_ENTRY (title_widget) &&
               ephy_location_entry_get_can_redo (EPHY_LOCATION_ENTRY (title_widget));
  } else {
    EphyEmbed *embed;
    WebKitWebView *view;
    WebKitEditorState *state;

    embed = window->active_embed;
    g_assert (embed);

    view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    state = webkit_web_view_get_editor_state (view);

    can_cut = webkit_editor_state_is_cut_available (state);
    can_copy = webkit_editor_state_is_copy_available (state);
    can_paste = webkit_editor_state_is_paste_available (state);
    can_undo = webkit_editor_state_is_undo_available (state);
    can_redo = webkit_editor_state_is_redo_available (state);
  }

  update_edit_action_sensitivity (window, "cut", can_cut, hide);
  update_edit_action_sensitivity (window, "copy", can_copy, hide);
  update_edit_action_sensitivity (window, "paste", can_paste, hide);
  update_edit_action_sensitivity (window, "paste-as-plain-text", can_paste, hide);
  update_edit_action_sensitivity (window, "undo", can_undo, hide);
  update_edit_action_sensitivity (window, "redo", can_redo, hide);
}

static void
enable_edit_actions_sensitivity (EphyWindow *window)
{
  GActionGroup *action_group;
  GAction *action;

  if (window->closing)
    return;

  action_group = ephy_window_get_action_group (window, "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "cut");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "copy");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "paste");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "paste-as-plain-text");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "undo");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "redo");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static void
change_combined_stop_reload_state (GSimpleAction *action,
                                   GVariant      *loading,
                                   gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyActionBarStart *header_bar_start = ephy_header_bar_get_action_bar_start (EPHY_HEADER_BAR (window->header_bar));
  EphyTitleWidget *title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  ephy_action_bar_start_change_combined_stop_reload_state (header_bar_start,
                                                           g_variant_get_boolean (loading));
  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    ephy_location_entry_start_change_combined_stop_reload_state (EPHY_LOCATION_ENTRY (title_widget),
                                                                 g_variant_get_boolean (loading));

  g_simple_action_set_state (action, loading);
}

static const GActionEntry window_entries [] = {
  { "page-menu", window_cmd_page_menu },
  { "new-tab", window_cmd_new_tab },
  { "open", window_cmd_open },
  { "save-as", window_cmd_save_as },
  { "save-as-application", window_cmd_save_as_application },
  { "screenshot", window_cmd_screenshot },
  { "open-application-manager", window_cmd_open_application_manager },
  { "undo", window_cmd_undo },
  { "redo", window_cmd_redo },
  { "cut", window_cmd_cut },
  { "copy", window_cmd_copy },
  { "paste", window_cmd_paste },
  { "paste-as-plain-text", window_cmd_paste_as_plain_text },
  { "delete", window_cmd_delete },
  { "zoom-in", window_cmd_zoom_in },
  { "zoom-out", window_cmd_zoom_out },
  { "zoom-normal", window_cmd_zoom_normal },
  { "print", window_cmd_print },
  { "find", window_cmd_find },
  { "find-prev", window_cmd_find_prev },
  { "find-next", window_cmd_find_next },
  { "bookmark-page", window_cmd_bookmark_page },
  { "bookmarks", window_cmd_bookmarks },
  { "show-downloads", window_cmd_show_downloads },
  { "encoding", window_cmd_encoding },
  { "privacy-report", window_cmd_privacy_report },
  { "passwords", window_cmd_passwords },
  { "page-source", window_cmd_page_source },
  { "toggle-inspector", window_cmd_toggle_inspector },
  { "security-permissions", window_cmd_security_and_permissions },

  { "select-all", window_cmd_select_all },

  { "location", window_cmd_go_location },
  { "location-search", window_cmd_location_search },
  { "home", window_cmd_go_home },
  { "tabs-view", window_cmd_go_tabs_view },
  { "switch-new-tab", window_cmd_switch_new_tab },

  /* Toggle actions */
  { "toggle-reader-mode", NULL, NULL, "false", window_cmd_toggle_reader_mode },
  { "browse-with-caret", NULL, NULL, "false", window_cmd_change_browse_with_caret_state },
  { "fullscreen", NULL, NULL, "false", window_cmd_change_fullscreen_state },
};

static const GActionEntry tab_entries [] = {
  { "duplicate", window_cmd_tabs_duplicate },
  { "close", window_cmd_tabs_close },
  { "close-left", window_cmd_tabs_close_left },
  { "close-right", window_cmd_tabs_close_right },
  { "close-others", window_cmd_tabs_close_others },
  { "reload-all", window_cmd_tabs_reload_all_tabs },
  { "pin", window_cmd_tabs_pin },
  { "unpin", window_cmd_tabs_unpin },
  { "mute", NULL, NULL, "false", window_cmd_change_tabs_mute_state },
};

static const GActionEntry toolbar_entries [] = {
  { "duplicate-tab", window_cmd_tabs_duplicate },
  { "navigation-back", window_cmd_navigation },
  { "navigation-back-new-tab", window_cmd_navigation_new_tab },
  { "navigation-forward", window_cmd_navigation },
  { "navigation-forward-new-tab", window_cmd_navigation_new_tab },
  { "homepage-new-tab", window_cmd_homepage_new_tab },
  { "new-tab-from-clipboard", window_cmd_new_tab_from_clipboard },

  { "stop", window_cmd_stop },
  { "reload", window_cmd_reload },
  { "reload-bypass-cache", window_cmd_reload_bypass_cache },
  { "always-stop", window_cmd_stop },
  { "combined-stop-reload", window_cmd_combined_stop_reload, NULL, "false", change_combined_stop_reload_state }
};

static const GActionEntry popup_entries [] = {
  { "context-bookmark-page", window_cmd_bookmark_page },
  /* Links. */

  { "open-link-in-new-window", context_cmd_link_in_new_window },
  { "open-link-in-new-tab", context_cmd_link_in_new_tab },
  { "open-link-in-incognito-window", context_cmd_link_in_incognito_window },
  { "download-link-as", context_cmd_download_link_as },
  { "copy-link-address", context_cmd_copy_link_address },
  { "copy-email-address", context_cmd_copy_link_address },
  { "send-via-email", context_cmd_send_via_email },
  { "add-link-to-bookmarks", context_cmd_add_link_to_bookmarks },

  /* Images. */

  { "view-image", context_cmd_view_image_in_new_tab },
  { "copy-image-location", context_cmd_copy_image_location },
  { "save-image-as", context_cmd_save_image_as },
  { "set-image-as-background", context_cmd_set_image_as_background },

  /* Video. */

  { "open-video-in-new-window", context_cmd_media_in_new_window },
  { "open-video-in-new-tab", context_cmd_media_in_new_tab },
  { "save-video-as", context_cmd_save_media_as },
  { "copy-video-location", context_cmd_copy_media_location },

  /* Audio. */

  { "open-audio-in-new-window", context_cmd_media_in_new_window },
  { "open-audio-in-new-tab", context_cmd_media_in_new_tab },
  { "save-audios-as", context_cmd_save_media_as },
  { "copy-audio-location", context_cmd_copy_media_location },

  /* Selection */
  { "search-selection", context_cmd_search_selection, "s" },
  { "open-selection", context_cmd_open_selection, "s" },
  { "open-selection-in-new-tab", context_cmd_open_selection_in_new_tab, "s" },
  { "open-selection-in-new-window", context_cmd_open_selection_in_new_window, "s" },
  { "open-selection-in-incognito-window", context_cmd_open_selection_in_incognito_window, "s" },
};

const struct {
  const char *action;
  const char *label;
} action_label [] = {
  /* Undo, redo. */
  { "undo", N_("_Undo") },
  { "redo", N_("Re_do") },

  /* Edit. */
  { "cut", N_("Cu_t") },
  { "copy", N_("_Copy") },
  { "paste", N_("_Paste") },
  { "paste-as-plain-text", N_("_Paste Text Only") },
  { "select-all", N_("Select _All") },

  { "send-via-email", N_("S_end Link by Email…") },
  { "add-link-to-bookmarks", N_("Add _Link to Bookmarks") },

  { "reload", N_("_Reload") },
  { "navigation-back", N_("_Back") },
  { "navigation-forward", N_("_Forward") },

  /* Bookmarks */
  { "context-bookmark-page", N_("Add Boo_kmark…") },

  /* Links. */

  { "open-link-in-new-window", N_("Open Link in New _Window") },
  { "open-link-in-new-tab", N_("Open Link in New _Tab") },
  { "open-link-in-incognito-window", N_("Open Link in I_ncognito Window") },
  { "download-link-as", N_("_Save Link As…") },
  { "copy-link-address", N_("_Copy Link Address") },
  { "copy-email-address", N_("_Copy E-mail Address") },

  /* Images. */

  { "view-image", N_("View _Image in New Tab") },
  { "copy-image-location", N_("Copy I_mage Address") },
  { "save-image-as", N_("_Save Image As…") },
  { "set-image-as-background", N_("Set as _Wallpaper") },

  /* Video. */

  { "open-video-in-new-window", N_("Open Video in New _Window") },
  { "open-video-in-new-tab", N_("Open Video in New _Tab") },
  { "save-video-as", N_("_Save Video As…") },
  { "copy-video-location", N_("_Copy Video Address") },

  /* Audio. */

  { "open-audio-in-new-window", N_("Open Audio in New _Window") },
  { "open-audio-in-new-tab", N_("Open Audio in New _Tab") },
  { "save-audios-as", N_("_Save Audio As…") },
  { "copy-audio-location", N_("_Copy Audio Address") },

  /* Selection */
  { "search-selection", "search-selection-placeholder" },
  { "open-selection", "open-selection-placeholder" },

  { "save-as", N_("Save Pa_ge As…") },
  { "screenshot", N_("_Take Screenshot…") },
  { "page-source", N_("_Page Source") }
};

static char *
calculate_location (const char *typed_address,
                    const char *address)
{
  const char *location;

  /* If there's a typed address, use that over address. Never
   * show URIs in the 'do_not_show_address' array. */
  location = typed_address ? typed_address : address;
  location = ephy_embed_utils_is_no_show_address (location) ? NULL : location;

  return g_strdup (location);
}

static void
_ephy_window_set_default_actions_sensitive (EphyWindow *window,
                                            guint       flags,
                                            gboolean    set)
{
  GActionGroup *action_group;
  GAction *action;
  int i;

  const char *action_group_actions[] = {
    "save-as", "save-as-application", "screenshot", "print",
    "find", "find-prev", "find-next",
    "bookmark-page", "encoding", "page-source",
    NULL
  };

  action_group = ephy_window_get_action_group (window, "win");

  /* Page menu */
  for (i = 0; action_group_actions[i]; i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         action_group_actions[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          flags, set);
  }

  /* Page context popup */
  action_group = ephy_window_get_action_group (window, "popup");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "context-bookmark-page");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        flags, set);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "send-via-email");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        flags, set);

  /* Toolbar */
  action_group = ephy_window_get_action_group (window, "toolbar");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "combined-stop-reload");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        flags, set);
}

static void
sync_tab_address (EphyWebView *view,
                  GParamSpec  *pspec,
                  EphyWindow  *window)
{
  const char *address;
  const char *typed_address;
  char *location;
  gboolean is_internal_page;
  EphyEmbed *embed = window->active_embed;
  EphyTitleWidget *title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));
  const char *current_text = ephy_title_widget_get_address (title_widget);
  GActionGroup *action_group;
  GAction *action;
  EphySecurityLevel security_level;

  if (window->closing || ephy_embed_get_web_view (embed) != view)
    return;

  address = ephy_web_view_get_display_address (view);
  typed_address = ephy_web_view_get_typed_address (view);
  is_internal_page = g_str_has_prefix (address, "about:") || g_str_has_prefix (address, "ephy-about:");

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_BLANK,
                                              ephy_web_view_get_is_blank (view));

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_INTERNAL_PAGE, is_internal_page);

  view = ephy_embed_get_web_view (ephy_window_get_active_embed (window));
  ephy_web_view_get_security_level (view, &security_level, NULL, NULL, NULL);

  action_group = ephy_window_get_action_group (window, "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "security-permissions");

  if (security_level == EPHY_SECURITY_LEVEL_LOCAL_PAGE || security_level == EPHY_SECURITY_LEVEL_TO_BE_DETERMINED)
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  else
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

  if ((ephy_web_view_get_is_blank (view) ||
       ephy_web_view_is_newtab (view) ||
       ephy_web_view_is_overview (view)) &&
      ephy_embed_get_typed_input (embed))
    location = g_strdup (ephy_embed_get_typed_input (embed));
  else
    location = calculate_location (typed_address, address);

  if (g_strcmp0 (location, current_text) != 0)
    ephy_window_set_location (window, location);

  if (EPHY_IS_LOCATION_ENTRY (title_widget)) {
    EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (title_widget);
    GtkWidget *site_menu_button = ephy_location_entry_get_site_menu_button (lentry);
    EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
    const char *address = ephy_web_view_get_address (view);
    EphyBookmark *bookmark;

    bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, address);
    ephy_site_menu_button_update_bookmark_item (EPHY_SITE_MENU_BUTTON (site_menu_button),
                                                !!bookmark);
  }

  g_free (location);
}

static void
sync_tab_zoom (WebKitWebView *web_view,
               GParamSpec    *pspec,
               EphyWindow    *window)
{
  GActionGroup *action_group;
  GAction *action;
  GtkWidget *lentry;
  gboolean can_zoom_in = TRUE, can_zoom_out = TRUE, can_zoom_normal = FALSE;
  double zoom;

  if (window->closing)
    return;

  zoom = webkit_web_view_get_zoom_level (web_view);

  lentry = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar)));
  if (EPHY_IS_LOCATION_ENTRY (lentry)) {
    g_autofree char *zoom_str = g_strdup_printf ("%.f%%", zoom * 100);
    ephy_location_entry_set_zoom_level (EPHY_LOCATION_ENTRY (lentry), zoom_str);
  }

  if (zoom >= ZOOM_MAXIMAL)
    can_zoom_in = FALSE;

  if (zoom <= ZOOM_MINIMAL)
    can_zoom_out = FALSE;

  if (zoom != g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL))
    can_zoom_normal = TRUE;

  action_group = ephy_window_get_action_group (window, "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "zoom-in");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_zoom_in);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "zoom-out");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_zoom_out);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "zoom-normal");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_zoom_normal);
}

static void
sync_tab_document_type (EphyWebView *view,
                        GParamSpec  *pspec,
                        EphyWindow  *window)
{
  GActionGroup *action_group;
  GAction *action;
  EphyWebViewDocumentType type;
  gboolean can_find, disable, is_image;

  if (window->closing)
    return;

  /* update zoom actions */
  sync_tab_zoom (WEBKIT_WEB_VIEW (view), NULL, window);

  type = ephy_web_view_get_document_type (view);
  can_find = (type != EPHY_WEB_VIEW_DOCUMENT_IMAGE);
  is_image = type == EPHY_WEB_VIEW_DOCUMENT_IMAGE;
  disable = (type != EPHY_WEB_VIEW_DOCUMENT_HTML);

  action_group = ephy_window_get_action_group (window, "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "encoding");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_DOCUMENT, disable);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "page-source");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_DOCUMENT, is_image);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "find");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_DOCUMENT, !can_find);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "find-prev");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_DOCUMENT, !can_find);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "find-next");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_DOCUMENT, !can_find);

  if (!can_find)
    ephy_find_toolbar_request_close (ephy_embed_get_find_toolbar (window->active_embed));
}

static void
_ephy_window_set_navigation_flags (EphyWindow                 *window,
                                   EphyWebViewNavigationFlags  flags)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = ephy_window_get_action_group (window, "toolbar");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "navigation-back");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), flags & EPHY_WEB_VIEW_NAV_BACK);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "navigation-forward");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), flags & EPHY_WEB_VIEW_NAV_FORWARD);
}

static void
sync_tab_navigation (EphyWebView *view,
                     GParamSpec  *pspec,
                     EphyWindow  *window)
{
  if (window->closing)
    return;

  _ephy_window_set_navigation_flags (window, ephy_web_view_get_navigation_flags (view));
}

static void
sync_tab_is_blank (EphyWebView *view,
                   GParamSpec  *pspec,
                   EphyWindow  *window)
{
  if (window->closing)
    return;

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_BLANK,
                                              ephy_web_view_get_is_blank (view));
}

static void
sync_tab_title (EphyEmbed  *embed,
                GParamSpec *pspec,
                EphyWindow *window)
{
  if (window->closing)
    return;

  gtk_window_set_title (GTK_WINDOW (window),
                        ephy_embed_get_title (embed));
}

static void
sync_tab_page_action (EphyWebView *view,
                      GParamSpec  *pspec,
                      EphyWindow  *window)
{
  EphyWebExtensionManager *manager;

  manager = ephy_web_extension_manager_get_default ();
  ephy_web_extension_manager_update_location_entry (manager, window);
}

static void
update_mute_action (EphyWindow    *window,
                    GParamSpec    *pspec,
                    WebKitWebView *web_view)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = ephy_window_get_action_group (window, "tab");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "mute");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), webkit_web_view_is_playing_audio (web_view));
}

static gboolean
idle_unref_context_event (EphyWindow *window)
{
  LOG ("Idle unreffing context event %p", window->context_event);

  g_clear_object (&window->context_event);

  window->idle_worker = 0;
  return FALSE;
}

static void
_ephy_window_set_context_event (EphyWindow          *window,
                                WebKitHitTestResult *hit_test_result)
{
  g_clear_handle_id (&window->idle_worker, g_source_remove);

  g_set_object (&window->context_event, hit_test_result);
}

static void
_ephy_window_unset_context_event (EphyWindow *window)
{
  /* Unref the event from idle since we still need it
   * from the action callbacks which will run before idle.
   */
  if (window->idle_worker == 0 && window->context_event) {
    window->idle_worker =
      g_idle_add ((GSourceFunc)idle_unref_context_event, window);
  }
}

static void
context_menu_dismissed_cb (WebKitWebView *webView,
                           EphyWindow    *window)
{
  LOG ("Deactivating popup menu");

  enable_edit_actions_sensitivity (window);

  g_signal_handlers_disconnect_by_func
    (webView, G_CALLBACK (context_menu_dismissed_cb), window);

  _ephy_window_unset_context_event (window);
}

static char *
ellipsize_string (const char *string,
                  glong       max_length)
{
  char *ellipsized;
  glong length = g_utf8_strlen (string, -1);

  if (length == 0)
    return NULL;

  if (length < max_length) {
    ellipsized = g_strdup (string);
  } else {
    char *str = g_utf8_substring (string, 0, max_length);
    ellipsized = g_strconcat (str, "…", NULL);
    g_free (str);
  }
  return ellipsized;
}

static char *
mnemonic_escape_string (const char *string)
{
  GString *gstring;
  const char *ptr;

  gstring = g_string_new (string);
  ptr = gstring->str;

  /* Convert each underscore to a double underscore. */
  while ((ptr = g_utf8_strchr (ptr, -1, '_'))) {
    ptrdiff_t pos = ptr - gstring->str;
    g_string_insert (gstring, pos, "_");
    ptr = gstring->str + pos + 2;
  }

  return g_string_free (gstring, FALSE);
}

static char *
format_search_label (const char *search_term)
{
  g_autofree char *ellipsized = ellipsize_string (search_term, 32);
  g_autofree char *escaped = mnemonic_escape_string (ellipsized);

  return g_strdup_printf (_("Search the Web for “%s”"), escaped);
}

static void
add_action_to_context_menu (WebKitContextMenu *context_menu,
                            GActionGroup      *action_group,
                            const char        *action_name,
                            EphyWindow        *window)
{
  GAction *action;
  char *name;
  const char *label;
  GVariant *target;

  g_action_parse_detailed_name (action_name, &name, &target, NULL);

  label = g_hash_table_lookup (window->action_labels, name);
  if (strcmp (label, "search-selection-placeholder") == 0) {
    const char *search_term;
    g_autofree char *search_label = NULL;

    search_term = g_variant_get_string (target, NULL);
    search_label = format_search_label (search_term);
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), name);
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, search_label, target));
  } else if (strcmp (label, "open-selection-placeholder") == 0) {
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-selection");
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, _("Open Link"), target));
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-selection-in-new-tab");
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, _("Open Link In New Tab"), target));
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-selection-in-new-window");
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, _("Open Link In New Window"), target));
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-selection-in-incognito-window");
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, _("Open Link In Incognito Window"), target));
  } else {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), name);
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, _(label), NULL));
  }
}

static void
add_item_to_context_menu (WebKitContextMenu     *context_menu,
                          WebKitContextMenuItem *item)
{
  if (!item)
    return;

  webkit_context_menu_append (context_menu, item);
  g_object_unref (item);
}

/* FIXME: Add webkit_context_menu_find() ? */
static WebKitContextMenuItem *
find_item_in_context_menu (WebKitContextMenu       *context_menu,
                           WebKitContextMenuAction  action)
{
  GList *items, *iter;

  items = webkit_context_menu_get_items (context_menu);
  for (iter = items; iter; iter = g_list_next (iter)) {
    WebKitContextMenuItem *item = (WebKitContextMenuItem *)iter->data;

    if (webkit_context_menu_item_get_stock_action (item) == action)
      return g_object_ref (item);
  }

  return NULL;
}

static GList *
find_spelling_guess_context_menu_items (WebKitContextMenu *context_menu)
{
  GList *items, *iter;
  guint i;
  GList *retval = NULL;

  items = webkit_context_menu_get_items (context_menu);
  for (iter = items, i = 0; iter && i < MAX_SPELL_CHECK_GUESSES; iter = g_list_next (iter), i++) {
    WebKitContextMenuItem *item = (WebKitContextMenuItem *)iter->data;

    if (webkit_context_menu_item_get_stock_action (item) == WEBKIT_CONTEXT_MENU_ACTION_SPELLING_GUESS) {
      retval = g_list_prepend (retval, g_object_ref (item));
    } else {
      /* Spelling guesses are always at the beginning of the context menu, so
       * we can break the loop as soon as we find the first item that is not
       * spelling guess.
       */
      break;
    }
  }

  return g_list_reverse (retval);
}

static gboolean
should_show_copy_outside_editable (WebKitWebView *view)
{
  WebKitEditorState *state;

  state = webkit_web_view_get_editor_state (view);

  return webkit_editor_state_is_copy_available (state);
}

static void
parse_context_menu_user_data (WebKitContextMenu  *context_menu,
                              const char        **selected_text)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, webkit_context_menu_get_user_data (context_menu));
  g_variant_dict_lookup (&dict, "SelectedText", "&s", selected_text);
}

static gboolean
populate_context_menu (WebKitWebView       *web_view,
                       WebKitContextMenu   *context_menu,
                       WebKitHitTestResult *hit_test_result,
                       EphyWindow          *window)
{
  EphyWebExtensionManager *extension_manager = ephy_web_extension_manager_get_default ();
  WebKitContextMenuItem *input_methods_item = NULL;
  WebKitContextMenuItem *insert_emoji_item = NULL;
  WebKitContextMenuItem *copy_image_item = NULL;
  WebKitContextMenuItem *play_pause_item = NULL;
  WebKitContextMenuItem *mute_item = NULL;
  WebKitContextMenuItem *toggle_controls_item = NULL;
  WebKitContextMenuItem *toggle_loop_item = NULL;
  WebKitContextMenuItem *fullscreen_item = NULL;
  WebKitContextMenuItem *paste_as_plain_text_item = NULL;
  WebKitContextMenuItem *delete_item = NULL;
  GActionGroup *window_action_group;
  GActionGroup *toolbar_action_group;
  GActionGroup *popup_action_group;
  GList *spelling_guess_items = NULL;
  gboolean app_mode, incognito_mode;
  gboolean is_document = FALSE;
  gboolean is_image = FALSE;
  gboolean is_media = FALSE;
  gboolean is_downloadable_video = FALSE;
  gboolean is_downloadable_audio = FALSE;
  gboolean is_selected_text = FALSE;
  gboolean can_search_selection = FALSE;
  gboolean can_open_selection = FALSE;
  char *search_selection_action_name = NULL;
  char *open_selection_action_name = NULL;
  const char *selected_text = NULL;
  const char *uri = NULL;
  GdkEvent *event = NULL;
  GdkModifierType state = 0;
  gboolean fullscreen_lockdown;

  fullscreen_lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN) ||
                        ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_KIOSK;

  if (fullscreen_lockdown || g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_CONTEXT_MENU))
    return GDK_EVENT_STOP;

  window_action_group = ephy_window_get_action_group (window, "win");
  toolbar_action_group = ephy_window_get_action_group (window, "toolbar");
  popup_action_group = ephy_window_get_action_group (window, "popup");

  if (webkit_hit_test_result_context_is_image (hit_test_result)) {
    is_image = TRUE;
    copy_image_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD);
  }

  if (webkit_hit_test_result_context_is_editable (hit_test_result)) {
    paste_as_plain_text_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_PASTE_AS_PLAIN_TEXT);
    input_methods_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_INPUT_METHODS);
    insert_emoji_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_INSERT_EMOJI);
    spelling_guess_items = find_spelling_guess_context_menu_items (context_menu);
    delete_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_DELETE);
  }

  if (webkit_hit_test_result_context_is_media (hit_test_result)) {
    WebKitContextMenuItem *item;

    is_media = TRUE;
    play_pause_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PLAY);
    if (!play_pause_item)
      play_pause_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PAUSE);
    mute_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_MUTE);
    toggle_controls_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_CONTROLS);
    toggle_loop_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_LOOP);
    fullscreen_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_ENTER_VIDEO_FULLSCREEN);

    item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_VIDEO_TO_DISK);
    if (item) {
      is_downloadable_video = TRUE;
      g_object_unref (item);
    } else {
      item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_AUDIO_TO_DISK);
      if (item) {
        is_downloadable_audio = TRUE;
        g_object_unref (item);
      }
    }
  }

  parse_context_menu_user_data (context_menu, &selected_text);
  if (selected_text && *selected_text) {
    is_selected_text = TRUE;

    if (g_uri_is_valid (selected_text, G_URI_FLAGS_PARSE_RELAXED, NULL)) {
      GVariant *value;

      value = g_variant_new_string (selected_text);
      open_selection_action_name = g_action_print_detailed_name ("open-selection",
                                                                 value);
      g_variant_unref (value);
      can_open_selection = TRUE;
    } else {
      GVariant *value;

      value = g_variant_new_string (selected_text);
      search_selection_action_name = g_action_print_detailed_name ("search-selection",
                                                                   value);
      g_variant_unref (value);
      can_search_selection = TRUE;
    }
  }

  webkit_context_menu_remove_all (context_menu);

  _ephy_window_set_context_event (window, hit_test_result);

  app_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION;
  incognito_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO;

  update_edit_actions_sensitivity (window, FALSE);

  if (webkit_hit_test_result_context_is_link (hit_test_result)) {
    const char *uri;
    gboolean link_has_web_scheme;

    uri = webkit_hit_test_result_get_link_uri (hit_test_result);
    link_has_web_scheme = ephy_embed_utils_address_has_web_scheme (uri);

    update_edit_actions_sensitivity (window, TRUE);
    update_link_actions_sensitivity (window, link_has_web_scheme);

    if (!app_mode) {
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-link-in-new-tab", window);
    }

    add_action_to_context_menu (context_menu, popup_action_group,
                                "open-link-in-new-window", window);
    if (!incognito_mode)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-link-in-incognito-window", window);
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());

    if (should_show_copy_outside_editable (web_view))
      add_action_to_context_menu (context_menu, window_action_group,
                                  "copy", window);
    if (can_search_selection && !app_mode)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  search_selection_action_name, window);
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, popup_action_group,
                                "download-link-as", window);

    if (g_str_has_prefix (uri, "mailto:")) {
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "copy-email-address", window);
    } else {
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "copy-link-address", window);
    }
    add_action_to_context_menu (context_menu, popup_action_group,
                                "send-via-email", window);
    add_action_to_context_menu (context_menu, popup_action_group,
                                "add-link-to-bookmarks", window);
  } else if (webkit_hit_test_result_context_is_editable (hit_test_result)) {
    GList *l;
    gboolean has_guesses = FALSE;

    /* FIXME: Add a Spelling Suggestions... submenu. Utilize
     * WEBKIT_CONTEXT_MENU_ACTION_NO_GUESSES_FOUND,
     * WEBKIT_CONTEXT_MENU_ACTION_IGNORE_SPELLING, and
     * WEBKIT_CONTEXT_MENU_ACTION_LEARN_SPELLING. */
    for (l = spelling_guess_items; l; l = g_list_next (l)) {
      WebKitContextMenuItem *item = WEBKIT_CONTEXT_MENU_ITEM (l->data);

      webkit_context_menu_append (context_menu, item);
      g_object_unref (item);
      has_guesses = TRUE;
    }
    g_list_free (spelling_guess_items);

    if (has_guesses) {
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
    }

    update_edit_actions_sensitivity (window, FALSE);

    add_action_to_context_menu (context_menu, window_action_group,
                                "undo", window);
    add_action_to_context_menu (context_menu, window_action_group,
                                "redo", window);
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "cut", window);
    add_action_to_context_menu (context_menu, window_action_group,
                                "copy", window);
    add_action_to_context_menu (context_menu, window_action_group,
                                "paste", window);
    if (paste_as_plain_text_item)
      add_action_to_context_menu (context_menu, window_action_group,
                                  "paste-as-plain-text", window);
    add_item_to_context_menu (context_menu, delete_item);
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "select-all", window);

    if (can_search_selection)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  search_selection_action_name, window);

    if (can_open_selection)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  open_selection_action_name, window);

    if (input_methods_item || insert_emoji_item)
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
    add_item_to_context_menu (context_menu, input_methods_item);
    add_item_to_context_menu (context_menu, insert_emoji_item);
  } else {
    is_document = TRUE;

    update_edit_actions_sensitivity (window, TRUE);

    if (should_show_copy_outside_editable (web_view))
      add_action_to_context_menu (context_menu, window_action_group, "copy",
                                  window);
    if (can_search_selection && !app_mode)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  search_selection_action_name, window);
    if (can_open_selection)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  open_selection_action_name, window);
    if (should_show_copy_outside_editable (web_view) || can_search_selection)
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());

    if (!is_selected_text && !is_image && !is_media) {
      add_action_to_context_menu (context_menu, toolbar_action_group,
                                  "navigation-back", window);
      add_action_to_context_menu (context_menu, toolbar_action_group,
                                  "navigation-forward", window);
      add_action_to_context_menu (context_menu, toolbar_action_group,
                                  "reload", window);
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
    }

    if (!app_mode && !is_image && !is_media)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "context-bookmark-page", window);
  }

  if (is_image) {
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, popup_action_group,
                                "save-image-as", window);
    add_item_to_context_menu (context_menu, copy_image_item);
    add_action_to_context_menu (context_menu, popup_action_group,
                                "copy-image-location", window);
    if (!app_mode)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "view-image", window);
    add_action_to_context_menu (context_menu, popup_action_group,
                                "set-image-as-background", window);
  }

  if (is_media) {
    add_item_to_context_menu (context_menu, play_pause_item);
    add_item_to_context_menu (context_menu, mute_item);
    add_item_to_context_menu (context_menu, toggle_controls_item);
    add_item_to_context_menu (context_menu, toggle_loop_item);
    add_item_to_context_menu (context_menu, fullscreen_item);
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    if (is_downloadable_video) {
      if (!app_mode) {
        add_action_to_context_menu (context_menu, popup_action_group,
                                    "open-video-in-new-window", window);
        add_action_to_context_menu (context_menu, popup_action_group,
                                    "open-video-in-new-tab", window);
      }
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "save-video-as", window);
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "copy-video-location", window);
    } else if (is_downloadable_audio) {
      if (!app_mode) {
        add_action_to_context_menu (context_menu, popup_action_group,
                                    "open-audio-in-new-window", window);
        add_action_to_context_menu (context_menu, popup_action_group,
                                    "open-audio-in-new-tab", window);
      }
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "save-audios-as", window);
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "copy-audio-location", window);
    }
  }

  g_signal_connect_object (web_view, "context-menu-dismissed",
                           G_CALLBACK (context_menu_dismissed_cb),
                           window, 0);

  g_free (search_selection_action_name);

  if (!app_mode) {
    if (is_document && !is_image && !is_media) {
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "send-via-email", window);
    }

    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "save-as", window);
    add_action_to_context_menu (context_menu, window_action_group,
                                "screenshot", window);

    if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SHOW_DEVELOPER_ACTIONS)) {
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
      uri = webkit_web_view_get_uri (web_view);
      if (uri && !strstr (uri, EPHY_VIEW_SOURCE_SCHEME)) {
        add_action_to_context_menu (context_menu, window_action_group,
                                    "page-source", window);
      }

      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_from_stock_action (WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT));
    }
  }

  event = webkit_context_menu_get_event (context_menu);
  if (event && gdk_event_get_event_type (event) == GDK_BUTTON_PRESS)
    state = gdk_event_get_modifier_state (event);

  ephy_web_extension_manager_append_context_menu (extension_manager, web_view,
                                                  context_menu, hit_test_result, state,
                                                  is_downloadable_audio, is_downloadable_video);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
save_target_uri (EphyWindow    *window,
                 WebKitWebView *view)
{
  guint context;
  char *location = NULL;
  gboolean retval = FALSE;

  g_object_get (window->hit_test_result, "context", &context, NULL);

  LOG ("save_target_uri: context %d", context);

  /* shift+click saves the link target */
  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
    g_object_get (G_OBJECT (window->hit_test_result), "link-uri", &location, NULL);
  /* Note: pressing enter to submit a form synthesizes a mouse click event */
  /* shift+click saves the non-link image */
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE &&
           !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE))
    g_object_get (G_OBJECT (window->hit_test_result), "image-uri", &location, NULL);

  if (location) {
    LOG ("Location: %s", location);

    retval = ephy_embed_utils_address_has_web_scheme (location);
    if (retval) {
      EphyDownload *download;

      download = ephy_download_new_for_uri (location);
      ephy_download_set_action (download, EPHY_DOWNLOAD_ACTION_OPEN);
      ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ())),
                                           download);
      g_object_unref (download);
    }

    g_free (location);
  }

  return retval;
}

static void
ephy_window_mouse_target_changed_cb (WebKitWebView       *web_view,
                                     WebKitHitTestResult *hit_test_result,
                                     guint                modifiers,
                                     EphyWindow          *window)
{
  if (window->hit_test_result)
    g_object_unref (window->hit_test_result);
  window->hit_test_result = g_object_ref (hit_test_result);
}

static void
web_process_terminated_cb (EphyWebView                       *web_view,
                           WebKitWebProcessTerminationReason  reason,
                           EphyWindow                        *window)
{
  gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
ephy_window_set_is_popup (EphyWindow *window,
                          gboolean    is_popup)
{
  window->is_popup = is_popup;

  g_object_notify (G_OBJECT (window), "is-popup");
}

static void
window_properties_geometry_changed (WebKitWindowProperties *properties,
                                    GParamSpec             *pspec,
                                    EphyWindow             *window)
{
  GdkRectangle geometry;

  webkit_window_properties_get_geometry (properties, &geometry);

  if (geometry.width > 0 && geometry.height > 0)
    gtk_window_set_default_size (GTK_WINDOW (window), geometry.width, geometry.height);
}

static void
ephy_window_configure_for_view (EphyWindow    *window,
                                WebKitWebView *web_view)
{
  WebKitWindowProperties *properties;
  GdkRectangle geometry;
  EphyWindowChrome chrome = 0;

  properties = webkit_web_view_get_window_properties (web_view);

  if (webkit_window_properties_get_toolbar_visible (properties))
    chrome |= EPHY_WINDOW_CHROME_HEADER_BAR;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION) {
    GtkWidget *title_widget;
    EphyLocationEntry *lentry;

    title_widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar)));
    lentry = EPHY_LOCATION_ENTRY (title_widget);
    gtk_editable_set_editable (GTK_EDITABLE (lentry), FALSE);

    if (webkit_window_properties_get_menubar_visible (properties))
      chrome |= EPHY_WINDOW_CHROME_MENU;
    if (webkit_window_properties_get_locationbar_visible (properties))
      chrome |= EPHY_WINDOW_CHROME_LOCATION;
  }

  webkit_window_properties_get_geometry (properties, &geometry);
  if (geometry.width > 0 && geometry.height > 0)
    gtk_window_set_default_size (GTK_WINDOW (window), geometry.width, geometry.height);

  if (!webkit_window_properties_get_resizable (properties))
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  window->is_popup = TRUE;
  ephy_window_set_chrome (window, chrome);
  g_signal_connect (properties, "notify::geometry",
                    G_CALLBACK (window_properties_geometry_changed),
                    window);
}

static gboolean
web_view_ready_cb (WebKitWebView *web_view,
                   WebKitWebView *parent_web_view)
{
  EphyWindow *window, *parent_view_window;
  gboolean using_new_window;

  window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (web_view)));
  parent_view_window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (parent_web_view)));

  using_new_window = window != parent_view_window;

  if (using_new_window) {
    ephy_window_configure_for_view (window, web_view);
    g_signal_emit_by_name (parent_web_view, "new-window", web_view);
  }

  gtk_widget_set_visible (GTK_WIDGET (window), TRUE);

  return TRUE;
}

static WebKitWebView *
create_web_view_cb (WebKitWebView          *web_view,
                    WebKitNavigationAction *navigation_action,
                    EphyWindow             *window)
{
  EphyEmbed *embed;
  WebKitWebView *new_web_view;
  EphyNewTabFlags flags;
  EphyWindow *target_window;

  if ((ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION) &&
      (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                               EPHY_PREFS_NEW_WINDOWS_IN_TABS) ||
       g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                               EPHY_PREFS_LOCKDOWN_FULLSCREEN))) {
    target_window = window;
    flags = EPHY_NEW_TAB_JUMP |
            EPHY_NEW_TAB_APPEND_AFTER;
  } else {
    target_window = ephy_window_new ();
    flags = EPHY_NEW_TAB_DONT_SHOW_WINDOW;
  }

  embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
                                   NULL,
                                   web_view,
                                   target_window,
                                   EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                                   flags);
  if (target_window == window)
    gtk_widget_grab_focus (GTK_WIDGET (embed));

  new_web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  g_signal_connect_object (new_web_view, "ready-to-show",
                           G_CALLBACK (web_view_ready_cb),
                           web_view, 0);

  return new_web_view;
}

typedef struct {
  EphyWindow *window;
  WebKitWebView *web_view;
  WebKitPolicyDecision *decision;
  WebKitPolicyDecisionType decision_type;
} VerifyUrlAsyncData;

static inline VerifyUrlAsyncData *
verify_url_async_data_new (EphyWindow               *window,
                           WebKitWebView            *web_view,
                           WebKitPolicyDecision     *decision,
                           WebKitPolicyDecisionType  decision_type)
{
  VerifyUrlAsyncData *data = g_new (VerifyUrlAsyncData, 1);

  data->window = g_object_ref (window);
  data->web_view = g_object_ref (web_view);
  data->decision = g_object_ref (decision);
  data->decision_type = decision_type;

  return data;
}

static inline void
verify_url_async_data_free (VerifyUrlAsyncData *data)
{
  g_object_unref (data->window);
  g_object_unref (data->web_view);
  g_object_unref (data->decision);
  g_free (data);
}

static gboolean
accept_navigation_policy_decision (EphyWindow           *window,
                                   WebKitPolicyDecision *decision,
                                   const char           *uri)
{
  g_autoptr (WebKitWebsitePolicies) website_policies = NULL;
  EphyPermission permission = EPHY_PERMISSION_UNDECIDED;
  EphyEmbedShell *shell;
  g_autofree char *origin = ephy_uri_to_security_origin (uri);

  shell = ephy_embed_shell_get_default ();

  if (origin) {
    permission = ephy_permissions_manager_get_permission (ephy_embed_shell_get_permissions_manager (shell),
                                                          EPHY_PERMISSION_TYPE_AUTOPLAY_POLICY,
                                                          origin);
  }

  switch (permission) {
    case EPHY_PERMISSION_UNDECIDED:
      website_policies = webkit_website_policies_new_with_policies ("autoplay", WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND, NULL);
      break;
    case EPHY_PERMISSION_PERMIT:
      website_policies = webkit_website_policies_new_with_policies ("autoplay", WEBKIT_AUTOPLAY_ALLOW, NULL);
      break;
    case EPHY_PERMISSION_DENY:
      website_policies = webkit_website_policies_new_with_policies ("autoplay", WEBKIT_AUTOPLAY_DENY, NULL);
      break;
  }

  webkit_policy_decision_use_with_policies (decision, website_policies);

  return TRUE;
}

static GHashTable *
load_auto_open_schemes_table (void)
{
  g_autoptr (GHashTable) table = NULL;
  GVariantIter *iter = NULL;
  char *pref_uri;
  char *pref_scheme;

  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_settings_get (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_AUTO_OPEN_SCHEMES, "a(ss)", &iter);

  while (g_variant_iter_next (iter, "(ss)", &pref_uri, &pref_scheme))
    g_hash_table_insert (table, pref_uri, pref_scheme);

  return g_steal_pointer (&table);
}

static gboolean
url_should_open_automatically (const char *opener_origin,
                               const char *navigation_uri)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) allowed_uris = NULL;
  g_autoptr (GUri) uri = g_uri_parse (navigation_uri, G_URI_FLAGS_NONE, &error);
  g_autoptr (GHashTable) table = NULL;
  const char *pref_scheme;

  if (!uri) {
    g_warning ("Failed to parse URI %s: %s", navigation_uri, error->message);
    return FALSE;
  }

  table = load_auto_open_schemes_table ();
  pref_scheme = g_hash_table_lookup (table, opener_origin);
  if (g_strcmp0 (pref_scheme, g_uri_get_scheme (uri)) == 0)
    return TRUE;

  return FALSE;
}

typedef struct {
  WebKitURIRequest *request;
  EphyWindow *window;
  char *target_scheme;
  char *opener_origin;

  GtkWidget *always_allow_switch;
} OpenURLPermissionData;

static OpenURLPermissionData *
open_url_permission_data_new (const char       *opener_origin,
                              WebKitURIRequest *request,
                              EphyWindow       *window)
{
  OpenURLPermissionData *data = g_new0 (OpenURLPermissionData, 1);
  const char *navigation_uri_str;
  g_autoptr (GUri) navigation_uri = NULL;
  g_autoptr (GError) error = NULL;

  navigation_uri_str = webkit_uri_request_get_uri (request);
  navigation_uri = g_uri_parse (navigation_uri_str, G_URI_FLAGS_NONE, &error);
  if (!navigation_uri) {
    g_warning ("Could not parse ask permission uri %s: %s", navigation_uri_str, error->message);
    return NULL;
  }

  data->opener_origin = g_strdup (opener_origin);
  data->target_scheme = g_strdup (g_uri_get_scheme (navigation_uri));
  data->request = g_object_ref (request);
  data->window = g_object_ref (window);

  return data;
}

static void
open_url_permission_data_new_free (OpenURLPermissionData *data)
{
  g_clear_pointer (&data->opener_origin, g_free);
  g_clear_pointer (&data->target_scheme, g_free);
  g_clear_object (&data->request);
  g_clear_object (&data->always_allow_switch);
  g_clear_object (&data->window);
}

static void
add_auto_open_scheme (char *openeer_uri,
                      char *target_scheme)
{
  GVariantBuilder builder;
  GVariant *variant;
  GHashTable *table = load_auto_open_schemes_table ();
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  g_hash_table_iter_init (&iter, table);

  while (g_hash_table_iter_next (&iter, &key, &value))
    g_variant_builder_add (&builder, "(ss)", key, value);

  g_variant_builder_add (&builder, "(ss)", openeer_uri, target_scheme);

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_AUTO_OPEN_SCHEMES, variant);
}

static void
on_open (AdwAlertDialog *self,
         gchar          *response,
         gpointer        user_data)
{
  OpenURLPermissionData *data = user_data;

  if (g_strcmp0 (response, "open") == 0) {
    const char *uri = webkit_uri_request_get_uri (data->request);
    g_autoptr (GFile) file = g_file_new_for_uri (uri);
    gboolean always_allow = adw_switch_row_get_active (ADW_SWITCH_ROW (data->always_allow_switch));

    if (always_allow)
      add_auto_open_scheme (data->opener_origin, data->target_scheme);

    ephy_file_launch_uri_handler (file,
                                  NULL,
                                  gtk_widget_get_display (GTK_WIDGET (data->window)),
                                  EPHY_FILE_LAUNCH_URI_HANDLER_FILE);
  }

  g_clear_pointer (&data, open_url_permission_data_new_free);
}

static gboolean
ask_for_permission (gpointer user_data)
{
  OpenURLPermissionData *data = user_data;
  g_autofree char *body = NULL;
  AdwDialog *dialog;
  GtkWidget *always_allow_switch;
  GtkWidget *group;

  /* Additional user interaction is required to prevent malicious websites
   * from launching URL handler application's on the user's computer without
   * explicit user consent. Notably, websites can download malicious files
   * without consent, so they should not be able to also open those files
   * without user interaction.
   */
  body = g_strdup_printf (_("Allow “%s” to open the “%s” link in an external app"), data->opener_origin, data->target_scheme);
  dialog = adw_alert_dialog_new (_("Open in External App?"), body);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "cancel", _("_Cancel"));
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "open", _("_Open Link"));
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "open", ADW_RESPONSE_SUGGESTED);

  group = adw_preferences_group_new ();
  always_allow_switch = adw_switch_row_new ();
  data->always_allow_switch = g_object_ref (always_allow_switch);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (always_allow_switch), _("Always Allow"));
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), always_allow_switch);
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), group);
  adw_dialog_present (dialog, GTK_WIDGET (data->window));

  g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (on_open), data);

  return G_SOURCE_REMOVE;
}

static gboolean
decide_navigation_policy (WebKitWebView            *web_view,
                          WebKitPolicyDecision     *decision,
                          WebKitPolicyDecisionType  decision_type,
                          EphyWindow               *window)
{
  WebKitNavigationPolicyDecision *navigation_decision;
  WebKitNavigationAction *navigation_action;
  WebKitNavigationType navigation_type;
  WebKitURIRequest *request;
  const char *request_uri;

  g_assert (WEBKIT_IS_WEB_VIEW (web_view));
  g_assert (WEBKIT_IS_NAVIGATION_POLICY_DECISION (decision));
  g_assert (decision_type != WEBKIT_POLICY_DECISION_TYPE_RESPONSE);
  g_assert (EPHY_IS_WINDOW (window));

  navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
  navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  request = webkit_navigation_action_get_request (navigation_action);
  request_uri = webkit_uri_request_get_uri (request);

  if (!ephy_embed_utils_address_has_web_scheme (request_uri)) {
    g_autofree char *opener_origin = NULL;
    const char *view_uri = webkit_web_view_get_uri (web_view);

    webkit_policy_decision_ignore (decision);

    if (!view_uri)
      return TRUE;

    opener_origin = ephy_uri_to_security_origin (view_uri);
    if (!opener_origin)
      return TRUE;

    if (url_should_open_automatically (opener_origin, request_uri)) {
      g_autoptr (GFile) file = g_file_new_for_uri (request_uri);

      ephy_file_launch_uri_handler (file,
                                    NULL,
                                    gtk_widget_get_display (GTK_WIDGET (window)),
                                    EPHY_FILE_LAUNCH_URI_HANDLER_FILE);
    } else {
      OpenURLPermissionData *data;

      /* User gesture is required to prevent websites from spamming open URL
       * requests unless the user has actually interacted with the website. (This
       * corresponds roughly to "transient activation" in the HTML standard.)
       */
      if (!webkit_navigation_action_is_user_gesture (navigation_action))
        return TRUE;

      data = open_url_permission_data_new (opener_origin, request, window);
      if (data)
        g_idle_add (ask_for_permission, data);
    }

    return TRUE;
  }

  if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION &&
      !g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_POPUPS) &&
      !webkit_navigation_action_is_user_gesture (navigation_action)) {
    webkit_policy_decision_ignore (decision);
    return TRUE;
  }

  navigation_type = webkit_navigation_action_get_navigation_type (navigation_action);

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    if (!gtk_widget_is_visible (GTK_WIDGET (window))) {
      if (ephy_web_application_is_uri_allowed (request_uri)) {
        gtk_widget_set_visible (GTK_WIDGET (window), TRUE);
      } else {
        ephy_file_open_uri_in_default_browser (request_uri, gtk_widget_get_display (GTK_WIDGET (window)));
        webkit_policy_decision_ignore (decision);

        gtk_window_destroy (GTK_WINDOW (window));

        return TRUE;
      }
    }

    if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED ||
        (navigation_type == WEBKIT_NAVIGATION_TYPE_OTHER && webkit_navigation_action_is_user_gesture (navigation_action))) {
      if (ephy_web_application_is_uri_allowed (request_uri))
        return accept_navigation_policy_decision (window, decision, request_uri);

      ephy_file_open_uri_in_default_browser (request_uri, gtk_widget_get_display (GTK_WIDGET (window)));
      webkit_policy_decision_ignore (decision);

      return TRUE;
    }
  }

  if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED) {
    gint button;
    gint state;
    EphyEmbed *new_embed;
    EphyWebView *new_view;
    EphyNewTabFlags flags = 0;
    EphyWindow *target_window = window;
    gboolean inherit_session = FALSE;

    button = webkit_navigation_action_get_mouse_button (navigation_action);
    state = webkit_navigation_action_get_modifiers (navigation_action);

    ephy_web_view_set_visit_type (EPHY_WEB_VIEW (web_view),
                                  EPHY_PAGE_VISIT_LINK);

    /* New tab in new window for shift+click */
    if (button == GDK_BUTTON_PRIMARY && state == GDK_SHIFT_MASK &&
        !g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                                 EPHY_PREFS_LOCKDOWN_FULLSCREEN)) {
      target_window = ephy_window_new ();
    }
    /* New background tab in existing window for middle click and
     * control+click */
    else if (button == GDK_BUTTON_MIDDLE ||
             (button == GDK_BUTTON_PRIMARY && (state == GDK_CONTROL_MASK))) {
      flags |= EPHY_NEW_TAB_APPEND_AFTER;

      if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
        flags |= EPHY_NEW_TAB_JUMP;
    }
    /* New active tab in existing window for control+shift+click */
    else if (button == GDK_BUTTON_PRIMARY && (state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))) {
      flags |= EPHY_NEW_TAB_APPEND_AFTER;
      inherit_session = TRUE;
    }
    /* Alt+click means download URI */
    else if (button == GDK_BUTTON_PRIMARY && state == GDK_ALT_MASK) {
      if (save_target_uri (window, web_view)) {
        webkit_policy_decision_ignore (decision);
        return TRUE;
      }
    } else {
      return accept_navigation_policy_decision (window, decision, request_uri);
    }

    new_embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
                                         NULL, NULL,
                                         target_window,
                                         window->active_embed,
                                         flags);

    new_view = ephy_embed_get_web_view (new_embed);
    if (inherit_session) {
      WebKitWebViewSessionState *session_state;

      session_state = webkit_web_view_get_session_state (web_view);
      webkit_web_view_restore_session_state (WEBKIT_WEB_VIEW (new_view), session_state);
      webkit_web_view_session_state_unref (session_state);

      if (button == GDK_BUTTON_PRIMARY)
        ephy_embed_container_set_active_child (EPHY_EMBED_CONTAINER (window), new_embed);
    }
    ephy_web_view_load_request (new_view, request);

    webkit_policy_decision_ignore (decision);

    return TRUE;
  }

  return accept_navigation_policy_decision (window, decision, request_uri);
}

static void
resolve_pending_decision (VerifyUrlAsyncData *async_data)
{
  decide_navigation_policy (async_data->web_view, async_data->decision, async_data->decision_type, async_data->window);
}

static void
filters_initialized_cb (EphyFiltersManager *filters_manager,
                        GParamSpec         *pspec,
                        EphyWindow         *window)
{
  /* This function is only ever invoked after initialization has completed. */
  g_assert (ephy_filters_manager_get_is_initialized (filters_manager));

  g_signal_handler_disconnect (filters_manager, window->filters_initialized_id);

  g_list_foreach (window->pending_decisions, (GFunc)resolve_pending_decision, NULL);
  g_list_free_full (window->pending_decisions, (GDestroyNotify)verify_url_async_data_free);
  window->pending_decisions = NULL;
}

static gboolean
decide_policy_cb (WebKitWebView            *web_view,
                  WebKitPolicyDecision     *decision,
                  WebKitPolicyDecisionType  decision_type,
                  EphyWindow               *window)
{
  const char *uri;

  /* Response policy decisions are handled in EphyWebView instead. */
  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
      decision_type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    return FALSE;

  /* We don't want to allow HTTP requests before the adblocker is ready, except
   * for internal Epiphany pages, which should never be delayed.
   */
  uri = webkit_web_view_get_uri (web_view);
  if (uri && !g_str_has_prefix (uri, "ephy-about:")) {
    EphyFiltersManager *filters_manager = ephy_embed_shell_get_filters_manager (ephy_embed_shell_get_default ());
    if (!ephy_filters_manager_get_is_initialized (filters_manager)) {
      /* Queue request while filters initialization is in progress. */
      VerifyUrlAsyncData *async_data = verify_url_async_data_new (window,
                                                                  web_view,
                                                                  decision,
                                                                  decision_type);
      window->pending_decisions = g_list_append (window->pending_decisions,
                                                 async_data);
      if (!window->filters_initialized_id) {
        window->filters_initialized_id =
          g_signal_connect_object (filters_manager,
                                   "notify::is-initialized",
                                   G_CALLBACK (filters_initialized_cb),
                                   window, 0);
      }
      return TRUE;
    }
  }

  return decide_navigation_policy (web_view, decision, decision_type, window);
}

static void
progress_update (WebKitWebView *web_view,
                 GParamSpec    *pspec,
                 EphyWindow    *window)
{
  EphyTitleWidget *title_widget;
  gdouble progress;
  gboolean loading;
  const char *address;

  progress = webkit_web_view_get_estimated_load_progress (web_view);
  loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (web_view));

  address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
  if (ephy_embed_utils_is_no_show_address (address))
    loading = FALSE;

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));
  ephy_location_entry_set_progress (EPHY_LOCATION_ENTRY (title_widget), progress, loading);
}

static void
load_changed_cb (EphyWebView     *view,
                 WebKitLoadEvent  load_event,
                 EphyWindow      *window)
{
  sync_tab_load_status (view, load_event, window);
  sync_tab_address (view, NULL, window);

  if (load_event != WEBKIT_LOAD_STARTED)
    return;
}

static void
ephy_window_connect_active_embed (EphyWindow *window)
{
  EphyEmbed *embed;
  WebKitWebView *web_view;
  EphyWebView *view;
  EphyTitleWidget *title_widget;

  g_assert (window->active_embed);

  embed = window->active_embed;
  view = ephy_embed_get_web_view (embed);
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  ephy_embed_attach_notification_container (window->active_embed);

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  /* This doesn't change the value returned by ephy_web_view_get_location_entry_has_focus(). */
  gtk_widget_grab_focus (GTK_WIDGET (view));

  if (EPHY_IS_LOCATION_ENTRY (title_widget)) {
    GListModel *model = ephy_web_view_get_opensearch_engines (view);
    EphyAddOpensearchEngineButton *opensearch_button =
      EPHY_ADD_OPENSEARCH_ENGINE_BUTTON (ephy_location_entry_get_opensearch_button (EPHY_LOCATION_ENTRY (title_widget)));
    ephy_add_opensearch_engine_button_set_model (opensearch_button, model);

    if (ephy_web_view_get_location_entry_has_focus (view)) {
      ephy_location_entry_grab_focus_without_selecting (EPHY_LOCATION_ENTRY (title_widget));
      ephy_location_entry_set_position (EPHY_LOCATION_ENTRY (title_widget), ephy_web_view_get_location_entry_position (view));
    }

    if (ephy_embed_get_do_animate_reader_mode (embed)) {
      GtkWidget *site_menu_button = ephy_location_entry_get_site_menu_button (EPHY_LOCATION_ENTRY (title_widget));

      ephy_site_menu_button_animate_reader_mode (EPHY_SITE_MENU_BUTTON (site_menu_button));
      ephy_embed_set_do_animate_reader_mode (embed, FALSE);
    }
  }

  sync_tab_security (view, NULL, window);
  sync_tab_document_type (view, NULL, window);
  sync_tab_load_status (view, WEBKIT_LOAD_STARTED, window);
  sync_tab_is_blank (view, NULL, window);
  sync_tab_navigation (view, NULL, window);
  sync_tab_title (embed, NULL, window);
  sync_tab_address (view, NULL, window);

  sync_tab_zoom (web_view, NULL, window);
  sync_tab_page_action (view, NULL, window);

  if (EPHY_IS_LOCATION_ENTRY (title_widget)) {
    gdouble progress = webkit_web_view_get_estimated_load_progress (web_view);
    gboolean loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (web_view));

    ephy_location_entry_set_progress (EPHY_LOCATION_ENTRY (title_widget), progress, loading);
    g_signal_connect_object (web_view, "notify::estimated-load-progress",
                             G_CALLBACK (progress_update),
                             window, 0);
  }

  g_signal_connect_object (web_view, "notify::is-playing-audio",
                           G_CALLBACK (update_mute_action), window,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (web_view, "notify::zoom-level",
                           G_CALLBACK (sync_tab_zoom),
                           window, 0);

  g_signal_connect_object (web_view, "create",
                           G_CALLBACK (create_web_view_cb),
                           window, 0);
  g_signal_connect_object (web_view, "decide-policy",
                           G_CALLBACK (decide_policy_cb),
                           window, 0);

  g_signal_connect_object (embed, "notify::title",
                           G_CALLBACK (sync_tab_title),
                           window, 0);
  g_signal_connect_object (view, "notify::address",
                           G_CALLBACK (sync_tab_address),
                           window, 0);
  g_signal_connect_object (view, "notify::security-level",
                           G_CALLBACK (sync_tab_security),
                           window, 0);
  g_signal_connect_object (view, "notify::document-type",
                           G_CALLBACK (sync_tab_document_type),
                           window, 0);
  g_signal_connect_object (view, "load-changed",
                           G_CALLBACK (load_changed_cb),
                           window, 0);
  g_signal_connect_object (view, "notify::navigation",
                           G_CALLBACK (sync_tab_navigation),
                           window, 0);
  g_signal_connect_object (view, "notify::is-blank",
                           G_CALLBACK (sync_tab_is_blank),
                           window, 0);
  g_signal_connect_object (view, "context-menu",
                           G_CALLBACK (populate_context_menu),
                           window, 0);
  g_signal_connect_object (view, "mouse-target-changed",
                           G_CALLBACK (ephy_window_mouse_target_changed_cb),
                           window, 0);
  g_signal_connect_object (view, "web-process-terminated",
                           G_CALLBACK (web_process_terminated_cb),
                           window, 0);

  update_mute_action (window, NULL, WEBKIT_WEB_VIEW (view));

  ephy_mouse_gesture_controller_set_web_view (window->mouse_gesture_controller, web_view);

  g_object_notify (G_OBJECT (window), "active-child");
}

static void
ephy_window_disconnect_active_embed (EphyWindow *window)
{
  EphyEmbed *embed;
  WebKitWebView *web_view;
  EphyWebView *view;
  EphyTitleWidget *title_widget;

  g_assert (window->active_embed);

  embed = window->active_embed;
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  view = EPHY_WEB_VIEW (web_view);

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  if (EPHY_IS_LOCATION_ENTRY (title_widget)) {
    EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (title_widget);
    EphySiteMenuButton *site_menu_button;

    ephy_web_view_set_location_entry_has_focus (view, ephy_location_entry_has_focus (lentry));
    ephy_web_view_set_location_entry_position (view, gtk_editable_get_position (GTK_EDITABLE (lentry)));

    site_menu_button = EPHY_SITE_MENU_BUTTON (ephy_location_entry_get_site_menu_button (lentry));
    if (ephy_site_menu_button_is_animating (site_menu_button))
      ephy_site_menu_button_cancel_animation (site_menu_button);
  }

  ephy_embed_detach_notification_container (window->active_embed);

  ephy_mouse_gesture_controller_unset_web_view (window->mouse_gesture_controller);

  g_signal_handlers_disconnect_by_func (web_view,
                                        G_CALLBACK (progress_update),
                                        window);
  g_signal_handlers_disconnect_by_func (web_view,
                                        G_CALLBACK (sync_tab_zoom),
                                        window);
  g_signal_handlers_disconnect_by_func (web_view,
                                        G_CALLBACK (create_web_view_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (decide_policy_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (embed,
                                        G_CALLBACK (sync_tab_title),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_address),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_security),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_document_type),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (load_changed_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_navigation),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_is_blank),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (populate_context_menu),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (ephy_window_mouse_target_changed_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (web_process_terminated_cb),
                                        window);
}

static void
ephy_window_set_active_tab (EphyWindow *window,
                            EphyEmbed  *new_embed)
{
  EphyEmbed *old_embed;

  g_assert (EPHY_IS_WINDOW (window));
  g_assert (gtk_widget_get_root (GTK_WIDGET (new_embed)) == GTK_ROOT (window));

  old_embed = window->active_embed;

  if (old_embed == new_embed)
    return;

  if (old_embed)
    ephy_window_disconnect_active_embed (window);

  window->active_embed = new_embed;

  if (new_embed)
    ephy_window_connect_active_embed (window);
}

static void
tab_view_setup_menu_cb (AdwTabView *tab_view,
                        AdwTabPage *page,
                        EphyWindow *window)
{
  EphyWebView *view = NULL;
  GActionGroup *action_group;
  GAction *action;
  int n_pages;
  int n_pinned_pages;
  int position;
  gboolean pinned;
  gboolean muted;
  gboolean overview_open = adw_tab_overview_get_open (ADW_TAB_OVERVIEW (window->overview));

  if (page) {
    view = ephy_embed_get_web_view (EPHY_EMBED (adw_tab_page_get_child (page)));
    n_pages = adw_tab_view_get_n_pages (tab_view);
    n_pinned_pages = adw_tab_view_get_n_pinned_pages (tab_view);
    position = adw_tab_view_get_page_position (tab_view, page);
    pinned = adw_tab_page_get_pinned (page);
  }

  action_group = ephy_window_get_action_group (window, "toolbar");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "reload");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !overview_open || page);

  action_group = ephy_window_get_action_group (window, "tab");

  /* enable/disable close others/left/right */
  /* If there's no page, enable all actions so that we don't interfere with hotkeys */
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "close-left");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !page || position > n_pinned_pages);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "close-right");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !page || (position < n_pages - 1 && !pinned));

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "close-others");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !page || (n_pages > n_pinned_pages + 1 && !pinned));

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "reload-all");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !page || n_pages > 1);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "pin");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !page || !pinned);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "unpin");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !page || pinned);

  muted = view && webkit_web_view_get_is_muted (WEBKIT_WEB_VIEW (view));
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "mute");
  g_simple_action_set_state (G_SIMPLE_ACTION (action),
                             g_variant_new_boolean (muted));
}

static gboolean
present_on_idle_cb (GtkWindow *window)
{
  gtk_window_present (window);
  g_object_unref (window);

  return FALSE;
}

static gboolean
delayed_remove_child (gpointer data)
{
  g_autoptr (GtkWidget) widget = GTK_WIDGET (data);
  EphyEmbedContainer *container = EPHY_EMBED_CONTAINER (gtk_widget_get_root (widget));

  ephy_embed_container_remove_child (container, EPHY_EMBED (widget));

  return FALSE;
}

static void
download_only_load_cb (EphyWebView *view,
                       EphyWindow  *window)
{
  if (ephy_tab_view_get_n_pages (window->tab_view) == 1) {
    ephy_web_view_load_homepage (view);
    return;
  }

  g_idle_add (delayed_remove_child, g_object_ref (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view)));
}

static void
update_reader_mode (EphyWindow  *window,
                    EphyWebView *view,
                    gboolean     update_entry)
{
  EphyWebView *active_view;
  GActionGroup *action_group;
  GAction *action;
  gboolean active;
  gboolean enabled;
  EphyTitleWidget *title_widget;
  EphyLocationEntry *entry;
  EphySiteMenuButton *site_menu_button;

  if (!window->active_embed || !gtk_widget_get_parent (GTK_WIDGET (view)))
    return;

  active = g_str_has_prefix (ephy_web_view_get_display_address (EPHY_WEB_VIEW (view)), EPHY_READER_SCHEME);
  enabled = (ephy_web_view_is_reader_mode_available (view) || active);

  active_view = ephy_embed_get_web_view (window->active_embed);
  if (active_view != view) {
    EphyEmbed *embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

    ephy_embed_set_do_animate_reader_mode (embed, enabled && !active);
    return;
  }

  action_group = ephy_window_get_action_group (window, "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "toggle-reader-mode");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (active));

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));
  if (!EPHY_IS_LOCATION_ENTRY (title_widget))
    return;

  entry = EPHY_LOCATION_ENTRY (title_widget);
  site_menu_button = EPHY_SITE_MENU_BUTTON (ephy_location_entry_get_site_menu_button (entry));

  if (update_entry && enabled && !active)
    ephy_site_menu_button_animate_reader_mode (site_menu_button);

  if (enabled)
    ephy_site_menu_button_append_description (site_menu_button, _("Reader Mode Available"));
}

static void
reader_mode_cb (EphyWebView *view,
                GParamSpec  *pspec,
                EphyWindow  *window)
{
  update_reader_mode (window, view, TRUE);
}

typedef struct {
  WebKitPermissionRequest *request;
  EphyPermissionType permission_type;
  char *origin;
} EphyPermissionRequestData;

static EphyPermissionRequestData *
ephy_permission_request_data_new (WebKitPermissionRequest *request,
                                  EphyPermissionType       permission_type,
                                  const char              *origin)
{
  EphyPermissionRequestData *data = g_new (EphyPermissionRequestData, 1);

  data->request = g_object_ref (request);
  data->permission_type = permission_type;
  data->origin = g_strdup (origin);

  return data;
}

static void
ephy_permission_request_data_free (EphyPermissionRequestData *data)
{
  g_clear_object (&data->request);
  g_free (data->origin);

  g_free (data);
}

static void
set_permission (EphyPermissionRequestData *data,
                gboolean                   response)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  EphyPermissionsManager *permissions_manager = ephy_embed_shell_get_permissions_manager (shell);

  if (ephy_permission_is_stored_by_permissions_manager (data->permission_type)) {
    ephy_permissions_manager_set_permission (permissions_manager,
                                             data->permission_type, data->origin,
                                             response ? EPHY_PERMISSION_PERMIT
                                                      : EPHY_PERMISSION_DENY);
  } else if (EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE) {
    ephy_permissions_manager_set_permission (permissions_manager,
                                             EPHY_PERMISSION_TYPE_ACCESS_WEBCAM,
                                             data->origin,
                                             response ? EPHY_PERMISSION_PERMIT
                                                      : EPHY_PERMISSION_DENY);
    ephy_permissions_manager_set_permission (permissions_manager,
                                             EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE,
                                             data->origin,
                                             response ? EPHY_PERMISSION_PERMIT
                                                      : EPHY_PERMISSION_DENY);
  }

  ephy_permission_request_data_free (data);
}

static void
on_permission_allow (AdwAlertDialog            *self,
                     char                      *response,
                     EphyPermissionRequestData *data)
{
  webkit_permission_request_allow (data->request);
  set_permission (data, TRUE);
}

static void
on_permission_deny (AdwAlertDialog            *self,
                    char                      *response,
                    EphyPermissionRequestData *data)
{
  webkit_permission_request_deny (data->request);
  set_permission (data, FALSE);
}

static void
permission_dialog_get_text (EphyPermissionRequestData  *data,
                            char                      **title,
                            char                      **message)
{
  const char *requesting_domain = NULL;
  const char *current_domain = NULL;
  g_autofree char *bold_origin = NULL;

  bold_origin = g_markup_printf_escaped ("<b>%s</b>", data->origin);
  switch (data->permission_type) {
    case EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
      /* Translators: Notification policy for a specific site. */
      *title = g_strdup (_("Notification Request"));
      /* Translators: Notification policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to send you notifications"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_LOCATION:
      /* Translators: Geolocation policy for a specific site. */
      *title = g_strdup (_("Location Access Request"));
      /* Translators: Geolocation policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to know your location"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE:
      /* Translators: Microphone policy for a specific site. */
      *title = g_strdup (_("Microphone Access Request"));
      /* Translators: Microphone policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to use your microphone"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM:
      /* Translators: Webcam policy for a specific site. */
      *title = g_strdup (_("Webcam Access Request"));
      /* Translators: Webcam policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to use your webcam"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE:
      /* Translators: Webcam and microphone policy for a specific site. */
      *title = g_strdup (_("Webcam and Microphone Access Request"));
      /* Translators: Webcam and microphone policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to use your webcam and microphone"),
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_DISPLAY:
      /* Translators: Display access policy for a specific site. */
      *title = g_strdup ("Display Access Request");
      /* Translators: Display access policy for a specific site. */
      *message = g_strdup_printf ("The page at “%s” would like to share your screen",
                                  bold_origin);
      break;
    case EPHY_PERMISSION_TYPE_WEBSITE_DATA_ACCESS:
      requesting_domain = webkit_website_data_access_permission_request_get_requesting_domain (WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST (data->request));
      current_domain = webkit_website_data_access_permission_request_get_current_domain (WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST (data->request));
      /* Translators: Storage access policy for a specific site. */
      *title = g_strdup (_("Website Data Access Request"));
      /* Translators: Storage access policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to access its own data (including cookies) while browsing “%s”. This will allow “%s” to track your activity on “%s”."),
                                  requesting_domain, current_domain, requesting_domain, current_domain);
      break;
    case EPHY_PERMISSION_TYPE_CLIPBOARD:
      /* Translators: Clipboard policy for a specific site. */
      *title = g_strdup (_("Clipboard Access Request"));
      /* Translators: Clipboard policy for a specific site. */
      *message = g_strdup_printf (_("The page at “%s” would like to access your clipboard"),
                                  bold_origin);
      break;
    default:
      g_assert_not_reached ();
  }
}

static void
permission_requested_cb (EphyWebView             *view,
                         EphyPermissionType       permission_type,
                         WebKitPermissionRequest *request,
                         const char              *origin,
                         EphyWindow              *window)
{
  EphyPermissionRequestData *data;
  AdwDialog *dialog;
  g_autofree char *title = NULL;
  g_autofree char *message = NULL;

  if (!gtk_widget_is_visible (GTK_WIDGET (window)))
    return;

  data = ephy_permission_request_data_new (request, permission_type, origin);
  permission_dialog_get_text (data, &title, &message);
  dialog = adw_alert_dialog_new (title, message);

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "close", _("_Ask Later"),
                                  "deny", _("_Deny"),
                                  "allow", _("_Allow"),
                                  NULL);

  adw_alert_dialog_set_body_use_markup (ADW_ALERT_DIALOG (dialog), TRUE);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "deny", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "allow", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "close");

  g_signal_connect (dialog, "response::allow", G_CALLBACK (on_permission_allow), data);
  g_signal_connect (dialog, "response::deny", G_CALLBACK (on_permission_deny), data);

  adw_dialog_present (dialog, GTK_WIDGET (window));
}

static void
tab_view_page_attached_cb (AdwTabView *tab_view,
                           AdwTabPage *page,
                           gint        position,
                           EphyWindow *window)
{
  GtkWidget *content = adw_tab_page_get_child (page);
  EphyEmbed *embed;

  g_assert (EPHY_IS_EMBED (content));

  embed = EPHY_EMBED (content);

  LOG ("page-attached tab view %p embed %p position %d\n", tab_view, embed, position);

  g_signal_connect_object (ephy_embed_get_web_view (embed), "download-only-load",
                           G_CALLBACK (download_only_load_cb), window, G_CONNECT_AFTER);

  g_signal_connect_object (ephy_embed_get_web_view (embed), "permission-requested",
                           G_CALLBACK (permission_requested_cb), window, G_CONNECT_AFTER);

  g_signal_connect_object (ephy_embed_get_web_view (embed), "notify::reader-mode",
                           G_CALLBACK (reader_mode_cb), window, G_CONNECT_AFTER);

  if (window->present_on_insert) {
    window->present_on_insert = FALSE;
    g_idle_add ((GSourceFunc)present_on_idle_cb, g_object_ref (window));
  }
}

static void
tab_view_page_detached_cb (AdwTabView *tab_view,
                           AdwTabPage *page,
                           gint        position,
                           EphyWindow *window)
{
  GtkWidget *content = adw_tab_page_get_child (page);

  LOG ("page-detached tab view %p embed %p position %d\n", tab_view, content, position);

  if (window->closing)
    return;

  g_assert (EPHY_IS_EMBED (content));

  g_signal_handlers_disconnect_by_func
    (ephy_embed_get_web_view (EPHY_EMBED (content)), G_CALLBACK (download_only_load_cb), window);
  g_signal_handlers_disconnect_by_func
    (ephy_embed_get_web_view (EPHY_EMBED (content)), G_CALLBACK (permission_requested_cb), window);

  if (ephy_tab_view_get_n_pages (window->tab_view) == 0)
    window->active_embed = NULL;
}

static void
ephy_window_close_tab (EphyWindow *window,
                       EphyEmbed  *tab)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (shell);
  gboolean keep_window_open = FALSE;

  /* This function can be called many times for the same embed if the
   * web process (or network process) has hung. E.g. the user could
   * click the close button several times. This is difficult to guard
   * against elsewhere.
   */
  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tab), "ephy-window-close-tab-closed")))
    return;

  if (mode != EPHY_EMBED_SHELL_MODE_AUTOMATION)
    keep_window_open = g_settings_get_boolean (EPHY_SETTINGS_UI, EPHY_PREFS_UI_KEEP_WINDOW_OPEN);

  if (keep_window_open && ephy_tab_view_get_n_pages (window->tab_view) == 1) {
    EphyWebView *view = ephy_embed_get_web_view (tab);

    if (ephy_web_view_get_is_blank (view) ||
        ephy_web_view_is_newtab (view) ||
        ephy_web_view_is_overview (view))
      return;

    ephy_link_open (EPHY_LINK (window), NULL, NULL, EPHY_LINK_NEW_TAB);
  }

  g_object_set_data (G_OBJECT (tab), "ephy-window-close-tab-closed", GINT_TO_POINTER (TRUE));

  /* If that was the last tab, destroy the window.
   *
   * Beware: window->closing could be true now, after destroying the
   * tab, even if it wasn't at the start of this function.
   */
  if (!window->closing && ephy_tab_view_get_n_pages (window->tab_view) == 0 &&
      !adw_tab_overview_get_open (ADW_TAB_OVERVIEW (window->overview)))
    gtk_window_destroy (GTK_WINDOW (window));
}

typedef struct {
  EphyWindow *window;
  EphyEmbed *embed;
  AdwTabPage *page;
} TabHasModifiedFormsData;

static TabHasModifiedFormsData *
tab_has_modified_forms_data_new (EphyWindow *window,
                                 EphyEmbed  *embed,
                                 AdwTabPage *page)
{
  TabHasModifiedFormsData *data = g_new (TabHasModifiedFormsData, 1);
  data->window = window;
  data->embed = g_object_ref (embed);
  data->page = page;
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&data->window);
  g_object_add_weak_pointer (G_OBJECT (page), (gpointer *)&data->page);
  return data;
}

static void
tab_has_modified_forms_data_free (TabHasModifiedFormsData *data)
{
  g_clear_weak_pointer (&data->window);
  g_clear_object (&data->embed);
  g_clear_weak_pointer (&data->page);
  g_clear_pointer (&data, g_free);
}

static void
tab_has_modified_forms_dialog_cb (AdwAlertDialog          *dialog,
                                  const char              *response,
                                  TabHasModifiedFormsData *data)
{
  AdwTabView *tab_view = ephy_tab_view_get_tab_view (data->window->tab_view);

  if (!strcmp (response, "accept")) {
    /* It's safe to close the tab immediately because we are only checking a
     * single tab for modified forms here. There is an entirely separate
     * codepath for checking modified forms when closing the whole window,
     * see ephy_window_check_modified_forms().
     */
    adw_tab_view_close_page_finish (tab_view, data->page, TRUE);
    ephy_window_close_tab (data->window, data->embed);
  } else {
    adw_tab_view_close_page_finish (tab_view, data->page, FALSE);
  }

  tab_has_modified_forms_data_free (data);
}

static void
tab_has_modified_forms_cb (EphyWebView             *view,
                           GAsyncResult            *result,
                           TabHasModifiedFormsData *data)
{
  gboolean has_modified_forms;

  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);

  if (data->window && data->embed && data->page) {
    AdwTabView *tab_view = ephy_tab_view_get_tab_view (data->window->tab_view);

    if (!has_modified_forms) {
      adw_tab_view_close_page_finish (tab_view, data->page, TRUE);
      ephy_window_close_tab (data->window, data->embed);
    } else {
      AdwDialog *dialog;

      dialog = construct_confirm_close_dialog (data->window,
                                               _("Leave Website?"),
                                               _("A form was modified and has not been submitted"),
                                               _("_Discard Form"));

      g_signal_connect (dialog, "response",
                        G_CALLBACK (tab_has_modified_forms_dialog_cb),
                        data);
      adw_dialog_present (dialog, GTK_WIDGET (data->window));

      return;
    }
  }

  tab_has_modified_forms_data_free (data);
}

static void
run_downloads_in_background (EphyWindow *window,
                             int         num)
{
  g_autoptr (GNotification) notification = NULL;
  g_autofree char *body = NULL;

  notification = g_notification_new (_("Download operation"));
  g_notification_set_default_action (notification, "app.show-downloads");
  g_notification_add_button (notification, _("Show details"), "app.show-downloads");

  body = g_strdup_printf (ngettext ("%d download operation active",
                                    "%d download operations active",
                                    num), num);
  g_notification_set_body (notification, body);

  ephy_shell_send_notification (ephy_shell_get_default (), "progress", notification);

  gtk_widget_set_visible (GTK_WIDGET (window), FALSE);
}

static gboolean
check_and_run_downloads_in_background (EphyWindow *self)
{
  EphyDownloadsManager *manager = ephy_embed_shell_get_downloads_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

  if (ephy_downloads_manager_has_active_downloads (manager)) {
    GList *list = ephy_downloads_manager_get_downloads (manager);
    run_downloads_in_background (self, g_list_length (list));

    return TRUE;
  }

  return FALSE;
}

static gboolean
tab_view_close_page_cb (AdwTabView *tab_view,
                        AdwTabPage *page,
                        EphyWindow *window)
{
  EphyEmbed *embed = EPHY_EMBED (adw_tab_page_get_child (page));

  if (ephy_tab_view_get_n_pages (window->tab_view) == 1) {
    gboolean fullscreen_lockdown;

    fullscreen_lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN) ||
                          ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_KIOSK;

    if (fullscreen_lockdown || g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                                                       EPHY_PREFS_LOCKDOWN_QUIT)) {
      adw_tab_view_close_page_finish (tab_view, page, FALSE);
      return GDK_EVENT_STOP;
    }

    if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
      /* Never prompt the user before closing in automation mode */
      ephy_window_close_tab (window, embed);
    }

    /* Last window, check ongoing downloads before closing the tab */
    if (ephy_shell_get_n_windows (ephy_shell_get_default ()) == 1) {
      if (check_and_run_downloads_in_background (window)) {
        adw_tab_view_close_page_finish (tab_view, page, FALSE);
        return GDK_EVENT_STOP;
      }
    }
  }

  if (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                              EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA)) {
    TabHasModifiedFormsData *data;

    /* The modified forms check runs in the web process, which is problematic
     * because we don't want our codepath for closing a web view to depend on
     * executing web process code, in case the web process has hung. (This can
     * also be caused by a network process hang!) We'll assume the process has
     * been hung if there's no response after one second.
     */
    data = tab_has_modified_forms_data_new (window, embed, page);
    ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
                                      NULL,
                                      (GAsyncReadyCallback)tab_has_modified_forms_cb,
                                      data);
    return GDK_EVENT_STOP;
  } else {
    ephy_window_close_tab (window, embed);
  }

  return GDK_EVENT_PROPAGATE;
}

static AdwTabView *
tab_view_create_window_cb (AdwTabView *tab_view,
                           EphyWindow *window)
{
  EphyWindow *new_window;

  new_window = ephy_window_new ();

  new_window->present_on_insert = TRUE;

  return ephy_tab_view_get_tab_view (new_window->tab_view);
}

void
ephy_window_update_entry_focus (EphyWindow  *window,
                                EphyWebView *view)
{
  GtkWidget *title_widget;
  const char *address = NULL;

  address = ephy_web_view_get_address (view);
  if (!ephy_embed_utils_is_no_show_address (address) &&
      !ephy_web_view_is_newtab (view) &&
      !ephy_web_view_is_overview (view))
    return;

  title_widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar)));

  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    ephy_location_entry_grab_focus_without_selecting (EPHY_LOCATION_ENTRY (title_widget));
}

static void
tab_view_notify_selected_page_cb (EphyWindow *window)
{
  EphyEmbed *embed;
  EphyWebView *view;
  int page_num;

  if (window->closing)
    return;

  page_num = ephy_tab_view_get_selected_index (window->tab_view);

  if (page_num < 0)
    return;

  LOG ("switch-page tab view %p position %d\n", window->tab_view, page_num);

  /* get the new tab */

  embed = EPHY_EMBED (ephy_tab_view_get_nth_page (window->tab_view, page_num));
  view = ephy_embed_get_web_view (embed);

  /* update new tab */
  ephy_window_set_active_tab (window, embed);

  update_reader_mode (window, view, FALSE);
}

static void
tab_view_notify_n_pages_cb (EphyWindow *window)
{
  int n_pages = ephy_tab_view_get_n_pages (window->tab_view);
  GActionGroup *action_group;
  GAction *action;

  action_group = ephy_window_get_action_group (window, "tab");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "close");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), n_pages > 0);
}

static EphyTabView *
setup_tab_view (EphyWindow *window)
{
  EphyTabView *tab_view = ephy_tab_view_new ();
  AdwTabView *view = ephy_tab_view_get_tab_view (tab_view);
  g_autoptr (GtkBuilder) builder = NULL;

  gtk_widget_set_vexpand (GTK_WIDGET (tab_view), TRUE);

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/notebook-context-menu.ui");

  adw_tab_view_set_menu_model (view, G_MENU_MODEL (gtk_builder_get_object (builder, "notebook-menu")));

  g_signal_connect_object (view, "notify::selected-page",
                           G_CALLBACK (tab_view_notify_selected_page_cb),
                           window,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::n-pages",
                           G_CALLBACK (tab_view_notify_n_pages_cb),
                           window,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  g_signal_connect_object (view, "create-window",
                           G_CALLBACK (tab_view_create_window_cb),
                           window,
                           0);
  g_signal_connect_object (view, "setup-menu",
                           G_CALLBACK (tab_view_setup_menu_cb),
                           window,
                           0);
  g_signal_connect_object (view, "close-page",
                           G_CALLBACK (tab_view_close_page_cb),
                           window,
                           0);

  g_signal_connect_object (view, "page-attached",
                           G_CALLBACK (tab_view_page_attached_cb),
                           window,
                           0);
  g_signal_connect_object (view, "page-detached",
                           G_CALLBACK (tab_view_page_detached_cb),
                           window,
                           0);

  return tab_view;
}

static const char *supported_mime_types[] = {
  "x-scheme-handler/http",
  "x-scheme-handler/https",
  "text/html",
  "application/xhtml+xml",
  NULL
};

static void
set_as_default_browser (void)
{
  g_autoptr (GError) error = NULL;
  GDesktopAppInfo *desktop_info;
  GAppInfo *info = NULL;
  g_autofree char *id = g_strconcat (APPLICATION_ID, ".desktop", NULL);
  int i;

  desktop_info = g_desktop_app_info_new (id);
  if (!desktop_info)
    return;

  info = G_APP_INFO (desktop_info);

  for (i = 0; supported_mime_types[i]; i++) {
    if (!g_app_info_set_as_default_for_type (info, supported_mime_types[i], &error)) {
      g_warning ("Failed to set '%s' as the default application for secondary content type '%s': %s",
                 g_app_info_get_name (info), supported_mime_types[i], error->message);
    } else {
      LOG ("Set '%s' as the default application for '%s'",
           g_app_info_get_name (info),
           supported_mime_types[i]);
    }
  }
}

static void
ignore_default_browser (void)
{
  g_settings_set_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_ASK_FOR_DEFAULT, FALSE);
}

typedef struct {
  AdwDialog *dialog;
  GtkWidget *window;
} SetAsDefaultBrowserDialogData;

static SetAsDefaultBrowserDialogData *
set_as_default_browser_dialog_data_new (AdwDialog *dialog,
                                        GtkWidget *window)
{
  SetAsDefaultBrowserDialogData *data = g_new (SetAsDefaultBrowserDialogData, 1);
  data->dialog = dialog;
  data->window = window;
  return data;
}

static void
set_as_default_browser_dialog_data_free (SetAsDefaultBrowserDialogData *data)
{
  g_free (data);
}

static void
show_default_browser_dialog (SetAsDefaultBrowserDialogData *data)
{
  adw_dialog_present (data->dialog, data->window);
  set_as_default_browser_dialog_data_free (data);
}

static void
add_default_browser_question (EphyWindow *window)
{
  AdwDialog *dialog;
  SetAsDefaultBrowserDialogData *data;

  dialog = adw_alert_dialog_new (NULL, NULL);

  adw_alert_dialog_set_heading (ADW_ALERT_DIALOG (dialog), _("Set as Default Browser?"));
#if !TECH_PREVIEW
  adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), _("Use Web as your default web browser and for opening external links"));
#else
  adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), _("Use Epiphany Technology Preview as your default web browser and for opening external links"));
#endif

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "close", _("_Ask Again Later"),
                                  "no", _("_No"),
                                  "yes", _("_Yes"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "no", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "yes", ADW_RESPONSE_SUGGESTED);

  g_signal_connect (dialog, "response::yes", G_CALLBACK (set_as_default_browser), NULL);
  g_signal_connect (dialog, "response::no", G_CALLBACK (ignore_default_browser), NULL);

  /* Ensure the main window is already mapped as otherwise the dialog
   * might show below the main window */
  data = set_as_default_browser_dialog_data_new (dialog, GTK_WIDGET (window));
  g_idle_add_once ((GSourceOnceFunc)show_default_browser_dialog, g_steal_pointer (&data));
}

static gboolean
is_browser_default (void)
{
  g_autoptr (GAppInfo) info = g_app_info_get_default_for_type (supported_mime_types[0], TRUE);

  if (info) {
    g_autofree gchar *id = g_strconcat (APPLICATION_ID, ".desktop", NULL);

    if (!strcmp (g_app_info_get_id (info), id))
      return TRUE;
  }

  return FALSE;
}

static void
ephy_window_dispose (GObject *object)
{
  EphyWindow *window = EPHY_WINDOW (object);

  LOG ("EphyWindow dispose %p", window);

  /* Only do these once */
  if (!window->closing) {
    window->closing = TRUE;

    g_cancellable_cancel (window->cancellable);
    g_clear_object (&window->cancellable);

    _ephy_window_set_context_event (window, NULL);

    g_clear_object (&window->hit_test_result);
    g_clear_object (&window->mouse_gesture_controller);

    g_clear_handle_id (&window->modified_forms_timeout_id, g_source_remove);

    g_clear_pointer (&window->action_labels, g_hash_table_unref);
    g_clear_pointer (&window->action_groups, g_hash_table_unref);
  }

  G_OBJECT_CLASS (ephy_window_parent_class)->dispose (object);
}

static void
ephy_window_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  EphyWindow *window = EPHY_WINDOW (object);

  switch (prop_id) {
    case PROP_ACTIVE_CHILD:
      impl_set_active_child (EPHY_EMBED_CONTAINER (window),
                             g_value_get_object (value));
      break;
    case PROP_CHROME:
      ephy_window_set_chrome (window, g_value_get_flags (value));
      break;
    case PROP_SINGLE_TAB_MODE:
      ephy_window_set_is_popup (window, g_value_get_boolean (value));
      break;
    case PROP_ADAPTIVE_MODE:
      ephy_window_set_adaptive_mode (window, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_window_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  EphyWindow *window = EPHY_WINDOW (object);

  switch (prop_id) {
    case PROP_ACTIVE_CHILD:
      g_value_set_object (value, window->active_embed);
      break;
    case PROP_CHROME:
      g_value_set_flags (value, window->chrome);
      break;
    case PROP_SINGLE_TAB_MODE:
      g_value_set_boolean (value, window->is_popup);
      break;
    case PROP_ADAPTIVE_MODE:
      g_value_set_enum (value, window->adaptive_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
ephy_window_set_default_size (EphyWindow *window,
                              gint        width,
                              gint        height)
{
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);
  window->has_default_size = TRUE;
}

static void
ephy_window_show (GtkWidget *widget)
{
  EphyWindow *window = EPHY_WINDOW (widget);

  if (window->is_popup) {
    GTK_WIDGET_CLASS (ephy_window_parent_class)->show (widget);
    return;
  }

  window->is_maximized = g_settings_get_boolean (EPHY_SETTINGS_STATE, "is-maximized");
  if (window->is_maximized)
    gtk_window_maximize (GTK_WINDOW (window));
  else {
    if (!window->has_default_size) {
      g_settings_get (EPHY_SETTINGS_STATE,
                      EPHY_PREFS_STATE_WINDOW_SIZE, "(ii)",
                      &window->current_width,
                      &window->current_height);

      if (window->current_width > 0 && window->current_height > 0) {
        gtk_window_set_default_size (GTK_WINDOW (window),
                                     window->current_width,
                                     window->current_height);
      }

      window->has_default_size = TRUE;
    }
  }

  GTK_WIDGET_CLASS (ephy_window_parent_class)->show (widget);

  /* Check for default browser */
  if (g_settings_get_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_ASK_FOR_DEFAULT) &&
      !is_browser_default () &&
      !ephy_profile_dir_is_web_application ())
    add_default_browser_question (window);
}

static void
compute_size_cb (EphyWindow      *window,
                 GdkToplevelSize *size)
{
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
  GdkToplevelState state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));

  window->is_maximized = gtk_window_is_maximized (GTK_WINDOW (window));

  if (state & (GDK_TOPLEVEL_STATE_FULLSCREEN |
               GDK_TOPLEVEL_STATE_MAXIMIZED |
               GDK_TOPLEVEL_STATE_TILED |
               GDK_TOPLEVEL_STATE_TOP_TILED |
               GDK_TOPLEVEL_STATE_RIGHT_TILED |
               GDK_TOPLEVEL_STATE_BOTTOM_TILED |
               GDK_TOPLEVEL_STATE_LEFT_TILED |
               GDK_TOPLEVEL_STATE_MINIMIZED)) {
    window->current_width = gdk_surface_get_width (surface);
    window->current_height = gdk_surface_get_height (surface);
  } else {
    gtk_window_get_default_size (GTK_WINDOW (window),
                                 &window->current_width,
                                 &window->current_height);
  }
}

static void
ephy_window_realize (GtkWidget *widget)
{
  EphyWindow *window = EPHY_WINDOW (widget);
  GdkSurface *surface;

  GTK_WIDGET_CLASS (ephy_window_parent_class)->realize (widget);

  surface = gtk_native_get_surface (GTK_NATIVE (window));

  g_signal_connect_swapped (surface, "compute-size", G_CALLBACK (compute_size_cb), window);
}

static void
ephy_window_unrealize (GtkWidget *widget)
{
  EphyWindow *window = EPHY_WINDOW (widget);
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));

  g_signal_handlers_disconnect_by_func (surface, G_CALLBACK (compute_size_cb), window);

  GTK_WIDGET_CLASS (ephy_window_parent_class)->unrealize (widget);
}

static gboolean
should_save_window_state (EphyWindow *window)
{
  if (window->is_popup)
    return FALSE;

  if (ephy_profile_dir_is_default () ||
      ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    /* The goal here is to save the window size in GSettings if it is the final
     * window. At this point, it's already being destroyed and isn't included in
     * the count of active windows. */
    return ephy_shell_get_n_windows (ephy_shell_get_default ()) == 0;
  }

  return FALSE;
}

static void
ephy_window_finalize (GObject *object)
{
  EphyWindow *window = EPHY_WINDOW (object);
  EphyShell *shell = ephy_shell_get_default ();

  if (should_save_window_state (window)) {
    g_settings_set (EPHY_SETTINGS_STATE,
                    EPHY_PREFS_STATE_WINDOW_SIZE, "(ii)",
                    window->current_width,
                    window->current_height);
    g_settings_set_boolean (EPHY_SETTINGS_STATE, "is-maximized", window->is_maximized);
  }

  G_OBJECT_CLASS (ephy_window_parent_class)->finalize (object);

  ephy_shell_unregister_window (shell, window);

  LOG ("EphyWindow finalized %p", object);
}

static void
sync_user_input_cb (EphyLocationController *action,
                    GParamSpec             *pspec,
                    EphyWindow             *window)
{
  const char *address;

  LOG ("sync_user_input_cb");

  if (window->updating_address)
    return;

  address = ephy_location_controller_get_address (action);

  window->updating_address = TRUE;
  g_assert (EPHY_IS_EMBED (window->active_embed));
  ephy_web_view_set_typed_address (ephy_embed_get_web_view (window->active_embed), address);
  window->updating_address = FALSE;
}

static GtkWidget *
setup_header_bar (EphyWindow *window)
{
  GtkWidget *header_bar;

  header_bar = ephy_header_bar_new (window);

  return header_bar;
}

static EphyLocationController *
setup_location_controller (EphyWindow    *window,
                           EphyHeaderBar *header_bar)
{
  EphyLocationController *location_controller;

  location_controller =
    g_object_new (EPHY_TYPE_LOCATION_CONTROLLER,
                  "window", window,
                  "title-widget", ephy_header_bar_get_title_widget (header_bar),
                  NULL);
  g_signal_connect (location_controller, "notify::address",
                    G_CALLBACK (sync_user_input_cb), window);
  g_signal_connect_swapped (location_controller, "open-link",
                            G_CALLBACK (ephy_link_open), window);

  return location_controller;
}

static const char *disabled_actions_for_app_mode[] = {
  "open",
  "save-as-application",
  "encoding",
  "bookmark-page",
  "new-tab",
  "home",
  "location-search",
  "tabs-view",
  "open-application-manager"
};

static const char *disabled_actions_for_incognito_mode[] = {
  "privacy-report"
};

static gboolean
browse_with_caret_get_mapping (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  g_value_set_variant (value, variant);

  return TRUE;
}

static void
download_start_toast_dismissed_cb (EphyWindow *window)
{
  window->download_start_toast = NULL;
}

static void
download_added_cb (EphyWindow *window)
{
  window->download_start_toast = adw_toast_new (_("Download started"));
  g_signal_connect_object (window->download_start_toast, "dismissed",
                           G_CALLBACK (download_start_toast_dismissed_cb), window, G_CONNECT_SWAPPED);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (window->toast_overlay), window->download_start_toast);
}

static void
download_completed_cb (EphyDownload *download,
                       gpointer      user_data)
{
  EphyShell *shell = ephy_shell_get_default ();
  EphyWindow *window;
  AdwToast *toast = adw_toast_new (_("Download finished"));

  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (shell)));
  if (window->download_start_toast)
    adw_toast_dismiss (window->download_start_toast);

  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (window->toast_overlay), toast);

  if (ephy_shell_get_n_windows (shell) != 1)
    return;

  if (gtk_widget_is_visible (GTK_WIDGET (window)))
    return;

  if (ephy_shell_close_all_windows (shell))
    g_application_quit (G_APPLICATION (shell));
}

static void
on_username_entry_changed (GtkEditable             *entry,
                           EphyPasswordRequestData *request_data)
{
  const char *text = gtk_editable_get_text (entry);
  g_free (request_data->username);
  request_data->username = g_strdup (text);
}

static void
on_password_entry_changed (GtkEditable             *entry,
                           EphyPasswordRequestData *request_data)
{
  const char *text = gtk_editable_get_text (entry);
  g_free (request_data->password);
  request_data->password = g_strdup (text);
}

static void
on_password_never (AdwAlertDialog          *dialog,
                   const char              *response,
                   EphyPasswordRequestData *request_data)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (ephy_embed_shell_get_default ());
  EphyPermissionsManager *permissions_manager = ephy_embed_shell_get_permissions_manager (shell);

  ephy_permissions_manager_set_permission (permissions_manager,
                                           EPHY_PERMISSION_TYPE_SAVE_PASSWORD,
                                           request_data->origin,
                                           EPHY_PERMISSION_DENY);
}

static void
on_password_save (AdwAlertDialog          *dialog,
                  const char              *response,
                  EphyPasswordRequestData *request_data)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (ephy_embed_shell_get_default ());
  EphyPasswordManager *password_manager = ephy_embed_shell_get_password_manager (shell);

  ephy_password_manager_save (password_manager, request_data->origin,
                              request_data->target_origin, request_data->username,
                              request_data->username, request_data->password, request_data->usernameField,
                              request_data->passwordField, request_data->isNew);
}

static void
save_password_cb (EphyEmbedShell          *shell,
                  EphyPasswordRequestData *request_data)
{
  GtkWindow *window;
  AdwDialog *dialog;
  AdwPreferencesGroup *prefs_group;
  GtkWidget *password_entry;

  /* Sanity checks would have already occurred before this point */
  window = gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ()));
  if (!gtk_widget_is_visible (GTK_WIDGET (window)))
    return;

  dialog = adw_alert_dialog_new (_("Save Password?"),
                                 _("Passwords are saved only on your device and can be removed at any time in Preferences"));
  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "close", _("Not Now"),
                                  "never", _("Never Save"),
                                  "save", _("Save"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "never", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "save", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "close");

  prefs_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), GTK_WIDGET (prefs_group));

  if (request_data->username) {
    GtkWidget *username_entry = adw_entry_row_new ();

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (username_entry), _("Username"));
    gtk_editable_set_text (GTK_EDITABLE (username_entry), request_data->username);
    adw_preferences_group_add (prefs_group, username_entry);
    g_signal_connect (GTK_EDITABLE (username_entry), "changed", G_CALLBACK (on_username_entry_changed), request_data);
  }

  password_entry = adw_password_entry_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (password_entry), _("Password"));
  gtk_editable_set_text (GTK_EDITABLE (password_entry), request_data->password);
  adw_preferences_group_add (prefs_group, password_entry);
  g_signal_connect (GTK_EDITABLE (password_entry), "changed", G_CALLBACK (on_password_entry_changed), request_data);

  g_signal_connect (dialog, "response::save", G_CALLBACK (on_password_save), request_data);
  g_signal_connect (dialog, "response::never", G_CALLBACK (on_password_never), request_data);

  adw_dialog_present (dialog, GTK_WIDGET (window));
}

static const char *disabled_win_actions_for_overview[] = {
  "bookmarks",
  "browse-with-caret",
  "location",
  "location-search",
  "select-all",
  "toggle-inspector",
  "toggle-reader-mode",
};

static const char *disabled_toolbar_actions_for_overview[] = {
  "reload-bypass-cache",
  "navigation-back",
  "navigation-forward",
};

static void
notify_overview_open_cb (EphyWindow *window)
{
  GActionGroup *action_group;
  GAction *action;
  gboolean overview_open;
  guint i;

  overview_open = adw_tab_overview_get_open (ADW_TAB_OVERVIEW (window->overview));
  action_group = ephy_window_get_action_group (window, "win");

  for (i = 0; i < G_N_ELEMENTS (disabled_win_actions_for_overview); i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         disabled_win_actions_for_overview[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_IS_OVERVIEW, overview_open);
  }

  action_group = ephy_window_get_action_group (window, "toolbar");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "reload");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !overview_open);

  for (i = 0; i < G_N_ELEMENTS (disabled_toolbar_actions_for_overview); i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         disabled_toolbar_actions_for_overview[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_IS_OVERVIEW, overview_open);
  }

  _ephy_window_set_default_actions_sensitive (window, SENS_FLAG_IS_OVERVIEW, overview_open);
}

static const char *disabled_win_actions_for_sidebar[] = {
  "home",
  "new-tab",
  "location",
  "location-search",
  "tabs-view",
  "show-downloads",
  "zoom-in",
  "zoom-out",
  "zoom-normal",
  "toggle-inspector",
  "toggle-reader-mode",
};

static const char *disabled_toolbar_actions_for_sidebar[] = {
  "stop",
  "reload",
  "reload-bypass-cache",
  "navigation-back",
  "navigation-forward",
};

static const char *disabled_tab_actions_for_sidebar[] = {
  "close",
  "duplicate",
};

static void
show_sidebar_cb (EphyWindow *window)
{
  GActionGroup *action_group;
  GAction *action;
  guint i;
  gboolean shown = adw_overlay_split_view_get_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view));
  AdwTabView *tab_view = ephy_tab_view_get_tab_view (window->tab_view);

  action_group = ephy_window_get_action_group (window, "win");
  for (i = 0; i < G_N_ELEMENTS (disabled_win_actions_for_sidebar); i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         disabled_win_actions_for_sidebar[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_IS_SIDEBAR, shown);
  }

  action_group = ephy_window_get_action_group (window, "toolbar");
  for (i = 0; i < G_N_ELEMENTS (disabled_toolbar_actions_for_sidebar); i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         disabled_toolbar_actions_for_sidebar[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_IS_SIDEBAR, shown);
  }

  action_group = ephy_window_get_action_group (window, "tab");
  for (i = 0; i < G_N_ELEMENTS (disabled_tab_actions_for_sidebar); i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         disabled_tab_actions_for_sidebar[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_IS_SIDEBAR, shown);
  }

  if (shown) {
    adw_tab_view_set_shortcuts (tab_view, ADW_TAB_VIEW_SHORTCUT_NONE);
  } else {
    adw_tab_view_set_shortcuts (tab_view, ADW_TAB_VIEW_SHORTCUT_ALL_SHORTCUTS);
    adw_tab_view_remove_shortcuts (tab_view,
                                   ADW_TAB_VIEW_SHORTCUT_CONTROL_HOME | ADW_TAB_VIEW_SHORTCUT_CONTROL_END |
                                   ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_HOME | ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_END);
  }

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_SIDEBAR,
                                              shown);
}

static AdwTabPage *
create_tab_cb (EphyWindow *window)
{
  AdwTabView *view = ephy_tab_view_get_tab_view (window->tab_view);

  window_cmd_new_tab (NULL, NULL, window);

  return adw_tab_view_get_selected_page (view);
}

static void
insert_action_group (const char   *prefix,
                     GActionGroup *group,
                     GtkWidget    *widget)
{
  gtk_widget_insert_action_group (widget, prefix, group);
}

static gboolean
scroll_cb (EphyWindow *self,
           double      dx,
           double      dy)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->action_bar_revealer), dy < 0);

  return FALSE;
}

static void
pan_cb (EphyWindow      *self,
        GtkPanDirection  direction,
        gdouble          offset,
        gpointer         user_data)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->action_bar_revealer), direction == GTK_PAN_DIRECTION_DOWN);
}

static void
shortcuts_action_cb (EphyWindow *window)
{
  AdwDialog *dialog = adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window));

  g_assert (ADW_IS_SHORTCUTS_DIALOG (dialog));

  if (ephy_can_install_web_apps ()) {
    AdwShortcutsSection *section = adw_shortcuts_section_new (C_("shortcuts dialog", "Web App"));
    AdwShortcutsItem *item;

    item = adw_shortcuts_item_new (C_("shortcuts dialog", "Install Site as Web App"), "<Shift><Primary>A");
    adw_shortcuts_section_add (section, item);
    adw_shortcuts_dialog_add (ADW_SHORTCUTS_DIALOG (dialog), section);
  }
}

static void
ephy_window_constructed (GObject *object)
{
  EphyWindow *window;
  GAction *action;
  GActionGroup *action_group;
  GSimpleActionGroup *simple_action_group;
  guint i;
  EphyShell *shell;
  EphyEmbedShellMode mode;
  EphyWindowChrome chrome = EPHY_WINDOW_CHROME_DEFAULT;
  GApplication *app;
  AdwBreakpoint *breakpoint;
  EphyDownloadsManager *downloads_manager;
  g_autoptr (GtkBuilder) builder = NULL;
  GtkEventController *scroll_controller;
  GtkGesture *pan_gesture;

#if 0
  /* Disabled due to https://gitlab.gnome.org/GNOME/epiphany/-/issues/1915 */
  GtkEventController *controller;
#endif

  G_OBJECT_CLASS (ephy_window_parent_class)->constructed (object);

  window = EPHY_WINDOW (object);

  window->action_groups = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 NULL);

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   window);
  g_hash_table_insert (window->action_groups, g_strdup ("win"), simple_action_group);

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   tab_entries,
                                   G_N_ELEMENTS (tab_entries),
                                   window);
  g_hash_table_insert (window->action_groups, g_strdup ("tab"), simple_action_group);

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   toolbar_entries,
                                   G_N_ELEMENTS (toolbar_entries),
                                   window);
  g_hash_table_insert (window->action_groups, g_strdup ("toolbar"), simple_action_group);

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   popup_entries,
                                   G_N_ELEMENTS (popup_entries),
                                   window);
  g_hash_table_insert (window->action_groups, g_strdup ("popup"), simple_action_group);

  g_hash_table_foreach (window->action_groups,
                        (GHFunc)insert_action_group, window);

  window->action_labels = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 (GDestroyNotify)g_free,
                                                 (GDestroyNotify)g_free);
  for (i = 0; i < G_N_ELEMENTS (action_label); i++) {
    g_hash_table_insert (window->action_labels,
                         g_strdup (action_label[i].action),
                         g_strdup (action_label[i].label));
  }

  /* Set accels for actions */
  app = g_application_get_default ();
  for (i = 0; i < G_N_ELEMENTS (accels); i++) {
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           accels[i].action_and_target,
                                           accels[i].accelerators);
  }

  accels_navigation_ltr_rtl = gtk_widget_get_default_direction () == GTK_TEXT_DIR_LTR ?
                              accels_navigation_ltr : accels_navigation_rtl;

  for (i = 0; i < G_N_ELEMENTS (accels_navigation_ltr); i++) {
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           accels_navigation_ltr_rtl[i].action_and_target,
                                           accels_navigation_ltr_rtl[i].accelerators);
  }

  g_signal_connect (window, "notify::fullscreened",
                    G_CALLBACK (notify_fullscreen_cb), NULL);

  window->tab_view = setup_tab_view (window);
  window->tab_bar = adw_tab_bar_new ();
  window->overview = adw_tab_overview_new ();
  window->fullscreen_box = ephy_fullscreen_box_new ();

  pan_gesture = gtk_gesture_pan_new (GTK_ORIENTATION_VERTICAL);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (pan_gesture), GTK_PHASE_CAPTURE);
  g_signal_connect_object (pan_gesture, "pan", G_CALLBACK (pan_cb), window, G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (window), GTK_EVENT_CONTROLLER (pan_gesture));

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/tab-overview-menu.ui");

  adw_tab_overview_set_enable_new_tab (ADW_TAB_OVERVIEW (window->overview), TRUE);
  adw_tab_overview_set_secondary_menu (ADW_TAB_OVERVIEW (window->overview),
                                       G_MENU_MODEL (gtk_builder_get_object (builder, "overview-menu")));
  g_signal_connect_swapped (window->overview, "notify::open",
                            G_CALLBACK (notify_overview_open_cb), window);
  g_signal_connect_swapped (window->overview, "create-tab",
                            G_CALLBACK (create_tab_cb), window);

  adw_tab_bar_set_view (window->tab_bar, ephy_tab_view_get_tab_view (window->tab_view));
  adw_tab_overview_set_view (ADW_TAB_OVERVIEW (window->overview), ephy_tab_view_get_tab_view (window->tab_view));

  shell = ephy_shell_get_default ();
  mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (shell));

  /* Setup incognito mode style */
  if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    gtk_widget_add_css_class (GTK_WIDGET (window), "incognito-mode");
  else if (mode == EPHY_EMBED_SHELL_MODE_AUTOMATION)
    gtk_widget_add_css_class (GTK_WIDGET (window), "automation-mode");

  /* Setup the toolbar. */
  window->header_bar = setup_header_bar (window);
  window->location_controller = setup_location_controller (window, EPHY_HEADER_BAR (window->header_bar));
  window->action_bar_revealer = gtk_revealer_new ();
  gtk_widget_set_visible (window->action_bar_revealer, FALSE);
  window->action_bar = GTK_WIDGET (ephy_action_bar_new (window));
  gtk_revealer_set_child (GTK_REVEALER (window->action_bar_revealer), window->action_bar);
  gtk_revealer_set_reveal_child (GTK_REVEALER (window->action_bar_revealer), FALSE);
  window->toast_overlay = adw_toast_overlay_new ();
  adw_toast_overlay_set_child (ADW_TOAST_OVERLAY (window->toast_overlay), GTK_WIDGET (window->tab_view));

  /* Add scroll container to hide action bar during scrolling */
  scroll_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_propagation_phase (scroll_controller, GTK_PHASE_CAPTURE);
  g_signal_connect_object (scroll_controller, "scroll", G_CALLBACK (scroll_cb), window, G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (window->tab_view), scroll_controller);

  ephy_fullscreen_box_set_content (window->fullscreen_box, GTK_WIDGET (window->toast_overlay));
  window->header_bin_top = adw_bin_new ();
  adw_bin_set_child (ADW_BIN (window->header_bin_top), window->header_bar);
  ephy_fullscreen_box_add_top_bar (window->fullscreen_box, GTK_WIDGET (window->header_bin_top));
  window->header_bin_bottom = adw_bin_new ();
  ephy_fullscreen_box_add_bottom_bar (window->fullscreen_box, GTK_WIDGET (window->header_bin_bottom));

  ephy_fullscreen_box_add_top_bar (window->fullscreen_box, GTK_WIDGET (window->tab_bar));
  ephy_fullscreen_box_add_bottom_bar (window->fullscreen_box, window->action_bar_revealer);

  adw_tab_overview_set_child (ADW_TAB_OVERVIEW (window->overview),
                              GTK_WIDGET (window->fullscreen_box));

  ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  window->bookmarks_dialog = ephy_bookmarks_dialog_new ();

  /* Overlay Split View */
  window->overlay_split_view = adw_overlay_split_view_new ();
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (window), GTK_WIDGET (window->overlay_split_view));

  adw_overlay_split_view_set_max_sidebar_width (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view), 360);
  adw_overlay_split_view_set_collapsed (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view), TRUE);
  adw_overlay_split_view_set_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view), FALSE);
  adw_overlay_split_view_set_sidebar_position (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view), GTK_PACK_END);

  adw_overlay_split_view_set_content (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view), GTK_WIDGET (window->overview));
  adw_overlay_split_view_set_sidebar (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view), window->bookmarks_dialog);
  g_signal_connect_object (window->overlay_split_view, "notify::show-sidebar",
                           G_CALLBACK (show_sidebar_cb), window, G_CONNECT_SWAPPED);

  ephy_tab_view_set_tab_bar (window->tab_view, window->tab_bar);
  ephy_tab_view_set_tab_overview (window->tab_view, ADW_TAB_OVERVIEW (window->overview));

  /* other notifiers */
  action = g_action_map_lookup_action (G_ACTION_MAP (shell),
                                       "shortcuts");
  g_signal_connect_swapped (action, "activate", G_CALLBACK (shortcuts_action_cb), window);

  action_group = ephy_window_get_action_group (window, "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "browse-with-caret");

  g_settings_bind_with_mapping (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_ENABLE_CARET_BROWSING,
                                G_SIMPLE_ACTION (action), "state",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_GET_NO_CHANGES,
                                browse_with_caret_get_mapping,
                                NULL,
                                action, NULL);

  action_group = ephy_window_get_action_group (window, "win");

  /* Disable actions not needed for popup mode. */
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        SENS_FLAG_CHROME,
                                        window->is_popup);

  action_group = ephy_window_get_action_group (window, "popup");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "open-link-in-new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        SENS_FLAG_CHROME,
                                        window->is_popup);

  if (mode == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    g_object_set (window->location_controller, "editable", FALSE, NULL);

    action_group = ephy_window_get_action_group (window, "popup");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "context-bookmark-page");
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_CHROME, TRUE);

    action_group = ephy_window_get_action_group (window, "win");
    for (i = 0; i < G_N_ELEMENTS (disabled_actions_for_app_mode); i++) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           disabled_actions_for_app_mode[i]);
      ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                            SENS_FLAG_CHROME, TRUE);
    }
    chrome &= ~(EPHY_WINDOW_CHROME_LOCATION | EPHY_WINDOW_CHROME_TABSBAR | EPHY_WINDOW_CHROME_BOOKMARKS);
  } else if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    action_group = ephy_window_get_action_group (window, "win");

    for (i = 0; i < G_N_ELEMENTS (disabled_actions_for_incognito_mode); i++) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           disabled_actions_for_incognito_mode[i]);
      ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                            SENS_FLAG_CHROME, TRUE);
    }
  } else if (mode == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    g_object_set (window->location_controller, "editable", FALSE, NULL);
  }

  window->mouse_gesture_controller = ephy_mouse_gesture_controller_new (window);

  ephy_window_set_chrome (window, chrome);

  ephy_web_extension_manager_install_actions (ephy_web_extension_manager_get_default (), window);

#if 0
  /* Disabled due to https://gitlab.gnome.org/GNOME/epiphany/-/issues/1915 */
  controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (controller, "key-pressed", G_CALLBACK (handle_key_cb), window);
  g_signal_connect_swapped (controller, "key-released", G_CALLBACK (handle_key_cb), window);
  gtk_widget_add_controller (GTK_WIDGET (window), controller);
#endif

  downloads_manager = ephy_embed_shell_get_downloads_manager (EPHY_EMBED_SHELL (shell));
  g_signal_connect_object (downloads_manager, "download-added", G_CALLBACK (download_added_cb), window, G_CONNECT_SWAPPED);

  gtk_widget_set_size_request (GTK_WIDGET (window), 360, 200);

  breakpoint = adw_breakpoint_new (adw_breakpoint_condition_parse ("max-width: 600px"));
  adw_breakpoint_add_setters (breakpoint,
                              G_OBJECT (window), "adaptive-mode", EPHY_ADAPTIVE_MODE_NARROW,
                              NULL);

  adw_application_window_add_breakpoint (ADW_APPLICATION_WINDOW (window), breakpoint);
}

static void
ephy_window_class_init (EphyWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);
  EphyShell *shell;
  EphyDownloadsManager *manager;

  object_class->constructed = ephy_window_constructed;
  object_class->dispose = ephy_window_dispose;
  object_class->finalize = ephy_window_finalize;
  object_class->get_property = ephy_window_get_property;
  object_class->set_property = ephy_window_set_property;

  widget_class->show = ephy_window_show;
  widget_class->realize = ephy_window_realize;
  widget_class->unrealize = ephy_window_unrealize;

  window_class->close_request = ephy_window_close_request;

  g_object_class_override_property (object_class,
                                    PROP_ACTIVE_CHILD,
                                    "active-child");

  g_object_class_override_property (object_class,
                                    PROP_SINGLE_TAB_MODE,
                                    "is-popup");

  g_object_class_install_property (object_class,
                                   PROP_CHROME,
                                   g_param_spec_flags ("chrome",
                                                       NULL,
                                                       NULL,
                                                       EPHY_TYPE_WINDOW_CHROME,
                                                       EPHY_WINDOW_CHROME_DEFAULT,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_ADAPTIVE_MODE,
                                   g_param_spec_enum ("adaptive-mode",
                                                      NULL,
                                                      NULL,
                                                      EPHY_TYPE_ADAPTIVE_MODE,
                                                      EPHY_ADAPTIVE_MODE_NORMAL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  shell = ephy_shell_get_default ();
  manager = ephy_embed_shell_get_downloads_manager (EPHY_EMBED_SHELL (shell));
  g_signal_connect (manager, "download-completed", G_CALLBACK (download_completed_cb), NULL);
  g_signal_connect (shell, "password-form-submitted", G_CALLBACK (save_password_cb), NULL);
}

static void
ephy_window_init (EphyWindow *window)
{
  EphyShell *shell = ephy_shell_get_default ();
  g_autoptr (GtkWindowGroup) window_group = gtk_window_group_new ();

  LOG ("EphyWindow initialising %p", window);

  window->uid = window_uid++;
  window->adaptive_mode = EPHY_ADAPTIVE_MODE_NORMAL;

  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  ephy_shell_register_window (shell, window);

#if TECH_PREVIEW
  gtk_widget_add_css_class (GTK_WIDGET (window), "devel");
#endif
}

/**
 * ephy_window_new:
 *
 * Equivalent to g_object_new() but returns an #EphyWindow so you don't have
 * to cast it.
 *
 * Return value: a new #EphyWindow
 **/
EphyWindow *
ephy_window_new (void)
{
  return g_object_new (EPHY_TYPE_WINDOW,
                       "application", GTK_APPLICATION (ephy_shell_get_default ()),
                       "default-height", 768,
                       "default-width", 1024,
                       "icon-name", APPLICATION_ID,
                       NULL);
}

/**
 * ephy_window_get_tab_view:
 * @window: an #EphyWindow
 *
 * Returns the #EphyTabView used by this window.
 *
 * Return value: (transfer none): the @window's #EphyTabView
 **/
EphyTabView *
ephy_window_get_tab_view (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return window->tab_view;
}

/**
 * ephy_window_get_active_embed:
 * @window: an #EphyWindow
 *
 * Returns the active #EphyEmbed for this window.
 *
 * Return value: (transfer none): the @window's active #EphyEmbed
 **/
EphyEmbed *
ephy_window_get_active_embed (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return window->active_embed;
}

/**
 * ephy_window_toggle_tab_overview
 * @window: an #EphyWindow
 *
 * Toggles the tab overview
 **/
void
ephy_window_toggle_tab_overview (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  if (!adw_tab_overview_get_open (ADW_TAB_OVERVIEW (window->overview)))
    adw_tab_overview_set_open (ADW_TAB_OVERVIEW (window->overview), TRUE);
  else
    adw_tab_overview_set_open (ADW_TAB_OVERVIEW (window->overview), FALSE);
}

/**
 * ephy_window_get_find_toolbar:
 * @window: an #EphyWindow
 *
 * Returns the #EphyFindToolbar used by this window.
 *
 * Return value: (transfer none): the @window's #EphyFindToolbar
 **/
GtkWidget *
ephy_window_get_current_find_toolbar (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return GTK_WIDGET (ephy_embed_get_find_toolbar (window->active_embed));
}

/**
 * ephy_window_load_url:
 * @window: a #EphyWindow
 * @url: the url to load
 *
 * Loads a new url in the active tab of @window.
 * Unlike ephy_web_view_load_url(), this function activates
 * the embed.
 *
 **/
void
ephy_window_load_url (EphyWindow *window,
                      const char *url)
{
  g_assert (url);

  ephy_link_open (EPHY_LINK (window), url, NULL, 0);
}

/**
 * ephy_window_focus_location_entry:
 * @window: an #EphyWindow
 *
 * Focus the location entry on @window's header bar.
 **/
void
ephy_window_focus_location_entry (EphyWindow *window)
{
  EphyTitleWidget *title_widget;

  if (!(window->chrome & EPHY_WINDOW_CHROME_LOCATION))
    return;

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    ephy_location_entry_grab_focus (EPHY_LOCATION_ENTRY (title_widget));
}

/**
 * ephy_window_location_search:
 * @window: an #EphyWindow
 *
 * Focuses the location entry on @window's header bar
 * and sets the text to the default search engine's bang.
 **/
void
ephy_window_location_search (EphyWindow *window)
{
  EphyTitleWidget *title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));
  EphyLocationEntry *location_entry = EPHY_LOCATION_ENTRY (title_widget);
  GtkApplication *gtk_application = gtk_window_get_application (GTK_WINDOW (window));
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (gtk_application);
  EphySearchEngineManager *search_engine_manager = ephy_embed_shell_get_search_engine_manager (embed_shell);
  EphySearchEngine *search_engine;
  const char *bang;
  g_autofree char *entry_text = NULL;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    search_engine = ephy_search_engine_manager_get_incognito_engine (search_engine_manager);
  else
    search_engine = ephy_search_engine_manager_get_default_engine (search_engine_manager);

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    ephy_location_entry_grab_focus_without_selecting (EPHY_LOCATION_ENTRY (title_widget));

  bang = ephy_search_engine_get_bang (search_engine);
  entry_text = g_strconcat (bang, " ", NULL);

  gtk_window_set_focus (GTK_WINDOW (window), GTK_WIDGET (location_entry));
  gtk_editable_set_text (GTK_EDITABLE (location_entry), entry_text);
  gtk_editable_set_position (GTK_EDITABLE (location_entry), strlen (entry_text));
}

/**
 * ephy_window_set_zoom:
 * @window: an #EphyWindow
 * @zoom: the desired zoom level
 *
 * Sets the zoom on @window's active #EphyEmbed. A @zoom of 1.0 corresponds to
 * 100% zoom (normal size).
 **/
void
ephy_window_set_zoom (EphyWindow *window,
                      double      zoom)
{
  EphyEmbed *embed;
  double current_zoom = 1.0;
  WebKitWebView *web_view;

  g_assert (EPHY_IS_WINDOW (window));

  embed = window->active_embed;

  if (!embed)
    return;

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  current_zoom = webkit_web_view_get_zoom_level (web_view);

  if (zoom == ZOOM_IN)
    zoom = ephy_zoom_get_changed_zoom_level (current_zoom, 1);
  else if (zoom == ZOOM_OUT)
    zoom = ephy_zoom_get_changed_zoom_level (current_zoom, -1);

  /* Use default zoom value if zoom is not set */
  if (!zoom)
    zoom = g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL);

  if (zoom != current_zoom)
    webkit_web_view_set_zoom_level (web_view, zoom);
}

/**
 * ephy_window_get_context_event:
 * @window: an #EphyWindow
 *
 * Returns the #EphyEmbedEvent for the current context menu.
 * Use this to get the event from the action callback.
 *
 * Return value: (transfer none): an #EphyEmbedEvent, or %NULL
 **/
WebKitHitTestResult *
ephy_window_get_context_event (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return window->context_event;
}

/**
 * ephy_window_get_location:
 * @window: an #EphyWindow widget
 *
 * Gets the current address according to @window's #EphyLocationController.
 *
 * Returns: current @window address
 **/
const char *
ephy_window_get_location (EphyWindow *window)
{
  return ephy_location_controller_get_address (window->location_controller);
}

/**
 * ephy_window_set_location:
 * @window: an #EphyWindow widget
 * @address: a decoded URI, suitable for display to the user
 *
 * Sets the internal #EphyLocationController address to @address.
 **/
void
ephy_window_set_location (EphyWindow *window,
                          const char *address)
{
  if (window->updating_address)
    return;

  window->updating_address = TRUE;
  ephy_location_controller_set_address (window->location_controller, address);
  window->updating_address = FALSE;
}

/**
 * ephy_window_get_location_controller:
 * @window: an #EphyWindow
 *
 * Returns the @window #EphyLocationController
 *
 * Returns: (transfer none): the @window #EphyLocationController
 **/
EphyLocationController *
ephy_window_get_location_controller (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return window->location_controller;
}

/**
 * ephy_window_get_header_bar:
 * @window: an #EphyWindow
 *
 * Returns the @window #EphyHeaderBar
 *
 * Returns: (transfer none): the @window #EphyHeaderBar
 **/
GtkWidget *
ephy_window_get_header_bar (EphyWindow *window)
{
  return window->header_bar;
}

typedef struct {
  EphyWindow *window;

  guint embeds_to_check;
  GList *modified_embeds;
} WindowHasModifiedFormsData;

static void
window_has_modified_forms_data_free (WindowHasModifiedFormsData *data)
{
  g_clear_list (&data->modified_embeds, NULL);
  g_free (data);
}

void
ephy_window_handle_quit_with_modified_forms (EphyWindow *window)
{
  EphyShell *shell = ephy_shell_get_default ();

  if (!ephy_shell_get_checking_modified_forms (shell)) {
    window->force_close = TRUE;
    ephy_window_close (window);
    window->force_close = FALSE;
  } else {
    int windows = ephy_shell_get_num_windows_with_modified_forms (shell);

    ephy_shell_set_num_windows_with_modified_forms (shell, windows - 1);
  }
}

guint
ephy_window_get_has_modified_forms (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return window->has_modified_forms;
}

static void
force_close_window_cb (EphyWindow *window)
{
  ephy_window_handle_quit_with_modified_forms (window);
  g_object_unref (window);
}

static void
finish_window_close_after_modified_forms_check (WindowHasModifiedFormsData *data)
{
  /* Need to schedule future destruction of the EphyWindow to ensure its child
   * AdwAlertDialog that's displaying the close confirmation warning gets
   * destroyed first.
   */
  g_idle_add_once ((GSourceOnceFunc)force_close_window_cb, g_object_ref (data->window));
  data->window->has_modified_forms = FALSE;
  window_has_modified_forms_data_free (data);
}

static void
stop_window_close_after_modified_forms_check (WindowHasModifiedFormsData *data)
{
  EphyShell *shell = ephy_shell_get_default ();

  data->window->has_modified_forms = FALSE;
  ephy_shell_set_checking_modified_forms (shell, FALSE);
  ephy_shell_set_num_windows_with_modified_forms (shell, 0);
  window_has_modified_forms_data_free (data);
}

static void
continue_window_close_after_modified_forms_check (WindowHasModifiedFormsData *data)
{
  data->window->checking_modified_forms = FALSE;
  g_clear_handle_id (&data->window->modified_forms_timeout_id, g_source_remove);

  if (data->modified_embeds) {
    AdwDialog *dialog;

    /* Jump to the current tab with modified forms */
    impl_set_active_child (EPHY_EMBED_CONTAINER (data->window),
                           data->modified_embeds->data);

    data->modified_embeds = data->modified_embeds->next;

    dialog = construct_confirm_close_dialog (data->window,
                                             _("Leave Website?"),
                                             _("A form was modified and has not been submitted"),
                                             _("_Discard Form"));
    g_signal_connect_swapped (dialog, "response::accept",
                              G_CALLBACK (continue_window_close_after_modified_forms_check),
                              data);
    g_signal_connect_swapped (dialog, "response::cancel",
                              G_CALLBACK (stop_window_close_after_modified_forms_check),
                              data);
    adw_dialog_present (dialog, GTK_WIDGET (data->window));

    return;
  }

  if (!check_and_run_downloads_in_background (data->window))
    finish_window_close_after_modified_forms_check (data);
}

static void
window_has_modified_forms_cb (EphyWebView                *view,
                              GAsyncResult               *result,
                              WindowHasModifiedFormsData *data)
{
  gboolean has_modified_forms;

  data->embeds_to_check--;
  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
  if (has_modified_forms) {
    data->modified_embeds = g_list_prepend (data->modified_embeds, EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view));
    data->window->has_modified_forms = TRUE;
  }

  if (data->embeds_to_check > 0)
    return;

  continue_window_close_after_modified_forms_check (data);
}

/* This function checks an entire EphyWindow to see if it contains any tab with
 * modified forms. There is an entirely separate codepath for checking whether
 * a single tab has modified forms, see tab_view_close_page_cb().
 */
static void
ephy_window_check_modified_forms (EphyWindow *window)
{
  GList *tabs, *l;
  WindowHasModifiedFormsData *data;

  data = g_new0 (WindowHasModifiedFormsData, 1);
  data->window = window;
  data->embeds_to_check = ephy_tab_view_get_n_pages (window->tab_view);

  tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));
  if (!tabs) {
    window_has_modified_forms_data_free (data);
    return;
  }

  window->checking_modified_forms = TRUE;

  for (l = tabs; l; l = l->next) {
    EphyEmbed *embed = (EphyEmbed *)l->data;

    ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
                                      NULL,
                                      (GAsyncReadyCallback)window_has_modified_forms_cb,
                                      data);
  }

  g_list_free (tabs);
}

static void
window_close_with_multiple_tabs_cb (EphyWindow *window)
{
  window->confirmed_close_with_multiple_tabs = TRUE;
  gtk_window_close (GTK_WINDOW (window));
}

gboolean
ephy_window_can_close (EphyWindow *window)
{
  EphySession *session;
  gboolean fullscreen_lockdown;

  fullscreen_lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN) ||
                        ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_KIOSK;

  /* We ignore the delete_event if the disable_quit lockdown has been set
   */
  if (fullscreen_lockdown || g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_QUIT))
    return FALSE;

  if (window->checking_modified_forms) {
    /* stop window close */
    return FALSE;
  }

  if (!window->force_close &&
      g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                              EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA) &&
      ephy_tab_view_get_n_pages (window->tab_view) > 0) {
    ephy_window_check_modified_forms (window);
    /* stop window close */
    return FALSE;
  }

  session = ephy_shell_get_session (ephy_shell_get_default ());
  if (ephy_shell_get_n_windows (ephy_shell_get_default ()) > 1 &&
      ephy_tab_view_get_n_pages (window->tab_view) > 1 &&
      !(session && ephy_session_is_closing (session)) &&
      !window->confirmed_close_with_multiple_tabs) {
    AdwDialog *dialog;

    dialog = construct_confirm_close_dialog (window,
                                             _("Close Multiple Tabs?"),
                                             _("If this window is closed, all open tabs will be lost"),
                                             _("C_lose Tabs"));

    g_signal_connect_swapped (dialog, "response::accept",
                              G_CALLBACK (window_close_with_multiple_tabs_cb),
                              window);
    adw_dialog_present (dialog, GTK_WIDGET (window));

    /* stop window close */
    return FALSE;
  }

  if (ephy_shell_get_n_windows (ephy_shell_get_default ()) == 1) {
    if (check_and_run_downloads_in_background (window)) {
      /* stop window close */
      return FALSE;
    }

    if (session)
      ephy_session_close (session);
  }

  return TRUE;
}

/**
 * ephy_window_close:
 * @window: an #EphyWindow
 *
 * Try to close the window. The window might refuse to close
 * if there are ongoing download operations or unsubmitted
 * modified forms.
 *
 * Returns: %TRUE if the window is closed, or %FALSE otherwise
 **/
gboolean
ephy_window_close (EphyWindow *window)
{
  gboolean can_close = ephy_window_can_close (window);

  if (can_close)
    gtk_window_destroy (GTK_WINDOW (window));

  return can_close;
}

EphyWindowChrome
ephy_window_get_chrome (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return window->chrome;
}

void
ephy_window_show_fullscreen_header_bar (EphyWindow *window)
{
  window->show_fullscreen_header_bar = TRUE;
}

gboolean
ephy_window_is_maximized (EphyWindow *window)
{
  return window->is_maximized;
}

gboolean
ephy_window_is_fullscreen (EphyWindow *window)
{
  return window->is_fullscreen;
}

guint64
ephy_window_get_uid (EphyWindow *window)
{
  return window->uid;
}

GActionGroup *
ephy_window_get_action_group (EphyWindow *window,
                              const char *prefix)
{
  return g_hash_table_lookup (window->action_groups, prefix);
}

gboolean
ephy_window_get_sidebar_shown (EphyWindow *window)
{
  return adw_overlay_split_view_get_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (window->overlay_split_view));
}

static void
drop_toast (EphyWindow *self)
{
  g_clear_pointer (&self->switch_toast, adw_toast_dismiss);
}

static void
toast_dismissed_cb (EphyWindow *self)
{
  g_object_weak_unref (G_OBJECT (self->switch_to_tab), (GWeakNotify)drop_toast, self);
  self->switch_to_tab = NULL;
}

void
ephy_window_switch_to_new_tab_toast (EphyWindow *self,
                                     GtkWidget  *tab)
{
  if (self->adaptive_mode != EPHY_ADAPTIVE_MODE_NARROW)
    return;

  self->switch_toast = adw_toast_new (_("New tab opened"));
  g_signal_connect_swapped (self->switch_toast, "dismissed", G_CALLBACK (toast_dismissed_cb), self);
  self->switch_to_tab = tab;
  g_object_weak_ref (G_OBJECT (self->switch_to_tab), (GWeakNotify)drop_toast, self);
  adw_toast_set_button_label (ADW_TOAST (self->switch_toast), _("Switch"));
  adw_toast_set_action_name (ADW_TOAST (self->switch_toast), "win.switch-new-tab");
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay), self->switch_toast);
}

void
ephy_window_switch_to_new_tab (EphyWindow *window)
{
  ephy_tab_view_select_page (ephy_window_get_tab_view (window), window->switch_to_tab);
}

gboolean
ephy_window_get_show_sidebar (EphyWindow *self)
{
  return adw_overlay_split_view_get_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->overlay_split_view));
}

static void
bookmark_removed_toast_dismissed (AdwToast     *toast,
                                  EphyBookmark *bookmark)
{
  g_object_unref (bookmark);
}

static void
bookmark_removed_toast_button_clicked (AdwToast     *toast,
                                       EphyBookmark *bookmark)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  if (ephy_bookmarks_manager_get_bookmark_by_url (manager, ephy_bookmark_get_url (bookmark)))
    return;

  ephy_bookmarks_manager_add_bookmark (manager, bookmark);
}

void
ephy_window_bookmark_removed_toast (EphyWindow   *self,
                                    EphyBookmark *bookmark,
                                    AdwToast     *toast)
{
  g_signal_connect_object (toast, "dismissed", G_CALLBACK (bookmark_removed_toast_dismissed), bookmark, 0);
  g_signal_connect_object (toast, "button-clicked", G_CALLBACK (bookmark_removed_toast_button_clicked), bookmark, 0);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay), toast);
}

void
ephy_window_toggle_bookmarks (EphyWindow *self)
{
  gboolean state;

  state = !adw_overlay_split_view_get_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->overlay_split_view));
  adw_overlay_split_view_set_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->overlay_split_view), state);

  if (state) {
    ephy_bookmarks_dialog_set_is_editing (EPHY_BOOKMARKS_DIALOG (self->bookmarks_dialog), FALSE);
    ephy_bookmarks_dialog_focus (EPHY_BOOKMARKS_DIALOG (self->bookmarks_dialog));
  } else {
    EphyWebView *web_view = ephy_embed_get_web_view (self->active_embed);

    ephy_bookmarks_dialog_clear_search (EPHY_BOOKMARKS_DIALOG (self->bookmarks_dialog));

    /* Set the navigation flags so the buttons don't become sensitive when exiting. */
    _ephy_window_set_navigation_flags (self, ephy_web_view_get_navigation_flags (web_view));
  }
}

void
ephy_window_show_toast (EphyWindow *window,
                        const char *text)
{
  AdwToast *toast = adw_toast_new (text);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (window->toast_overlay), toast);
}
