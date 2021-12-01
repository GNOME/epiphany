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
#include "ephy-gui.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-window.h"

/* Translators: tooltip for the refresh button */
static const char *REFRESH_BUTTON_TOOLTIP = N_("Reload the current page");

struct _EphyActionBarStart {
  GtkBox parent_instance;

  GtkWidget *navigation_box;
  GtkWidget *navigation_back;
  GtkWidget *navigation_forward;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *combined_stop_reload_image;
  GtkWidget *homepage_button;
  GtkWidget *new_tab_button;
  GtkWidget *placeholder;

  guint navigation_buttons_menu_timeout;
};

G_DEFINE_TYPE (EphyActionBarStart, ephy_action_bar_start, GTK_TYPE_BOX)

typedef enum {
  EPHY_NAVIGATION_HISTORY_DIRECTION_BACK,
  EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD
} EphyNavigationHistoryDirection;

typedef enum {
  WEBKIT_HISTORY_BACKWARD,
  WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;

typedef struct {
  GtkWidget *button;
  EphyWindow *window;
  EphyNavigationHistoryDirection direction;
  GdkEventButton *event;
} PopupData;

#define MAX_LABEL_LENGTH 48
#define HISTORY_ITEM_DATA_KEY "history-item-data-key"

static gboolean
item_enter_notify_event_cb (GtkWidget   *widget,
                            GdkEvent    *event,
                            EphyWebView *view)
{
  char *text;

  text = g_object_get_data (G_OBJECT (widget), "link-message");
  ephy_web_view_set_link_message (view, text);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
item_leave_notify_event_cb (GtkWidget   *widget,
                            GdkEvent    *event,
                            EphyWebView *view)
{
  ephy_web_view_set_link_message (view, NULL);
  return GDK_EVENT_PROPAGATE;
}

static void
icon_loaded_cb (GObject      *source,
                GAsyncResult *result,
                GtkWidget    *image)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  GdkPixbuf *favicon = NULL;
  cairo_surface_t *icon_surface = webkit_favicon_database_get_favicon_finish (database, result, NULL);

  if (icon_surface) {
    gint scale = gtk_widget_get_scale_factor (image);

    favicon = ephy_pixbuf_get_from_surface_scaled (icon_surface, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
    cairo_surface_destroy (icon_surface);
  }

  if (favicon)
    gtk_image_set_from_gicon (GTK_IMAGE (image), G_ICON (favicon), GTK_ICON_SIZE_MENU);

  g_object_unref (image);
}

static GtkWidget *
new_history_menu_item (EphyWebView *view,
                       const char  *origtext,
                       const char  *address)
{
  GtkWidget *menu_item;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  WebKitFaviconDatabase *database;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  g_assert (address != NULL && origtext != NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 0);

  label = gtk_label_new (origtext);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_LABEL_LENGTH);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

  menu_item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_item), box);

  database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));
  webkit_favicon_database_get_favicon (database, address,
                                       NULL,
                                       (GAsyncReadyCallback)icon_loaded_cb,
                                       g_object_ref (image));

  g_object_set_data_full (G_OBJECT (menu_item), "link-message", g_strdup (address), (GDestroyNotify)g_free);

  g_signal_connect (menu_item, "enter-notify-event",
                    G_CALLBACK (item_enter_notify_event_cb), view);
  g_signal_connect (menu_item, "leave-notify-event",
                    G_CALLBACK (item_leave_notify_event_cb), view);

  gtk_widget_show_all (menu_item);

  return menu_item;
}

static void
middle_click_handle_on_history_menu_item (EphyEmbed                 *embed,
                                          WebKitBackForwardListItem *item)
{
  EphyEmbed *new_embed = NULL;
  const gchar *url;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  0);
  g_assert (new_embed != NULL);

  /* Load the new URL */
  url = webkit_back_forward_list_item_get_original_uri (item);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), url);
}

