/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002-2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2005 Peter Harvey <pah06@uow.edu.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-topics-palette.h"
#include "ephy-nodes-cover.h"
#include "ephy-node-common.h"
#include "ephy-bookmarks.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

struct _EphyTopicsPalette {
  GtkListStore parent_instance;

  /* construct properties */
  EphyBookmarks *bookmarks;
  EphyNode *bookmark;

  /* non-construct property */
  int mode;
};

enum {
  PROP_0,
  PROP_BOOKMARKS,
  PROP_BOOKMARK,
  PROP_MODE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  MODE_GROUPED,
  MODE_LIST,
  MODES
};

G_DEFINE_TYPE (EphyTopicsPalette, ephy_topics_palette, GTK_TYPE_LIST_STORE)

static void
append_topics (EphyTopicsPalette *self,
               GtkTreeIter       *iter,
               gboolean          *valid,
               gboolean          *first,
               GPtrArray         *topics)
{
  EphyNode *node;
  const char *title;
  guint i;

  if (topics->len == 0) {
    return;
  }

  if (!*first) {
    if (!*valid) gtk_list_store_append (GTK_LIST_STORE (self), iter);
    gtk_list_store_set (GTK_LIST_STORE (self), iter,
                        EPHY_TOPICS_PALETTE_COLUMN_TITLE, NULL,
                        EPHY_TOPICS_PALETTE_COLUMN_NODE, NULL,
                        -1);
    *valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self), iter);
  }

  for (i = 0; i < topics->len; i++) {
    node = g_ptr_array_index (topics, i);
    title = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);

    if (!*valid) gtk_list_store_append (GTK_LIST_STORE (self), iter);
    gtk_list_store_set (GTK_LIST_STORE (self), iter,
                        EPHY_TOPICS_PALETTE_COLUMN_TITLE, title,
                        EPHY_TOPICS_PALETTE_COLUMN_NODE, node,
                        EPHY_TOPICS_PALETTE_COLUMN_SELECTED, ephy_node_has_child (node, self->bookmark),
                        -1);
    *valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self), iter);
    *first = FALSE;
  }
}

void
ephy_topics_palette_update_list (EphyTopicsPalette *self)
{
  GPtrArray *children, *bookmarks, *topics;
  EphyNode *node;
  GtkTreeIter iter;
  guint i;
  gint priority;
  gboolean valid, first;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self), &iter);
  first = TRUE;

  if (self->mode == MODE_LIST) {
    /* Allocate and fill the suggestions array. */
    node = ephy_bookmarks_get_keywords (self->bookmarks);
    children = ephy_node_get_children (node);
    topics = g_ptr_array_sized_new (children->len);
    for (i = 0; i < children->len; i++) {
      node = g_ptr_array_index (children, i);

      priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
      if (priority != EPHY_NODE_NORMAL_PRIORITY)
        continue;

      g_ptr_array_add (topics, node);
    }

    g_ptr_array_sort (topics, ephy_bookmarks_compare_topic_pointers);
    append_topics (self, &iter, &valid, &first, topics);
    g_ptr_array_free (topics, TRUE);
  } else if (self->mode == MODE_GROUPED) {
    GPtrArray *suggested, *selected;

    /* Allocate and fill the bookmarks array. */
    node = ephy_bookmarks_get_bookmarks (self->bookmarks);
    children = ephy_node_get_children (node);
    bookmarks = g_ptr_array_sized_new (children->len);
    for (i = 0; i < children->len; i++) {
      g_ptr_array_add (bookmarks, g_ptr_array_index (children, i));
    }

    /* Allocate and fill the topics array. */
    node = ephy_bookmarks_get_keywords (self->bookmarks);
    children = ephy_node_get_children (node);
    topics = g_ptr_array_sized_new (children->len);
    suggested = g_ptr_array_sized_new (children->len);
    selected = g_ptr_array_sized_new (children->len);
    for (i = 0; i < children->len; i++) {
      node = g_ptr_array_index (children, i);

      priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
      if (priority != EPHY_NODE_NORMAL_PRIORITY)
        continue;

      /* We'll consider only bookmarks covered by the same topics as our bookmark. */
      if (ephy_node_has_child (node, self->bookmark)) {
        ephy_nodes_remove_not_covered (node, bookmarks);
        g_ptr_array_add (selected, node);
      }
      /* We'll onsider only topics that are not already selected for our bookmark. */
      else{
        g_ptr_array_add (topics, node);
      }
    }

    /* Get the minimum cover of topics for the bookmarks. */
    suggested = ephy_nodes_get_covering (topics, bookmarks, suggested, 0, 0);

    for (i = 0; i < suggested->len; i++) {
      g_ptr_array_remove_fast (topics, g_ptr_array_index (suggested, i));
    }

    /* Add any topics which cover the bookmarks completely in their own right, or
       have no bookmarks currently associated with it. */
    for (i = 0; i < topics->len; i++) {
      node = g_ptr_array_index (topics, i);
      if (!ephy_node_has_child (node, self->bookmark) &&
          ephy_nodes_covered (node, bookmarks)) {
        g_ptr_array_add (suggested, node);
        g_ptr_array_remove_index_fast (topics, i);
        i--;
      }
    }

    g_ptr_array_sort (selected, ephy_bookmarks_compare_topic_pointers);
    g_ptr_array_sort (suggested, ephy_bookmarks_compare_topic_pointers);
    g_ptr_array_sort (topics, ephy_bookmarks_compare_topic_pointers);
    append_topics (self, &iter, &valid, &first, selected);
    append_topics (self, &iter, &valid, &first, suggested);
    append_topics (self, &iter, &valid, &first, topics);
    g_ptr_array_free (selected, TRUE);
    g_ptr_array_free (suggested, TRUE);
    g_ptr_array_free (bookmarks, TRUE);
    g_ptr_array_free (topics, TRUE);
  }

  while (valid) {
    valid = gtk_list_store_remove (GTK_LIST_STORE (self), &iter);
  }
}

