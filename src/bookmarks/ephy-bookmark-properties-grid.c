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

#include "ephy-bookmark-properties-grid.h"

#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-type-builtins.h"
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <string.h>

struct _EphyBookmarkPropertiesGrid {
  GtkGrid                         parent_instance;

  EphyBookmarksManager           *manager;
  EphyBookmark                   *bookmark;
  gboolean                        bookmark_is_modified;
  gboolean                        bookmark_is_removed;

  EphyBookmarkPropertiesGridType  type;
  GtkWidget                      *parent;

  GtkWidget                      *popover_bookmark_label;
  GtkWidget                      *name_entry;
  GtkWidget                      *address_entry;
  GtkWidget                      *popover_tags_label;
  GtkWidget                      *tags_box;
  GtkWidget                      *add_tag_entry;
  GtkWidget                      *add_tag_button;
  GtkWidget                      *remove_bookmark_button;
};

G_DEFINE_TYPE (EphyBookmarkPropertiesGrid, ephy_bookmark_properties_grid, GTK_TYPE_GRID)

enum {
  PROP_0,
  PROP_BOOKMARK,
  PROP_TYPE,
  PROP_PARENT,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static int
flow_box_sort_func (GtkFlowBoxChild *child1, GtkFlowBoxChild *child2)
{
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *label1;
  GtkWidget *label2;
  const char *tag1;
  const char *tag2;

  g_assert (GTK_IS_FLOW_BOX_CHILD (child1));
  g_assert (GTK_IS_FLOW_BOX_CHILD (child2));

  box1 = gtk_bin_get_child (GTK_BIN (child1));
  box2 = gtk_bin_get_child (GTK_BIN (child2));

  label1 = g_object_get_data (G_OBJECT (box1), "label");
  label2 = g_object_get_data (G_OBJECT (box2), "label");

  tag1 = gtk_label_get_text (GTK_LABEL (label1));
  tag2 = gtk_label_get_text (GTK_LABEL (label2));

  return ephy_bookmark_tags_compare (tag1, tag2);
}

static void
ephy_bookmark_properties_grid_tags_box_child_activated_cb (EphyBookmarkPropertiesGrid *self,
                                                           GtkFlowBoxChild            *child,
                                                           GtkFlowBox                 *flow_box)
{
  GtkStyleContext *context;
  GtkWidget *box;
  GtkWidget *label;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (GTK_IS_FLOW_BOX_CHILD (child));
  g_assert (GTK_IS_FLOW_BOX (flow_box));

  box = gtk_bin_get_child (GTK_BIN (child));
  label = g_object_get_data (G_OBJECT (box), "label");

  context = gtk_widget_get_style_context (GTK_WIDGET (child));
  if (gtk_style_context_has_class (context, "bookmark-tag-widget-selected")) {
    ephy_bookmark_remove_tag (self->bookmark,
                              gtk_label_get_text (GTK_LABEL (label)));
    gtk_style_context_remove_class (context, "bookmark-tag-widget-selected");
  } else {
    ephy_bookmark_add_tag (self->bookmark,
                           gtk_label_get_text (GTK_LABEL (label)));
    gtk_style_context_add_class (context, "bookmark-tag-widget-selected");
  }
}

static void
ephy_bookmark_properties_grid_tag_widget_button_clicked_cb (EphyBookmarkPropertiesGrid *self,
                                                            GtkButton                  *button)
{
  GtkWidget *box;
  GtkWidget *flow_box_child;
  GtkLabel *label;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (GTK_IS_BUTTON (button));

  box = gtk_widget_get_parent (GTK_WIDGET (button));
  g_assert (GTK_IS_BOX (box));
  label = g_object_get_data (G_OBJECT (box), "label");

  ephy_bookmarks_manager_delete_tag (self->manager, gtk_label_get_text (label));

  flow_box_child = gtk_widget_get_parent (box);
  gtk_widget_destroy (flow_box_child);
}

static GtkWidget *
ephy_bookmark_properties_grid_create_tag_widget (EphyBookmarkPropertiesGrid *self,
                                                 const char                 *tag,
                                                 gboolean                    selected)
{
  GtkWidget *widget;
  GtkWidget *box;
  GtkWidget *label;
  GtkStyleContext *context;
  gboolean default_tag;
  const char *label_text;

  default_tag = (g_strcmp0 (tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0);

  widget = gtk_flow_box_child_new ();
  gtk_widget_set_can_focus (widget, FALSE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  if (default_tag) {
    GtkWidget *image;

    image = gtk_image_new_from_icon_name ("emblem-favorite-symbolic",
                                          GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_margin_bottom (image, 8);
    gtk_widget_set_margin_top (image, 8);
    gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  }

  label_text = default_tag ? _(EPHY_BOOKMARKS_FAVORITES_TAG) : tag;
  label = gtk_label_new (label_text);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  if (!default_tag) {
    GtkWidget *button;

    button = gtk_button_new ();
    gtk_button_set_image (GTK_BUTTON (button),
                          gtk_image_new_from_icon_name ("window-close-symbolic",
                                                        GTK_ICON_SIZE_MENU));
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus (button, FALSE);
    gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
    g_signal_connect_object (button, "clicked",
                             G_CALLBACK (ephy_bookmark_properties_grid_tag_widget_button_clicked_cb),
                             self,
                             G_CONNECT_SWAPPED);
  }

  g_object_set_data (G_OBJECT (box), "label", label);

  gtk_container_add (GTK_CONTAINER (widget), box);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, "bookmark-tag-widget");
  if (selected)
    gtk_style_context_add_class (context, "bookmark-tag-widget-selected");

  gtk_widget_show_all (widget);

  return widget;
}

static void
ephy_bookmarks_properties_grid_actions_add_tag (GSimpleAction *action,
                                                GVariant      *value,
                                                gpointer       user_data)
{
  EphyBookmarkPropertiesGrid *self = user_data;
  GtkEntryBuffer *buffer;
  GtkWidget *widget;
  const char *text;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));

  buffer = gtk_entry_get_buffer (GTK_ENTRY (self->add_tag_entry));
  text = gtk_entry_buffer_get_text (buffer);

  /* Create new tag with the given title */
  ephy_bookmarks_manager_create_tag (self->manager, text);

  /* Add tag to the bookmark's list of tags. */
  ephy_bookmark_add_tag (self->bookmark, text);

  /* Create a new widget for the new tag */
  widget = ephy_bookmark_properties_grid_create_tag_widget (self, text, TRUE);
  gtk_flow_box_insert (GTK_FLOW_BOX (self->tags_box), widget, -1);

  /* Empty entry and disable button's action until new text is inserted */
  gtk_entry_set_text (GTK_ENTRY (self->add_tag_entry), "");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  gtk_widget_grab_focus (GTK_WIDGET (self->add_tag_entry));
}

static void
ephy_bookmarks_properties_grid_actions_remove_bookmark (GSimpleAction *action,
                                                        GVariant      *value,
                                                        gpointer       user_data)
{
  EphyBookmarkPropertiesGrid *self = user_data;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));

  self->bookmark_is_removed = TRUE;
  ephy_bookmarks_manager_remove_bookmark (self->manager, self->bookmark);

  if (self->type == EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_DIALOG)
    gtk_widget_destroy (self->parent);
}

