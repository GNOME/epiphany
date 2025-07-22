/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2025 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "ephy-page-menu-button.h"

#include "ephy-desktop-utils.h"
#include "ephy-embed-shell.h"
#include "ephy-flatpak-utils.h"

struct _EphyPageMenuButton {
  AdwBin parent_instance;

  GtkWidget *menu_button;
  GMenu *page_menu;
  GtkWidget *page_menu_popover;
  GtkWidget *zoom_level;
};

G_DEFINE_FINAL_TYPE (EphyPageMenuButton, ephy_page_menu_button, ADW_TYPE_BIN)

static gboolean
remove_menu_item (GMenu      *menu,
                  const char *action_name)
{
  int i, n;

  n = g_menu_model_get_n_items (G_MENU_MODEL (menu));

  for (i = 0; i < n; i++) {
    g_autofree char *item_action = NULL;
    g_autofree char *submenu_id = NULL;
    g_autoptr (GMenuModel) section = NULL;

    g_menu_model_get_item_attribute (G_MENU_MODEL (menu),
                                     i,
                                     G_MENU_ATTRIBUTE_ACTION,
                                     "s",
                                     &item_action);

    if (!g_strcmp0 (action_name, item_action)) {
      g_menu_remove (menu, i);
      return TRUE;
    }

    /* FIXME: this isn't particularly great. Maybe we should have custom
     * attributes for everything like show-in-app-mode etc? */
    g_menu_model_get_item_attribute (G_MENU_MODEL (menu),
                                     i,
                                     "ephy-submenu-id",
                                     "s",
                                     &submenu_id);

    if (!g_strcmp0 (action_name, submenu_id)) {
      g_menu_remove (menu, i);
      return TRUE;
    }

    section = g_menu_model_get_item_link (G_MENU_MODEL (menu),
                                          i,
                                          G_MENU_LINK_SECTION);

    if (!G_IS_MENU (section))
      continue;

    if (remove_menu_item (G_MENU (section), action_name))
      return TRUE;
  }

  return FALSE;
}

static void
ephy_page_menu_button_class_init (EphyPageMenuButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/page-menu-button.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPageMenuButton, menu_button);
  gtk_widget_class_bind_template_child (widget_class, EphyPageMenuButton, page_menu);
  gtk_widget_class_bind_template_child (widget_class, EphyPageMenuButton, page_menu_popover);
}

static void
ephy_page_menu_button_init (EphyPageMenuButton *self)
{
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();

  gtk_widget_init_template (GTK_WIDGET (self));

  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    remove_menu_item (self->page_menu, "app.new-incognito");
    remove_menu_item (self->page_menu, "app.reopen-closed-tab");
    remove_menu_item (self->page_menu, "win.save-as-application");
    remove_menu_item (self->page_menu, "win.open-application-manager");
    remove_menu_item (self->page_menu, "win.encoding");
    remove_menu_item (self->page_menu, "app.shortcuts");
    remove_menu_item (self->page_menu, "app.help");
    remove_menu_item (self->page_menu, "app.firefox-sync-dialog");
    remove_menu_item (self->page_menu, "import-export");
    remove_menu_item (self->page_menu, "webapps");
  }

  if (ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_APPLICATION
      || !ephy_is_running_inside_sandbox ()) {
    remove_menu_item (self->page_menu, "app.run-in-background");
    remove_menu_item (self->page_menu, "app.quit");
  }

  if (!ephy_can_install_web_apps ()) {
    remove_menu_item (self->page_menu, "win.save-as-application");
    remove_menu_item (self->page_menu, "win.open-application-manager");
  }

  if (is_desktop_pantheon ()) {
    remove_menu_item (self->page_menu, "app.about");

    if (ephy_is_running_inside_sandbox ())
      remove_menu_item (self->page_menu, "app.help");

    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->menu_button), "open-menu");
    gtk_widget_add_css_class (self->menu_button, "toolbar-button");
  }

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->menu_button), self->page_menu_popover);
}

EphyPageMenuButton *
ephy_page_menu_button_new (void)
{
  return g_object_new (EPHY_TYPE_PAGE_MENU_BUTTON, NULL);
}

void
ephy_page_menu_button_popup (EphyPageMenuButton *self)
{
  gtk_menu_button_popup (GTK_MENU_BUTTON (self->menu_button));
}

void
ephy_page_menu_button_popdown (EphyPageMenuButton *self)
{
  gtk_menu_button_popdown (GTK_MENU_BUTTON (self->menu_button));
}
