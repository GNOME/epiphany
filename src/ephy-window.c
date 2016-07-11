/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2011 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-window.h"

#include "ephy-action-helper.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-gui.h"
#include "ephy-initial-state.h"
#include "ephy-link.h"
#include "ephy-link-action.h"
#include "ephy-location-entry.h"
#include "ephy-notebook.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-security-popover.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"
#include "ephy-toolbar.h"
#include "ephy-type-builtins.h"
#include "ephy-web-view.h"
#include "ephy-zoom.h"
#include "popup-commands.h"
#include "window-commands.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <webkit2/webkit2.h>

#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

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
  const gchar *action_and_target;
  const gchar *accelerators[9];
} accels [] = {
  /* Page Menu accels */
  { "win.new-tab", { "<Primary>T", NULL } },
  { "win.open", { "<Primary>O", NULL } },
  { "win.save-as", { "<shift><Primary>S", "<Primary>S", NULL } },
  { "win.save-as-application", { "<shift><Primary>A", NULL } },
  { "win.undo", { "<Primary>Z", NULL } },
  { "win.redo", { "<shift><Primary>Z", NULL } },
  { "win.copy", { "<Primary>C", NULL } },
  { "win.cut", { "<Primary>X", NULL } },
  { "win.paste", { "<Primary>V", NULL } },
  { "win.zoom-in", { "<Primary>plus", "<Primary>KP_Add", "<Primary>equal", "ZoomIn", NULL } },
  { "win.zoom-out", { "<Primary>minus", "<Primary>KP_Subtract", "ZoomOut", NULL } },
  { "win.zoom-normal", { "<Primary>0", "<Primary>KP_0", NULL } },
  { "win.print", { "<Primary>P", NULL } },
  { "win.find", { "<Primary>F", "Search", NULL } },
  { "win.find-prev", { "<shift><Primary>G", NULL } },
  { "win.find-next", { "<Primary>G", NULL } },
  { "win.bookmark-page", { "<Primary>D", "AddFavorite", NULL } },
  { "win.encoding", { NULL } },
  { "win.page-source", { "<Primary>U", NULL } },
  { "win.toggle-inspector", { "<shift><Primary>I", "F12", NULL } },

  { "win.select-all", { "<Primary>A", NULL } },

  { "win.send-to", { "Send", NULL } },
  { "win.location", { "<Primary>L", "F6", "Go", "OpenURL", NULL } },
  { "win.home", { "<alt>Home", NULL } },

  /* Toggle actions */
  { "win.browse-with-caret", { "F7", NULL } },
  { "win.fullscreen", { "F11", NULL } },
  { "win.allow-popup-windows", { NULL } },

  /* Navigation */
  { "toolbar.stop", { "Escape", "Stop", NULL } },
  { "toolbar.reload", { "<Primary>R", "<shift><Primary>R", "F5", "<Primary>F5", "<shift>F5", "<shift><Primary>F5", "Refresh", "Reload", NULL } },
  { "toolbar.combined-stop-reload", { NULL } },

  /* Tabs */
  { "tab.previous", { "<Primary>Page_Up", "<Primary>KP_9", NULL } },
  { "tab.next", { "<Primary>Page_Down", "<Primary>KP_3", NULL } },
  { "tab.move-left", { "<shift><Primary>Page_Up", "<shift><Primary>Page_Up", NULL } },
  { "tab.move-right", { "<shift><Primary>Page_Down", "<shift><Primary>Page_Down", NULL } },
  { "tab.duplicate", { "<shift><Primary>D", NULL } },
  { "tab.close", { "<Primary>W", NULL } }
}, accels_navigation_ltr [] = {
  { "toolbar.navigation-back", { "<alt>Left", "<alt>KP_Left", "KP_4", "Back", NULL } },
  { "toolbar.navigation-forward", { "<alt>Right", "<alt>KP_Right", "KP_6", "Forward", NULL } }
}, accels_navigation_rtl [] = {
  { "toolbar.navigation-back", { "<alt>Left", "<alt>KP_Left", "KP_6", "Back", NULL } },
  { "toolbar.navigation-forward", { "<alt>Right", "<alt>KP_Right", "KP_4", "Forward", NULL } }
}, *accels_navigation_ltr_rtl;

#define SETTINGS_CONNECTION_DATA_KEY    "EphyWindowSettings"

struct _EphyWindow {
  GtkApplicationWindow parent_instance;

  GtkWidget *toolbar;
  GtkUIManager *manager;
  GHashTable *action_labels;
  GtkActionGroup *toolbar_action_group;
  GtkNotebook *notebook;
  EphyEmbed *active_embed;
  EphyWindowChrome chrome;
  EphyEmbedEvent *context_event;
  WebKitHitTestResult *hit_test_result;
  guint idle_worker;

  EphyLocationController *location_controller;

  guint closing : 1;
  guint has_size : 1;
  guint fullscreen_mode : 1;
  guint is_popup : 1;
  guint present_on_insert : 1;
  guint key_theme_is_emacs : 1;
  guint updating_address : 1;
  guint force_close : 1;
  guint checking_modified_forms : 1;
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

  g_return_val_if_fail (!window->is_popup ||
                        gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) < 1, -1);

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

  page = gtk_notebook_page_num
           (window->notebook, GTK_WIDGET (child));
  gtk_notebook_set_current_page
    (window->notebook, page);
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

  gtk_message_dialog_format_secondary_text
    (GTK_MESSAGE_DIALOG (dialog), "%s", info);

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
                                           _("There are unsubmitted changes to form elements"),
                                           _("If you close the document anyway, "
                                             "you will lose that information."),
                                           _("Close _Document"));
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

static void
ephy_window_embed_container_iface_init (EphyEmbedContainerInterface *iface)
{
  iface->add_child = impl_add_child;
  iface->set_active_child = impl_set_active_child;
  iface->remove_child = impl_remove_child;
  iface->get_active_child = impl_get_active_child;
  iface->get_children = impl_get_children;
  iface->get_is_popup = impl_get_is_popup;
}

