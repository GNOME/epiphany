/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2013 Yosef Or Boczko <yoseforb@gmail.com>
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
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
#include "ephy-header-bar.h"

#include "ephy-action-helper.h"
#include "ephy-bookmarks-popover.h"
#include "ephy-bookmark-properties-grid.h"
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
#include "ephy-title-box.h"
#include "ephy-title-widget.h"
#include "ephy-type-builtins.h"

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

struct _EphyHeaderBar {
  GtkHeaderBar parent_instance;

  EphyWindow *window;
  EphyTitleWidget *title_widget;
  GtkWidget *navigation_box;
  GtkWidget *new_tab_button;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *bookmarks_button;
  GtkWidget *page_menu_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;

  guint navigation_buttons_menu_timeout;
};

G_DEFINE_TYPE (EphyHeaderBar, ephy_header_bar, GTK_TYPE_HEADER_BAR)

static gboolean
header_bar_is_for_active_window (EphyHeaderBar *header_bar)
{
  EphyShell *shell = ephy_shell_get_default ();
  GtkWindow *active_window;

  active_window = gtk_application_get_active_window (GTK_APPLICATION (shell));

  return active_window == GTK_WINDOW (header_bar->window);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyHeaderBar        *header_bar)
{
  if (!header_bar->downloads_popover) {
    header_bar->downloads_popover = ephy_downloads_popover_new (header_bar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (header_bar->downloads_button),
                                 header_bar->downloads_popover);
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->downloads_revealer), TRUE);

  if (header_bar_is_for_active_window (header_bar))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (header_bar->downloads_button), TRUE);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyHeaderBar        *header_bar)
{
  if (header_bar_is_for_active_window (header_bar))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (header_bar->downloads_button), TRUE);
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload         *download,
                     EphyHeaderBar        *header_bar)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->downloads_revealer), FALSE);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyHeaderBar        *header_bar)
{
  gtk_widget_queue_draw (gtk_button_get_image (GTK_BUTTON (header_bar->downloads_button)));
}

static void
ephy_header_bar_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      header_bar->window = EPHY_WINDOW (g_value_get_object (value));
      g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_header_bar_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      g_value_set_object (value, header_bar->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sync_chromes_visibility (EphyHeaderBar *header_bar)
{
  EphyWindowChrome chrome;

  chrome = ephy_window_get_chrome (header_bar->window);

  gtk_widget_set_visible (header_bar->navigation_box, chrome & EPHY_WINDOW_CHROME_HEADER_BAR);
  gtk_widget_set_visible (header_bar->bookmarks_button, chrome & EPHY_WINDOW_CHROME_BOOKMARKS);
  gtk_widget_set_visible (header_bar->page_menu_button, chrome & EPHY_WINDOW_CHROME_MENU);
  gtk_widget_set_visible (header_bar->new_tab_button, chrome & EPHY_WINDOW_CHROME_TABSBAR);
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
                         gpointer            user_data)
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
  }

  g_strfreev (actions);
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
middle_click_handle_on_history_menu_item (EphyEmbed                 *embed,
                                          WebKitBackForwardListItem *item)
{
  EphyEmbed *new_embed = NULL;
  const gchar *url;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  0);
  g_return_if_fail (new_embed != NULL);

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
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (user_data);
  EphyNavigationHistoryDirection direction;
  const gchar *action_name;

  action_name = gtk_actionable_get_action_name (GTK_ACTIONABLE (button));

  direction = strstr (action_name, "back") ? EPHY_NAVIGATION_HISTORY_DIRECTION_BACK
                                           : EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;

  if (((GdkEventButton *)event)->button == GDK_BUTTON_SECONDARY) {
    popup_history_menu (GTK_WIDGET (button), header_bar->window,
                        direction, (GdkEventButton *)event);
  } else {
    PopupData *data;

    data = g_new (PopupData, 1);
    data->button = GTK_WIDGET (button);
    data->window = header_bar->window;
    data->direction = direction;
    data->event = (GdkEventButton *)event;

    header_bar->navigation_buttons_menu_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT, 500,
                                                                      (GSourceFunc)menu_timeout_cb,
                                                                      data,
                                                                      (GDestroyNotify)g_free);
    g_source_set_name_by_id (header_bar->navigation_buttons_menu_timeout, "[epiphany] menu_timeout_cb");
  }

  return FALSE;
}

