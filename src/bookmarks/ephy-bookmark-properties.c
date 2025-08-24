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

#include "ephy-bookmark-properties.h"

#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-shell.h"
#include "ephy-type-builtins.h"
#include "ephy-uri-helpers.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <string.h>

struct _EphyBookmarkProperties {
  AdwDialog parent_instance;

  EphyBookmarksManager *manager;
  EphyBookmark *bookmark;
  gboolean bookmark_is_modified;
  gboolean bookmark_is_removed;
  gboolean bookmark_is_new;

  GtkWidget *header_bar;
  GtkWidget *tag_header_bar;
  GtkWidget *navigation_view;
  GtkWidget *name_row;
  GtkWidget *address_row;
  GtkWidget *cancel_button;
  GtkWidget *add_button;
  GtkWidget *remove_button;
  GtkWidget *add_tag_row;
  GtkWidget *tag_list;
};

G_DEFINE_FINAL_TYPE (EphyBookmarkProperties, ephy_bookmark_properties, ADW_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_BOOKMARK,
  PROP_BOOKMARK_IS_NEW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static int
tag_sort_func (GtkListBoxRow *child1,
               GtkListBoxRow *child2,
               gpointer       user_data)
{
  const char *tag1;
  const char *tag2;

  g_assert (GTK_IS_LIST_BOX_ROW (child1));
  g_assert (GTK_IS_LIST_BOX_ROW (child2));

  tag1 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (child1));
  tag2 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (child2));

  return ephy_bookmark_tags_compare (tag1, tag2);
}

static void
ephy_bookmark_properties_tag_widget_button_clicked_cb (EphyBookmarkProperties *self,
                                                       GtkButton              *button)
{
  GtkWidget *row;
  const char *label;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));
  g_assert (GTK_IS_BUTTON (button));

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_ACTION_ROW);
  g_assert (ADW_IS_ACTION_ROW (row));
  label = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

  ephy_bookmarks_manager_delete_tag (self->manager, label);

  gtk_list_box_remove (GTK_LIST_BOX (self->tag_list), row);
}

static void
on_check_button_toggled (GtkWidget *button,
                         gpointer   user_data)
{
  EphyBookmarkProperties *self = EPHY_BOOKMARK_PROPERTIES (user_data);
  const char *label;
  GtkWidget *row;

  g_assert (GTK_IS_CHECK_BUTTON (button));
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_ACTION_ROW);
  label = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (button))) {
    ephy_bookmark_remove_tag (self->bookmark, label);
  } else {
    ephy_bookmark_add_tag (self->bookmark, label);
  }
}

static GtkWidget *
ephy_bookmark_properties_create_tag_widget (EphyBookmarkProperties *self,
                                            const char             *tag,
                                            gboolean                selected)
{
  GtkWidget *widget;
  GtkWidget *check_button;
  gboolean default_tag;
  const char *label_text;

  default_tag = (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0);

  widget = adw_action_row_new ();

  if (default_tag) {
    GtkWidget *image;

    image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic");
    adw_action_row_add_prefix (ADW_ACTION_ROW (widget), image);
  }

  check_button = gtk_check_button_new ();
  gtk_widget_set_valign (check_button, GTK_ALIGN_CENTER);
  gtk_accessible_update_property (GTK_ACCESSIBLE (check_button), GTK_ACCESSIBLE_PROPERTY_LABEL, _("Select current tag"), -1);
  gtk_widget_add_css_class (check_button, "selection-mode");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), selected);
  g_signal_connect_object (G_OBJECT (check_button), "toggled", G_CALLBACK (on_check_button_toggled), self, 0);
  adw_action_row_add_prefix (ADW_ACTION_ROW (widget), check_button);

  label_text = default_tag ? EPHY_BOOKMARKS_FAVORITES_TAG : tag;

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (widget), label_text);

  if (!default_tag) {
    GtkWidget *button;

    button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
    gtk_accessible_update_property (GTK_ACCESSIBLE (button), GTK_ACCESSIBLE_PROPERTY_LABEL, _("Remove current tag"), -1);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "flat");
    adw_action_row_add_suffix (ADW_ACTION_ROW (widget), button);
    g_signal_connect_object (button, "clicked",
                             G_CALLBACK (ephy_bookmark_properties_tag_widget_button_clicked_cb),
                             self,
                             G_CONNECT_SWAPPED);
  }


  return widget;
}

