/*
 * Copyright (C) 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-bookmarks-popover.h"

#include "ephy-bookmark.h"
#include "ephy-bookmark-row.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>

struct _EphyBookmarksPopover {
  GtkPopover      parent_instance;

  GtkWidget      *bookmarks_list_box;

  EphyWindow     *window;
};

G_DEFINE_TYPE (EphyBookmarksPopover, ephy_bookmarks_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
bookmark_added_cb (EphyBookmarksPopover *self,
                   EphyBookmark         *bookmark)
{
  GtkWidget *bookmark_row;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  bookmark_row = ephy_bookmark_row_new (bookmark);

  gtk_list_box_prepend (GTK_LIST_BOX (self->bookmarks_list_box), bookmark_row);
}

static void
bookmarks_list_box_row_activated_cb (EphyBookmarksPopover   *self,
                                     EphyBookmarkRow        *row,
                                     GtkListBox             *box)
{
  EphyBookmark *bookmark;
  GActionGroup *action_group;
  GAction *action;
  const gchar *url;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK_ROW (row));
  g_assert (GTK_IS_LIST_BOX (box));


  action_group = gtk_widget_get_action_group (GTK_WIDGET (self->window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-bookmark");

  bookmark = ephy_bookmark_row_get_bookmark (row);
  url = ephy_bookmark_get_url (bookmark);

  g_action_activate (action, g_variant_new_string (url));
}

static void
ephy_bookmarks_popover_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphyBookmarksPopover *self = EPHY_BOOKMARKS_POPOVER (object);

  switch (prop_id) {
    case PROP_WINDOW:
      self->window = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmarks_popover_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmarks_popover_class_init (EphyBookmarksPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_bookmarks_popover_set_property;
  object_class->get_property = ephy_bookmarks_popover_get_property;

  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "An EphyWindow object",
                         "The popover's parent EphyWindow",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmarks-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, bookmarks_list_box);
}

static void
ephy_bookmarks_popover_init (EphyBookmarksPopover *self)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GList *bookmarks;
  GList *tags = NULL;
  GList *l;
  EphyBookmark *dummy_bookmark;

  gtk_widget_init_template (GTK_WIDGET (self));

  dummy_bookmark = ephy_bookmark_new (g_strdup ("https://duckduckgo.com"), g_strdup ("Test title"));
  tags = g_list_append (tags, g_strdup ("Fun"));
  tags = g_list_append (tags, g_strdup ("Work"));
  ephy_bookmark_set_tags (dummy_bookmark, tags);
  ephy_bookmarks_manager_add_bookmark (manager, dummy_bookmark);

  dummy_bookmark = ephy_bookmark_new (g_strdup ("https://wikipedia.com"), g_strdup ("wikipedia"));
  ephy_bookmarks_manager_add_bookmark (manager, dummy_bookmark);

  bookmarks = ephy_bookmarks_manager_get_bookmarks (manager);
  for (l = bookmarks; l != NULL; l = g_list_next (l)) {
    EphyBookmark *bookmark = (EphyBookmark *)l->data;
    GtkWidget *bookmark_row;

    bookmark_row = ephy_bookmark_row_new (bookmark);
    gtk_list_box_prepend (GTK_LIST_BOX (self->bookmarks_list_box), bookmark_row);
  }

  g_signal_connect_object (manager, "bookmark-added",
                           G_CALLBACK (bookmark_added_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->bookmarks_list_box, "row-activated",
                           G_CALLBACK (bookmarks_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
}

EphyBookmarksPopover *
ephy_bookmarks_popover_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_POPOVER,
                       "window", window,
                       NULL);
}
