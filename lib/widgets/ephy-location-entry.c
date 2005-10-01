/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
 *  Copyright (C) 2003, 2004  Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005  Christian Persch
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
 *  $Id$
 */

#include "config.h"

#include "ephy-icon-entry.h"
#include "ephy-tree-model-node.h"
#include "ephy-location-entry.h"
#include "ephy-marshal.h"
#include "ephy-signal-accumulator.h"
#include "ephy-dnd.h"
#include "egg-editable-toolbar.h"
#include "ephy-stock-icons.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkclipboard.h>

#include <string.h>

#define EPHY_LOCATION_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryPrivate))

struct _EphyLocationEntryPrivate
{
	GtkTooltips *tips;
	EphyIconEntry *icon_entry;
	GtkWidget *icon_ebox;
	GtkWidget *icon;
	GtkWidget *lock_ebox;
	GtkWidget *lock;
	GdkPixbuf *favicon;

	char *before_completion;

	guint text_col;
	guint action_col;
	guint keywords_col;
	guint relevance_col;

	guint hash;
	guint user_changed : 1;
	guint original_address : 1;
};

static const struct
{
	const char *prefix;
	int len;
}
web_prefixes [] =
{
	{ "http://www.", 11 },
	{ "http://", 7 },
	{ "https://www.", 12 },
	{ "https://", 8 },
	{ "www.", 4 }
};

static const GtkTargetEntry url_drag_types [] =
{
	{ EPHY_DND_URL_TYPE,        0, 0 },
	{ EPHY_DND_URI_LIST_TYPE,   0, 1 },
	{ EPHY_DND_TEXT_TYPE,       0, 2 }
};

static void ephy_location_entry_class_init (EphyLocationEntryClass *klass);
static void ephy_location_entry_init (EphyLocationEntry *le);

static GObjectClass *parent_class = NULL;

enum signalsEnum
{
	USER_CHANGED,
	LOCK_CLICKED,
	GET_LOCATION,
	GET_TITLE,
	LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

#define MAX_LOC_HISTORY_ITEMS 10
#define EPHY_LOC_HISTORY_XML_ROOT "ephy_location_history"
#define EPHY_LOC_HISTORY_XML_VERSION "0.1"

GType
ephy_location_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLocationEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_location_entry_class_init,
			NULL,
			NULL,
			sizeof (EphyLocationEntry),
			0,
			(GInstanceInitFunc) ephy_location_entry_init
		};

		type = g_type_register_static (GTK_TYPE_TOOL_ITEM,
					       "EphyLocationEntry",
					       &our_info, 0);
	}

	return type;
}

static gboolean
ephy_location_entry_set_tooltip (GtkToolItem *tool_item,
				 GtkTooltips *tooltips,
				 const char *tip_text,
				 const char *tip_private)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (tool_item);
	EphyLocationEntryPrivate *priv = entry->priv;

	gtk_tooltips_set_tip (tooltips, priv->icon_entry->entry,
			      tip_text, tip_private);

	return TRUE;
}

static void
ephy_location_entry_finalize (GObject *object)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);
	EphyLocationEntryPrivate *priv = entry->priv;

	if (priv->favicon != NULL)
	{
		g_object_unref (priv->favicon);
	}

	g_object_unref (priv->tips);

	parent_class->finalize (object);
}

static void
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkToolItemClass *tool_item_class = GTK_TOOL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_location_entry_finalize;

	tool_item_class->set_tooltip = ephy_location_entry_set_tooltip;

	signals[USER_CHANGED] = g_signal_new (
		"user_changed", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyLocationEntryClass, user_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0,
		G_TYPE_NONE);

	signals[LOCK_CLICKED] = g_signal_new (
		"lock-clicked",
		EPHY_TYPE_LOCATION_ENTRY,
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationEntryClass, lock_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);

	signals[GET_LOCATION] = g_signal_new (
		"get-location", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyLocationEntryClass, get_location),
		ephy_signal_accumulator_string, NULL,
		ephy_marshal_STRING__VOID,
		G_TYPE_STRING,
		0,
		G_TYPE_NONE);

	signals[GET_TITLE] = g_signal_new (
		"get-title", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EphyLocationEntryClass, get_title),
		ephy_signal_accumulator_string, NULL,
		ephy_marshal_STRING__VOID,
		G_TYPE_STRING,
		0,
		G_TYPE_NONE);


	g_type_class_add_private (object_class, sizeof (EphyLocationEntryPrivate));
}

