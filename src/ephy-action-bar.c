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

#include "ephy-action-bar.h"

#include "ephy-downloads-paintable.h"
#include "ephy-downloads-popover.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-helpers.h"
#include "ephy-page-menu-button.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-tab-view.h"

#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

struct _EphyActionBar {
  AdwBin parent_instance;

  EphyWindow *window;
  GtkWidget *toolbar;
  GtkWidget *menu_button;
  AdwTabButton *tab_button;
  GtkWidget *navigation_back;
  GtkWidget *navigation_forward;
  GCancellable *cancellable;
  GtkWidget *history_menu;

  /* Downloads */
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;
  GtkWidget *downloads_icon;
  GdkPaintable *downloads_paintable;
  guint downloads_button_attention_timeout_id;
};

G_DEFINE_FINAL_TYPE (EphyActionBar, ephy_action_bar, ADW_TYPE_BIN)

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

static void
middle_click_handle_on_history_menu_item (EphyEmbed                 *embed,
                                          WebKitBackForwardListItem *item)
{
  EphyEmbed *new_embed = NULL;
  const gchar *url;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
                                  embed,
                                  0);
  g_assert (new_embed);

  /* Load the new URL */
  url = webkit_back_forward_list_item_get_original_uri (item);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), url);
}

static void
history_row_released_cb (GtkGesture    *gesture,
                         int            n_click,
                         double         x,
                         double         y,
                         EphyActionBar *action_bar)
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

  root = gtk_widget_get_root (GTK_WIDGET (action_bar));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (root));

  item = WEBKIT_BACK_FORWARD_LIST_ITEM (g_object_get_data (G_OBJECT (widget), HISTORY_ITEM_DATA_KEY));

  if (button == GDK_BUTTON_MIDDLE) {
    middle_click_handle_on_history_menu_item (embed, item);
  } else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_list_item (web_view, item);

    gtk_popover_popdown (GTK_POPOVER (action_bar->history_menu));
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
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkTexture) icon_texture = webkit_favicon_database_get_favicon_finish (database, result, &error);
  int scale;

  if (error) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;

    g_debug ("Could not get icon: %s", error->message);
    return;
  }

  if (!icon_texture)
    return;

  scale = gtk_widget_get_scale_factor (image);
  favicon = ephy_favicon_get_from_texture_scaled (icon_texture, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
  if (favicon)
    gtk_image_set_from_gicon (GTK_IMAGE (image), favicon);
}

static GtkWidget *
build_history_row (EphyActionBar             *action_bar,
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
                                       action_bar->cancellable,
                                       (GAsyncReadyCallback)icon_loaded_cb,
                                       g_object_ref (icon));

  g_object_set_data_full (G_OBJECT (row), "link-message",
                          g_strdup (uri), (GDestroyNotify)g_free);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (history_row_enter_cb), action_bar);
  g_signal_connect (controller, "leave", G_CALLBACK (history_row_leave_cb), action_bar);
  gtk_widget_add_controller (row, controller);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (history_row_released_cb), action_bar);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (gesture));

  return row;
}

static void
build_history_menu (EphyActionBar                  *action_bar,
                    GtkWidget                      *parent,
                    EphyNavigationHistoryDirection  direction)
{
  GtkWidget *popover, *listbox;
  GtkRoot *root;
  EphyEmbed *embed;
  WebKitWebView *web_view;
  g_autoptr (GList) list = NULL;
  GList *l;

  root = gtk_widget_get_root (GTK_WIDGET (action_bar));
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
    GtkWidget *row = build_history_row (action_bar, l->data);

    gtk_list_box_append (GTK_LIST_BOX (listbox), row);
  }

  action_bar->history_menu = popover;
}

static void
history_menu_closed_cb (EphyActionBar *action_bar)
{
  GtkWidget *parent = gtk_widget_get_parent (action_bar->history_menu);

  g_clear_pointer (&action_bar->history_menu, gtk_widget_unparent);
  gtk_widget_unset_state_flags (parent, GTK_STATE_FLAG_CHECKED);

  g_cancellable_cancel (action_bar->cancellable);
  g_clear_object (&action_bar->cancellable);
  action_bar->cancellable = g_cancellable_new ();
}

static void
handle_history_menu (EphyActionBar *action_bar,
                     double         x,
                     double         y,
                     GtkGesture    *gesture)
{
  GtkWidget *widget;
  EphyNavigationHistoryDirection direction;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  if (widget == action_bar->navigation_back)
    direction = EPHY_NAVIGATION_HISTORY_DIRECTION_BACK;
  else if (widget == action_bar->navigation_forward)
    direction = EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD;
  else
    g_assert_not_reached ();

  build_history_menu (action_bar, widget, direction);

  gtk_popover_popup (GTK_POPOVER (action_bar->history_menu));
  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_CHECKED, FALSE);

  g_signal_connect_swapped (action_bar->history_menu, "closed",
                            G_CALLBACK (history_menu_closed_cb), action_bar);

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
long_pressed_cb (GtkGesture    *gesture,
                 double         x,
                 double         y,
                 EphyActionBar *action_bar)
{
  handle_history_menu (action_bar, x, y, gesture);
}

