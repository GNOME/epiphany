/*
 *  Copyright © 2002 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2005, 2006 Peter A. Harvey
 *  Copyright © 2006 Christian Persch
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

#include "ephy-bookmark-properties.h"

#include "ephy-bookmarks-ui.h"
#include "ephy-topics-entry.h"
#include "ephy-topics-palette.h"
#include "ephy-node-common.h"
#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-initial-state.h"
#include "ephy-gui.h"
#include "ephy-dnd.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <string.h>

struct _EphyBookmarkProperties
{
	GtkDialog parent_instance;

	/* construct properties */
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	gboolean creating;

	/* counters */
	gint duplicate_count;
	gint duplicate_idle;

	/* from UI file */
	GtkGrid           *grid;
	GtkEntry          *title_entry;
	GtkEntry          *adress_entry;
	GtkLabel          *topics_label;
	GtkExpander       *topics_expander;
	GtkTreeView       *topics_tree_view;
	GtkScrolledWindow *topics_scrolled_window;
	GtkLabel          *warning_label;

	/* model */
	EphyTopicsPalette *topics_model;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK,
	PROP_CREATING,
	LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyBookmarkProperties, ephy_bookmark_properties, GTK_TYPE_DIALOG)

static gboolean
update_warning (EphyBookmarkProperties *properties)
{
	char *label;

	properties->duplicate_idle = 0;
	properties->duplicate_count = ephy_bookmarks_get_similar
	  (properties->bookmarks, properties->bookmark, NULL, NULL);

        /* Translators: This string is used when counting bookmarks that
         * are similar to each other */
	label = g_strdup_printf (ngettext("%d bookmark is similar", "%d bookmarks are similar", properties->duplicate_count), properties->duplicate_count);
	gtk_label_set_text (properties->warning_label, label);
	g_free (label);

	return FALSE;
}

static void
update_warning_idle (EphyBookmarkProperties *properties)
{
	if (properties->duplicate_idle != 0)
	{
		g_source_remove (properties->duplicate_idle);
	}

	properties->duplicate_idle = g_timeout_add
	  (500, (GSourceFunc)update_warning, properties);
	g_source_set_name_by_id (properties->duplicate_idle, "[epiphany] update_warning");
}

static void
node_added_cb (EphyNode *bookmarks,
	       EphyNode *bookmark,
	       EphyBookmarkProperties *properties)
{
	update_warning_idle (properties);
}

static void
node_changed_cb (EphyNode *bookmarks,
		 EphyNode *bookmark,
		 guint property,
		 EphyBookmarkProperties *properties)
{
	if (property == EPHY_NODE_BMK_PROP_LOCATION) 
	{
		update_warning_idle (properties);
	}
}

static void
node_removed_cb (EphyNode *bookmarks,
		 EphyNode *bookmark,
		 guint index,
		 EphyBookmarkProperties *properties)
{
	update_warning_idle (properties);
}

static void
node_destroy_cb (EphyNode *bookmark,
		 GtkWidget *dialog)
{
	EPHY_BOOKMARK_PROPERTIES (dialog)->creating = FALSE;
	gtk_widget_destroy (dialog);
}

static void
ephy_bookmark_properties_set_bookmark (EphyBookmarkProperties *properties,
				       EphyNode *bookmark)
{
	LOG ("Set bookmark");

	if (properties->bookmark)
	{
		ephy_node_signal_disconnect_object (properties->bookmark,
						    EPHY_NODE_DESTROY,
						    (EphyNodeCallback) node_destroy_cb,
						    G_OBJECT (properties));
	}

	properties->bookmark = bookmark;

	ephy_node_signal_connect_object (properties->bookmark,
					 EPHY_NODE_DESTROY,
					 (EphyNodeCallback) node_destroy_cb,
					 G_OBJECT (properties));
}

static void
ephy_bookmark_properties_destroy_cb (GtkDialog *dialog,
                                     gpointer   data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);

	if (properties->creating)
	{
		ephy_node_unref (properties->bookmark);
		properties->creating = FALSE;
	}

	if (properties->duplicate_idle != 0)
	{
		g_source_remove (properties->duplicate_idle);
		properties->duplicate_idle = 0;
	}
}

