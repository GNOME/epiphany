/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2004 Peter Harvey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
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

#include "ephy-bookmarks-menu.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-bookmark-action.h"
#include "ephy-open-tabs-action.h"
#include "ephy-topic-action.h"
#include "ephy-nodes-cover.h"
#include "ephy-node-common.h"
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <string.h>

#define MIN_MENU_SIZE 3
#define MAX_MENU_SIZE 21

enum {
  BUILD_SUBDIVIS       = 1 << 0,
  BUILD_SUBMENUS       = 1 << 1,
  BUILD_CHILD_SUBDIVIS = 1 << 2,
  BUILD_CHILD_SUBMENUS = 1 << 3
};

/* Construct a block of bookmark actions. Note that no bookmark action appears
 * more than once in a menu, so no need to supply names. */
static void
append_bookmarks (GMenu         *menu,
                  const GPtrArray *bookmarks)
{
  EphyNode *child;
  const gchar *action_name;

  long i;

  for (i = 0; i < bookmarks->len; i++) {
    child = g_ptr_array_index (bookmarks, i);

    action_name = g_action_print_detailed_name ("win.open-bookmark",
                                                g_variant_new_string (ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION)));

    g_menu_append (menu,
                   ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_TITLE),
                   action_name);
  }
}

/* Build a menu of the given bookmarks categorised by the given topics.
 * Shows categorisation using subdivisions, submenus, or a mix of both. */
static void
append_menu (GMenu *menu, const GPtrArray *topics, const GPtrArray *bookmarks, guint flags)
{
  GPtrArray *uncovered;
  guint i, j;
  GMenu *submenu, *section;

  gboolean use_subdivis = flags & BUILD_SUBDIVIS;
  gboolean use_submenus = flags & BUILD_SUBMENUS;

  if (use_subdivis || use_submenus) {
    GPtrArray *subset, *covering, *subdivisions, *submenus, *unused;
    GArray *sizes = 0;
    EphyNode *topic;
    gint size, total;
    gboolean separate = FALSE;
    const gchar *name;

    /* Get the subtopics, uncovered bookmarks, and subtopic sizes. */
    sizes = g_array_sized_new (FALSE, FALSE, sizeof (int), topics->len);
    uncovered = g_ptr_array_sized_new (bookmarks->len);
    covering = ephy_nodes_get_covering (topics, bookmarks, 0, uncovered, sizes);

    /* Preallocate arrays for submenus, subdivisions, and bookmark subsets. */
    subdivisions = g_ptr_array_sized_new (topics->len);
    submenus = g_ptr_array_sized_new (topics->len);
    subset = g_ptr_array_sized_new (bookmarks->len);
    unused = g_ptr_array_sized_new (bookmarks->len);

    /* Get the total number of items in the menu. */
    total = uncovered->len;
    for (i = 0; i < covering->len; i++)
      total += g_array_index (sizes, int, i);

    /* Seperate covering into list of submenus and subdivisions */
    for (i = 0; i < covering->len; i++) {
      topic = g_ptr_array_index (covering, i);
      size = g_array_index (sizes, int, i);

      if (!use_submenus || (use_subdivis && (size < MIN_MENU_SIZE || total < MAX_MENU_SIZE))) {
        g_ptr_array_add (subdivisions, topic);
      } else {
        g_ptr_array_add (submenus, topic);
        total = total - size + 1;
      }
    }

    /* Sort the list of submenus and subdivisions. */
    g_ptr_array_sort (submenus, ephy_bookmarks_compare_topic_pointers);
    g_ptr_array_sort (subdivisions, ephy_bookmarks_compare_topic_pointers);

    if (flags & BUILD_CHILD_SUBDIVIS) flags |= BUILD_SUBDIVIS;
    if (flags & BUILD_CHILD_SUBMENUS) flags |= BUILD_SUBMENUS;

    /* Create each of the submenus. */
    for (i = 0; i < submenus->len; i++) {
      topic = g_ptr_array_index (submenus, i);
      ephy_nodes_get_covered (topic, bookmarks, subset);

      name = ephy_node_get_property_string (topic, EPHY_NODE_KEYWORD_PROP_NAME);

      submenu = g_menu_new ();
      g_menu_append_submenu (menu, name, G_MENU_MODEL (submenu));
      append_menu (G_MENU (submenu), topics, subset, flags);

      separate = TRUE;
    }

    /* Build a list of bookmarks which don't appear in any subdivision yet. */
    for (i = 0; i < bookmarks->len; i++) {
      g_ptr_array_add (unused, g_ptr_array_index (bookmarks, i));
    }

    /* Create each of the subdivisions. */
    for (i = 0; i < subdivisions->len; i++) {
      topic = g_ptr_array_index (subdivisions, i);
      ephy_nodes_get_covered (topic, unused, subset);
      g_ptr_array_sort (subset, ephy_bookmarks_compare_bookmark_pointers);

      if (separate) {
        section = g_menu_new ();
        g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
      }

      append_bookmarks (G_MENU (section), subset);
      separate = TRUE;

      /* Record that each bookmark has been added. */
      for (j = 0; j < subset->len; j++) {
        g_ptr_array_remove_fast (unused, g_ptr_array_index (subset, j));
      }
    }

    g_array_free (sizes, TRUE);
    g_ptr_array_free (covering, TRUE);
    g_ptr_array_free (subdivisions, TRUE);
    g_ptr_array_free (submenus, TRUE);
    g_ptr_array_free (subset, TRUE);
    g_ptr_array_free (unused, TRUE);
  } else {
    uncovered = g_ptr_array_sized_new (bookmarks->len);
    for (i = 0; i < bookmarks->len; i++)
      g_ptr_array_add (uncovered, g_ptr_array_index (bookmarks, i));
    g_ptr_array_sort (uncovered, ephy_bookmarks_compare_bookmark_pointers);
  }

  /* Create the final subdivision (uncovered bookmarks). */
  g_ptr_array_sort (uncovered, ephy_bookmarks_compare_bookmark_pointers);
  append_bookmarks (menu, uncovered);
  g_ptr_array_free (uncovered, TRUE);
}

