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
  GtkSvg *svg;

  char *description;

  guint timeout_id;
  gboolean do_animation;
  gboolean is_animating;
  GArray *queued_states;
};

G_DEFINE_FINAL_TYPE (EphySiteMenuButton, ephy_site_menu_button, GTK_TYPE_BUTTON)

void ephy_site_menu_button_update_bookmark_item (EphySiteMenuButton *self,
                                                 gboolean            has_bookmark);

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

  g_array_free (self->queued_states, TRUE);
  self->queued_states = NULL;

  gtk_widget_unparent (self->popover_menu);

  G_OBJECT_CLASS (ephy_site_menu_button_parent_class)->dispose (object);
}

static void
ephy_site_menu_button_realize (GtkWidget *widget)
{
  EphySiteMenuButton *self = EPHY_SITE_MENU_BUTTON (widget);
  GtkWidget *window = gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW);

  gtk_svg_set_frame_clock (self->svg, gtk_widget_get_frame_clock (window));
  gtk_svg_play (self->svg);

  GTK_WIDGET_CLASS (ephy_site_menu_button_parent_class)->realize (widget);
}

static void
ephy_site_menu_button_class_init (EphySiteMenuButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/site-menu-button.ui");

  object_class->dispose = ephy_site_menu_button_dispose;

  widget_class->realize = ephy_site_menu_button_realize;

  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, menu_model);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, items_section);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, popover_menu);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, zoom_level);
  gtk_widget_class_bind_template_child (widget_class, EphySiteMenuButton, svg);

  gtk_widget_class_bind_template_callback (widget_class, on_clicked);
}

static void
ephy_site_menu_button_init (EphySiteMenuButton *self)
{
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();
  EphyShell *shell = ephy_shell_get_default ();
  EphyBookmarksManager *manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_menu_append_section (self->menu_model, NULL, self->items_section);

  gtk_widget_set_parent (self->popover_menu, GTK_WIDGET (self));

  if (!EPHY_IS_SHELL (shell))
    return;

  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    remove_menu_item (self->menu_model, "win.toggle-reader-mode");
    remove_menu_item (self->menu_model, "win.bookmark-page");
    remove_menu_item (self->menu_model, "win.add-search-engine");
  }

  manager = ephy_shell_get_bookmarks_manager (shell);
  g_signal_connect_object (manager, "bookmark-added",
                           G_CALLBACK (on_bookmark_added), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (manager, "bookmark-removed",
                           G_CALLBACK (on_bookmark_removed), self,
                           G_CONNECT_SWAPPED);

  self->queued_states = g_array_new (FALSE, FALSE, sizeof (int));
}

GtkWidget *
ephy_site_menu_button_new (void)
{
  return g_object_new (EPHY_TYPE_SITE_MENU_BUTTON, NULL);
}

void
ephy_site_menu_button_set_zoom_level (EphySiteMenuButton *self,
                                      const char         *zoom_level)
{
  gtk_label_set_text (GTK_LABEL (self->zoom_level), zoom_level);
}

void
ephy_site_menu_button_set_state (EphySiteMenuButton *self,
                                 unsigned int        state)
{
  gtk_svg_set_state (self->svg, state);
}

