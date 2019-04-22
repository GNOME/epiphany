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

#include "ephy-action-bar.h"
#include "ephy-action-helper.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-gsb-utils.h"
#include "ephy-gui.h"
#include "ephy-header-bar.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-mouse-gesture-controller.h"
#include "ephy-notebook.h"
#include "ephy-pages-view.h"
#include "ephy-prefs.h"
#include "ephy-security-popover.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-tab-header-bar.h"
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-view.h"
#include "ephy-zoom.h"
#include "popup-commands.h"
#include "window-commands.h"

#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <stdlib.h>

#include <webkit2/webkit2.h>

/**
 * SECTION:ephy-window
 * @short_description: Epiphany's main #GtkWindow widget
 *
 * #EphyWindow is Epiphany's main widget.
 */

static void ephy_window_change_allow_popup_windows_state (GSimpleAction *action,
                                                          GVariant      *state,
                                                          gpointer       user_data);

const struct {
  const char *action_and_target;
  const char *accelerators[9];
} accels [] = {
  /* Page Menu accels */
  { "win.page-menu", { "F10", NULL } },
  { "win.new-tab", { "<Primary>T", NULL } },
  { "win.open", { "<Primary>O", NULL } },
  { "win.save-as", { "<shift><Primary>S", "<Primary>S", NULL } },
  { "win.save-as-application", { "<shift><Primary>A", NULL } },
  { "win.open-application-manager", { "<shift><Primary>C", NULL } },
  { "win.undo", { "<Primary>Z", NULL } },
  { "win.redo", { "<shift><Primary>Z", NULL } },
  { "win.copy", { "<Primary>C", NULL } },
  { "win.cut", { "<Primary>X", NULL } },
  { "win.paste", { "<Primary>V", NULL } },
  { "win.zoom-in", { "<Primary>plus", "<Primary>KP_Add", "<Primary>equal", "ZoomIn", NULL } },
  { "win.zoom-out", { "<Primary>minus", "<Primary>KP_Subtract", "ZoomOut", NULL } },
  { "win.zoom-normal", { "<Primary>0", "<Primary>KP_0", NULL } },
  { "win.print", { "<Primary>P", NULL } },
  { "win.find", { "<Primary>F", NULL } },
  { "win.find-prev", { "<shift><Primary>G", NULL } },
  { "win.find-next", { "<Primary>G", NULL } },
  { "win.bookmark-page", { "<Primary>D", "AddFavorite", NULL } },
  { "win.encoding", { NULL } },
  { "win.page-source", { "<Primary>U", NULL } },
  { "win.toggle-inspector", { "<shift><Primary>I", "F12", NULL } },
  { "win.toggle-reader-mode", { "<alt><Primary>r", NULL } },

  { "win.select-all", { "<Primary>A", NULL } },

  { "win.send-to", { "Send", NULL } },
  { "win.location", { "<Primary>L", "<alt>D", "F6", "Go", "OpenURL", NULL } },
  { "win.home", { "<alt>Home", NULL } },
  { "win.content", { "Escape", NULL } },

  /* Toggle actions */
  { "win.browse-with-caret", { "F7", NULL } },
  { "win.fullscreen", { "F11", NULL } },
  { "win.allow-popup-windows", { NULL } },

  /* Navigation */
  { "toolbar.stop", { "Escape", "Stop", NULL } },
  { "toolbar.reload", { "<Primary>R", "F5", "Refresh", "Reload", NULL } },
  { "toolbar.reload-bypass-cache", { "<shift><Primary>R", "<shift>F5" } },
  { "toolbar.combined-stop-reload", { NULL } },

  /* Tabs */
  { "tab.previous", { "<Primary>Page_Up", "<Primary>KP_9", "<shift><Primary>Tab", NULL } },
  { "tab.next", { "<Primary>Page_Down", "<Primary>KP_3", "<Primary>Tab", NULL } },
  { "tab.move-left", { "<shift><Primary>Page_Up", "<shift><Primary>Page_Up", NULL } },
  { "tab.move-right", { "<shift><Primary>Page_Down", "<shift><Primary>Page_Down", NULL } },
  { "tab.duplicate", { NULL } },
  { "tab.close", { "<Primary>W", NULL } },
  { "tab.pin", { NULL } }
}, accels_navigation_ltr [] = {
  { "toolbar.navigation-back", { "<alt>Left", "<alt>KP_Left", "<alt>KP_4", "Back", NULL } },
  { "toolbar.navigation-forward", { "<alt>Right", "<alt>KP_Right", "<alt>KP_6", "Forward", NULL } }
}, accels_navigation_rtl [] = {
  { "toolbar.navigation-back", { "<alt>Right", "<alt>KP_Right", "<alt>KP_6", "Back", NULL } },
  { "toolbar.navigation-forward", { "<alt>Left", "<alt>KP_Left", "<alt>KP_4", "Forward", NULL } }
}, *accels_navigation_ltr_rtl;

#define SETTINGS_CONNECTION_DATA_KEY    "EphyWindowSettings"

struct _EphyWindow {
  DzlApplicationWindow parent_instance;

  GtkWidget *header_bar;
  GtkWidget *main_stack;
  EphyPagesView *pages_view;
  EphyBookmarksManager *bookmarks_manager;
  GHashTable *action_labels;
  GtkNotebook *notebook;
  GtkWidget *action_bar;
  EphyEmbed *active_embed;
  EphyWindowChrome chrome;
  EphyEmbedEvent *context_event;
  WebKitHitTestResult *hit_test_result;
  guint idle_worker;
  EphyLocationController *location_controller;
  guint modified_forms_timeout_id;
  EphyMouseGestureController *mouse_gesture_controller;
  EphyEmbed *last_opened_embed;
  int last_opened_pos;
  gboolean show_fullscreen_header_bar;

  gint current_width;
  gint current_height;
  gint current_x;
  gint current_y;

  guint has_default_size : 1;
  guint has_default_position : 1;
  guint is_maximized : 1;
  guint is_fullscreen : 1;
  guint closing : 1;
  guint is_popup : 1;
  guint updating_address : 1;
  guint force_close : 1;
  guint checking_modified_forms : 1;
  guint present_on_insert : 1;

  guint32 present_on_insert_user_time;
};

enum {
  PROP_0,
  PROP_ACTIVE_CHILD,
  PROP_CHROME,
  PROP_SINGLE_TAB_MODE
};

/* Make sure not to overlap with those in ephy-lockdown.c */
enum {
  SENS_FLAG_CHROME        = 1 << 0,
  SENS_FLAG_CONTEXT       = 1 << 1,
  SENS_FLAG_DOCUMENT      = 1 << 2,
  SENS_FLAG_LOADING       = 1 << 3,
  SENS_FLAG_NAVIGATION    = 1 << 4,
  SENS_FLAG_IS_BLANK      = 1 << 5
};

static gint
impl_add_child (EphyEmbedContainer *container,
                EphyEmbed          *child,
                gint                position,
                gboolean            jump_to)
{
  EphyWindow *window = EPHY_WINDOW (container);

  g_assert (!window->is_popup || gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) < 1);

  return ephy_notebook_add_tab (EPHY_NOTEBOOK (window->notebook),
                                child, position, jump_to);
}

static void
impl_set_active_child (EphyEmbedContainer *container,
                       EphyEmbed          *child)
{
  int page;
  EphyWindow *window;

  window = EPHY_WINDOW (container);

  page = gtk_notebook_page_num (window->notebook, GTK_WIDGET (child));
  gtk_notebook_set_current_page (window->notebook, page);
}

static GtkWidget *
construct_confirm_close_dialog (EphyWindow *window,
                                const char *title,
                                const char *info,
                                const char *action)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   "%s", title);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", info);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         action, GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  /* FIXME gtk_window_set_title (GTK_WINDOW (dialog), _("Close Document?")); */
  gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (window)),
                               GTK_WINDOW (dialog));

  return dialog;
}

static gboolean
confirm_close_with_modified_forms (EphyWindow *window)
{
  GtkWidget *dialog;
  int response;

  dialog = construct_confirm_close_dialog (window,
                                           _("Do you want to leave this website?"),
                                           _("A form you modified has not been submitted."),
                                           _("_Discard form"));
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return response == GTK_RESPONSE_ACCEPT;
}

static gboolean
confirm_close_with_downloads (EphyWindow *window)
{
  GtkWidget *dialog;
  int response;

  dialog = construct_confirm_close_dialog (window,
                                           _("There are ongoing downloads"),
                                           _("If you quit, the downloads will be cancelled"),
                                           _("Quit and cancel downloads"));
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return response == GTK_RESPONSE_ACCEPT;
}

static void
impl_remove_child (EphyEmbedContainer *container,
                   EphyEmbed          *child)
{
  EphyWindow *window;

  window = EPHY_WINDOW (container);
  g_signal_emit_by_name (window->notebook,
                         "tab-close-request",
                         child, window);
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

  return gtk_container_get_children (GTK_CONTAINER (window->notebook));
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

  return gtk_notebook_get_n_pages (window->notebook);
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
ephy_window_open_link (EphyLink     *link,
                       const char   *address,
                       EphyEmbed    *embed,
                       EphyLinkFlags flags)
{
  EphyWindow *window = EPHY_WINDOW (link);
  EphyEmbed *new_embed;
  EphyWebView *web_view;

  g_assert (address != NULL || (flags & (EPHY_LINK_NEW_WINDOW | EPHY_LINK_NEW_TAB | EPHY_LINK_HOME_PAGE)));

  if (embed == NULL) {
    embed = window->active_embed;
  }

  if (flags & EPHY_LINK_BOOKMARK)
    ephy_web_view_set_visit_type (ephy_embed_get_web_view (embed),
                                  EPHY_PAGE_VISIT_BOOKMARK);
  else if (flags & EPHY_LINK_TYPED)
    ephy_web_view_set_visit_type (ephy_embed_get_web_view (embed),
                                  EPHY_PAGE_VISIT_TYPED);

  if (flags & (EPHY_LINK_JUMP_TO |
               EPHY_LINK_NEW_TAB |
               EPHY_LINK_NEW_WINDOW)) {
    EphyNewTabFlags ntflags = 0;
    EphyWindow *target_window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed)));

    if (flags & EPHY_LINK_JUMP_TO) {
      ntflags |= EPHY_NEW_TAB_JUMP;
    }

    if (flags & EPHY_LINK_NEW_WINDOW ||
        (flags & EPHY_LINK_NEW_TAB && window->is_popup)) {
      target_window = ephy_window_new ();
    }

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

  if (address)
    ephy_web_view_load_url (web_view, address);
  else if (flags & EPHY_LINK_NEW_TAB)
    ephy_web_view_load_new_tab_page (web_view);
  else if (flags & (EPHY_LINK_NEW_WINDOW | EPHY_LINK_HOME_PAGE))
    ephy_web_view_load_homepage (web_view);

  if (ephy_web_view_get_is_blank (web_view))
    ephy_window_activate_location (window);
  else
    gtk_widget_grab_focus (GTK_WIDGET (new_embed));

  return new_embed;
}

