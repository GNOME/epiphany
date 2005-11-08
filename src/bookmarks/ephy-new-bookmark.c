/* 
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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

#include "ephy-new-bookmark.h"
#include "ephy-bookmarks.h"
#include "ephy-state.h"
#include "ephy-topics-selector.h"
#include "ephy-debug.h"
#include "ephy-stock-icons.h"
#include "ephy-gui.h"

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <glib/gi18n.h>
#include <string.h>

static void ephy_new_bookmark_class_init (EphyNewBookmarkClass *klass);
static void ephy_new_bookmark_init (EphyNewBookmark *editor);
static void ephy_new_bookmark_finalize (GObject *object);
static void ephy_new_bookmark_set_property (GObject *object,
		                                guint prop_id,
		                                const GValue *value,
		                                GParamSpec *pspec);
static void ephy_new_bookmark_get_property (GObject *object,
						guint prop_id,
						GValue *value,
						GParamSpec *pspec);

#define DATA_KEY	"EphyNode"

#define EPHY_NEW_BOOKMARK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NEW_BOOKMARK, EphyNewBookmarkPrivate))

enum
{
	RESPONSE_NEW_TOPIC
};

struct _EphyNewBookmarkPrivate
{
	EphyBookmarks *bookmarks;
	char *location;
	char *icon;
	guint id;

	GtkWidget *title_entry;
	GtkWidget *topics_selector;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_LOCATION
};

static GObjectClass *parent_class = NULL;

GType
ephy_new_bookmark_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyNewBookmarkClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_new_bookmark_class_init,
			NULL,
			NULL,
			sizeof (EphyNewBookmark),
			0,
			(GInstanceInitFunc) ephy_new_bookmark_init
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "EphyNewBookmark",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_new_bookmark_class_init (EphyNewBookmarkClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_new_bookmark_finalize;

	object_class->set_property = ephy_new_bookmark_set_property;
	object_class->get_property = ephy_new_bookmark_get_property;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_string ("location",
							      "Bookmark location",
							      "Bookmark location",
							      "",
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyNewBookmarkPrivate));
}

static void
ephy_new_bookmark_finalize (GObject *object)
{
	EphyNewBookmark *editor = EPHY_NEW_BOOKMARK (object);

	g_free (editor->priv->location);
	g_free (editor->priv->icon);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_new_bookmark_add (EphyNewBookmark *new_bookmark)
{
	char *title;
	EphyNode *node;
	EphyTopicsSelector *selector;

	selector = EPHY_TOPICS_SELECTOR (new_bookmark->priv->topics_selector);

	title = gtk_editable_get_chars
		(GTK_EDITABLE (new_bookmark->priv->title_entry), 0, -1);
	node = ephy_bookmarks_add (new_bookmark->priv->bookmarks, title,
			           new_bookmark->priv->location);
	new_bookmark->priv->id = ephy_node_get_id (node);

	ephy_topics_selector_apply (selector, node);

	if (new_bookmark->priv->icon)
	{
		ephy_bookmarks_set_icon (new_bookmark->priv->bookmarks,
					 new_bookmark->priv->location,
					 new_bookmark->priv->icon);
	}

	g_free (title);
}

static void
response_cb (EphyNewBookmark *new_bookmark,
	     int response_id,
	     gpointer user_data)
{
	EphyTopicsSelector *selector;

	selector = EPHY_TOPICS_SELECTOR (new_bookmark->priv->topics_selector);

	switch (response_id)
	{
		case GTK_RESPONSE_HELP:
			ephy_gui_help (GTK_WINDOW (new_bookmark), 
		       		       "epiphany",
		       		       "to-create-new-bookmark");
			break;
		case RESPONSE_NEW_TOPIC:
			ephy_topics_selector_new_topic (selector);
			break;
		/* For both OK and Cancel we want to destroy the dialog */
		case GTK_RESPONSE_OK:
			ephy_new_bookmark_add (new_bookmark);
		case GTK_RESPONSE_CANCEL:
		default:
			gtk_widget_destroy (GTK_WIDGET (new_bookmark));
			break;
	}
}

static GtkWidget *
build_editing_table (EphyNewBookmark *editor)
{
	GtkWidget *table, *label, *entry, *topics_selector, *scrolled_window;
	char *str;

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_TABLE (table)), 5);
	gtk_widget_show (table);


	entry = gtk_entry_new ();
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

	
	topics_selector = ephy_topics_selector_new (editor->priv->bookmarks, NULL);
	gtk_widget_show (topics_selector);
	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,	
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_widget_show (scrolled_window);
	gtk_container_add (GTK_CONTAINER (scrolled_window), topics_selector);
	editor->priv->topics_selector = topics_selector;
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	str = g_strconcat ("<b>", _("To_pics:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), topics_selector);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), scrolled_window, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	return table;
}

static void
ephy_new_bookmark_construct (EphyNewBookmark *editor)
{
	ephy_state_add_window (GTK_WIDGET(editor),
			       "new_bookmark",
		               280, 240, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE);

	gtk_window_set_title (GTK_WINDOW (editor),
			      _("Add Bookmark"));
	gtk_window_set_icon_name (GTK_WINDOW (editor), STOCK_ADD_BOOKMARK);

	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
	g_signal_connect (G_OBJECT (editor),
			  "response",
			  G_CALLBACK (response_cb),
			  editor);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (editor)->vbox), 2);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox),
			    build_editing_table (editor),
			    TRUE, TRUE, 0);

	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_HELP,
			       GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       _("_New Topic"),
			       RESPONSE_NEW_TOPIC);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_OK,
			       GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_OK);
}

