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
 */

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkeditable.h>
#include <libgnome/gnome-i18n.h>

#include "ephy-new-bookmark.h"
#include "ephy-keywords-entry.h"
#include "ephy-debug.h"

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

struct EphyNewBookmarkPrivate
{
	EphyBookmarks *bookmarks;
	char *location;
	char *smarturl;
	char *icon;
	gulong id;

	GtkWidget *title_entry;
	GtkWidget *keywords_entry;
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
	static GType ephy_new_bookmark_type = 0;

	if (ephy_new_bookmark_type == 0)
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

		ephy_new_bookmark_type = g_type_register_static (GTK_TYPE_DIALOG,
							             "EphyNewBookmark",
							             &our_info, 0);
	}

	return ephy_new_bookmark_type;
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
							      EPHY_BOOKMARKS_TYPE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_string ("location",
							      "Bookmark location",
							      "Bookmark location",
							      "",
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
ephy_new_bookmark_finalize (GObject *object)
{
	EphyNewBookmark *editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_NEW_BOOKMARK (object));

	editor = EPHY_NEW_BOOKMARK (object);

	g_return_if_fail (editor->priv != NULL);

	g_free (editor->priv->location);
	g_free (editor->priv->smarturl);
	g_free (editor->priv->icon);

	g_free (editor->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_new_bookmark_add (EphyNewBookmark *new_bookmark)
{
	char *title;
	char *keywords;
	EphyNode *node;

	title = gtk_editable_get_chars
		(GTK_EDITABLE (new_bookmark->priv->title_entry), 0, -1);
	keywords = gtk_editable_get_chars
		(GTK_EDITABLE (new_bookmark->priv->keywords_entry), 0, -1);
	node = ephy_bookmarks_add (new_bookmark->priv->bookmarks, title,
			           new_bookmark->priv->location,
			           new_bookmark->priv->smarturl, keywords);
	new_bookmark->priv->id = ephy_node_get_id (node);

	if (new_bookmark->priv->icon)
	{
		ephy_bookmarks_set_icon (new_bookmark->priv->bookmarks,
					 new_bookmark->priv->location,
					 new_bookmark->priv->icon);
	}
}

static void
ephy_new_bookmark_response_cb (GtkDialog *dialog,
		               int response_id,
			       EphyNewBookmark *new_bookmark)
{
	switch (response_id)
	{
		case GTK_RESPONSE_CANCEL:
			break;
		case GTK_RESPONSE_OK:
			ephy_new_bookmark_add (new_bookmark);
			break;
	}
}

static GtkWidget *
build_editing_table (EphyNewBookmark *editor)
{
	GtkWidget *table, *label, *entry;
	char *str;

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("Title:"), "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_show (label);
	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	editor->priv->title_entry = entry;
	gtk_widget_set_size_request (entry, 200, -1);
	gtk_widget_show (entry);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 0, 1);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("Topics:"), "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_show (label);
	entry = ephy_keywords_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	ephy_keywords_entry_set_bookmarks (EPHY_KEYWORDS_ENTRY (entry),
					   editor->priv->bookmarks);
	editor->priv->keywords_entry = entry;
	gtk_widget_show (entry);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 1, 2);

	return table;
}

static void
ephy_new_bookmark_construct (EphyNewBookmark *editor)
{
	GtkWidget *hbox, *vbox;

	gtk_window_set_title (GTK_WINDOW (editor),
			      _("Add bookmark"));

	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (editor), 6);
	g_signal_connect (G_OBJECT (editor),
			  "response",
			  G_CALLBACK (ephy_new_bookmark_response_cb),
			  editor);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (editor)->vbox), 12);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox),
			    hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	gtk_box_pack_start (GTK_BOX (vbox),
			    build_editing_table (editor),
			    FALSE, FALSE, 0);

	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_OK,
			       GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_OK);
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
	editor->priv = g_new0 (EphyNewBookmarkPrivate, 1);
	editor->priv->location = NULL;
	editor->priv->smarturl = NULL;
	editor->priv->icon = NULL;
	editor->priv->id = 0;
}

void
ephy_new_bookmark_set_title (EphyNewBookmark *bookmark,
			     const char *title)
{
	LOG ("Setting new bookmark title to: \"%s\"", title)
	gtk_entry_set_text (GTK_ENTRY (bookmark->priv->title_entry),
			    g_strdup (title));
}

void
ephy_new_bookmark_set_smarturl (EphyNewBookmark *bookmark,
			        const char *url)
{
	g_free (bookmark->priv->smarturl);
	bookmark->priv->smarturl = g_strdup (url);
}

void
ephy_new_bookmark_set_icon (EphyNewBookmark *bookmark,
			    const char *icon)
{
	g_free (bookmark->priv->icon);
	bookmark->priv->icon = icon ? g_strdup (icon) : NULL;
}

gulong
ephy_new_bookmark_get_id (EphyNewBookmark *bookmark)
{
	return bookmark->priv->id;
}