static EphyEmbed *
ephy_window_open_link (EphyLink     *link,
                       const char   *address,
                       EphyEmbed    *embed,
                       EphyLinkFlags flags)
{
  EphyWindow *window = EPHY_WINDOW (link);
  EphyEmbed *new_embed;

  g_return_val_if_fail (address != NULL, NULL);

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

    new_embed = ephy_shell_new_tab
                  (ephy_shell_get_default (),
                  target_window,
                  embed, ntflags);
    if (flags & EPHY_LINK_HOME_PAGE) {
      ephy_web_view_load_homepage (ephy_embed_get_web_view (new_embed));
      ephy_window_activate_location (window);
    } else {
      ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), address);
      if (flags & EPHY_LINK_JUMP_TO) {
        gtk_widget_grab_focus (GTK_WIDGET (new_embed));
      }
    }
  } else {
    ephy_web_view_load_url (ephy_embed_get_web_view (embed), address);

    if (address == NULL || address[0] == '\0' || g_str_equal (address, "about:blank")) {
      ephy_window_activate_location (window);
    } else {
      gtk_widget_grab_focus (GTK_WIDGET (embed));
    }

    new_embed = embed;
  }

  return new_embed;
}

static void
ephy_window_link_iface_init (EphyLinkInterface *iface)
{
  iface->open_link = ephy_window_open_link;
}

G_DEFINE_TYPE_WITH_CODE (EphyWindow, ephy_window, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                ephy_window_link_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_CONTAINER,
                                                ephy_window_embed_container_iface_init))

static void
settings_change_notify (GtkSettings *settings,
                        EphyWindow  *window)
{
  char *key_theme_name;

  g_object_get (settings,
                "gtk-key-theme-name", &key_theme_name,
                NULL);

  window->key_theme_is_emacs =
    key_theme_name &&
    g_ascii_strcasecmp (key_theme_name, "Emacs") == 0;

  g_free (key_theme_name);
}

static void
settings_changed_cb (GtkSettings *settings)
{
  GList *list, *l;

  /* FIXME: multi-head */
  list = gtk_window_list_toplevels ();

  for (l = list; l != NULL; l = l->next) {
    if (EPHY_IS_WINDOW (l->data)) {
      settings_change_notify (settings, l->data);
    }
  }

  g_list_free (list);
}

static void
sync_chromes_visibility (EphyWindow *window)
{
  gboolean show_tabsbar;

  if (window->closing)
    return;

  show_tabsbar = (window->chrome & EPHY_WINDOW_CHROME_TABSBAR);

  ephy_notebook_set_tabs_allowed (EPHY_NOTEBOOK (window->notebook),
                                  show_tabsbar && !(window->is_popup || window->fullscreen_mode));
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

  if (window->closing) return;

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
  EphyTitleBox *title_box;
  EphySecurityLevel security_level;

  if (window->closing) return;

  ephy_web_view_get_security_level (view, &security_level, NULL, NULL);
  title_box = ephy_toolbar_get_title_box (EPHY_TOOLBAR (window->toolbar));
  ephy_title_box_set_security_level (title_box, security_level);
}

static void
ephy_window_fullscreen (EphyWindow *window)
{
  EphyEmbed *embed;

  window->fullscreen_mode = TRUE;

  /* sync status */
  embed = window->active_embed;
  sync_tab_load_status (ephy_embed_get_web_view (embed), WEBKIT_LOAD_STARTED, window);
  sync_tab_security (ephy_embed_get_web_view (embed), NULL, window);

  sync_chromes_visibility (window);
  gtk_widget_hide (window->toolbar);
  ephy_embed_entering_fullscreen (embed);
}

static void
ephy_window_unfullscreen (EphyWindow *window)
{
  window->fullscreen_mode = FALSE;

  gtk_widget_show (window->toolbar);
  sync_chromes_visibility (window);
  ephy_embed_leaving_fullscreen (window->active_embed);
}

static gboolean
ephy_window_key_press_event (GtkWidget   *widget,
                             GdkEventKey *event)
{
  EphyWindow *window = EPHY_WINDOW (widget);
  GtkWidget *focus_widget;
  gboolean shortcircuit = FALSE, force_chain = FALSE, handled = FALSE;
  guint modifier = event->state & gtk_accelerator_get_default_mod_mask ();

  /* In an attempt to get the mozembed playing nice with things like emacs keybindings
   * we are passing important events to the focused child widget before letting the window's
   * base handler see them. This is *completely against* stated gtk2 policy but the
   * 'correct' behaviour is exceptionally useless. We need to keep an eye out for
   * unexpected consequences of this decision. IME's should be a high concern, but
   * considering that the IME folks complained about the upside-down event propagation
   * rules, we might be doing them a favour.
   *
   * We achieve this by first evaluating the event to see if it's important, and if
   * so, we get the focus widget and attempt to get the widget to handle that event.
   * If the widget does handle it, we're done (unless force_chain is true, in which
   * case the event is handled as normal in addition to being sent to the focus
   * widget), otherwise the event follows the normal handling path.
   */

  if ((event->state & GDK_CONTROL_MASK ||
       event->state & GDK_MOD1_MASK ||
       event->state & GDK_SHIFT_MASK) &&
      event->length > 0) {
    /* Pass (CTRL|ALT|SHIFT)+letter characters to the widget */
    shortcircuit = TRUE;
  } else if (event->keyval == GDK_KEY_Escape && modifier == 0) {
    /* Always pass Escape to both the widget, and the parent */
    shortcircuit = TRUE;
    force_chain = TRUE;
  } else if (window->key_theme_is_emacs &&
             (modifier == GDK_CONTROL_MASK) &&
             event->length > 0 &&
             /* But don't pass Ctrl+Enter twice */
             event->keyval != GDK_KEY_Return &&
             event->keyval != GDK_KEY_KP_Enter &&
             event->keyval != GDK_KEY_ISO_Enter) {
    /* Pass CTRL+letter characters to the widget */
    shortcircuit = TRUE;
  }

  if (shortcircuit) {
    focus_widget = gtk_window_get_focus (GTK_WINDOW (window));

    if (GTK_IS_WIDGET (focus_widget)) {
      handled = gtk_widget_event (focus_widget,
                                  (GdkEvent *)event);
    }

    if (handled && !force_chain) {
      return handled;
    }
  }

  return GTK_WIDGET_CLASS (ephy_window_parent_class)->key_press_event (widget, event);
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
update_edit_action_sensitivity (EphyWindow *window, const gchar *action_name, gboolean sensitive, gboolean hide)
{
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       action_name);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive || hide);
}

typedef struct {
  EphyWindow *window;
  const gchar *action_name;
  gboolean hide;
} CanEditCommandAsyncData;

static CanEditCommandAsyncData *
can_edit_command_async_data_new (EphyWindow *window, const gchar *action_name, gboolean hide)
{
  CanEditCommandAsyncData *data;

  data = g_slice_new (CanEditCommandAsyncData);
  data->window = g_object_ref (window);
  data->action_name = action_name;
  data->hide = hide;

  return data;
}