static gboolean
navigation_menu_item_pressed_cb (GtkWidget *menuitem,
                                 GdkEvent  *event,
                                 gpointer   user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  WebKitBackForwardListItem *item;
  EphyEmbed *embed;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

  item = (WebKitBackForwardListItem *)g_object_get_data (G_OBJECT (menuitem), HISTORY_ITEM_DATA_KEY);

  if (((GdkEventButton *)event)->button == GDK_BUTTON_MIDDLE) {
    middle_click_handle_on_history_menu_item (embed, item);
  } else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_list_item (web_view, item);
  }

  return GDK_EVENT_STOP;
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
build_dropdown_menu (EphyWindow                     *window,
                     EphyNavigationHistoryDirection  direction)
{
  GtkMenuShell *menu;
  EphyEmbed *embed;
  GList *list, *l;
  WebKitWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK)
    list = construct_webkit_history_list (web_view,
                                          WEBKIT_HISTORY_BACKWARD, 10);
  else
    list = construct_webkit_history_list (web_view,
                                          WEBKIT_HISTORY_FORWARD, 10);

  for (l = list; l != NULL; l = l->next) {
    GtkWidget *item;
    WebKitBackForwardListItem *hitem;
    const char *uri;
    char *title;

    hitem = (WebKitBackForwardListItem *)l->data;
    uri = webkit_back_forward_list_item_get_uri (hitem);
    title = g_strdup (webkit_back_forward_list_item_get_title (hitem));

    if (title == NULL || g_strstrip (title)[0] == '\0')
      item = new_history_menu_item (EPHY_WEB_VIEW (web_view), uri, uri);
    else
      item = new_history_menu_item (EPHY_WEB_VIEW (web_view), title, uri);

    g_free (title);

    g_object_set_data_full (G_OBJECT (item), HISTORY_ITEM_DATA_KEY,
                            g_object_ref (hitem), g_object_unref);

    g_signal_connect (item, "button-release-event",
                      G_CALLBACK (navigation_menu_item_pressed_cb), window);

    gtk_menu_shell_append (menu, item);
    gtk_widget_show_all (item);
  }

  g_list_free (list);

  return GTK_WIDGET (menu);
}

static void
popup_history_menu (GtkWidget                      *widget,
                    EphyWindow                     *window,
                    EphyNavigationHistoryDirection  direction,
                    GdkEventButton                 *event)
{
  GtkWidget *menu;

  menu = build_dropdown_menu (window, direction);
  gtk_menu_popup_at_widget (GTK_MENU (menu),
                            widget,
                            GDK_GRAVITY_SOUTH_WEST,
                            GDK_GRAVITY_NORTH_WEST,
                            (GdkEvent *)event);
}

static gboolean
menu_timeout_cb (PopupData *data)
{
  if (data != NULL && data->window)
    popup_history_menu (data->button, data->window, data->direction, data->event);

  return G_SOURCE_REMOVE;
}

static gboolean
navigation_button_press_event_cb (GtkButton *button,
                                  GdkEvent  *event,
                                  gpointer   user_data)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (user_data);
  EphyNavigationHistoryDirection direction;
  PopupData *data;
  gboolean is_back = FALSE;

  is_back = (GTK_WIDGET (button) == action_bar_start->navigation_back);

  direction = is_back ? EPHY_NAVIGATION_HISTORY_DIRECTION_BACK
                      : EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;

  switch (((GdkEventButton *)event)->button) {
    case GDK_BUTTON_SECONDARY:
      popup_history_menu (GTK_WIDGET (button), EPHY_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (action_bar_start), EPHY_TYPE_WINDOW)),
                          direction, (GdkEventButton *)event);
      return GDK_EVENT_STOP;
    case GDK_BUTTON_MIDDLE:
      /* If a desktop-wide middle-click titlebar action is enabled, we want to
       * prevent that action from occurring because we handle middle click in
       * navigation_button_release_event_cb().
       */
      return GDK_EVENT_STOP;
    default:
      break;
  }

  data = g_new (PopupData, 1);
  data->button = GTK_WIDGET (button);
  data->window = EPHY_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (action_bar_start), EPHY_TYPE_WINDOW));
  data->direction = direction;
  data->event = (GdkEventButton *)event;

  action_bar_start->navigation_buttons_menu_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT, 500,
                                                                          (GSourceFunc)menu_timeout_cb,
                                                                          data,
                                                                          (GDestroyNotify)g_free);
  g_source_set_name_by_id (action_bar_start->navigation_buttons_menu_timeout, "[epiphany] menu_timeout_cb");

  return GDK_EVENT_PROPAGATE;
}

