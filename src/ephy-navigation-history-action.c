/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2008 Jan Alonzo
 *  Copyright © 2009 Igalia S.L.
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
#include "ephy-navigation-history-action.h"

#include "ephy-action-helper.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-helpers.h"
#include "ephy-gui.h"
#include "ephy-history-service.h"
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-type-builtins.h"
#include "ephy-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#define EPHY_NAVIGATION_HISTORY_ACTION_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE ((object),				\
				EPHY_TYPE_NAVIGATION_HISTORY_ACTION,	\
				EphyNavigationHistoryActionPrivate))

struct _EphyNavigationHistoryActionPrivate {
  EphyNavigationHistoryDirection direction;
  EphyHistoryService *history;
  guint menu_timeout;
};

enum {
  PROP_0,
  PROP_DIRECTION
};

#define MAX_LABEL_LENGTH 48
#define HISTORY_ITEM_DATA_KEY "history-item-data-key"

typedef enum {
  WEBKIT_HISTORY_BACKWARD,
  WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;


static void ephy_navigation_history_action_init       (EphyNavigationHistoryAction *action);
static void ephy_navigation_history_action_class_init (EphyNavigationHistoryActionClass *klass);

G_DEFINE_TYPE (EphyNavigationHistoryAction, ephy_navigation_history_action, EPHY_TYPE_LINK_ACTION)

static void
ephy_history_cleared_cb (EphyHistoryService *history,
                         EphyNavigationHistoryAction *action)
{
  ephy_action_change_sensitivity_flags (GTK_ACTION (action), SENS_FLAG, TRUE);
}

static void
action_activate (GtkAction *action)
{
  EphyNavigationHistoryAction *history_action;
  EphyWindow *window;
  EphyEmbed *embed;
  WebKitWebView *web_view;

  history_action = EPHY_NAVIGATION_HISTORY_ACTION (action);
  window = ephy_window_action_get_window (EPHY_WINDOW_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  /* We use ephy_link_action_get_button on top of
   * ephy_gui_is_middle_click because of the hacks we have to do to
   * fake middle clicks on tool buttons. Read the documentation of
   * ephy_link_action_get_button for more details. */
  if (history_action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
    if (ephy_gui_is_middle_click () ||
        ephy_link_action_get_button (EPHY_LINK_ACTION (history_action)) == 2) {
      const char *back_uri;
      WebKitBackForwardList *history;
      WebKitBackForwardListItem *back_item;

      /* FIXME: in WebKit2 the back/forward list is immutable, so we are not able to
       * copy it. Ideally the webkit1 code path should also work for webkit2. */

      history = webkit_web_view_get_back_forward_list (web_view);
      back_item = webkit_back_forward_list_get_back_item (history);
      back_uri = webkit_back_forward_list_item_get_original_uri (back_item);

      embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  NULL,
                                  0);

      web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
      webkit_web_view_load_uri (web_view, back_uri);
      gtk_widget_grab_focus (GTK_WIDGET (embed));
      return;
    }

    webkit_web_view_go_back (web_view);
    gtk_widget_grab_focus (GTK_WIDGET (embed));
  } else if (history_action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD) {
    if (ephy_gui_is_middle_click () ||
        ephy_link_action_get_button (EPHY_LINK_ACTION (history_action)) == 2) {
      const char *forward_uri;
      WebKitBackForwardList *history;
      WebKitBackForwardListItem *forward_item;

      /* Forward history is not copied when opening
         a new tab, so get the forward URI manually
         and load it */
      history = webkit_web_view_get_back_forward_list (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
      forward_item = webkit_back_forward_list_get_forward_item (history);
      forward_uri = webkit_back_forward_list_item_get_original_uri (forward_item);

      embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  0);

      web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
      webkit_web_view_load_uri (web_view, forward_uri);
    } else {
      webkit_web_view_go_forward (web_view);
      gtk_widget_grab_focus (GTK_WIDGET (embed));
    }
  }
}

static void
ephy_navigation_history_action_init (EphyNavigationHistoryAction *action)
{
  action->priv = EPHY_NAVIGATION_HISTORY_ACTION_GET_PRIVATE (action);

  action->priv->history = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ()));

  action->priv->menu_timeout = 0;

  g_signal_connect (action->priv->history,
                    "cleared", G_CALLBACK (ephy_history_cleared_cb),
                    action);
}

