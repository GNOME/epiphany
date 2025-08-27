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

#include "ephy-bookmarks-dialog.h"

#include "ephy-bookmark.h"
#include "ephy-bookmark-row.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-link.h"
#include "ephy-shell.h"

#include <adwaita.h>
#include <glib/gi18n.h>

struct _EphyBookmarksDialog {
  AdwBin parent_instance;

  GtkWidget *toast_overlay;
  GtkWidget *toolbar_view;
  GtkWidget *edit_button;
  GtkWidget *done_button;
  GtkWidget *toplevel_stack;
  GtkWidget *bookmarks_list_box;
  GtkWidget *tag_detail_list_box;
  GtkWidget *searching_bookmarks_list_box;
  GtkWidget *tag_detail_label;
  GtkWidget *search_entry;
  char *tag_detail_tag;

  EphyBookmarksManager *manager;
};

G_DEFINE_FINAL_TYPE (EphyBookmarksDialog, ephy_bookmarks_dialog, ADW_TYPE_BIN)

#define EPHY_LIST_BOX_ROW_TYPE_BOOKMARK "bookmark"
#define EPHY_LIST_BOX_ROW_TYPE_TAG "tag"

enum {
  MOVE_TAG_ROW,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GtkWidget * create_bookmark_row (gpointer item,
                                        gpointer user_data);
static GtkWidget *create_tag_row (EphyBookmarksDialog *self,
                                  const char          *tag);
static void set_row_is_editable (GtkWidget *row,
                                 gboolean   is_editable);
static void ephy_bookmarks_dialog_show_tag_detail (EphyBookmarksDialog *self,
                                                   const char          *tag);
static void populate_tag_detail_list_box (EphyBookmarksDialog *self,
                                          GSequence           *order);

static void
tag_detail_back (EphyBookmarksDialog *self)
{
  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_list_box_remove_all (GTK_LIST_BOX (self->tag_detail_list_box));
}

static void
update_rows_movable (EphyBookmarksDialog *self,
                     GtkListBox          *list_box)
{
  GtkListBoxRow *row;
  int i = 0;
  int n_rows = 0;

  while ((row = gtk_list_box_get_row_at_index (list_box, i++)))
    n_rows++;

  for (i = 0; (row = gtk_list_box_get_row_at_index (list_box, i)); i++) {
    gtk_widget_action_set_enabled (GTK_WIDGET (row), "row.move-up", i > 0);
    gtk_widget_action_set_enabled (GTK_WIDGET (row), "row.move-down", i < (n_rows - 1));

    if (EPHY_IS_BOOKMARK_ROW (row)) {
      ephy_bookmark_row_set_movable (EPHY_BOOKMARK_ROW (row), n_rows > 1);
    } else {
      GtkWidget *drag_handle = gtk_widget_get_first_child (gtk_widget_get_first_child (
                                                             gtk_widget_get_first_child (GTK_WIDGET (row))));
      GtkWidget *move_menu_button = gtk_widget_get_last_child (gtk_widget_get_last_child (
                                                                 gtk_widget_get_first_child (GTK_WIDGET (row))));

      gtk_widget_set_sensitive (drag_handle, n_rows > 1);
      gtk_widget_set_sensitive (move_menu_button, n_rows > 1);
    }
  }
}

static void
update_bookmarks_order (EphyBookmarksDialog *self)
{
  GtkListBoxRow *row;
  int i = 0;

  ephy_bookmarks_manager_clear_bookmarks_order (self->manager);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
    const char *type = g_object_get_data (G_OBJECT (row), "type");
    const char *item;

    if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0)
      item = ephy_bookmark_row_get_bookmark_url (EPHY_BOOKMARK_ROW (row));
    else
      item = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

    ephy_bookmarks_manager_add_to_bookmarks_order (self->manager, type, item, i - 1);
  }

  ephy_bookmarks_manager_save (self->manager, TRUE, FALSE,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);
  ephy_bookmarks_manager_sort_bookmarks_order (self->manager);
}

static void
update_tags_order (EphyBookmarksDialog *self)
{
  GtkListBoxRow *row;
  int i = 0;
  GSequence *urls;

  ephy_bookmarks_manager_tags_order_clear_tag (self->manager, self->tag_detail_tag);

  urls = g_sequence_new (g_free);
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), i++))) {
    const char *bookmark_url = ephy_bookmark_row_get_bookmark_url (EPHY_BOOKMARK_ROW (row));

    g_sequence_append (urls, g_strdup (bookmark_url));
  }

  ephy_bookmarks_manager_tags_order_add_tag (self->manager, self->tag_detail_tag, urls);

  ephy_bookmarks_manager_save (self->manager, FALSE, TRUE,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);
}