static gboolean
navigation_button_release_event_cb (GtkButton *button,
                                    GdkEvent  *event,
                                    gpointer   user_data)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (user_data);
  GActionGroup *action_group;
  GAction *action;
  EphyNavigationHistoryDirection direction;
  const gchar *action_name;

  if (header_bar->navigation_buttons_menu_timeout > 0) {
    g_source_remove (header_bar->navigation_buttons_menu_timeout);
    header_bar->navigation_buttons_menu_timeout = 0;
  }

  action_name = gtk_actionable_get_action_name (GTK_ACTIONABLE (button));
  action_group = gtk_widget_get_action_group (GTK_WIDGET (header_bar->window), "toolbar");

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
      popup_history_menu (GTK_WIDGET (button), header_bar->window,
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
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (user_data);

  if (header_bar->navigation_buttons_menu_timeout > 0) {
    g_source_remove (header_bar->navigation_buttons_menu_timeout);
    header_bar->navigation_buttons_menu_timeout = 0;
  }

  return G_SOURCE_REMOVE;
}

void
ephy_header_bar_change_combined_stop_reload_state (GSimpleAction *action,
                                                   GVariant      *loading,
                                                   gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyHeaderBar *header_bar;
  GtkWidget *image;

  if (g_variant_get_boolean (loading))
    image = gtk_image_new_from_icon_name ("process-stop-symbolic",
                                          GTK_ICON_SIZE_BUTTON);
  else
    image = gtk_image_new_from_icon_name ("view-refresh-symbolic",
                                          GTK_ICON_SIZE_BUTTON);

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  gtk_button_set_image (GTK_BUTTON (header_bar->combined_stop_reload_button),
                        image);
}

/**
 * update_bookmarked_status_cb:
 * @bookmark: an #EphyBookmark pbject
 * @toolbar: an #EphyToolbar widget
 *
 * Remove bookmarked status if the @bookmark was removed.
 *
 **/
static void
update_bookmarked_status_cb (EphyBookmark *bookmark,
                             EphyToolbar  *toolbar)
{
  EphyEmbed *embed;
  EphyWebView *view;
  const char *address;

  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_TOOLBAR (toolbar));

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (toolbar->window));
  view = ephy_embed_get_web_view (embed);

  address = ephy_web_view_get_address (view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) == 0) {
    ephy_location_entry_set_bookmarked_status (EPHY_LOCATION_ENTRY (toolbar->entry),
                                               FALSE);
  }
}

static void
add_bookmark_button_clicked_cb (EphyLocationEntry *entry,
                                gpointer          *user_data)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (user_data);
  EphyEmbed *embed;
  GtkWidget *popover;
  GdkRectangle rectangle;
  EphyBookmark *bookmark;
  const char *location;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (header_bar->window));

  location = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
  bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, location);
  if (!bookmark) {
    bookmark = ephy_bookmark_new (g_strdup (location),
                                  g_strdup (ephy_embed_get_title (embed)),
                                  g_sequence_new (g_free));

    g_signal_connect_object (bookmark, "removed",
                             G_CALLBACK (update_bookmarked_status_cb),
                             toolbar, 0);
    ephy_bookmarks_manager_add_bookmark (manager, bookmark);
    ephy_location_entry_set_bookmarked_status (entry, TRUE);
  }

  popover = gtk_popover_new (GTK_WIDGET (entry));
  gtk_entry_get_icon_area (GTK_ENTRY (entry),
                           GTK_ENTRY_ICON_SECONDARY,
                           &rectangle);
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rectangle);
  gtk_container_add (GTK_CONTAINER (popover),
                     ephy_bookmark_properties_grid_new (bookmark,
                                                        EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_POPOVER,
                                                        popover));
  gtk_widget_show (popover);
}