static void
ephy_window_link_iface_init (EphyLinkInterface *iface)
{
  iface->open_link = ephy_window_open_link;
}

G_DEFINE_TYPE_WITH_CODE (EphyWindow, ephy_window, DZL_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                ephy_window_link_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_CONTAINER,
                                                ephy_window_embed_container_iface_init))

static void
sync_chromes_visibility (EphyWindow *window)
{
  gboolean show_tabsbar;

  if (window->closing)
    return;

  show_tabsbar = (window->chrome & EPHY_WINDOW_CHROME_TABSBAR);

  ephy_notebook_set_tabs_allowed (EPHY_NOTEBOOK (window->notebook),
                                  show_tabsbar && !(window->is_popup || window->is_fullscreen));
}

static void
ephy_window_set_chrome (EphyWindow      *window,
                        EphyWindowChrome chrome)
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
sync_tab_load_status (EphyWebView    *view,
                      WebKitLoadEvent load_event,
                      EphyWindow     *window)
{
  GActionGroup *action_group;
  GAction *action;
  gboolean loading;

  if (window->closing)
    return;

  loading = ephy_web_view_is_loading (view);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");

  /* disable print while loading, see bug #116344 */
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "print");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        SENS_FLAG_LOADING, loading);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "toolbar");

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
update_adaptive_mode (EphyWindow *window)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  EphyActionBar *action_bar = EPHY_ACTION_BAR (window->action_bar);
  EphyNotebook *notebook = EPHY_NOTEBOOK (window->notebook);
  gboolean is_narrow, is_mobile_landscape;
  EphyAdaptiveMode adaptive_mode;
  gint width, height;
  GdkDisplay *display;
  GdkWindow *surface;
  GdkMonitor *monitor = NULL;
  GdkRectangle geometry = {};

  gtk_window_get_size (GTK_WINDOW (window),
                       &width,
                       &height);

  /* Get the monitor to guess whether we are on a mobile or not. If not found,
   * fallback to the window size.
   */
  display = gtk_widget_get_display (GTK_WIDGET (window));
  surface = gtk_widget_get_window (GTK_WIDGET (window));
  if (display != NULL && surface != NULL)
    monitor = gdk_display_get_monitor_at_window (display, surface);
  if (monitor != NULL)
    gdk_monitor_get_geometry (monitor, &geometry);
  else
    geometry.height = height;

  /* window->is_maximized doesn't work here for some reason, so we use
   * gtk_window_is_maximized() instead.
   */
  is_narrow = width <= 600;
  is_mobile_landscape = geometry.height <= 400 &&
                        (gtk_window_is_maximized (GTK_WINDOW (window)) ||
                         window->is_fullscreen);
  adaptive_mode = is_narrow || is_mobile_landscape ?
    EPHY_ADAPTIVE_MODE_NARROW :
    EPHY_ADAPTIVE_MODE_NORMAL;
  ephy_header_bar_set_adaptive_mode (header_bar, adaptive_mode);
  ephy_action_bar_set_adaptive_mode (action_bar, adaptive_mode);
  ephy_notebook_set_adaptive_mode (notebook, adaptive_mode);

  /* When switching to desktop sizes, drop the tabs view and go back
   * to the main view.
   */
  if (adaptive_mode == EPHY_ADAPTIVE_MODE_NORMAL)
    gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "content");
}

static void
ephy_window_fullscreen (EphyWindow *window)
{
  EphyEmbed *embed;

  window->is_fullscreen = TRUE;

  /* sync status */
  embed = window->active_embed;
  sync_tab_load_status (ephy_embed_get_web_view (embed), WEBKIT_LOAD_STARTED, window);
  sync_tab_security (ephy_embed_get_web_view (embed), NULL, window);

  update_adaptive_mode (window);
  sync_chromes_visibility (window);
  ephy_embed_entering_fullscreen (embed);
}

static void
ephy_window_unfullscreen (EphyWindow *window)
{
  window->is_fullscreen = FALSE;

  update_adaptive_mode (window);
  sync_chromes_visibility (window);
  ephy_embed_leaving_fullscreen (window->active_embed);
}

static gboolean
ephy_window_should_view_receive_key_press_event (EphyWindow  *window,
                                                 GdkEventKey *event)
{
  GdkDisplay *display;
  GdkKeymap *keymap;
  guint keyval;
  GdkModifierType consumed;
  GdkModifierType state_mask = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK;

  display = gtk_widget_get_display (GTK_WIDGET (window));
  keymap = gdk_keymap_get_for_display (display);

  gdk_keymap_translate_keyboard_state (keymap,
                                       event->hardware_keycode,
                                       event->state,
                                       event->group,
                                       &keyval,
                                       NULL,
                                       NULL,
                                       &consumed);
  state_mask &= ~consumed;

  /* Focus location entry */
  if (keyval == GDK_KEY_F6)
    return FALSE;

  /* Websites are allowed to override most Epiphany accelerators, but not
   * window or tab management accelerators. */
  if ((event->state & state_mask) == GDK_CONTROL_MASK)
    return keyval != GDK_KEY_n &&         /* New Window */
           keyval != GDK_KEY_q &&         /* Quit */
           keyval != GDK_KEY_T &&         /* Reopen Closed Tab */
           keyval != GDK_KEY_t &&         /* New Tab */
           keyval != GDK_KEY_w &&         /* Close Tab */
           keyval != GDK_KEY_Page_Up &&   /* Previous Tab */
           keyval != GDK_KEY_KP_9 &&      /* Previous Tab */
           keyval != GDK_KEY_Page_Down && /* Next Tab */
           keyval != GDK_KEY_KP_3 &&      /* Next Tab */
           keyval != GDK_KEY_Tab &&       /* Next Tab */
           keyval != GDK_KEY_ISO_Left_Tab;/* Previous Tab (Shift+Tab -> ISO Left Tab) */

  if ((event->state & state_mask) == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
    return keyval != GDK_KEY_n &&         /* New Incognito Window */
           keyval != GDK_KEY_Page_Up &&   /* Move Tab Left */
           keyval != GDK_KEY_KP_9 &&      /* Move Tab Left */
           keyval != GDK_KEY_Page_Down && /* Move Tab Right */
           keyval != GDK_KEY_KP_3;        /* Move Tab Right */

  if ((event->state & state_mask) == GDK_MOD1_MASK)
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
ephy_window_key_press_event (GtkWidget   *widget,
                             GdkEventKey *event)
{
  EphyWebView *view;

  view = ephy_embed_get_web_view (EPHY_WINDOW (widget)->active_embed);
  if (gtk_window_get_focus (GTK_WINDOW (widget)) != GTK_WIDGET (view))
    return GTK_WIDGET_CLASS (ephy_window_parent_class)->key_press_event (widget, event);

  /* GtkWindow's key press handler first calls gtk_window_activate_key,
   * then gtk_window_propagate_key_event. We want to do the opposite,
   * because we want to give webpages the chance to override most
   * Epiphany shortcuts. For example, Ctrl+I in Google Docs should
   * italicize your text and not open a new incognito window. So:
   * first, propagate the event to the web view. Next, try
   * accelerators only if the web view did not handle the event. But
   * short-circuit the event propagation if it's a special keybinding
   * that is reserved for Epiphany not allowed to be seen by webpages.
   */
  if (!ephy_window_should_view_receive_key_press_event (EPHY_WINDOW (widget), event) ||
      !gtk_window_propagate_key_event (GTK_WINDOW (widget), event))
    gtk_window_activate_key (GTK_WINDOW (widget), event);

  return GDK_EVENT_STOP;
}

static gboolean
ephy_window_delete_event (GtkWidget   *widget,
                          GdkEventAny *event)
{
  if (!ephy_window_close (EPHY_WINDOW (widget)))
    return TRUE;

  /* proceed with window close */
  if (GTK_WIDGET_CLASS (ephy_window_parent_class)->delete_event) {
    return GTK_WIDGET_CLASS (ephy_window_parent_class)->delete_event (widget, event);
  }

  return FALSE;
}

#define MAX_SPELL_CHECK_GUESSES 4

static void
update_link_actions_sensitivity (EphyWindow *window,
                                 gboolean    link_has_web_scheme)
{
  GAction *action;
  GActionGroup *action_group;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "popup");

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
update_edit_action_sensitivity (EphyWindow *window, const char *action_name, gboolean sensitive, gboolean hide)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       action_name);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive || hide);
}

static void
update_edit_actions_sensitivity (EphyWindow *window, gboolean hide)
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
    can_undo = EPHY_IS_LOCATION_ENTRY (title_widget) &&
               ephy_location_entry_get_can_undo (EPHY_LOCATION_ENTRY (title_widget));
    can_redo = EPHY_IS_LOCATION_ENTRY (title_widget) &&
               ephy_location_entry_get_can_redo (EPHY_LOCATION_ENTRY (title_widget));
  } else {
    EphyEmbed *embed;
    WebKitWebView *view;
    WebKitEditorState *state;

    embed = window->active_embed;
    g_assert (embed != NULL);

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
  update_edit_action_sensitivity (window, "undo", can_undo, hide);
  update_edit_action_sensitivity (window, "redo", can_redo, hide);
}

static void
enable_edit_actions_sensitivity (EphyWindow *window)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "cut");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "copy");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "paste");
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
  EphyActionBarStart *action_bar_start = ephy_action_bar_get_action_bar_start (EPHY_ACTION_BAR (window->action_bar));

  ephy_action_bar_start_change_combined_stop_reload_state (header_bar_start,
                                                           g_variant_get_boolean (loading));
  ephy_action_bar_start_change_combined_stop_reload_state (action_bar_start,
                                                           g_variant_get_boolean (loading));

  g_simple_action_set_state (action, loading);
}

static const GActionEntry window_entries [] =
{
  { "page-menu", window_cmd_page_menu },
  { "new-tab", window_cmd_new_tab },
  { "open", window_cmd_open },
  { "save-as", window_cmd_save_as },
  { "save-as-application", window_cmd_save_as_application },
  { "open-application-manager", window_cmd_open_application_manager },
  { "undo", window_cmd_undo },
  { "redo", window_cmd_redo },
  { "cut", window_cmd_cut },
  { "copy", window_cmd_copy },
  { "paste", window_cmd_paste },
  { "delete", window_cmd_delete },
  { "zoom-in", window_cmd_zoom_in },
  { "zoom-out", window_cmd_zoom_out },
  { "zoom-normal", window_cmd_zoom_normal },
  { "print", window_cmd_print },
  { "find", window_cmd_find },
  { "find-prev", window_cmd_find_prev },
  { "find-next", window_cmd_find_next },
  { "open-bookmark", window_cmd_open_bookmark, "s" },
  { "bookmark-page", window_cmd_bookmark_page },
  { "encoding", window_cmd_encoding },
  { "page-source", window_cmd_page_source },
  { "toggle-inspector", window_cmd_toggle_inspector },
  { "toggle-reader-mode", window_cmd_toggle_reader_mode },

  { "select-all", window_cmd_select_all },

  { "send-to", window_cmd_send_to },
  { "location", window_cmd_go_location },
  { "home", window_cmd_go_home },
  { "content", window_cmd_go_content },
  { "tabs-view", window_cmd_go_tabs_view },

  { "show-tab", window_cmd_show_tab, "u", "uint32 0", window_cmd_change_show_tab_state },

  /* Toggle actions */
  { "browse-with-caret", NULL, NULL, "false", window_cmd_change_browse_with_caret_state },
  { "fullscreen", NULL, NULL, "false", window_cmd_change_fullscreen_state },
  { "allow-popup-windows", NULL, NULL, "true", ephy_window_change_allow_popup_windows_state }
};