void
ephy_bookmarks_menu_build (GMenu *menu, EphyNode *parent)
{
  GPtrArray *children, *topics;
  EphyBookmarks *eb;
  EphyNode *node;
  gint priority;
  guint flags, id, i;

  eb = ephy_shell_get_bookmarks (ephy_shell_get_default ());

  children = ephy_node_get_children (ephy_bookmarks_get_keywords (eb));
  topics = g_ptr_array_sized_new (children->len);
  for (i = 0; i < children->len; i++) {
    node = g_ptr_array_index (children, i);
    priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
    if (priority == EPHY_NODE_NORMAL_PRIORITY)
      g_ptr_array_add (topics, node);
  }

  /* If no parent was supplied, use the default 'All' */
  node = parent ? parent : ephy_bookmarks_get_bookmarks (eb);
  children = ephy_node_get_children (node);

  /* Determine what kind of menu we want. */
  id = ephy_node_get_id (node);
  switch (id) {
    case FAVORITES_NODE_ID:
      flags = 0;
      break;
    case BOOKMARKS_NODE_ID:
      flags = BUILD_SUBMENUS | BUILD_CHILD_SUBDIVIS;
      break;
    default:
      flags = BUILD_SUBMENUS | BUILD_SUBDIVIS | BUILD_CHILD_SUBDIVIS;
      /* flags = BUILD_SUBDIVIS; */
      break;
  }

  /* If this menu is the 'All' menu, be sure to include the 'local' topic. */
  if (id == BOOKMARKS_NODE_ID) {
    EphyNode *local_node;

    local_node = ephy_bookmarks_get_local (eb);
    if (local_node != NULL) {
      g_ptr_array_add (topics, ephy_bookmarks_get_local (eb));
    }

    append_menu (menu, topics, children, flags);
    g_ptr_array_free (topics, TRUE);
  }
}