static void
ephy_header_bar_constructed (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);
  GtkWidget *box, *button;
  GtkMenu *menu;
  GMenu *page_menu;
  EphyDownloadsManager *downloads_manager;
  GtkBuilder *builder;
  EphyHistoryService *history_service;
  EphyEmbedShell *embed_shell;

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->constructed (object);

  g_signal_connect_swapped (header_bar->window, "notify::chrome",
                            G_CALLBACK (sync_chromes_visibility), header_bar);

  /* Back and Forward */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  header_bar->navigation_box = box;

  /* Back */
  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("go-previous-symbolic",
                        GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                  "toolbar.navigation-back");
  g_signal_connect (button, "button-press-event",
                    G_CALLBACK (navigation_button_press_event_cb), header_bar);
  g_signal_connect (button, "button-release-event",
                    G_CALLBACK (navigation_button_release_event_cb), header_bar);
  g_signal_connect (button, "leave-notify-event",
                    G_CALLBACK (navigation_leave_notify_event_cb), header_bar);
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
                    G_CALLBACK (navigation_button_press_event_cb), header_bar);
  g_signal_connect (button, "button-release-event",
                    G_CALLBACK (navigation_button_release_event_cb), header_bar);
  g_signal_connect (button, "leave-notify-event",
                    G_CALLBACK (navigation_leave_notify_event_cb), header_bar);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_widget_show (GTK_WIDGET (button));

  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "raised");
  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "linked");

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), box);

  /* Reload/Stop */
  button = gtk_button_new ();
  header_bar->combined_stop_reload_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                  "toolbar.combined-stop-reload");
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               "image-button");
  gtk_widget_show (GTK_WIDGET (button));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), button);

  /* New Tab */
  button = gtk_button_new ();
  header_bar->new_tab_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.new-tab");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), button);

  /* Title widget (location entry or title box) */
  embed_shell = ephy_embed_shell_get_default ();
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_title_box_new ());
  else
    header_bar->title_widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());
  gtk_widget_set_margin_start (GTK_WIDGET (header_bar->title_widget), 54);
  gtk_widget_set_margin_end (GTK_WIDGET (header_bar->title_widget), 54);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header_bar), GTK_WIDGET (header_bar->title_widget));
  gtk_widget_show (GTK_WIDGET (header_bar->title_widget));

  if (EPHY_IS_LOCATION_ENTRY (header_bar->title_widget)) {
    g_signal_connect_object (header_bar->title_widget,
                             "bookmark-clicked",
                             G_CALLBACK (add_bookmark_button_clicked_cb),
                             header_bar,
                             0);
  }

  /* Page Menu */
  button = gtk_menu_button_new ();
  header_bar->page_menu_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/menus.ui");
  page_menu = G_MENU (gtk_builder_get_object (builder, "page-menu"));
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), FALSE);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button),
                                  G_MENU_MODEL (page_menu));
  g_object_unref (builder);

  menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (button));
  gtk_widget_set_halign (GTK_WIDGET (menu), GTK_ALIGN_END);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), button);

  /* Bookmarks button */
  button = gtk_menu_button_new ();
  header_bar->bookmarks_button = button;
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("ephy-bookmarks-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), GTK_WIDGET (ephy_bookmarks_popover_new (header_bar->window)));

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), button);

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  header_bar->downloads_revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (header_bar->downloads_revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (header_bar->downloads_revealer),
                                 ephy_downloads_manager_get_downloads (downloads_manager) != NULL);

  header_bar->downloads_button = gtk_menu_button_new ();
  gtk_button_set_image (GTK_BUTTON (header_bar->downloads_button), ephy_downloads_progress_icon_new ());
  gtk_widget_set_valign (header_bar->downloads_button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (header_bar->downloads_revealer), header_bar->downloads_button);
  gtk_widget_show (header_bar->downloads_button);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    header_bar->downloads_popover = ephy_downloads_popover_new (header_bar->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (header_bar->downloads_button),
                                 header_bar->downloads_popover);
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

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), header_bar->downloads_revealer);
  gtk_widget_show (header_bar->downloads_revealer);

  history_service = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ()));

  g_signal_connect (history_service,
                    "cleared", G_CALLBACK (ephy_history_cleared_cb),
                    header_bar->window);
}

static void
ephy_header_bar_init (EphyHeaderBar *header_bar)
{
}

static void
ephy_header_bar_dispose (GObject *object)
{
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (object);

  if (header_bar->navigation_buttons_menu_timeout > 0) {
    g_source_remove (header_bar->navigation_buttons_menu_timeout);
    header_bar->navigation_buttons_menu_timeout = 0;
  }

  G_OBJECT_CLASS (ephy_header_bar_parent_class)->dispose (object);
}

static void
ephy_header_bar_class_init (EphyHeaderBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = ephy_header_bar_dispose;
  gobject_class->set_property = ephy_header_bar_set_property;
  gobject_class->get_property = ephy_header_bar_get_property;
  gobject_class->constructed = ephy_header_bar_constructed;

  object_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The header_bar's EphyWindow",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     object_properties);
}

GtkWidget *
ephy_header_bar_new (EphyWindow *window)
{
  g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

  return GTK_WIDGET (g_object_new (EPHY_TYPE_HEADER_BAR,
                                   "show-close-button", TRUE,
                                   "window", window,
                                   NULL));
}

EphyTitleWidget *
ephy_header_bar_get_title_widget (EphyHeaderBar *header_bar)
{
  return header_bar->title_widget;
}

GtkWidget *
ephy_header_bar_get_page_menu_button (EphyHeaderBar *header_bar)
{
  return header_bar->page_menu_button;
}

GtkWidget *
ephy_header_bar_get_new_tab_button (EphyHeaderBar *header_bar)
{
  return header_bar->new_tab_button;
}
