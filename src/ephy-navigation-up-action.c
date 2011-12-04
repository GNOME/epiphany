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

  action_class->activate = action_activate;
}