static void
update_tags_order_without_list_box (EphyBookmarksDialog *self,
                                    const char          *tag,
                                    gboolean             do_save)
{
  GSequence *current_order;
  GSequenceIter *iter;
  GSequence *urls;
  GSequence *bookmarks;

  current_order = ephy_bookmarks_manager_tags_order_get_tag (self->manager, tag);

  /* Add URLs to the sequence from the existing variant. Skip bookmarks that
   * aren't in the tag anymore. */
  urls = g_sequence_new (g_free);
  if (current_order) {
    for (iter = g_sequence_get_begin_iter (current_order);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      const char *url = g_sequence_get (iter);
      EphyBookmark *bookmark = ephy_bookmarks_manager_get_bookmark_by_url (self->manager, url);

      if (!bookmark)
        continue;

      if (!ephy_bookmark_has_tag (bookmark, tag))
        continue;

      g_sequence_append (urls, g_strdup (url));
    }
    g_sequence_free (current_order);
  }

  /* Add any bookmarks not already added to the sequence. */
  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, tag);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    const char *url = ephy_bookmark_get_url (bookmark);
    GSequenceIter *lookup_iter;

    lookup_iter = g_sequence_lookup (urls,
                                     (gpointer)url,
                                     (GCompareDataFunc)g_strcmp0,
                                     NULL);
    if (!lookup_iter)
      g_sequence_insert_sorted (urls, g_strdup (url), (GCompareDataFunc)g_strcmp0, NULL);
  }

  ephy_bookmarks_manager_tags_order_clear_tag (self->manager, tag);
  ephy_bookmarks_manager_tags_order_add_tag (self->manager, tag, urls);

  if (do_save) {
    ephy_bookmarks_manager_save (self->manager, FALSE, TRUE,
                                 ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                                 ephy_bookmarks_manager_save_warn_on_error_cb,
                                 NULL);
  }
}

static void
row_moved_cb (AdwActionRow        *row,
              AdwActionRow        *dest_row,
              EphyBookmarksDialog *self)
{
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (dest_row));
  GtkWidget *list_box = g_object_steal_data (G_OBJECT (row), "list-box");
  GtkWidget *dest_list_box = gtk_widget_get_parent (GTK_WIDGET (dest_row));
  const char *visible_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));

  if (list_box != dest_list_box) {
    return;
  }

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX (dest_list_box));

  g_object_ref (row);
  gtk_list_box_remove (GTK_LIST_BOX (dest_list_box),
                       GTK_WIDGET (row));
  gtk_list_box_insert (GTK_LIST_BOX (dest_list_box),
                       GTK_WIDGET (row), index);
  g_object_unref (row);

  if (g_strcmp0 (visible_child, "default") == 0) {
    update_bookmarks_order (self);
    g_signal_emit_by_name (self->manager, "sorted", NULL);
  } else {
    update_tags_order (self);
    g_signal_emit_by_name (self->manager, "sorted", self->tag_detail_tag);
  }
}

static void
remove_bookmark_row (EphyBookmarksDialog *self,
                     GtkListBox          *list_box,
                     const char          *url)
{
  GtkListBoxRow *row;
  int i = 0;

  g_assert (GTK_IS_LIST_BOX (list_box));

  while ((row = gtk_list_box_get_row_at_index (list_box, i++))) {
    const char *type = g_object_get_data (G_OBJECT (row), "type");

    if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0 &&
        g_strcmp0 (ephy_bookmark_row_get_bookmark_url (EPHY_BOOKMARK_ROW (row)), url) == 0) {
      gtk_list_box_remove (list_box, GTK_WIDGET (row));
      break;
    }
  }
}

static void
remove_tag_row (EphyBookmarksDialog *self,
                const char          *tag)
{
  GtkListBoxRow *row;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

    if (g_strcmp0 (title, tag) == 0)
      gtk_list_box_remove (GTK_LIST_BOX (self->bookmarks_list_box), GTK_WIDGET (row));
  }

  update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
  update_bookmarks_order (self);

  i = 0;
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->searching_bookmarks_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

    if (g_strcmp0 (title, tag) == 0)
      gtk_list_box_remove (GTK_LIST_BOX (self->searching_bookmarks_list_box), GTK_WIDGET (row));
  }
}

static void
ephy_bookmarks_dialog_bookmark_tag_added_cb (EphyBookmarksDialog  *self,
                                             EphyBookmark         *bookmark,
                                             const char           *tag,
                                             EphyBookmarksManager *manager)
{
  gboolean exists;
  const char *visible_stack_child;
  GtkListBoxRow *row;
  int i = 0;

  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));

  /* If the bookmark no longer has 0 tags, we remove it from the tags list box */
  if (g_sequence_get_length (ephy_bookmark_get_tags (bookmark)) == 1) {
    remove_bookmark_row (self, GTK_LIST_BOX (self->bookmarks_list_box),
                         ephy_bookmark_get_url (bookmark));
    update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
  }

  /* If we are on the tag detail list box, then the user has toggled the state
   * of the tag widget multiple times. The first time the bookmark was removed
   * from the list box. Now we have to add it back. */
  visible_stack_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));
  if (g_strcmp0 (visible_stack_child, "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    GtkWidget *bookmark_row;

    bookmark_row = create_bookmark_row (bookmark, self);
    gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), bookmark_row);
    update_rows_movable (self, GTK_LIST_BOX (self->tag_detail_list_box));
    update_tags_order (self);
  } else {
    update_tags_order_without_list_box (self, tag, TRUE);
  }

  exists = FALSE;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    const char *type = g_object_get_data (G_OBJECT (row), "type");

    if (g_strcmp0 (title, tag) == 0 &&
        g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0) {
      exists = TRUE;
      break;
    }
  }

  if (!exists) {
    GtkWidget *tag_row = create_tag_row (self, tag);

    gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), tag_row);
    update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
    update_bookmarks_order (self);

    tag_row = create_tag_row (self, tag);
    set_row_is_editable (tag_row, FALSE);
    gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), tag_row);
  }
}