static void
ephy_bookmark_properties_response_cb (GtkDialog *dialog,
                                      int        response_id,
                                      gpointer   data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);

	switch (response_id)
	{
	 	case GTK_RESPONSE_ACCEPT:
			properties->creating = FALSE;
			break;
	 	default:
			break;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
update_entry (EphyBookmarkProperties *properties,
	      GtkWidget *entry,
	      guint prop)
{
	GValue value = { 0, };
	char *text;

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	g_value_init (&value, G_TYPE_STRING);
	g_value_take_string (&value, text);
	ephy_node_set_property (properties->bookmark,
				prop,
				&value);
	g_value_unset (&value);
}

static void
update_window_title (EphyBookmarkProperties *properties)
{
	char *title;
	const char *tmp;

	tmp = ephy_node_get_property_string (properties->bookmark,
					     EPHY_NODE_BMK_PROP_TITLE);

	if (properties->creating)
		title = g_strdup (_("Add Bookmark"));
	else
		title = g_strdup_printf (_("“%s” Properties"), tmp);

	gtk_window_set_title (GTK_WINDOW (properties), title);
	g_free (title);
}

static void
title_entry_changed_cb (GtkWidget              *entry,
                        EphyBookmarkProperties *properties)
{
	update_entry (properties, entry, EPHY_NODE_BMK_PROP_TITLE);
	update_window_title (properties);
}

static void
on_topic_edited (GtkCellRendererText    *renderer,
                 const char             *path_str,
                 const char             *new_text,
                 EphyBookmarkProperties *properties)
{
	if (*new_text != 0)
	{
		EphyNode *node;
		node = ephy_bookmarks_add_keyword (properties->bookmarks, new_text);
		ephy_bookmarks_set_keyword (properties->bookmarks, node, properties->bookmark);
	}
	else
	{
		ephy_topics_palette_update_list (properties->topics_model);
	}
}

static void
on_topic_toggled (GtkCellRendererToggle  *cell_renderer,
                  gchar                  *path,
                  EphyBookmarkProperties *properties)
{
	EphyNode     *topic;
	GtkTreeModel *model;
	GtkTreeIter   iter;

	model = gtk_tree_view_get_model (properties->topics_tree_view);

	g_return_if_fail (gtk_tree_model_get_iter_from_string (model, &iter, path));

	gtk_tree_model_get (model, &iter, EPHY_TOPICS_PALETTE_COLUMN_NODE, &topic, -1);

	/* Protect against toggling separators. */
	if (topic == NULL)
		return;

	if (ephy_node_has_child (topic, properties->bookmark))
	{
		ephy_bookmarks_unset_keyword (properties->bookmarks,
		                              topic,
		                              properties->bookmark);
	}
	else
	{
		ephy_bookmarks_set_keyword (properties->bookmarks,
		                            topic,
		                            properties->bookmark);
	}
}

static gboolean
is_separator (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      data)
{
	EphyNode *node;
	gtk_tree_model_get (model, iter, EPHY_TOPICS_PALETTE_COLUMN_NODE, &node, -1);
	return (node == NULL);
}

static void
location_entry_changed_cb (GtkWidget *entry,
			   EphyBookmarkProperties *properties)
{
	ephy_bookmarks_set_address (properties->bookmarks,
				    properties->bookmark,
				    gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
ephy_bookmark_properties_init (EphyBookmarkProperties *properties)
{
}

static GObject *
ephy_bookmark_properties_constructor (GType                  type,
                                      guint                  n_construct_properties,
                                      GObjectConstructParam *construct_params)
{
	GObject                *object;
	EphyBookmarkProperties *properties;

	gboolean      lockdown;
	const char   *tmp;
	char         *unescaped_url;
	GtkWidget    *entry;

	object = G_OBJECT_CLASS (ephy_bookmark_properties_parent_class)->constructor (type,
                                                                                      n_construct_properties,
                                                                                      construct_params);
	properties = EPHY_BOOKMARK_PROPERTIES (object);

	gtk_widget_init_template (GTK_WIDGET (properties));

	if (!properties->creating)
	{
		ephy_initial_state_add_window (GTK_WIDGET (properties),
		                               "bookmark_properties",
		                               290, 280, FALSE,
		                               EPHY_INITIAL_STATE_WINDOW_SAVE_POSITION |
		                               EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE);
	}
	/* Lockdown */
	lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
	                                   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING);

	update_window_title (properties);

	gtk_editable_set_editable (GTK_EDITABLE (properties->title_entry), !lockdown);
	tmp = ephy_node_get_property_string (properties->bookmark, EPHY_NODE_BMK_PROP_TITLE);
	gtk_entry_set_text (properties->title_entry, tmp);

	gtk_editable_set_editable (GTK_EDITABLE (properties->adress_entry), !lockdown);
	tmp = ephy_node_get_property_string (properties->bookmark, EPHY_NODE_BMK_PROP_LOCATION);
	unescaped_url = ephy_uri_safe_unescape (tmp);
	gtk_entry_set_text (properties->adress_entry, unescaped_url);
	g_free (unescaped_url);

	entry = ephy_topics_entry_new (properties->bookmarks, properties->bookmark);
	gtk_editable_set_editable (GTK_EDITABLE (entry), !lockdown);
	gtk_label_set_mnemonic_widget (properties->topics_label, entry);
	gtk_widget_show (entry);
	gtk_grid_attach (properties->grid, entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand (entry, TRUE);

	gtk_tree_view_set_row_separator_func (properties->topics_tree_view, is_separator, NULL, NULL);

	properties->topics_model = ephy_topics_palette_new (properties->bookmarks, properties->bookmark);
	gtk_tree_view_set_model (properties->topics_tree_view, GTK_TREE_MODEL (properties->topics_model));

	/* TODO bind; and the entry?! */
	gtk_widget_set_sensitive (GTK_WIDGET (properties->topics_scrolled_window), !lockdown);

	ephy_initial_state_add_expander (GTK_WIDGET (properties->topics_expander), "bookmark_properties_list", FALSE);

	if (properties->creating)
	{
		gtk_dialog_add_button (GTK_DIALOG (properties), _("_Cancel"), GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (properties), _("_Add"), GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response (GTK_DIALOG (properties), GTK_RESPONSE_ACCEPT);
	}

	update_warning (properties);
	
	return object;
}

static void
ephy_bookmark_properties_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (object);
	EphyNode *bookmarks;

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			properties->bookmarks = g_value_get_object (value);
			bookmarks = ephy_bookmarks_get_bookmarks (properties->bookmarks);
			ephy_node_signal_connect_object (bookmarks,
							 EPHY_NODE_CHILD_ADDED,
							 (EphyNodeCallback) node_added_cb,
							 object);
			ephy_node_signal_connect_object (bookmarks,
							 EPHY_NODE_CHILD_REMOVED,
							 (EphyNodeCallback) node_removed_cb,
							 object);
			ephy_node_signal_connect_object (bookmarks,
							 EPHY_NODE_CHILD_CHANGED,
							 (EphyNodeCallback) node_changed_cb,
							 object);
			break;
		case PROP_BOOKMARK:
			ephy_bookmark_properties_set_bookmark
				(properties, g_value_get_pointer (value));
			break;
		case PROP_CREATING:
			properties->creating = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ephy_bookmark_properties_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (object);

	switch (prop_id)
	{
		case PROP_BOOKMARK:
			g_value_set_object (value, properties);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ephy_bookmark_properties_class_init (EphyBookmarkPropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructor = ephy_bookmark_properties_constructor;
	object_class->set_property = ephy_bookmark_properties_set_property;
	object_class->get_property = ephy_bookmark_properties_get_property;

	obj_properties[PROP_BOOKMARKS] =
		g_param_spec_object ("bookmarks",
		                     "bookmarks",
		                     "bookmarks",
		                     EPHY_TYPE_BOOKMARKS,
		                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

	obj_properties[PROP_BOOKMARK] =
		g_param_spec_pointer ("bookmark",
		                      "bookmark",
		                      "bookmark",
		                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

	obj_properties[PROP_CREATING] =
		g_param_spec_boolean ("creating",
		                      "creating",
		                      "creating",
		                      FALSE,
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

	/* from UI file */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/bookmark-properties.ui");

	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, grid);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, title_entry);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, adress_entry);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, topics_label);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, topics_expander);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, topics_tree_view);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, topics_scrolled_window);
	gtk_widget_class_bind_template_child (widget_class, EphyBookmarkProperties, warning_label);

	gtk_widget_class_bind_template_callback (widget_class, title_entry_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, on_topic_toggled);   // TODO make the row activatable instead of a little togglebutton
	gtk_widget_class_bind_template_callback (widget_class, on_topic_edited);    // TODO topics’ names are not editable from there, they are synced with bookmarks... but this is used for... updating the entry \o/ yay \o/
	gtk_widget_class_bind_template_callback (widget_class, location_entry_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, ephy_bookmark_properties_response_cb);
	gtk_widget_class_bind_template_callback (widget_class, ephy_bookmark_properties_destroy_cb);
}

/* public API */

GtkWidget *
ephy_bookmark_properties_new (EphyBookmarks *bookmarks,
			      EphyNode *bookmark,
			      gboolean creating)
{
	g_assert (bookmarks != NULL);

	return GTK_WIDGET (g_object_new	(EPHY_TYPE_BOOKMARK_PROPERTIES,
			   		 "bookmarks", bookmarks,
					 "bookmark", bookmark,
					 "creating", creating,
					 "use-header-bar", TRUE,
					 NULL));
}

EphyNode *
ephy_bookmark_properties_get_node (EphyBookmarkProperties *properties)
{
	return properties->bookmark;
}
