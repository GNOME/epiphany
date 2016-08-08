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
#include "ephy-debug.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>

struct _EphyBookmarksPopover {
  GtkPopover      parent_instance;

  GtkWidget      *toplevel_stack;
  GtkWidget      *bookmarks_list_box;
  GtkWidget      *tags_list_box;
  GtkWidget      *tag_detail_list_box;
  GtkWidget      *tag_detail_back_button;
  GtkWidget      *tag_detail_label;

  EphyWindow     *window;
};

G_DEFINE_TYPE (EphyBookmarksPopover, ephy_bookmarks_popover, GTK_TYPE_POPOVER)

#define EPHY_LIST_BOX_ROW_TYPE_BOOKMARK "bookmark"
#define EPHY_LIST_BOX_ROW_TYPE_TAG "tag"

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static int
tags_list_box_sort_func (GtkListBoxRow *row1, GtkListBoxRow *row2)
{
  const char *type1;
  const char *type2;
  const char *title1;
  const char *title2;

  g_assert (GTK_IS_LIST_BOX_ROW (row1));
  g_assert (GTK_IS_LIST_BOX_ROW (row2));

  type1 = g_object_get_data (G_OBJECT (row1), "type");
  type2 = g_object_get_data (G_OBJECT (row2), "type");

  title1 = g_object_get_data (G_OBJECT (row1), "title");
  title2 = g_object_get_data (G_OBJECT (row2), "title");

  if (g_strcmp0 (type1, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0
      && g_strcmp0 (type2, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0)
      return ephy_bookmark_tags_compare (title1, title2);

  if (g_strcmp0 (type1, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0)
    return -1;
  if (g_strcmp0 (type2, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0)
    return 1;

  return g_strcmp0 (title1, title2);
}

static GtkWidget *
create_bookmark_row (gpointer item,
                     gpointer user_data)
{
  EphyBookmark *bookmark = EPHY_BOOKMARK (item);
  GtkWidget *row;

  row = ephy_bookmark_row_new (bookmark);
  g_object_set_data (G_OBJECT (row), "type", g_strdup (EPHY_LIST_BOX_ROW_TYPE_BOOKMARK));
  g_object_set_data (G_OBJECT (row), "title", g_strdup (ephy_bookmark_get_title (bookmark)));

  return row;
}

static void
ephy_bookmarks_popover_actions_tag_detail_back (GSimpleAction *action,
                                                GVariant      *value,
                                                gpointer       user_data)
{
  EphyBookmarksPopover *self = user_data;
  GList *l;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack),
                                    "default");

  for (l = gtk_container_get_children (GTK_CONTAINER (self->tag_detail_list_box));
       l != NULL;
       l = l->next)
    gtk_container_remove (GTK_CONTAINER (self->tag_detail_list_box), l->data);
}

static void
ephy_bookmarks_popover_show_tag_detail (EphyBookmarksPopover *self,
                                        const char           *tag)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GSequence *bookmarks;
  GSequenceIter *iter;

  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (manager, tag);

  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GtkWidget *row;

    row = create_bookmark_row (bookmark, NULL);
    gtk_list_box_prepend (GTK_LIST_BOX (self->tag_detail_list_box), row);
  }

  gtk_label_set_label (GTK_LABEL (self->tag_detail_label), tag);

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack),
                                    "tag_detail");
}

static void
ephy_bookmarks_popover_list_box_row_activated_cb (EphyBookmarksPopover   *self,
                                                  GtkListBoxRow          *row,
                                                  GtkListBox             *box)
{
  const char *type;
  const char *tag;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (box));

  type = g_object_get_data (G_OBJECT (row), "type");
  if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0) {
    EphyBookmark *bookmark;
    GActionGroup *action_group;
    GAction *action;
    const char *url;

    action_group = gtk_widget_get_action_group (GTK_WIDGET (self->window), "win");
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-bookmark");

    bookmark = ephy_bookmark_row_get_bookmark (EPHY_BOOKMARK_ROW (row));
    url = ephy_bookmark_get_url (bookmark);

    g_action_activate (action, g_variant_new_string (url));
  } else {
    tag = g_object_get_data (G_OBJECT (row), "title");
    ephy_bookmarks_popover_show_tag_detail (self, tag);
  }
}

static GtkWidget *
create_tag_row (const char *tag)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  row = gtk_list_box_row_new ();
  g_object_set_data (G_OBJECT (row), "type", g_strdup (EPHY_LIST_BOX_ROW_TYPE_TAG));
  g_object_set_data (G_OBJECT (row), "title", g_strdup (tag));
  g_object_set (G_OBJECT (row), "height-request", 40, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (box, GTK_ALIGN_START);

  if (g_strcmp0 (tag, "Favorites") == 0)
    image = gtk_image_new_from_icon_name ("user-bookmarks-symbolic", GTK_ICON_SIZE_MENU);
  else
    image = gtk_image_new_from_icon_name ("ephy-bookmark-tag", GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 6);

  label = gtk_label_new (tag);
  gtk_box_pack_start (GTK_BOX (box),label, TRUE, FALSE, 6);

  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_show_all (row);

  return row;
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
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, toplevel_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, bookmarks_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tags_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tag_detail_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tag_detail_back_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tag_detail_label);
}

static const GActionEntry entries[] = {
  { "tag-detail-back", ephy_bookmarks_popover_actions_tag_detail_back }
};

static void
ephy_bookmarks_popover_init (EphyBookmarksPopover *self)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GSequence *tags;
  GSequence *bookmarks;
  GSequenceIter *iter;
  GSimpleActionGroup *group;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/org/gnome/epiphany/icons/");

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "popover",
                                  G_ACTION_GROUP (group));
  g_object_unref (group);

  gtk_list_box_bind_model (GTK_LIST_BOX (self->bookmarks_list_box),
                           G_LIST_MODEL (manager),
                           create_bookmark_row,
                           NULL, NULL);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->tags_list_box),
                              (GtkListBoxSortFunc)tags_list_box_sort_func,
                              NULL, NULL);

  tags = ephy_bookmarks_manager_get_tags (manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);
    GtkWidget *tag_row;

    tag_row = create_tag_row (tag);
    gtk_container_add (GTK_CONTAINER (self->tags_list_box), tag_row);
  }

  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (manager, NULL);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GtkWidget *bookmark_row;

    bookmark_row = create_bookmark_row (bookmark, NULL);
    gtk_widget_show_all (bookmark_row);
    gtk_container_add (GTK_CONTAINER (self->tags_list_box), bookmark_row);
  }

  g_signal_connect_object (self->bookmarks_list_box, "row-activated",
                           G_CALLBACK (ephy_bookmarks_popover_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->tags_list_box, "row-activated",
                           G_CALLBACK (ephy_bookmarks_popover_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
}

EphyBookmarksPopover *
ephy_bookmarks_popover_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_POPOVER,
                       "window", window,
                       NULL);
}