static const GActionEntry tab_entries [] = {
  { "previous", window_cmd_tabs_previous },
  { "next", window_cmd_tabs_next },
  { "duplicate", window_cmd_tabs_duplicate },
  { "detach", window_cmd_tabs_detach },
  { "close", window_cmd_tabs_close },
  { "close-tab", window_cmd_tabs_close },
  { "close-left", window_cmd_tabs_close_left },
  { "close-right", window_cmd_tabs_close_right },
  { "move-left", window_cmd_tabs_move_left },
  { "move-right", window_cmd_tabs_move_right },
  { "close-others", window_cmd_tabs_close_others },
  { "reload", window_cmd_tabs_reload },
  { "reopen", window_cmd_tabs_reopen_closed_tab },
  { "reload-all", window_cmd_tabs_reload_all_tabs },
  { "pin", window_cmd_tabs_pin },
  { "unpin", window_cmd_tabs_unpin },
};

static const GActionEntry toolbar_entries [] = {
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

  { "open-link-in-new-window", popup_cmd_link_in_new_window },
  { "open-link-in-new-tab", popup_cmd_link_in_new_tab },
  { "open-link-in-incognito-window", popup_cmd_link_in_incognito_window },
  { "download-link-as", popup_cmd_download_link_as },
  { "copy-link-address", popup_cmd_copy_link_address },
  { "copy-email-address", popup_cmd_copy_link_address },

  /* Images. */

  { "view-image", popup_cmd_view_image_in_new_tab },
  { "copy-image-location", popup_cmd_copy_image_location },
  { "save-image-as", popup_cmd_save_image_as },
  { "set-image-as-background", popup_cmd_set_image_as_background },

  /* Video. */

  { "open-video-in-new-window", popup_cmd_media_in_new_window },
  { "open-video-in-new-tab", popup_cmd_media_in_new_tab },
  { "save-video-as", popup_cmd_save_media_as },
  { "copy-video-location", popup_cmd_copy_media_location },

  /* Audio. */

  { "open-audio-in-new-window", popup_cmd_media_in_new_window },
  { "open-audio-in-new-tab", popup_cmd_media_in_new_tab },
  { "save-audios-as", popup_cmd_save_media_as },
  { "copy-audio-location", popup_cmd_copy_media_location },

  /* Selection */
  { "search-selection", popup_cmd_search_selection, "s" }
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
  { "select-all", N_("Select _All") },

  { "send-to", N_("S_end Link by Email…") },

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

  { "save-as", N_("Save Pa_ge As…") },
  { "page-source", N_("_Page Source") }
};

static char *
calculate_location (const char *typed_address, const char *address)
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

  const char *action_group_actions[] = { "save-as", "save-as-application", "print",
                                         "find", "find-prev", "find-next",
                                         "bookmark-page", "encoding", "page-source",
                                         "send-to",
                                         NULL };

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");

  /* Page menu */
  for (i = 0; action_group_actions[i] != NULL; i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         action_group_actions[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          flags, set);
  }

  /* Page context popup */
  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "popup");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "context-bookmark-page");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        flags, set);

  /* Toolbar */
  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "toolbar");
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

  if (window->closing)
    return;

  address = ephy_web_view_get_display_address (view);
  typed_address = ephy_web_view_get_typed_address (view);

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_BLANK,
                                              ephy_web_view_get_is_blank (view));

  location = calculate_location (typed_address, address);
  ephy_window_set_location (window, location);

  g_free (location);
}

static void
sync_tab_zoom (WebKitWebView *web_view, GParamSpec *pspec, EphyWindow *window)
{
  GActionGroup *action_group;
  GAction *action;
  gboolean can_zoom_in = TRUE, can_zoom_out = TRUE, can_zoom_normal = FALSE;
  double zoom;
  GtkWidget *zoom_level_button;
  gchar *zoom_level;

  if (window->closing)
    return;

  zoom = webkit_web_view_get_zoom_level (web_view);

  zoom_level_button = ephy_header_bar_get_zoom_level_button (EPHY_HEADER_BAR (window->header_bar));

  zoom_level = g_strdup_printf ("%2.0f%%", zoom * 100);
  gtk_button_set_label (GTK_BUTTON (zoom_level_button), zoom_level);
  g_free (zoom_level);

  if (zoom >= ZOOM_MAXIMAL) {
    can_zoom_in = FALSE;
  }

  if (zoom <= ZOOM_MINIMAL) {
    can_zoom_out = FALSE;
  }

  if (zoom != g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL)) {
    can_zoom_normal = TRUE;
  }

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

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

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

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

  if (!can_find) {
    ephy_find_toolbar_request_close (ephy_embed_get_find_toolbar (window->active_embed));
  }
}

static void
_ephy_window_set_navigation_flags (EphyWindow                *window,
                                   EphyWebViewNavigationFlags flags)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "toolbar");

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

  _ephy_window_set_navigation_flags (window,
                                     ephy_web_view_get_navigation_flags (view));
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
sync_tab_bookmarked_status (EphyWebView   *view,
                            GParamSpec    *pspec,
                            EphyWindow    *window)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  EphyEmbedShellMode mode;
  EphyLocationEntryBookmarkIconState state;
  GtkWidget *widget;
  EphyBookmark *bookmark;
  const char *address;

  widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar)));

  if (!EPHY_IS_LOCATION_ENTRY (widget))
    return;

  address = ephy_web_view_get_address (view);
  mode = ephy_embed_shell_get_mode (shell);

  if (!address ||
      ephy_embed_utils_is_no_show_address (address) ||
      mode == EPHY_EMBED_SHELL_MODE_INCOGNITO ||
      mode == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    state = EPHY_LOCATION_ENTRY_BOOKMARK_ICON_HIDDEN;
  } else {
    bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, address);
    state = bookmark ? EPHY_LOCATION_ENTRY_BOOKMARK_ICON_BOOKMARKED
                     : EPHY_LOCATION_ENTRY_BOOKMARK_ICON_EMPTY;
  }

  ephy_location_entry_set_bookmark_icon_state (EPHY_LOCATION_ENTRY (widget), state);
}

static void
sync_tab_popup_windows (EphyWebView *view,
                        GParamSpec  *pspec,
                        EphyWindow  *window)
{
  /* FIXME: show popup count somehow */
}

static void
sync_tab_popups_allowed (EphyWebView *view,
                         GParamSpec  *pspec,
                         EphyWindow  *window)
{
  GActionGroup *action_group;
  GAction *action;
  gboolean allow;

  g_assert (EPHY_IS_WEB_VIEW (view));
  g_assert (EPHY_IS_WINDOW (window));

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "allow-popup-windows");

  g_object_get (view, "popups-allowed", &allow, NULL);

  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (allow));
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

static gboolean
idle_unref_context_event (EphyWindow *window)
{
  LOG ("Idle unreffing context event %p", window->context_event);

  if (window->context_event != NULL) {
    g_object_unref (window->context_event);
    window->context_event = NULL;
  }

  window->idle_worker = 0;
  return FALSE;
}

static void
_ephy_window_set_context_event (EphyWindow     *window,
                                EphyEmbedEvent *event)
{
  if (window->idle_worker != 0) {
    g_source_remove (window->idle_worker);
    window->idle_worker = 0;
  }

  if (window->context_event != NULL) {
    g_object_unref (window->context_event);
  }

  window->context_event = event != NULL ? g_object_ref (event) : NULL;
}

static void
_ephy_window_unset_context_event (EphyWindow *window)
{
  /* Unref the event from idle since we still need it
   * from the action callbacks which will run before idle.
   */
  if (window->idle_worker == 0 && window->context_event != NULL) {
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
  while ((ptr = g_utf8_strchr (ptr, -1, '_')) != NULL) {
    ptrdiff_t pos = ptr - gstring->str;
    g_string_insert (gstring, pos, "_");
    ptr = gstring->str + pos + 2;
  }

  return g_string_free (gstring, FALSE);
}

static char *
format_search_label (const char *search_term)
{
  char *ellipsized = ellipsize_string (search_term, 32);
  char *escaped = mnemonic_escape_string (ellipsized);
  char *label;

  label = g_strdup_printf (_("Search the Web for “%s”"), escaped);

  g_free (ellipsized);
  g_free (escaped);

  return label;
}

static void
add_action_to_context_menu (WebKitContextMenu *context_menu,
                            GActionGroup      *action_group,
                            const char        *action_name,
                            EphyWindow        *window)
{
  GAction *action;
  const char *label;
  char *name;
  char *search_label;
  const char *search_term;
  GVariant *target;

  g_action_parse_detailed_name (action_name, &name, &target, NULL);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), name);
  label = g_hash_table_lookup (window->action_labels, name);
  if (strcmp (label, "search-selection-placeholder") != 0) {
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, _(label), NULL));
  } else {
    search_term = g_variant_get_string (target, NULL);
    search_label = format_search_label (search_term);
    webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, search_label, target));
    g_free (search_label);
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
find_item_in_context_menu (WebKitContextMenu      *context_menu,
                           WebKitContextMenuAction action)
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
parse_context_menu_user_data (WebKitContextMenu *context_menu,
                              const char       **selected_text)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, webkit_context_menu_get_user_data (context_menu));
  g_variant_dict_lookup (&dict, "SelectedText", "&s", selected_text);
}

