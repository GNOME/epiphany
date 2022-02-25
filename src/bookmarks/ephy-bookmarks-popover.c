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

#include "ephy-bookmarks-popover.h"

#include "ephy-bookmark.h"
#include "ephy-bookmark-row.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-gui.h"
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include <glib/gi18n.h>

struct _EphyBookmarksPopover {
  GtkPopover parent_instance;

  GtkWidget *toplevel_stack;
  GtkWidget *bookmarks_list_box;
  GtkWidget *tags_list_box;
  GtkWidget *tag_detail_list_box;
  GtkWidget *tag_detail_label;
  char *tag_detail_tag;

  EphyBookmarksManager *manager;
};

G_DEFINE_TYPE (EphyBookmarksPopover, ephy_bookmarks_popover, GTK_TYPE_POPOVER)

#define EPHY_LIST_BOX_ROW_TYPE_BOOKMARK "bookmark"
#define EPHY_LIST_BOX_ROW_TYPE_TAG "tag"

static GtkWidget * create_bookmark_row (gpointer item, gpointer user_data);
static GtkWidget *create_tag_row (const char *tag);

static void
tag_detail_back (EphyBookmarksPopover *self)
{
  GtkListBoxRow *row;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack),
                                    "default");

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0)))
    gtk_container_remove (GTK_CONTAINER (self->tag_detail_list_box), GTK_WIDGET (row));
}

static void
remove_bookmark_row (GtkListBox *list_box,
                     const char *url)
{
  GtkListBoxRow *row;
  int i = 0;

  g_assert (GTK_IS_LIST_BOX (list_box));

  while ((row = gtk_list_box_get_row_at_index (list_box, i++))) {
    const char *type = g_object_get_data (G_OBJECT (row), "type");

    if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0 &&
        g_strcmp0 (ephy_bookmark_row_get_bookmark_url (EPHY_BOOKMARK_ROW (row)), url) == 0) {
      gtk_container_remove (GTK_CONTAINER (list_box), GTK_WIDGET (row));
      break;
    }
  }
}

static void
ephy_bookmarks_popover_bookmark_tag_added_cb (EphyBookmarksPopover *self,
                                              EphyBookmark         *bookmark,
                                              const char           *tag,
                                              EphyBookmarksManager *manager)
{
  gboolean exists;
  const char *visible_stack_child;
  GtkListBoxRow *row;
  int i = 0;

  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));

  /* If the bookmark no longer has 0 tags, we remove it from the tags list box */
  if (g_sequence_get_length (ephy_bookmark_get_tags (bookmark)) == 1)
    remove_bookmark_row (GTK_LIST_BOX (self->tags_list_box),
                         ephy_bookmark_get_url (bookmark));

  /* If we are on the tag detail list box, then the user has toggled the state
   * of the tag widget multiple times. The first time the bookmark was removed
   * from the list box. Now we have to add it back. */
  visible_stack_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));
  if (g_strcmp0 (visible_stack_child, "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    GtkWidget *bookmark_row;

    bookmark_row = create_bookmark_row (bookmark, self);
    gtk_container_add (GTK_CONTAINER (self->tag_detail_list_box), bookmark_row);
  }

  exists = FALSE;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tags_list_box), i++))) {
    const char *title = g_object_get_data (G_OBJECT (row), "title");
    const char *type = g_object_get_data (G_OBJECT (row), "type");

    if (g_strcmp0 (title, tag) == 0 &&
        g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0) {
      exists = TRUE;
      break;
    }
  }

  if (!exists) {
    GtkWidget *tag_row = create_tag_row (tag);
    gtk_container_add (GTK_CONTAINER (self->tags_list_box), tag_row);
  }
}

