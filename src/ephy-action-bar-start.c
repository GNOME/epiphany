/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Purism SPC
 *  Copyright © 2018 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "ephy-action-bar-start.h"

#include "ephy-desktop-utils.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-helpers.h"
#include "ephy-file-helpers.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-window.h"

/* Translators: tooltip for the refresh button */
static const char *REFRESH_BUTTON_TOOLTIP = N_("Reload");

struct _EphyActionBarStart {
  GtkBox parent_instance;

  GtkWidget *navigation_box;
  GtkWidget *navigation_back;
  GtkWidget *navigation_forward;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *homepage_button;
  GtkWidget *new_tab_button;
  GtkWidget *placeholder;

  GtkWidget *history_menu;

  GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (EphyActionBarStart, ephy_action_bar_start, GTK_TYPE_BOX)

typedef enum {
  EPHY_NAVIGATION_HISTORY_DIRECTION_BACK,
  EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD
} EphyNavigationHistoryDirection;

typedef enum {
  WEBKIT_HISTORY_BACKWARD,
  WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;

#define MAX_LABEL_LENGTH 48
#define HISTORY_ITEM_DATA_KEY "history-item-data-key"

static void
history_row_enter_cb (GtkEventController *controller,
                      double              x,
                      double              y,
                      EphyActionBarStart *action_bar_start)
{
  GtkWidget *widget;
  const char *text;
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;

  widget = gtk_event_controller_get_widget (controller);
  text = g_object_get_data (G_OBJECT (widget), "link-message");

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_start));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  ephy_web_view_set_link_message (EPHY_WEB_VIEW (web_view), text);
}

static void
history_row_leave_cb (GtkEventController *controller,
                      EphyActionBarStart *action_bar_start)
{
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_start));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  ephy_web_view_set_link_message (EPHY_WEB_VIEW (web_view), NULL);
}

static void
middle_click_handle_on_history_menu_item (EphyEmbed                 *embed,
                                          WebKitBackForwardListItem *item)
{
  EphyEmbed *new_embed = NULL;
  EphyNewTabFlags flags = 0;
  const gchar *url;

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
    flags |= EPHY_NEW_TAB_JUMP;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
                                  embed,
                                  flags);
  g_assert (new_embed);

  /* Load the new URL */
  url = webkit_back_forward_list_item_get_original_uri (item);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), url);
}

static void
history_row_released_cb (GtkGesture         *gesture,
                         int                 n_click,
                         double              x,
                         double              y,
                         EphyActionBarStart *action_bar_start)
{
  WebKitBackForwardListItem *item;
  GtkWidget *widget;
  guint button;
  GtkRoot *root;
  EphyEmbed *embed;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button != GDK_BUTTON_PRIMARY && button != GDK_BUTTON_MIDDLE) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_start));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));

  item = WEBKIT_BACK_FORWARD_LIST_ITEM (g_object_get_data (G_OBJECT (widget), HISTORY_ITEM_DATA_KEY));

  if (button == GDK_BUTTON_MIDDLE) {
    middle_click_handle_on_history_menu_item (embed, item);

    if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
      gtk_popover_popdown (GTK_POPOVER (action_bar_start->history_menu));
  } else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_list_item (web_view, item);

    gtk_popover_popdown (GTK_POPOVER (action_bar_start->history_menu));
  }
}

static void
icon_loaded_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  g_autoptr (GtkWidget) image = user_data;
  g_autoptr (GIcon) favicon = NULL;
  g_autoptr (GdkTexture) icon_texture = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  int scale;

  if (!icon_texture)
    return;

  scale = gtk_widget_get_scale_factor (image);
  favicon = ephy_favicon_get_from_texture_scaled (icon_texture, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
  if (favicon)
    gtk_image_set_from_gicon (GTK_IMAGE (image), favicon);
}

