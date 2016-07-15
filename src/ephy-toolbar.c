/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2013 Yosef Or Boczko <yoseforb@gmail.com>
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
#include "ephy-toolbar.h"

#include "ephy-action-helper.h"
#include "ephy-bookmarks-popover.h"
#include "ephy-downloads-popover.h"
#include "ephy-downloads-progress-icon.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-helpers.h"
#include "ephy-gui.h"
#include "ephy-history-service.h"
#include "ephy-location-entry.h"
#include "ephy-middle-clickable-button.h"
#include "ephy-private.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

#define MAX_LABEL_LENGTH 48
#define HISTORY_ITEM_DATA_KEY "history-item-data-key"

struct _EphyToolbar {
  GtkHeaderBar parent_instance;

  EphyWindow *window;
  EphyTitleBox *title_box;
  GtkWidget *entry;
  GtkWidget *navigation_box;
  GtkWidget *new_tab_button;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *bookmarks_button;
  GtkWidget *page_menu_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;

  GMenu *page_menu;

  guint navigation_buttons_menu_timeout;
};

G_DEFINE_TYPE (EphyToolbar, ephy_toolbar, GTK_TYPE_HEADER_BAR)

static gboolean
toolbar_is_for_active_window (EphyToolbar *toolbar)
{
  EphyShell *shell = ephy_shell_get_default ();
  GtkWindow *active_window;

  active_window = gtk_application_get_active_window (GTK_APPLICATION (shell));

  return active_window == GTK_WINDOW (toolbar->window);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyToolbar          *toolbar)
{
  if (!toolbar->downloads_popover) {
    toolbar->downloads_popover = ephy_downloads_popover_new (toolbar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (toolbar->downloads_button),
                                 toolbar->downloads_popover);
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (toolbar->downloads_revealer), TRUE);

  if (toolbar_is_for_active_window (toolbar))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->downloads_button), TRUE);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyToolbar          *toolbar)
{
  if (toolbar_is_for_active_window (toolbar))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->downloads_button), TRUE);
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload         *download,
                     EphyToolbar          *toolbar)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_revealer_set_reveal_child (GTK_REVEALER (toolbar->downloads_revealer), FALSE);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyToolbar          *toolbar)
{
  gtk_widget_queue_draw (gtk_button_get_image (GTK_BUTTON (toolbar->downloads_button)));
}

static void
ephy_toolbar_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      toolbar->window = EPHY_WINDOW (g_value_get_object (value));
      g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_toolbar_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      g_value_set_object (value, toolbar->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sync_chromes_visibility (EphyToolbar *toolbar)
{
  EphyWindowChrome chrome;

  chrome = ephy_window_get_chrome (toolbar->window);

  gtk_widget_set_visible (toolbar->navigation_box, chrome & EPHY_WINDOW_CHROME_TOOLBAR);
  gtk_widget_set_visible (toolbar->bookmarks_button, chrome & EPHY_WINDOW_CHROME_BOOKMARKS);
  gtk_widget_set_visible (toolbar->page_menu_button, chrome & EPHY_WINDOW_CHROME_MENU);
  gtk_widget_set_visible (toolbar->new_tab_button, chrome & EPHY_WINDOW_CHROME_TABSBAR);
}

typedef enum {
  EPHY_NAVIGATION_HISTORY_DIRECTION_BACK,
  EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD
} EphyNavigationHistoryDirection;

typedef enum {
  WEBKIT_HISTORY_BACKWARD,
  WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;

static void
ephy_history_cleared_cb (EphyHistoryService *history,
                         gpointer           user_data)
{
  GActionGroup *action_group;
  GAction *action;
  guint i;
  gchar **actions;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (user_data), "toolbar");
  actions = g_action_group_list_actions (action_group);
  for (i = 0; actions[i] != NULL; i++) {
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), actions[i]);
    ephy_action_change_sensitivity_flags (G_SIMPLE_ACTION (action), SENS_FLAG, TRUE);

    g_free (actions[i]);
  }

  g_free (actions);
}

static gboolean
item_enter_notify_event_cb (GtkWidget   *widget,
                            GdkEvent    *event,
                            EphyWebView *view)
{
  char *text;

  text = g_object_get_data (G_OBJECT (widget), "link-message");
  ephy_web_view_set_link_message (view, text);

  return FALSE;
}

static gboolean
item_leave_notify_event_cb (GtkWidget   *widget,
                            GdkEvent    *event,
                            EphyWebView *view)
{
  ephy_web_view_set_link_message (view, NULL);
  return FALSE;
}