static void
ephy_bookmarks_dialog_bookmark_tag_removed_cb (EphyBookmarksDialog  *self,
                                               EphyBookmark         *bookmark,
                                               const char           *tag,
                                               EphyBookmarksManager *manager)
{
  const char *visible_stack_child;
  gboolean exists;

  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));

  /* If the bookmark has 0 tags after removing one, we add it to the tags list
   * box */
  if (g_sequence_is_empty (ephy_bookmark_get_tags (bookmark))) {
    GtkListBoxRow *row;
    int i = 0;

    exists = FALSE;
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
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

      gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), row);
      update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
    }
  }

  /* If we are on the tag detail list box of the tag that was removed, we
   * remove the bookmark from it to reflect the changes. */
  visible_stack_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));
  if (g_strcmp0 (visible_stack_child, "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    remove_bookmark_row (self, GTK_LIST_BOX (self->tag_detail_list_box),
                         ephy_bookmark_get_url (bookmark));
    update_rows_movable (self, GTK_LIST_BOX (self->tag_detail_list_box));
    update_tags_order (self);

    /* If we removed the tag's last bookmark, switch back to the tags list. */
    if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag))
      tag_detail_back (self);
  } else {
    update_tags_order_without_list_box (self, tag, TRUE);
  }

  /* If the tag no longer contains bookmarks, remove it from the tags list */
  if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag))
    remove_tag_row (self, tag);
}

static GtkWidget *
create_bookmark_row (gpointer item,
                     gpointer user_data)
{
  EphyBookmark *bookmark = EPHY_BOOKMARK (item);
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);
  GtkWidget *row;
  gboolean is_editing = !gtk_widget_is_visible (self->edit_button);

  row = ephy_bookmark_row_new (bookmark);
  g_object_set_data_full (G_OBJECT (row), "type",
                          g_strdup (EPHY_LIST_BOX_ROW_TYPE_BOOKMARK),
                          (GDestroyNotify)g_free);

  g_signal_connect_object (row, "move-row", G_CALLBACK (row_moved_cb), self, 0);

  set_row_is_editable (row, is_editing);

  return row;
}

static void
on_tag_row_activated (AdwActionRow *row,
                      gpointer      user_data)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);
  const char *tag = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

  ephy_bookmarks_dialog_show_tag_detail (self, tag);
}

static GdkContentProvider *
tag_row_drag_prepare_cb (AdwActionRow *self,
                         double        x,
                         double        y)
{
  return gdk_content_provider_new_typed (ADW_TYPE_ACTION_ROW, self);
}

static void
tag_row_drag_begin_cb (AdwActionRow *self,
                       GdkDrag      *drag)
{
  GtkWidget *drag_list;
  GtkWidget *drag_row;
  GtkWidget *drag_image;
  GtkWidget *drag_icon;
  int width, height;
  const char *tag = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self));

  width = gtk_widget_get_width (GTK_WIDGET (self));
  height = gtk_widget_get_height (GTK_WIDGET (self));

  drag_list = gtk_list_box_new ();
  gtk_widget_set_size_request (drag_list, width, height);
  gtk_widget_add_css_class (drag_list, "boxed-list");

  drag_row = adw_action_row_new ();
  if (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    drag_image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic");
  else
    drag_image = gtk_image_new_from_icon_name ("ephy-bookmark-tag-symbolic");
  adw_action_row_add_prefix (ADW_ACTION_ROW (drag_row), drag_image);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (drag_row), tag);

  drag_image = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (drag_row), drag_image);

  drag_image = gtk_image_new_from_icon_name ("list-drag-handle-symbolic");
  adw_action_row_add_prefix (ADW_ACTION_ROW (drag_row), drag_image);

  gtk_list_box_append (GTK_LIST_BOX (drag_list), drag_row);

  drag_icon = gtk_drag_icon_get_for_drag (drag);
  gtk_widget_add_css_class (drag_icon, "boxed-list");
  gtk_drag_icon_set_child (GTK_DRAG_ICON (drag_icon), drag_list);
}

static gboolean
tag_row_drop_cb (AdwActionRow *self,
                 const GValue *value,
                 double        x,
                 double        y)
{
  AdwActionRow *source;

  if (!G_VALUE_HOLDS (value, ADW_TYPE_ACTION_ROW))
    return FALSE;

  source = g_value_get_object (value);
  g_object_set_data (G_OBJECT (source), "list-box", gtk_widget_get_parent (GTK_WIDGET (source)));

  if (EPHY_IS_BOOKMARK_ROW (source))
    g_signal_emit_by_name (source, "move-row", self);
  else
    g_signal_emit (source, signals[MOVE_TAG_ROW], 0, self);

  return TRUE;
}

