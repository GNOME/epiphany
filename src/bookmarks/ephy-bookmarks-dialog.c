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
#include "ephy-settings.h"
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
  MOVE_ROW,
  ORDER_UPDATED,
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

static void
tag_detail_back (EphyBookmarksDialog *self)
{
  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_list_box_remove_all (GTK_LIST_BOX (self->tag_detail_list_box));
}

static void
update_bookmarks_order (EphyBookmarksDialog *self)
{
  GVariantBuilder builder;
  GVariant *variant;
  GtkListBoxRow *row;
  int i = 0;
  gboolean not_empty = FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->bookmarks_list_box), i++))) {
    GVariantDict dict;
    const char *type = g_object_get_data (G_OBJECT (row), "type");
    const char *item;

    if (g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_BOOKMARK) == 0) {
      EphyBookmark *bookmark = ephy_bookmark_row_get_bookmark (EPHY_BOOKMARK_ROW (row));
      item = ephy_bookmark_get_id (bookmark);
    } else {
      item = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    }

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "type", "s", type);
    g_variant_dict_insert (&dict, "item", "s", item);
    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
    not_empty = TRUE;
  }

  if (!not_empty) {
    GVariantDict dict;

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "tag", "s", "");
    g_variant_dict_insert (&dict, "bookmark-id", "s", "");
    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
  }

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_BOOKMARKS_ORDER, variant);
}

static void
update_tags_order (EphyBookmarksDialog *self,
                   const char          *tag)
{
  GVariantBuilder builder;
  g_autoptr (GVariantIter) iter;
  GVariant *variant;
  GtkListBoxRow *row;
  int i = 0;
  gboolean not_empty = FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  /* Add the other tags to the builder. */
  g_settings_get (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_TAGS_ORDER, "aa{sv}", &iter);
  while ((variant = g_variant_iter_next_value (iter))) {
    GVariantDict dict;
    const char *variant_tag, *bookmark_id;

    g_variant_dict_init (&dict, variant);
    g_variant_dict_lookup (&dict, "tag", "&s", &variant_tag);
    g_variant_dict_lookup (&dict, "bookmark-id", "&s", &bookmark_id);

    if (g_strcmp0 (tag, variant_tag) != 0) {
      g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
      not_empty = TRUE;
    }

    g_variant_unref (variant);
  }

  /* Add the tag's bookmarks to the builder from the list box. */
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), i++))) {
    GVariantDict dict;
    EphyBookmark *bookmark = ephy_bookmark_row_get_bookmark (EPHY_BOOKMARK_ROW (row));
    const char *bookmark_id = ephy_bookmark_get_id (bookmark);

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "tag", "s", tag);
    g_variant_dict_insert (&dict, "bookmark-id", "s", bookmark_id);
    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
    not_empty = TRUE;
  }

  if (!not_empty) {
    GVariantDict dict;

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "tag", "s", "");
    g_variant_dict_insert (&dict, "bookmark-id", "s", "");
    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
  }

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_TAGS_ORDER, variant);
}

static void
update_tags_order_without_list_box (EphyBookmarksDialog *self,
                                    const char          *tag)
{
  GVariantBuilder builder;
  g_autoptr (GVariantIter) iter;
  GVariant *variant;
  gboolean not_empty = FALSE;
  EphyBookmark *bookmark;
  GSequence *bookmarks;
  GSequenceIter *seq_iter;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, tag);

  g_settings_get (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_TAGS_ORDER, "aa{sv}", &iter);
  while ((variant = g_variant_iter_next_value (iter))) {
    GVariantDict dict;
    const char *variant_tag, *bookmark_id;

    g_variant_dict_init (&dict, variant);
    g_variant_dict_lookup (&dict, "tag", "&s", &variant_tag);
    g_variant_dict_lookup (&dict, "bookmark-id", "&s", &bookmark_id);

    if (g_strcmp0 (tag, variant_tag) != 0) {
      /* Add the other tags to the builder. */
      g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
      not_empty = TRUE;
    } else if (ephy_bookmarks_manager_tag_exists (self->manager, tag)) {
      bookmark = ephy_bookmarks_manager_get_bookmark_by_id (self->manager, bookmark_id);

      if (bookmark == NULL)
        continue;

      seq_iter = g_sequence_lookup (bookmarks,
                                    (gpointer)bookmark,
                                    (GCompareDataFunc)ephy_bookmark_bookmarks_compare_func,
                                    NULL);

      if (seq_iter != NULL) {
        g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
        not_empty = TRUE;
        g_sequence_remove (seq_iter);
      }
    }

    g_variant_unref (variant);
  }

  /* Add any bookmarks not previously in the variant to the tag. */
  for (seq_iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (seq_iter);
       seq_iter = g_sequence_iter_next (seq_iter)) {
    GVariantDict dict;
    const char *bookmark_id;

    bookmark = g_sequence_get (seq_iter);
    bookmark_id = ephy_bookmark_get_id (bookmark);

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "tag", "s", tag);
    g_variant_dict_insert (&dict, "bookmark-id", "s", bookmark_id);
    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
    not_empty = TRUE;
  }

  if (!not_empty) {
    GVariantDict dict;

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "tag", "s", "");
    g_variant_dict_insert (&dict, "bookmark-id", "s", "");
    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
  }

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_TAGS_ORDER, variant);
}


