/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
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

#include "config.h"

#include "ephy-add-bookmark-popover.h"

#include "ephy-bookmark-properties-grid.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-embed-container.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"

struct _EphyAddBookmarkPopover {
  GtkPopover     parent_instance;

  char          *address;

  GtkWidget     *grid;
  EphyHeaderBar *header_bar;
};

G_DEFINE_TYPE (EphyAddBookmarkPopover, ephy_add_bookmark_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_HEADER_BAR,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_bookmarks_popover_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphyAddBookmarkPopover *self = EPHY_ADD_BOOKMARK_POPOVER (object);

  switch (prop_id) {
    case PROP_HEADER_BAR:
      self->header_bar = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_add_bookmark_popover_finalize (GObject *object)
{
  EphyAddBookmarkPopover *self = EPHY_ADD_BOOKMARK_POPOVER (object);

  if (self->address)
    g_free (self->address);

  G_OBJECT_CLASS (ephy_add_bookmark_popover_parent_class)->finalize (object);
}

static void
ephy_add_bookmark_popover_constructed (GObject *object)
{
  EphyAddBookmarkPopover *self = EPHY_ADD_BOOKMARK_POPOVER (object);
  GtkWidget *location_entry;

  G_OBJECT_CLASS (ephy_add_bookmark_popover_parent_class)->constructed (object);

  location_entry = GTK_WIDGET (ephy_header_bar_get_title_widget (self->header_bar));
  g_assert (EPHY_IS_LOCATION_ENTRY (location_entry));

  gtk_popover_set_relative_to (GTK_POPOVER (self), location_entry);
}

static void
ephy_add_bookmark_popover_class_init (EphyAddBookmarkPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_bookmarks_popover_set_property;
  object_class->finalize = ephy_add_bookmark_popover_finalize;
  object_class->constructed = ephy_add_bookmark_popover_constructed;

  obj_properties[PROP_HEADER_BAR] =
    g_param_spec_object ("header-bar",
                         "An EphyHeaderBar object",
                         "The popover's parent EphyHeaderBar",
                         EPHY_TYPE_HEADER_BAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_add_bookmark_popover_init (EphyAddBookmarkPopover *self)
{
}

GtkWidget *
ephy_add_bookmark_popover_new (EphyHeaderBar *header_bar)
{
  g_return_val_if_fail (EPHY_IS_HEADER_BAR (header_bar), NULL);

  return g_object_new (EPHY_TYPE_ADD_BOOKMARK_POPOVER,
                       "header-bar", header_bar,
                       NULL);
}

/**
 * update_bookmarked_status_cb:
 * @bookmark: an #EphyBookmark object
 * @header_bar: an #EphyHeaderBar widget
 *
 * Remove bookmarked status if the @bookmark was removed.
 *
 **/
static void
ephy_add_bookmark_popover_update_bookmarked_status_cb (EphyAddBookmarkPopover *self,
                                                       EphyBookmark           *bookmark,
                                                       EphyBookmarksManager   *manager)
{
  GtkWidget *location_entry;
  EphyWindow *window;
  EphyEmbed *embed;
  EphyWebView *view;
  const char *address;

  g_assert (EPHY_IS_ADD_BOOKMARK_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  location_entry = GTK_WIDGET (ephy_header_bar_get_title_widget (self->header_bar));
  window = ephy_header_bar_get_window (self->header_bar);
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  view = ephy_embed_get_web_view (embed);

  address = ephy_web_view_get_address (view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) == 0) {
    ephy_location_entry_set_bookmark_icon_state (EPHY_LOCATION_ENTRY (location_entry),
                                                 EPHY_LOCATION_ENTRY_BOOKMARK_ICON_EMPTY);
  }

  ephy_bookmarks_manager_save_to_file_async (manager, NULL,
                                             ephy_bookmarks_manager_save_to_file_warn_on_error_cb,
                                             NULL);

  gtk_widget_hide (GTK_WIDGET (self));

  g_clear_pointer (&self->address, g_free);
  g_clear_pointer (&self->grid, gtk_widget_destroy);
}

void
ephy_add_bookmark_popover_show (EphyAddBookmarkPopover *self)
{
  EphyBookmarksManager *manager;
  GtkWidget *location_entry;
  EphyWindow *window;
  EphyEmbed *embed;
  EphyBookmark *bookmark;
  const char *address;

  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  location_entry = GTK_WIDGET (ephy_header_bar_get_title_widget (self->header_bar));
  window = ephy_header_bar_get_window (self->header_bar);
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, address);
  if (!bookmark) {
    bookmark = ephy_bookmark_new (address,
                                  ephy_embed_get_title (embed),
                                  g_sequence_new (g_free));

    ephy_bookmarks_manager_add_bookmark (manager, bookmark);
    ephy_location_entry_set_bookmark_icon_state (EPHY_LOCATION_ENTRY (location_entry),
                                                 EPHY_LOCATION_ENTRY_BOOKMARK_ICON_BOOKMARKED);
  }

  g_signal_connect_object (manager, "bookmark-removed",
                         G_CALLBACK (ephy_add_bookmark_popover_update_bookmarked_status_cb),
                         self,
                         G_CONNECT_SWAPPED);

  if (!self->address || g_strcmp0 (address, self->address) != 0) {
    if (self->grid)
      gtk_widget_destroy (self->grid);

    self->grid = ephy_bookmark_properties_grid_new (bookmark,
                                                    EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_POPOVER,
                                                    GTK_WIDGET (self));
    gtk_container_add (GTK_CONTAINER (self), self->grid);
    gtk_popover_set_default_widget (GTK_POPOVER (self),
                                    ephy_bookmark_properties_grid_get_add_tag_button (EPHY_BOOKMARK_PROPERTIES_GRID (self->grid)));

    g_free (self->address);
    self->address = g_strdup (address);
  }

  gtk_popover_popup (GTK_POPOVER (self));
}