static GtkWidget*
duplicate_dialog_construct (GtkWindow *parent,
			    const char *title)
{
	GtkWidget *dialog;
	char *str, *tmp_str;

	tmp_str = g_markup_printf_escaped (_("You already have a bookmark titled “%s” for this page."),
				   	   title);
	str = g_strconcat ("<span weight=\"bold\" size=\"larger\">", tmp_str, "</span>", NULL);

	dialog = gtk_message_dialog_new
		(GTK_WINDOW (parent), GTK_DIALOG_DESTROY_WITH_PARENT,
		 GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, NULL);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), str);
	g_free (tmp_str);
	g_free (str);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_View Properties"), GTK_RESPONSE_ACCEPT);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Duplicated Bookmark"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	return dialog;
}

static void
duplicate_bookmark_response_cb (GtkWidget *dialog,
	     			int response,
	     			EphyBookmarks *bookmarks)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		EphyNode *node;
		GtkWidget *parent;

		node = g_object_get_data (G_OBJECT (dialog), DATA_KEY);
		parent = GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (dialog)));
		ephy_bookmarks_show_bookmark_properties (bookmarks, node, parent);
	}

	gtk_widget_destroy (dialog);
}

static void
duplicated_node_destroy_cb (EphyNode *node,
			    GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

gboolean
ephy_new_bookmark_is_unique (EphyBookmarks *bookmarks,
			     GtkWindow *parent,
			     const char *address)
{
	EphyNode *node;

	node = ephy_bookmarks_find_bookmark (bookmarks, address);
	if (node)
	{
		GtkWidget *dialog;
		const char *title;

		title = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_TITLE);
		dialog = duplicate_dialog_construct (parent, title);
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
		g_object_set_data (G_OBJECT (dialog), DATA_KEY, node);
		g_signal_connect (G_OBJECT (dialog),
			  	  "response",
			  	  G_CALLBACK (duplicate_bookmark_response_cb),
			  	  bookmarks);
		ephy_node_signal_connect_object (node, EPHY_NODE_DESTROY,
						 (EphyNodeCallback) duplicated_node_destroy_cb,
						 G_OBJECT (dialog));
		gtk_widget_show (GTK_WIDGET (dialog));
		return FALSE;
	}

	return TRUE;
}

GtkWidget *
ephy_new_bookmark_new (EphyBookmarks *bookmarks,
		       GtkWindow *parent,
		       const char *location)
{
	EphyNewBookmark *editor;

	g_assert (bookmarks != NULL);

	editor = EPHY_NEW_BOOKMARK (g_object_new
			(EPHY_TYPE_NEW_BOOKMARK,
			 "bookmarks", bookmarks,
			 "location", location,
			 NULL));
	if (parent)
	{
		gtk_window_set_transient_for (GTK_WINDOW (editor), parent);
	}

	ephy_new_bookmark_construct (editor);

	return GTK_WIDGET (editor);
}

static void
ephy_new_bookmark_set_property (GObject *object,
		                guint prop_id,
		                const GValue *value,
		                GParamSpec *pspec)
{
	EphyNewBookmark *editor = EPHY_NEW_BOOKMARK (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		editor->priv->bookmarks = g_value_get_object (value);
		break;
	case PROP_LOCATION:
		g_free (editor->priv->location);
		editor->priv->location = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_new_bookmark_get_property (GObject *object,
		                guint prop_id,
		                GValue *value,
		                GParamSpec *pspec)
{
	EphyNewBookmark *editor = EPHY_NEW_BOOKMARK (object);

	switch (prop_id)
	{
	case PROP_LOCATION:
		g_value_set_string (value, editor->priv->location);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_new_bookmark_init (EphyNewBookmark *editor)
{
	editor->priv = EPHY_NEW_BOOKMARK_GET_PRIVATE (editor);
}

void
ephy_new_bookmark_set_title (EphyNewBookmark *bookmark,
			     const char *title)
{
	const char *real_title;

	LOG ("Setting new bookmark title to: \"%s\"", title);

	if (title == NULL || strlen (title) == 0)
	{
		real_title = bookmark->priv->location;
	}
	else
	{
		real_title = title;
	}

	gtk_entry_set_text (GTK_ENTRY (bookmark->priv->title_entry),
			    real_title);
}

void
ephy_new_bookmark_set_icon (EphyNewBookmark *bookmark,
			    const char *icon)
{
	g_free (bookmark->priv->icon);
	bookmark->priv->icon = icon ? g_strdup (icon) : NULL;
}

guint
ephy_new_bookmark_get_id (EphyNewBookmark *bookmark)
{
	return bookmark->priv->id;
}
