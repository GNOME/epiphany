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
  GtkWidget *relative_to;
  GtkWindow *window;
};

G_DEFINE_TYPE (EphyAddBookmarkPopover, ephy_add_bookmark_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_RELATIVE_TO,
  PROP_WINDOW,
  LAST_PROP
};

enum signalsEnum {
  UPDATE_STATE,
  LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_bookmarks_popover_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphyAddBookmarkPopover *self = EPHY_ADD_BOOKMARK_POPOVER (object);

  switch (prop_id) {
    case PROP_RELATIVE_TO:
      self->relative_to = g_value_get_object (value);
      break;
    case PROP_WINDOW:
      self->window = g_value_get_object (value);
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

  G_OBJECT_CLASS (ephy_add_bookmark_popover_parent_class)->constructed (object);

  gtk_popover_set_relative_to (GTK_POPOVER (self), self->relative_to);
}

static void
ephy_add_bookmark_popover_class_init (EphyAddBookmarkPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_bookmarks_popover_set_property;
  object_class->finalize = ephy_add_bookmark_popover_finalize;
  object_class->constructed = ephy_add_bookmark_popover_constructed;

  obj_properties[PROP_RELATIVE_TO] =
    g_param_spec_object ("relative-to",
                         "A GtkWidget object",
                         "The popover's parent widget",
                         GTK_TYPE_WIDGET,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "A GtkWidget object",
                         "The popover's parent window",
                         GTK_TYPE_WIDGET,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /**
   * EphAddBookmarkPopover::update-state:
   * @entry: the object on which the signal is emitted
   *
   * Emitted when the bookmark state changes
   *
   */
  signals[UPDATE_STATE] = g_signal_new ("update-state", G_OBJECT_CLASS_TYPE (klass),
                                        G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_INT);
}

static void
ephy_add_bookmark_popover_notify_visible_cb (GtkPopover *popover,
                                             GParamSpec *param,
                                             gpointer    user_data)
{
  EphyAddBookmarkPopover *self;
  EphyBookmarksManager *manager;

  g_assert (EPHY_IS_ADD_BOOKMARK_POPOVER (popover));

  if (gtk_widget_get_visible (GTK_WIDGET (popover)))
    return;

  self = EPHY_ADD_BOOKMARK_POPOVER (popover);
  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  ephy_bookmarks_manager_save (manager,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  g_clear_pointer (&self->address, g_free);
  g_clear_pointer (&self->grid, gtk_widget_destroy);
}

static void
ephy_add_bookmark_popover_init (EphyAddBookmarkPopover *self)
{
  g_signal_connect (self, "notify::visible",
                    G_CALLBACK (ephy_add_bookmark_popover_notify_visible_cb),
                    NULL);
}

GtkWidget *
ephy_add_bookmark_popover_new (GtkWidget *relative_to,
                               GtkWidget *window)
{
  return g_object_new (EPHY_TYPE_ADD_BOOKMARK_POPOVER,
                       "relative-to", relative_to,
                       "window", window,
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
  EphyEmbed *embed;
  EphyWebView *view;
  const char *address;

  g_assert (EPHY_IS_ADD_BOOKMARK_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (self->window));
  view = ephy_embed_get_web_view (embed);

  address = ephy_web_view_get_address (view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) == 0)
    g_signal_emit (self, signals[UPDATE_STATE], 0, EPHY_BOOKMARK_ICON_EMPTY);

  ephy_bookmarks_manager_save (manager,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  gtk_popover_popdown (GTK_POPOVER (self));
}

void
ephy_add_bookmark_popover_show (EphyAddBookmarkPopover *self)
{
  EphyBookmarksManager *manager;
  EphyBookmark *bookmark;
  EphyEmbed *embed;
  const char *address;

  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (self->window));

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
    g_signal_emit (self, signals[UPDATE_STATE], 0, EPHY_BOOKMARK_ICON_BOOKMARKED);

    bookmark = new_bookmark;
  }

  g_signal_connect_object (manager, "bookmark-removed",
                           G_CALLBACK (ephy_add_bookmark_popover_update_bookmarked_status_cb),
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

  gtk_popover_popup (GTK_POPOVER (self));
}
