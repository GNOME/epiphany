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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
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

static const GtkTargetEntry dest_drag_types[] = {
  {EPHY_DND_URL_TYPE, 0, 0},
};

#define EPHY_BOOKMARK_PROPERTIES_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARK_PROPERTIES, EphyBookmarkPropertiesPrivate))

struct _EphyBookmarkPropertiesPrivate
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	gboolean creating;
	
	gint duplicate_count;
	gint duplicate_idle;
	
	GtkWidget *warning;
	GtkWidget *entry;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK,
	PROP_CREATING
};

G_DEFINE_TYPE (EphyBookmarkProperties, ephy_bookmark_properties, GTK_TYPE_DIALOG)

static gboolean
update_warning (EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	char *label;

	priv->duplicate_idle = 0;	
	priv->duplicate_count = ephy_bookmarks_get_similar
	  (priv->bookmarks, priv->bookmark, NULL, NULL);
	
        /* Translators: This string is used when counting bookmarks that
         * are similar to each other */
	label = g_strdup_printf (ngettext("%d bookmark is similar", "%d bookmarks are similar", priv->duplicate_count), priv->duplicate_count);
	gtk_label_set_text (GTK_LABEL (priv->warning), label);
	g_free (label);

	return FALSE;
}

static void
update_warning_idle (EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	
	if(priv->duplicate_idle != 0)
	{
		g_source_remove (priv->duplicate_idle);
	}
	
	priv->duplicate_idle = g_timeout_add 
	  (500, (GSourceFunc)update_warning, properties);
	g_source_set_name_by_id (priv->duplicate_idle, "[epiphany] update_warning");
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
	EPHY_BOOKMARK_PROPERTIES (dialog)->priv->creating = FALSE;
	gtk_widget_destroy (dialog);
}

static void
ephy_bookmark_properties_set_bookmark (EphyBookmarkProperties *properties,
				       EphyNode *bookmark)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;

	LOG ("Set bookmark");
	
	if (priv->bookmark)
	{
		ephy_node_signal_disconnect_object (priv->bookmark,
						    EPHY_NODE_DESTROY,
						    (EphyNodeCallback) node_destroy_cb,
						    G_OBJECT (properties));
	}

	priv->bookmark = bookmark;

	ephy_node_signal_connect_object (priv->bookmark,
					 EPHY_NODE_DESTROY,
					 (EphyNodeCallback) node_destroy_cb,
					 G_OBJECT (properties));
}

static void
bookmark_properties_destroy_cb (GtkDialog *dialog,
				gpointer data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);
	EphyBookmarkPropertiesPrivate *priv = properties->priv;

	if (priv->creating)
	{
		ephy_node_unref (priv->bookmark);
		priv->creating = FALSE;
	}
	
	if(priv->duplicate_idle != 0)
	{
		g_source_remove (priv->duplicate_idle);
		priv->duplicate_idle = 0;
	}
}