static void
can_edit_command_async_data_free (CanEditCommandAsyncData *data)
{
  if (G_UNLIKELY (!data))
    return;

  g_object_unref (data->window);
  g_slice_free (CanEditCommandAsyncData, data);
}

static void
can_edit_command_callback (GObject *object, GAsyncResult *result, CanEditCommandAsyncData *data)
{
  gboolean sensitive;
  GError *error = NULL;

  sensitive = webkit_web_view_can_execute_editing_command_finish (WEBKIT_WEB_VIEW (object), result, &error);
  if (!error) {
    update_edit_action_sensitivity (data->window, data->action_name, sensitive, data->hide);
  } else {
    g_error_free (error);
  }

  can_edit_command_async_data_free (data);
}

static void
update_edit_actions_sensitivity (EphyWindow *window, gboolean hide)
{
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));
  gboolean can_copy, can_cut, can_undo, can_redo, can_paste;

  if (GTK_IS_EDITABLE (widget)) {
    GtkWidget *entry;
    gboolean has_selection;

    entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->toolbar));

    has_selection = gtk_editable_get_selection_bounds
                      (GTK_EDITABLE (widget), NULL, NULL);

    can_copy = has_selection;
    can_cut = has_selection;
    can_paste = TRUE;
    can_undo = ephy_location_entry_get_can_undo (EPHY_LOCATION_ENTRY (entry));
    can_redo = ephy_location_entry_get_can_redo (EPHY_LOCATION_ENTRY (entry));
  } else {
    EphyEmbed *embed;
    WebKitWebView *view;
    CanEditCommandAsyncData *data;

    embed = window->active_embed;
    g_return_if_fail (embed != NULL);

    view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

    data = can_edit_command_async_data_new (window, "copy", hide);
    webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_COPY, NULL,
                                                 (GAsyncReadyCallback)can_edit_command_callback,
                                                 data);
    data = can_edit_command_async_data_new (window, "cut", hide);
    webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_CUT, NULL,
                                                 (GAsyncReadyCallback)can_edit_command_callback,
                                                 data);
    data = can_edit_command_async_data_new (window, "paste", hide);
    webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_PASTE, NULL,
                                                 (GAsyncReadyCallback)can_edit_command_callback,
                                                 data);
    data = can_edit_command_async_data_new (window, "undo", hide);
    webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_UNDO, NULL,
                                                 (GAsyncReadyCallback)can_edit_command_callback,
                                                 data);
    data = can_edit_command_async_data_new (window, "redo", hide);
    webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_REDO, NULL,
                                                 (GAsyncReadyCallback)can_edit_command_callback,
                                                 data);
    return;
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
edit_menu_toggle_cb (GtkWidget  *menu,
                     EphyWindow *window)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (menu)))
    update_edit_actions_sensitivity (window, FALSE);
  else
    enable_edit_actions_sensitivity (window);
}

static void
init_menu_updaters (EphyWindow *window)
{
  GtkWidget *page_menu;

  page_menu = ephy_toolbar_get_page_menu_button (EPHY_TOOLBAR (window->toolbar));

  g_signal_connect (page_menu, "toggled",
                    G_CALLBACK (edit_menu_toggle_cb), window);
}

static const GActionEntry window_entries [] =
{
  { "new-tab", window_cmd_new_tab },
  { "open", window_cmd_open },
  { "save-as", window_cmd_save_as },
  { "save-as-application", window_cmd_save_as_application },
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

  { "select-all", window_cmd_select_all },

  { "send-to", window_cmd_send_to },
  { "location", window_cmd_go_location },
  { "home", window_cmd_go_home },

  /* Toggle actions */
  { "browse-with-caret", NULL, NULL, "false", window_cmd_change_browse_with_caret_state },
  { "fullscreen", NULL, NULL, "false", window_cmd_change_fullscreen_state },
  { "allow-popup-windows", NULL, NULL, "true", ephy_window_change_allow_popup_windows_state }
};

static const GActionEntry tab_entries [] = {
  { "previous", window_cmd_tabs_previous },
  { "next", window_cmd_tabs_next },
  { "move-left", window_cmd_tabs_move_left },
  { "move-right", window_cmd_tabs_move_right },
  { "duplicate", window_cmd_tabs_duplicate },
  { "detach", window_cmd_tabs_detach },
  { "close", window_cmd_tabs_close }
};

static const GActionEntry toolbar_entries [] = {
  { "navigation-back", window_cmd_navigation },
  { "navigation-back-new-tab", window_cmd_navigation_new_tab },
  { "navigation-forward", window_cmd_navigation },
  { "navigation-forward-new-tab", window_cmd_navigation_new_tab },

  { "stop", window_cmd_stop },
  { "reload", window_cmd_reload },
  { "always-stop", window_cmd_stop },
  { "combined-stop-reload", window_cmd_combined_stop_reload, NULL, "false", ephy_toolbar_change_combined_stop_reload_state }
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
  const gchar *action;
  const gchar *label;
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
  { "search-selection", NULL }
};

static void
setup_ui_manager (EphyWindow *window)
{
  GtkAction *action;
  GtkUIManager *manager;

  manager = gtk_ui_manager_new ();

  action =
    g_object_new (EPHY_TYPE_LINK_ACTION,
                  "name", "FileHome",
                  "label", _("Go to most visited"),
                  NULL);
  g_object_unref (action);

  window->manager = manager;
}

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

  const char *action_group_actions[] = { "save-as", "save-as-application",
                                         "zoom-in", "zoom-out", "print",
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

  if (window->closing) return;

  address = ephy_web_view_get_display_address (view);
  typed_address = ephy_web_view_get_typed_address (view);

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_BLANK,
                                              ephy_web_view_is_overview (view));

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

  if (window->closing) return;

  zoom = webkit_web_view_get_zoom_level (web_view);

  if (zoom >= ZOOM_MAXIMAL) {
    can_zoom_in = FALSE;
  }

  if (zoom <= ZOOM_MINIMAL) {
    can_zoom_out = FALSE;
  }

  if (zoom != 1.0) {
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

  if (window->closing) return;

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
_ephy_window_action_set_favicon (EphyWindow *window,
                                 GdkPixbuf  *icon)
{
  g_object_set (window->location_controller, "icon", icon, NULL);
}

static void
sync_tab_icon (EphyWebView *view,
               GParamSpec  *pspec,
               EphyWindow  *window)
{
  GdkPixbuf *icon;

  if (window->closing) return;

  icon = ephy_web_view_get_icon (view);

  _ephy_window_action_set_favicon (window, icon);
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
  if (window->closing) return;

  _ephy_window_set_navigation_flags (window,
                                     ephy_web_view_get_navigation_flags (view));
}

static void
sync_tab_is_blank (EphyWebView *view,
                   GParamSpec  *pspec,
                   EphyWindow  *window)
{
  if (window->closing) return;

  _ephy_window_set_default_actions_sensitive (window,
                                              SENS_FLAG_IS_BLANK,
                                              ephy_web_view_get_is_blank (view));
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

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));
  g_return_if_fail (EPHY_IS_WINDOW (window));

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
  if (window->closing) return;

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