static void
ephy_bookmarks_popover_bookmark_tag_removed_cb (EphyBookmarksPopover *self,
                                                EphyBookmark         *bookmark,
                                                const char           *tag,
                                                EphyBookmarksManager *manager)
{
  const char *visible_stack_child;
  gboolean exists;

  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));

  /* If the bookmark has 0 tags after removing one, we add it to the tags list
   * box */
  if (g_sequence_is_empty (ephy_bookmark_get_tags (bookmark))) {
    GtkListBoxRow *row;
    int i = 0;

    exists = FALSE;
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tags_list_box), i++))) {
      const char *type = g_object_get_data (G_OBJECT (row), "type");

      if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0) {
        const char *url = ephy_bookmark_row_get_bookmark_url (EPHY_BOOKMARK_ROW (row));

        if (g_strcmp0 (ephy_bookmark_get_url (bookmark), url) == 0) {
          exists = TRUE;
          break;
        }
      }
    }

    if (!exists) {
      GtkWidget *row = create_bookmark_row (bookmark, self);
      gtk_container_add (GTK_CONTAINER (self->tags_list_box), row);
    }
  }

  /* If we are on the tag detail list box of the tag that was removed, we
   * remove the bookmark from it to reflect the changes. */
  visible_stack_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));
  if (g_strcmp0 (visible_stack_child, "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    remove_bookmark_row (GTK_LIST_BOX (self->tag_detail_list_box),
                         ephy_bookmark_get_url (bookmark));

    /* If we removed the tag's last bookmark, switch back to the tags list. */
    if (ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag))
      tag_detail_back (self);
  }

  /* If the tag no longer contains bookmarks, remove it from the tags list */
  if (ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag)) {
    GtkListBoxRow *row;
    int i = 0;

    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tags_list_box), i++))) {
      const char *title = g_object_get_data (G_OBJECT (row), "title");

      if (g_strcmp0 (title, tag) == 0)
        gtk_container_remove (GTK_CONTAINER (self->tags_list_box), GTK_WIDGET (row));
    }
  }
}

static GtkWidget *
create_bookmark_row (gpointer item,
                     gpointer user_data)
{
  EphyBookmark *bookmark = EPHY_BOOKMARK (item);
  GtkWidget *row;

  row = ephy_bookmark_row_new (bookmark);
  g_object_set_data_full (G_OBJECT (row), "type",
                          g_strdup (EPHY_LIST_BOX_ROW_TYPE_BOOKMARK),
                          (GDestroyNotify)g_free);
  g_object_set_data_full (G_OBJECT (row), "title",
                          g_strdup (ephy_bookmark_get_title (bookmark)),
                          (GDestroyNotify)g_free);

  return row;
}

static GtkWidget *
create_tag_row (const char *tag)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  row = gtk_list_box_row_new ();
  g_object_set_data_full (G_OBJECT (row), "type",
                          g_strdup (EPHY_LIST_BOX_ROW_TYPE_TAG),
                          (GDestroyNotify)g_free);
  g_object_set_data_full (G_OBJECT (row), "title",
                          g_strdup (tag),
                          (GDestroyNotify)g_free);
  g_object_set (G_OBJECT (row), "height-request", 40, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  if (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0) {
    image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic", GTK_ICON_SIZE_MENU);
  } else {
    image = gtk_image_new_from_icon_name ("ephy-bookmark-tag-symbolic", GTK_ICON_SIZE_MENU);
  }
  label = gtk_label_new (tag);

  gtk_widget_set_hexpand (label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_show_all (row);

  return row;
}

static void
ephy_bookmarks_popover_bookmark_added_cb (EphyBookmarksPopover *self,
                                          EphyBookmark         *bookmark,
                                          EphyBookmarksManager *manager)
{
  GtkWidget *row;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  if (g_sequence_is_empty (ephy_bookmark_get_tags (bookmark))) {
    row = create_bookmark_row (bookmark, self);
    gtk_container_add (GTK_CONTAINER (self->tags_list_box), row);
  }

  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "empty-state") == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
}

