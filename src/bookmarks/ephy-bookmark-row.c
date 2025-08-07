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
#include "ephy-bookmark-row.h"
#include "ephy-bookmarks-dialog.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"
#include "ephy-settings.h"
#include "ephy-shell.h"

#include <adwaita.h>

struct _EphyBookmarkRow {
  AdwActionRow parent_instance;

  EphyBookmark *bookmark;
  GCancellable *cancellable;

  GtkWidget *favicon_image;
  GtkWidget *drag_handle;
  GtkWidget *remove_button;
  GtkWidget *properties_button;
  GtkWidget *move_menu_button;
};

G_DEFINE_FINAL_TYPE (EphyBookmarkRow, ephy_bookmark_row, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,
  PROP_BOOKMARK,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  MOVE_ROW,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
move_up_cb (GtkWidget  *self,
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
move_down_cb (GtkWidget  *self,
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

static void
ephy_bookmark_row_remove_button_clicked_cb (EphyBookmarkRow *row,
                                            GtkButton       *button)
{
  g_assert (EPHY_IS_BOOKMARK_ROW (row));
  g_assert (GTK_IS_BUTTON (button));

  ephy_bookmarks_manager_remove_bookmark (ephy_shell_get_bookmarks_manager (ephy_shell_get_default ()), row->bookmark);
}

static void
ephy_bookmark_row_properties_button_clicked_cb (EphyBookmarkRow *row,
                                                GtkButton       *button)
{
  GtkWidget *dialog;

  g_assert (EPHY_IS_BOOKMARK_ROW (row));
  g_assert (GTK_IS_BUTTON (button));

  dialog = ephy_bookmark_properties_new (ephy_bookmark_row_get_bookmark (row), FALSE);
  adw_dialog_present (ADW_DIALOG (dialog), gtk_widget_get_parent (GTK_WIDGET (row)));
}

static GdkContentProvider *
drag_prepare_cb (AdwActionRow *self,
                 double        x,
                 double        y)
{
  return gdk_content_provider_new_typed (ADW_TYPE_ACTION_ROW, self);
}

static void
drag_begin_cb (EphyBookmarkRow *self,
               GdkDrag         *drag)
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

  drag_row = ephy_bookmark_row_new (self->bookmark);
  gtk_list_box_append (GTK_LIST_BOX (drag_list), drag_row);

  drag_icon = gtk_drag_icon_get_for_drag (drag);
  gtk_widget_add_css_class (drag_icon, "boxed-list");
  gtk_drag_icon_set_child (GTK_DRAG_ICON (drag_icon), drag_list);
}

static gboolean
drop_cb (AdwActionRow *self,
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
    g_signal_emit (source, signals[MOVE_ROW], 0, self);
  else
    g_signal_emit_by_name (source, "move-tag-row", self);

  return TRUE;
}

void
ephy_bookmark_row_open (EphyBookmarkRow *self,
                        EphyLinkFlags    flags)
{
  GtkWidget *window = gtk_widget_get_ancestor (GTK_WIDGET (self), EPHY_TYPE_WINDOW);
  const char *url = ephy_bookmark_get_url (self->bookmark);
  ephy_link_open (EPHY_LINK (window), url, NULL, flags | EPHY_LINK_BOOKMARK);
  gtk_widget_grab_focus (GTK_WIDGET (self));
}

static void
ephy_bookmark_row_favicon_loaded_cb (GObject      *source,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  EphyBookmarkRow *self = user_data;
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  g_autoptr (GdkTexture) icon_texture = NULL;
  g_autoptr (GIcon) favicon = NULL;
  int scale;

  icon_texture = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (!icon_texture)
    return;

  g_assert (EPHY_IS_BOOKMARK_ROW (self));

  scale = gtk_widget_get_scale_factor (self->favicon_image);
  favicon = ephy_favicon_get_from_texture_scaled (icon_texture, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
  if (favicon && self->favicon_image)
    gtk_image_set_from_gicon (GTK_IMAGE (self->favicon_image), favicon);
}

static void
ephy_bookmark_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);

  switch (prop_id) {
    case PROP_BOOKMARK:
      self->bookmark = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_row_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);

  switch (prop_id) {
    case PROP_BOOKMARK:
      g_value_set_object (value, ephy_bookmark_row_get_bookmark (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_row_dispose (GObject *object)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);

  g_clear_object (&self->bookmark);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ephy_bookmark_row_parent_class)->dispose (object);
}

static gboolean
transform_bookmark_title (GBinding     *binding,
                          const GValue *from_value,
                          GValue       *to_value,
                          gpointer      user_data)
{
  EphyBookmarkRow *row = EPHY_BOOKMARK_ROW (user_data);
  const char *title;
  g_autofree char *converted_title = NULL;

  title = g_value_get_string (from_value);
  converted_title = g_markup_escape_text (title, -1);

  if (strlen (converted_title) == 0) {
    EphyBookmark *bookmark;
    const char *url;

    bookmark = EPHY_BOOKMARK (row->bookmark);
    url = ephy_bookmark_get_url (bookmark);

    g_value_set_string (to_value, url);
    gtk_widget_set_tooltip_text (GTK_WIDGET (row), url);
  } else {
    g_value_set_string (to_value, converted_title);
    gtk_widget_set_tooltip_text (GTK_WIDGET (row), converted_title);
  }

  return TRUE;
}

static void
ephy_bookmark_row_map (GtkWidget *widget)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (widget);
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitFaviconDatabase *database;

  GTK_WIDGET_CLASS (ephy_bookmark_row_parent_class)->map (widget);

  database = ephy_embed_shell_get_favicon_database (shell);
  webkit_favicon_database_get_favicon (database,
                                       ephy_bookmark_get_url (self->bookmark),
                                       self->cancellable,
                                       (GAsyncReadyCallback)ephy_bookmark_row_favicon_loaded_cb,
                                       self);
}

static void
ephy_bookmark_row_constructed (GObject *object)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);

  G_OBJECT_CLASS (ephy_bookmark_row_parent_class)->constructed (object);

  g_object_bind_property_full (self->bookmark, "title",
                               self, "title",
                               G_BINDING_SYNC_CREATE,
                               transform_bookmark_title,
                               NULL,
                               self, NULL);

  g_signal_connect_object (self->bookmark,
                           "notify::title",
                           G_CALLBACK (gtk_list_box_row_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->bookmark,
                           "notify::bmkUri",
                           G_CALLBACK (gtk_list_box_row_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_settings_bind (EPHY_SETTINGS_LOCKDOWN,
                   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING,
                   self->remove_button,
                   "visible",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (EPHY_SETTINGS_LOCKDOWN,
                   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING,
                   self->properties_button,
                   "visible",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static void
ephy_bookmark_row_class_init (EphyBookmarkRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_bookmark_row_set_property;
  object_class->get_property = ephy_bookmark_row_get_property;
  object_class->dispose = ephy_bookmark_row_dispose;
  object_class->constructed = ephy_bookmark_row_constructed;

  widget_class->map = ephy_bookmark_row_map;

  obj_properties[PROP_BOOKMARK] =
    g_param_spec_object ("bookmark",
                         NULL, NULL,
                         EPHY_TYPE_BOOKMARK,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_install_action (widget_class, "row.move-up", NULL, move_up_cb);
  gtk_widget_class_install_action (widget_class, "row.move-down", NULL, move_down_cb);

  signals[MOVE_ROW] =
    g_signal_new ("move-row",
                  EPHY_TYPE_BOOKMARK_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  ADW_TYPE_ACTION_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmark-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, favicon_image);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, drag_handle);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, remove_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, properties_button);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, move_menu_button);

  gtk_widget_class_bind_template_callback (widget_class, drag_prepare_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_begin_cb);
}

static void
on_row_activated (AdwActionRow *self,
                  gpointer      user_data)
{
  ephy_bookmark_row_open (EPHY_BOOKMARK_ROW (self), 0);
}

static void
ephy_bookmark_row_init (EphyBookmarkRow *self)
{
  GtkDropTarget *target;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self, "activated", G_CALLBACK (on_row_activated), self, G_CONNECT_DEFAULT);

  g_signal_connect_object (self->remove_button,
                           "clicked",
                           G_CALLBACK (ephy_bookmark_row_remove_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->properties_button,
                           "clicked",
                           G_CALLBACK (ephy_bookmark_row_properties_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->cancellable = g_cancellable_new ();

  target = gtk_drop_target_new (ADW_TYPE_ACTION_ROW, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (target, TRUE);
  g_signal_connect_swapped (target, "drop", G_CALLBACK (drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (target));
}

GtkWidget *
ephy_bookmark_row_new (EphyBookmark *bookmark)
{
  return g_object_new (EPHY_TYPE_BOOKMARK_ROW,
                       "bookmark", bookmark,
                       NULL);
}

EphyBookmark *
ephy_bookmark_row_get_bookmark (EphyBookmarkRow *self)
{
  g_assert (EPHY_IS_BOOKMARK_ROW (self));

  return self->bookmark;
}

const char *
ephy_bookmark_row_get_bookmark_url (EphyBookmarkRow *self)
{
  g_assert (EPHY_IS_BOOKMARK_ROW (self));

  return ephy_bookmark_get_url (self->bookmark);
}

void
ephy_bookmark_row_set_movable (EphyBookmarkRow *self,
                               gboolean         movable)
{
  gtk_widget_set_sensitive (self->drag_handle, movable);
  gtk_widget_set_sensitive (self->move_menu_button, movable);
}