typedef struct {
  GAction *action;
  GVariant *parameter;
} GActionData;

static void
action_activate_cb (GtkAction *action, gpointer user_data)
{
  GActionData *action_data = (GActionData *)user_data;

  g_action_activate (action_data->action, action_data->parameter);

  g_slice_free (GActionData, action_data);
}

static WebKitContextMenuItem*
webkit_context_menu_item_new_from_gaction (GAction *action, const gchar *label)
{
  GtkAction *gtk_action;
  WebKitContextMenuItem *item;
  GActionData *action_data;

  action_data = g_slice_new (GActionData);
  action_data->action = action;
  if (g_action_get_parameter_type (action) != NULL
      && g_variant_type_equal (g_action_get_parameter_type (action), G_VARIANT_TYPE_STRING)) {
    action_data->parameter = g_variant_new_string (label);
  }
  else {
    action_data->parameter = NULL;
  }

  gtk_action = gtk_action_new (g_action_get_name (action), label, NULL, NULL);
  g_signal_connect (gtk_action, "activate",
                    G_CALLBACK (action_activate_cb), action_data);

  g_object_bind_property (action, "enabled",
                          gtk_action, "sensitive",
                          G_BINDING_SYNC_CREATE);

  item = webkit_context_menu_item_new (gtk_action);

  g_object_unref (gtk_action);

  return item;
}

static void
add_action_to_context_menu (WebKitContextMenu *context_menu,
                            GActionGroup      *action_group,
                            const char        *action_name,
                            EphyWindow        *window)
{
  GAction *action;
  const gchar *label;
  gchar *name;
  GVariant *target;

  g_action_parse_detailed_name (action_name, &name, &target, NULL);

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), name);
  label = g_hash_table_lookup (window->action_labels, name);
  if (label == NULL)
    label = g_variant_get_string (target, NULL);

  g_return_if_fail (label != NULL);

  webkit_context_menu_append (context_menu, webkit_context_menu_item_new_from_gaction (action, label));
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
  gboolean is_image;
  gboolean is_media = FALSE;
  gboolean is_video = FALSE;
  gboolean is_audio = FALSE;
  gboolean can_search_selection = FALSE;
  gchar *search_selection_action_name = NULL;
  const gchar *selected_text = NULL;

  window_action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                                     "win");
  toolbar_action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                                      "toolbar");
  popup_action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                                    "popup");

  is_image = webkit_hit_test_result_context_is_image (hit_test_result);

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
    char *ellipsized = ellipsize_string (selected_text, 32);
    if (ellipsized) {
      gchar *label;

      label = g_strdup_printf (_("Search the Web for '%s'"), ellipsized);
      search_selection_action_name = g_action_print_detailed_name ("search-selection",
                                                                   g_variant_new_string (label)),
      can_search_selection = TRUE;

      g_free (label);
    }
  }

  webkit_context_menu_remove_all (context_menu);

  embed_event = ephy_embed_event_new ((GdkEventButton *)event, hit_test_result);
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
    if (input_methods_item || unicode_item)
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
    add_item_to_context_menu (context_menu, input_methods_item);
    add_item_to_context_menu (context_menu, unicode_item);
  } else {
    is_document = TRUE;

    update_edit_actions_sensitivity (window, TRUE);

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

    add_action_to_context_menu (context_menu, window_action_group,
                                "copy", window);
    if (can_search_selection)
      add_action_to_context_menu (context_menu, popup_action_group,
                                  search_selection_action_name, window);

    if (!app_mode && !is_image && !is_media) {
      webkit_context_menu_append (context_menu,
                                  webkit_context_menu_item_new_separator ());
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "context-bookmark-page", window);
    }
  }

  if (is_image) {
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, popup_action_group,
                                "save-image-as", window);
    add_action_to_context_menu (context_menu, popup_action_group,
                                "copy-image-location", window);
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
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-video-in-new-window", window);
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-video-in-new-tab", window);
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "save-video-as", window);
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "copy-video-location", window);
    } else if (is_audio) {
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-audio-in-new-window", window);
      add_action_to_context_menu (context_menu, popup_action_group,
                                  "open-audio-in-new-tab", window);
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

  if (app_mode)
    return FALSE;

  if (is_document && !is_image && !is_media) {
    webkit_context_menu_append (context_menu,
                                webkit_context_menu_item_new_separator ());
    add_action_to_context_menu (context_menu, window_action_group,
                                "send-to", window);
  }

  webkit_context_menu_append (context_menu,
                              webkit_context_menu_item_new_separator ());
  webkit_context_menu_append (context_menu,
                              webkit_context_menu_item_new_from_stock_action (WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT));

  return FALSE;
}

static gboolean
save_target_uri (EphyWindow          *window,
                 WebKitWebView       *view,
                 GdkEventButton      *event,
                 WebKitHitTestResult *hit_test_result)
{
  guint context;
  char *location = NULL;
  gboolean retval = FALSE;

  g_object_get (hit_test_result, "context", &context, NULL);

  LOG ("ephy_window_dom_mouse_click_cb: button %d, context %d, modifier %d (%d:%d)",
       event->button, context, event->state, (int)event->x, (int)event->y);

  /* shift+click saves the link target */
  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    g_object_get (G_OBJECT (hit_test_result), "link-uri", &location, NULL);
  }
  /* Note: pressing enter to submit a form synthesizes a mouse
   * click event
   */
  /* shift+click saves the non-link image */
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE &&
           !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
    g_object_get (G_OBJECT (hit_test_result), "image-uri", &location, NULL);
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