static void
ephy_bookmark_properties_grid_buffer_text_changed_cb (EphyBookmarkPropertiesGrid *self,
                                                      GParamSpec                 *pspec,
                                                      GtkEntryBuffer             *buffer)
{
  GActionGroup *group;
  GAction *action;
  const char *text;

  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (GTK_IS_ENTRY_BUFFER (buffer));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "grid");
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "add-tag");
  text = gtk_entry_buffer_get_text (buffer);
  if (ephy_bookmarks_manager_tag_exists (self->manager, text))
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  else
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static void
ephy_bookmark_properties_grid_bookmark_title_changed_cb (EphyBookmarkPropertiesGrid *self,
                                                         EphyBookmark               *bookmark,
                                                         EphyBookmarksManager       *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_grid_bookmark_url_changed_cb (EphyBookmarkPropertiesGrid *self,
                                                       EphyBookmark               *bookmark,
                                                       EphyBookmarksManager       *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_grid_bookmark_tag_added_cb (EphyBookmarkPropertiesGrid *self,
                                                     EphyBookmark               *bookmark,
                                                     const char                 *tag,
                                                     EphyBookmarksManager       *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_grid_bookmark_tag_removed_cb (EphyBookmarkPropertiesGrid *self,
                                                       EphyBookmark               *bookmark,
                                                       const char                 *tag,
                                                       EphyBookmarksManager       *manager)
{
  g_assert (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self));
  g_assert (EPHY_IS_BOOKMARK (bookmark));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (manager));
  g_assert (tag);

  self->bookmark_is_modified = TRUE;
}

static void
ephy_bookmark_properties_grid_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  EphyBookmarkPropertiesGrid *self = EPHY_BOOKMARK_PROPERTIES_GRID (object);

  switch (prop_id) {
    case PROP_BOOKMARK:
      self->bookmark = g_value_dup_object (value);
      break;
    case PROP_TYPE:
      self->type = g_value_get_enum (value);
      break;
    case PROP_PARENT:
      self->parent = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_properties_grid_constructed (GObject *object)
{
  EphyBookmarkPropertiesGrid *self = EPHY_BOOKMARK_PROPERTIES_GRID (object);
  GSequence *tags;
  GSequence *bookmark_tags;
  GSequenceIter *iter;
  const char *address;

  G_OBJECT_CLASS (ephy_bookmark_properties_grid_parent_class)->constructed (object);

  /* Set appearance based on type */
  if (self->type == EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_DIALOG) {
    gtk_container_remove (GTK_CONTAINER (self), self->popover_bookmark_label);
    gtk_container_remove (GTK_CONTAINER (self), self->popover_tags_label);
  } else if (self->type == EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_POPOVER) {
    gtk_grid_remove_column (GTK_GRID (self), 0);
    gtk_container_remove (GTK_CONTAINER (self), self->address_entry);
  }

  /* Set text for name entry */
  gtk_entry_set_text (GTK_ENTRY (self->name_entry),
                      ephy_bookmark_get_title (self->bookmark));

  g_object_bind_property (GTK_ENTRY (self->name_entry), "text",
                          self->bookmark, "title",
                          G_BINDING_DEFAULT);

  /* Set text for address entry */
  if (self->type == EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_DIALOG) {
    char *decoded_address;
    address = ephy_bookmark_get_url (self->bookmark);
    decoded_address = ephy_uri_decode (address);
    gtk_entry_set_text (GTK_ENTRY (self->address_entry), decoded_address);
    g_free (decoded_address);

    g_object_bind_property (GTK_ENTRY (self->address_entry), "text",
                            self->bookmark, "bmkUri",
                            G_BINDING_DEFAULT);
  }

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

    widget = ephy_bookmark_properties_grid_create_tag_widget (self, tag, selected);
    gtk_flow_box_insert (GTK_FLOW_BOX (self->tags_box), widget, -1);
  }

  g_signal_connect_object (self->tags_box, "child-activated",
                           G_CALLBACK (ephy_bookmark_properties_grid_tags_box_child_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_show_all (self->tags_box);
}

static void
ephy_bookmark_properties_grid_finalize (GObject *object)
{
  EphyBookmarkPropertiesGrid *self = EPHY_BOOKMARK_PROPERTIES_GRID (object);

  if (self->bookmark_is_modified && !self->bookmark_is_removed)
    g_signal_emit_by_name (self->manager, "synchronizable-modified", self->bookmark, FALSE);

  ephy_bookmarks_manager_save_to_file_async (self->manager, NULL,
                                             ephy_bookmarks_manager_save_to_file_warn_on_error_cb,
                                             NULL);

  G_OBJECT_CLASS (ephy_bookmark_properties_grid_parent_class)->finalize (object);
}

static void
ephy_bookmark_properties_grid_class_init (EphyBookmarkPropertiesGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_bookmark_properties_grid_set_property;
  object_class->constructed = ephy_bookmark_properties_grid_constructed;
  object_class->finalize = ephy_bookmark_properties_grid_finalize;

  obj_properties[PROP_BOOKMARK] =
    g_param_spec_object ("bookmark",
                         "An EphyBookmark object",
                         "The EphyBookmark whose properties are being displayed",
                         EPHY_TYPE_BOOKMARK,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TYPE] =
    g_param_spec_enum ("type",
                       "An EphyBookmarkPropertiesGrid object",
                       "The type of widget the grid will be used for",
                       EPHY_TYPE_BOOKMARK_PROPERTIES_GRID_TYPE,
                       EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_DIALOG,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_PARENT] =
    g_param_spec_object ("parent",
                         "A GtkWidget",
                         "The dialog or popover that needs to be destroyed when the bookmark is removed",
                         GTK_TYPE_WIDGET,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmark-properties-grid.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, popover_bookmark_label);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, name_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, address_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, popover_tags_label);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, tags_box);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, add_tag_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, add_tag_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkPropertiesGrid, remove_bookmark_button);
}

