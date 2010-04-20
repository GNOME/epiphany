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
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-gui.h"
#include "ephy-history.h"
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-type-builtins.h"
#include "ephy-window.h"

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define HISTORY_ITEM_DATA_KEY "HistoryItem"

#define EPHY_NAVIGATION_HISTORY_ACTION_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE ((object),				\
				EPHY_TYPE_NAVIGATION_HISTORY_ACTION,	\
				EphyNavigationHistoryActionPrivate))

typedef enum {
  WEBKIT_HISTORY_BACKWARD,
  WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;

struct _EphyNavigationHistoryActionPrivate {
  EphyNavigationHistoryDirection direction;
  EphyHistory *history;
};

enum {
  PROP_0,
  PROP_DIRECTION
};

static void ephy_navigation_history_action_init       (EphyNavigationHistoryAction *action);
static void ephy_navigation_history_action_class_init (EphyNavigationHistoryActionClass *klass);

G_DEFINE_TYPE (EphyNavigationHistoryAction, ephy_navigation_history_action, EPHY_TYPE_NAVIGATION_ACTION)

static void
set_new_back_history (EphyEmbed *source, EphyEmbed *dest, gint offset)
{
  WebKitWebView *source_view, *dest_view;
  WebKitWebBackForwardList* source_list, *dest_list;
  WebKitWebHistoryItem *item;
  GList *items;
  guint limit;
  guint i;

  g_return_if_fail (EPHY_IS_EMBED (source));
  g_return_if_fail (EPHY_IS_EMBED (dest));

  source_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (source);
  dest_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (dest);

  source_list = webkit_web_view_get_back_forward_list (source_view);
  dest_list = webkit_web_view_get_back_forward_list (dest_view);

  if (offset >= 0) {
    /* Copy the whole back history in this case (positive offset) */
    ephy_web_view_copy_back_history (ephy_embed_get_web_view (source),
                                     ephy_embed_get_web_view (dest));

    items = webkit_web_back_forward_list_get_forward_list_with_limit (source_list,
                                                                      EPHY_WEBKIT_BACK_FORWARD_LIMIT);
    limit = offset - 1;
  } else {
    items = webkit_web_back_forward_list_get_back_list_with_limit (source_list,
                                                                   EPHY_WEBKIT_BACK_FORWARD_LIMIT);
    limit = g_list_length (items) + offset;
  }

  /* Add the remaining items to the BackForward list */
  items = g_list_reverse (items);
  for (i = 0; i < limit; i++) {
    item = webkit_web_history_item_copy ((WebKitWebHistoryItem*)items->data);
    webkit_web_back_forward_list_add_item (dest_list, item);
    g_object_unref (item);

    items = items->next;
  }
  g_list_free (items);
}

static void
middle_click_handle_on_history_menu_item (EphyNavigationHistoryAction *action,
                                          EphyEmbed *embed,
                                          WebKitWebHistoryItem *item)
{
  EphyEmbed *new_embed = NULL;
  WebKitWebView *web_view;
  WebKitWebBackForwardList *history;
  GList *list;
  const gchar *url;
  guint current;
  gint offset;

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  /* Save old history and item's offset from current */
  history = webkit_web_view_get_back_forward_list (web_view);
  if (action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
    list = webkit_web_back_forward_list_get_back_list_with_limit (history,
                                                                  EPHY_WEBKIT_BACK_FORWARD_LIMIT);
    current = -1;
  } else {
    list = webkit_web_back_forward_list_get_forward_list_with_limit (history,
                                                                     EPHY_WEBKIT_BACK_FORWARD_LIMIT);
    current = g_list_length (list);
  }
  offset = current - g_list_index (list, item);

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  NULL,
                                  EPHY_NEW_TAB_IN_EXISTING_WINDOW |
                                  EPHY_NEW_TAB_DONT_COPY_HISTORY);
  g_return_if_fail (new_embed != NULL);

  /* We manually set the back history instead of trusting
     ephy_shell_new_tab because the logic is more complex than
     usual, due to handling also the forward history */
  set_new_back_history (embed, new_embed, offset);

  /* Load the new URL */
  url = webkit_web_history_item_get_original_uri (item);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), url);
}