static gboolean
ephy_window_dom_mouse_click_cb (WebKitWebView  *view,
                                GdkEventButton *event,
                                EphyWindow     *window)
{
  WebKitHitTestResult *hit_test_result;
  gboolean handled = FALSE;

  /* Since we're only dealing with shift+click, we can do these
     checks early. */
  if (!(event->state & GDK_SHIFT_MASK) || event->button != GDK_BUTTON_PRIMARY) {
    return FALSE;
  }

  hit_test_result = g_object_ref (window->hit_test_result);
  handled = save_target_uri (window, view, event, hit_test_result);
  g_object_unref (hit_test_result);

  return handled;
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
    chrome |= EPHY_WINDOW_CHROME_TOOLBAR;

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION) {
    GtkWidget *entry;

    entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->toolbar));
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);

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

static gboolean
decide_policy_cb (WebKitWebView           *web_view,
                  WebKitPolicyDecision    *decision,
                  WebKitPolicyDecisionType decision_type,
                  EphyWindow              *window)
{
  WebKitNavigationPolicyDecision *navigation_decision;
  WebKitNavigationAction *navigation_action;
  WebKitNavigationType navigation_type;
  WebKitURIRequest *request;
  const char *uri;
  EphyEmbed *embed;

  if (decision_type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
    return FALSE;

  navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
  navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  request = webkit_navigation_action_get_request (navigation_action);
  uri = webkit_uri_request_get_uri (request);

  if (!ephy_embed_utils_address_has_web_scheme (uri)) {
    GError *error = NULL;
    GdkScreen *screen;

    screen = gtk_widget_get_screen (GTK_WIDGET (web_view));
    gtk_show_uri (screen, uri, GDK_CURRENT_TIME, &error);

    if (error) {
      LOG ("failed to handle non web scheme: %s", error->message);
      g_error_free (error);

      return FALSE;
    }

    webkit_policy_decision_ignore (decision);

    return TRUE;
  }

  if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
    const char *frame_name = webkit_navigation_policy_decision_get_frame_name (navigation_decision);

    if (g_strcmp0 (frame_name, "_evince_download") == 0) {
      /* The Evince Browser Plugin is requesting us to downlod the document */
      webkit_policy_decision_download (decision);
      return TRUE;
    }

    if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_POPUPS) &&
        !webkit_navigation_action_is_user_gesture (navigation_action)) {
      webkit_policy_decision_ignore (decision);
      return TRUE;
    }
  }

  navigation_type = webkit_navigation_action_get_navigation_type (navigation_action);

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    if (!gtk_widget_is_visible (GTK_WIDGET (window))) {
      char *referrer;

      referrer = (char *)g_object_get_data (G_OBJECT (window), "referrer");

      if (ephy_embed_utils_urls_have_same_origin (uri, referrer)) {
        gtk_widget_show (GTK_WIDGET (window));
      } else {
        ephy_file_open_uri_in_default_browser (uri, GDK_CURRENT_TIME,
                                               gtk_window_get_screen (GTK_WINDOW (window)));
        webkit_policy_decision_ignore (decision);

        gtk_widget_destroy (GTK_WIDGET (window));

        return TRUE;
      }
    }

    if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED) {
      if (ephy_embed_utils_urls_have_same_origin (uri, webkit_web_view_get_uri (web_view))) {
        return FALSE;
      }

      ephy_file_open_uri_in_default_browser (uri, GDK_CURRENT_TIME,
                                             gtk_window_get_screen (GTK_WINDOW (window)));
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

    /* New tab in new window for control+shift+click */
    if (button == 1 && state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK) &&
        !g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                                 EPHY_PREFS_LOCKDOWN_FULLSCREEN)) {
      target_window = ephy_window_new ();
    }
    /* New tab in existing window for middle click and
     * control+click */
    else if (button == 2 || (button == 1 && state == GDK_CONTROL_MASK)) {
      flags |= EPHY_NEW_TAB_APPEND_AFTER;
      inherit_session = TRUE;
    }
    /* Because we connect to button-press-event *after*
     * (G_CONNECT_AFTER) we need to prevent WebKit from browsing to
     * a link when you shift+click it. Otherwise when you
     * shift+click a link to download it you would also be taken to
     * the link destination. */
    else if (button == 1 && state == GDK_SHIFT_MASK) {
      webkit_policy_decision_ignore (decision);

      return TRUE;
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
    }
    ephy_web_view_load_request (new_view, request);

    webkit_policy_decision_ignore (decision);

    return TRUE;
  }

  return FALSE;
}

static void
ephy_window_connect_active_embed (EphyWindow *window)
{
  EphyEmbed *embed;
  WebKitWebView *web_view;
  EphyWebView *view;

  g_return_if_fail (window->active_embed != NULL);

  embed = window->active_embed;
  view = ephy_embed_get_web_view (embed);
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  sync_tab_security (view, NULL, window);
  sync_tab_document_type (view, NULL, window);
  sync_tab_load_status (view, WEBKIT_LOAD_STARTED, window);
  sync_tab_is_blank (view, NULL, window);
  sync_tab_navigation (view, NULL, window);
  sync_tab_title (embed, NULL, window);
  sync_tab_address (view, NULL, window);
  sync_tab_icon (view, NULL, window);
  sync_tab_popup_windows (view, NULL, window);
  sync_tab_popups_allowed (view, NULL, window);

  sync_tab_zoom (web_view, NULL, window);

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
                           G_CALLBACK (sync_tab_address),
                           window, 0);
  g_signal_connect_object (view, "notify::icon",
                           G_CALLBACK (sync_tab_icon),
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
  g_signal_connect_object (view, "button-press-event",
                           G_CALLBACK (ephy_window_dom_mouse_click_cb),
                           window, 0);
  g_signal_connect_object (view, "context-menu",
                           G_CALLBACK (populate_context_menu),
                           window, 0);
  g_signal_connect_object (view, "mouse-target-changed",
                           G_CALLBACK (ephy_window_mouse_target_changed_cb),
                           window, 0);

  g_object_notify (G_OBJECT (window), "active-child");
}