static void
update_address_state (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (priv->icon_entry->entry));
	priv->original_address = text != NULL &&
				 g_str_hash (text) == priv->hash;
}

static void
update_favicon (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	GtkImage *image = GTK_IMAGE (priv->icon);

	/* Only show the favicon if the entry's text is the
	 * address of the current page.
	 */
	if (priv->favicon != NULL && priv->original_address)
	{
		gtk_image_set_from_pixbuf (image, priv->favicon);
	}
	else
	{
		gtk_image_set_from_stock (image,
					  GTK_STOCK_NEW,
					  GTK_ICON_SIZE_MENU);
	}
}

static void
editable_changed_cb (GtkEditable *editable,
		     EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	update_address_state (entry);

	if (priv->user_changed == FALSE) return;

	update_favicon (entry);

	g_signal_emit (entry, signals[USER_CHANGED], 0);
}

static gboolean
entry_button_press_cb (GtkWidget *entry, GdkEventButton *event, EphyLocationEntry *le)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
		ephy_location_entry_activate (le);
		return TRUE;
	}

	return FALSE;
}

static gboolean
entry_key_press_cb (GtkEntry *entry,
		    GdkEventKey *event,
		    EphyLocationEntry *lentry)
{
	guint state = event->state & gtk_accelerator_get_default_mod_mask ();

	if ((event->keyval == GDK_Return ||
	     event->keyval == GDK_KP_Enter ||
	     event->keyval == GDK_ISO_Enter) &&
	    state == GDK_CONTROL_MASK)
	{
		gtk_im_context_reset (entry->im_context);

		g_signal_emit_by_name (entry, "activate");

		return TRUE;
	}
	else if (event->keyval == GDK_Escape && state == 0)
	{
		ephy_location_entry_reset (lentry);
		/* don't return TRUE since we want to cancel the autocompletion popup too */
	}

	return FALSE;
}

static gboolean
keyword_match (const char *list,
	       const char *keyword)
{
	const char *p;
	gsize keyword_len;

	p = list;
	keyword_len = strlen (keyword);

	while (*p)
	{
		int i;

		for (i = 0; i < keyword_len; i++)
		{
			if (p[i] != keyword[i])
			{
				goto next_token;
			}
		}
	  
		return TRUE;
	  
		next_token:

		while (*p && *p != ' ') p++;
		if (*p) p++;
	}

	return FALSE;
}

static gboolean
completion_func (GtkEntryCompletion *completion,
                 const char *key,
		 GtkTreeIter *iter,
		 gpointer data)
{
	int i, len_key, len_prefix;
	char *item = NULL;
	char *keywords = NULL;
	gboolean ret = FALSE;
	EphyLocationEntry *le = EPHY_LOCATION_ENTRY (data);
	GtkTreeModel *model;

	model = gtk_entry_completion_get_model (completion);

	gtk_tree_model_get (model, iter,
			    le->priv->text_col, &item,
			    le->priv->keywords_col, &keywords,
			    -1);

	len_key = strlen (key);
	if (!strncasecmp (key, item, len_key))
	{
		ret = TRUE;
	}
	else if (keyword_match (keywords, key))
	{
		ret = TRUE;
	}
	else
	{
		for (i = 0; i < G_N_ELEMENTS (web_prefixes); i++)
		{
			len_prefix = web_prefixes[i].len;
			if (!strncmp (web_prefixes[i].prefix, item, len_prefix) &&
			    !strncasecmp (key, item + len_prefix, len_key))
			{
				ret = TRUE;
				break;
			}
		}
	}

	g_free (item);
	g_free (keywords);

	return ret;
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel *model,
		   GtkTreeIter *iter,
		   EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	char *item = NULL;

	gtk_tree_model_get (model, iter,
			    priv->action_col, &item, -1);
	if (item == NULL) return FALSE;

	ephy_location_entry_set_location (entry, item, NULL);
	g_signal_emit_by_name (priv->icon_entry->entry, "activate");

	g_free (item);

	return TRUE;
}

