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
	RESPONSE_NEW_TOPIC = 1
};

static GObjectClass *parent_class;

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
bookmark_properties_close_cb (GtkDialog *dialog,
			      gpointer data)
{
	EphyBookmarkProperties *properties = EPHY_BOOKMARK_PROPERTIES (dialog);
	EphyBookmarkPropertiesPrivate *priv = properties->priv;

	if (priv->creating)
	{
		ephy_node_unref (priv->bookmark);
		priv->bookmark = NULL;
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
		case RESPONSE_NEW_TOPIC:
			ephy_bookmarks_ui_add_topic (GTK_WIDGET (dialog),
						     priv->bookmark);
			return;
		case GTK_RESPONSE_CANCEL:
			ephy_node_unref (priv->bookmark);
			priv->bookmark = NULL;
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
combo_changed_cb (GtkComboBox *combobox,
		  GtkWidget *palette)
{
	int active;

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (combobox));
	g_object_set (palette, "mode", active, NULL);
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
	GtkWidget *widget, *table, *label, *entry, *palette;
	GtkWidget *scrolled_window;
	GtkWindow *window;
	GtkDialog *dialog;
	GtkComboBox *cbox;
	const char *tmp;

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

	g_signal_connect (properties, "close",
			  G_CALLBACK (bookmark_properties_close_cb), properties);

	ephy_state_add_window (widget,
			       "bookmark_properties",
			       290, 280, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE);

	update_window_title (properties);

	gtk_dialog_set_has_separator (dialog, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (properties), 5);
	gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2);

	table = gtk_table_new (4, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
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

	cbox = GTK_COMBO_BOX (gtk_combo_box_new_text ());
	gtk_widget_show (GTK_WIDGET (cbox));
	gtk_combo_box_append_text (cbox, _("All"));
	gtk_combo_box_append_text (cbox, _("Subtopics"));
	label = gtk_label_new_with_mnemonic(_("T_opics:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (cbox));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (cbox), 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	palette = ephy_topics_palette_new (priv->bookmarks, priv->bookmark);
	priv->palette = EPHY_TOPICS_PALETTE (palette);

	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), palette);
	gtk_widget_show (palette);
  
	g_signal_connect_object (cbox, "changed",
				 G_CALLBACK (combo_changed_cb), palette,
				 G_CONNECT_AFTER);
	
	gtk_table_attach (GTK_TABLE (table), scrolled_window, 1, 2, 3, 4,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show (scrolled_window);

	gtk_box_pack_start (GTK_BOX (dialog->vbox), table, TRUE, TRUE, 0);
	
	gtk_dialog_add_button (dialog,
			       GTK_STOCK_HELP,
			       GTK_RESPONSE_HELP);
	gtk_dialog_add_button (dialog,
			       _("_New Topic"),
			       RESPONSE_NEW_TOPIC);
	
	if (priv->creating)
	{
		gtk_dialog_add_button (dialog,
				       GTK_STOCK_CANCEL,
				       GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (dialog,
				       GTK_STOCK_OK,
				       GTK_RESPONSE_OK);
		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);
	}
	else
	{
		gtk_dialog_add_button (dialog,
				       GTK_STOCK_CLOSE,
				       GTK_RESPONSE_CLOSE);
		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CLOSE);
	}

	gtk_combo_box_set_active (cbox, 1);

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

	switch (prop_id)
	{
		case PROP_BOOKMARKS:
			priv->bookmarks = g_value_get_object (value);
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
