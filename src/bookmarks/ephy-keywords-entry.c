/*
 *  Copyright (C) 2002  Marco Pesenti Gritti
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
 */

#include "ephy-keywords-entry.h"
#include "ephy-marshal.h"
#include "ephy-gobject-misc.h"
#include "ephy-debug.h"

#include <gdk/gdkkeysyms.h>

/**
 * Private data
 */
struct _EphyKeywordsEntryPrivate
{
	EphyBookmarks *bookmarks;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_keywords_entry_class_init		(EphyKeywordsEntryClass *klass);
static void		ephy_keywords_entry_init		(EphyKeywordsEntry *w);
static void		ephy_keywords_entry_finalize_impl	(GObject *o);
static gint		ephy_keywords_entry_key_press		(GtkWidget *widget,
								 GdkEventKey *event);

enum
{
	KEYWORDS_CHANGED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint keywords_entry_signals[LAST_SIGNAL] = { 0 };

MAKE_GET_TYPE (ephy_keywords_entry, "EphyKeywordsEntry", EphyKeywordsEntry,
	       ephy_keywords_entry_class_init,
	       ephy_keywords_entry_init, GTK_TYPE_ENTRY);

static void
ephy_keywords_entry_class_init (EphyKeywordsEntryClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	widget_class = (GtkWidgetClass*) class;

	gobject_class->finalize = ephy_keywords_entry_finalize_impl;

	widget_class->key_press_event = ephy_keywords_entry_key_press;

	keywords_entry_signals[KEYWORDS_CHANGED] =
                g_signal_new ("keywords_changed",
                              G_OBJECT_CLASS_TYPE (gobject_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyKeywordsEntryClass, keywords_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
}

static void
try_to_expand_keyword (GtkEditable *editable)
{
	char *entry_text;
	char *user_text;
	const char *expand_text;
	char *insert_text;
	int user_text_length;
	int expand_text_length;
	int keyword_offset = 0;
	int tmp;
	EphyKeywordsEntry *entry = EPHY_KEYWORDS_ENTRY (editable);
	EphyNode *node;

	entry_text = gtk_editable_get_chars (editable, 0, -1);
	g_return_if_fail (entry_text != NULL);

	LOG ("Entry text \"%s\"", entry_text)

	user_text = g_utf8_strrchr (entry_text, -1, ' ');

	if (user_text)
	{
		user_text = g_utf8_find_next_char (user_text, NULL);
		keyword_offset = g_utf8_pointer_to_offset
			(entry_text, user_text);
	}
	else
	{
		user_text = entry_text;
	}

	LOG ("User text \"%s\"", user_text)

	node = ephy_bookmarks_find_keyword (entry->priv->bookmarks,
					    user_text, TRUE);
	if (node)
	{
		expand_text = ephy_node_get_property_string
			(node, EPHY_NODE_KEYWORD_PROP_NAME);

		LOG ("Expand text %s", expand_text)

		expand_text_length = g_utf8_strlen (expand_text, -1);
		user_text_length = g_utf8_strlen (user_text, -1);

		insert_text = g_utf8_offset_to_pointer (expand_text, user_text_length);
		gtk_editable_insert_text (editable,
					  insert_text,
					  g_utf8_strlen (insert_text, -1),
					  &tmp);
		gtk_editable_select_region (editable, user_text_length + keyword_offset, -1);
	}
	else
	{
		LOG ("No expansion.")
	}

	g_free (entry_text);
}

/* Until we have a more elegant solution, this is how we figure out if
 * the GtkEntry inserted characters, assuming that the return value is
 * TRUE indicating that the GtkEntry consumed the key event for some
 * reason. This is a clone of code from GtkEntry.
 */
static gboolean
entry_would_have_inserted_characters (const GdkEventKey *event)
{
	switch (event->keyval) {
	case GDK_BackSpace:
	case GDK_Clear:
	case GDK_Insert:
	case GDK_Delete:
	case GDK_Home:
	case GDK_End:
	case GDK_Left:
	case GDK_Right:
	case GDK_Return:
		return FALSE;
	default:
		if (event->keyval >= 0x20 && event->keyval <= 0xFF) {
			if ((event->state & GDK_CONTROL_MASK) != 0) {
				return FALSE;
			}
			if ((event->state & GDK_MOD1_MASK) != 0) {
				return FALSE;
			}
		}
		return event->length > 0;
	}
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
	char *text;
	int length;

	text = gtk_editable_get_chars (editable, 0, -1);
	length = g_utf8_strlen (text, -1);
	g_free (text);
	return length;
}

static void
set_position_and_selection_to_end (GtkEditable *editable)
{
	int end;

	end = get_editable_number_of_chars (editable);
	gtk_editable_select_region (editable, end, end);
	gtk_editable_set_position (editable, end);
}

static gboolean
position_and_selection_are_at_end (GtkEditable *editable)
{
	int end;
	int start_sel, end_sel;

	end = get_editable_number_of_chars (editable);
	if (gtk_editable_get_selection_bounds (editable, &start_sel, &end_sel))
	{
		if (start_sel != end || end_sel != end)
		{
			return FALSE;
		}
	}
	return gtk_editable_get_position (editable) == end;
}

static gint
ephy_keywords_entry_key_press (GtkWidget *widget,
			       GdkEventKey *event)
{
	GtkEditable *editable;
	GdkEventKey *keyevent;
	EphyKeywordsEntry *entry;
	gboolean result;

	entry = EPHY_KEYWORDS_ENTRY (widget);
	editable = GTK_EDITABLE (entry);
	keyevent = (GdkEventKey *)event;

	/* After typing the right arrow key we move the selection to
	 * the end, if we have a valid selection - since this is most
	 * likely an auto-completion. We ignore shift / control since
	 * they can validly be used to extend the selection.
	 */
	if ((keyevent->keyval == GDK_Right || keyevent->keyval == GDK_End) &&
	    !(keyevent->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) &&
	    gtk_editable_get_selection_bounds (editable, NULL, NULL))
	{
		set_position_and_selection_to_end (editable);
	}

	result = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);

	/* Only do expanding when we are typing at the end of the
	 * text.
	 */
	if (entry_would_have_inserted_characters (event)
	    && position_and_selection_are_at_end (editable))
	{
		try_to_expand_keyword (editable);
	}

	g_signal_emit (G_OBJECT (entry), keywords_entry_signals[KEYWORDS_CHANGED], 0);

	return result;
}

static void
ephy_keywords_entry_init (EphyKeywordsEntry *w)
{
	w->priv = g_new0 (EphyKeywordsEntryPrivate, 1);
	w->priv->bookmarks = NULL;
}

static void
ephy_keywords_entry_finalize_impl (GObject *o)
{
	EphyKeywordsEntry *w = EPHY_KEYWORDS_ENTRY (o);
	EphyKeywordsEntryPrivate *p = w->priv;

	g_free (p);
	G_OBJECT_CLASS (parent_class)->finalize (o);
}

GtkWidget *
ephy_keywords_entry_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

void
ephy_keywords_entry_set_bookmarks (EphyKeywordsEntry *w,
				   EphyBookmarks *bookmarks)
{
	w->priv->bookmarks = bookmarks;
}