static void
tag_row_move_up_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  GtkWidget *row = GTK_WIDGET (user_data);
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (row));
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)) - 1;
  GtkListBoxRow *prev_row = gtk_list_box_get_row_at_index (list_box, index);

  if (!prev_row)
    return;

  g_object_set_data (G_OBJECT (row), "list-box", list_box);
  g_signal_emit (ADW_ACTION_ROW (row), signals[MOVE_TAG_ROW], 0, ADW_ACTION_ROW (prev_row));
}

static void
tag_row_move_down_cb (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  GtkWidget *row = GTK_WIDGET (user_data);
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (row));
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, index);

  if (!next_row)
    return;

  g_object_set_data (G_OBJECT (row), "list-box", list_box);
  g_signal_emit (ADW_ACTION_ROW (row), signals[MOVE_TAG_ROW], 0, ADW_ACTION_ROW (next_row));
}

static GActionGroup *
create_tag_row_action_group (GtkWidget *row)
{
  const GActionEntry entries[] = {
    { "move-up", tag_row_move_up_cb },
    { "move-down", tag_row_move_down_cb },
  };

  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), row);

  return G_ACTION_GROUP (group);
}

static GtkWidget *
create_tag_row (EphyBookmarksDialog *self,
                const char          *tag)
{
  GtkWidget *row;
  GtkWidget *image;
  GtkWidget *move_menu_button;
  GMenu *move_menu;
  GtkWidget *drag_image;
  GtkDragSource *source;
  GtkDropTarget *target;
  gboolean is_editing;

  row = adw_action_row_new ();
  g_object_set_data_full (G_OBJECT (row), "type",
                          g_strdup (EPHY_LIST_BOX_ROW_TYPE_TAG),
                          (GDestroyNotify)g_free);

  if (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic");
  else
    image = gtk_image_new_from_icon_name ("ephy-bookmark-tag-symbolic");

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), tag);
  gtk_widget_set_tooltip_text (row, tag);

  image = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), image);

  move_menu_button = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (move_menu_button), "view-more-symbolic");
  gtk_widget_set_receives_default (move_menu_button, FALSE);
  gtk_widget_set_valign (move_menu_button, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (move_menu_button, _("Move Controls"));
  gtk_widget_add_css_class (move_menu_button, "flat");

  gtk_widget_insert_action_group (row, "row", create_tag_row_action_group (row));
  move_menu = g_menu_new ();
  g_menu_append (move_menu, _("Move Up"), "row.move-up");
  g_menu_append (move_menu, _("Move Down"), "row.move-down");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (move_menu_button), G_MENU_MODEL (move_menu));
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), move_menu_button);

  drag_image = gtk_image_new_from_icon_name ("list-drag-handle-symbolic");
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), drag_image);

  g_signal_connect_object (row, "activated", G_CALLBACK (on_tag_row_activated), self, 0);
  g_signal_connect_object (row, "move-tag-row", G_CALLBACK (row_moved_cb), self, 0);

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
  g_signal_connect_swapped (source, "prepare", G_CALLBACK (tag_row_drag_prepare_cb), row);
  g_signal_connect_swapped (source, "drag-begin", G_CALLBACK (tag_row_drag_begin_cb), row);
  gtk_widget_add_controller (drag_image, GTK_EVENT_CONTROLLER (source));

  target = gtk_drop_target_new (ADW_TYPE_ACTION_ROW, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (target, TRUE);
  g_signal_connect_swapped (target, "drop", G_CALLBACK (tag_row_drop_cb), row);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (target));

  is_editing = !gtk_widget_is_visible (self->edit_button);
  set_row_is_editable (row, is_editing);

  return row;
}

static void
bookmark_removed_toast_dismissed (AdwToast     *toast,
                                  EphyBookmark *bookmark)
{
  g_object_unref (bookmark);
}

static void
bookmark_removed_toast_button_clicked (AdwToast     *toast,
                                       EphyBookmark *bookmark)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  if (ephy_bookmarks_manager_get_bookmark_by_url (manager, ephy_bookmark_get_url (bookmark)))
    return;

  ephy_bookmarks_manager_add_bookmark (manager, bookmark);
}

void
ephy_bookmarks_dialog_bookmark_removed_toast (EphyBookmarksDialog *self,
                                              EphyBookmark        *bookmark,
                                              AdwToast            *toast)
{
  g_signal_connect_object (toast, "dismissed", G_CALLBACK (bookmark_removed_toast_dismissed), bookmark, 0);
  g_signal_connect_object (toast, "button-clicked", G_CALLBACK (bookmark_removed_toast_button_clicked), bookmark, 0);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay), toast);
}