static void
ephy_bookmarks_popover_bookmark_removed_cb (EphyBookmarksPopover *self,
                                            EphyBookmark         *bookmark,
                                            EphyBookmarksManager *manager)
{
  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  remove_bookmark_row (GTK_LIST_BOX (self->tags_list_box),
                       ephy_bookmark_get_url (bookmark));
  remove_bookmark_row (GTK_LIST_BOX (self->tag_detail_list_box),
                       ephy_bookmark_get_url (bookmark));

  if (g_list_model_get_n_items (G_LIST_MODEL (self->manager)) == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");
  } else if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0 &&
             ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, self->tag_detail_tag)) {
    /* If we removed the tag's last bookmark, switch back to the tags list. */
    tag_detail_back (self);
  }
}

static void
ephy_bookmarks_popover_tag_created_cb (EphyBookmarksPopover *self,
                                       const char           *tag,
                                       EphyBookmarksManager *manager)
{
  GtkWidget *tag_row;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (tag != NULL);
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  tag_row = create_tag_row (tag);
  gtk_container_add (GTK_CONTAINER (self->tags_list_box), tag_row);
}

static void
ephy_bookmarks_popover_tag_deleted_cb (EphyBookmarksPopover *self,
                                       const char           *tag,
                                       int                   position,
                                       EphyBookmarksManager *manager)
{
  GtkListBoxRow *row;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tags_list_box), position);
  gtk_container_remove (GTK_CONTAINER (self->tags_list_box),
                        GTK_WIDGET (row));

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    tag_detail_back (self);
  }
}

static int
tags_list_box_sort_func (GtkListBoxRow *row1,
                         GtkListBoxRow *row2)
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

static void
ephy_bookmarks_popover_actions_tag_detail_back (GSimpleAction *action,
                                                GVariant      *value,
                                                gpointer       user_data)
{
  EphyBookmarksPopover *self = user_data;

  tag_detail_back (self);
}

static void
ephy_bookmarks_popover_show_tag_detail (EphyBookmarksPopover *self,
                                        const char           *tag)
{
  GSequence *bookmarks;
  GSequenceIter *iter;

  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, tag);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GtkWidget *row;

    row = create_bookmark_row (bookmark, self);
    gtk_container_add (GTK_CONTAINER (self->tag_detail_list_box), row);
  }

  gtk_label_set_label (GTK_LABEL (self->tag_detail_label), tag);

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");

  if (self->tag_detail_tag != NULL)
    g_free (self->tag_detail_tag);
  self->tag_detail_tag = g_strdup (tag);

  g_sequence_free (bookmarks);
}

static void
ephy_bookmarks_popover_open_bookmark (EphyBookmarksPopover *self,
                                      GtkListBoxRow        *row)
{
  GtkWidget *window;
  const char *url;
  GdkEventType type = GDK_NOTHING;
  guint state = 0, button = (guint) - 1;
  EphyLinkFlags flags;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW);
  g_assert (EPHY_IS_WINDOW (window));
  url = ephy_bookmark_row_get_bookmark_url (EPHY_BOOKMARK_ROW (row));

  ephy_gui_get_current_event (&type, &state, &button, NULL);
  flags = ephy_link_flags_from_modifiers (state, button == GDK_BUTTON_MIDDLE &&
                                          (type == GDK_BUTTON_PRESS ||
                                           type == GDK_BUTTON_RELEASE));

  ephy_link_open (EPHY_LINK (window), url, NULL, flags | EPHY_LINK_BOOKMARK);
}

static void
ephy_bookmarks_popover_list_box_row_activated_cb (EphyBookmarksPopover *self,
                                                  GtkListBoxRow        *row,
                                                  GtkListBox           *box)
{
  const char *type;
  const char *tag;

  g_assert (EPHY_IS_BOOKMARKS_POPOVER (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (box));

  type = g_object_get_data (G_OBJECT (row), "type");
  if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0) {
    ephy_bookmarks_popover_open_bookmark (self, row);
  } else {
    tag = g_object_get_data (G_OBJECT (row), "title");
    ephy_bookmarks_popover_show_tag_detail (self, tag);
  }
}

