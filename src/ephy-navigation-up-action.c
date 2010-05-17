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
#include "ephy-navigation-up-action.h"

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

#define URL_DATA_KEY "GoURL"

static void ephy_navigation_up_action_init       (EphyNavigationUpAction *action);
static void ephy_navigation_up_action_class_init (EphyNavigationUpActionClass *klass);

G_DEFINE_TYPE (EphyNavigationUpAction, ephy_navigation_up_action, EPHY_TYPE_NAVIGATION_ACTION)

static void
activate_up_menu_item_cb (GtkWidget *menuitem,
			  EphyNavigationUpAction *action)
{
  EphyWindow *window;
  EphyEmbed *embed;
  char *url;

  window = _ephy_navigation_action_get_window (EPHY_NAVIGATION_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
  g_return_if_fail (url != NULL);

  ephy_link_open (EPHY_LINK (action), url, NULL,
		  ephy_gui_is_middle_click () ? EPHY_LINK_NEW_TAB : 0);
}

static GtkWidget *
build_dropdown_menu (EphyNavigationAction *nav_action)
{
  EphyNavigationUpAction *action;
  EphyWindow *window;
  EphyEmbed *embed;
  EphyHistory *history;
  GtkMenuShell *menu;
  GtkWidget *item;
  GSList *list, *l;
  char *url;

  action = EPHY_NAVIGATION_UP_ACTION (nav_action);
  window = _ephy_navigation_action_get_window (nav_action);
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_val_if_fail (embed != NULL, NULL);

  menu = GTK_MENU_SHELL (gtk_menu_new ());
  history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));

  list = ephy_web_view_get_go_up_list (ephy_embed_get_web_view (embed));
  for (l = list; l != NULL; l = l->next) {
    EphyNode *node;
    const char *title = NULL;

    url = g_strdup (l->data);

    if (url == NULL) continue;

    node = ephy_history_get_page (history, url);
    if (node != NULL) {
      title = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_TITLE);
    }

    item = _ephy_navigation_action_new_history_menu_item (title ? title : url, url);

    g_object_set_data_full (G_OBJECT (item), URL_DATA_KEY, url,
                            (GDestroyNotify) g_free);
    g_signal_connect (item, "activate",
                      G_CALLBACK (activate_up_menu_item_cb), action);

    gtk_menu_shell_append (menu, item);
    gtk_widget_show (item);
  }

  /* the list data has been consumed */
  g_slist_foreach (list, (GFunc) g_free, NULL);
  g_slist_free (list);

  return GTK_WIDGET (menu);
}

static void
action_activate (GtkAction *action)
{
  EphyWindow *window;
  EphyEmbed *embed;
  GSList *up_list;

  window = _ephy_navigation_action_get_window (EPHY_NAVIGATION_ACTION (action));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_return_if_fail (embed != NULL);

  up_list = ephy_web_view_get_go_up_list (ephy_embed_get_web_view (embed));
  ephy_link_open (EPHY_LINK (action),
		  up_list->data,
		  NULL,
		  ephy_gui_is_middle_click () ? EPHY_LINK_NEW_TAB : 0);
  g_slist_foreach (up_list, (GFunc) g_free, NULL);
  g_slist_free (up_list);
}

static void
ephy_navigation_up_action_init (EphyNavigationUpAction *action)
{
}

static void
ephy_navigation_up_action_class_init (EphyNavigationUpActionClass *klass)
{
  GtkActionClass *action_class = GTK_ACTION_CLASS (klass);
  EphyNavigationActionClass *nav_action_class = EPHY_NAVIGATION_ACTION_CLASS (klass);

  action_class->activate = action_activate;
  nav_action_class->build_dropdown_menu = build_dropdown_menu;
}