static GList *
construct_webkit_history_list (WebKitWebView     *web_view,
                               WebKitHistoryType  hist_type,
                               int                limit)
{
  WebKitBackForwardList *back_forward_list;

  back_forward_list = webkit_web_view_get_back_forward_list (web_view);
  return hist_type == WEBKIT_HISTORY_FORWARD ?
         g_list_reverse (webkit_back_forward_list_get_forward_list_with_limit (back_forward_list, limit)) :
         webkit_back_forward_list_get_back_list_with_limit (back_forward_list, limit);
}

static GtkWidget *
build_history_row (EphyActionBarStart        *action_bar_start,
                   WebKitBackForwardListItem *item)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  const char *uri;
  g_autofree char *title = NULL;
  WebKitFaviconDatabase *database;
  GtkWidget *row, *box, *icon, *label;
  GtkEventController *controller;
  GtkGesture *gesture;

  uri = webkit_back_forward_list_item_get_uri (item);
  title = g_strdup (webkit_back_forward_list_item_get_title (item));

  row = gtk_list_box_row_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);

  icon = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
  gtk_box_append (GTK_BOX (box), icon);

  label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_LABEL_LENGTH);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  g_object_set_data_full (G_OBJECT (row), HISTORY_ITEM_DATA_KEY,
                          g_object_ref (item), g_object_unref);

  if (title && *title)
    gtk_label_set_label (GTK_LABEL (label), title);
  else
    gtk_label_set_label (GTK_LABEL (label), uri);

  database = ephy_embed_shell_get_favicon_database (shell);
  webkit_favicon_database_get_favicon (database, uri,
                                       action_bar_start->cancellable,
                                       (GAsyncReadyCallback)icon_loaded_cb,
                                       g_object_ref (icon));

  g_object_set_data_full (G_OBJECT (row), "link-message",
                          g_strdup (uri), (GDestroyNotify)g_free);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (history_row_enter_cb), action_bar_start);
  g_signal_connect (controller, "leave", G_CALLBACK (history_row_leave_cb), action_bar_start);
  gtk_widget_add_controller (row, controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (history_row_released_cb), action_bar_start);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (gesture));

  return row;
}

static void
build_history_menu (EphyActionBarStart             *action_bar_start,
                    GtkWidget                      *parent,
                    EphyNavigationHistoryDirection  direction)
{
  GtkWidget *popover, *listbox;
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;
  g_autoptr (GList) list = NULL;
  GList *l;

  root = gtk_widget_get_root (GTK_WIDGET (action_bar_start));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));
  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK)
    list = construct_webkit_history_list (web_view,
                                          WEBKIT_HISTORY_BACKWARD, 10);
  else
    list = construct_webkit_history_list (web_view,
                                          WEBKIT_HISTORY_FORWARD, 10);

  popover = gtk_popover_new ();
  gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);
  gtk_widget_set_halign (popover, GTK_ALIGN_START);
  gtk_widget_add_css_class (popover, "menu");
  gtk_widget_set_parent (popover, parent);

  listbox = gtk_list_box_new ();
  gtk_popover_set_child (GTK_POPOVER (popover), listbox);

  for (l = list; l; l = l->next) {
    GtkWidget *row = build_history_row (action_bar_start, l->data);

    gtk_list_box_append (GTK_LIST_BOX (listbox), row);
  }

  action_bar_start->history_menu = popover;
}

static void
history_menu_closed_cb (EphyActionBarStart *action_bar_start)
{
  GtkWidget *parent = gtk_widget_get_parent (action_bar_start->history_menu);

  g_clear_pointer (&action_bar_start->history_menu, gtk_widget_unparent);
  gtk_widget_unset_state_flags (parent, GTK_STATE_FLAG_CHECKED);

  g_cancellable_cancel (action_bar_start->cancellable);
  g_clear_object (&action_bar_start->cancellable);
  action_bar_start->cancellable = g_cancellable_new ();
}