static void
ephy_navigation_history_action_finalize (GObject *object)
{
  EphyNavigationHistoryAction *action = EPHY_NAVIGATION_HISTORY_ACTION (object);

  if (action->priv->menu_timeout > 0)
    g_source_remove (action->priv->menu_timeout);

  g_signal_handlers_disconnect_by_func (action->priv->history,
                                        ephy_history_cleared_cb,
                                        action);

  G_OBJECT_CLASS (ephy_navigation_history_action_parent_class)->finalize (object);
}

static void
ephy_navigation_history_action_set_property (GObject *object,
					     guint prop_id,
					     const GValue *value,
					     GParamSpec *pspec)
{
  EphyNavigationHistoryAction *nav = EPHY_NAVIGATION_HISTORY_ACTION (object);

  switch (prop_id) {
  case PROP_DIRECTION:
    nav->priv->direction = g_value_get_int (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_navigation_history_action_get_property (GObject *object,
					     guint prop_id,
					     GValue *value,
					     GParamSpec *pspec)
{
  EphyNavigationHistoryAction *nav = EPHY_NAVIGATION_HISTORY_ACTION (object);

  switch (prop_id) {
  case PROP_DIRECTION:
    g_value_set_int (value, nav->priv->direction);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static gboolean
item_enter_notify_event_cb (GtkWidget *widget,
                            GdkEvent *event,
                            EphyWebView *view)
{
  char *text;

  text = g_object_get_data (G_OBJECT (widget), "link-message");
  ephy_web_view_set_link_message (view, text);

  return FALSE;
}

static gboolean
item_leave_notify_event_cb (GtkWidget *widget,
                            GdkEvent *event,
                            EphyWebView *view)
{
  ephy_web_view_set_link_message (view, NULL);
  return FALSE;
}

static void
icon_loaded_cb (GObject *source,
                GAsyncResult *result,
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
                       const char *origtext,
                       const char *address)
{
  GtkWidget *item;
  GtkLabel *label;
  WebKitFaviconDatabase* database;
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

  g_object_set_data_full (G_OBJECT (item), "link-message", g_strdup (address), (GDestroyNotify) g_free);

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
                      gint offset)
{
  /* TODO: WebKitBackForwardList: In WebKit2 WebKitBackForwardList can't be modified */
}

static void
middle_click_handle_on_history_menu_item (EphyNavigationHistoryAction *action,
                                          EphyEmbed *embed,
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

static void
activate_menu_item_cb (GtkWidget *menuitem,
                       EphyNavigationHistoryAction *action)
{
  WebKitBackForwardListItem *item;
  EphyWindow *window;
  EphyEmbed *embed;

  window = ephy_window_action_get_window (EPHY_WINDOW_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  item = (WebKitBackForwardListItem *) g_object_get_data (G_OBJECT (menuitem), HISTORY_ITEM_DATA_KEY);
  g_return_if_fail (item != NULL);

  if (ephy_gui_is_middle_click ())
    middle_click_handle_on_history_menu_item (action, embed, item);
  else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_list_item (web_view, item);
  }
}

static GList *
construct_webkit_history_list (WebKitWebView *web_view,
                               WebKitHistoryType hist_type,
                               int limit)
{
  WebKitBackForwardList *back_forward_list;

  back_forward_list = webkit_web_view_get_back_forward_list (web_view);
  return hist_type == WEBKIT_HISTORY_FORWARD ?
    g_list_reverse (webkit_back_forward_list_get_forward_list_with_limit (back_forward_list, limit)) :
    webkit_back_forward_list_get_back_list_with_limit (back_forward_list, limit);
}

static GtkWidget *
build_dropdown_menu (EphyNavigationHistoryAction *action)
{
  EphyWindow *window;
  GtkMenuShell *menu;
  EphyEmbed *embed;
  GList *list, *l;
  WebKitWebView *web_view;

  window = ephy_window_action_get_window (EPHY_WINDOW_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_val_if_fail (embed != NULL, NULL);

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK)
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

    hitem = (WebKitBackForwardListItem *) l->data;
    uri = webkit_back_forward_list_item_get_uri (hitem);
    title = g_strdup (webkit_back_forward_list_item_get_title (hitem));

    if (title == NULL || g_strstrip (title)[0] == '\0')
      item = new_history_menu_item (EPHY_WEB_VIEW (web_view), uri, uri);
    else
      item = new_history_menu_item (EPHY_WEB_VIEW (web_view), title, uri);

    g_free (title);

    g_object_set_data_full (G_OBJECT (item), HISTORY_ITEM_DATA_KEY,
                            g_object_ref (hitem), g_object_unref);

    g_signal_connect (item, "activate",
                      G_CALLBACK (activate_menu_item_cb), action);

    gtk_menu_shell_append (menu, item);
    gtk_widget_show_all (item);
  }

  g_list_free (list);

  return GTK_WIDGET (menu);
}

typedef struct {
  EphyNavigationHistoryAction *action;
  GdkEventButton *event;
  GtkWidget *widget;
} PopupData;

static GtkWidget *
popup_history_menu (EphyNavigationHistoryAction *action,
                    GtkWidget *widget,
                    GdkEventButton *event)
{
    GtkWidget *menu;

    menu = build_dropdown_menu (action);
    gtk_menu_popup (GTK_MENU (menu),
                    NULL, NULL,
                    ephy_gui_menu_position_under_widget, widget,
                    event->button, event->time);

    return menu;
}

static gboolean
menu_timeout_cb (PopupData *data)
{
  if (data != NULL && data->widget)
    popup_history_menu (data->action, data->widget, data->event);

  return FALSE;
}

static gboolean
tool_button_press_event_cb (GtkButton *button,
                            GdkEventButton *event,
                            EphyNavigationHistoryAction *action)
{
  if (event->button == 1) {
    PopupData *data;

    data = g_new (PopupData, 1);
    data->action = action;
    data->event = event;
    data->widget = GTK_WIDGET (button);

    action->priv->menu_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT, 500,
                                                     (GSourceFunc) menu_timeout_cb,
                                                     data,
                                                     (GDestroyNotify) g_free);
    g_source_set_name_by_id (action->priv->menu_timeout, "[epiphany] menu_timeout_cb");
  } else if (event->button == 3) {
    popup_history_menu (action, GTK_WIDGET (button), event);
    return TRUE;
  }
  return FALSE;
}

static gboolean
tool_leave_notify_event_cb (GtkButton *button,
                            GdkEvent *event,
                            EphyNavigationHistoryAction *action)
{
  if (action->priv->menu_timeout > 0)
    g_source_remove (action->priv->menu_timeout);

  action->priv->menu_timeout = 0;
  return FALSE;
}

static void
connect_proxy (GtkAction *gaction,
               GtkWidget *proxy)
{
  g_signal_connect (proxy, "button-press-event",
                    G_CALLBACK (tool_button_press_event_cb), gaction);
  g_signal_connect (proxy, "button-release-event",
                    G_CALLBACK (tool_leave_notify_event_cb), gaction);
  g_signal_connect (proxy, "leave-notify-event",
                    G_CALLBACK (tool_leave_notify_event_cb), gaction);

  GTK_ACTION_CLASS (ephy_navigation_history_action_parent_class)->connect_proxy (gaction, proxy);
}

static void
disconnect_proxy (GtkAction *gaction,
                  GtkWidget *proxy)
{
  g_signal_handlers_disconnect_by_func (proxy,
                    G_CALLBACK (tool_button_press_event_cb), gaction);
  g_signal_handlers_disconnect_by_func (proxy,
                    G_CALLBACK (tool_leave_notify_event_cb), gaction);

  GTK_ACTION_CLASS (ephy_navigation_history_action_parent_class)->disconnect_proxy (gaction, proxy);
}

static void
ephy_navigation_history_action_class_init (EphyNavigationHistoryActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkActionClass *action_class = GTK_ACTION_CLASS (klass);

  object_class->finalize = ephy_navigation_history_action_finalize;
  object_class->set_property = ephy_navigation_history_action_set_property;
  object_class->get_property = ephy_navigation_history_action_get_property;

  action_class->activate = action_activate;
  action_class->connect_proxy = connect_proxy;
  action_class->disconnect_proxy = disconnect_proxy;

  g_object_class_install_property (object_class,
				   PROP_DIRECTION,
				   g_param_spec_int ("direction", NULL, NULL,
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (EphyNavigationHistoryActionPrivate));
}