static void
ephy_bookmarks_dialog_bookmark_added_cb (EphyBookmarksDialog  *self,
                                         EphyBookmark         *bookmark,
                                         EphyBookmarksManager *manager)
{
  GtkWidget *row;
  GSequence *tags;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  tags = ephy_bookmark_get_tags (bookmark);
  if (g_sequence_is_empty (tags)) {
    row = create_bookmark_row (bookmark, self);
    gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), row);
    update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
    update_bookmarks_order (self);
  } else {
    GSequenceIter *iter;

    for (iter = g_sequence_get_begin_iter (tags);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      const char *tag = g_sequence_get (iter);
      GtkListBoxRow *existing_row;
      int i = 0;
      gboolean exists = FALSE;

      update_tags_order_without_list_box (self, tag, FALSE);

      while ((existing_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
        const char *type = g_object_get_data (G_OBJECT (existing_row), "type");

        if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0 &&
            g_strcmp0 (adw_preferences_row_get_title (ADW_PREFERENCES_ROW (existing_row)), tag) == 0) {
          exists = TRUE;
          break;
        }
      }

      if (!exists) {
        row = create_tag_row (self, tag);
        gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), row);
        update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
        update_bookmarks_order (self);
      }
    }
  }

  ephy_bookmarks_manager_save (self->manager, FALSE, TRUE,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  row = create_bookmark_row (bookmark, self);
  set_row_is_editable (row, FALSE);
  gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), row);

  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "empty-state") == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
    gtk_widget_set_visible (self->search_entry, TRUE);
    gtk_widget_set_visible (self->edit_button, TRUE);
  } else if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0) {
    if (ephy_bookmark_has_tag (bookmark, self->tag_detail_tag)) {
      row = create_bookmark_row (bookmark, self);
      gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), row);
      update_rows_movable (self, GTK_LIST_BOX (self->tag_detail_list_box));
    }
  }
}

static void
ephy_bookmarks_dialog_bookmark_removed_cb (EphyBookmarksDialog  *self,
                                           EphyBookmark         *bookmark,
                                           EphyBookmarksManager *manager)
{
  GSequence *tags;
  GSequenceIter *iter;
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  GtkApplication *application = GTK_APPLICATION (ephy_shell_get_default ());
  EphyWindow *active_window = EPHY_WINDOW (gtk_application_get_active_window (application));

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  remove_bookmark_row (self, GTK_LIST_BOX (self->bookmarks_list_box),
                       ephy_bookmark_get_url (bookmark));
  remove_bookmark_row (self, GTK_LIST_BOX (self->tag_detail_list_box),
                       ephy_bookmark_get_url (bookmark));
  remove_bookmark_row (self, GTK_LIST_BOX (self->searching_bookmarks_list_box),
                       ephy_bookmark_get_url (bookmark));

  update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
  update_rows_movable (self, GTK_LIST_BOX (self->tag_detail_list_box));

  if (g_list_model_get_n_items (G_LIST_MODEL (self->manager)) == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");
    gtk_widget_set_visible (self->search_entry, FALSE);
    gtk_widget_set_visible (self->edit_button, FALSE);
    ephy_bookmarks_dialog_set_is_editing (self, FALSE);
  } else if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0 &&
             !ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, self->tag_detail_tag)) {
    /* If we removed the tag's last bookmark, switch back to the tags list. */
    tag_detail_back (self);
  }

  tags = ephy_bookmarks_manager_get_tags (self->manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);

    update_tags_order_without_list_box (self, tag, FALSE);

    /* If the tag no longer contains bookmarks, remove it from the tags list */
    if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag)) {
      remove_tag_row (self, tag);
    }
  }

  update_bookmarks_order (self);
  ephy_bookmarks_manager_save (self->manager, FALSE, TRUE,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  if (window == active_window) {
    AdwToast *toast = adw_toast_new (_("Bookmark removed"));

    adw_toast_set_button_label (toast, _("_Undo"));
    if (ephy_window_get_show_sidebar (window))
      ephy_bookmarks_dialog_bookmark_removed_toast (self, bookmark, toast);
    else
      ephy_window_bookmark_removed_toast (window, bookmark, toast);
  }
}

static void
ephy_bookmarks_dialog_tag_created_cb (EphyBookmarksDialog  *self,
                                      const char           *tag,
                                      EphyBookmarksManager *manager)
{
  GtkWidget *tag_row;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (tag);
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  tag_row = create_tag_row (self, tag);
  gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), tag_row);
  update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
  update_bookmarks_order (self);

  tag_row = create_tag_row (self, tag);
  set_row_is_editable (tag_row, FALSE);
  gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), tag_row);
}

