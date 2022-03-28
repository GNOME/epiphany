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

  GtkWidget *favicon_image;
  GtkWidget *title_label;
  GtkWidget *properties_button;
};

G_DEFINE_TYPE (EphyBookmarkRow, ephy_bookmark_row, GTK_TYPE_LIST_BOX_ROW)

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
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *grid;

  g_assert (EPHY_IS_BOOKMARK_ROW (row));
  g_assert (GTK_IS_BUTTON (button));

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "title", _("Bookmark Properties"),
                         "transient-for", GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (row))),
                         "use-header-bar", TRUE,
                         "modal", TRUE,
                         NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  grid = ephy_bookmark_properties_new (ephy_bookmark_row_get_bookmark (row),
                                       EPHY_BOOKMARK_PROPERTIES_TYPE_DIALOG,
                                       dialog);
  gtk_window_set_default (GTK_WINDOW (dialog),
                          ephy_bookmark_properties_get_add_tag_button (EPHY_BOOKMARK_PROPERTIES (grid)));

  gtk_container_add (GTK_CONTAINER (content_area), grid);

  gtk_widget_show (dialog);
}

static void
ephy_bookmark_row_favicon_loaded_cb (GObject      *source,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  g_autoptr (EphyBookmarkRow) self = user_data;
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  cairo_surface_t *icon_surface;
  g_autoptr (GdkPixbuf) favicon = NULL;

  g_assert (EPHY_IS_BOOKMARK_ROW (self));

  icon_surface = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (icon_surface) {
    int scale = gtk_widget_get_scale_factor (self->favicon_image);

    favicon = ephy_pixbuf_get_from_surface_scaled (icon_surface, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
    cairo_surface_destroy (icon_surface);
  }

  if (favicon) {
    if (self->favicon_image != NULL)
      gtk_image_set_from_gicon (GTK_IMAGE (self->favicon_image), G_ICON (favicon), GTK_ICON_SIZE_BUTTON);
  }
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

  G_OBJECT_CLASS (ephy_bookmark_row_parent_class)->dispose (object);
}

static void
favicon_image_destroyed (EphyBookmarkRow *self,
                         GtkWidget       *favicon_image)
{
  self->favicon_image = NULL;
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
ephy_bookmark_row_constructed (GObject *object)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitFaviconDatabase *database;

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

  database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));
  webkit_favicon_database_get_favicon (database,
                                       ephy_bookmark_get_url (self->bookmark),
                                       NULL,
                                       (GAsyncReadyCallback)ephy_bookmark_row_favicon_loaded_cb,
                                       g_object_ref (self));

  /* Although we keep a ref to ourself during the favicon load, so we are
   * guaranteed to remain a valid GObject, the widget hierarchy could still
   * be destroyed before ephy_bookmark_favicon_loaded_cb() is called. Hence we
   * need to keep track of whether self->favicon_image is still valid. */
  g_signal_connect_object (self->favicon_image, "destroy",
                           G_CALLBACK (favicon_image_destroyed), self, G_CONNECT_SWAPPED);
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

  obj_properties[PROP_BOOKMARK] =
    g_param_spec_object ("bookmark",
                         "An EphyBookmark object",
                         "The EphyBookmark shown by this widget",
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