static void
icon_loaded_cb (GObject          *source,
                GAsyncResult     *result,
                GtkImageMenuItem *item)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  GdkPixbuf *favicon = NULL;
  cairo_surface_t *icon_surface = webkit_favicon_database_get_favicon_finish (database, result, NULL);

  if (icon_surface) {
    favicon = ephy_pixbuf_get_from_surface_scaled (icon_surface, FAVICON_SIZE, FAVICON_SIZE);
    cairo_surface_destroy (icon_surface);
  }

  if (favicon) {
    GtkWidget *image;

    image = gtk_image_new_from_pixbuf (favicon);
    gtk_image_menu_item_set_image (item, image);
    gtk_image_menu_item_set_always_show_image (item, TRUE);

    g_object_unref (favicon);
  }

  g_object_unref (item);
}

static GtkWidget *
new_history_menu_item (EphyWebView *view,
                       const char  *origtext,
                       const char  *address)
{
  GtkWidget *item;
  GtkLabel *label;
  WebKitFaviconDatabase *database;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  g_return_val_if_fail (address != NULL && origtext != NULL, NULL);

  item = gtk_image_menu_item_new_with_label (origtext);

  label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (item)));
  gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (label, MAX_LABEL_LENGTH);

  database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));
  webkit_favicon_database_get_favicon (database, address,
                                       NULL,
                                       (GAsyncReadyCallback)icon_loaded_cb,
                                       g_object_ref (item));

  g_object_set_data_full (G_OBJECT (item), "link-message", g_strdup (address), (GDestroyNotify)g_free);

  g_signal_connect (item, "enter-notify-event",
                    G_CALLBACK (item_enter_notify_event_cb), view);
  g_signal_connect (item, "leave-notify-event",
                    G_CALLBACK (item_leave_notify_event_cb), view);

  gtk_widget_show (item);

  return item;
}

static void
set_new_back_history (EphyEmbed *source,
                      EphyEmbed *dest,
                      gint       offset)
{
  /* TODO: WebKitBackForwardList: In WebKit2 WebKitBackForwardList can't be modified */
}

static void
middle_click_handle_on_history_menu_item (EphyEmbed                 *embed,
                                          WebKitBackForwardListItem *item)
{
  EphyEmbed *new_embed = NULL;
  const gchar *url;
  gint offset;

  /* TODO: WebKitBackForwardList is read-only in WebKit2 */
  offset = 0;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  0);
  g_return_if_fail (new_embed != NULL);

  /* We manually set the back history instead of trusting
     ephy_shell_new_tab because the logic is more complex than
     usual, due to handling also the forward history */
  set_new_back_history (embed, new_embed, offset);

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

  if (((GdkEventButton *)event)->button == 2) {
    middle_click_handle_on_history_menu_item (embed, item);
  } else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_list_item (web_view, item);
  }

  return G_SOURCE_REMOVE;
}

static GList *
construct_webkit_history_list (WebKitWebView    *web_view,
                               WebKitHistoryType hist_type,
                               int               limit)
{
  WebKitBackForwardList *back_forward_list;

  back_forward_list = webkit_web_view_get_back_forward_list (web_view);
  return hist_type == WEBKIT_HISTORY_FORWARD ?
         g_list_reverse (webkit_back_forward_list_get_forward_list_with_limit (back_forward_list, limit)) :
         webkit_back_forward_list_get_back_list_with_limit (back_forward_list, limit);
}

static GtkWidget *
build_dropdown_menu (EphyWindow                    *window,
                     EphyNavigationHistoryDirection direction)
{
  GtkMenuShell *menu;
  EphyEmbed *embed;
  GList *list, *l;
  WebKitWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_val_if_fail (embed != NULL, NULL);

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
popup_history_menu (GtkWidget                     *widget,
                    EphyWindow                    *window,
                    EphyNavigationHistoryDirection direction,
                    GdkEventButton                *event)
{
  GtkWidget *menu;

  menu = build_dropdown_menu (window, direction);
  gtk_menu_popup (GTK_MENU (menu),
                  NULL, NULL,
                  ephy_gui_menu_position_under_widget, widget,
                  event->button, event->time);
}

typedef struct {
  GtkWidget *button;
  EphyWindow *window;
  EphyNavigationHistoryDirection direction;
  GdkEventButton *event;
} PopupData;

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
  EphyToolbar *toolbar = EPHY_TOOLBAR (user_data);
  EphyNavigationHistoryDirection direction;
  const gchar *action_name;

  action_name = gtk_actionable_get_action_name (GTK_ACTIONABLE (button));

  direction = strstr (action_name, "back") ? EPHY_NAVIGATION_HISTORY_DIRECTION_BACK
                                           : EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;

  if (((GdkEventButton *)event)->button == 3) {
    popup_history_menu (GTK_WIDGET (button), toolbar->window,
                        direction, (GdkEventButton *)event);
  } else {
    PopupData *data;

    data = g_new (PopupData, 1);
    data->button = GTK_WIDGET (button);
    data->window = toolbar->window;
    data->direction = direction;
    data->event = (GdkEventButton *)event;

    toolbar->navigation_buttons_menu_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT, 500,
                                                                   (GSourceFunc)menu_timeout_cb,
                                                                   data,
                                                                   (GDestroyNotify)g_free);
    g_source_set_name_by_id (toolbar->navigation_buttons_menu_timeout, "[epiphany] menu_timeout_cb");
  }

  return FALSE;
}