static void
ephy_window_disconnect_active_embed (EphyWindow *window)
{
  EphyEmbed *embed;
  WebKitWebView *web_view;
  EphyWebView *view;

  g_return_if_fail (window->active_embed != NULL);

  embed = window->active_embed;
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  view = EPHY_WEB_VIEW (web_view);

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
                                        G_CALLBACK (sync_tab_icon),
                                        window);

  g_signal_handlers_disconnect_by_func
    (view, G_CALLBACK (ephy_window_dom_mouse_click_cb), window);
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

  g_return_if_fail (EPHY_IS_WINDOW (window));
  g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (new_embed)) == GTK_WIDGET (window));

  old_embed = window->active_embed;

  if (old_embed == new_embed) return;

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
  const gchar *action_name;
  int tab_number;

  action_name = g_action_get_name (G_ACTION (action));

  tab_number = atoi (action_name + strlen ("accel-"));

  gtk_notebook_set_current_page (EPHY_WINDOW (user_data)->notebook, tab_number);
}

static void
tab_accels_update (EphyWindow *window)
{
  gint n_pages, i = 0;
  GActionGroup *action_group;
  gchar **actions;

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
    char *accel;

    action_name = g_strdup_printf ("accel-%d", i);
    accel = g_strdup_printf ("<alt>%d", (i + 1) % TAB_ACCELS_N);

    simple_action = g_simple_action_new (action_name, NULL);

    g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (simple_action));
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           g_strconcat ("tab.", action_name, NULL),
                                           (const gchar*[]) {accel, NULL});

    g_signal_connect (G_ACTION (simple_action), "activate",
                      G_CALLBACK (tab_accels_item_activate), window);

    g_object_unref (simple_action);
    g_free (accel);
    g_free (action_name);
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

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/menus.ui");
  if (!builder) {
    g_object_unref (builder);
    return FALSE;
  }

  menu_model = G_MENU (gtk_builder_get_object (builder, "notebook-menu"));
  menu = gtk_menu_new_from_model (G_MENU_MODEL (menu_model));
  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (window->active_embed), NULL);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "tab");

  if (event != NULL) {
    gint n_pages, page_num;

    tab = GTK_WIDGET (window->active_embed);
    n_pages = gtk_notebook_get_n_pages (notebook);
    page_num = gtk_notebook_page_num (notebook, tab);

    /* enable/disable move left/right items*/
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "move-left");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), page_num > 0);

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                         "move-right");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), page_num < n_pages - 1);

    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    NULL, NULL,
                    event->button, event->time);
  } else {
    tab = GTK_WIDGET (window->active_embed);
    tab_label = gtk_notebook_get_tab_label (notebook, tab);

    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    ephy_gui_menu_position_under_widget, tab_label,
                    0, gtk_get_current_event_time ());
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
  gtk_window_present (window);
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
notebook_page_added_cb (EphyNotebook *notebook,
                        EphyEmbed    *embed,
                        guint         position,
                        EphyWindow   *window)
{
  LOG ("page-added notebook %p embed %p position %u\n", notebook, embed, position);

  g_return_if_fail (EPHY_IS_EMBED (embed));

#if 0
  g_signal_connect_object (embed, "open-link",
                           G_CALLBACK (ephy_link_open), window,
                           G_CONNECT_SWAPPED);
#endif

  g_signal_connect_object (ephy_embed_get_web_view (embed), "download-only-load",
                           G_CALLBACK (download_only_load_cb), window, G_CONNECT_AFTER);

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

  if (window->closing) return;

  g_return_if_fail (EPHY_IS_EMBED (embed));

#if 0
  g_signal_handlers_disconnect_by_func (G_OBJECT (embed),
                                        G_CALLBACK (ephy_link_open),
                                        window);
#endif

  g_signal_handlers_disconnect_by_func
    (ephy_embed_get_web_view (embed), G_CALLBACK (download_only_load_cb), window);

  tab_accels_update (window);
}

static void
ephy_window_close_tab (EphyWindow *window,
                       EphyEmbed  *tab)
{
  gtk_widget_destroy (GTK_WIDGET (tab));

  /* If that was the last tab, destroy the window. */
  if (gtk_notebook_get_n_pages (window->notebook) == 0) {
    gtk_widget_destroy (GTK_WIDGET (window));
  }
}

static void
tab_has_modified_forms_cb (EphyWebView  *view,
                           GAsyncResult *result,
                           EphyWindow   *window)
{
  gboolean has_modified_forms;

  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
  if (!has_modified_forms || confirm_close_with_modified_forms (window)) {
    ephy_window_close_tab (window, EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view));
  }
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
    ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
                                      NULL,
                                      (GAsyncReadyCallback)tab_has_modified_forms_cb,
                                      window);
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

  g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

  return EPHY_EMBED (embed);
}

static void
notebook_switch_page_cb (GtkNotebook *notebook,
                         GtkWidget   *page,
                         guint        page_num,
                         EphyWindow  *window)
{
  EphyEmbed *embed;

  LOG ("switch-page notebook %p position %u\n", notebook, page_num);

  if (window->closing) return;

  /* get the new tab */
  embed = real_get_active_tab (window, page_num);

  /* update new tab */
  ephy_window_set_active_tab (window, embed);

  ephy_title_box_set_web_view (ephy_toolbar_get_title_box (EPHY_TOOLBAR (window->toolbar)),
                               EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
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
  GSList *popups;

  LOG ("EphyWindow dispose %p", window);

  /* Only do these once */
  if (window->closing == FALSE) {
    window->closing = TRUE;

    ephy_bookmarks_ui_detach_window (window);

    /* Deactivate menus */
    popups = gtk_ui_manager_get_toplevels (window->manager, GTK_UI_MANAGER_POPUP);
    g_slist_foreach (popups, (GFunc)gtk_menu_shell_deactivate, NULL);
    g_slist_free (popups);

    g_object_unref (window->manager);
    window->manager = NULL;

    _ephy_window_set_context_event (window, NULL);

    g_clear_object (&window->hit_test_result);

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
ephy_window_state_event (GtkWidget           *widget,
                         GdkEventWindowState *event)
{
  EphyWindow *window = EPHY_WINDOW (widget);

  if (GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event) {
    GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event (widget, event);
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

    action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "fullscreen");

    g_simple_action_set_state (G_SIMPLE_ACTION (action),
                               g_variant_new_boolean (fullscreen));
  }

  return FALSE;
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

  g_return_if_fail (EPHY_IS_WINDOW (window));

  tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));

  for (; tabs; tabs = g_list_next (tabs)) {
    embed = EPHY_EMBED (tabs->data);
    g_return_if_fail (EPHY_IS_EMBED (embed));

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

  if (window->updating_address) return;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (EPHY_IS_EMBED (embed));

  address = ephy_location_controller_get_address (action);

  window->updating_address = TRUE;
  ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), address);
  window->updating_address = FALSE;
}