static gboolean
populate_context_menu (WebKitWebView       *web_view,
                       WebKitContextMenu   *context_menu,
                       GdkEvent            *event,
                       WebKitHitTestResult *hit_test_result,
                       EphyWindow          *window)
{
  WebKitContextMenuItem *input_methods_item = NULL;
  WebKitContextMenuItem *unicode_item = NULL;
  WebKitContextMenuItem *copy_image_item = NULL;
  WebKitContextMenuItem *play_pause_item = NULL;
  WebKitContextMenuItem *mute_item = NULL;
  WebKitContextMenuItem *toggle_controls_item = NULL;
  WebKitContextMenuItem *toggle_loop_item = NULL;
  WebKitContextMenuItem *fullscreen_item = NULL;
  GActionGroup *window_action_group;
  GActionGroup *toolbar_action_group;
  GActionGroup *popup_action_group;
  GList *spelling_guess_items = NULL;
  EphyEmbedEvent *embed_event;
  gboolean app_mode, incognito_mode;
  gboolean is_document = FALSE;
  gboolean is_image = FALSE;
  gboolean is_media = FALSE;
  gboolean is_video = FALSE;
  gboolean is_audio = FALSE;
  gboolean can_search_selection = FALSE;
  char *search_selection_action_name = NULL;
  const char *selected_text = NULL;

  window_action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                                     "win");
  toolbar_action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                                      "toolbar");
  popup_action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                                    "popup");

  if (webkit_hit_test_result_context_is_image (hit_test_result)) {
    is_image = TRUE;
    copy_image_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD);
  }

  if (webkit_hit_test_result_context_is_editable (hit_test_result)) {
    input_methods_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_INPUT_METHODS);
    unicode_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_UNICODE);
    spelling_guess_items = find_spelling_guess_context_menu_items (context_menu);
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

    item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_COPY_VIDEO_LINK_TO_CLIPBOARD);
    if (item) {
      is_video = TRUE;
      g_object_unref (item);
    } else {
      item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_COPY_AUDIO_LINK_TO_CLIPBOARD);
      if (item) {
        is_audio = TRUE;
        g_object_unref (item);
      }
    }
  }

  parse_context_menu_user_data (context_menu, &selected_text);
  if (selected_text) {
    GVariant *value;

    value = g_variant_new_string (selected_text);
    search_selection_action_name = g_action_print_detailed_name ("search-selection",
                                                                 value);
    g_variant_unref (value);
    can_search_selection = TRUE;
  }

  webkit_context_menu_remove_all (context_menu);

  embed_event = ephy_embed_event_new (event, hit_test_result);
  _ephy_window_set_context_event (window, embed_event);
  g_object_unref (embed_event);

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
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-link-in-new-window", window);
      if (!incognito_mode)
        add_action_to_context_menu (context_menu, popup_action_group,
                                    "open-link-in-incognito-window", window);
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
    }

    if (should_show_copy_outside_editable (web_view))
      add_action_to_context_menu (context_menu, window_action_group,
                                  "copy", window);
    if (can_search_selection)
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
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "select-all", window);

    if (can_search_selection)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  search_selection_action_name, window);

    if (input_methods_item || unicode_item)
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
    add_item_to_context_menu (context_menu, input_methods_item);
    add_item_to_context_menu (context_menu, unicode_item);
  } else {
    is_document = TRUE;

    update_edit_actions_sensitivity (window, TRUE);

    if (should_show_copy_outside_editable (web_view))
      add_action_to_context_menu (context_menu, window_action_group, "copy",
                                  window);
    if (can_search_selection)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  search_selection_action_name, window);
    if (should_show_copy_outside_editable (web_view) || can_search_selection)
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());

    if (!is_image && !is_media) {
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
    if (is_video) {
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
    } else if (is_audio) {
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

  g_signal_connect (web_view, "context-menu-dismissed",
                    G_CALLBACK (context_menu_dismissed_cb),
                    window);

  g_free (search_selection_action_name);

  if (is_document && !is_image && !is_media) {
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "send-to", window);
  }

  if (!app_mode) {
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "save-as", window);

    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "page-source", window);
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_from_stock_action (WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT));
  }

  return FALSE;
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
  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    g_object_get (G_OBJECT (window->hit_test_result), "link-uri", &location, NULL);
  }
  /* Note: pressing enter to submit a form synthesizes a mouse
   * click event
   */
  /* shift+click saves the non-link image */
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE &&
           !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
    g_object_get (G_OBJECT (window->hit_test_result), "image-uri", &location, NULL);
  }

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
  if (geometry.x >= 0 && geometry.y >= 0)
    gtk_window_move (GTK_WINDOW (window), geometry.x, geometry.y);

  if (geometry.width > 0 && geometry.height > 0)
    gtk_window_resize (GTK_WINDOW (window), geometry.width, geometry.height);
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
    gtk_editable_set_editable (GTK_EDITABLE (ephy_location_entry_get_entry (lentry)), FALSE);

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

  window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (web_view)));
  parent_view_window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (parent_web_view)));

  using_new_window = window != parent_view_window;

  if (using_new_window) {
    ephy_window_configure_for_view (window, web_view);
    g_signal_emit_by_name (parent_web_view, "new-window", web_view);
  }

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION &&
      !webkit_web_view_get_uri (web_view)) {
    /* Wait until we have a valid URL to decide whether to show the window
     * or load the URL in the default web browser
     */
    g_object_set_data_full (G_OBJECT (window), "referrer",
                            g_strdup (webkit_web_view_get_uri (parent_web_view)),
                            g_free);
    return TRUE;
  }

  gtk_widget_show (GTK_WIDGET (window));

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
                                   flags,
                                   0);
  if (target_window == window)
    gtk_widget_grab_focus (GTK_WIDGET (embed));

  new_web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  g_signal_connect (new_web_view, "ready-to-show",
                    G_CALLBACK (web_view_ready_cb),
                    web_view);

  return new_web_view;
}

typedef struct {
  EphyWindow               *window;
  WebKitWebView            *web_view;
  WebKitPolicyDecision     *decision;
  WebKitPolicyDecisionType  decision_type;
  char                     *request_uri;
} VerifyUrlAsyncData;

static inline VerifyUrlAsyncData *
verify_url_async_data_new (EphyWindow               *window,
                           WebKitWebView            *web_view,
                           WebKitPolicyDecision     *decision,
                           WebKitPolicyDecisionType  decision_type,
                           const char               *request_uri)
{
  VerifyUrlAsyncData *data = g_new (VerifyUrlAsyncData, 1);

  data->window = g_object_ref (window);
  data->web_view = g_object_ref (web_view);
  data->decision = g_object_ref (decision);
  data->decision_type = decision_type;
  data->request_uri = g_strdup (request_uri);

  return data;
}

static inline void
verify_url_async_data_free (VerifyUrlAsyncData *data)
{
  g_object_unref (data->window);
  g_object_unref (data->web_view);
  g_object_unref (data->decision);
  g_free (data->request_uri);
  g_free (data);
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
  const char *uri;
  EphyEmbed *embed;

  g_assert (WEBKIT_IS_WEB_VIEW (web_view));
  g_assert (WEBKIT_IS_NAVIGATION_POLICY_DECISION (decision));
  g_assert (decision_type != WEBKIT_POLICY_DECISION_TYPE_RESPONSE);
  g_assert (EPHY_IS_WINDOW (window));

  navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
  navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  request = webkit_navigation_action_get_request (navigation_action);
  uri = webkit_uri_request_get_uri (request);

  if (!ephy_embed_utils_address_has_web_scheme (uri)) {
    GError *error = NULL;

    gtk_show_uri_on_window (GTK_WINDOW (window), uri, GDK_CURRENT_TIME, &error);

    if (error) {
      LOG ("failed to handle non web scheme: %s", error->message);
      g_error_free (error);

      return FALSE;
    }

    webkit_policy_decision_ignore (decision);

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
      char *referrer;

      referrer = (char *)g_object_get_data (G_OBJECT (window), "referrer");

      if (ephy_embed_utils_urls_have_same_origin (uri, referrer) ||
          ephy_web_application_is_uri_allowed (uri)) {
        gtk_widget_show (GTK_WIDGET (window));
      } else {
        /* We can't get here under flatpak because this code only
         * executes in web app mode.
         */
        ephy_file_open_uri_in_default_browser (uri, GDK_CURRENT_TIME,
                                               gtk_window_get_screen (GTK_WINDOW (window)),
                                               EPHY_FILE_HELPERS_I_UNDERSTAND_I_MUST_NOT_USE_THIS_FUNCTION_UNDER_FLATPAK);
        webkit_policy_decision_ignore (decision);

        gtk_widget_destroy (GTK_WIDGET (window));

        return TRUE;
      }
    }

    if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED ||
        (navigation_type == WEBKIT_NAVIGATION_TYPE_OTHER && webkit_navigation_action_is_user_gesture (navigation_action))) {
      if (ephy_embed_utils_urls_have_same_origin (uri, webkit_web_view_get_uri (web_view)) ||
          ephy_web_application_is_uri_allowed (uri)) {
        return FALSE;
      }

      /* We can't get here under flatpak because this code only
       * executes in web app mode.
       */
      ephy_file_open_uri_in_default_browser (uri, GDK_CURRENT_TIME,
                                             gtk_window_get_screen (GTK_WINDOW (window)),
                                             EPHY_FILE_HELPERS_I_UNDERSTAND_I_MUST_NOT_USE_THIS_FUNCTION_UNDER_FLATPAK);
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
    }
    /* New active tab in existing window for control+shift+click */
    else if (button == GDK_BUTTON_PRIMARY && (state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))) {
      flags |= EPHY_NEW_TAB_APPEND_AFTER;
      inherit_session = TRUE;
    }
    /* Alt+click means download URI */
    else if (button == GDK_BUTTON_PRIMARY && state == GDK_MOD1_MASK) {
      if (save_target_uri (window, web_view)) {
        webkit_policy_decision_ignore (decision);
        return TRUE;
      }
    }
    /* Those were our special cases, we won't handle this */
    else {
      return FALSE;
    }

    embed = ephy_embed_container_get_active_child
              (EPHY_EMBED_CONTAINER (window));

    new_embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
                                         NULL, NULL,
                                         target_window,
                                         embed,
                                         flags,
                                         0);

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

  return FALSE;
}

static void
verify_url_cb (EphyGSBService     *service,
               GAsyncResult       *result,
               VerifyUrlAsyncData *data)
{
  GList *threats = ephy_gsb_service_verify_url_finish (service, result);

  if (threats) {
    webkit_policy_decision_ignore (data->decision);

    /* Very rarely there are URLs that pose multiple types of threats.
     * However, inform the user only about the first threat type.
     */
    ephy_web_view_load_error_page (EPHY_WEB_VIEW (data->web_view),
                                   data->request_uri,
                                   EPHY_WEB_VIEW_ERROR_UNSAFE_BROWSING,
                                   NULL, threats->data);

    g_list_free_full (threats, g_free);
  } else {
    decide_navigation_policy (data->web_view, data->decision,
                              data->decision_type, data->window);
  }

  verify_url_async_data_free (data);
}

