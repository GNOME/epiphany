/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2001 Matthew Mueller
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright © 2003 Marco Pesenti Gritti <mpeseng@tin.it>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-initial-state.h"

#include "ephy-file-helpers.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-node-common.h"
#include "ephy-node-db.h"

#include <gtk/gtk.h>
#include <string.h>

#define EPHY_STATES_XML_FILE  "states.xml"
#define EPHY_STATES_XML_ROOT    (const xmlChar *)"ephy_states"
#define EPHY_STATES_XML_VERSION (const xmlChar *)"1.0"

enum
{
  EPHY_NODE_INITIAL_STATE_PROP_NAME = 2,
  EPHY_NODE_INITIAL_STATE_PROP_WIDTH = 3,
  EPHY_NODE_INITIAL_STATE_PROP_HEIGHT = 4,
  EPHY_NODE_INITIAL_STATE_PROP_MAXIMIZE = 5,
  EPHY_NODE_INITIAL_STATE_PROP_POSITION_X = 6,
  EPHY_NODE_INITIAL_STATE_PROP_POSITION_Y = 7,
  EPHY_NODE_INITIAL_STATE_PROP_SIZE = 8,
  EPHY_NODE_INITIAL_STATE_PROP_POSITION = 9,
  EPHY_NODE_INITIAL_STATE_PROP_ACTIVE = 10
};

static EphyNode *states = NULL;
static EphyNodeDb *states_db = NULL;

static void
ephy_states_save (void)
{
  char *xml_file;

  xml_file = g_build_filename (ephy_dot_dir (),
                               EPHY_STATES_XML_FILE,
                               NULL);

  ephy_node_db_write_to_xml_safe (states_db, 
                                  (const xmlChar *)xml_file,
                                  EPHY_STATES_XML_ROOT,
                                  EPHY_STATES_XML_VERSION,
                                  NULL, /* comment */
                                  states, NULL, NULL,
                                  NULL);
  
  g_free (xml_file);  
}

static EphyNode *
find_by_name (const char *name)
{
  EphyNode *result = NULL;
  GPtrArray *children;
  int i;

  children = ephy_node_get_children (states);
  for (i = 0; i < children->len; i++) {
    EphyNode *kid;
    const char *node_name;

    kid = g_ptr_array_index (children, i);

    node_name = ephy_node_get_property_string
      (kid, EPHY_NODE_INITIAL_STATE_PROP_NAME);

    if (strcmp (node_name, name) == 0)
      result = kid;
  }

  return result;
}

static void
ensure_states (void)
{
  if (states == NULL) {
    char *xml_file;

    xml_file = g_build_filename (ephy_dot_dir (),
                                 EPHY_STATES_XML_FILE,
                                 NULL);

    states_db = ephy_node_db_new (EPHY_NODE_DB_STATES);
    states = ephy_node_new_with_id (states_db, STATES_NODE_ID);
    ephy_node_db_load_from_file (states_db, xml_file,
                                 EPHY_STATES_XML_ROOT,
                                 EPHY_STATES_XML_VERSION);
  
    g_free (xml_file);
  }
}

static void
ephy_state_window_set_size (GtkWidget *window, EphyNode *node)
{
  int width, height, w = -1, h = -1;
  gboolean maximize, size;

  width = ephy_node_get_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_WIDTH);
  height = ephy_node_get_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_HEIGHT);
  maximize = ephy_node_get_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_MAXIMIZE);
  size = ephy_node_get_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_SIZE);

  gtk_window_get_default_size (GTK_WINDOW (window), &w, &h);

  if (size && w == -1 && h == -1) {
    GdkScreen *screen;
    int screen_width, screen_height;

    screen = gdk_screen_get_default ();
    screen_width = gdk_screen_get_width (screen);
    screen_height = gdk_screen_get_height (screen);

    gtk_window_set_default_size (GTK_WINDOW (window),
                                 MIN (width, screen_width),
                                 MIN (height, screen_height));
  }

  if (maximize)
    gtk_window_maximize (GTK_WINDOW (window));
}