static gboolean
navigation_button_release_event_cb (GtkButton *button,
                                    GdkEvent  *event,
                                    gpointer   user_data)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (user_data);
  GActionGroup *action_group;
  GAction *action;
  EphyNavigationHistoryDirection direction;
  const gchar *action_name;

  if (toolbar->navigation_buttons_menu_timeout > 0)
    g_source_remove (toolbar->navigation_buttons_menu_timeout);
  toolbar->navigation_buttons_menu_timeout = 0;

  action_name = gtk_actionable_get_action_name (GTK_ACTIONABLE (button));
  action_group = gtk_widget_get_action_group (GTK_WIDGET (toolbar->window), "toolbar");

  direction = strstr (action_name, "back") == 0 ? EPHY_NAVIGATION_HISTORY_DIRECTION_BACK
                                                : EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;

  switch (((GdkEventButton *)event)->button) {
    case GDK_BUTTON_MIDDLE:
      if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                             "navigation-back-new-tab");
        g_action_activate (action, NULL);
      } else if (direction == EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD) {
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                             "navigation-forward-new-tab");
        g_action_activate (action, NULL);
      }
      break;
    case GDK_BUTTON_SECONDARY:
      popup_history_menu (GTK_WIDGET (button), toolbar->window,
                          direction, (GdkEventButton *)event);
      break;
    default:
      break;
  }

  return G_SOURCE_REMOVE;
}

static gboolean
navigation_leave_notify_event_cb (GtkButton *button,
                                  GdkEvent  *event,
                                  gpointer   user_data)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (user_data);

  if (toolbar->navigation_buttons_menu_timeout > 0)
    g_source_remove (toolbar->navigation_buttons_menu_timeout);

  toolbar->navigation_buttons_menu_timeout = 0;

  return G_SOURCE_REMOVE;
}

void
ephy_toolbar_change_combined_stop_reload_state (GSimpleAction *action,
                                                GVariant      *loading,
                                                gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyToolbar *toolbar;
  GtkWidget *image;

  if (g_variant_get_boolean (loading))
    image = gtk_image_new_from_icon_name ("process-stop-symbolic",
                                          GTK_ICON_SIZE_BUTTON);
  else
    image = gtk_image_new_from_icon_name ("view-refresh-symbolic",
                                          GTK_ICON_SIZE_BUTTON);

  toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));
  gtk_button_set_image (GTK_BUTTON (toolbar->combined_stop_reload_button),
                        image);
}

