/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Epiphany Contributors
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

#include "ephy-bookmark-tag-row.h"

#include "ephy-bookmarks-manager.h"

struct _EphyBookmarkTagRow {
  AdwActionRow parent_instance;

  const char *tag;

  GtkWidget *tag_image;
  GtkWidget *drag_handle;
  GtkWidget *move_menu_button;
};

G_DEFINE_FINAL_TYPE (EphyBookmarkTagRow, ephy_bookmark_tag_row, ADW_TYPE_ACTION_ROW)

typedef enum {
  PROP_TAG = 1,
} EphyBookmarkTagRowProps;

static GParamSpec *obj_properties[PROP_TAG + 1];

enum {
  MOVE_ROW,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
on_move_up (GtkWidget  *self,
            const char *action_name,
            GVariant   *parameter)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (self));
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) - 1;
  GtkListBoxRow *prev_row = gtk_list_box_get_row_at_index (list_box, index);

  if (!prev_row)
    return;

  g_object_set_data (G_OBJECT (self), "list-box", list_box);
  g_signal_emit (ADW_ACTION_ROW (self), signals[MOVE_ROW], 0, ADW_ACTION_ROW (prev_row));
}

static void
on_move_down (GtkWidget  *self,
              const char *action_name,
              GVariant   *parameter)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (self));
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, index);

  if (!next_row)
    return;

  g_object_set_data (G_OBJECT (self), "list-box", list_box);
  g_signal_emit (ADW_ACTION_ROW (self), signals[MOVE_ROW], 0, ADW_ACTION_ROW (next_row));
}

static GdkContentProvider *
on_drag_prepare (AdwActionRow *self,
                 double        x,
                 double        y)
{
  return gdk_content_provider_new_typed (ADW_TYPE_ACTION_ROW, self);
}

static void
on_drag_begin (EphyBookmarkTagRow *self,
               GdkDrag            *drag)
{
  GtkWidget *drag_list;
  GtkWidget *drag_row;
  GtkWidget *drag_icon;
  int width, height;

  width = gtk_widget_get_width (GTK_WIDGET (self));
  height = gtk_widget_get_height (GTK_WIDGET (self));

  drag_list = gtk_list_box_new ();
  gtk_widget_set_size_request (drag_list, width, height);
  gtk_widget_add_css_class (drag_list, "boxed-list");

  drag_row = ephy_bookmark_tag_row_new (self->tag);
  gtk_list_box_append (GTK_LIST_BOX (drag_list), drag_row);

  drag_icon = gtk_drag_icon_get_for_drag (drag);
  gtk_widget_add_css_class (drag_icon, "boxed-list");
  gtk_drag_icon_set_child (GTK_DRAG_ICON (drag_icon), drag_list);
}

static gboolean
on_target_drop (AdwActionRow *self,
                const GValue *value,
                double        x,
                double        y)
{
  AdwActionRow *source;

  if (!G_VALUE_HOLDS (value, ADW_TYPE_ACTION_ROW))
    return FALSE;

  source = g_value_get_object (value);
  g_object_set_data (G_OBJECT (source), "list-box", gtk_widget_get_parent (GTK_WIDGET (source)));
  g_signal_emit_by_name (source, "move-row", self);

  return TRUE;
}

static void
ephy_bookmark_tag_row_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphyBookmarkTagRow *self = EPHY_BOOKMARK_TAG_ROW (object);

  switch ((EphyBookmarkTagRowProps)prop_id) {
    case PROP_TAG:
      self->tag = g_value_dup_string (value);
      break;
  }
}

static void
ephy_bookmark_tag_row_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphyBookmarkTagRow *self = EPHY_BOOKMARK_TAG_ROW (object);

  switch ((EphyBookmarkTagRowProps)prop_id) {
    case PROP_TAG:
      g_value_set_string (value, self->tag);
      break;
  }
}

static void
ephy_bookmark_tag_row_dispose (GObject *object)
{
  EphyBookmarkTagRow *self = EPHY_BOOKMARK_TAG_ROW (object);

  g_free (&self->tag);

  G_OBJECT_CLASS (ephy_bookmark_tag_row_parent_class)->dispose (object);
}

static void
ephy_bookmark_tag_row_constructed (GObject *object)
{
  EphyBookmarkTagRow *self = EPHY_BOOKMARK_TAG_ROW (object);

  G_OBJECT_CLASS (ephy_bookmark_tag_row_parent_class)->constructed (object);

  if (g_strcmp0 (self->tag, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    gtk_image_set_from_icon_name (GTK_IMAGE (self->tag_image), "ephy-heart-filled-symbolic");
}

static void
ephy_bookmark_tag_row_class_init (EphyBookmarkTagRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_bookmark_tag_row_set_property;
  object_class->get_property = ephy_bookmark_tag_row_get_property;
  object_class->constructed = ephy_bookmark_tag_row_dispose;
  object_class->constructed = ephy_bookmark_tag_row_constructed;

  obj_properties[PROP_TAG] =
    g_param_spec_string ("tag",
                         NULL, NULL, "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_properties), obj_properties);

  gtk_widget_class_install_action (widget_class, "row.move-up", NULL, on_move_up);
  gtk_widget_class_install_action (widget_class, "row.move-down", NULL, on_move_down);

  signals[MOVE_ROW] =
    g_signal_new ("move-row",
                  EPHY_TYPE_BOOKMARK_TAG_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  ADW_TYPE_ACTION_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmark-tag-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkTagRow, tag_image);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkTagRow, drag_handle);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkTagRow, move_menu_button);

  gtk_widget_class_bind_template_callback (widget_class, on_drag_prepare);
  gtk_widget_class_bind_template_callback (widget_class, on_drag_begin);
  gtk_widget_class_bind_template_callback (widget_class, on_target_drop);
}

static void
ephy_bookmark_tag_row_init (EphyBookmarkTagRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ephy_bookmark_tag_row_new (const char *tag)
{
  return g_object_new (EPHY_TYPE_BOOKMARK_TAG_ROW,
                       "tag", tag,
                       NULL);
}

const char *
ephy_bookmark_tag_row_get_tag (EphyBookmarkTagRow *self)
{
  g_assert (EPHY_IS_BOOKMARK_TAG_ROW (self));

  return self->tag;
}

void
ephy_bookmark_tag_row_set_editable (EphyBookmarkTagRow *self,
                                    gboolean            editable)
{
  g_assert (EPHY_IS_BOOKMARK_TAG_ROW (self));

  gtk_widget_set_visible (self->drag_handle, editable);
  gtk_widget_set_visible (self->move_menu_button, editable);
}

void
ephy_bookmark_tag_row_set_movable (EphyBookmarkTagRow *self,
                                   gboolean            movable)
{
  g_assert (EPHY_IS_BOOKMARK_TAG_ROW (self));

  gtk_widget_set_sensitive (self->drag_handle, movable);
  gtk_widget_set_sensitive (self->move_menu_button, movable);
}
