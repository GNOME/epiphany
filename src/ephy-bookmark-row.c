/*
 * Copyright (C) 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-bookmark-properties-grid.h"
#include "ephy-bookmark-row.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"

struct _EphyBookmarkRow {
  GtkListBoxRow    parent_instance;

  EphyBookmark    *bookmark;

  GtkWidget       *favicon_image;
  GtkWidget       *title_label;
  GtkWidget       *properties_button;
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

  dialog = gtk_dialog_new_with_buttons ("Bookmark Properties",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (row))),
                                        GTK_DIALOG_USE_HEADER_BAR,
                                        NULL, NULL);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  grid = ephy_bookmark_properties_grid_new (ephy_bookmark_row_get_bookmark (row),
                                            EPHY_BOOKMARK_PROPERTIES_GRID_TYPE_DIALOG);
  gtk_window_set_default (GTK_WINDOW (dialog),
                          ephy_bookmark_properties_grid_get_add_tag_button (EPHY_BOOKMARK_PROPERTIES_GRID (grid)));

  gtk_container_add (GTK_CONTAINER (content_area), grid);

  gtk_widget_show (dialog);
}

static void
ephy_bookmark_row_favicon_loaded_cb (GObject      *source,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  EphyBookmarkRow *self = user_data;
  WebKitFaviconDatabase *database = (WebKitFaviconDatabase *)source;
  cairo_surface_t *icon_surface;
  GdkPixbuf *favicon;

  g_assert (EPHY_IS_BOOKMARK_ROW (self));

  icon_surface = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (icon_surface) {
    favicon = ephy_pixbuf_get_from_surface_scaled (icon_surface, FAVICON_SIZE, FAVICON_SIZE);
    cairo_surface_destroy (icon_surface);
  }

  if (favicon) {
    gtk_image_set_from_pixbuf (GTK_IMAGE (self->favicon_image), favicon);
    g_object_unref (favicon);
  }
}

static void
ephy_bookmark_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);

  switch (prop_id)
    {
    case PROP_BOOKMARK:
      self->bookmark = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ephy_bookmark_row_get_property (GObject      *object,
                                guint         prop_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);

  switch (prop_id)
    {
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
ephy_bookmark_row_constructed (GObject *object)
{
  EphyBookmarkRow *self = EPHY_BOOKMARK_ROW (object);
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitFaviconDatabase *database;

  g_object_bind_property (self->bookmark, "title",
                          self->title_label, "label",
                          G_BINDING_SYNC_CREATE);

  database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (shell));
  webkit_favicon_database_get_favicon (database,
                                       ephy_bookmark_get_url (self->bookmark),
                                       NULL,
                                       (GAsyncReadyCallback)ephy_bookmark_row_favicon_loaded_cb,
                                       self);

  G_OBJECT_CLASS (ephy_bookmark_row_parent_class)->constructed (object);
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
  g_return_val_if_fail (EPHY_IS_BOOKMARK_ROW (self), NULL);

  return self->bookmark;
}
