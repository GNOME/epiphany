/*
 *  Copyright (C) 2002 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright (C) 2005, 2006 Peter A. Harvey
 *  Copyright (C) 2006 Christian Persch
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-bookmark-properties.h"

#include "ephy-bookmarks-ui.h"
#include "ephy-topics-entry.h"
#include "ephy-topics-palette.h"
#include "ephy-stock-icons.h"
#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-dnd.h"

#include <glib/gi18n.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkscrolledwindow.h>

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
	GtkWidget *palette;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK,
	PROP_CREATING
};

static GObjectClass *parent_class;

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
	label = g_strdup_printf (ngettext("%d _Similar", "%d _Similar", priv->duplicate_count), priv->duplicate_count);
	gtk_button_set_label (GTK_BUTTON (priv->warning), label);
	g_free (label);

	g_object_set (priv->warning, "sensitive", priv->duplicate_count > 0, NULL);

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
activate_merge_cb (GtkMenuItem *item,
		   EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	GPtrArray *topics;
	EphyNode *node, *topic;
	gint i, j;

	GPtrArray *identical = g_ptr_array_new ();
	
	ephy_bookmarks_get_similar
	  (priv->bookmarks, priv->bookmark, identical, NULL);
	
	node = ephy_bookmarks_get_keywords (priv->bookmarks);
	topics = ephy_node_get_children (node);

	for (i = 0; i < identical->len; i++)
	{	
		node = g_ptr_array_index (identical, i);
		for (j = 0; j < topics->len; j++)
		{
			topic = g_ptr_array_index (topics, j);

			if (ephy_node_has_child (topic, node))
			{
				ephy_bookmarks_set_keyword
				  (priv->bookmarks, topic, priv->bookmark);
			}
		}
		ephy_node_unref (node);
	}	
	
	g_ptr_array_free (identical, TRUE);
	
	update_warning (properties);
}

static void
activate_show_cb (GtkMenuItem *item,
		  EphyNode *node)
{
	ephy_bookmarks_ui_show_bookmark (node);
}

static void
show_duplicate_cb (GtkButton *button,
		   EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	EphyNode *node;
	GtkMenuShell *menu;
	GtkWidget *item, *image;
	char *label;
	gint i;
	
	GPtrArray *identical = g_ptr_array_new ();
	GPtrArray *similar = g_ptr_array_new ();

	ephy_bookmarks_get_similar (priv->bookmarks,
				    priv->bookmark,
				    identical,
				    similar);
	
	if (identical->len + similar->len > 0)
	{
		menu = GTK_MENU_SHELL (gtk_menu_new ());
		
		if (identical->len > 0)
		{
			label = g_strdup_printf (ngettext ("_Unify With %d Identical Bookmark",
							   "_Unify With %d Identical Bookmarks",
							   identical->len),
						 identical->len);
			item = gtk_image_menu_item_new_with_mnemonic (label);
			g_free (label);
			image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
			g_signal_connect (item, "activate", G_CALLBACK (activate_merge_cb), properties);
			gtk_widget_show (image);
			gtk_widget_show (item);
			gtk_menu_shell_append (menu, item);
	
			item = gtk_separator_menu_item_new ();
			gtk_widget_show (item);
			gtk_menu_shell_append (menu, item);
	
			for (i = 0; i < identical->len; i++)
			{
				node = g_ptr_array_index (identical, i);
				label = g_strdup_printf (_("Show “%s”"),
							 ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_TITLE));
				item = gtk_image_menu_item_new_with_label (label);
				g_free (label);
				g_signal_connect (item, "activate", G_CALLBACK (activate_show_cb), node);
				gtk_widget_show (item);
				gtk_menu_shell_append (menu, item);
			}
		}
		
		if (identical->len > 0 && similar->len > 0)
		{
			item = gtk_separator_menu_item_new ();
			gtk_widget_show (item);
			gtk_menu_shell_append (menu, item);
		}
		
		if (similar->len > 0)
		{
			for (i = 0; i < similar->len; i++)
			{
				node = g_ptr_array_index (similar, i);
				label = g_strdup_printf (_("Show “%s”"),
							 ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_TITLE));
				item = gtk_image_menu_item_new_with_label (label);
				g_free (label);
				g_signal_connect (item, "activate", G_CALLBACK (activate_show_cb), node);
				gtk_widget_show (item);
				gtk_menu_shell_append (menu, item);
			}
		}
		
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				ephy_gui_menu_position_under_widget, button,
				0, gtk_get_current_event_time ());
	}
		
	g_ptr_array_free (similar, TRUE);
	g_ptr_array_free (identical, TRUE);
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
		case GTK_RESPONSE_HELP:
			ephy_gui_help (GTK_WINDOW (dialog),
				       "epiphany", 
				       "to-edit-bookmark-properties");
			return;
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
toggled_cb (GtkToggleButton *button,
	    EphyBookmarkProperties *properties)
{
	EphyBookmarkPropertiesPrivate *priv = properties->priv;
	GdkGeometry geometry;
	
	if(gtk_toggle_button_get_active (button))
	{
		g_object_set (priv->entry, "sensitive", FALSE, NULL);
		gtk_widget_show (priv->palette);
	
		geometry.min_width = -1;
		geometry.min_height = 230;
		gtk_window_set_geometry_hints (GTK_WINDOW (properties),
					       priv->palette, &geometry,
					       GDK_HINT_MIN_SIZE);
	}
	else
	{
		g_object_set (priv->entry, "sensitive", TRUE, NULL);
		gtk_widget_hide (priv->palette);
		
		geometry.max_height = -1;
		geometry.max_width = G_MAXINT;
		gtk_window_set_geometry_hints (GTK_WINDOW (properties),	
					       GTK_WIDGET (properties),
					       &geometry, GDK_HINT_MAX_SIZE);
	}
}


static void
ephy_bookmark_properties_init (EphyBookmarkProperties *properties)
{
	properties->priv = EPHY_BOOKMARK_PROPERTIES_GET_PRIVATE (properties);
}

static GObject *
ephy_bookmark_properties_constructor (GType type,
				      guint n_construct_properties,
				      GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyBookmarkProperties *properties;
	EphyBookmarkPropertiesPrivate *priv;
	GtkWidget *widget, *table, *label, *entry, *button;
	GtkWidget *container, *palette;
	GtkWindow *window;
	GtkDialog *dialog;
	const char *tmp;
	char *text;

	object = parent_class->constructor (type, n_construct_properties,
					    construct_params);

	widget = GTK_WIDGET (object);
	window = GTK_WINDOW (object);
	dialog = GTK_DIALOG (object);
	properties = EPHY_BOOKMARK_PROPERTIES (object);
	priv = properties->priv;

	gtk_window_set_icon_name (window, STOCK_BOOKMARK);
	gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_NORMAL);
	ephy_gui_ensure_window_group (window);

	g_signal_connect (properties, "response",
			  G_CALLBACK (bookmark_properties_response_cb), properties);

	g_signal_connect (properties, "destroy",
			  G_CALLBACK (bookmark_properties_destroy_cb), properties);

	ephy_state_add_window (widget,
			       "bookmark_properties",
			       290, 280, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE |
			       EPHY_STATE_WINDOW_SAVE_POSITION);

	update_window_title (properties);

	gtk_dialog_set_has_separator (dialog, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (properties), 5);
	gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2);

	table = gtk_table_new (4, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_table_set_col_spacing (GTK_TABLE (table), 1, 6);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);
	gtk_widget_show (table);

	entry = gtk_entry_new ();
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
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	entry = gtk_entry_new ();
	tmp = ephy_node_get_property_string (properties->priv->bookmark,
					     EPHY_NODE_BMK_PROP_LOCATION);
	gtk_entry_set_text (GTK_ENTRY (entry), tmp);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (location_entry_changed_cb), properties);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_show (entry);
	label = gtk_label_new_with_mnemonic (_("A_ddress:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	entry = ephy_topics_entry_new (priv->bookmarks, priv->bookmark);
	priv->entry = entry;
	gtk_widget_show (entry);
	label = gtk_label_new_with_mnemonic(_("T_opics:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	palette = ephy_topics_palette_new (priv->bookmarks, priv->bookmark);
	container = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
				  "hadjustment", NULL,
				  "vadjustment", NULL,
				  "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
				  "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
				  "shadow_type", GTK_SHADOW_IN,
				  NULL);
	priv->palette = container;
	gtk_container_add (GTK_CONTAINER (container), palette);
	gtk_widget_show (palette);
	gtk_table_attach (GTK_TABLE (table), container, 1, 3, 3, 4,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show (container);

	widget = gtk_image_new_from_stock (GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (widget);
	button = gtk_toggle_button_new_with_label ("");
	gtk_button_set_image (GTK_BUTTON (button), widget);
	g_signal_connect (button, "toggled", G_CALLBACK (toggled_cb), properties);
	toggled_cb (GTK_TOGGLE_BUTTON (button), properties);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	
	gtk_box_pack_start (GTK_BOX (dialog->vbox), table, TRUE, TRUE, 0);
	
	priv->warning = gtk_button_new ();
	gtk_button_set_use_underline (GTK_BUTTON (priv->warning), TRUE);
	text = g_strdup_printf (ngettext("%d _Similar", "%d _Similar", 0), 0);
	gtk_button_set_label (GTK_BUTTON (priv->warning), text);
	g_free (text);
	widget = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (widget);
	gtk_button_set_image (GTK_BUTTON (priv->warning), widget);
	g_object_set (priv->warning, "sensitive", FALSE, NULL);
	gtk_widget_show (priv->warning);
	g_signal_connect (priv->warning, "clicked",
			  G_CALLBACK(show_duplicate_cb), properties);
		
	gtk_dialog_add_button (dialog,
			       GTK_STOCK_HELP,
			       GTK_RESPONSE_HELP);
	
	gtk_box_pack_end (GTK_BOX (dialog->action_area), 
			  priv->warning, FALSE, TRUE, 0);
	gtk_button_box_set_child_secondary
	  (GTK_BUTTON_BOX (dialog->action_area), priv->warning, TRUE);
	
	if (priv->creating)
	{
		gtk_dialog_add_button (dialog,
				       GTK_STOCK_CANCEL,
				       GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (dialog,
				       GTK_STOCK_ADD,
				       GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
	}
	else
	{
		gtk_dialog_add_button (dialog,
				       GTK_STOCK_CLOSE,
				       GTK_RESPONSE_CLOSE);
		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CLOSE);
	}

	update_warning_idle (properties);
	
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

	parent_class = g_type_class_peek_parent (klass);

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

GType
ephy_bookmark_properties_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyBookmarkPropertiesClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_bookmark_properties_class_init,
			NULL,
			NULL,
			sizeof (EphyBookmarkProperties),
			0,
			(GInstanceInitFunc) ephy_bookmark_properties_init
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "EphyBookmarkProperties",
					       &our_info, 0);
	}

	return type;
}

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
					 NULL));
}

EphyNode *
ephy_bookmark_properties_get_node (EphyBookmarkProperties *properties)
{
	return properties->priv->bookmark;
}