static void
activate_back_or_forward_menu_item_cb (GtkWidget *menuitem,
				       EphyNavigationHistoryAction *action)
{
  WebKitWebHistoryItem *item;
  EphyWindow *window;
  EphyEmbed *embed;

  window = _ephy_navigation_action_get_window (EPHY_NAVIGATION_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  item = (WebKitWebHistoryItem*)g_object_get_data (G_OBJECT (menuitem), HISTORY_ITEM_DATA_KEY);
  g_return_if_fail (item != NULL);

  if (ephy_gui_is_middle_click ()) {
    middle_click_handle_on_history_menu_item (action, embed, item);
  } else {
    WebKitWebView *web_view;

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_go_to_back_forward_item (web_view, item);
  }
}

static void
select_menu_item_cb (GtkWidget *menuitem,
		     EphyNavigationHistoryAction *action)
{
  WebKitWebHistoryItem *item;

  item = (WebKitWebHistoryItem*)g_object_get_data (G_OBJECT (menuitem),
						   HISTORY_ITEM_DATA_KEY);
  if (item) {
    const char *url;
    EphyWindow *window;
    EphyNavigationAction *nav_action;
    GtkWidget *statusbar;
    guint statusbar_cid;

    url = webkit_web_history_item_get_uri (item);
    window = _ephy_navigation_action_get_window (EPHY_NAVIGATION_ACTION (action));
    statusbar = ephy_window_get_statusbar (window);

    /* Update status bar */
    nav_action = EPHY_NAVIGATION_ACTION (action);
    statusbar_cid = _ephy_navigation_action_get_statusbar_context_id (nav_action);
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), statusbar_cid, url);
  }
}

static void
deselect_menu_item_cb (GtkWidget *menuitem,
		       EphyNavigationAction *action)
{
  GtkWidget *statusbar;
  EphyWindow *window;
  EphyNavigationAction *nav_action;
  guint statusbar_cid;

  window = _ephy_navigation_action_get_window (EPHY_NAVIGATION_ACTION (action));
  statusbar = ephy_window_get_statusbar (window);

  /* Update status bar */
  nav_action = EPHY_NAVIGATION_ACTION (action);
  statusbar_cid = _ephy_navigation_action_get_statusbar_context_id (nav_action);
  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), statusbar_cid);
}

static void
ephy_history_cleared_cb (EphyHistory *history,
                         EphyNavigationHistoryAction *action)
{
  ephy_action_change_sensitivity_flags (GTK_ACTION (action), SENS_FLAG, TRUE);
}

static GList*
webkit_construct_history_list (WebKitWebView *web_view, WebKitHistoryType hist_type)
{
  WebKitWebBackForwardList *web_back_forward_list;
  GList *webkit_items;

  web_back_forward_list = webkit_web_view_get_back_forward_list (web_view);

  if (hist_type == WEBKIT_HISTORY_FORWARD) {
    webkit_items =
      g_list_reverse (webkit_web_back_forward_list_get_forward_list_with_limit (web_back_forward_list,
                                                                                EPHY_WEBKIT_BACK_FORWARD_LIMIT));
  } else {
    webkit_items =
      webkit_web_back_forward_list_get_back_list_with_limit (web_back_forward_list,
                                                             EPHY_WEBKIT_BACK_FORWARD_LIMIT);
  }

  return webkit_items;
}

