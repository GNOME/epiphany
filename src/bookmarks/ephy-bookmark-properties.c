/*
 *  Copyright (C) 2002 Marco Pesenti Gritti <mpeseng@tin.it>
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
#include "ephy-topics-palette.h"
#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-dnd.h"
#include "ephy-favicon-cache.h"

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkscrolledwindow.h>
#include <glib/gi18n.h>
#include <string.h>

enum
{
	LIST_SEPARATOR,
	LIST_ICON,
	LIST_FROMFILE
};

static const GtkTargetEntry dest_drag_types[] = {
  {EPHY_DND_URL_TYPE, 0, 0},
};

static void ephy_bookmark_properties_class_init (EphyBookmarkPropertiesClass *klass);
static void ephy_bookmark_properties_init (EphyBookmarkProperties *properties);
static void ephy_bookmark_properties_set_property (GObject *object,
					       guint prop_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void ephy_bookmark_properties_get_property (GObject *object,
					       guint prop_id,
					       GValue *value,
					       GParamSpec *pspec);

#define EPHY_BOOKMARK_PROPERTIES_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARK_PROPERTIES, EphyBookmarkPropertiesPrivate))

struct _EphyBookmarkPropertiesPrivate
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	EphyTopicsPalette *palette;
	gboolean creating;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK,
	PROP_CREATING
};

enum
{
	RESPONSE_NEW_TOPIC
};

static GObjectClass *parent_class = NULL;

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

static void
ephy_bookmark_properties_class_init (EphyBookmarkPropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_bookmark_properties_set_property;
	object_class->get_property = ephy_bookmark_properties_get_property;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "Bookmark",
							       "Bookmark",
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_CREATING,
					 g_param_spec_boolean ("creating",
							       "New bookmark",
							       "New bookmark",
							       FALSE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyBookmarkPropertiesPrivate));
}

static void
node_destroy_cb (EphyNode *bookmark,
		 GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

static void
ephy_bookmark_properties_set_bookmark (EphyBookmarkProperties *properties,
				       EphyNode *bookmark)
{
	LOG ("Set bookmark");
	
	if (properties->priv->bookmark)
	{
		ephy_node_signal_disconnect_object (properties->priv->bookmark,
						    EPHY_NODE_DESTROY,
						    (EphyNodeCallback) node_destroy_cb,
						    G_OBJECT (properties));
	}

	properties->priv->bookmark = bookmark;

	ephy_node_signal_connect_object (properties->priv->bookmark,
					 EPHY_NODE_DESTROY,
					 (EphyNodeCallback) node_destroy_cb,
					 G_OBJECT (properties));

	g_object_notify (G_OBJECT (properties), "bookmark");
}

static void
ephy_bookmark_properties_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		properties->priv->bookmarks = g_value_get_object (value);
		break;
	case PROP_BOOKMARK:
		ephy_bookmark_properties_set_bookmark
			(properties, g_value_get_pointer (value));
		break;
	 case PROP_CREATING:
		properties->priv->creating = g_value_get_boolean (value);
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
bookmark_properties_close_cb (GtkDialog *dialog,
			      gpointer data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);
	if (properties->priv->creating)
	{
		ephy_node_unref (properties->priv->bookmark);
	}
}

static void
bookmark_properties_response_cb (GtkDialog *dialog,
				 int response_id,
				 gpointer data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);
	switch (response_id)
	{
		case GTK_RESPONSE_HELP:
			ephy_gui_help (GTK_WINDOW (dialog),
				       "epiphany", 
				       "to-edit-bookmark-properties");
			return;
		case RESPONSE_NEW_TOPIC:
			ephy_bookmarks_ui_add_topic (GTK_WIDGET (dialog),
						     properties->priv->bookmark);
			return;
		case GTK_RESPONSE_CANCEL:
			ephy_node_unref (properties->priv->bookmark);
			break;
	 	default:
			break;
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
update_entry (EphyBookmarkProperties *props, GtkWidget *entry, guint prop)
{
	GValue value = { 0, };
	char *text;

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, text);
	ephy_node_set_property (props->priv->bookmark,
				prop,
				&value);
	g_value_unset (&value);
	g_free (text);
}

static void
update_window_title (EphyBookmarkProperties *properties)
{
	char *title;
	const char *tmp;

	tmp = ephy_node_get_property_string (properties->priv->bookmark,
					     EPHY_NODE_BMK_PROP_TITLE);
	title = g_strdup_printf (_("“%s” Properties"), tmp);
	gtk_window_set_title (GTK_WINDOW (properties), title);
	g_free (title);
}


static void 
combo_changed_cb (GtkComboBox *combobox, GtkWidget *palette)
{
	int active = gtk_combo_box_get_active (GTK_COMBO_BOX (combobox));
	g_object_set (palette, "mode", active, NULL);
}

static void
title_entry_changed_cb (GtkWidget *entry, EphyBookmarkProperties *props)
{
	update_entry (props, entry, EPHY_NODE_BMK_PROP_TITLE);
	update_window_title(props);
}

static void
location_entry_changed_cb (GtkWidget *entry, EphyBookmarkProperties *props)
{
	ephy_bookmarks_set_address (props->priv->bookmarks,
				    props->priv->bookmark,
				    gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
set_window_icon (EphyBookmarkProperties *properties)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *icon = NULL;

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (embed_shell));
	icon_location = ephy_node_get_property_string
		(properties->priv->bookmark, EPHY_NODE_BMK_PROP_ICON);

	LOG ("Get favicon for %s", icon_location ? icon_location : "None");

	if (icon_location != NULL)
	{
		icon = ephy_favicon_cache_get (cache, icon_location);
	}

	if (icon != NULL)
	{
		gtk_window_set_icon (GTK_WINDOW (properties), icon);
		g_object_unref (icon);
	}
	else
	{
		gtk_window_set_icon_name (GTK_WINDOW (properties),
					  GTK_STOCK_PROPERTIES);
	}
}

static void
refresh_icon (GtkComboBox *combobox, const char *location, gboolean set, gboolean create)
{
	EphyFaviconCache *cache;
	GtkListStore *store;
	GtkTreeIter iter;
	GdkPixbuf *icon;
	gboolean valid;
	const char *filename;
	
	store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
	while (valid)
	{
		int type;
		char *tmp;
		
		// Check that this is really an icon
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 3, &type, -1);
		if (type == LIST_ICON)
		{
			// Check if it's the icon we're looking for
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &tmp, -1);
			if (location == tmp || (tmp != NULL && location != NULL && strcmp (location, tmp) == 0))
			{
				g_free (tmp);
				break;
			}
			g_free (tmp);
		}
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}
	
	if (!valid && !create) return;
	
	cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache (embed_shell));
	icon = (location && *location) ? ephy_favicon_cache_get (cache, location) : NULL;
	
	if (!valid)
	{
		filename = g_strrstr (location, "/");
		filename = filename ? (filename+1) : "";
		filename = *filename ? filename : location;
		gtk_list_store_insert_with_values (store, &iter, 0, 0, location, 1, filename, 2, icon, 3, LIST_ICON, -1);
	}
	else
	{
		gtk_list_store_set (store, &iter, 2, icon, -1);
	}
	
	if (set)
	{
		gtk_combo_box_set_active_iter (combobox, &iter);
	}
}

static void
icon_changed_cb (GtkComboBox *combobox, 
		 EphyBookmarkProperties *properties)
{
	GValue value = { 0, };
	GtkTreeIter iter;
	int type;
	char *location;
	
	gtk_combo_box_get_active_iter (combobox, &iter);
	gtk_tree_model_get (gtk_combo_box_get_model (combobox), &iter, 0, &location, 3, &type, -1);

	if (type == LIST_ICON)
	{
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, location);
		ephy_node_set_property (properties->priv->bookmark,
					EPHY_NODE_BMK_PROP_USERICON,
					&value);
		g_value_unset (&value);
	}
	else if (type == LIST_FROMFILE)
	{
		GtkWidget *widget = gtk_file_chooser_dialog_new
		  (_("Open Icon"), GTK_WINDOW (properties), GTK_FILE_CHOOSER_ACTION_OPEN,
		   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		gtk_widget_show (widget);
	}
	
	g_free (location);

        refresh_icon (GTK_COMBO_BOX (combobox), 
	   ephy_node_get_property_string (properties->priv->bookmark, EPHY_NODE_BMK_PROP_USERICON),
	   TRUE, TRUE);
}

static void
icon_drag_data_received_cb (GObject            *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    GtkSelectionData   *selection_data,
			    guint               info,
			    guint               time,
			    gpointer            user)
{  
	gchar **netscape_url;
	
	netscape_url = g_strsplit ((char *)selection_data->data, "\n", 2);
	if (!netscape_url || !netscape_url[0])
	{
		g_strfreev (netscape_url);
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}
	
	refresh_icon (GTK_COMBO_BOX (widget), netscape_url[0], TRUE, TRUE);
	g_strfreev (netscape_url);
	gtk_drag_finish (context, TRUE, TRUE, time);
}

static void
icon_cache_changed (EphyFaviconCache *cache, const char *url, GtkComboBox *combobox)
{
	refresh_icon (combobox, url, FALSE, FALSE);
}


static gboolean
is_separator (GtkTreeModel *model,
	      GtkTreeIter *iter,
	      gpointer data)
{
	int type;
	gtk_tree_model_get (model, iter, 3, &type, -1);
	return (type == LIST_SEPARATOR);
}

static GtkWidget *
build_icon (EphyBookmarkProperties *properties)
{
	EphyFaviconCache *cache;
	GtkWidget *combobox;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellLayout *layout;
	GtkCellRenderer *renderer;
	const char *location;
	
	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_INT);
	combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combobox), is_separator, NULL, NULL);
	layout = GTK_CELL_LAYOUT (combobox);

	cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache (embed_shell));
	g_signal_connect (cache, "changed", G_CALLBACK (icon_cache_changed), combobox);
	
	gtk_list_store_insert_with_values (store, &iter, 0, 0, NULL, 1, _("From file..."), 2, NULL, 3, LIST_FROMFILE, -1);
	gtk_list_store_insert_with_values (store, &iter, 0, 0, NULL, 1, NULL, 2, NULL, 3, LIST_SEPARATOR, -1);
	gtk_list_store_insert_with_values (store, &iter, 0, 0, "", 1, _("None"), 2, NULL, 3, LIST_ICON, -1);	
	gtk_list_store_insert_with_values (store, &iter, 0, 0, NULL, 1, _("Default"), 2, NULL, 3, LIST_ICON, -1);
	
	location = ephy_node_get_property_string (properties->priv->bookmark, EPHY_NODE_BMK_PROP_ICON);
        refresh_icon (GTK_COMBO_BOX (combobox), location, FALSE, TRUE);
	
	location = ephy_node_get_property_string (properties->priv->bookmark, EPHY_NODE_BMK_PROP_USERICON);
        refresh_icon (GTK_COMBO_BOX (combobox), location, TRUE, TRUE);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (layout, renderer, TRUE);
	gtk_cell_layout_add_attribute (layout, renderer, "text", 1);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (layout, renderer, FALSE);
	gtk_cell_layout_add_attribute (layout, renderer, "pixbuf", 2);
	
	g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (icon_changed_cb), properties);
	gtk_widget_show (combobox);

	g_signal_connect (G_OBJECT (combobox), "drag_data_received",
			  G_CALLBACK (icon_drag_data_received_cb), NULL);
	gtk_drag_dest_set (combobox, GTK_DEST_DEFAULT_ALL, dest_drag_types,
			   G_N_ELEMENTS (dest_drag_types), GDK_ACTION_COPY);
	
	return combobox;
}

static void
build_ui (EphyBookmarkProperties *properties)
{
	GtkWidget *table, *label, *entry, *palette;
	GtkWidget *scrolled_window;
	GtkComboBox *cbox;
	const char *tmp;

	g_signal_connect (G_OBJECT (properties),
			  "response",
			  G_CALLBACK (bookmark_properties_response_cb),
			  properties);

	g_signal_connect (G_OBJECT (properties),
			  "close",
			  G_CALLBACK (bookmark_properties_close_cb),
			  properties);

	ephy_state_add_window (GTK_WIDGET(properties),
			       "bookmark_properties",
			       290, 280, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE);

	update_window_title (properties);
	set_window_icon (properties);

	gtk_dialog_set_has_separator (GTK_DIALOG (properties), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (properties), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (properties)->vbox), 2);

	table = gtk_table_new (5, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_TABLE (table)), 5);
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
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new ();
	tmp = ephy_node_get_property_string (properties->priv->bookmark,
					     EPHY_NODE_BMK_PROP_LOCATION);
	gtk_entry_set_text (GTK_ENTRY (entry), tmp);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (location_entry_changed_cb), properties);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_show (entry);
	label = gtk_label_new_with_mnemonic (_("_Address:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	
	entry = build_icon (properties);
	label = gtk_label_new_with_mnemonic (_("I_con:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	cbox = GTK_COMBO_BOX (gtk_combo_box_new_text ());
	gtk_widget_show (GTK_WIDGET (cbox));
	gtk_combo_box_append_text (cbox, _("All"));
	gtk_combo_box_append_text (cbox, _("Subtopics"));
	label = gtk_label_new_with_mnemonic(_("T_opics:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (cbox));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (cbox), 1, 2, 3, 4, GTK_FILL, 0, 0, 0);

	palette = ephy_topics_palette_new (properties->priv->bookmarks,
					   properties->priv->bookmark);
	properties->priv->palette = EPHY_TOPICS_PALETTE (palette);
	gtk_widget_show (palette);
	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_widget_show (scrolled_window);
	gtk_container_add (GTK_CONTAINER (scrolled_window), palette);
	  
	g_signal_connect_object (G_OBJECT (cbox), "changed", G_CALLBACK (combo_changed_cb), 
				 palette, G_CONNECT_AFTER);
	
	gtk_table_attach (GTK_TABLE (table), scrolled_window, 1, 2, 4, 5,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_table_set_row_spacing (GTK_TABLE (table), 3, 3);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (properties)->vbox),
			    table, TRUE, TRUE, 0);
	
	gtk_dialog_add_button (GTK_DIALOG (properties),
			       GTK_STOCK_HELP,
			       GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (properties),
			       _("_New Topic"),
			       RESPONSE_NEW_TOPIC);
	
	if (properties->priv->creating)
	{
		gtk_dialog_add_button (GTK_DIALOG (properties),
				       GTK_STOCK_CANCEL,
				       GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (properties),
				       GTK_STOCK_OK,
				       GTK_RESPONSE_OK);
		gtk_dialog_set_default_response (GTK_DIALOG (properties), GTK_RESPONSE_OK);
	}
	else
	{
		gtk_dialog_add_button (GTK_DIALOG (properties),
				       GTK_STOCK_CLOSE,
				       GTK_RESPONSE_CLOSE);
		gtk_dialog_set_default_response (GTK_DIALOG (properties), GTK_RESPONSE_CLOSE);
	}

	gtk_combo_box_set_active (cbox, 1);
}

static void
ephy_bookmark_properties_init (EphyBookmarkProperties *properties)
{
	properties->priv = EPHY_BOOKMARK_PROPERTIES_GET_PRIVATE (properties);
}

GtkWidget *
ephy_bookmark_properties_new (EphyBookmarks *bookmarks,
			      EphyNode *bookmark,
			      gboolean creating)
{
	EphyBookmarkProperties *properties;

	g_assert (bookmarks != NULL);

	properties = EPHY_BOOKMARK_PROPERTIES (g_object_new
			(EPHY_TYPE_BOOKMARK_PROPERTIES,
			 "bookmarks", bookmarks,
			 "bookmark", bookmark,
			 "creating", creating,
			 NULL));

	build_ui (properties);
	
	gtk_window_set_destroy_with_parent (GTK_WINDOW (properties), TRUE);

	return GTK_WIDGET (properties);
}

EphyNode *
ephy_bookmark_properties_get_node (EphyBookmarkProperties *properties)
{
	return properties->priv->bookmark;
}