static void
open_security_popover (EphyWindow   *window,
                       GtkWidget    *relative_to,
                       GdkRectangle *lock_position)
{
  EphyWebView *view;
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
  GtkWidget *location_entry;
  GtkWidget *security_popover;

  view = ephy_embed_get_web_view (window->active_embed);
  ephy_web_view_get_security_level (view, &security_level, &certificate, &tls_errors);
  location_entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->toolbar));

  security_popover = ephy_security_popover_new (relative_to,
                                                ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (location_entry)),
                                                certificate,
                                                tls_errors,
                                                security_level);

  g_signal_connect (security_popover, "closed",
                    G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_popover_set_pointing_to (GTK_POPOVER (security_popover), lock_position);
  gtk_popover_set_position (GTK_POPOVER (security_popover), GTK_POS_BOTTOM);
  gtk_widget_show (security_popover);
}

static void
location_controller_lock_clicked_cb (EphyLocationController *controller,
                                     gpointer                user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkWidget *location_entry;
  GdkRectangle lock_position;

  location_entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->toolbar));
  gtk_entry_get_icon_area (GTK_ENTRY (location_entry), GTK_ENTRY_ICON_SECONDARY, &lock_position);
  open_security_popover (window, location_entry, &lock_position);
}

static void
title_box_lock_clicked_cb (EphyTitleBox *title_box,
                           GdkRectangle *lock_position,
                           gpointer      user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);

  open_security_popover (window, GTK_WIDGET (title_box), lock_position);
}

static GtkWidget *
setup_toolbar (EphyWindow *window)
{
  GtkWidget *toolbar;
  EphyEmbedShellMode app_mode;
  EphyTitleBox *title_box;

  toolbar = ephy_toolbar_new (window);
  gtk_window_set_titlebar (GTK_WINDOW (window), toolbar);
  gtk_widget_show (toolbar);

  app_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());
  if (app_mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    gtk_style_context_add_class (gtk_widget_get_style_context (toolbar), "incognito-mode");

  title_box = ephy_toolbar_get_title_box (EPHY_TOOLBAR (toolbar));
  g_signal_connect (title_box, "lock-clicked",
                    G_CALLBACK (title_box_lock_clicked_cb), window);

  return toolbar;
}

static EphyLocationController *
setup_location_controller (EphyWindow  *window,
                           EphyToolbar *toolbar)
{
  EphyLocationController *location_controller;

  location_controller =
    g_object_new (EPHY_TYPE_LOCATION_CONTROLLER,
                  "window", window,
                  "location-entry", ephy_toolbar_get_location_entry (toolbar),
                  "title-box", ephy_toolbar_get_title_box (toolbar),
                  NULL);
  g_signal_connect (location_controller, "notify::address",
                    G_CALLBACK (sync_user_input_cb), window);
  g_signal_connect_swapped (location_controller, "open-link",
                            G_CALLBACK (ephy_link_open), window);
  g_signal_connect (location_controller, "lock-clicked",
                    G_CALLBACK (location_controller_lock_clicked_cb), window);

  return location_controller;
}

static const char *disabled_actions_for_app_mode[] = { "open",
                                                       "save-as",
                                                       "save-as-application",
                                                       "encoding",
                                                       "bookmark-page",
                                                       "page-source",
                                                       "toggle-inspector" };

static void
parse_css_error (GtkCssProvider *provider,
                 GtkCssSection  *section,
                 GError         *error,
                 gpointer        user_data)
{
  g_warning ("CSS error in section beginning line %u at offset %u:\n %s",
             gtk_css_section_get_start_line (section) + 1,
             gtk_css_section_get_start_position (section),
             error->message);
}

static gboolean
browse_with_caret_get_mapping (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  g_value_set_variant (value, variant);

  return TRUE;
}

static GObject *
ephy_window_constructor (GType                  type,
                         guint                  n_construct_properties,
                         GObjectConstructParam *construct_params)
{
  GObject *object;
  EphyWindow *window;
  GtkSettings *settings;
  GAction *new_action;
  GActionGroup *action_group;
  GSimpleActionGroup *simple_action_group;

  guint settings_connection;
  GtkCssProvider *css_provider;
  guint i;
  EphyEmbedShellMode mode;
  EphyWindowChrome chrome = EPHY_WINDOW_CHROME_DEFAULT;
  GApplication *app;

  object = G_OBJECT_CLASS (ephy_window_parent_class)->constructor
             (type, n_construct_properties, construct_params);

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

  /* initialize the listener for the key theme
   * FIXME: Need to handle multi-head and migration.
   */
  settings = gtk_settings_get_default ();
  settings_connection = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (settings),
                                                             SETTINGS_CONNECTION_DATA_KEY));
  if (settings_connection == 0) {
    settings_connection =
      g_signal_connect (settings, "notify::gtk-key-theme-name",
                        G_CALLBACK (settings_changed_cb), NULL);
    g_object_set_data (G_OBJECT (settings), SETTINGS_CONNECTION_DATA_KEY,
                       GUINT_TO_POINTER (settings_connection));
  }

  settings_change_notify (settings, window);

  /* Setup the UI manager and connect verbs */
  setup_ui_manager (window);
  setup_tab_accels (window);

  window->notebook = setup_notebook (window);

  /* Setup the toolbar. */
  window->toolbar = setup_toolbar (window);
  window->location_controller = setup_location_controller (window, EPHY_TOOLBAR (window->toolbar));
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (window->notebook));
  gtk_widget_show (GTK_WIDGET (window->notebook));

  /* Attach the CSS provider to the window. */
  css_provider = gtk_css_provider_new ();
  g_signal_connect (css_provider,
                    "parsing-error",
                    G_CALLBACK (parse_css_error), window);
  gtk_css_provider_load_from_resource (css_provider, "/org/gnome/epiphany/epiphany.css");
  gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (window)),
                                             GTK_STYLE_PROVIDER (css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  /* Initialize the menus */
  ephy_bookmarks_ui_attach_window (window);

  /* other notifiers */
  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
  new_action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "browse-with-caret");

  g_settings_bind_with_mapping (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_ENABLE_CARET_BROWSING,
                                G_SIMPLE_ACTION (new_action), "state",
                                G_SETTINGS_BIND_GET|G_SETTINGS_BIND_GET_NO_CHANGES,
                                browse_with_caret_get_mapping,
                                NULL,
                                new_action, NULL);

  g_signal_connect (EPHY_SETTINGS_WEB,
                    "changed::" EPHY_PREFS_WEB_ENABLE_POPUPS,
                    G_CALLBACK (allow_popups_notifier), window);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window),
                                              "win");

  /* Disable actions not needed for popup mode. */
  new_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (new_action),
                                        SENS_FLAG_CHROME,
                                        window->is_popup);

  action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "popup");
  new_action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "open-link-in-new-tab");
  ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (new_action),
                                        SENS_FLAG_CHROME,
                                        window->is_popup);

  /* Disabled actions not needed for application mode. */
  mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());
  if (mode == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    g_object_set (window->location_controller, "editable", FALSE, NULL);

    /* We don't need to show the page menu and the new tab button in web
     * application mode.
     */
    gtk_widget_set_visible (ephy_toolbar_get_page_menu_button (EPHY_TOOLBAR (window->toolbar)), FALSE);
    gtk_widget_set_visible (ephy_toolbar_get_new_tab_button (EPHY_TOOLBAR (window->toolbar)), FALSE);

    new_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "context-bookmark-page");
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (new_action), SENS_FLAG_CHROME, TRUE);

    for (i = 0; i < G_N_ELEMENTS (disabled_actions_for_app_mode); i++) {
      new_action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           disabled_actions_for_app_mode[i]);
      ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (new_action),
                                            SENS_FLAG_CHROME, TRUE);
    }
    chrome &= ~(EPHY_WINDOW_CHROME_MENU | EPHY_WINDOW_CHROME_TABSBAR);
  }

  /* We never want the menubar shown, we merge the app menu into
   * our super menu manually when running outside the Shell. */
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), FALSE);

  /* ensure the UI is updated */
  gtk_ui_manager_ensure_update (window->manager);

  init_menu_updaters (window);

  ephy_window_set_chrome (window, chrome);

  return object;
}

