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
#include "ephy-topics-selector.h"
#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-favicon-cache.h"

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkscrolledwindow.h>
#include <glib/gi18n.h>

static void ephy_bookmark_properties_class_init (EphyBookmarkPropertiesClass *klass);
static void ephy_bookmark_properties_init (EphyBookmarkProperties *editor);
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

	GtkWidget *title_entry;
	GtkWidget *location_entry;
	GtkWidget *topics_selector;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK
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

	g_type_class_add_private (object_class, sizeof(EphyBookmarkPropertiesPrivate));
}

static void
ephy_bookmark_properties_set_bookmark (EphyBookmarkProperties *selector,
				       EphyNode *bookmark)
{
	LOG ("Set bookmark");

	selector->priv->bookmark = bookmark;

	g_object_notify (G_OBJECT (selector), "bookmark");
}

static void
ephy_bookmark_properties_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	EphyBookmarkProperties *selector = EPHY_BOOKMARK_PROPERTIES (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		selector->priv->bookmarks = g_value_get_object (value);
		break;
	case PROP_BOOKMARK:
		ephy_bookmark_properties_set_bookmark
			(selector, g_value_get_pointer (value));
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
	EphyBookmarkProperties *selector = EPHY_BOOKMARK_PROPERTIES (object);

	switch (prop_id)
	{
	case PROP_BOOKMARK:
		g_value_set_object (value, selector);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
bookmark_properties_response_cb (GtkDialog *dialog,
				 int response_id,
				 gpointer data)
{
	switch (response_id)
	{
		case GTK_RESPONSE_HELP:
			ephy_gui_help (GTK_WINDOW (dialog),
				       "epiphany", 
				       "to-edit-bookmark-properties");
			break;
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
	}
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
update_window_title(EphyBookmarkProperties *editor)
{
	char *title;
	const char *tmp;

	tmp = ephy_node_get_property_string (editor->priv->bookmark,
					     EPHY_NODE_BMK_PROP_TITLE);
	title = g_strdup_printf (_("“%s” Properties"), tmp);
	gtk_window_set_title (GTK_WINDOW (editor), title);
	g_free (title);
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
set_window_icon (EphyBookmarkProperties *editor)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *icon = NULL;

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (embed_shell));
	icon_location = ephy_node_get_property_string
		(editor->priv->bookmark, EPHY_NODE_BMK_PROP_ICON);

	LOG ("Get favicon for %s", icon_location ? icon_location : "None");

	if (icon_location != NULL)
	{
		icon = ephy_favicon_cache_get (cache, icon_location);
	}

	if (icon != NULL)
	{
		gtk_window_set_icon (GTK_WINDOW (editor), icon);
		g_object_unref (icon);
	}
	else
	{
		gtk_window_set_icon_name (GTK_WINDOW (editor),
					  GTK_STOCK_PROPERTIES);
	}

}

static void
build_ui (EphyBookmarkProperties *editor)
{
	GtkWidget *table, *label, *entry, *topics_selector;
	GtkWidget *scrolled_window;
	char *str;
	const char *tmp;

	g_signal_connect (G_OBJECT (editor),
			  "response",
			  G_CALLBACK (bookmark_properties_response_cb),
			  editor);

	ephy_state_add_window (GTK_WIDGET(editor),
			       "bookmark_properties",
			       290, 280, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE);

	update_window_title (editor);
	set_window_icon (editor);

	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (editor)->vbox), 2);

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_TABLE (table)), 5);
	gtk_widget_show (table);

	entry = gtk_entry_new ();
	tmp = ephy_node_get_property_string (editor->priv->bookmark,
					     EPHY_NODE_BMK_PROP_TITLE);
	gtk_entry_set_text (GTK_ENTRY (entry), tmp);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (title_entry_changed_cb), editor);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	editor->priv->title_entry = entry;
	gtk_widget_set_size_request (entry, 200, -1);
	gtk_widget_show (entry);
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("_Title:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new ();
	tmp = ephy_node_get_property_string (editor->priv->bookmark,
					     EPHY_NODE_BMK_PROP_LOCATION);
	gtk_entry_set_text (GTK_ENTRY (entry), tmp);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (location_entry_changed_cb), editor);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	editor->priv->location_entry = entry;
	gtk_widget_show (entry);
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("_Address:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	topics_selector = ephy_topics_selector_new (editor->priv->bookmarks,
						    editor->priv->bookmark);
	gtk_widget_show (topics_selector);
	editor->priv->topics_selector = topics_selector;
	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_widget_show (scrolled_window);
	gtk_container_add (GTK_CONTAINER (scrolled_window), topics_selector);
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	str = g_strconcat ("<b>", _("To_pics:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), topics_selector);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), scrolled_window, 1, 2, 2, 3,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox),
			    table, TRUE, TRUE, 0);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_HELP,
			       GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_CLOSE);
}

static void
ephy_bookmark_properties_init (EphyBookmarkProperties *editor)
{
	editor->priv = EPHY_BOOKMARK_PROPERTIES_GET_PRIVATE (editor);
}

GtkWidget *
ephy_bookmark_properties_new (EphyBookmarks *bookmarks,
			      EphyNode *bookmark,
			      GtkWidget *parent_window)
{
	EphyBookmarkProperties *editor;

	g_assert (bookmarks != NULL);

	editor = EPHY_BOOKMARK_PROPERTIES (g_object_new
			(EPHY_TYPE_BOOKMARK_PROPERTIES,
			 "bookmarks", bookmarks,
			 "bookmark", bookmark,
			 NULL));

	build_ui (editor);	
	
	if (parent_window)
	{
		gtk_window_set_transient_for (GTK_WINDOW (editor),
					      GTK_WINDOW (parent_window));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (editor), TRUE);
	}
	return GTK_WIDGET (editor);
}

EphyNode *
ephy_bookmark_properties_get_node (EphyBookmarkProperties *properties)
{
	return properties->priv->bookmark;
}
