/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
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

#include "ephy-bookmarks-list-model.h"

struct _EphyBookmarksListModel {
  GObject     parent_instance;

  EphyBookmarksManager *bookmarks_manager;
  GList *dumb_bookmarks;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyBookmarksListModel, ephy_bookmarks_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_BOOKMARKS_MANAGER,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
maybe_add_bookmark (EphyBookmark           *bookmark,
                    EphyBookmarksListModel *self)
{
  if (!ephy_bookmark_is_smart (bookmark))
    self->dumb_bookmarks = g_list_append (self->dumb_bookmarks, g_object_ref (bookmark));
}

static void
refresh_bookmarks_list (EphyBookmarksListModel *self)
{
  GSequence *bookmarks;
  guint previous_length = 0;

  if (self->dumb_bookmarks != NULL) {
    previous_length = g_list_length (self->dumb_bookmarks);
    g_list_free_full (self->dumb_bookmarks, g_object_unref);
    self->dumb_bookmarks = NULL;
  }

  bookmarks = ephy_bookmarks_manager_get_bookmarks (self->bookmarks_manager);
  g_sequence_foreach (bookmarks, (GFunc)maybe_add_bookmark, self);

  g_list_model_items_changed (G_LIST_MODEL (self),
                              0,
                              previous_length,
                              g_list_length (self->dumb_bookmarks));
}

static void
bookmarks_modified_cb (EphyBookmarksManager   *manager,
                       EphyBookmark           *bookmark,
                       EphyBookmarksListModel *self)
{
  if (!ephy_bookmark_is_smart (bookmark))
    refresh_bookmarks_list (self);
}

static GType
ephy_bookmarks_list_model_list_model_get_item_type (GListModel *model)
{
  return EPHY_TYPE_BOOKMARK;
}

static guint
ephy_bookmarks_list_model_list_model_get_n_items (GListModel *model)
{
  EphyBookmarksListModel *self = EPHY_BOOKMARKS_LIST_MODEL (model);

  return g_list_length (self->dumb_bookmarks);
}

static gpointer
ephy_bookmarks_list_model_list_model_get_item (GListModel *model,
                                               guint       position)
{
  EphyBookmarksListModel *self = EPHY_BOOKMARKS_LIST_MODEL (model);

  return g_object_ref (g_list_nth_data (self->dumb_bookmarks, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ephy_bookmarks_list_model_list_model_get_item_type;
  iface->get_n_items = ephy_bookmarks_list_model_list_model_get_n_items;
  iface->get_item = ephy_bookmarks_list_model_list_model_get_item;
}

static void
ephy_bookmarks_list_model_dispose (GObject *object)
{
  EphyBookmarksListModel *self = EPHY_BOOKMARKS_LIST_MODEL (object);

  if (self->dumb_bookmarks != NULL) {
    g_list_free_full (self->dumb_bookmarks, g_object_unref);
    self->dumb_bookmarks = NULL;
  }

  G_OBJECT_CLASS (ephy_bookmarks_list_model_parent_class)->dispose (object);
}

static void
ephy_bookmarks_list_model_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  EphyBookmarksListModel *self = EPHY_BOOKMARKS_LIST_MODEL (object);

  switch (prop_id) {
    case PROP_BOOKMARKS_MANAGER:
      self->bookmarks_manager = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmarks_list_model_constructed (GObject *object)
{
  EphyBookmarksListModel *self = EPHY_BOOKMARKS_LIST_MODEL (object);

  G_OBJECT_CLASS (ephy_bookmarks_list_model_parent_class)->constructed (object);

  refresh_bookmarks_list (self);

  g_signal_connect_object (self->bookmarks_manager, "bookmark-added",
                           G_CALLBACK (bookmarks_modified_cb), self, 0);
  g_signal_connect_object (self->bookmarks_manager, "bookmark-removed",
                           G_CALLBACK (bookmarks_modified_cb), self, 0);
  g_signal_connect_object (self->bookmarks_manager, "bookmark-title-changed",
                           G_CALLBACK (bookmarks_modified_cb), self, 0);
  g_signal_connect_object (self->bookmarks_manager, "bookmark-url-changed",
                           G_CALLBACK (bookmarks_modified_cb), self, 0);
}

static void
ephy_bookmarks_list_model_class_init (EphyBookmarksListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_bookmarks_list_model_constructed;
  object_class->dispose = ephy_bookmarks_list_model_dispose;
  object_class->set_property = ephy_bookmarks_list_model_set_property;

  obj_properties[PROP_BOOKMARKS_MANAGER] =
    g_param_spec_object ("bookmarks-manager",
                         "The bookmarks manager",
                         "The bookmarks manager",
                         EPHY_TYPE_BOOKMARKS_MANAGER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_bookmarks_list_model_init (EphyBookmarksListModel *self)
{
}

EphyBookmarksListModel *
ephy_bookmarks_list_model_new (EphyBookmarksManager *manager)
{
  return EPHY_BOOKMARKS_LIST_MODEL (g_object_new (EPHY_TYPE_BOOKMARKS_LIST_MODEL,
                                                  "bookmarks-manager", manager,
                                                  NULL));
}