static void
ephy_bookmarks_dialog_tag_deleted_cb (EphyBookmarksDialog  *self,
                                      const char           *tag,
                                      EphyBookmarksManager *manager)
{
  GtkListBoxRow *row;
  int i = 0;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    if (g_strcmp0 (title, tag) == 0) {
      gtk_list_box_remove (GTK_LIST_BOX (self->bookmarks_list_box), GTK_WIDGET (row));
      update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
      break;
    }
  }

  update_bookmarks_order (self);

  i = 0;
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->searching_bookmarks_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    if (g_strcmp0 (title, tag) == 0) {
      gtk_list_box_remove (GTK_LIST_BOX (self->searching_bookmarks_list_box), GTK_WIDGET (row));
      break;
    }
  }

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0)
    tag_detail_back (self);

  ephy_bookmarks_manager_tags_order_clear_tag (self->manager, tag);
  ephy_bookmarks_manager_save (self->manager, FALSE, TRUE,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);
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

  title1 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row1));
  title2 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row2));

  if (g_strcmp0 (type1, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0
      && g_strcmp0 (type2, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0)
    return ephy_bookmark_tags_compare (title1, title2);

  if (g_strcmp0 (type1, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0
      && g_strcmp0 (type2, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0)
    return ephy_bookmark_bookmarks_compare_func (ephy_bookmark_row_get_bookmark (EPHY_BOOKMARK_ROW (row1)),
                                                 ephy_bookmark_row_get_bookmark (EPHY_BOOKMARK_ROW (row2)));

  if (g_strcmp0 (type1, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0)
    return -1;
  if (g_strcmp0 (type2, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0)
    return 1;

  return g_strcmp0 (title1, title2);
}

static gboolean
tags_list_box_filter_func (GtkListBoxRow *row,
                           gpointer       user_data)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);
  g_autofree gchar *search_casefold = NULL;
  g_autofree gchar *title_casefold = NULL;
  const char *title;
  const char *search_text;

  g_assert (GTK_IS_LIST_BOX_ROW (row));

  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  title_casefold = g_utf8_casefold (title, -1);

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  search_casefold = g_utf8_casefold (search_text, -1);

  return !!strstr (title_casefold, search_casefold);
}

static void
ephy_bookmarks_dialog_show_tag_detail (EphyBookmarksDialog *self,
                                       const char          *tag)
{
  GSequence *tag_order;

  tag_order = ephy_bookmarks_manager_tags_order_get_tag (self->manager, tag);
  if (tag_order) {
    populate_tag_detail_list_box (self, tag_order);
  } else {
    GSequence *bookmarks;
    GSequenceIter *iter;

    bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, tag);
    for (iter = g_sequence_get_begin_iter (bookmarks);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      EphyBookmark *bookmark = g_sequence_get (iter);
      GtkWidget *row;

      row = create_bookmark_row (bookmark, self);
      gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), row);
    }

    update_rows_movable (self, GTK_LIST_BOX (self->tag_detail_list_box));
    g_sequence_free (bookmarks);
  }

  gtk_label_set_label (GTK_LABEL (self->tag_detail_label), tag);

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_widget_set_state_flags (self->search_entry, GTK_STATE_FLAG_NORMAL, TRUE);

  if (self->tag_detail_tag)
    g_free (self->tag_detail_tag);
  self->tag_detail_tag = g_strdup (tag);

  update_tags_order (self);
}

static void
row_clicked_cb (GtkGesture          *gesture,
                int                  n_click,
                double               x,
                double               y,
                EphyBookmarksDialog *self)
{
  GtkWidget *list;
  GtkListBoxRow *row;
  guint button;
  const char *type;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button != GDK_BUTTON_PRIMARY && button != GDK_BUTTON_MIDDLE) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  list = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  g_assert (GTK_IS_LIST_BOX (list));

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

  row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (list), y);
  if (!row)
    return;

  type = g_object_get_data (G_OBJECT (row), "type");
  if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0) {
    GdkModifierType modifiers;
    EphyLinkFlags flags;

    modifiers = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
    modifiers &= gtk_accelerator_get_default_mod_mask ();

    flags = ephy_link_flags_from_modifiers (modifiers, button == GDK_BUTTON_MIDDLE);

    ephy_bookmark_row_open (EPHY_BOOKMARK_ROW (row), flags);

    /* Close the bookmarks sidebar if flags == 0, since this indicates we are not opening the link in a new tab. */
    if (flags == 0) {
      EphyWindow *window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
      ephy_window_toggle_bookmarks (window);
    }
  } else {
    const char *tag = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    ephy_bookmarks_dialog_show_tag_detail (self, tag);
  }
}

static void
set_row_is_editable (GtkWidget *row,
                     gboolean   is_editable)
{
  GtkWidget *drag_handle = gtk_widget_get_first_child (gtk_widget_get_first_child (gtk_widget_get_first_child (row)));
  GtkWidget *buttons_box = gtk_widget_get_last_child (gtk_widget_get_first_child (row));

  gtk_widget_set_visible (drag_handle, is_editable);
  if (EPHY_IS_BOOKMARK_ROW (row))
    gtk_widget_set_visible (buttons_box, is_editable);
  else
    gtk_widget_set_visible (gtk_widget_get_last_child (buttons_box), is_editable);
}

