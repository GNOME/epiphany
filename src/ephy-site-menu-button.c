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

#include "ephy-site-menu-button.h"

#include "ephy-bookmarks-manager.h"
#include "ephy-embed.h"
#include "ephy-shell.h"
#include "ephy-web-view.h"
#include "ephy-window.h"

#include <webkit/webkit.h>

struct _EphySiteMenuButton {
  GtkButton parent_instance;

  GMenu *menu_model;
  GMenuModel *items_section;
  GtkWidget *popover_menu;
  GtkWidget *zoom_level;
  GtkPathPaintable *paintable;

  char *description;

  guint timeout_id;
  gboolean do_animation;
  gboolean is_animating;
  unsigned int prev_state;
};

G_DEFINE_FINAL_TYPE (EphySiteMenuButton, ephy_site_menu_button, GTK_TYPE_BUTTON)

void ephy_site_menu_button_update_bookmark_item (EphySiteMenuButton *self,
                                                 gboolean            has_bookmark);

static void
on_clicked (GtkWidget *button,
            gpointer   user_data)
{
  EphySiteMenuButton *self = EPHY_SITE_MENU_BUTTON (user_data);

  gtk_popover_popup (GTK_POPOVER (self->popover_menu));
}

static void
on_bookmark_added (EphySiteMenuButton *self,
                   EphyBookmark       *bookmark)
{
  GtkWidget *window = gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW);
  EphyEmbed *embed = ephy_window_get_active_embed (EPHY_WINDOW (window));
  EphyWebView *web_view = ephy_embed_get_web_view (embed);
  const char *address = ephy_web_view_get_address (web_view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) != 0)
    return;

  ephy_site_menu_button_update_bookmark_item (self, TRUE);
}

static void
on_bookmark_removed (EphySiteMenuButton *self,
                     EphyBookmark       *bookmark)
{
  GtkWidget *window = gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW);
  EphyEmbed *embed = ephy_window_get_active_embed (EPHY_WINDOW (window));
  EphyWebView *web_view = ephy_embed_get_web_view (embed);
  const char *address = ephy_web_view_get_address (web_view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) != 0)
    return;

  ephy_site_menu_button_update_bookmark_item (self, FALSE);
}

static void
ephy_site_menu_button_dispose (GObject *object)
{
  EphySiteMenuButton *self = EPHY_SITE_MENU_BUTTON (object);

  g_clear_handle_id (&self->timeout_id, g_source_remove);

  gtk_widget_unparent (self->popover_menu);

  G_OBJECT_CLASS (ephy_site_menu_button_parent_class)->dispose (object);
}

static void
ephy_site_menu_button_class_init (EphySiteMenuButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/site-menu-button.ui");

  object_class->dispose = ephy_site_menu_button_dispose;

  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, menu_model);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, items_section);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, popover_menu);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, zoom_level);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, paintable);

  gtk_widget_class_bind_template_callback (widget_class, on_clicked);
}

static void
ephy_site_menu_button_init (EphySiteMenuButton *self)
{
  EphyShell *shell = ephy_shell_get_default ();
  EphyBookmarksManager *manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_menu_append_section (self->menu_model, NULL, self->items_section);

  gtk_widget_set_parent (self->popover_menu, GTK_WIDGET (self));

  if (!EPHY_IS_SHELL (shell))
    return;

  manager = ephy_shell_get_bookmarks_manager (shell);
  g_signal_connect_object (manager, "bookmark-added",
                           G_CALLBACK (on_bookmark_added), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (manager, "bookmark-removed",
                           G_CALLBACK (on_bookmark_removed), self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
ephy_site_menu_button_new (void)
{
  return g_object_new (EPHY_TYPE_SITE_MENU_BUTTON, NULL);
}

void
ephy_site_menu_button_set_zoom_level (EphySiteMenuButton *self,
                                      char               *zoom_level)
{
  gtk_label_set_text (GTK_LABEL (self->zoom_level), zoom_level);
}

void
ephy_site_menu_button_set_state (EphySiteMenuButton *self,
                                 unsigned int        state)
{
  gtk_path_paintable_set_state (self->paintable, state);
}

void
ephy_site_menu_button_append_description (EphySiteMenuButton *self,
                                          const char         *section)
{
  g_autofree char *new_description = NULL;

  if (self->description)
    new_description = g_strjoin (". ", self->description, section, NULL);
  else
    new_description = g_strdup (section);

  g_clear_pointer (&self->description, g_free);
  self->description = g_strdup (new_description);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                  self->description,
                                  -1);
}

void
ephy_site_menu_button_clear_description (EphySiteMenuButton *self)
{
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                  NULL, -1);

  g_clear_pointer (&self->description, g_free);
}

void
ephy_site_menu_button_update_bookmark_item (EphySiteMenuButton *self,
                                            gboolean            has_bookmark)
{
  g_menu_remove (G_MENU (self->items_section), 3);

  if (has_bookmark)
    g_menu_insert (G_MENU (self->items_section), 3, _("Edit _Bookmark…"), "win.bookmark-page");
  else
    g_menu_insert (G_MENU (self->items_section), 3, _("Add _Bookmark…"), "win.bookmark-page");
}

gboolean
ephy_site_menu_button_is_animating (EphySiteMenuButton *self)
{
  return self->is_animating;
}

void
ephy_site_menu_button_set_do_animation (EphySiteMenuButton *self,
                                        gboolean            do_animation)
{
  self->do_animation = do_animation;
}

void
on_animation_timeout (EphySiteMenuButton *self)
{
  gtk_path_paintable_set_state (self->paintable, self->prev_state);

  self->is_animating = FALSE;
  self->do_animation = FALSE;
  g_clear_handle_id (&self->timeout_id, g_source_remove);
}

void
ephy_site_menu_button_animate_reader_mode (EphySiteMenuButton *self)
{
  int delay = adw_get_enable_animations (GTK_WIDGET (self)) ? 1400 : 1500;

  if (!self->do_animation)
    return;

  if (self->is_animating)
    ephy_site_menu_button_cancel_animation (self);

  self->is_animating = TRUE;
  self->prev_state = gtk_path_paintable_get_state (self->paintable);
  gtk_path_paintable_set_state (self->paintable, 1);

  g_clear_handle_id (&self->timeout_id, g_source_remove);
  self->timeout_id = g_timeout_add_once (delay, (GSourceOnceFunc)on_animation_timeout, self);
}

void
ephy_site_menu_button_cancel_animation (EphySiteMenuButton *self)
{
  self->is_animating = FALSE;
  self->do_animation = FALSE;

  gtk_path_paintable_set_state (self->paintable, self->prev_state);
}