static void
bookmark_properties_response_cb (GtkDialog *dialog,
				 int response_id,
				 gpointer data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);
	EphyBookmarkPropertiesPrivate *priv = properties->priv;

	switch (response_id)
	{
	 	case GTK_RESPONSE_ACCEPT:
			priv->creating = FALSE;
			break;
	 	default:
			break;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
update_entry (EphyBookmarkProperties *props,
	      GtkWidget *entry,
	      guint prop)
{
	GValue value = { 0, };
	char *text;

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	g_value_init (&value, G_TYPE_STRING);
	g_value_take_string (&value, text);
	ephy_node_set_property (props->priv->bookmark,
				prop,
				&value);
	g_value_unset (&value);
}

static void
update_window_title (EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	char *title;
	const char *tmp;

	tmp = ephy_node_get_property_string (priv->bookmark,
					     EPHY_NODE_BMK_PROP_TITLE);

	if (priv->creating)
		title = g_strdup (_("Add Bookmark"));
	else
		title = g_strdup_printf (_("“%s” Properties"), tmp);

	gtk_window_set_title (GTK_WINDOW (properties), title);
	g_free (title);
}

static void
title_entry_changed_cb (GtkWidget *entry,
			EphyBookmarkProperties *props)
{
	update_entry (props, entry, EPHY_NODE_BMK_PROP_TITLE);
	update_window_title (props);
}

static void
location_entry_changed_cb (GtkWidget *entry,
			   EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;

	ephy_bookmarks_set_address (priv->bookmarks,
				    priv->bookmark,
				    gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
list_mapped_cb (GtkWidget *widget,
		 EphyBookmarkProperties *properties)
{
	GdkGeometry geometry;
	
	geometry.min_width = -1;
	geometry.min_height = 230;
	gtk_window_set_geometry_hints (GTK_WINDOW (properties),
				       widget, &geometry,
				       GDK_HINT_MIN_SIZE);
}

static void
list_unmapped_cb (GtkWidget *widget,
		 EphyBookmarkProperties *properties)
{
	GdkGeometry geometry;
	
	geometry.max_height = -1;
	geometry.max_width = G_MAXINT;
	gtk_window_set_geometry_hints (GTK_WINDOW (properties),	
				       GTK_WIDGET (properties),
				       &geometry, GDK_HINT_MAX_SIZE);
}

static void
ephy_bookmark_properties_init (EphyBookmarkProperties *properties)
{
	properties->priv = EPHY_BOOKMARK_PROPERTIES_GET_PRIVATE (properties);
	gtk_window_set_modal (GTK_WINDOW (properties), TRUE);
}

static GObject *
ephy_bookmark_properties_constructor (GType type,
				      guint n_construct_properties,
				      GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyBookmarkProperties *properties;
	EphyBookmarkPropertiesPrivate *priv;
	GtkWidget *widget, *grid, *label, *entry, *container;
	GtkWidget *content_area;
	GtkWindow *window;
	GtkDialog *dialog;
	gboolean lockdown;
	const char *tmp;
	char *unescaped_url;

	object = G_OBJECT_CLASS (ephy_bookmark_properties_parent_class)->constructor (type,
                                                                                      n_construct_properties,
                                                                                      construct_params);

	widget = GTK_WIDGET (object);
	window = GTK_WINDOW (object);
	dialog = GTK_DIALOG (object);
	properties = EPHY_BOOKMARK_PROPERTIES (object);
	priv = properties->priv;

	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);

	g_signal_connect (properties, "response",
			  G_CALLBACK (bookmark_properties_response_cb), properties);

	g_signal_connect (properties, "destroy",
			  G_CALLBACK (bookmark_properties_destroy_cb), properties);

	if (!priv->creating)
	{
		ephy_initial_state_add_window (widget,
                                               "bookmark_properties",
                                               290, 280, FALSE,
                                               EPHY_INITIAL_STATE_WINDOW_SAVE_POSITION |
                                               EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE);
	}
	/* Lockdown */
	lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
					   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING);

	update_window_title (properties);
	content_area = gtk_dialog_get_content_area (dialog);

	gtk_container_set_border_width (GTK_CONTAINER (properties), 5);
	gtk_box_set_spacing (GTK_BOX (content_area), 2);

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
	gtk_widget_show (grid);

	entry = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (entry), !lockdown);
	tmp = ephy_node_get_property_string (properties->priv->bookmark,
					     EPHY_NODE_BMK_PROP_TITLE);
	gtk_entry_set_text (GTK_ENTRY (entry), tmp);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (title_entry_changed_cb), properties);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_set_size_request (entry, 200, -1);
	gtk_widget_show (entry);
	label = gtk_label_new_with_mnemonic (_("_Title:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand (entry, TRUE);

	entry = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (entry), !lockdown);
	tmp = ephy_node_get_property_string (properties->priv->bookmark,
					     EPHY_NODE_BMK_PROP_LOCATION);
	unescaped_url = ephy_uri_safe_unescape (tmp);
	gtk_entry_set_text (GTK_ENTRY (entry), unescaped_url);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (location_entry_changed_cb), properties);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_show (entry);
	label = gtk_label_new_with_mnemonic (_("A_ddress:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand (entry, TRUE);
	g_free (unescaped_url);

	entry = ephy_topics_entry_new (priv->bookmarks, priv->bookmark);
	gtk_editable_set_editable (GTK_EDITABLE (entry), !lockdown);
	priv->entry = entry;
	gtk_widget_show (entry);
	label = gtk_label_new_with_mnemonic(_("T_opics:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand (entry, TRUE);

	widget = ephy_topics_palette_new (priv->bookmarks, priv->bookmark);
	container = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
				  "hadjustment", NULL,
				  "vadjustment", NULL,
				  "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				  "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				  "shadow_type", GTK_SHADOW_IN,
				  NULL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);
	gtk_widget_set_sensitive (container, !lockdown);
	gtk_widget_show (container);
	g_signal_connect (container, "map", G_CALLBACK (list_mapped_cb), properties);
	g_signal_connect (container, "unmap", G_CALLBACK (list_unmapped_cb), properties);

	widget = gtk_expander_new (_("Sho_w all topics"));
	gtk_expander_set_use_underline (GTK_EXPANDER (widget), TRUE);
	ephy_initial_state_add_expander (widget, "bookmark_properties_list", FALSE);
	gtk_container_add (GTK_CONTAINER (widget), container);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (grid), widget, 1, 3, 1, 1);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);

	priv->warning = gtk_label_new (NULL);
	gtk_grid_attach (GTK_GRID (grid), priv->warning, 0, 4, 2, 1);
	gtk_widget_show (priv->warning);

	gtk_box_pack_start (GTK_BOX (content_area), grid, TRUE, TRUE, 0);

	if (priv->creating)
	{
		gtk_dialog_add_button (dialog,
				       _("_Cancel"),
				       GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (dialog,
				       _("_Add"),
				       GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
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
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	EphyNode *bookmarks;

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			priv->bookmarks = g_value_get_object (value);
			bookmarks = ephy_bookmarks_get_bookmarks (priv->bookmarks);
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
			priv->creating = g_value_get_boolean (value);
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

	object_class->constructor = ephy_bookmark_properties_constructor;
	object_class->set_property = ephy_bookmark_properties_set_property;
	object_class->get_property = ephy_bookmark_properties_get_property;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "bookmarks",
							      "bookmarks",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "bookmark",
							       "bookmark",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_CREATING,
					 g_param_spec_boolean ("creating",
							       "creating",
							       "creating",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyBookmarkPropertiesPrivate));
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
	return properties->priv->bookmark;
}