static void
populate_bookmarks_list_box (EphyBookmarksDialog *self)
{
  g_autoptr (GVariantIter) variant_iter = NULL;
  GVariant *variant;

  g_settings_get (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_BOOKMARKS_ORDER, "aa{sv}", &variant_iter);
  while ((variant = g_variant_iter_next_value (variant_iter))) {
    GVariantDict dict;
    const char *type, *item;
    GtkWidget *row;

    g_variant_dict_init (&dict, variant);
    g_variant_dict_lookup (&dict, "type", "&s", &type);
    g_variant_dict_lookup (&dict, "item", "&s", &item);
    g_variant_dict_clear (&dict);

    if (item == NULL) {
      g_variant_unref (variant);
      continue;
    }

    if (g_strcmp0 (type, "bookmark") == 0) {
      EphyBookmark *bookmark = ephy_bookmarks_manager_get_bookmark_by_id (self->manager, item);

      g_assert (bookmark != NULL);

      row = create_bookmark_row (bookmark, self);
    } else {
      if (g_strcmp0 (item, "") == 0) {
        g_variant_unref (variant);
        continue;
      }

      g_assert (ephy_bookmarks_manager_tag_exists (self->manager, item));

      row = create_tag_row (self, item);
    }

    gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), row);
    g_variant_unref (variant);
  }
}

static void
populate_tag_detail_list_box (EphyBookmarksDialog *self,
                              const char          *tag)
{
  g_autoptr (GVariantIter) variant_iter;
  GVariant *variant;

  g_settings_get (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_TAGS_ORDER, "aa{sv}", &variant_iter);
  while ((variant = g_variant_iter_next_value (variant_iter))) {
    GVariantDict dict;
    const char *variant_tag, *bookmark_id;

    g_variant_dict_init (&dict, variant);
    g_variant_dict_lookup (&dict, "tag", "&s", &variant_tag);
    g_variant_dict_lookup (&dict, "bookmark-id", "&s", &bookmark_id);
    g_variant_dict_clear (&dict);

    if (g_strcmp0 (variant_tag, "") == 0 || variant_tag == NULL) {
      g_variant_unref (variant);
      continue;
    }

    g_assert (ephy_bookmarks_manager_tag_exists (self->manager, variant_tag));

    if (g_strcmp0 (variant_tag, tag) == 0) {
      EphyBookmark *bookmark = ephy_bookmarks_manager_get_bookmark_by_id (self->manager, bookmark_id);
      GtkWidget *row;

      g_assert (bookmark != NULL);

      row = create_bookmark_row (bookmark, self);
      gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), row);
    }

    g_variant_unref (variant);
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

  if (list_box != dest_list_box)
    return;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX (dest_list_box));

  g_object_ref (row);
  gtk_list_box_remove (GTK_LIST_BOX (dest_list_box),
                       GTK_WIDGET (row));
  gtk_list_box_insert (GTK_LIST_BOX (dest_list_box),
                       GTK_WIDGET (row), index);
  g_object_unref (row);

  if (g_strcmp0 (visible_child, "tag_detail") == 0) {
    update_tags_order (self, self->tag_detail_tag);
    g_signal_emit (self->manager, signals[ORDER_UPDATED], 0, self->tag_detail_tag);
  } else {
    update_bookmarks_order (self);
    g_signal_emit (self->manager, signals[ORDER_UPDATED], 0, NULL);
  }
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
  gboolean update_bookmarks = FALSE;

  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));

  /* If the bookmark no longer has 0 tags, we remove it from the tags list box */
  if (g_sequence_get_length (ephy_bookmark_get_tags (bookmark)) == 1) {
    remove_bookmark_row (GTK_LIST_BOX (self->bookmarks_list_box), ephy_bookmark_get_url (bookmark));
    update_bookmarks = TRUE;
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
    update_tags_order (self, tag);
  } else {
    update_tags_order_without_list_box (self, tag);
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
    tag_row = create_tag_row (self, tag);
    set_row_is_editable (tag_row, FALSE);
    gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), tag_row);
    update_bookmarks = TRUE;
  }

  if (update_bookmarks)
    update_bookmarks_order (self);
}