static gboolean
toolbar_is_editable (GtkWidget *widget)
{
	GtkWidget *etoolbar;

	etoolbar = gtk_widget_get_ancestor (widget, EGG_TYPE_EDITABLE_TOOLBAR);

	if (etoolbar)
	{
		return egg_editable_toolbar_get_edit_mode
			(EGG_EDITABLE_TOOLBAR (etoolbar));
	}

	return FALSE;
}

static gboolean
entry_drag_motion_cb (GtkWidget        *widget,
		      GdkDragContext   *context,
		      gint              x,
		      gint              y,
		      guint             time)
{
	if (toolbar_is_editable (widget))
	{
		g_signal_stop_emission_by_name (widget, "drag_motion");
	}
    
	return FALSE;
}

static gboolean
entry_drag_drop_cb (GtkWidget          *widget,
		    GdkDragContext     *context,
		    gint                x,
		    gint                y,
		    guint               time)
{
	if (toolbar_is_editable (widget))
	{
		g_signal_stop_emission_by_name (widget, "drag_drop");
	}

	return FALSE;
}

static void
entry_clear_activate_cb (GtkMenuItem *item,
			 EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	priv->user_changed = FALSE;
	gtk_entry_set_text (GTK_ENTRY (priv->icon_entry->entry), "");
	priv->user_changed = TRUE;
}

static void
entry_populate_popup_cb (GtkEntry *entry,
			 GtkMenu *menu,
			 EphyLocationEntry *lentry)
{
	EphyLocationEntryPrivate *priv = lentry->priv;
	GtkWidget *image;
	GtkWidget *menuitem;
	GList *children, *item;
	int pos = 0, sep = 0;
	gboolean is_editable;

	/* Clear and Copy mnemonics conflict, make custom menuitem */
	image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);

	/* Translators: the mnemonic shouldn't conflict with any of the
	 * standard items in the GtkEntry context menu (Cut, Copy, Paste, Delete,
	 * Select All, Input Methods and Insert Unicode control character.)
	 */
	menuitem = gtk_image_menu_item_new_with_mnemonic (_("Cl_ear"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menuitem), image);
	g_signal_connect (menuitem , "activate",
			  G_CALLBACK (entry_clear_activate_cb), lentry);
	is_editable = gtk_editable_get_editable (GTK_EDITABLE (priv->icon_entry->entry));
	gtk_widget_set_sensitive (menuitem, is_editable);
	gtk_widget_show (menuitem);

	/* search for the 2nd separator (the one after Select All) in the context
	 * menu, and insert this menu item before it.
	 * It's a bit of a hack, but there seems to be no better way to do it :/
	 */
	children = GTK_MENU_SHELL (menu)->children;
	for (item = children; item != NULL && sep < 2; item = item->next, pos++)
	{
		if (GTK_IS_SEPARATOR_MENU_ITEM (item->data)) sep++;
	}

	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, pos - 1);
}

static void
each_url_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			  gpointer iterator_context,
			  gpointer return_data)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (iterator_context);
	char *title = NULL, *address = NULL;

	g_signal_emit (entry, signals[GET_LOCATION], 0, &address);
	g_signal_emit (entry, signals[GET_TITLE], 0, &title);
	g_return_if_fail (address != NULL && title != NULL);

	iteratee (address, title, return_data);

	g_free (address);
	g_free (title);
}

static void
favicon_drag_data_get_cb (GtkWidget *widget,
			  GdkDragContext *context,
			  GtkSelectionData *selection_data,
			  guint info,
			  guint32 time,
			  EphyLocationEntry *entry)
{
	g_assert (widget != NULL);
	g_return_if_fail (context != NULL);

	ephy_dnd_drag_data_get (widget, context, selection_data,
		time, entry, each_url_get_data_binder);
}

static gboolean
lock_button_press_event_cb (GtkWidget *ebox,
			    GdkEventButton *event,
			    EphyLocationEntry *entry)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 1 /* left */)
	{
		g_signal_emit (entry, signals[LOCK_CLICKED], 0);

		return TRUE;
	}

	return FALSE;
}