static void
ephy_window_show (GtkWidget *widget)
{
  EphyWindow *window = EPHY_WINDOW (widget);

  if (!window->has_size) {
    EphyEmbed *embed;
    int flags = 0;

    embed = window->active_embed;
    g_return_if_fail (EPHY_IS_EMBED (embed));

    if (!window->is_popup)
      flags = EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE;

    ephy_initial_state_add_window (widget, "main_window", 600, 500,
                                   TRUE, flags);
    window->has_size = TRUE;
  }

  GTK_WIDGET_CLASS (ephy_window_parent_class)->show (widget);
}

static void
ephy_window_class_init (EphyWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructor = ephy_window_constructor;
  object_class->dispose = ephy_window_dispose;
  object_class->finalize = ephy_window_finalize;
  object_class->get_property = ephy_window_get_property;
  object_class->set_property = ephy_window_set_property;

  widget_class->show = ephy_window_show;
  widget_class->key_press_event = ephy_window_key_press_event;
  widget_class->window_state_event = ephy_window_state_event;
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
                       "icon-name", "web-browser",
                       NULL);
}

/**
 * ephy_window_get_ui_manager:
 * @window: an #EphyWindow
 *
 * Returns this window's UI manager.
 *
 * Return value: (transfer none): an #GtkUIManager
 **/
GtkUIManager *
ephy_window_get_ui_manager (EphyWindow *window)
{
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

  return window->manager;
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
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

  return GTK_WIDGET (window->notebook);
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
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

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
  g_return_if_fail (url != NULL);

  ephy_link_open (EPHY_LINK (window), url, NULL, 0);
}

/**
 * ephy_window_activate_location:
 * @window: an #EphyWindow
 *
 * Activates the location entry on @window's toolbar.
 **/
void
ephy_window_activate_location (EphyWindow *window)
{
  GtkWidget *entry;

  if (!(window->chrome & EPHY_WINDOW_CHROME_LOCATION))
    return;

  ephy_title_box_set_mode (ephy_toolbar_get_title_box (EPHY_TOOLBAR (window->toolbar)),
                           EPHY_TITLE_BOX_MODE_LOCATION_ENTRY);

  entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->toolbar));
  ephy_location_entry_activate (EPHY_LOCATION_ENTRY (entry));
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

  g_return_if_fail (EPHY_IS_WINDOW (window));

  embed = window->active_embed;
  g_return_if_fail (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  current_zoom = webkit_web_view_get_zoom_level (web_view);

  if (zoom == ZOOM_IN) {
    zoom = ephy_zoom_get_changed_zoom_level (current_zoom, 1);
  } else if (zoom == ZOOM_OUT) {
    zoom = ephy_zoom_get_changed_zoom_level (current_zoom, -1);
  }

  if (zoom != current_zoom) {
    webkit_web_view_set_zoom_level (web_view, zoom);
  }
}

static void
ephy_window_change_allow_popup_windows_state (GSimpleAction *action,
                                              GVariant      *state,
                                              gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  gboolean allow;

  g_return_if_fail (EPHY_IS_WINDOW (window));

  embed = window->active_embed;
  g_return_if_fail (EPHY_IS_EMBED (embed));

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
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

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
  if (window->updating_address) return;

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
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

  return window->location_controller;
}

/**
 * ephy_window_get_toolbar:
 * @window: an #EphyWindow
 *
 * Returns the @window #EphyToolbar
 *
 * Returns: (transfer none): the @window #EphyToolbar
 **/
GtkWidget *
ephy_window_get_toolbar (EphyWindow *window)
{
  return window->toolbar;
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

  g_slice_free (ModifiedFormsData, data);
}

static void
continue_window_close_after_modified_forms_check (ModifiedFormsData *data)
{
  gboolean should_close;

  data->window->checking_modified_forms = FALSE;

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

static void
ephy_window_check_modified_forms (EphyWindow *window)
{
  GList *tabs, *l;
  ModifiedFormsData *data;

  window->checking_modified_forms = TRUE;

  data = g_slice_new0 (ModifiedFormsData);
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
                              EPHY_PREFS_LOCKDOWN_QUIT)) return FALSE;

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

    if (ephy_downloads_manager_has_active_downloads (manager) &&
        !confirm_close_with_downloads (window)) {
      /* stop window close */
      return FALSE;
    }

    ephy_session_close (ephy_shell_get_session (ephy_shell_get_default ()));
  }

  /* See bug #114689 */
  gtk_widget_hide (GTK_WIDGET (window));

  return TRUE;
}

EphyWindowChrome
ephy_window_get_chrome (EphyWindow *window)
{
  g_return_val_if_fail (EPHY_IS_WINDOW (window), EPHY_WINDOW_CHROME_DEFAULT);

  return window->chrome;
}