static gboolean
decide_policy_cb (WebKitWebView           *web_view,
                  WebKitPolicyDecision    *decision,
                  WebKitPolicyDecisionType decision_type,
                  EphyWindow              *window)
{
  EphyGSBService *service;
  WebKitNavigationPolicyDecision *navigation_decision;
  WebKitNavigationAction *navigation_action;
  WebKitURIRequest *request;
  const char *request_uri;

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
      decision_type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    return FALSE;

  navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
  navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  request = webkit_navigation_action_get_request (navigation_action);
  request_uri = webkit_uri_request_get_uri (request);

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_SAFE_BROWSING)) {
    if (ephy_web_view_get_should_bypass_safe_browsing (EPHY_WEB_VIEW (web_view))) {
      /* This means the user has decided to proceed to an unsafe website. */
      ephy_web_view_set_should_bypass_safe_browsing (EPHY_WEB_VIEW (web_view), FALSE);
      return decide_navigation_policy (web_view, decision, decision_type, window);
    }

    service = ephy_embed_shell_get_global_gsb_service (ephy_embed_shell_get_default ());
    ephy_gsb_service_verify_url (service, request_uri,
                                 (GAsyncReadyCallback)verify_url_cb,
                                 /* Note: this refs the policy decision, so we can complete it asynchronously. */
                                 verify_url_async_data_new (window, web_view,
                                                            decision, decision_type,
                                                            request_uri));
    return TRUE;
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
ephy_window_connect_active_embed (EphyWindow *window)
{
  EphyEmbed *embed;
  WebKitWebView *web_view;
  EphyWebView *view;
  EphyTitleWidget *title_widget;

  g_assert (window->active_embed != NULL);

  embed = window->active_embed;
  view = ephy_embed_get_web_view (embed);
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  ephy_embed_attach_notification_container (window->active_embed);

  sync_tab_security (view, NULL, window);
  sync_tab_document_type (view, NULL, window);
  sync_tab_load_status (view, WEBKIT_LOAD_STARTED, window);
  sync_tab_is_blank (view, NULL, window);
  sync_tab_navigation (view, NULL, window);
  sync_tab_title (embed, NULL, window);
  sync_tab_bookmarked_status (view, NULL, window);
  sync_tab_address (view, NULL, window);
  sync_tab_popup_windows (view, NULL, window);
  sync_tab_popups_allowed (view, NULL, window);

  sync_tab_zoom (web_view, NULL, window);

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  if (EPHY_IS_LOCATION_ENTRY (title_widget)) {
    gdouble progress = webkit_web_view_get_estimated_load_progress (web_view);
    gboolean loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (web_view));

    ephy_location_entry_set_progress (EPHY_LOCATION_ENTRY (title_widget), progress, loading);
    g_signal_connect_object (web_view, "notify::estimated-load-progress",
                             G_CALLBACK (progress_update),
                             window, 0);
  }

  g_signal_connect_object (web_view, "notify::zoom-level",
                           G_CALLBACK (sync_tab_zoom),
                           window, 0);

  g_signal_connect_object (web_view, "create",
                           G_CALLBACK (create_web_view_cb),
                           window, 0);
  g_signal_connect_object (web_view, "decide-policy",
                           G_CALLBACK (decide_policy_cb),
                           window, 0);

  g_signal_connect_object (view, "notify::hidden-popup-count",
                           G_CALLBACK (sync_tab_popup_windows),
                           window, 0);
  g_signal_connect_object (view, "notify::popups-allowed",
                           G_CALLBACK (sync_tab_popups_allowed),
                           window, 0);
  g_signal_connect_object (embed, "notify::title",
                           G_CALLBACK (sync_tab_title),
                           window, 0);
  g_signal_connect_object (view, "notify::address",
                           G_CALLBACK (sync_tab_bookmarked_status),
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
                           G_CALLBACK (sync_tab_load_status),
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

  ephy_mouse_gesture_controller_set_web_view (window->mouse_gesture_controller, web_view);

  g_object_notify (G_OBJECT (window), "active-child");
}

static void
ephy_window_disconnect_active_embed (EphyWindow *window)
{
  EphyEmbed *embed;
  WebKitWebView *web_view;
  EphyWebView *view;

  g_assert (window->active_embed != NULL);

  embed = window->active_embed;
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  view = EPHY_WEB_VIEW (web_view);

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
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_popup_windows),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_popups_allowed),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_security),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_document_type),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_load_status),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_is_blank),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_navigation),
                                        window);
  g_signal_handlers_disconnect_by_func (embed,
                                        G_CALLBACK (sync_tab_title),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (sync_tab_address),
                                        window);

  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (populate_context_menu),
                                        window);
  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (ephy_window_mouse_target_changed_cb),
                                        window);
}

static void
ephy_window_set_active_tab (EphyWindow *window, EphyEmbed *new_embed)
{
  EphyEmbed *old_embed;

  g_assert (EPHY_IS_WINDOW (window));
  g_assert (gtk_widget_get_toplevel (GTK_WIDGET (new_embed)) == GTK_WIDGET (window));

  old_embed = window->active_embed;

  if (old_embed == new_embed)
    return;

  if (old_embed != NULL)
    ephy_window_disconnect_active_embed (window);

  window->active_embed = new_embed;

  if (new_embed != NULL)
    ephy_window_connect_active_embed (window);
}

static void
tab_accels_item_activate (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  const char *action_name;
  int tab_number;

  action_name = g_action_get_name (G_ACTION (action));

  tab_number = atoi (action_name + strlen ("accel-"));

  gtk_notebook_set_current_page (EPHY_WINDOW (user_data)->notebook, tab_number);
}

static void
tab_accels_update (EphyWindow *window)
{
  int n_pages;
  int i = 0;
  GActionGroup *action_group;
  char **actions;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "tab");
  actions = g_action_group_list_actions (action_group);

  n_pages = gtk_notebook_get_n_pages (window->notebook);
  for (i = 0; actions[i] != NULL; i++) {
    if (strstr (actions[i], "accel-") != NULL) {
      GAction *action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                                    actions[i]);
      int tab_number = atoi (actions[i] + strlen ("accel-"));

      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), (tab_number < n_pages));
    }
  }

  g_strfreev (actions);
}

#define TAB_ACCELS_N 10

static void
setup_tab_accels (EphyWindow *window)
{
  GActionGroup *action_group;
  GApplication *app;
  guint i;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "tab");
  app = g_application_get_default ();

  for (i = 0; i < TAB_ACCELS_N; i++) {
    GSimpleAction *simple_action;
    char *action_name;
    char *action_name_with_tab;
    char *accel;

    action_name = g_strdup_printf ("accel-%d", i);
    action_name_with_tab = g_strconcat ("tab.", action_name, NULL);
    accel = g_strdup_printf ("<alt>%d", (i + 1) % TAB_ACCELS_N);

    simple_action = g_simple_action_new (action_name, NULL);

    g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (simple_action));
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           action_name_with_tab,
                                           (const gchar*[]) {accel, NULL});

    g_signal_connect (G_ACTION (simple_action), "activate",
                      G_CALLBACK (tab_accels_item_activate), window);

    g_object_unref (simple_action);
    g_free (accel);
    g_free (action_name);
    g_free (action_name_with_tab);
  }
}

static gboolean
show_notebook_popup_menu (GtkNotebook    *notebook,
                          EphyWindow     *window,
                          GdkEventButton *event)
{
  GtkWidget *menu, *tab, *tab_label;
  GMenu *menu_model;
  GtkBuilder *builder;
  GActionGroup *action_group;
  GAction *action;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/notebook-context-menu.ui");

  menu_model = G_MENU (gtk_builder_get_object (builder, "notebook-menu"));
  menu = gtk_menu_new_from_model (G_MENU_MODEL (menu_model));
  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (window->active_embed), NULL);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "tab");

  if (event != NULL) {
    int n_pages;
    int page_num;
    gboolean pinned;

    tab = GTK_WIDGET (window->active_embed);
    n_pages = gtk_notebook_get_n_pages (notebook);
    page_num = gtk_notebook_page_num (notebook, tab);

    /* enable/disable close others/left/right */
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "close-left");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), page_num > 0);

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "close-right");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), page_num < n_pages - 1);

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "close-others");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), n_pages > 1);

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "reload-all");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), n_pages > 1);

    pinned = ephy_notebook_tab_is_pinned (EPHY_NOTEBOOK (notebook), EPHY_EMBED (tab));
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "pin");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !pinned);

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "unpin");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), pinned);

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "close-tab");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !pinned);

    gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *)event);
  } else {
    tab = GTK_WIDGET (window->active_embed);
    tab_label = gtk_notebook_get_tab_label (notebook, tab);

    /* Not tested, because I don't know how to trigger this code. */
    gtk_menu_popup_at_widget (GTK_MENU (menu),
                              tab_label,
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              NULL);
    gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
  }

  g_object_unref (builder);

  return TRUE;
}

static gboolean
notebook_button_press_cb (GtkNotebook    *notebook,
                          GdkEventButton *event,
                          EphyWindow     *window)
{
  if (GDK_BUTTON_PRESS == event->type && 3 == event->button) {
    return show_notebook_popup_menu (notebook, window, event);
  }

  return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkNotebook *notebook,
                        EphyWindow  *window)
{
  /* Only respond if the notebook is the actual focus */
  if (EPHY_IS_NOTEBOOK (gtk_window_get_focus (GTK_WINDOW (window)))) {
    return show_notebook_popup_menu (notebook, window, NULL);
  }

  return FALSE;
}

static gboolean
present_on_idle_cb (GtkWindow *window)
{
  EphyWindow *ephy_window = EPHY_WINDOW (window);

  gtk_window_present_with_time (window, ephy_window->present_on_insert_user_time);
  ephy_window->present_on_insert_user_time = 0;

  return FALSE;
}

static gboolean
delayed_remove_child (gpointer data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  EphyEmbedContainer *container = EPHY_EMBED_CONTAINER (gtk_widget_get_toplevel (widget));

  ephy_embed_container_remove_child (container, EPHY_EMBED (widget));

  return FALSE;
}

static void
download_only_load_cb (EphyWebView *view,
                       EphyWindow  *window)
{
  if (gtk_notebook_get_n_pages (window->notebook) == 1) {
    ephy_web_view_load_homepage (view);
    return;
  }

  g_idle_add (delayed_remove_child, EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view));
}

static void
update_reader_mode (EphyWindow  *window,
                    EphyWebView *view)
{
  EphyEmbed *embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  EphyWebView *active_view = ephy_embed_get_web_view (embed);
  gboolean available = ephy_web_view_is_reader_mode_available (view);
  GtkWidget *title_widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar)));
  EphyLocationEntry *lentry;

  if (!EPHY_IS_LOCATION_ENTRY (title_widget))
    return;

  if (active_view != view)
    return;

  lentry = EPHY_LOCATION_ENTRY (title_widget);
  ephy_location_entry_set_reader_mode_visible (lentry, available);

  if (available)
    ephy_location_entry_set_reader_mode_state (lentry, ephy_web_view_get_reader_mode_state (view));
}

static void
reader_mode_cb (EphyWebView *view,
                GParamSpec  *pspec,
                EphyWindow  *window)
{
  update_reader_mode (window, view);
}