static void
ephy_bookmarks_dialog_bookmark_tag_removed_cb (EphyBookmarksDialog  *self,
                                               EphyBookmark         *bookmark,
                                               const char           *tag,
                                               EphyBookmarksManager *manager)
{
  const char *visible_stack_child;
  gboolean exists;
  gboolean update_bookmarks = FALSE;

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
      update_bookmarks = TRUE;
    }
  }

  /* If we are on the tag detail list box of the tag that was removed, we
   * remove the bookmark from it to reflect the changes. */
  visible_stack_child = gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack));
  if (g_strcmp0 (visible_stack_child, "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    remove_bookmark_row (GTK_LIST_BOX (self->tag_detail_list_box),
                         ephy_bookmark_get_url (bookmark));
    update_tags_order (self, tag);

    /* If we removed the tag's last bookmark, switch back to the tags list. */
    if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag))
      tag_detail_back (self);
  } else {
    update_tags_order_without_list_box (self, tag);
  }

  /* If the tag no longer contains bookmarks, remove it from the tags list */
  if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag)) {
    remove_tag_row (self, tag);
    update_bookmarks = TRUE;
  }

  if (update_bookmarks)
    update_bookmarks_order (self);
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

  g_signal_connect_object (row, "bmks-move-row", G_CALLBACK (row_moved_cb), self, 0);

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

  g_signal_emit (source, signals[MOVE_ROW], 0, self);

  return TRUE;
}

static GtkWidget *
create_tag_row (EphyBookmarksDialog *self,
                const char          *tag)
{
  GtkWidget *row;
  GtkWidget *image;
  GtkWidget *drag_image;
  GtkDragSource *source;
  GtkDropTarget *target;
  gboolean is_editing;

  row = adw_action_row_new ();
  g_object_set_data_full (G_OBJECT (row), "type",
                          g_strdup (EPHY_LIST_BOX_ROW_TYPE_TAG),
                          (GDestroyNotify)g_free);

  if (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0) {
    image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic");
  } else {
    image = gtk_image_new_from_icon_name ("ephy-bookmark-tag-symbolic");
  }
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), tag);
  gtk_widget_set_tooltip_text (row, tag);

  image = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), image);

  drag_image = gtk_image_new_from_icon_name ("list-drag-handle-symbolic");
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), drag_image);

  g_signal_connect_object (row, "activated", G_CALLBACK (on_tag_row_activated), self, 0);
  g_signal_connect_object (row, "bmks-move-row", G_CALLBACK (row_moved_cb), self, 0);

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
  GtkWindow *window;
  EphyEmbed *embed;
  EphyWebView *view;
  const char *address;

  if (ephy_bookmarks_manager_get_bookmark_by_url (manager, ephy_bookmark_get_url (bookmark)))
    return;

  ephy_bookmarks_manager_add_bookmark (manager, bookmark);

  window = gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ()));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  view = ephy_embed_get_web_view (embed);

  address = ephy_web_view_get_address (view);

  if (g_strcmp0 (ephy_bookmark_get_url (bookmark), address) == 0)
    ephy_window_sync_bookmark_state (EPHY_WINDOW (window), EPHY_BOOKMARK_ICON_BOOKMARKED);
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
  gboolean update_bookmarks = FALSE;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  tags = ephy_bookmark_get_tags (bookmark);
  if (g_sequence_is_empty (tags)) {
    row = create_bookmark_row (bookmark, self);
    gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), row);
    update_bookmarks = TRUE;
  } else {
    GSequenceIter *iter;

    for (iter = g_sequence_get_begin_iter (tags);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      const char *tag = g_sequence_get (iter);
      GtkListBoxRow *existing_row;
      int i = 0;
      gboolean exists = FALSE;

      update_tags_order_without_list_box (self, tag);

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
        update_bookmarks = TRUE;
      }
    }
  }

  row = create_bookmark_row (bookmark, self);
  set_row_is_editable (row, FALSE);
  gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), row);

  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "empty-state") == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
    gtk_widget_set_visible (self->search_entry, TRUE);
    gtk_widget_set_visible (self->edit_button, TRUE);
  } else if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0) {
    if (g_sequence_lookup (tags,
                           (gpointer)self->tag_detail_tag,
                           (GCompareDataFunc)ephy_bookmark_tags_compare,
                           NULL)) {
      row = create_bookmark_row (bookmark, self);
      gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), row);
    }
  }

  if (update_bookmarks)
    update_bookmarks_order (self);
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

  remove_bookmark_row (GTK_LIST_BOX (self->bookmarks_list_box),
                       ephy_bookmark_get_url (bookmark));
  remove_bookmark_row (GTK_LIST_BOX (self->tag_detail_list_box),
                       ephy_bookmark_get_url (bookmark));
  remove_bookmark_row (GTK_LIST_BOX (self->searching_bookmarks_list_box),
                       ephy_bookmark_get_url (bookmark));

  if (g_list_model_get_n_items (G_LIST_MODEL (self->manager)) == 0) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");
    gtk_widget_set_visible (self->search_entry, FALSE);
    gtk_widget_set_visible (self->edit_button, FALSE);
    ephy_bookmarks_dialog_set_is_editing (self, FALSE);
  } else if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0 &&
             !ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, self->tag_detail_tag)) {
    /* If we removed the tag's last bookmark, switch back to the tags list. */
    update_tags_order (self, self->tag_detail_tag);
    tag_detail_back (self);
  }

  tags = ephy_bookmarks_manager_get_tags (self->manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);
    /* If the tag no longer contains bookmarks, remove it from the tags list */
    if (!ephy_bookmarks_manager_has_bookmarks_with_tag (self->manager, tag))
      remove_tag_row (self, tag);
  }

  update_bookmarks_order (self);

  if (window == active_window) {
    AdwToast *toast = adw_toast_new (_("Bookmark removed"));

    adw_toast_set_priority (toast, ADW_TOAST_PRIORITY_HIGH);
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
  g_assert (tag != NULL);
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  tag_row = create_tag_row (self, tag);
  gtk_list_box_append (GTK_LIST_BOX (self->bookmarks_list_box), tag_row);
  tag_row = create_tag_row (self, tag);
  set_row_is_editable (tag_row, FALSE);
  gtk_list_box_append (GTK_LIST_BOX (self->searching_bookmarks_list_box), tag_row);

  update_bookmarks_order (self);
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
      update_bookmarks_order (self);
      break;
    }
  }

  i = 0;
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->searching_bookmarks_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    if (g_strcmp0 (title, tag) == 0) {
      gtk_list_box_remove (GTK_LIST_BOX (self->searching_bookmarks_list_box), GTK_WIDGET (row));
      break;
    }
  }

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "tag_detail") == 0 &&
      g_strcmp0 (self->tag_detail_tag, tag) == 0) {
    update_tags_order (self, tag);
    tag_detail_back (self);
  } else {
    update_tags_order_without_list_box (self, tag);
  }
}