static gboolean
navigation_button_release_event_cb (GtkButton *button,
                                    GdkEvent  *event,
                                    gpointer   user_data)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (user_data);
  GActionGroup *action_group;
  GAction *action;
  EphyNavigationHistoryDirection direction;
  gboolean is_back = FALSE;
  gboolean open_in_new_tab = FALSE;
  gboolean open_in_current_tab = FALSE;
  GdkEventType type = GDK_NOTHING;
  guint state = 0, button_val = (guint) - 1, keyval = (guint) - 1;

  ephy_gui_get_current_event (&type, &state, &button_val, &keyval);
  is_back = (GTK_WIDGET (button) == action_bar_start->navigation_back);

  g_clear_handle_id (&action_bar_start->navigation_buttons_menu_timeout, g_source_remove);

  action_group = gtk_widget_get_action_group (gtk_widget_get_ancestor (GTK_WIDGET (action_bar_start), EPHY_TYPE_WINDOW), "toolbar");

  direction = is_back ? EPHY_NAVIGATION_HISTORY_DIRECTION_BACK
                      : EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;

  open_in_new_tab = (((GdkEventButton *)event)->button == GDK_BUTTON_MIDDLE) || (state == GDK_CONTROL_MASK);
  open_in_current_tab = ((GdkEventButton *)event)->button == GDK_BUTTON_PRIMARY;

  if (open_in_new_tab) {
    if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           "navigation-back-new-tab");
      g_action_activate (action, NULL);
    } else if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           "navigation-forward-new-tab");
      g_action_activate (action, NULL);
    }

    /* Don't propagate the event to avoid other middle-click actions. */
    return GDK_EVENT_STOP;
  }

  if (open_in_current_tab) {
    if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           "navigation-back");
      g_action_activate (action, NULL);
    } else if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                           "navigation-forward");
      g_action_activate (action, NULL);
    }

    /* Propagate the event to allow the button to correctly reset its internal
     * state. */
    return GDK_EVENT_PROPAGATE;
  }

  /* Propagate other unhandled events. */
  return GDK_EVENT_PROPAGATE;
}

static gboolean
homepage_button_release_event_cb (GtkButton *button,
                                  GdkEvent  *event,
                                  gpointer   user_data)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (user_data);
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (gtk_widget_get_ancestor (GTK_WIDGET (action_bar_start), EPHY_TYPE_WINDOW), "toolbar");

  switch (((GdkEventButton *)event)->button) {
    case GDK_BUTTON_MIDDLE:
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "homepage-new-tab");
      g_action_activate (action, NULL);
      return GDK_EVENT_STOP;
    default:
      break;
  }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
new_tab_button_release_event_cb (GtkButton *button,
                                 GdkEvent  *event,
                                 gpointer   user_data)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (user_data);
  GActionGroup *action_group;
  GAction *action;

  action_group = gtk_widget_get_action_group (gtk_widget_get_ancestor (GTK_WIDGET (action_bar_start), EPHY_TYPE_WINDOW), "toolbar");

  switch (((GdkEventButton *)event)->button) {
    case GDK_BUTTON_MIDDLE:
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "new-tab-from-clipboard");
      g_action_activate (action, NULL);
      return GDK_EVENT_STOP;
    default:
      break;
  }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