static void
notebook_page_added_cb (EphyNotebook *notebook,
                        EphyEmbed    *embed,
                        guint         position,
                        EphyWindow   *window)
{
  LOG ("page-added notebook %p embed %p position %u\n", notebook, embed, position);

  g_assert (EPHY_IS_EMBED (embed));

  g_signal_connect_object (ephy_embed_get_web_view (embed), "download-only-load",
                           G_CALLBACK (download_only_load_cb), window, G_CONNECT_AFTER);

  g_signal_connect_object (ephy_embed_get_web_view (embed), "notify::reader-mode",
                           G_CALLBACK (reader_mode_cb), window, G_CONNECT_AFTER);

  if (window->present_on_insert) {
    window->present_on_insert = FALSE;
    g_idle_add ((GSourceFunc)present_on_idle_cb, g_object_ref (window));
  }

  tab_accels_update (window);
}

static void
notebook_page_removed_cb (EphyNotebook *notebook,
                          EphyEmbed    *embed,
                          guint         position,
                          EphyWindow   *window)
{
  LOG ("page-removed notebook %p embed %p position %u\n", notebook, embed, position);

  if (window->closing)
    return;

  g_assert (EPHY_IS_EMBED (embed));

  g_signal_handlers_disconnect_by_func
    (ephy_embed_get_web_view (embed), G_CALLBACK (download_only_load_cb), window);

  tab_accels_update (window);

  if (gtk_notebook_get_n_pages (window->notebook) == 0)
    ephy_window_close (window);
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
   *
   * Even if there is only one close request,
   * tab_has_modified_forms_timeout_cb() will run when the web process
   * has hung, calling ephy_window_close_tab(). Then in the first call
   * to gtk_widget_destroy() below, that will trigger the completion of
   * a stalled call to ephy_web_view_has_modified_forms(), so
   * tab_has_modified_forms_cb() will execute, and that also calls this
   * function.
   *
   * Either way, a second call to ephy_web_view_close_tab() can be
   * triggered recursively before gtk_widget_destroy() has completed.
   * And it turns out that calling gtk_widget_destroy() recursively
   * inside itself is bad. So guard against this.
   */
  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tab), "ephy-window-close-tab-closed")))
    return;

  if (mode != EPHY_EMBED_SHELL_MODE_AUTOMATION)
    keep_window_open = g_settings_get_boolean (EPHY_SETTINGS_UI, EPHY_PREFS_UI_KEEP_WINDOW_OPEN);

  if (keep_window_open && gtk_notebook_get_n_pages (window->notebook) == 1) {
    EphyWebView *view = ephy_embed_get_web_view (tab);

    if (ephy_web_view_get_is_blank (view) || ephy_web_view_is_overview (view))
      return;

    ephy_link_open (EPHY_LINK (window), NULL, NULL, EPHY_LINK_NEW_TAB);
  }

  g_object_set_data (G_OBJECT (tab), "ephy-window-close-tab-closed", GINT_TO_POINTER (TRUE));
  gtk_widget_destroy (GTK_WIDGET (tab));

  /* If that was the last tab, destroy the window.
   *
   * Beware: window->closing could be true now, after destroying the
   * tab, even if it wasn't at the start of this function.
   */
  if (!window->closing && gtk_notebook_get_n_pages (window->notebook) == 0)
    gtk_widget_destroy (GTK_WIDGET (window));
}

typedef struct {
  EphyWindow *window;
  EphyEmbed *embed;
  guint id;
} TabHasModifiedFormsData;

static TabHasModifiedFormsData *
tab_has_modified_forms_data_new (EphyWindow *window,
                                 EphyEmbed  *embed)
{
  TabHasModifiedFormsData *data = g_new (TabHasModifiedFormsData, 1);
  data->window = window;
  data->embed = embed;
  data->id = 0;
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&data->window);
  g_object_add_weak_pointer (G_OBJECT (embed), (gpointer *)&data->embed);
  return data;
}

static void
tab_has_modified_forms_data_free (TabHasModifiedFormsData *data)
{
  g_clear_weak_pointer (&data->window);
  g_clear_weak_pointer (&data->embed);
  g_clear_handle_id (&data->id, g_source_remove);
  g_clear_pointer (&data, g_free);
}

static void
tab_has_modified_forms_cb (EphyWebView             *view,
                           GAsyncResult            *result,
                           TabHasModifiedFormsData *data)
{
  gboolean has_modified_forms;

  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);

  if (data->id != 0) {
    /* Ensure tab doesn't close while waiting for the user. */
    g_source_remove (data->id);
  }

  if (data->id != 0 &&
      data->window != NULL &&
      data->embed != NULL &&
      (!has_modified_forms || confirm_close_with_modified_forms (data->window))) {
    ephy_window_close_tab (data->window, data->embed);
  }

  data->id = 0;
  tab_has_modified_forms_data_free (data);
}

static gboolean
tab_has_modified_forms_timeout_cb (TabHasModifiedFormsData *data)
{
  /* The web process has stalled and ephy_web_view_has_modified_forms()
   * will probably not complete unless we destroy the tab. So do that
   * now. Beware: this will trigger the callback to complete before it
   * returns! So data will be freed during the call to
   * ephy_window_close_tab().
   */
  data->id = 0;
  if (data->window != NULL && data->embed != NULL)
    ephy_window_close_tab (data->window, data->embed);

  return G_SOURCE_REMOVE;
}

static void
notebook_page_close_request_cb (EphyNotebook *notebook,
                                EphyEmbed    *embed,
                                EphyWindow   *window)
{
  if (gtk_notebook_get_n_pages (window->notebook) == 1) {
    if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                                EPHY_PREFS_LOCKDOWN_QUIT)) {
      return;
    }

    if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
      /* Never prompt the user before closing in automation mode */
      ephy_window_close_tab (window, embed);
    }

    /* Last window, check ongoing downloads before closing the tab */
    if (ephy_shell_get_n_windows (ephy_shell_get_default ()) == 1) {
      EphyDownloadsManager *manager = ephy_embed_shell_get_downloads_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

      if (ephy_downloads_manager_has_active_downloads (manager) &&
          !confirm_close_with_downloads (window))
        return;
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
    data = tab_has_modified_forms_data_new (window, embed);
    data->id = g_timeout_add_seconds (1, (GSourceFunc)tab_has_modified_forms_timeout_cb, data);
    ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
                                      NULL,
                                      (GAsyncReadyCallback)tab_has_modified_forms_cb,
                                      data);
  } else {
    ephy_window_close_tab (window, embed);
  }
}

static GtkWidget *
notebook_create_window_cb (GtkNotebook *notebook,
                           GtkWidget   *page,
                           int          x,
                           int          y,
                           EphyWindow  *window)
{
  EphyWindow *new_window;

  new_window = ephy_window_new ();

  new_window->present_on_insert = TRUE;
  new_window->present_on_insert_user_time = gtk_get_current_event_time ();

  return ephy_window_get_notebook (new_window);
}

static EphyEmbed *
real_get_active_tab (EphyWindow *window, int page_num)
{
  GtkWidget *embed;

  if (page_num == -1) {
    page_num = gtk_notebook_get_current_page (window->notebook);
  }

  embed = gtk_notebook_get_nth_page (window->notebook, page_num);

  g_assert (EPHY_IS_EMBED (embed));

  return EPHY_EMBED (embed);
}

static void
notebook_switch_page_cb (GtkNotebook *notebook,
                         GtkWidget   *page,
                         guint        page_num,
                         EphyWindow  *window)
{
  EphyEmbed *embed;
  GActionGroup *group;
  GAction *action;
  EphyWebView *view;

  LOG ("switch-page notebook %p position %u\n", notebook, page_num);

  if (window->closing)
    return;

  /* get the new tab */
  embed = real_get_active_tab (window, page_num);
  view = ephy_embed_get_web_view (embed);

  /* update new tab */
  ephy_window_set_active_tab (window, embed);

  /* update notebook menu */
  group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "show-tab");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_uint32 (page_num));

  update_reader_mode (window, view);
}

static GtkNotebook *
setup_notebook (EphyWindow *window)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (g_object_new (EPHY_TYPE_NOTEBOOK, NULL));

  g_signal_connect_after (notebook, "switch-page",
                          G_CALLBACK (notebook_switch_page_cb),
                          window);
  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (notebook_create_window_cb),
                    window);

  g_signal_connect (notebook, "popup-menu",
                    G_CALLBACK (notebook_popup_menu_cb), window);
  g_signal_connect (notebook, "button-press-event",
                    G_CALLBACK (notebook_button_press_cb), window);

  g_signal_connect (notebook, "page-added",
                    G_CALLBACK (notebook_page_added_cb), window);
  g_signal_connect (notebook, "page-removed",
                    G_CALLBACK (notebook_page_removed_cb), window);
  g_signal_connect (notebook, "tab-close-request",
                    G_CALLBACK (notebook_page_close_request_cb), window);

  g_signal_connect_swapped (notebook, "open-link",
                            G_CALLBACK (ephy_link_open), window);

  return notebook;
}

static void
ephy_window_dispose (GObject *object)
{
  EphyWindow *window = EPHY_WINDOW (object);

  LOG ("EphyWindow dispose %p", window);

  /* Only do these once */
  if (window->closing == FALSE) {
    window->closing = TRUE;

    _ephy_window_set_context_event (window, NULL);

    if (window->last_opened_embed)
      g_clear_weak_pointer ((gpointer *)&window->last_opened_embed);

    g_clear_object (&window->bookmarks_manager);
    g_clear_object (&window->hit_test_result);
    g_clear_object (&window->mouse_gesture_controller);

    g_clear_handle_id (&window->modified_forms_timeout_id, g_source_remove);

    g_hash_table_unref (window->action_labels);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
ephy_window_configure_event (GtkWidget *widget,
                             GdkEventConfigure *event)
{
  EphyWindow *window = EPHY_WINDOW (widget);
  gboolean result;
  gint width, height;

  result = GTK_WIDGET_CLASS (ephy_window_parent_class)->configure_event (widget, event);

  gtk_window_get_size (GTK_WINDOW (widget),
                       &width,
                       &height);

  if (!window->is_maximized && !window->is_fullscreen) {
    gtk_window_get_position (GTK_WINDOW (widget),
                             &window->current_x,
                             &window->current_y);
    window->current_width = width;
    window->current_height = height;
  }

  update_adaptive_mode (window);

  return result;
}

static gboolean
ephy_window_state_event (GtkWidget           *widget,
                         GdkEventWindowState *event)
{
  EphyWindow *window = EPHY_WINDOW (widget);
  gboolean result = GDK_EVENT_PROPAGATE;

  if (GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event) {
    result = GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event (widget, event);
  }

  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    GActionGroup *action_group;
    GAction *action;
    gboolean fullscreen;

    fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

    if (fullscreen) {
      ephy_window_fullscreen (window);
    } else {
      ephy_window_unfullscreen (window);
    }

    if (window->show_fullscreen_header_bar)
      dzl_application_window_set_fullscreen (DZL_APPLICATION_WINDOW (window), fullscreen);

    window->show_fullscreen_header_bar = FALSE;

    action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "fullscreen");

    g_simple_action_set_state (G_SIMPLE_ACTION (action),
                               g_variant_new_boolean (fullscreen));
  } else if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
    window->is_maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
  }

  update_adaptive_mode (window);

  return result;
}