static void
ephy_bookmark_properties_actions_add_tag (EphyBookmarkProperties *self)
{
  GtkWidget *widget;
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (self->add_tag_row));

  /* Create new tag with the given title if this bookmark isn't new. */
  /* If the bookmark is new, the tag will be added to the manager if the
   * bookmark is also added. */
  if (!self->bookmark_is_new)
    ephy_bookmarks_manager_create_tag (self->manager, text);

  /* Add tag to the bookmark's list of tags. */
  ephy_bookmark_add_tag (self->bookmark, text);

  /* Create a new widget for the new tag */
  widget = ephy_bookmark_properties_create_tag_widget (self, text, TRUE);
  gtk_list_box_insert (GTK_LIST_BOX (self->tag_list), widget, -1);

  /* Empty entry and disable button's action until new text is inserted */
  gtk_editable_set_text (GTK_EDITABLE (self->add_tag_row), "");
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "bookmark-properties.add-tag", FALSE);

  gtk_widget_grab_focus (GTK_WIDGET (self->add_tag_row));
}

static void
ephy_bookmark_properties_actions_remove_bookmark (EphyBookmarkProperties *self)
{
  AdwDialog *dialog = ADW_DIALOG (gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_DIALOG));

  self->bookmark_is_removed = TRUE;
  ephy_bookmarks_manager_remove_bookmark (self->manager, self->bookmark);

  adw_dialog_close (dialog);
}

static void
ephy_bookmark_properties_buffer_text_changed_cb (EphyBookmarkProperties *self,
                                                 GParamSpec             *pspec,
                                                 GtkEntryBuffer         *buffer)
{
  const char *text;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));

  text = gtk_editable_get_text (GTK_EDITABLE (self->add_tag_row));
  if (ephy_bookmarks_manager_tag_exists (self->manager, text) || g_strcmp0 (text, "") == 0)
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "bookmark-properties.add-tag", FALSE);
  else
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "bookmark-properties.add-tag", TRUE);
}

static void
ephy_bookmark_properties_bookmark_title_changed_cb (EphyBookmarkProperties *self,
                                                    EphyBookmark           *bookmark,
                                                    EphyBookmarksManager   *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_bookmark_url_changed_cb (EphyBookmarkProperties *self,
                                                  EphyBookmark           *bookmark,
                                                  EphyBookmarksManager   *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_bookmark_tag_added_cb (EphyBookmarkProperties *self,
                                                EphyBookmark           *bookmark,
                                                const char             *tag,
                                                EphyBookmarksManager   *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_bookmark_tag_removed_cb (EphyBookmarkProperties *self,
                                                  EphyBookmark           *bookmark,
                                                  const char             *tag,
                                                  EphyBookmarksManager   *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));
  g_assert (tag);

  self->bookmark_is_modified = TRUE;
}

static void
update_for_new_bookmark (EphyBookmarkProperties *self)
{
  AdwNavigationPage *default_page;

  default_page = adw_navigation_view_find_page (ADW_NAVIGATION_VIEW (self->navigation_view), "default");
  adw_navigation_page_set_title (default_page, _("Add Bookmark"));
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->header_bar), FALSE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->tag_header_bar), FALSE);
  gtk_widget_set_visible (gtk_widget_get_parent (self->remove_button), FALSE);
  gtk_widget_set_visible (self->cancel_button, TRUE);
  gtk_widget_set_visible (self->add_button, TRUE);
}

static void
on_add_button_clicked (GtkButton              *button,
                       EphyBookmarkProperties *self)
{
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (ephy_bookmark_get_tags (self->bookmark));
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    const char *tag = g_sequence_get (iter);
    GSequenceIter *manager_iter;

    manager_iter = g_sequence_lookup (ephy_bookmarks_manager_get_tags (self->manager),
                                      (gpointer)tag,
                                      (GCompareDataFunc)ephy_bookmark_tags_compare,
                                      NULL);
    if (!manager_iter)
      ephy_bookmarks_manager_create_tag (self->manager, tag);
  }

  ephy_bookmarks_manager_add_bookmark (self->manager, self->bookmark);

  adw_dialog_close (ADW_DIALOG (self));
}