static void
handle_history_menu (EphyActionBarStart *action_bar_start,
                     double              x,
                     double              y,
                     GtkGesture         *gesture)
{
  GtkWidget *widget;
  EphyNavigationHistoryDirection direction;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  if (widget == action_bar_start->navigation_back)
    direction = EPHY_NAVIGATION_HISTORY_DIRECTION_BACK;
  else if (widget == action_bar_start->navigation_forward)
    direction = EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;
  else
    g_assert_not_reached ();

  build_history_menu (action_bar_start, widget, direction);

  gtk_popover_popup (GTK_POPOVER (action_bar_start->history_menu));
  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_CHECKED, FALSE);

  g_signal_connect_swapped (action_bar_start->history_menu, "closed",
                            G_CALLBACK (history_menu_closed_cb), action_bar_start);

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
long_pressed_cb (GtkGesture         *gesture,
                 double              x,
                 double              y,
                 EphyActionBarStart *action_bar_start)
{
  handle_history_menu (action_bar_start, x, y, gesture);
}

static void
right_click_pressed_cb (GtkGesture         *gesture,
                        int                 n_click,
                        double              x,
                        double              y,
                        EphyActionBarStart *action_bar_start)
{
  handle_history_menu (action_bar_start, x, y, gesture);
}

static void
middle_click_pressed_cb (GtkGesture *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
middle_click_released_cb (GtkGesture         *gesture,
                          int                 n_click,
                          double              x,
                          double              y,
                          EphyActionBarStart *action_bar_start)
{
  GtkWidget *widget;
  EphyWindow *window;
  GActionGroup *action_group;
  GAction *action;
  const char *action_name;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  if (widget == action_bar_start->navigation_back)
    action_name = "navigation-back-new-tab";
  else if (widget == action_bar_start->navigation_forward)
    action_name = "navigation-forward-new-tab";
  else if (widget == action_bar_start->combined_stop_reload_button)
    action_name = "duplicate-tab";
  else if (widget == action_bar_start->homepage_button)
    action_name = "homepage-new-tab";
  else if (widget == action_bar_start->new_tab_button)
    action_name = "new-tab-from-clipboard";
  else
    g_assert_not_reached ();

  window = EPHY_WINDOW (gtk_widget_get_root (widget));
  action_group = ephy_window_get_action_group (window, "toolbar");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), action_name);
  g_action_activate (action, NULL);
}

static void
homepage_url_changed (GSettings  *settings,
                      const char *key,
                      GtkWidget  *button)
{
  char *setting;
  gboolean show_button;

  setting = g_settings_get_string (settings, key);
  if (setting && setting[0])
    show_button = g_strcmp0 (setting, "about:newtab") != 0;
  else
    show_button = is_desktop_pantheon ();

  gtk_widget_set_visible (button, show_button);
  g_free (setting);
}

static void
ephy_action_bar_start_dispose (GObject *object)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (object);

  g_clear_pointer (&action_bar_start->history_menu, gtk_widget_unparent);

  g_cancellable_cancel (action_bar_start->cancellable);
  g_clear_object (&action_bar_start->cancellable);

  G_OBJECT_CLASS (ephy_action_bar_start_parent_class)->dispose (object);
}

static void
update_new_tab_button_visibility (EphyActionBarStart *action_bar_start)
{
  EphyEmbedShell *embed_shell;

  embed_shell = ephy_embed_shell_get_default ();

  gtk_widget_set_visible (action_bar_start->new_tab_button,
                          (ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_APPLICATION) &&
                          !is_desktop_pantheon ());
}