static void
ephy_location_entry_construct_contents (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	GtkWidget *alignment;

	LOG ("EphyLocationEntry constructing contents %p", entry);

	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 1, 1);
	gtk_container_add (GTK_CONTAINER (entry), alignment);

	priv->icon_entry = EPHY_ICON_ENTRY (ephy_icon_entry_new ());
	gtk_container_add (GTK_CONTAINER (alignment),
			   GTK_WIDGET (priv->icon_entry));

	priv->icon_ebox = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (priv->icon_ebox), 2);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (priv->icon_ebox), FALSE);
	gtk_drag_source_set (priv->icon_ebox, GDK_BUTTON1_MASK,
			     url_drag_types, G_N_ELEMENTS (url_drag_types),
			     GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK);
	gtk_tooltips_set_tip (priv->tips, priv->icon_ebox,
			      _("Drag and drop this icon to create a link to this page"), NULL);

	priv->icon = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (priv->icon_ebox), priv->icon);

	ephy_icon_entry_pack_widget (priv->icon_entry, priv->icon_ebox, TRUE);

	priv->lock_ebox = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (priv->lock_ebox), 2);
	gtk_widget_add_events (priv->lock_ebox, GDK_BUTTON_PRESS_MASK);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (priv->lock_ebox), FALSE);

	priv->lock = gtk_image_new_from_stock (STOCK_LOCK_INSECURE,
					       GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (priv->lock_ebox), priv->lock);

	ephy_icon_entry_pack_widget (priv->icon_entry, priv->lock_ebox, FALSE);

	g_signal_connect (priv->icon_ebox, "drag-data-get",
			  G_CALLBACK (favicon_drag_data_get_cb), entry);
	g_signal_connect (priv->lock_ebox, "button-press-event",
			  G_CALLBACK (lock_button_press_event_cb), entry);

	g_signal_connect (priv->icon_entry->entry, "populate_popup",
			  G_CALLBACK (entry_populate_popup_cb), entry);
	g_signal_connect (priv->icon_entry->entry, "key-press-event",
			  G_CALLBACK (entry_key_press_cb), entry);
	g_signal_connect (priv->icon_entry->entry, "button-press-event",
			  G_CALLBACK (entry_button_press_cb), entry);
	g_signal_connect (priv->icon_entry->entry, "changed",
			  G_CALLBACK (editable_changed_cb), entry);
	g_signal_connect (priv->icon_entry->entry, "drag-motion",
			  G_CALLBACK (entry_drag_motion_cb), entry);
	g_signal_connect (priv->icon_entry->entry, "drag-drop",
			  G_CALLBACK (entry_drag_drop_cb), entry);

	gtk_widget_show_all (alignment);
}

static void
ephy_location_entry_init (EphyLocationEntry *le)
{
	EphyLocationEntryPrivate *p;

	LOG ("EphyLocationEntry initialising %p", le);

	p = EPHY_LOCATION_ENTRY_GET_PRIVATE (le);
	le->priv = p;

	p->user_changed = TRUE;

	p->tips = gtk_tooltips_new ();
	g_object_ref (p->tips);
	gtk_object_sink (GTK_OBJECT (p->tips));

	ephy_location_entry_construct_contents (le);

	gtk_tool_item_set_expand (GTK_TOOL_ITEM (le), TRUE);
}

GtkWidget *
ephy_location_entry_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

static gint
sort_func (GtkTreeModel *model,
	   GtkTreeIter *a,
	   GtkTreeIter *b,
	   gpointer data)
{
	gint valuea, valueb;
	EphyLocationEntry *le = EPHY_LOCATION_ENTRY (data);

	gtk_tree_model_get (model, a,
			    le->priv->relevance_col, &valuea, -1);
	gtk_tree_model_get (model, b,
			    le->priv->relevance_col, &valueb, -1);

	return valueb - valuea;
}

void
ephy_location_entry_set_completion (EphyLocationEntry *le,
				    GtkTreeModel *model,
				    guint text_col,
				    guint action_col,
				    guint keywords_col,
				    guint relevance_col)
{
	EphyLocationEntryPrivate *priv = le->priv;
	GtkTreeModel *sort_model;
	GtkEntryCompletion *completion;
	GtkCellRenderer *cell;

	le->priv->text_col = text_col;
	le->priv->action_col = action_col;
	le->priv->keywords_col = keywords_col;
	le->priv->relevance_col = relevance_col;

	sort_model = gtk_tree_model_sort_new_with_model (model);
	g_object_unref (model);
	gtk_tree_sortable_set_default_sort_func
		(GTK_TREE_SORTABLE (sort_model),
		 sort_func, le, NULL);

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, sort_model);
	g_object_unref (sort_model);
	gtk_entry_completion_set_match_func (completion, completion_func, le, NULL);
	g_signal_connect (completion, "match_selected",
			  G_CALLBACK (match_selected_cb), le);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
				    cell, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
				       cell, "text", text_col);

	gtk_entry_set_completion (GTK_ENTRY (priv->icon_entry->entry), completion);
	g_object_unref (completion);
}