static void
on_tags_activated (AdwActionRow *row,
                   gpointer      user_data)
{
  EphyBookmarkProperties *self = EPHY_BOOKMARK_PROPERTIES (user_data);

  adw_navigation_view_push_by_tag (ADW_NAVIGATION_VIEW (self->navigation_view), "tags");
}

static void
on_add_tag_entry_activated (AdwEntryRow *row,
                            gpointer     user_data)
{
  EphyBookmarkProperties *self = EPHY_BOOKMARK_PROPERTIES (user_data);
  const char *text = gtk_editable_get_text (GTK_EDITABLE (row));

  if (!ephy_bookmarks_manager_tag_exists (self->manager, text))
    ephy_bookmark_properties_actions_add_tag (self);
}

static void
ephy_bookmark_properties_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  EphyBookmarkProperties *self = EPHY_BOOKMARK_PROPERTIES (object);

  switch (prop_id) {
    case PROP_BOOKMARK:
      self->bookmark = g_value_dup_object (value);
      break;
    case PROP_BOOKMARK_IS_NEW:
      self->bookmark_is_new = g_value_get_boolean (value);
      if (self->bookmark_is_new)
        update_for_new_bookmark (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_properties_constructed (GObject *object)
{
  EphyBookmarkProperties *self = EPHY_BOOKMARK_PROPERTIES (object);
  GSequence *tags;
  GSequence *bookmark_tags;
  GSequenceIter *iter;
  const char *address;
  g_autofree char *decoded_address = NULL;

  G_OBJECT_CLASS (ephy_bookmark_properties_parent_class)->constructed (object);

  /* Set text for name entry */
  gtk_editable_set_text (GTK_EDITABLE (self->name_row),
                         ephy_bookmark_get_title (self->bookmark));

  g_object_bind_property (GTK_EDITABLE (self->name_row), "text",
                          self->bookmark, "title",
                          G_BINDING_DEFAULT);

  /* Set text for address entry */
  address = ephy_bookmark_get_url (self->bookmark);
  decoded_address = ephy_uri_decode (address);
  gtk_editable_set_text (GTK_EDITABLE (self->address_row), decoded_address);

  g_object_bind_property (GTK_EDITABLE (self->address_row), "text",
                          self->bookmark, "bmkUri",
                          G_BINDING_DEFAULT);

  /* Create tag widgets */
  tags = ephy_bookmarks_manager_get_tags (self->manager);
  bookmark_tags = ephy_bookmark_get_tags (self->bookmark);
  for (iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    GtkWidget *widget;
    gboolean selected = FALSE;
    const char *tag = g_sequence_get (iter);

    if (g_sequence_lookup (bookmark_tags,
                           (gpointer)tag,
                           (GCompareDataFunc)ephy_bookmark_tags_compare,
                           NULL))
      selected = TRUE;

    widget = ephy_bookmark_properties_create_tag_widget (self, tag, selected);
    gtk_list_box_insert (GTK_LIST_BOX (self->tag_list), widget, -1);
  }

  adw_dialog_set_focus (ADW_DIALOG (self), self->name_row);
}

static void
ephy_bookmark_properties_finalize (GObject *object)
{
  EphyBookmarkProperties *self = EPHY_BOOKMARK_PROPERTIES (object);

  if (self->bookmark_is_modified && !self->bookmark_is_removed)
    g_signal_emit_by_name (self->manager, "synchronizable-modified", self->bookmark, FALSE);

  ephy_bookmarks_manager_save (self->manager, FALSE, FALSE,
                               ephy_bookmarks_manager_save_warn_on_error_cancellable (self->manager),
                               ephy_bookmarks_manager_save_warn_on_error_cb,
                               NULL);

  g_object_unref (self->bookmark);

  G_OBJECT_CLASS (ephy_bookmark_properties_parent_class)->finalize (object);
}

static void
ephy_bookmark_properties_class_init (EphyBookmarkPropertiesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_bookmark_properties_set_property;
  object_class->constructed = ephy_bookmark_properties_constructed;
  object_class->finalize = ephy_bookmark_properties_finalize;

  obj_properties[PROP_BOOKMARK] =
    g_param_spec_object ("bookmark",
                         NULL, NULL,
                         EPHY_TYPE_BOOKMARK,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_BOOKMARK_IS_NEW] =
    g_param_spec_boolean ("bookmark-is-new",
                          NULL, NULL, FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmark-properties.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, name_row);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, address_row);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, add_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, remove_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, add_tag_row);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, tag_list);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, header_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, tag_header_bar);

  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_tags_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_add_tag_entry_activated);

  gtk_widget_class_install_action (widget_class, "bookmark-properties.add-tag",
                                   NULL, (GtkWidgetActionActivateFunc)ephy_bookmark_properties_actions_add_tag);
  gtk_widget_class_install_action (widget_class, "bookmark-properties.remove-bookmark",
                                   NULL, (GtkWidgetActionActivateFunc)ephy_bookmark_properties_actions_remove_bookmark);
}