void
ephy_window_set_default_size (EphyWindow *window,
                              gint        width,
                              gint        height)
{
  gtk_window_set_default_size (GTK_WINDOW (window), width, height);
  window->has_default_size = TRUE;
}

void
ephy_window_set_default_position (EphyWindow *window,
                                  gint        x,
                                  gint        y)
{
  gtk_window_move (GTK_WINDOW (window), x, y);
  window->has_default_position = TRUE;
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
    if (!window->has_default_position) {
      g_settings_get (EPHY_SETTINGS_STATE,
                      "window-position", "(ii)",
                      &window->current_x,
                      &window->current_y);
      if (window->current_x >= 0 && window->current_y >= 0) {
        gtk_window_move (GTK_WINDOW (window),
                         window->current_x,
                         window->current_y);
      }

      window->has_default_position = TRUE;
    }

    if (!window->has_default_size) {
      g_settings_get (EPHY_SETTINGS_STATE,
                      "window-size", "(ii)",
                      &window->current_width,
                      &window->current_height);

      if (window->current_width > 0 && window->current_height > 0) {
        gtk_window_resize (GTK_WINDOW (window),
                           window->current_width,
                           window->current_height);
      }

      window->has_default_size = TRUE;
    }
  }

  update_adaptive_mode (window);

  GTK_WIDGET_CLASS (ephy_window_parent_class)->show (widget);
}

static gboolean
ephy_window_should_save_state (EphyWindow *window)
{
  if (window->is_popup)
    return FALSE;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    return TRUE;

  return ephy_profile_dir_is_default ();
}

static void
ephy_window_destroy (GtkWidget *widget)
{
  EphyWindow *window = EPHY_WINDOW (widget);

  if (ephy_window_should_save_state (window)) {
    g_settings_set (EPHY_SETTINGS_STATE,
                    "window-size", "(ii)",
                    window->current_width,
                    window->current_height);
    g_settings_set (EPHY_SETTINGS_STATE,
                    "window-position", "(ii)",
                    window->current_x,
                    window->current_y);
    g_settings_set_boolean (EPHY_SETTINGS_STATE, "is-maximized", window->is_maximized);
  }

  GTK_WIDGET_CLASS (ephy_window_parent_class)->destroy (widget);
}

static void
ephy_window_finalize (GObject *object)
{
  G_OBJECT_CLASS (ephy_window_parent_class)->finalize (object);

  LOG ("EphyWindow finalised %p", object);
}

static void
allow_popups_notifier (GSettings  *settings,
                       char       *key,
                       EphyWindow *window)
{
  GList *tabs;
  EphyEmbed *embed;

  g_assert (EPHY_IS_WINDOW (window));

  tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));

  for (; tabs; tabs = g_list_next (tabs)) {
    embed = EPHY_EMBED (tabs->data);
    g_assert (EPHY_IS_EMBED (embed));

    g_object_notify (G_OBJECT (ephy_embed_get_web_view (embed)), "popups-allowed");
  }
  g_list_free (tabs);
}

static void
sync_user_input_cb (EphyLocationController *action,
                    GParamSpec             *pspec,
                    EphyWindow             *window)
{
  EphyEmbed *embed;
  const char *address;

  LOG ("sync_user_input_cb");

  if (window->updating_address)
    return;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (EPHY_IS_EMBED (embed));

  address = ephy_location_controller_get_address (action);

  window->updating_address = TRUE;
  ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), address);
  window->updating_address = FALSE;
}

static void
title_widget_lock_clicked_cb (EphyTitleWidget *title_widget,
                              GdkRectangle    *lock_position,
                              gpointer         user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyWebView *view;
  const char *address;
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
  GtkWidget *security_popover;

  view = ephy_embed_get_web_view (window->active_embed);
  ephy_web_view_get_security_level (view, &security_level, &address, &certificate, &tls_errors);

  security_popover = ephy_security_popover_new (GTK_WIDGET (title_widget),
                                                address,
                                                certificate,
                                                tls_errors,
                                                security_level);

  g_signal_connect (security_popover, "closed",
                    G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_popover_set_pointing_to (GTK_POPOVER (security_popover), lock_position);
  gtk_popover_set_position (GTK_POPOVER (security_popover), GTK_POS_BOTTOM);
  gtk_popover_popup (GTK_POPOVER (security_popover));
}

static GtkWidget *
setup_header_bar (EphyWindow *window)
{
  GtkWidget *header_bar;
  GtkWidget *tab_header_bar;
  GtkWidget *header_stack;
  EphyTitleWidget *title_widget;

  header_bar = ephy_header_bar_new (window);
  tab_header_bar = ephy_tab_header_bar_new ();
  header_stack = gtk_stack_new ();

  gtk_stack_add_named (GTK_STACK (header_stack), header_bar, "content");
  gtk_stack_add_named (GTK_STACK (header_stack), tab_header_bar, "tabs");

  g_object_bind_property (G_OBJECT (window->main_stack),
                          "visible-child-name", G_OBJECT (header_stack),
                          "visible-child-name", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (window->main_stack),
                          "transition-type", G_OBJECT (header_stack),
                          "transition-type", G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (window->main_stack),
                          "transition-duration", G_OBJECT (header_stack),
                          "transition-duration", G_BINDING_SYNC_CREATE);

  dzl_application_window_set_titlebar (DZL_APPLICATION_WINDOW (window), header_stack);
  gtk_widget_show (header_bar);
  gtk_widget_show (header_stack);

  gtk_stack_set_visible_child_name (GTK_STACK (header_stack), "content");

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (header_bar));
  g_signal_connect (title_widget, "lock-clicked",
                    G_CALLBACK (title_widget_lock_clicked_cb), window);

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

static GtkWidget *
setup_action_bar (EphyWindow *window)
{
  GtkWidget *action_bar;

  action_bar = GTK_WIDGET (ephy_action_bar_new (window));
  gtk_revealer_set_transition_type (GTK_REVEALER (action_bar), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_widget_show (action_bar);

  return action_bar;
}

static const char *disabled_actions_for_app_mode[] = { "open",
                                                       "save-as-application",
                                                       "encoding",
                                                       "bookmark-page",
                                                       "new-tab",
                                                       "home" };

static gboolean
browse_with_caret_get_mapping (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  g_value_set_variant (value, variant);

  return TRUE;
}

static
const gchar *supported_mime_types[] = {
  "x-scheme-handler/http",
  "x-scheme-handler/https",
  "text/html",
  "application/xhtml+xml",
  NULL
};

static void
set_as_default_browser ()
{
  g_autoptr(GError) error = NULL;
  GDesktopAppInfo *desktop_info;
  GAppInfo *info = NULL;
  g_autofree gchar *id = g_strconcat (APPLICATION_ID, ".desktop", NULL);
  int i;

  desktop_info = g_desktop_app_info_new (id);
  if (!desktop_info)
    return;

  info = G_APP_INFO (desktop_info);

  for (i = 0; supported_mime_types[i]; i++) {
    if (!g_app_info_set_as_default_for_type (info, supported_mime_types[i], &error))
      g_warning ("Failed to set '%s' as the default application for secondary content type '%s': %s",
                 g_app_info_get_name (info), supported_mime_types[i], error->message);
    else
      LOG ("Set '%s' as the default application for '%s'",
           g_app_info_get_name (info),
           supported_mime_types[i]);
  }
}

static void
on_default_browser_question_response (GtkInfoBar *info_bar,
                                      gint        response_id,
                                      gpointer    user_data)
{
  if (response_id == GTK_RESPONSE_YES)
    set_as_default_browser ();
  else if (response_id == GTK_RESPONSE_NO)
    g_settings_set_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_ASK_FOR_DEFAULT, FALSE);

  gtk_widget_destroy (GTK_WIDGET (info_bar));
}

static void
add_default_browser_question (GtkBox *box)
{
  GtkWidget *label;
  GtkWidget *info_bar;
  GtkWidget *content_area;

#if !TECH_PREVIEW
  label = gtk_label_new (_("Set Web as your default browser?"));
#else
  label = gtk_label_new (_("Set Epiphany Technology Preview as your default browser?"));
#endif
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_show (label);

  info_bar = gtk_info_bar_new ();
  gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_QUESTION);
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (info_bar), TRUE);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);

  gtk_info_bar_add_button (GTK_INFO_BAR (info_bar), _("_Yes"), GTK_RESPONSE_YES);
  gtk_info_bar_add_button (GTK_INFO_BAR (info_bar), _("_No"), GTK_RESPONSE_NO);

  g_signal_connect (info_bar, "response", G_CALLBACK (on_default_browser_question_response), NULL);

  gtk_box_pack_start (box, info_bar, FALSE, FALSE, 0);

  gtk_widget_show (info_bar);
}

static gboolean
is_browser_default (void)
{
  g_autoptr(GAppInfo) info = g_app_info_get_default_for_type (supported_mime_types[0], TRUE);

  if (info) {
    g_autofree gchar *id = g_strconcat (APPLICATION_ID, ".desktop", NULL);

    if (!strcmp (g_app_info_get_id (info), id))
      return TRUE;
  }

  return FALSE;
}