static void
ephy_state_window_set_position (GtkWidget *window, EphyNode *node)
{
  GdkScreen *screen;
  int x, y;
  int screen_width, screen_height;
  gboolean maximize, size;

  g_return_if_fail (GTK_IS_WINDOW (window));

  /* Setting the default size doesn't work when the window is already showing. */
  g_return_if_fail (!gtk_widget_get_visible (window));

  maximize = ephy_node_get_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_MAXIMIZE);
  size = ephy_node_get_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_POSITION);

  /* Don't set the position of the window if it is maximized */
  if ((!maximize) && size) {
    x = ephy_node_get_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_POSITION_X);
    y = ephy_node_get_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_POSITION_Y);

    screen = gtk_window_get_screen (GTK_WINDOW (window));
    screen_width  = gdk_screen_get_width  (screen);
    screen_height = gdk_screen_get_height (screen);

    if ((x <= screen_width) && (y <= screen_height) &&
        (x >= 0) && (y >= 0))
      gtk_window_move (GTK_WINDOW (window), x, y);
  }
}

static void
ephy_state_save_unmaximized_size (EphyNode *node, int width, int height)
{
  ephy_node_set_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_WIDTH,
                              width);
  ephy_node_set_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_HEIGHT,
                              height);
  ephy_node_set_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_SIZE,
                                  TRUE);
}

static void
ephy_state_save_position (EphyNode *node, int x, int y)
{
  ephy_node_set_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_POSITION_X,
                              x);
  ephy_node_set_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_POSITION_Y,
                              y);
  ephy_node_set_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_POSITION,
                                  TRUE);
}


static void
ephy_state_window_save_size (GtkWidget *window, EphyNode *node)
{
  int width, height;
  gboolean maximize;
  GdkWindowState state;

  state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));
  maximize = ((state & GDK_WINDOW_STATE_MAXIMIZED) > 0);

  gtk_window_get_size (GTK_WINDOW(window),
                       &width, &height);

  if (!maximize)
    ephy_state_save_unmaximized_size (node, width, height);
  
  ephy_node_set_property_boolean (node,
                                  EPHY_NODE_INITIAL_STATE_PROP_MAXIMIZE,
                                  maximize);
}

static void
ephy_state_window_save_position (GtkWidget *window, EphyNode *node)
{
  int x,y;
  gboolean maximize;
  GdkWindowState state;

  state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));
  maximize = ((state & GDK_WINDOW_STATE_MAXIMIZED) > 0);

  /* Don't save the position if maximized. */
  if (!maximize) {
    gtk_window_get_position (GTK_WINDOW (window), &x, &y);
    ephy_state_save_position (node, x, y);
  }
}

static void
ephy_state_window_save (GtkWidget *widget, EphyNode *node)
{
  EphyInitialStateWindowFlags flags;

  flags = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "state_flags"));

  if (flags & EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE)
    ephy_state_window_save_size (widget, node);

  if (flags & EPHY_INITIAL_STATE_WINDOW_SAVE_POSITION)
    ephy_state_window_save_position (widget, node);
}

static gboolean
window_configure_event_cb (GtkWidget *widget,
                           GdkEventConfigure *event,
                           EphyNode *node)
{
  GdkWindowState state;

  state = gdk_window_get_state (gtk_widget_get_window (widget));

  if (!(state & GDK_WINDOW_STATE_FULLSCREEN))
    ephy_state_window_save (widget, node);

  return FALSE;
}

static gboolean
window_state_event_cb (GtkWidget *widget,
                       GdkEventWindowState *event,
                       EphyNode *node)
{
  if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN))
    ephy_state_window_save (widget, node);

  return FALSE;
}

static EphyNode *
create_window_node (const char *name,
                    int default_width,
                    int default_height,
                    gboolean maximize,
                    EphyInitialStateWindowFlags flags)
{
  EphyNode *node;

  node = ephy_node_new (states_db);
  ephy_node_add_child (states, node);

  ephy_node_set_property_string (node, EPHY_NODE_INITIAL_STATE_PROP_NAME,
                                 name);
  ephy_node_set_property_boolean (node, EPHY_NODE_INITIAL_STATE_PROP_MAXIMIZE,
                                  maximize);

  if (flags & EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE) {
    ephy_state_save_unmaximized_size (node,
                                      default_width,
                                      default_height);
  }

  if (flags & EPHY_INITIAL_STATE_WINDOW_SAVE_POSITION) {
    /* Constants for now, these should be default_wi  dth
       and default_height. */
    ephy_state_save_position (node, 0, 0);
  }

  return node;
}

/**
 * ephy_initial_state_add_window:
 * @window: a #GtkWindow
 * @name: the name we'll use to identify this window
 * @default_width: the default width we want to give it
 * @default_height: the default height we want to give it
 * @maximize: whether it should be maximized by default
 * @flags: #EphyInitialStateWindowFlags defining what state we want to saze
 * 
 * This method will set the correct default size and position for
 * @window given the previously stored state information for its type
 * (defined by @name). If there's no data available, the default
 * values passed as parameters will be used. The @flags parameter
 * controls whether we want to track the window's size or position in
 * order to update our default values for this type.
 * 
 **/