static gboolean
ephy_bookmarks_popover_list_box_button_release_event_cb (EphyBookmarksPopover *self,
                                                         GdkEvent             *event,
                                                         GtkListBox           *box)
{
  GdkEventButton *event_button = (GdkEventButton *)event;
  GtkListBoxRow *row = gtk_list_box_get_row_at_y (box, event_button->y);

  if (event_button->button != GDK_BUTTON_MIDDLE)
    return GDK_EVENT_PROPAGATE;

  ephy_bookmarks_popover_open_bookmark (self, row);

  return GDK_EVENT_STOP;
}

static void
ephy_bookmarks_popover_finalize (GObject *object)
{
  EphyBookmarksPopover *self = EPHY_BOOKMARKS_POPOVER (object);

  g_free (self->tag_detail_tag);

  G_OBJECT_CLASS (ephy_bookmarks_popover_parent_class)->finalize (object);
}

static void
ephy_bookmarks_popover_class_init (EphyBookmarksPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ephy_bookmarks_popover_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmarks-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, toplevel_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, bookmarks_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tags_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tag_detail_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, tag_detail_label);
}

static const GActionEntry entries[] = {
  { "tag-detail-back", ephy_bookmarks_popover_actions_tag_detail_back }
};

static void
ephy_bookmarks_popover_init (EphyBookmarksPopover *self)
{
  GSequence *tags;
  GSequenceIter *iter;
  g_autoptr (GSequence) bookmarks = NULL;
  g_autoptr (GSimpleActionGroup) group = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "popover",
                                  G_ACTION_GROUP (group));

  gtk_list_box_bind_model (GTK_LIST_BOX (self->bookmarks_list_box),
                           G_LIST_MODEL (self->manager),
                           create_bookmark_row,
                           self, NULL);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->manager)) == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->tags_list_box),
                              (GtkListBoxSortFunc)tags_list_box_sort_func,
                              NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->tag_detail_list_box),
                              (GtkListBoxSortFunc)tags_list_box_sort_func,
                              NULL, NULL);

  tags = ephy_bookmarks_manager_get_tags (self->manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);
    GtkWidget *tag_row;

    if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag)) {
      tag_row = create_tag_row (tag);
      gtk_container_add (GTK_CONTAINER (self->tags_list_box), tag_row);
    }
  }

  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, NULL);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GtkWidget *bookmark_row;

    bookmark_row = create_bookmark_row (bookmark, self);
    gtk_widget_show_all (bookmark_row);
    gtk_container_add (GTK_CONTAINER (self->tags_list_box), bookmark_row);
  }

  g_signal_connect_object (self->manager, "bookmark-added",
                           G_CALLBACK (ephy_bookmarks_popover_bookmark_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "bookmark-removed",
                           G_CALLBACK (ephy_bookmarks_popover_bookmark_removed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "tag-created",
                           G_CALLBACK (ephy_bookmarks_popover_tag_created_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "tag-deleted",
                           G_CALLBACK (ephy_bookmarks_popover_tag_deleted_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "bookmark-tag-added",
                           G_CALLBACK (ephy_bookmarks_popover_bookmark_tag_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "bookmark-tag-removed",
                           G_CALLBACK (ephy_bookmarks_popover_bookmark_tag_removed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->bookmarks_list_box, "row-activated",
                           G_CALLBACK (ephy_bookmarks_popover_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->tags_list_box, "row-activated",
                           G_CALLBACK (ephy_bookmarks_popover_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->tag_detail_list_box, "row-activated",
                           G_CALLBACK (ephy_bookmarks_popover_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->bookmarks_list_box, "button-release-event",
                           G_CALLBACK (ephy_bookmarks_popover_list_box_button_release_event_cb),
                           self, G_CONNECT_SWAPPED);
}

EphyBookmarksPopover *
ephy_bookmarks_popover_new (void)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_POPOVER,
                       NULL);
}