void
ephy_bookmarks_dialog_set_is_editing (EphyBookmarksDialog *self,
                                      gboolean             is_editing)
{
  GtkListBoxRow *row;
  int i = 0;

  gtk_widget_set_visible (self->edit_button, !is_editing);
  gtk_widget_set_visible (self->done_button, is_editing);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++)))
    set_row_is_editable (GTK_WIDGET (row), is_editing);
  i = 0;
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), i++)))
    set_row_is_editable (GTK_WIDGET (row), is_editing);
}

static void
on_close_button_clicked (GtkButton *button,
                         gpointer   user_data)
{
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (button)));

  ephy_window_toggle_bookmarks (window);
}

static void
on_edit_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);

  ephy_bookmarks_dialog_set_is_editing (self, TRUE);
}

static void
on_done_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);

  ephy_bookmarks_dialog_set_is_editing (self, FALSE);
}

static void
on_search_entry_changed (GtkSearchEntry *entry,
                         gpointer        user_data)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);
  const char *entry_text = gtk_editable_get_text (GTK_EDITABLE (entry));
  const char *visible_stack_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));
  GtkListBoxRow *row;
  int idx = 0;
  int mapped = 0;

  if (g_strcmp0 (entry_text, "") != 0) {
    ephy_bookmarks_dialog_set_is_editing (self, FALSE);
    gtk_widget_set_sensitive (self->edit_button, FALSE);
  } else {
    gtk_widget_set_sensitive (self->edit_button, TRUE);
  }

  if (g_strcmp0 (entry_text, "") != 0 && g_strcmp0 (visible_stack_child, "default") == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "searching_bookmarks");
  } else if (g_strcmp0 (entry_text, "") == 0 && g_strcmp0 (visible_stack_child, "searching_bookmarks") == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
  }

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->tag_detail_list_box));
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->searching_bookmarks_list_box));

  if (g_strcmp0 (entry_text, "") != 0 &&
      g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "empty-state") == 0) {
    if ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0)))
      gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");
    else
      gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "searching_bookmarks");
  }

  if ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0))) {
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), idx++))) {
      if (gtk_widget_get_mapped (GTK_WIDGET (row)))
        mapped++;
    }
  } else {
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->searching_bookmarks_list_box), idx++))) {
      if (gtk_widget_get_mapped (GTK_WIDGET (row)))
        mapped++;
    }
  }

  if (mapped != 0)
    return;

  if (g_strcmp0 (entry_text, "") != 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");
  else if ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0)))
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
}

static gboolean
on_search_entry_key_pressed (GtkEventControllerKey *key_controller,
                             guint                  keyval,
                             guint                  keycode,
                             GdkModifierType        state,
                             EphyBookmarksDialog   *self)
{
  if (keyval == GDK_KEY_Escape) {
    GtkWidget *window = gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW);

    ephy_window_toggle_bookmarks (EPHY_WINDOW (window));
    return TRUE;
  }

  return FALSE;
}

static void
populate_bookmarks_list_box (EphyBookmarksDialog *self)
{
  GSequence *order = NULL;
  GSequenceIter *iter;

  order = ephy_bookmarks_manager_get_bookmarks_order (self->manager);

  for (iter = g_sequence_get_begin_iter (order);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    GVariant *variant = g_sequence_get (iter);
    const char *type, *item;
    int index;
    GtkWidget *row;

    g_variant_get (variant, "(ssi)", &type, &item, &index);

    if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0) {
      EphyBookmark *bookmark = ephy_bookmarks_manager_get_bookmark_by_url (self->manager, item);

      row = create_bookmark_row (bookmark, self);
    } else {
      row = create_tag_row (self, item);
    }

    gtk_list_box_insert (GTK_LIST_BOX (self->bookmarks_list_box), row, index);
  }

  update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
}

static void
populate_tag_detail_list_box (EphyBookmarksDialog *self,
                              GSequence           *order)
{
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (order);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *url = g_sequence_get (iter);
    EphyBookmark *bookmark = ephy_bookmarks_manager_get_bookmark_by_url (self->manager, url);
    GtkWidget *row = create_bookmark_row (bookmark, self);

    gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), row);
  }

  update_rows_movable (self, GTK_LIST_BOX (self->tag_detail_list_box));
}

static void
ephy_bookmarks_dialog_sorted_cb (EphyBookmarksDialog  *self,
                                 const char           *view,
                                 EphyBookmarksManager *manager)
{
  if (g_strcmp0 (view, NULL) == 0) {
    gtk_list_box_remove_all (GTK_LIST_BOX (self->bookmarks_list_box));
    populate_bookmarks_list_box (self);
  } else if (g_strcmp0 (self->tag_detail_tag, view) == 0) {
    GSequence *order = ephy_bookmarks_manager_tags_order_get_tag (self->manager, view);

    gtk_list_box_remove_all (GTK_LIST_BOX (self->tag_detail_list_box));

    if (order)
      populate_tag_detail_list_box (self, order);
  }
}

static void
ephy_bookmarks_dialog_finalize (GObject *object)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (object);

  g_free (self->tag_detail_tag);

  G_OBJECT_CLASS (ephy_bookmarks_dialog_parent_class)->finalize (object);
}