static const GActionEntry entries[] = {
  { "add-tag", ephy_bookmarks_properties_grid_actions_add_tag },
  { "remove-bookmark", ephy_bookmarks_properties_grid_actions_remove_bookmark }
};

static void
ephy_bookmark_properties_grid_init (EphyBookmarkPropertiesGrid *self)
{
  GSimpleActionGroup *group;
  GAction *action;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_signal_connect_object (self->manager,
                           "bookmark-title-changed",
                           G_CALLBACK (ephy_bookmark_properties_grid_bookmark_title_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "bookmark-url-changed",
                           G_CALLBACK (ephy_bookmark_properties_grid_bookmark_url_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "bookmark-tag-added",
                           G_CALLBACK (ephy_bookmark_properties_grid_bookmark_tag_added_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "bookmark-tag-removed",
                           G_CALLBACK (ephy_bookmark_properties_grid_bookmark_tag_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_flow_box_set_sort_func (GTK_FLOW_BOX (self->tags_box),
                              (GtkFlowBoxSortFunc)flow_box_sort_func,
                              NULL, NULL);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "grid",
                                  G_ACTION_GROUP (group));

  /* Disable the "add-tag" action until text is inserted in the corresponding
   * entry */
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "add-tag");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  g_object_unref (group);

  g_signal_connect_object (gtk_entry_get_buffer (GTK_ENTRY (self->add_tag_entry)),
                           "notify::text",
                           G_CALLBACK (ephy_bookmark_properties_grid_buffer_text_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
ephy_bookmark_properties_grid_new (EphyBookmark                   *bookmark,
                                   EphyBookmarkPropertiesGridType  type,
                                   GtkWidget                      *parent)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (bookmark), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);

  return g_object_new (EPHY_TYPE_BOOKMARK_PROPERTIES_GRID,
                       "bookmark", bookmark,
                       "type", type,
                       "parent", parent,
                       NULL);
}

GtkWidget *
ephy_bookmark_properties_grid_get_add_tag_button (EphyBookmarkPropertiesGrid *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK_PROPERTIES_GRID (self), NULL);

  return self->add_tag_button;
}