static void
ephy_toolbar_constructed (GObject *object)
{
  EphyToolbar *toolbar = EPHY_TOOLBAR (object);
  GtkWidget *box, *button;
  GtkMenu *menu;
  EphyDownloadsManager *downloads_manager;
  GtkBuilder *builder;
  EphyHistoryService *history_service;

  G_OBJECT_CLASS (ephy_toolbar_parent_class)->constructed (object);

  g_signal_connect_swapped (toolbar->window, "notify::chrome",
                            G_CALLBACK (sync_chromes_visibility), toolbar);

  /* Back and Forward */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  toolbar->navigation_box = box;

  /* Back */
  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("go-previous-symbolic",
                        GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                  "toolbar.navigation-back");
  g_signal_connect (button, "button-press-event",
                    G_CALLBACK (navigation_button_press_event_cb), toolbar);
  g_signal_connect (button, "button-release-event",
                    G_CALLBACK (navigation_button_release_event_cb), toolbar);
  g_signal_connect (button, "leave-notify-event",
                    G_CALLBACK (navigation_leave_notify_event_cb), toolbar);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_widget_show (GTK_WIDGET (button));

  /* Forward */
  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("go-next-symbolic",
                        GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                  "toolbar.navigation-forward");
  g_signal_connect (button, "button-press-event",
                    G_CALLBACK (navigation_button_press_event_cb), toolbar);
  g_signal_connect (button, "button-release-event",
                    G_CALLBACK (navigation_button_release_event_cb), toolbar);
  g_signal_connect (button, "leave-notify-event",
                    G_CALLBACK (navigation_leave_notify_event_cb), toolbar);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_widget_show (GTK_WIDGET (button));

  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "raised");
  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "linked");

  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), box);

  /* Reload/Stop */
  button = gtk_button_new ();
  toolbar->combined_stop_reload_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                  "toolbar.combined-stop-reload");
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               "image-button");
  gtk_widget_show (GTK_WIDGET (button));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), button);

  /* New Tab */
  button = gtk_button_new ();
  toolbar->new_tab_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.new-tab");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), button);

  /* Location bar + Title */
  toolbar->title_box = ephy_title_box_new (toolbar->window);
  toolbar->entry = ephy_title_box_get_location_entry (toolbar->title_box);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (toolbar), GTK_WIDGET (toolbar->title_box));
  gtk_widget_show (GTK_WIDGET (toolbar->title_box));

  /* Page Menu */
  button = gtk_menu_button_new ();
  toolbar->page_menu_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/menus.ui");
  toolbar->page_menu = G_MENU (gtk_builder_get_object (builder, "page-menu"));
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), FALSE);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button),
                                  G_MENU_MODEL (toolbar->page_menu));
  menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (button));
  gtk_widget_set_halign (GTK_WIDGET (menu), GTK_ALIGN_END);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);

  /* Bookmarks button */
  button = gtk_menu_button_new ();
  toolbar->bookmarks_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("user-bookmarks-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), GTK_WIDGET (ephy_bookmarks_popover_new ()));

  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  toolbar->downloads_revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (toolbar->downloads_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (toolbar->downloads_revealer),
                                 ephy_downloads_manager_get_downloads (downloads_manager) != NULL);

  toolbar->downloads_button = gtk_menu_button_new ();
  gtk_button_set_image (GTK_BUTTON (toolbar->downloads_button), ephy_downloads_progress_icon_new ());
  gtk_widget_set_valign (toolbar->downloads_button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (toolbar->downloads_revealer), toolbar->downloads_button);
  gtk_widget_show (toolbar->downloads_button);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    toolbar->downloads_popover = ephy_downloads_popover_new (toolbar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (toolbar->downloads_button),
                                 toolbar->downloads_popover);
  }

  g_signal_connect_object (downloads_manager, "download-added",
                           G_CALLBACK (download_added_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "download-completed",
                           G_CALLBACK (download_completed_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "download-removed",
                           G_CALLBACK (download_removed_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "estimated-progress-changed",
                           G_CALLBACK (downloads_estimated_progress_cb),
                           object, 0);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), toolbar->downloads_revealer);
  gtk_widget_show (toolbar->downloads_revealer);

  history_service = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ()));

  if (toolbar->navigation_buttons_menu_timeout > 0)
    g_source_remove (toolbar->navigation_buttons_menu_timeout);
  toolbar->navigation_buttons_menu_timeout = 0;

  g_signal_connect (history_service,
                    "cleared", G_CALLBACK (ephy_history_cleared_cb),
                    toolbar->window);
}

static void
ephy_toolbar_init (EphyToolbar *toolbar)
{
}

static void
ephy_toolbar_finalize (GObject *object)
{
  if (EPHY_TOOLBAR (object)->navigation_buttons_menu_timeout > 0)
    g_source_remove (EPHY_TOOLBAR (object)->navigation_buttons_menu_timeout);

  G_OBJECT_CLASS (ephy_toolbar_parent_class)->finalize (object);
}

static void
ephy_toolbar_class_init (EphyToolbarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ephy_toolbar_finalize;
  gobject_class->set_property = ephy_toolbar_set_property;
  gobject_class->get_property = ephy_toolbar_get_property;
  gobject_class->constructed = ephy_toolbar_constructed;

  object_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The toolbar's EphyWindow",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     object_properties);
}

GtkWidget *
ephy_toolbar_new (EphyWindow *window)
{
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

  return GTK_WIDGET (g_object_new (EPHY_TYPE_TOOLBAR,
                                   "show-close-button", TRUE,
                                   "window", window,
                                   NULL));
}

GtkWidget *
ephy_toolbar_get_location_entry (EphyToolbar *toolbar)
{
  return toolbar->entry;
}

EphyTitleBox *
ephy_toolbar_get_title_box (EphyToolbar *toolbar)
{
  return toolbar->title_box;
}

GMenu *
ephy_toolbar_get_page_menu (EphyToolbar *toolbar)
{
  return toolbar->page_menu;
}

GtkWidget *
ephy_toolbar_get_page_menu_button (EphyToolbar *toolbar)
{
  return toolbar->page_menu_button;
}

GtkWidget *
ephy_toolbar_get_new_tab_button (EphyToolbar *toolbar)
{
  return toolbar->new_tab_button;
}