static void
tree_changed_cb (EphyBookmarks     *bookmarks,
                 EphyTopicsPalette *self)
{
  ephy_topics_palette_update_list (self);
}

static void
node_added_cb (EphyNode          *parent,
               EphyNode          *child,
               EphyTopicsPalette *self)
{
  ephy_topics_palette_update_list (self);
}

static void
node_changed_cb (EphyNode          *parent,
                 EphyNode          *child,
                 guint              property_id,
                 EphyTopicsPalette *self)
{
  ephy_topics_palette_update_list (self);
}

static void
node_removed_cb (EphyNode          *parent,
                 EphyNode          *child,
                 guint              index,
                 EphyTopicsPalette *self)
{
  ephy_topics_palette_update_list (self);
}

static void
ephy_topics_palette_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyTopicsPalette *self = EPHY_TOPICS_PALETTE (object);
  EphyNode *node;

  switch (prop_id) {
    case PROP_BOOKMARKS:
      self->bookmarks = g_value_get_object (value);
      node = ephy_bookmarks_get_keywords (self->bookmarks);
      ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
                                       (EphyNodeCallback)node_added_cb, object);
      ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
                                       (EphyNodeCallback)node_changed_cb, object);
      ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
                                       (EphyNodeCallback)node_removed_cb, object);
      g_signal_connect_object (self->bookmarks, "tree-changed",
                               G_CALLBACK (tree_changed_cb), self,
                               G_CONNECT_AFTER);
      break;
    case PROP_BOOKMARK:
      self->bookmark = g_value_get_pointer (value);
      break;
    case PROP_MODE:
      self->mode = g_value_get_int (value);
      ephy_topics_palette_update_list (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GObject *
ephy_topics_palette_constructor (GType                  type,
                                 guint                  n_construct_properties,
                                 GObjectConstructParam *construct_params)

{
  GObject *object;
  GType types[3] = { G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN };

  object = G_OBJECT_CLASS (ephy_topics_palette_parent_class)->constructor (type,
                                                                           n_construct_properties,
                                                                           construct_params);
  gtk_list_store_set_column_types (GTK_LIST_STORE (object), 3, types);
  ephy_topics_palette_update_list (EPHY_TOPICS_PALETTE (object));

  return object;
}

static void
ephy_topics_palette_init (EphyTopicsPalette *self)
{
}

EphyTopicsPalette *
ephy_topics_palette_new (EphyBookmarks *bookmarks,
                         EphyNode      *bookmark)
{
  g_assert (bookmarks != NULL);

  return g_object_new (EPHY_TYPE_TOPICS_PALETTE,
                       "bookmarks", bookmarks,
                       "bookmark", bookmark,
                       NULL);
}

static void
ephy_topics_palette_class_init (EphyTopicsPaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_topics_palette_set_property;
  object_class->constructor = ephy_topics_palette_constructor;

  obj_properties[PROP_BOOKMARKS] =
    g_param_spec_object ("bookmarks",
                         "Bookmarks set",
                         "Bookmarks set",
                         EPHY_TYPE_BOOKMARKS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_BOOKMARK] =
    g_param_spec_pointer ("bookmark",
                          "Bookmark",
                          "Bookmark",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_MODE] =
    g_param_spec_int ("mode",
                      "Mode",
                      "Mode",
                      0, MODES - 1, 0,
                      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}
