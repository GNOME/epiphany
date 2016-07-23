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

#include "ephy-bookmark-row.h"

struct _EphyBookmarkRow {
  GtkListBoxRow    parent_instance;

  EphyBookmark    *bookmark;

  GtkWidget       *title_label;
};

G_DEFINE_TYPE (EphyBookmarkRow, ephy_bookmark_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_BOOKMARK,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

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
      g_value_set_object (value, self->bookmark);
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

  g_object_bind_property (self->bookmark, "title",
                          self->title_label, "label",
                          G_BINDING_SYNC_CREATE);

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
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarkRow, title_label);
}

static void
ephy_bookmark_row_init (EphyBookmarkRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ephy_bookmark_row_new (EphyBookmark *bookmark)
{
  return g_object_new (EPHY_TYPE_BOOKMARK_ROW,
                       "bookmark", bookmark,
                       NULL);
}