static void
ephy_action_bar_start_constructed (GObject *object)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (object);
  EphyEmbedShell *embed_shell;

  G_OBJECT_CLASS (ephy_action_bar_start_parent_class)->constructed (object);

  gtk_widget_init_template (GTK_WIDGET (action_bar_start));

  /* Combined_stop_reload */
  gtk_widget_set_tooltip_text (action_bar_start->combined_stop_reload_button, _(REFRESH_BUTTON_TOOLTIP));

  /* Homepage */
  embed_shell = ephy_embed_shell_get_default ();
  if (ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_APPLICATION) {
    homepage_url_changed (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL, action_bar_start->homepage_button);
    g_signal_connect_object (EPHY_SETTINGS_MAIN,
                             "changed::" EPHY_PREFS_HOMEPAGE_URL,
                             G_CALLBACK (homepage_url_changed),
                             action_bar_start->homepage_button,
                             0);
  } else {
    gtk_widget_set_visible (action_bar_start->homepage_button, FALSE);
  }

  /* New Tab Button */
  update_new_tab_button_visibility (action_bar_start);

  if (ephy_profile_dir_is_web_application ()) {
    GtkWidget *navigation_box = ephy_action_bar_start_get_navigation_box (action_bar_start);

    g_settings_bind (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_SHOW_NAVIGATION_BUTTONS, navigation_box, "visible", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);
  }
}

static void
ephy_action_bar_start_class_init (EphyActionBarStartClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = ephy_action_bar_start_dispose;
  gobject_class->constructed = ephy_action_bar_start_constructed;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/action-bar-start.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        navigation_box);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        navigation_back);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        navigation_forward);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        combined_stop_reload_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        homepage_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        new_tab_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        placeholder);

  gtk_widget_class_bind_template_callback (widget_class,
                                           right_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class,
                                           long_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class,
                                           middle_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class,
                                           middle_click_released_cb);
}

static void
ephy_action_bar_start_init (EphyActionBarStart *action_bar_start)
{
  action_bar_start->cancellable = g_cancellable_new ();
}

EphyActionBarStart *
ephy_action_bar_start_new (void)
{
  return g_object_new (EPHY_TYPE_ACTION_BAR_START,
                       NULL);
}

GtkWidget *
ephy_action_bar_start_get_navigation_box (EphyActionBarStart *action_bar_start)
{
  return action_bar_start->navigation_box;
}

void
ephy_action_bar_start_change_combined_stop_reload_state (EphyActionBarStart *action_bar_start,
                                                         gboolean            loading)
{
  if (loading) {
    gtk_button_set_icon_name (GTK_BUTTON (action_bar_start->combined_stop_reload_button),
                              "process-stop-symbolic");
    /* Translators: tooltip for the stop button */
    gtk_widget_set_tooltip_text (action_bar_start->combined_stop_reload_button,
                                 _("Stop"));
  } else {
    gtk_button_set_icon_name (GTK_BUTTON (action_bar_start->combined_stop_reload_button),
                              "view-refresh-symbolic");
    gtk_widget_set_tooltip_text (action_bar_start->combined_stop_reload_button,
                                 _(REFRESH_BUTTON_TOOLTIP));
  }
}

GtkWidget *
ephy_action_bar_start_get_placeholder (EphyActionBarStart *action_bar_start)
{
  return action_bar_start->placeholder;
}

void
ephy_action_bar_start_set_adaptive_mode (EphyActionBarStart *action_bar,
                                         EphyAdaptiveMode    adaptive_mode)
{
  GValue val = G_VALUE_INIT;
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  g_value_init (&val, G_TYPE_INT);

  gtk_widget_set_visible (action_bar->new_tab_button, adaptive_mode == EPHY_ADAPTIVE_MODE_NORMAL);
  gtk_widget_set_visible (action_bar->combined_stop_reload_button, mode == EPHY_EMBED_SHELL_MODE_APPLICATION && adaptive_mode == EPHY_ADAPTIVE_MODE_NORMAL);

  if (adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW)
    g_value_set_int (&val, 42);
  else
    g_value_set_int (&val, -1);

  g_object_set_property (G_OBJECT (action_bar->navigation_back), "width-request", &val);
  g_object_set_property (G_OBJECT (action_bar->navigation_forward), "width-request", &val);

  g_value_unset (&val);
}