static void
ephy_window_constructed (GObject *object)
{
  EphyWindow *window;
  GtkBox *box;
  GAction *action;
  GActionGroup *action_group;
  GSimpleActionGroup *simple_action_group;
  guint i;
  EphyShell *shell;
  EphyEmbedShellMode mode;
  EphyWindowChrome chrome = EPHY_WINDOW_CHROME_DEFAULT;
  GApplication *app;

  G_OBJECT_CLASS (ephy_window_parent_class)->constructed (object);

  window = EPHY_WINDOW (object);

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   tab_entries,
                                   G_N_ELEMENTS (tab_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window), "tab",
                                  G_ACTION_GROUP (simple_action_group));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   toolbar_entries,
                                   G_N_ELEMENTS (toolbar_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "toolbar",
                                  G_ACTION_GROUP (simple_action_group));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   popup_entries,
                                   G_N_ELEMENTS (popup_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window),
                                  "popup",
                                  G_ACTION_GROUP (simple_action_group));

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

  ephy_gui_ensure_window_group (GTK_WINDOW (window));

  /* Setup tab accels */
  setup_tab_accels (window);

  window->notebook = setup_notebook (window);
  window->main_stack = gtk_stack_new ();
  window->pages_view = ephy_pages_view_new ();

  ephy_pages_view_set_notebook (window->pages_view, EPHY_NOTEBOOK (window->notebook));

  shell = ephy_shell_get_default ();
  mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (shell));

  /* Setup incognito mode style */
  if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "incognito-mode");
  else if (mode == EPHY_EMBED_SHELL_MODE_AUTOMATION)
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "automation-mode");

  /* Setup the toolbar. */
  window->header_bar = setup_header_bar (window);
  window->location_controller = setup_location_controller (window, EPHY_HEADER_BAR (window->header_bar));
  window->action_bar = setup_action_bar (window);
  box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));

  if (g_settings_get_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_ASK_FOR_DEFAULT) &&
      !is_browser_default () &&
      !ephy_profile_dir_is_web_application ())
    add_default_browser_question (box);

  gtk_box_pack_start (box, GTK_WIDGET (window->notebook), TRUE, TRUE, 0);
  gtk_box_pack_start (box, GTK_WIDGET (window->action_bar), FALSE, TRUE, 0);
  gtk_stack_add_named (GTK_STACK (window->main_stack), GTK_WIDGET (box), "content");
  gtk_stack_add_named (GTK_STACK (window->main_stack), GTK_WIDGET (window->pages_view), "tabs");
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (window->main_stack));
  gtk_widget_show_all (GTK_WIDGET (window->pages_view));
  gtk_widget_show (GTK_WIDGET (window->main_stack));
  gtk_widget_show (GTK_WIDGET (box));
  gtk_widget_show (GTK_WIDGET (window->notebook));

  gtk_stack_set_visible_child_name (GTK_STACK (window->main_stack), "content");
  gtk_stack_set_transition_type (GTK_STACK (window->main_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

  /* other notifiers */
  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           "browse-with-caret");

  g_settings_bind_with_mapping (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_ENABLE_CARET_BROWSING,
                                G_SIMPLE_ACTION (action), "state",
                                G_SETTINGS_BIND_GET|G_SETTINGS_BIND_GET_NO_CHANGES,
                                browse_with_caret_get_mapping,
                                NULL,
                                action, NULL);

  g_signal_connect (EPHY_SETTINGS_WEB,
                    "changed::" EPHY_PREFS_WEB_ENABLE_POPUPS,
                    G_CALLBACK (allow_popups_notifier), window);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

  /* Disable actions not needed for popup mode. */
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        SENS_FLAG_CHROME,
                                        window->is_popup);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "popup");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "open-link-in-new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                        SENS_FLAG_CHROME,
                                        window->is_popup);

  /* Disabled actions not needed for application mode. */
  if (mode == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    g_object_set (window->location_controller, "editable", FALSE, NULL);

    action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "popup");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "context-bookmark-page");
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG_CHROME, TRUE);

    action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
    for (i = 0; i < G_N_ELEMENTS (disabled_actions_for_app_mode); i++) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           disabled_actions_for_app_mode[i]);
      ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                            SENS_FLAG_CHROME, TRUE);
    }
    chrome &= ~(EPHY_WINDOW_CHROME_LOCATION | EPHY_WINDOW_CHROME_TABSBAR | EPHY_WINDOW_CHROME_BOOKMARKS);
  } else if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "bookmark-page");
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_CHROME, TRUE);

    action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "popup");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "context-bookmark-page");
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action),
                                          SENS_FLAG_CHROME, TRUE);
  } else if (mode == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    g_object_set (window->location_controller, "editable", FALSE, NULL);
  }

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "tab");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "reopen");
  if (mode == EPHY_EMBED_SHELL_MODE_INCOGNITO || mode == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  } else {
    g_object_bind_property (G_OBJECT (ephy_shell_get_session (shell)),
                            "can-undo-tab-closed",
                            action,
                            "enabled",
                            G_BINDING_SYNC_CREATE);
  }

  window->mouse_gesture_controller = ephy_mouse_gesture_controller_new (window);

  ephy_window_set_chrome (window, chrome);
}

static void
ephy_window_class_init (EphyWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ephy_window_constructed;
  object_class->dispose = ephy_window_dispose;
  object_class->finalize = ephy_window_finalize;
  object_class->get_property = ephy_window_get_property;
  object_class->set_property = ephy_window_set_property;

  widget_class->key_press_event = ephy_window_key_press_event;
  widget_class->configure_event = ephy_window_configure_event;
  widget_class->window_state_event = ephy_window_state_event;
  widget_class->show = ephy_window_show;
  widget_class->destroy = ephy_window_destroy;
  widget_class->delete_event = ephy_window_delete_event;

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
}

static void
ephy_window_init (EphyWindow *window)
{
  LOG ("EphyWindow initialising %p", window);
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
 * ephy_window_get_notebook:
 * @window: an #EphyWindow
 *
 * Returns the #GtkNotebook used by this window.
 *
 * Return value: (transfer none): the @window's #GtkNotebook
 **/
GtkWidget *
ephy_window_get_notebook (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return GTK_WIDGET (window->notebook);
}

/**
 * ephy_window_get_stack:
 * @window: an #EphyWindow
 *
 * Returns the #GtkStack housing the content and tab views
 *
 * Return value: (transfer none): the @window's #GtkStack
 **/
GtkWidget *
ephy_window_get_stack (EphyWindow *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  return GTK_WIDGET (window->main_stack);
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
  g_assert (url != NULL);

  ephy_link_open (EPHY_LINK (window), url, NULL, 0);
}

/**
 * ephy_window_activate_location:
 * @window: an #EphyWindow
 *
 * Activates the location entry on @window's header bar.
 **/
void
ephy_window_activate_location (EphyWindow *window)
{
  EphyTitleWidget *title_widget;

  if (!(window->chrome & EPHY_WINDOW_CHROME_LOCATION))
    return;

  title_widget = ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (window->header_bar));

  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    ephy_location_entry_focus (EPHY_LOCATION_ENTRY (title_widget));
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
  g_assert (embed != NULL);

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

static void
ephy_window_change_allow_popup_windows_state (GSimpleAction *action,
                                              GVariant      *state,
                                              gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  gboolean allow;

  g_assert (EPHY_IS_WINDOW (window));

  embed = window->active_embed;
  g_assert (EPHY_IS_EMBED (embed));

  allow = g_variant_get_boolean (state);

  g_object_set (G_OBJECT (ephy_embed_get_web_view (embed)), "popups-allowed", allow, NULL);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (allow));
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
EphyEmbedEvent *
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
  GCancellable *cancellable;

  guint embeds_to_check;
  EphyEmbed *modified_embed;
} ModifiedFormsData;

static void
modified_forms_data_free (ModifiedFormsData *data)
{
  g_object_unref (data->cancellable);

  g_free (data);
}

static void
continue_window_close_after_modified_forms_check (ModifiedFormsData *data)
{
  gboolean should_close;

  data->window->checking_modified_forms = FALSE;
  g_clear_handle_id (&data->window->modified_forms_timeout_id, g_source_remove);

  if (data->modified_embed) {
    /* jump to the first tab with modified forms */
    impl_set_active_child (EPHY_EMBED_CONTAINER (data->window),
                           data->modified_embed);
    if (!confirm_close_with_modified_forms (data->window))
      return;
  }

  data->window->force_close = TRUE;
  should_close = ephy_window_close (data->window);
  data->window->force_close = FALSE;
  if (should_close)
    gtk_widget_destroy (GTK_WIDGET (data->window));
}

static void
has_modified_forms_cb (EphyWebView       *view,
                       GAsyncResult      *result,
                       ModifiedFormsData *data)
{
  gboolean has_modified_forms;

  data->embeds_to_check--;
  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
  if (has_modified_forms) {
    /* Cancel all others */
    g_cancellable_cancel (data->cancellable);
    data->modified_embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);
  }

  if (data->embeds_to_check > 0)
    return;

  continue_window_close_after_modified_forms_check (data);
  modified_forms_data_free (data);
}

static gboolean
has_modified_forms_timeout_cb (ModifiedFormsData *data)
{
  data->window->modified_forms_timeout_id = 0;

  /* We have a stalled web process. Using the cancellable is not enough.
   * Nothing for it but to force things. Each web view's
   * has_modified_forms_cb() will complete as that view is being
   * destroyed.
   */
  gtk_widget_destroy (GTK_WIDGET (data->window));

  return G_SOURCE_REMOVE;
}

static void
ephy_window_check_modified_forms (EphyWindow *window)
{
  GList *tabs, *l;
  ModifiedFormsData *data;

  window->checking_modified_forms = TRUE;

  data = g_new0 (ModifiedFormsData, 1);
  data->window = window;
  data->cancellable = g_cancellable_new ();
  data->embeds_to_check = gtk_notebook_get_n_pages (window->notebook);

  tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));
  for (l = tabs; l != NULL; l = l->next) {
    EphyEmbed *embed = (EphyEmbed *)l->data;

    ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
                                      data->cancellable,
                                      (GAsyncReadyCallback)has_modified_forms_cb,
                                      data);
  }

  /* Set timeout to guard against web process hangs. Otherwise, a single
   * unresponsive web process would prevent the window from closing.
   */
  window->modified_forms_timeout_id =
    g_timeout_add_seconds (1, (GSourceFunc)has_modified_forms_timeout_cb, data);

  g_list_free (tabs);
}

/**
 * ephy_window_close:
 * @window: an #EphyWindow
 *
 * Try to close the window. The window might refuse to close
 * if there are ongoing download operations or unsubmitted
 * modifed forms.
 *
 * Returns: %TRUE if the window is closed, or %FALSE otherwise
 **/
gboolean
ephy_window_close (EphyWindow *window)
{
  /* We ignore the delete_event if the disable_quit lockdown has been set
   */
  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_QUIT))
    return FALSE;

  if (window->checking_modified_forms) {
    /* stop window close */
    return FALSE;
  }

  if (!window->force_close &&
      g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                              EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA) &&
      gtk_notebook_get_n_pages (window->notebook) > 0) {
    ephy_window_check_modified_forms (window);
    /* stop window close */
    return FALSE;
  }

  /* If this is the last window, check ongoing downloads and save its state in the session. */
  if (ephy_shell_get_n_windows (ephy_shell_get_default ()) == 1) {
    EphyDownloadsManager *manager = ephy_embed_shell_get_downloads_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
    EphySession *session;

    if (ephy_downloads_manager_has_active_downloads (manager) &&
        !confirm_close_with_downloads (window)) {
      /* stop window close */
      return FALSE;
    }

    session = ephy_shell_get_session (ephy_shell_get_default ());
    if (session)
      ephy_session_close (session);
  }

  /* See bug #114689 */
  gtk_widget_hide (GTK_WIDGET (window));

  return TRUE;
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

int
ephy_window_get_position_for_new_embed (EphyWindow *window,
                                        EphyEmbed  *embed)
{
  GtkWidget *nb = ephy_window_get_notebook (window);
  int position;

  if (embed == window->last_opened_embed)
    return window->last_opened_pos++;

  position = gtk_notebook_page_num (GTK_NOTEBOOK (nb), GTK_WIDGET (embed)) + 1;

  if (window->last_opened_embed)
    g_object_remove_weak_pointer (G_OBJECT (window->last_opened_embed), (gpointer *)&window->last_opened_embed);

  g_object_add_weak_pointer (G_OBJECT (embed), (gpointer *)&window->last_opened_embed);
  window->last_opened_embed = embed;
  window->last_opened_pos = position + 1;

  return position;
}
