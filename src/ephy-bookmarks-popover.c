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
  GtkWidget      *tags_list_box;

  EphyWindow     *window;
};

G_DEFINE_TYPE (EphyBookmarksPopover, ephy_bookmarks_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static GtkWidget *
create_bookmark_row (gpointer item,
                     gpointer user_data)
{
  EphyBookmark *bookmark = EPHY_BOOKMARK (item);

  return ephy_bookmark_row_new (bookmark);
}

static void
bookmarks_list_box_row_activated_cb (EphyBookmarksPopover   *self,
                                     EphyBookmarkRow        *row,
                                     GtkListBox             *box)
{
  EphyBookmark *bookmark;
  GActionGroup *action_group;
  GAction *action;
  const char *url;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK_ROW (row));
  g_assert (GTK_IS_LIST_BOX (box));

  action_group = gtk_widget_get_action_group (GTK_WIDGET (self->window), "win");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-bookmark");

  bookmark = ephy_bookmark_row_get_bookmark (row);
  url = ephy_bookmark_get_url (bookmark);

  g_action_activate (action, g_variant_new_string (url));
}

static GtkWidget *
create_tag_box (const char *tag)
{
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (box, GTK_ALIGN_START);

  image = gtk_image_new_from_icon_name ("user-bookmarks-symbolic", GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 6);
  gtk_widget_show (image);

  label = gtk_label_new (tag);
  gtk_box_pack_start (GTK_BOX (box),label, TRUE, FALSE, 6);
  gtk_widget_show (label);

  gtk_widget_show (box);

  return box;
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
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tags_list_box);
}

static void
ephy_bookmarks_popover_init (EphyBookmarksPopover *self)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GSequence *tags, *tags1, *tags2;
  GSequenceIter *iter;
  EphyBookmark *dummy_bookmark;
  gint i;

  gtk_widget_init_template (GTK_WIDGET (self));

  dummy_bookmark = ephy_bookmark_new (g_strdup ("https://duckduckgo.com/asdasdas/asdas"), g_strdup ("Test title"));
  tags1 = g_sequence_new (g_free);
  for (i = 0; i < 20; i++)
    g_sequence_insert_sorted (tags1, g_strdup_printf ("Fun %d", i), (GCompareDataFunc)g_strcmp0, NULL);

  ephy_bookmark_set_tags (dummy_bookmark, tags1);
  ephy_bookmarks_manager_add_bookmark (manager, dummy_bookmark);

  dummy_bookmark = ephy_bookmark_new (g_strdup ("https://wikipedia.com"), g_strdup ("wikipedia"));
  ephy_bookmarks_manager_add_bookmark (manager, dummy_bookmark);
  tags2 = g_sequence_new (g_free);
  g_sequence_insert_sorted (tags2, g_strdup_printf ("Not Fun %d", 1), (GCompareDataFunc)g_strcmp0, NULL);
  ephy_bookmark_set_tags (dummy_bookmark, tags2);

  gtk_list_box_bind_model (GTK_LIST_BOX (self->bookmarks_list_box),
                           G_LIST_MODEL (manager),
                           create_bookmark_row,
                           NULL, NULL);

  tags = ephy_bookmarks_manager_get_tags (manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    GtkWidget *tag_box;
    const char *tag = g_sequence_get (iter);

    tag_box = create_tag_box (tag);
    gtk_list_box_prepend (GTK_LIST_BOX (self->tags_list_box), tag_box);
  }

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
