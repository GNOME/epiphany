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
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"
#include "ephy-settings.h"

struct _EphyBookmarkRow {
  GtkListBoxRow parent_instance;

  EphyBookmark *bookmark;
  GCancellable *cancellable;

  GtkWidget *favicon_image;
  GtkWidget *title_label;
  GtkWidget *properties_button;
};

G_DEFINE_FINAL_TYPE (EphyBookmarkRow, ephy_bookmark_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_BOOKMARK,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_bookmark_row_button_clicked_cb (EphyBookmarkRow *row,
                                     GtkButton       *button)
{
  GtkWidget *popover;
  GtkWidget *dialog;
  GtkWidget *grid;
  GtkWidget *toolbar_view;
  GtkEventController *controller;
  GtkShortcut *shortcut;

  g_assert (EPHY_IS_BOOKMARK_ROW (row));
  g_assert (GTK_IS_BUTTON (button));

  dialog = g_object_new (ADW_TYPE_WINDOW,
                         "title", _("Bookmark Properties"),
                         "transient-for", GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (row))),
                         "modal", TRUE,
                         NULL);

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_Escape, 0),
                               gtk_named_action_new ("window.close"));
  controller = gtk_shortcut_controller_new ();

  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_add_controller (dialog, controller);

  popover = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_POPOVER);

  if (popover)
    gtk_popover_popdown (GTK_POPOVER (popover));

  grid = ephy_bookmark_properties_new (ephy_bookmark_row_get_bookmark (row),
                                       EPHY_BOOKMARK_PROPERTIES_TYPE_DIALOG,
                                       dialog);
  gtk_window_set_default_widget (GTK_WINDOW (dialog),
                                 ephy_bookmark_properties_get_add_tag_button (EPHY_BOOKMARK_PROPERTIES (grid)));

  toolbar_view = adw_toolbar_view_new ();
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view),
                                adw_header_bar_new ());
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view),
                                grid);
  adw_window_set_content (ADW_WINDOW (dialog), toolbar_view);

  gtk_window_present (GTK_WINDOW (dialog));
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

  title = g_value_get_string (from_value);

  if (strlen (title) == 0) {
    EphyBookmark *bookmark;
    const char *url;

    bookmark = EPHY_BOOKMARK (row->bookmark);
    url = ephy_bookmark_get_url (bookmark);

    g_value_set_string (to_value, url);
    gtk_widget_set_tooltip_text (GTK_WIDGET (row), url);
  } else {
    g_value_set_string (to_value, title);
    gtk_widget_set_tooltip_text (GTK_WIDGET (row), title);
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
                               self->title_label, "label",
                               G_BINDING_SYNC_CREATE,
                               transform_bookmark_title,
                               NULL,
                               self, NULL);

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmark-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, favicon_image);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, title_label);
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, properties_button);
}

static void
ephy_bookmark_row_init (EphyBookmarkRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->properties_button,
                           "clicked",
                           G_CALLBACK (ephy_bookmark_row_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->cancellable = g_cancellable_new ();
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
