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
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include <adwaita.h>
#include <glib/gi18n.h>

struct _EphyBookmarksDialog {
  AdwBin parent_instance;

  GtkWidget *toplevel_stack;
  GtkWidget *bookmarks_list_box;
  GtkWidget *tags_list_box;
  GtkWidget *tag_detail_list_box;
  GtkWidget *tag_detail_label;
  GtkWidget *search_entry;
  char *tag_detail_tag;

  EphyBookmarksManager *manager;
};

G_DEFINE_FINAL_TYPE (EphyBookmarksDialog, ephy_bookmarks_dialog, ADW_TYPE_BIN)

#define EPHY_LIST_BOX_ROW_TYPE_BOOKMARK "bookmark"
#define EPHY_LIST_BOX_ROW_TYPE_TAG "tag"

static GtkWidget * create_bookmark_row (gpointer item, gpointer user_data);
static GtkWidget *create_tag_row (const char *tag);

static void
tag_detail_back (EphyBookmarksDialog *self)
{
  GtkListBoxRow *row;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack),
                                    "default");

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tag_detail_list_box), 0)))
    gtk_list_box_remove (GTK_LIST_BOX (self->tag_detail_list_box), GTK_WIDGET (row));
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
    gtk_list_box_append (GTK_LIST_BOX (self->tag_detail_list_box), bookmark_row);
  }

  exists = FALSE;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tags_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    const char *type = g_object_get_data (G_OBJECT (row), "type");

    if (g_strcmp0 (title, tag) == 0 &&
        g_strcmp0 (type, EPHY_LIST_BOX_ROW_TYPE_TAG) == 0) {
      exists = TRUE;
      break;
    }
  }

  if (!exists) {
    GtkWidget *tag_row = create_tag_row (tag);
    gtk_list_box_append (GTK_LIST_BOX (self->tags_list_box), tag_row);
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
      gtk_list_box_append (GTK_LIST_BOX (self->tags_list_box), row);
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
      const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

      if (g_strcmp0 (title, tag) == 0)
        gtk_list_box_remove (GTK_LIST_BOX (self->tags_list_box), GTK_WIDGET (row));
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

  return row;
}

static GtkWidget *
create_tag_row (const char *tag)
{
  GtkWidget *row;
  GtkWidget *image;

  row = adw_action_row_new ();
  g_object_set_data_full (G_OBJECT (row), "type",
                          g_strdup (EPHY_LIST_BOX_ROW_TYPE_TAG),
                          (GDestroyNotify)g_free);
  g_object_set (G_OBJECT (row), "height-request", 40, NULL);

  if (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0) {
    image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic");
  } else {
    image = gtk_image_new_from_icon_name ("ephy-bookmark-tag-symbolic");
  }
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), tag);

  return row;
}

static void
ephy_bookmarks_dialog_bookmark_added_cb (EphyBookmarksDialog  *self,
                                         EphyBookmark         *bookmark,
                                         EphyBookmarksManager *manager)
{
  GtkWidget *row;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  if (g_sequence_is_empty (ephy_bookmark_get_tags (bookmark))) {
    row = create_bookmark_row (bookmark, self);
    gtk_list_box_append (GTK_LIST_BOX (self->tags_list_box), row);
  }

  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->toplevel_stack)), "empty-state") == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "default");
}

static void
ephy_bookmarks_dialog_bookmark_removed_cb (EphyBookmarksDialog  *self,
                                           EphyBookmark         *bookmark,
                                           EphyBookmarksManager *manager)
{
  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
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
ephy_bookmarks_dialog_tag_created_cb (EphyBookmarksDialog  *self,
                                      const char           *tag,
                                      EphyBookmarksManager *manager)
{
  GtkWidget *tag_row;

  g_assert (EPHY_IS_BOOKMARKS_DIALOG (self));
  g_assert (tag != NULL);
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  tag_row = create_tag_row (tag);
  gtk_list_box_append (GTK_LIST_BOX (self->tags_list_box), tag_row);
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

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->tags_list_box), i++))) {
    const char *title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
    if (g_strcmp0 (title, tag) == 0) {
      gtk_list_box_remove (GTK_LIST_BOX (self->tags_list_box), GTK_WIDGET (row));
      break;
    }
  }

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

  gtk_label_set_label (GTK_LABEL (self->tag_detail_label), tag);

  gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "tag_detail");

  if (self->tag_detail_tag != NULL)
    g_free (self->tag_detail_tag);
  self->tag_detail_tag = g_strdup (tag);

  g_sequence_free (bookmarks);
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

  row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (list), y);

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

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
on_search_entry_changed (GtkSearchEntry *entry,
                         gpointer        user_data)
{
  EphyBookmarksDialog *self = EPHY_BOOKMARKS_DIALOG (user_data);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->tags_list_box));
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
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, toplevel_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, bookmarks_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, tags_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, tag_detail_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, tag_detail_label);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksDialog, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);

  gtk_widget_class_install_action (widget_class, "dialog.tag-detail-back", NULL,
                                   (GtkWidgetActionActivateFunc)tag_detail_back);
}

static void
ephy_bookmarks_dialog_init (EphyBookmarksDialog *self)
{
  GSequence *tags;
  GSequenceIter *iter;
  g_autoptr (GSequence) bookmarks = NULL;
  GtkGesture *gesture;
  GtkFilter *filter;
  g_autoptr (GtkFilterListModel) filter_model = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  filter = GTK_FILTER (gtk_string_filter_new (gtk_property_expression_new (EPHY_TYPE_BOOKMARK, NULL, "title")));
  g_object_bind_property (self->search_entry, "text", filter, "search", 0);
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (self->manager), filter);

  gtk_list_box_bind_model (GTK_LIST_BOX (self->bookmarks_list_box),
                           G_LIST_MODEL (filter_model),
                           create_bookmark_row,
                           self, NULL);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->manager)) == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (self->toplevel_stack), "empty-state");

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->tags_list_box),
                              (GtkListBoxSortFunc)tags_list_box_sort_func,
                              NULL, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->tags_list_box),
                                (GtkListBoxFilterFunc)tags_list_box_filter_func,
                                self, NULL);
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
      gtk_list_box_append (GTK_LIST_BOX (self->tags_list_box), tag_row);
    }
  }

  bookmarks = ephy_bookmarks_manager_get_bookmarks_with_tag (self->manager, NULL);
  for (iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark = g_sequence_get (iter);
    GtkWidget *bookmark_row;

    bookmark_row = create_bookmark_row (bookmark, self);
    gtk_list_box_append (GTK_LIST_BOX (self->tags_list_box), bookmark_row);
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

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (row_clicked_cb), self);
  gtk_widget_add_controller (self->bookmarks_list_box, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (row_clicked_cb), self);
  gtk_widget_add_controller (self->tags_list_box, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (row_clicked_cb), self);
  gtk_widget_add_controller (self->tag_detail_list_box, GTK_EVENT_CONTROLLER (gesture));
}

GtkWidget *
ephy_bookmarks_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_DIALOG,
                       NULL);
}