static void
ephy_bookmarks_dialog_class_init (EphyBookmarksDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ephy_bookmarks_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmarks-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, toolbar_view);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, edit_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, done_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, toplevel_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, bookmarks_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, tag_detail_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, searching_bookmarks_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, tag_detail_label);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_close_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_edit_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_done_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_key_pressed);

  /* FIXME: Once tag rows are moved to their own file, move this signal
   * definition there too. We should define the signal on an Epiphany
   * type, not on ADW_TYPE_ACTION_ROW.
   *
   * https://gitlab.gnome.org/GNOME/epiphany/-/issues/2721
   */
  signals[MOVE_TAG_ROW] =
    g_signal_new ("move-tag-row",
                  ADW_TYPE_ACTION_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  ADW_TYPE_ACTION_ROW);

  gtk_widget_class_install_action (widget_class, "dialog.tag-detail-back", NULL,
                                   (GtkWidgetActionActivateFunc)tag_detail_back);
}

static void
ephy_bookmarks_dialog_init (EphyBookmarksDialog *self)
{
  GSequence *tags;
  GSequenceIter *iter;
  GSequence *bookmarks;
  GtkGesture *gesture;
  GtkFilter *filter;
  g_autoptr (GtkFilterListModel) filter_model = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  filter = GTK_FILTER (gtk_string_filter_new (gtk_property_expression_new (EPHY_TYPE_BOOKMARK, NULL, "title")));
  g_object_bind_property (self->search_entry, "text", filter, "search", 0);
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (g_object_ref (self->manager)), filter);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->manager)) == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");
    gtk_widget_set_visible (self->search_entry, FALSE);
    gtk_widget_set_visible (self->edit_button, FALSE);
  }

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->searching_bookmarks_list_box),
                              (GtkListBoxSortFunc)tags_list_box_sort_func,
                              NULL, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->searching_bookmarks_list_box),
                                (GtkListBoxFilterFunc)tags_list_box_filter_func,
                                self, NULL);

  tags = ephy_bookmarks_manager_get_tags (self->manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);
    GtkWidget *tag_row;

    if (ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag)) {
      tag_row = create_tag_row (self, tag);
      set_row_is_editable (tag_row, FALSE);
      gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), tag_row);
    }
  }

  bookmarks = ephy_bookmarks_manager_get_bookmarks (self->manager);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GtkWidget *bookmark_row;

    bookmark_row = create_bookmark_row (bookmark, self);
    set_row_is_editable (bookmark_row, FALSE);
    gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), bookmark_row);
  }

  if (!g_sequence_is_empty (ephy_bookmarks_manager_get_bookmarks_order (self->manager))) {
    populate_bookmarks_list_box (self);
  } else {
    tags = ephy_bookmarks_manager_get_tags (self->manager);
    for (iter = g_sequence_get_begin_iter (tags);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      const char *tag = g_sequence_get (iter);
      GtkWidget *tag_row;

      if (ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag)) {
        tag_row = create_tag_row (self, tag);
        gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), tag_row);
      }
    }

    bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, NULL);
    for (iter = g_sequence_get_begin_iter (bookmarks);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      EphyBookmark *bookmark = g_sequence_get (iter);
      GtkWidget *bookmark_row;

      bookmark_row = create_bookmark_row (bookmark, self);
      gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), bookmark_row);
    }

    update_rows_movable (self, GTK_LIST_BOX (self->bookmarks_list_box));
    update_bookmarks_order (self);
  }

  g_signal_connect_object (self->manager, "bookmark-added",
                           G_CALLBACK (ephy_bookmarks_dialog_bookmark_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "bookmark-removed",
                           G_CALLBACK (ephy_bookmarks_dialog_bookmark_removed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "tag-created",
                           G_CALLBACK (ephy_bookmarks_dialog_tag_created_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "tag-deleted",
                           G_CALLBACK (ephy_bookmarks_dialog_tag_deleted_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "bookmark-tag-added",
                           G_CALLBACK (ephy_bookmarks_dialog_bookmark_tag_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "bookmark-tag-removed",
                           G_CALLBACK (ephy_bookmarks_dialog_bookmark_tag_removed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager, "sorted",
                           G_CALLBACK (ephy_bookmarks_dialog_sorted_cb),
                           self, G_CONNECT_SWAPPED);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (row_clicked_cb), self);
  gtk_widget_add_controller (self->bookmarks_list_box, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (row_clicked_cb), self);
  gtk_widget_add_controller (self->tag_detail_list_box, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (row_clicked_cb), self);
  gtk_widget_add_controller (self->searching_bookmarks_list_box, GTK_EVENT_CONTROLLER (gesture));
}

GtkWidget *
ephy_bookmarks_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_DIALOG,
                       NULL);
}

void
ephy_bookmarks_dialog_focus (EphyBookmarksDialog *self)
{
  gtk_widget_grab_focus (self->search_entry);
}

void
ephy_bookmarks_dialog_clear_search (EphyBookmarksDialog *self)
{
  gtk_editable_delete_text (GTK_EDITABLE (self->search_entry), 0, -1);
}