void
ephy_site_menu_button_append_description (EphySiteMenuButton *self,
                                          const char         *section)
{
  g_autofree char *new_description = NULL;

  if (self->description && strstr (self->description, section))
    return;

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

GVariant *
create_variant_from_autodiscovery_link (EphyOpensearchAutodiscoveryLink *link)
{
  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  g_autofree char *name = g_markup_escape_text (ephy_opensearch_autodiscovery_link_get_name (link), -1);
  const char *url = ephy_opensearch_autodiscovery_link_get_url (link);
  GVariant *variant;

  g_variant_builder_add (builder, "s", name);
  g_variant_builder_add (builder, "s", url);
  variant = g_variant_new ("as", builder);
  g_variant_builder_unref (builder);

  return variant;
}

void
ephy_site_menu_button_update_search_engine_item (EphySiteMenuButton *self,
                                                 const char         *item_label,
                                                 gboolean            has_multiple_engines)
{
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW));
  GListModel *search_engine_model = ephy_window_get_search_engine_model (window);
  g_autofree GMenuItem *search_engine_item = NULL;
  const char *name = item_label;

  g_menu_remove (G_MENU (self->items_section), 4);

  if (has_multiple_engines) {
    /* Create a submenu with a list of the available search engines. */
    GMenu *sub_menu = g_menu_new ();

    for (guint i = 0; i < g_list_model_get_n_items (search_engine_model); i++) {
      g_autofree GMenuItem *sub_menu_item = NULL;
      EphyOpensearchAutodiscoveryLink *link = g_list_model_get_item (search_engine_model, i);
      GVariant *variant = create_variant_from_autodiscovery_link (link);

      name = g_markup_escape_text (ephy_opensearch_autodiscovery_link_get_name (link), -1);

      sub_menu_item = g_menu_item_new (name, "win.add-search-engine");
      g_menu_item_set_action_and_target_value (sub_menu_item,
                                               "win.add-search-engine", variant);
      g_menu_append_item (sub_menu, sub_menu_item);
    }

    search_engine_item = g_menu_item_new (item_label, NULL);
    g_menu_item_set_submenu (search_engine_item, G_MENU_MODEL (sub_menu));
  } else if (g_list_model_get_n_items (search_engine_model) > 0) {
    /* Create a menu item that adds the available search engine. */
    EphyOpensearchAutodiscoveryLink *link = g_list_model_get_item (search_engine_model, 0);
    GVariant *variant = create_variant_from_autodiscovery_link (link);

    search_engine_item = g_menu_item_new (name, "win.add-search-engine");
    g_menu_item_set_action_and_target_value (search_engine_item,
                                             "win.add-search-engine", variant);
    g_menu_item_set_attribute_value (search_engine_item, "hidden-when",
                                     g_variant_new_string ("action-disabled"));
  } else {
    /* No search engines, so create a placeholder menu item that's invisible.
     * This is done so this function doesn't remove the wrong menu item when being called. */
    search_engine_item = g_menu_item_new (name, NULL);
    g_menu_item_set_attribute_value (search_engine_item, "hidden-when",
                                     g_variant_new_string ("action-disabled"));
  }

  g_menu_insert_item (G_MENU (self->items_section), 4, search_engine_item);
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
  g_clear_handle_id (&self->timeout_id, g_source_remove);

  if (self->queued_states->len == 0) {
    gtk_svg_set_state (self->svg, 0);

    self->is_animating = FALSE;
    self->do_animation = FALSE;
  } else {
    int delay = adw_get_enable_animations (GTK_WIDGET (self)) ? 1400 : 1500;
    int queued_state = g_array_index (self->queued_states, int, 0);

    gtk_svg_set_state (self->svg, queued_state);
    g_array_remove_index (self->queued_states, 0);

    self->timeout_id = g_timeout_add_once (delay, (GSourceOnceFunc)on_animation_timeout, self);
  }
}

void
ephy_site_menu_button_animate_reader_mode (EphySiteMenuButton *self)
{
  int delay = adw_get_enable_animations (GTK_WIDGET (self)) ? 1400 : 1500;

  if (!self->do_animation)
    return;

  if (self->is_animating) {
    int queued_state = 1;

    g_array_append_val (self->queued_states, queued_state);
    return;
  }

  self->is_animating = TRUE;
  gtk_svg_set_state (self->svg, 1);

  g_clear_handle_id (&self->timeout_id, g_source_remove);
  self->timeout_id = g_timeout_add_once (delay, (GSourceOnceFunc)on_animation_timeout, self);
}

void
ephy_site_menu_button_animate_search_engine (EphySiteMenuButton *self)
{
  int delay = adw_get_enable_animations (GTK_WIDGET (self)) ? 1400 : 1500;

  if (!self->do_animation)
    return;

  if (self->is_animating) {
    int queued_state = 2;

    g_array_append_val (self->queued_states, queued_state);
    return;
  }

  self->is_animating = TRUE;
  gtk_svg_set_state (self->svg, 2);

  g_clear_handle_id (&self->timeout_id, g_source_remove);
  self->timeout_id = g_timeout_add_once (delay, (GSourceOnceFunc)on_animation_timeout, self);
}

void
ephy_site_menu_button_cancel_animation (EphySiteMenuButton *self)
{
  g_clear_handle_id (&self->timeout_id, g_source_remove);

  self->is_animating = FALSE;
  self->do_animation = FALSE;

  g_array_remove_range (self->queued_states, 0, self->queued_states->len);
  gtk_svg_set_state (self->svg, 0);
}
