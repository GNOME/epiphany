/*
 *  Copyright (C) 2001 Matthew Mueller
 *            (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 *	      (C) 2003 Marco Pesenti Gritti <mpeseng@tin.it>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "ephy-state.h"
#include "ephy-file-helpers.h"
#include "ephy-node.h"
#include "ephy-types.h"

#include <string.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkpaned.h>

#define STATES_FILE "states.xml"

enum
{
	EPHY_NODE_STATE_PROP_NAME = 2,
	EPHY_NODE_STATE_PROP_WIDTH = 3,
	EPHY_NODE_STATE_PROP_HEIGHT = 4,
	EPHY_NODE_STATE_PROP_MAXIMIZE = 5
};

static EphyNode *states;

static void
ephy_states_load (void)
{
	xmlDocPtr doc;
	xmlNodePtr root, child;
	char *xml_file;

	xml_file = g_build_filename (ephy_dot_dir (),
                                     STATES_FILE,
                                     NULL);

	if (g_file_test (xml_file, G_FILE_TEST_EXISTS) == FALSE)
		return;

	doc = xmlParseFile (xml_file);
	g_assert (doc != NULL);

	root = xmlDocGetRootElement (doc);

	for (child = root->children; child != NULL; child = child->next)
	{
		EphyNode *node;

		node = ephy_node_new_from_xml (child);
	}

	xmlFreeDoc (doc);

	g_free (xml_file);
}

static void
ephy_states_save (void)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GPtrArray *children;
	int i;
	char *xml_file;

	xml_file = g_build_filename (ephy_dot_dir (),
                                     STATES_FILE,
                                     NULL);

	/* save nodes to xml */
	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "ephy_bookmarks", NULL);
	xmlDocSetRootElement (doc, root);

	children = ephy_node_get_children (states);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		ephy_node_save_to_xml (kid, root);
	}
	ephy_node_thaw (states);

	xmlSaveFormatFile (xml_file, doc, 1);
	g_free (xml_file);
}

static EphyNode *
find_by_name (const char *name)
{
	EphyNode *result = NULL;
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (states);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *node_name;

		kid = g_ptr_array_index (children, i);

		node_name = ephy_node_get_property_string
			(kid, EPHY_NODE_STATE_PROP_NAME);
		if (strcmp (node_name, name) == 0)
		{
			result = kid;
		}
	}
	ephy_node_thaw (states);

	return result;
}

static void
ensure_states (void)
{
	if (states == NULL)
	{
		states = ephy_node_new_with_id (STATES_NODE_ID);
		ephy_states_load ();
	}
}

static void
ephy_state_window_set_size (GtkWidget *window, EphyNode *node)
{
	int width;
	int height;
	gboolean maximize;

	width = ephy_node_get_property_int (node, EPHY_NODE_STATE_PROP_WIDTH);
	height = ephy_node_get_property_int (node, EPHY_NODE_STATE_PROP_HEIGHT);
	maximize = ephy_node_get_property_boolean (node, EPHY_NODE_STATE_PROP_MAXIMIZE);

	if (width > 0 && height > 0)
	{
		gtk_window_set_default_size
			(GTK_WINDOW (window), width, height);
	}

	if (maximize)
	{
		gtk_window_maximize (GTK_WINDOW (window));
	}
}

static void
ephy_state_window_save_size (GtkWidget *window, EphyNode *node)
{
	int width, height;
	gboolean maximize;
	GdkWindowState state;
	GValue value = { 0, };

	state = gdk_window_get_state (GTK_WIDGET (window)->window);
	maximize = ((state & GDK_WINDOW_STATE_MAXIMIZED) > 0);

	gtk_window_get_size (GTK_WINDOW(window),
			     &width, &height);

	if (!maximize)
	{
		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, width);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_WIDTH,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, height);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_HEIGHT,
				        &value);
		g_value_unset (&value);
	}

	g_value_init (&value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&value, maximize);
	ephy_node_set_property (node, EPHY_NODE_STATE_PROP_MAXIMIZE,
			        &value);
	g_value_unset (&value);
}

static gboolean
window_configure_event_cb (GtkWidget *widget,
			   GdkEventConfigure *event,
			   EphyNode *node)
{
	ephy_state_window_save_size (widget, node);
	return FALSE;
}

static gboolean
window_state_event_cb (GtkWidget *widget,
		       GdkEventWindowState *event,
		       EphyNode *node)
{
	ephy_state_window_save_size (widget, node);
	return FALSE;
}

void
ephy_state_add_window (GtkWidget *window,
		       const char *name,
		       int default_width,
		       int default_height)
{
	EphyNode *node;

	ensure_states ();

	node = find_by_name (name);
	if (node == NULL)
	{
		GValue value = { 0, };

		node = ephy_node_new ();
		ephy_node_add_child (states, node);

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, name);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_NAME,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, default_width);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_WIDTH,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, default_height);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_HEIGHT,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_BOOLEAN);
		g_value_set_boolean (&value, FALSE);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_MAXIMIZE,
				        &value);
		g_value_unset (&value);
	}

	ephy_state_window_set_size (window, node);

	g_signal_connect_object (window, "configure_event",
			         G_CALLBACK (window_configure_event_cb), node, 0);
	g_signal_connect_object (window, "window_state_event",
			         G_CALLBACK (window_state_event_cb), node, 0);
}

static gboolean
paned_size_allocate_cb (GtkWidget *paned,
			GtkAllocation *allocation,
			EphyNode *node)
{
	int width;
	GValue value = { 0, };

	width = gtk_paned_get_position (GTK_PANED (paned));

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, width);
	ephy_node_set_property (node, EPHY_NODE_STATE_PROP_WIDTH,
			        &value);
	g_value_unset (&value);

	return FALSE;
}

void
ephy_state_add_paned (GtkWidget *paned,
		      const char *name,
		      int default_width)
{
	EphyNode *node;
	int width;

	ensure_states ();

	node = find_by_name (name);
	if (node == NULL)
	{
		GValue value = { 0, };

		node = ephy_node_new ();
		ephy_node_add_child (states, node);

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, name);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_NAME,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, default_width);
		ephy_node_set_property (node, EPHY_NODE_STATE_PROP_WIDTH,
				        &value);
		g_value_unset (&value);
	}

	width = ephy_node_get_property_int (node, EPHY_NODE_STATE_PROP_WIDTH);
	gtk_paned_set_position (GTK_PANED (paned), width);

	g_signal_connect_object (paned, "size_allocate",
			         G_CALLBACK (paned_size_allocate_cb), node, 0);
}

void
ephy_state_save (void)
{
	ephy_states_save ();
	ephy_node_unref (states);
	states = NULL;
}