void
ephy_location_entry_set_location (EphyLocationEntry *entry,
				  const char *address,
				  const char *typed_address)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	GtkClipboard *clipboard;
	const char *text;
	char* selection = NULL;
	int start, end;

	g_return_if_fail (address != NULL);

        /* Setting a new text will clear the clipboard. This makes it impossible
	 * to copy&paste from the location entry of one tab into another tab, see
	 * bug #155824. So we save the selection iff the clipboard was owned by
	 * the location entry.
	 */
	if (GTK_WIDGET_REALIZED (GTK_WIDGET (priv->icon_entry)))
	{
		GtkWidget *gtkentry = priv->icon_entry->entry;

		clipboard = gtk_widget_get_clipboard (gtkentry,
						      GDK_SELECTION_PRIMARY);
		g_return_if_fail (clipboard != NULL);

		if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (gtkentry) &&
		    gtk_editable_get_selection_bounds (GTK_EDITABLE (gtkentry),
	     					       &start, &end))
		{
			selection = gtk_editable_get_chars (GTK_EDITABLE (gtkentry),
							    start, end);
		}
	}

	if (typed_address != NULL)
	{
		text = typed_address;
	}
	else if (address != NULL && strcmp (address, "about:blank") != 0)
	{
		text = address;
	}
	else
	{
		text = "";
	}

	/* First record the new hash, then update the entry text */
	priv->hash = g_str_hash (address);

	priv->user_changed = FALSE;
	gtk_entry_set_text (GTK_ENTRY (priv->icon_entry->entry), text);
	priv->user_changed = TRUE;

	update_favicon (entry);

	/* Now restore the selection.
	 * Note that it's not owned by the entry anymore!
	 */
	if (selection != NULL)
	{
		gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
					selection, strlen (selection));
		g_free (selection);
	}
}

const char *
ephy_location_entry_get_location (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	return gtk_entry_get_text (GTK_ENTRY (priv->icon_entry->entry));
}

gboolean
ephy_location_entry_reset (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	const char *text, *old_text;
	char *url = NULL;
	gboolean retval;

	g_signal_emit (entry, signals[GET_LOCATION], 0, &url);
	text = url != NULL ? url : "";
	old_text = gtk_entry_get_text (GTK_ENTRY (priv->icon_entry->entry));
	old_text = old_text != NULL ? old_text : "";

	retval = g_str_hash (text) != g_str_hash (old_text);

	ephy_location_entry_set_location (entry, text, NULL);
	g_free (url);

	return retval;
}

void
ephy_location_entry_activate (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));

	gtk_editable_select_region (GTK_EDITABLE(priv->icon_entry->entry),
				    0, -1);
        gtk_window_set_focus (GTK_WINDOW(toplevel),
                              priv->icon_entry->entry);
}

GtkWidget *
ephy_location_entry_get_entry (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	return priv->icon_entry->entry;
}

void
ephy_location_entry_set_favicon (EphyLocationEntry *entry,
				 GdkPixbuf *pixbuf)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	if (priv->favicon != NULL)
	{
		g_object_unref (priv->favicon);
	}

	priv->favicon = pixbuf ? g_object_ref (pixbuf) : NULL;

	update_favicon (entry);
}

void
ephy_location_entry_set_show_lock (EphyLocationEntry *entry,
				   gboolean show_lock)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	g_object_set (priv->lock_ebox, "visible", show_lock, NULL);
}

void
ephy_location_entry_set_lock_stock (EphyLocationEntry *entry,
				    const char *stock_id)

{
	EphyLocationEntryPrivate *priv = entry->priv;

	gtk_image_set_from_stock (GTK_IMAGE (priv->lock), stock_id,
				  GTK_ICON_SIZE_MENU);
}

void
ephy_location_entry_set_lock_tooltip (EphyLocationEntry *entry,
				      const char *tooltip)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	gtk_tooltips_set_tip (priv->tips, priv->lock_ebox, tooltip, NULL);
}