void
ephy_initial_state_add_window (GtkWidget *window,
                               const char *name,
                               int default_width,
                               int default_height,
                               gboolean maximize,
                               EphyInitialStateWindowFlags flags)
{
  EphyNode *node;

  g_return_if_fail (GTK_IS_WIDGET (window));
  g_return_if_fail (name != NULL);

  ensure_states ();

  node = find_by_name (name);

  if (node == NULL)
    node = create_window_node (name, default_width, default_height,
                               maximize, flags);

  ephy_state_window_set_size (window, node);
  ephy_state_window_set_position (window, node);

  g_object_set_data (G_OBJECT (window), "state_flags", GINT_TO_POINTER (flags));

  g_signal_connect (window, "configure_event",
                    G_CALLBACK (window_configure_event_cb), node);
  g_signal_connect (window, "window_state_event",
                    G_CALLBACK (window_state_event_cb), node);
}

static gboolean
paned_sync_position_cb (GtkWidget *paned,
                        GParamSpec *pspec,
                        EphyNode *node)
{
  int width;

  width = gtk_paned_get_position (GTK_PANED (paned));
  ephy_node_set_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_WIDTH,
                              width);
  return FALSE;
}

void
ephy_initial_state_add_paned (GtkWidget *paned,
                              const char *name,
                              int default_width)
{
  EphyNode *node;
  int width;

  ensure_states ();

  node = find_by_name (name);

  if (node == NULL) {
    node = ephy_node_new (states_db);
    ephy_node_add_child (states, node);

    ephy_node_set_property_string (node,
                                   EPHY_NODE_INITIAL_STATE_PROP_NAME,
                                   name);
    ephy_node_set_property_int (node,
                                EPHY_NODE_INITIAL_STATE_PROP_WIDTH,
                                default_width);
  }

  width = ephy_node_get_property_int (node, EPHY_NODE_INITIAL_STATE_PROP_WIDTH);
  gtk_paned_set_position (GTK_PANED (paned), width);

  g_signal_connect (paned, "notify::position",
                    G_CALLBACK (paned_sync_position_cb), node);
}

static void
sync_expander_cb (GtkExpander *expander,
                  GParamSpec *pspec,
                  EphyNode *node)
{
  gboolean is_expanded;

  is_expanded = gtk_expander_get_expanded (expander);
  ephy_node_set_property_boolean (node,
                                  EPHY_NODE_INITIAL_STATE_PROP_ACTIVE,
                                  is_expanded);
}

static void
sync_toggle_cb (GtkToggleButton *toggle,
                GParamSpec *pspec,
                EphyNode *node)
{
  gboolean is_active;

  is_active = gtk_toggle_button_get_active (toggle);
  ephy_node_set_property_boolean (node,
                                  EPHY_NODE_INITIAL_STATE_PROP_ACTIVE,
                                  is_active);
}

void 
ephy_initial_state_add_expander (GtkWidget *widget,
                                 const char *name,
                                 gboolean default_state)
{
  EphyNode *node;
  gboolean active;

  ensure_states ();

  node = find_by_name (name);

  if (node == NULL) {
    node = ephy_node_new (states_db);
    ephy_node_add_child (states, node);

    ephy_node_set_property_string (node,
                                   EPHY_NODE_INITIAL_STATE_PROP_NAME,
                                   name);
    ephy_node_set_property_boolean (node,
                                    EPHY_NODE_INITIAL_STATE_PROP_ACTIVE,
                                    default_state);
  }

  active = ephy_node_get_property_boolean
    (node, EPHY_NODE_INITIAL_STATE_PROP_ACTIVE);

  if (GTK_IS_TOGGLE_BUTTON (widget)) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (sync_toggle_cb), node);
  } else if (GTK_IS_EXPANDER (widget)) {
    gtk_expander_set_expanded (GTK_EXPANDER (widget), active);
    g_signal_connect (widget, "notify::expanded",
                      G_CALLBACK (sync_expander_cb), node);
  }
}

void
ephy_initial_state_save (void)
{
  if (states) {
    ephy_states_save ();
    ephy_node_unref (states);
    g_object_unref (states_db);
    states = NULL;
    states_db = NULL;
  }
}
