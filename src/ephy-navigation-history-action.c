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
ephy_history_cleared_cb (EphyHistory *history,
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

  object_class->finalize = ephy_navigation_history_action_finalize;
  object_class->set_property = ephy_navigation_history_action_set_property;
  object_class->get_property = ephy_navigation_history_action_get_property;

  action_class->activate = action_activate;

  g_object_class_install_property (object_class,
				   PROP_DIRECTION,
				   g_param_spec_int ("direction", NULL, NULL,
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (EphyNavigationHistoryActionPrivate));
}
