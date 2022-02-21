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

#include "ephy-bookmark-properties.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-embed-container.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-sync-utils.h"

struct _EphyAddBookmarkPopover {
  GtkPopover parent_instance;

  char *address;

  GtkWidget *grid;
};

G_DEFINE_TYPE (EphyAddBookmarkPopover, ephy_add_bookmark_popover, GTK_TYPE_POPOVER)

static void
bookmark_removed_cb (EphyAddBookmarkPopover *self,
                     EphyBookmark           *bookmark,
                     EphyBookmarksManager   *manager)
{
  GtkWidget *relative_to;
  GtkWidget *window;
  EphyEmbed *embed;
  EphyWebView *view;
  const char *address;

  g_assert (EPHY_IS_ADD_BOOKMARK_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  relative_to = gtk_popover_get_relative_to (GTK_POPOVER (self));

  if (!relative_to)
    return;

  window = gtk_widget_get_toplevel (relative_to);
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  view = ephy_embed_get_web_view (embed);

  address = ephy_web_view_get_address (view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) == 0)
    ephy_window_sync_bookmark_state (EPHY_WINDOW (window), EPHY_BOOKMARK_ICON_EMPTY);

  ephy_bookmarks_manager_save (manager,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  gtk_popover_popdown (GTK_POPOVER (self));
}

static void
popover_shown (EphyAddBookmarkPopover *self)
{
  GtkWidget *relative_to;
  GtkWidget *window;
  EphyBookmarksManager *manager;
  EphyBookmark *bookmark;
  EphyEmbed *embed;
  const char *address;

  relative_to = gtk_popover_get_relative_to (GTK_POPOVER (self));

  if (!relative_to)
    return;

  window = gtk_widget_get_toplevel (relative_to);
  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, address);
  if (!bookmark) {
    g_autofree char *id = NULL;
    g_autoptr (EphyBookmark) new_bookmark = NULL;

    id = ephy_bookmark_generate_random_id ();
    new_bookmark = ephy_bookmark_new (address,
                                      ephy_embed_get_title (embed),
                                      g_sequence_new (g_free),
                                      id);

    ephy_bookmarks_manager_add_bookmark (manager, new_bookmark);
    ephy_window_sync_bookmark_state (EPHY_WINDOW (window), EPHY_BOOKMARK_ICON_BOOKMARKED);

    bookmark = new_bookmark;
  }

  g_signal_connect_object (manager, "bookmark-removed",
                           G_CALLBACK (bookmark_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->grid = ephy_bookmark_properties_new (bookmark,
                                             EPHY_BOOKMARK_PROPERTIES_TYPE_POPOVER,
                                             GTK_WIDGET (self));
  gtk_container_add (GTK_CONTAINER (self), self->grid);
  gtk_popover_set_default_widget (GTK_POPOVER (self),
                                  ephy_bookmark_properties_get_add_tag_button (EPHY_BOOKMARK_PROPERTIES (self->grid)));

  g_free (self->address);
  self->address = g_strdup (address);
}

static void
popover_hidden (EphyAddBookmarkPopover *self)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  ephy_bookmarks_manager_save (manager,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  g_clear_pointer (&self->address, g_free);

  if (self->grid) {
    gtk_popover_set_default_widget (GTK_POPOVER (self), NULL);
    gtk_container_remove (GTK_CONTAINER (self), self->grid);
    self->grid = NULL;
  }
}

static void
ephy_add_bookmark_popover_finalize (GObject *object)
{
  EphyAddBookmarkPopover *self = EPHY_ADD_BOOKMARK_POPOVER (object);

  g_free (self->address);

  G_OBJECT_CLASS (ephy_add_bookmark_popover_parent_class)->finalize (object);
}

static void
ephy_add_bookmark_popover_class_init (EphyAddBookmarkPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_add_bookmark_popover_finalize;
}

static void
ephy_add_bookmark_popover_notify_visible_cb (GtkPopover *popover,
                                             GParamSpec *param,
                                             gpointer    user_data)
{
  g_assert (EPHY_IS_ADD_BOOKMARK_POPOVER (popover));

  if (gtk_widget_get_visible (GTK_WIDGET (popover)))
    popover_shown (EPHY_ADD_BOOKMARK_POPOVER (popover));
  else
    popover_hidden (EPHY_ADD_BOOKMARK_POPOVER (popover));
}

static void
ephy_add_bookmark_popover_init (EphyAddBookmarkPopover *self)
{
  g_signal_connect (self, "notify::visible",
                    G_CALLBACK (ephy_add_bookmark_popover_notify_visible_cb),
                    NULL);
}

GtkWidget *
ephy_add_bookmark_popover_new (void)
{
  return g_object_new (EPHY_TYPE_ADD_BOOKMARK_POPOVER, NULL);
}