navigation_leave_notify_event_cb (GtkButton *button,
                                  GdkEvent  *event,
                                  gpointer   user_data)
{
  EphyActionBarStart *action_bar_start = EPHY_ACTION_BAR_START (user_data);

  g_clear_handle_id (&action_bar_start->navigation_buttons_menu_timeout, g_source_remove);

  return GDK_EVENT_PROPAGATE;
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

  g_clear_handle_id (&action_bar_start->navigation_buttons_menu_timeout, g_source_remove);

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

  /* Back */
  g_signal_connect (action_bar_start->navigation_back, "button-press-event",
                    G_CALLBACK (navigation_button_press_event_cb), action_bar_start);
  g_signal_connect (action_bar_start->navigation_back, "button-release-event",
                    G_CALLBACK (navigation_button_release_event_cb), action_bar_start);
  g_signal_connect (action_bar_start->navigation_back, "leave-notify-event",
                    G_CALLBACK (navigation_leave_notify_event_cb), action_bar_start);

  /* Forward */
  g_signal_connect (action_bar_start->navigation_forward, "button-press-event",
                    G_CALLBACK (navigation_button_press_event_cb), action_bar_start);
  g_signal_connect (action_bar_start->navigation_forward, "button-release-event",
                    G_CALLBACK (navigation_button_release_event_cb), action_bar_start);
  g_signal_connect (action_bar_start->navigation_forward, "leave-notify-event",
                    G_CALLBACK (navigation_leave_notify_event_cb), action_bar_start);

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
  g_signal_connect (action_bar_start->homepage_button, "button-release-event",
                    G_CALLBACK (homepage_button_release_event_cb), action_bar_start);

  /* New Tab Button */
  update_new_tab_button_visibility (action_bar_start);

  g_signal_connect (action_bar_start->new_tab_button, "button-release-event",
                    G_CALLBACK (new_tab_button_release_event_cb), action_bar_start);

  if (is_desktop_pantheon ()) {
    gtk_button_set_image (GTK_BUTTON (action_bar_start->navigation_back),
                          gtk_image_new_from_icon_name ("go-previous-symbolic",
                                                        get_icon_size ()));
    gtk_button_set_image (GTK_BUTTON (action_bar_start->navigation_forward),
                          gtk_image_new_from_icon_name ("go-next-symbolic",
                                                        get_icon_size ()));
    gtk_button_set_image (GTK_BUTTON (action_bar_start->homepage_button),
                          gtk_image_new_from_icon_name ("go-home-symbolic",
                                                        get_icon_size ()));
  }

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
                                        combined_stop_reload_image);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        homepage_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        new_tab_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarStart,
                                        placeholder);
}

static void
ephy_action_bar_start_init (EphyActionBarStart *action_bar_start)
{
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
    gtk_image_set_from_icon_name (GTK_IMAGE (action_bar_start->combined_stop_reload_image),
                                  "process-stop-symbolic",
                                  get_icon_size ());
    /* Translators: tooltip for the stop button */
    gtk_widget_set_tooltip_text (action_bar_start->combined_stop_reload_button,
                                 _("Stop loading the current page"));
  } else {
    gtk_image_set_from_icon_name (GTK_IMAGE (action_bar_start->combined_stop_reload_image),
                                  "view-refresh-symbolic",
                                  get_icon_size ());
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

  g_value_init (&val, G_TYPE_INT);

  gtk_widget_set_visible (action_bar->new_tab_button, adaptive_mode == EPHY_ADAPTIVE_MODE_NORMAL);
  gtk_widget_set_visible (action_bar->combined_stop_reload_button, adaptive_mode == EPHY_ADAPTIVE_MODE_NORMAL);

  if (adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW)
    g_value_set_int (&val, 42);
  else
    g_value_set_int (&val, -1);

  g_object_set_property (G_OBJECT (action_bar->navigation_back), "width-request", &val);
  g_object_set_property (G_OBJECT (action_bar->navigation_forward), "width-request", &val);

  g_value_unset (&val);
}