static GtkWidget *
build_dropdown_menu (EphyNavigationAction *nav_action)
{
  EphyNavigationHistoryAction *action;
  EphyWindow *window;
  GtkMenuShell *menu;
  EphyEmbed *embed;
  GList *list, *l;
  WebKitWebView *web_view;

  action = EPHY_NAVIGATION_HISTORY_ACTION (nav_action);
  window = _ephy_navigation_action_get_window (nav_action);
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_val_if_fail (embed != NULL, NULL);

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  g_return_val_if_fail (web_view != NULL, NULL);

  if (action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
    list = webkit_construct_history_list (web_view,
                                          WEBKIT_HISTORY_BACKWARD);
  } else {
    list = webkit_construct_history_list (web_view,
                                          WEBKIT_HISTORY_FORWARD);
  }

  for (l = list; l != NULL; l = l->next) {
    GtkWidget *item;
    WebKitWebHistoryItem *hitem;
    const char *url;
    char *title;

    hitem = (WebKitWebHistoryItem*)l->data;
    url = webkit_web_history_item_get_uri (hitem);

    title = g_strdup (webkit_web_history_item_get_title (hitem));

    if ((title == NULL || g_strstrip (title)[0] == '\0'))
      item = _ephy_navigation_action_new_history_menu_item (url, url);
    else
      item = _ephy_navigation_action_new_history_menu_item (title, url);

    g_free (title);

    g_object_set_data_full (G_OBJECT (item), HISTORY_ITEM_DATA_KEY,
                            g_object_ref (hitem), g_object_unref);

    g_signal_connect (item, "activate",
                      G_CALLBACK (activate_back_or_forward_menu_item_cb),
                      action);
    g_signal_connect (item, "select",
                      G_CALLBACK (select_menu_item_cb),
                      action);
    g_signal_connect (item, "deselect",
                      G_CALLBACK (deselect_menu_item_cb),
                      action);

    gtk_menu_shell_append (menu, item);
    gtk_widget_show_all (item);
  }

  g_list_free (list);

  return GTK_WIDGET (menu);
}

static void
action_activate (GtkAction *action)
{
  EphyNavigationHistoryAction *history_action;
  EphyWindow *window;
  EphyEmbed *embed;
  WebKitWebView *web_view;

  history_action = EPHY_NAVIGATION_HISTORY_ACTION (action);
  window = _ephy_navigation_action_get_window (EPHY_NAVIGATION_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (history_action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_BACK) {
    if (ephy_gui_is_middle_click ()) {
      embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  NULL,
                                  EPHY_NEW_TAB_IN_EXISTING_WINDOW);
      web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    }
    webkit_web_view_go_back (web_view);
  } else if (history_action->priv->direction == EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD) {
    if (ephy_gui_is_middle_click ()) {
      const char *forward_uri;
      WebKitWebHistoryItem *forward_item;
      WebKitWebBackForwardList *history;

      /* Forward history is not copied when opening
         a new tab, so get the forward URI manually
         and load it */
      history = webkit_web_view_get_back_forward_list (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
      forward_item = webkit_web_back_forward_list_get_forward_item (history);
      forward_uri = webkit_web_history_item_get_original_uri (forward_item);

      embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                  embed,
                                  NULL,
                                  EPHY_NEW_TAB_IN_EXISTING_WINDOW);

      web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
      webkit_web_view_load_uri (web_view, forward_uri);
    } else {
      webkit_web_view_go_forward (web_view);
    }
  }
}

static void
ephy_navigation_history_action_init (EphyNavigationHistoryAction *action)
{
  EphyHistory *history;

  action->priv = EPHY_NAVIGATION_HISTORY_ACTION_GET_PRIVATE (action);

  history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
  action->priv->history = EPHY_HISTORY (g_object_ref (history));

  g_signal_connect (action->priv->history,
                    "cleared", G_CALLBACK (ephy_history_cleared_cb),
                    action);
}

static void
ephy_navigation_history_action_finalize (GObject *object)
{
  EphyNavigationHistoryAction *action = EPHY_NAVIGATION_HISTORY_ACTION (object);

  g_signal_handlers_disconnect_by_func (action->priv->history,
                                        ephy_history_cleared_cb,
                                        action);
  g_object_unref (action->priv->history);
  action->priv->history = NULL;

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

static void
ephy_navigation_history_action_class_init (EphyNavigationHistoryActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkActionClass *action_class = GTK_ACTION_CLASS (klass);
  EphyNavigationActionClass *nav_action_class = EPHY_NAVIGATION_ACTION_CLASS (klass);

  object_class->finalize = ephy_navigation_history_action_finalize;
  object_class->set_property = ephy_navigation_history_action_set_property;
  object_class->get_property = ephy_navigation_history_action_get_property;

  action_class->activate = action_activate;

  nav_action_class->build_dropdown_menu = build_dropdown_menu;

  g_object_class_install_property (object_class,
				   PROP_DIRECTION,
				   g_param_spec_int ("direction", NULL, NULL,
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (EphyNavigationHistoryActionPrivate));
}