static void
right_click_pressed_cb (GtkGesture    *gesture,
                        int            n_click,
                        double         x,
                        double         y,
                        EphyActionBar *action_bar)
{
  handle_history_menu (action_bar, x, y, gesture);
}

static void
middle_click_pressed_cb (GtkGesture *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
middle_click_released_cb (GtkGesture    *gesture,
                          int            n_click,
                          double         x,
                          double         y,
                          EphyActionBar *action_bar)
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

  if (widget == action_bar->navigation_back)
    action_name = "navigation-back-new-tab";
  else if (widget == action_bar->navigation_forward)
    action_name = "navigation-forward-new-tab";
  else
    g_assert_not_reached ();

  window = EPHY_WINDOW (gtk_widget_get_root (widget));
  action_group = ephy_window_get_action_group (window, "toolbar");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), action_name);
  g_action_activate (action, NULL);
}

static void
add_attention_timeout_cb (EphyActionBar *self)
{
  gtk_widget_remove_css_class (self->downloads_icon, "accent");
  self->downloads_button_attention_timeout_id = 0;
}

static void
add_attention (EphyActionBar *self)
{
  g_clear_handle_id (&self->downloads_button_attention_timeout_id, g_source_remove);

  gtk_widget_add_css_class (self->downloads_icon, "accent");
  self->downloads_button_attention_timeout_id = g_timeout_add_once (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                                                                    (GSourceOnceFunc)add_attention_timeout_cb,
                                                                    self);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyActionBar        *action_bar)
{
  if (!action_bar->downloads_popover) {
    action_bar->downloads_popover = ephy_downloads_popover_new ();
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar->downloads_button),
                                 action_bar->downloads_popover);
  }

  add_attention (action_bar);

  if (!gtk_widget_get_parent (action_bar->downloads_button))
    gtk_box_append (GTK_BOX (action_bar->toolbar), action_bar->downloads_button);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyActionBar        *action_bar)
{
  ephy_downloads_paintable_animate_done (EPHY_DOWNLOADS_PAINTABLE (action_bar->downloads_paintable));
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload         *download,
                     EphyActionBar        *action_bar)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_widget_unparent (action_bar->downloads_button);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyActionBar        *action_bar)
{
  gdouble fraction = ephy_downloads_manager_get_estimated_progress (manager);

  g_object_set (action_bar->downloads_paintable, "progress", fraction, NULL);
}

static void
show_downloads_cb (EphyDownloadsManager *manager,
                   EphyActionBar        *action_bar)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (action_bar)))
    gtk_menu_button_popup (GTK_MENU_BUTTON (action_bar->downloads_button));
}

static void
ephy_action_bar_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      action_bar->window = EPHY_WINDOW (g_value_get_object (value));
      g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_action_bar_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);

  switch (property_id) {
    case PROP_WINDOW:
      g_value_set_object (value, action_bar->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_action_bar_constructed (GObject *object)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);
  EphyTabView *view;

  G_OBJECT_CLASS (ephy_action_bar_parent_class)->constructed (object);

  view = ephy_window_get_tab_view (action_bar->window);

  adw_tab_button_set_view (action_bar->tab_button,
                           ephy_tab_view_get_tab_view (view));
}

static void
ephy_action_bar_dispose (GObject *object)
{
  EphyActionBar *action_bar = EPHY_ACTION_BAR (object);

  g_clear_pointer (&action_bar->history_menu, gtk_widget_unparent);

  g_cancellable_cancel (action_bar->cancellable);
  g_clear_object (&action_bar->cancellable);

  G_OBJECT_CLASS (ephy_action_bar_parent_class)->dispose (object);
}

static void
ephy_action_bar_class_init (EphyActionBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = ephy_action_bar_set_property;
  gobject_class->get_property = ephy_action_bar_get_property;
  gobject_class->constructed = ephy_action_bar_constructed;
  gobject_class->dispose = ephy_action_bar_dispose;

  object_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         NULL, NULL,
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     object_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/action-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, toolbar);
  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, tab_button);
  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, navigation_back);
  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, navigation_forward);
  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, menu_button);
  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, downloads_button);
  gtk_widget_class_bind_template_child (widget_class, EphyActionBar, downloads_icon);

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
ephy_action_bar_init (EphyActionBar *action_bar)
{
  GObject *object = G_OBJECT (action_bar);
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();
  EphyDownloadsManager *downloads_manager;
  EphyEmbedShellMode mode;

  gtk_widget_init_template (GTK_WIDGET (action_bar));

  mode = ephy_embed_shell_get_mode (embed_shell);
  gtk_widget_set_visible (GTK_WIDGET (action_bar->tab_button),
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (embed_shell);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    action_bar->downloads_popover = ephy_downloads_popover_new ();
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar->downloads_button), action_bar->downloads_popover);
  } else {
    gtk_widget_unparent (action_bar->downloads_button);
  }

  action_bar->downloads_paintable = ephy_downloads_paintable_new (action_bar->downloads_icon);
  gtk_image_set_from_paintable (GTK_IMAGE (action_bar->downloads_icon),
                                action_bar->downloads_paintable);


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
  g_signal_connect_object (downloads_manager, "show-downloads",
                           G_CALLBACK (show_downloads_cb),
                           object, 0);
}

EphyActionBar *
ephy_action_bar_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_ACTION_BAR,
                       "window", window,
                       NULL);
}