static void
ephy_bookmark_properties_init (EphyBookmarkProperties *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_signal_connect_object (self->manager,
                           "bookmark-title-changed",
                           G_CALLBACK (ephy_bookmark_properties_bookmark_title_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "bookmark-url-changed",
                           G_CALLBACK (ephy_bookmark_properties_bookmark_url_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "bookmark-tag-added",
                           G_CALLBACK (ephy_bookmark_properties_bookmark_tag_added_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "bookmark-tag-removed",
                           G_CALLBACK (ephy_bookmark_properties_bookmark_tag_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->tag_list), tag_sort_func, NULL, NULL);

  /* Disable the "add-tag" action until text is inserted in the corresponding
   * entry */
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "bookmark-properties.add-tag", FALSE);

  g_signal_connect_object (self->add_tag_row,
                           "notify::text-length",
                           G_CALLBACK (ephy_bookmark_properties_buffer_text_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
ephy_bookmark_properties_new (EphyBookmark *bookmark,
                              gboolean      bookmark_is_new)
{
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  return g_object_new (EPHY_TYPE_BOOKMARK_PROPERTIES,
                       "bookmark", bookmark,
                       "bookmark-is-new", bookmark_is_new,
                       NULL);
}

GtkWidget *
ephy_bookmark_properties_get_add_tag_button (EphyBookmarkProperties *self)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES (self));

  return self->add_tag_row;
}

GtkWidget *
ephy_bookmark_properties_new_for_window (EphyWindow *window)
{
  g_autoptr (EphyBookmark) bookmark = NULL;
  EphyBookmarksManager *manager;
  EphyEmbed *embed;
  const char *address;
  gboolean bookmark_is_new = FALSE;

  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, address);
  if (!bookmark) {
    g_autofree char *id = NULL;
    g_autoptr (GSequence) tags = NULL;

    bookmark_is_new = TRUE;

    id = ephy_bookmark_generate_random_id ();
    tags = g_sequence_new (g_free);

    bookmark = ephy_bookmark_new (address,
                                  ephy_embed_get_title (embed),
                                  g_steal_pointer (&tags),
                                  id);
  }

  return ephy_bookmark_properties_new (g_steal_pointer (&bookmark), bookmark_is_new);
}

GtkWidget *
ephy_bookmark_properties_new_for_link (EphyWindow *window,
                                       const char *link)
{
  g_autoptr (EphyBookmark) bookmark = NULL;
  EphyBookmarksManager *manager;
  gboolean bookmark_is_new = FALSE;

  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  bookmark = ephy_bookmarks_manager_get_bookmark_by_url (manager, link);
  if (!bookmark) {
    g_autofree char *id = NULL;
    g_autoptr (GSequence) tags = NULL;

    bookmark_is_new = TRUE;

    id = ephy_bookmark_generate_random_id ();
    tags = g_sequence_new (g_free);

    bookmark = ephy_bookmark_new (link,
                                  link,
                                  g_steal_pointer (&tags),
                                  id);
  }

  return ephy_bookmark_properties_new (g_steal_pointer (&bookmark), bookmark_is_new);
}