static void
ephy_bookmarks_dialog_order_updated_cb (EphyBookmarksDialog  *self,
                                        const char           *view,
                                        EphyBookmarksManager *manager)
{
  if (g_strcmp0 (view, NULL) == 0) {
    gtk_list_box_remove_all (GTK_LIST_BOX (self->bookmarks_list_box));
    populate_bookmarks_list_box (self);
  } else if (g_strcmp0 (self->tag_detail_tag, view) == 0) {
    gtk_list_box_remove_all (GTK_LIST_BOX (self->tag_detail_list_box));
    populate_tag_detail_list_box (self, self->tag_detail_tag);
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
  populate_tag_detail_list_box (self, tag);

  gtk_label_set_label (GTK_LABEL (self->tag_detail_label), tag);

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_widget_set_state_flags (self->search_entry, GTK_STATE_FLAG_NORMAL, TRUE);

  if (self->tag_detail_tag != NULL)
    g_free (self->tag_detail_tag);
  self->tag_detail_tag = g_strdup (tag);

  update_tags_order (self, tag);
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

  if (g_strcmp0 (entry_text, "") != 0 && g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "empty-state") == 0) {
    if ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0)))
      gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");
    else
      gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "searching_bookmarks");
  }

  if ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0))) {
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), idx++)) != NULL) {
      if (gtk_widget_get_mapped (GTK_WIDGET (row)))
        mapped++;
    }
  } else {
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->searching_bookmarks_list_box), idx++)) != NULL) {
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

  signals[MOVE_ROW] =
    g_signal_new ("bmks-move-row",
                  ADW_TYPE_ACTION_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  ADW_TYPE_ACTION_ROW);

  signals[ORDER_UPDATED] = g_signal_lookup ("order-updated", EPHY_TYPE_BOOKMARKS_MANAGER);

  gtk_widget_class_install_action (widget_class, "dialog.tag-detail-back", NULL,
                                   (GtkWidgetActionActivateFunc)tag_detail_back);
}

static void
ephy_bookmarks_dialog_init (EphyBookmarksDialog *self)
{
  GSequence *tags;
  GSequenceIter *iter;
  GSequence *bookmarks;
  g_autoptr (GVariantIter) variant_iter = NULL;
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

  g_settings_get (EPHY_SETTINGS_STATE, EPHY_PREFS_STATE_BOOKMARKS_ORDER, "aa{sv}", &variant_iter);
  if (g_variant_iter_n_children (variant_iter) != 0) {
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
  }

  update_bookmarks_order (self);
  tags = ephy_bookmarks_manager_get_tags (self->manager);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);

    update_tags_order_without_list_box (self, tag);
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
  g_signal_connect_object (self->manager, "order-updated",
                           G_CALLBACK (ephy_bookmarks_dialog_order_updated_cb),
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
