/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003, 2004  Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005  Christian Persch
 *  Copyright © 2008  Xan López
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-location-entry.h"

#include "ephy-widgets-type-builtins.h"
#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-gui.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-signal-accumulator.h"
#include "ephy-uri-helpers.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#if 0
/* FIXME: Refactor the DNS prefetch, this is a layering violation */
#include <libsoup/soup.h>
#include <webkit2/webkit2.h>
#endif

/**
 * SECTION:ephy-location-entry
 * @short_description: A location entry widget
 * @see_also: #GtkEntry
 *
 * #EphyLocationEntry implements the location bar in the main Epiphany window.
 */

#define EPHY_LOCATION_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryPrivate))

struct _EphyLocationEntryPrivate
{
	GdkPixbuf *favicon;
	GtkTreeModel *model;

	GSList *search_terms;

	char *before_completion;
	char *saved_text;

	guint text_col;
	guint action_col;
	guint keywords_col;
	guint relevance_col;
	guint url_col;
	guint extra_col;
	guint favicon_col;

	guint hash;

	gulong dns_prefetch_handler;

	guint user_changed : 1;
	guint can_redo : 1;
	guint block_update : 1;
	guint original_address : 1;
	guint apply_colors : 1;
	guint needs_reset : 1;
	guint show_favicon : 1;

	GtkTargetList *drag_targets;
	GdkDragAction drag_actions;
};

static const GtkTargetEntry url_drag_types [] =
{
	{ EPHY_DND_URL_TYPE,        0, 0 },
	{ EPHY_DND_URI_LIST_TYPE,   0, 1 },
	{ EPHY_DND_TEXT_TYPE,       0, 2 }
};

static gboolean ephy_location_entry_reset_internal (EphyLocationEntry *, gboolean);

static void textcell_data_func (GtkCellLayout *cell_layout,
				GtkCellRenderer *cell,
				GtkTreeModel *tree_model,
				GtkTreeIter *iter,
				gpointer data);
static void extracell_data_func (GtkCellLayout *cell_layout,
				 GtkCellRenderer *cell,
				 GtkTreeModel *tree_model,
				 GtkTreeIter *iter,
				 gpointer data);

enum
{
	PROP_0,
	PROP_LOCATION,
	PROP_FAVICON,
	PROP_SECURITY_LEVEL,
	PROP_SHOW_FAVICON
};

enum signalsEnum
{
	USER_CHANGED,
	LOCK_CLICKED,
	GET_LOCATION,
	GET_TITLE,
	LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EphyLocationEntry, ephy_location_entry, GTK_TYPE_ENTRY)

static void
ephy_location_entry_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

	switch (prop_id)
	{
	case PROP_LOCATION:
		ephy_location_entry_set_location (entry,
						  g_value_get_string (value));
		break;
	case PROP_FAVICON:
		ephy_location_entry_set_favicon (entry,
						 g_value_get_object (value));
		break;
	case PROP_SECURITY_LEVEL:
		ephy_location_entry_set_security_level (entry,
							g_value_get_enum (value));
		break;
	case PROP_SHOW_FAVICON:
		ephy_location_entry_set_show_favicon (entry,
						      g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id,pspec);
	}
}

static void
ephy_location_entry_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

	switch (prop_id)
	{
	case PROP_LOCATION:
		g_value_set_string (value, ephy_location_entry_get_location (entry));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id,pspec);
	}
}

static void
ephy_location_entry_finalize (GObject *object)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);
	EphyLocationEntryPrivate *priv = entry->priv;
	
	g_free (priv->saved_text);

	if (priv->drag_targets != NULL)
	{
		gtk_target_list_unref (priv->drag_targets);
	}

	if (priv->favicon != NULL)
	{
		g_object_unref (priv->favicon);
	}

	G_OBJECT_CLASS (ephy_location_entry_parent_class)->finalize (object);
}

static void
ephy_location_entry_get_preferred_width (GtkWidget       *widget,
					 gint            *minimum_width,
					 gint            *natural_width)
{
	if (minimum_width)
		*minimum_width = -1;

	if (natural_width)
		*natural_width = 848;
}

static void
ephy_location_entry_copy_clipboard (GtkEntry *entry)
{
	char *text;
	gint start;
	gint end;

	if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
		return;

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);

	if (start == 0)
	{
		char *tmp = text;
		text = ephy_uri_normalize (tmp);
		g_free (tmp);
	}

	gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry),
							  GDK_SELECTION_CLIPBOARD),
							  text, -1);
	g_free (text);
}

static void
ephy_location_entry_cut_clipboard (GtkEntry *entry)
{
	if (!gtk_editable_get_editable (GTK_EDITABLE (entry)))
	{
		gtk_widget_error_bell (GTK_WIDGET (entry));
		return;
	}

	ephy_location_entry_copy_clipboard (entry);
	gtk_editable_delete_selection (GTK_EDITABLE (entry));
}

static void
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkEntryClass *entry_class = GTK_ENTRY_CLASS (klass);

	object_class->get_property = ephy_location_entry_get_property;
	object_class->set_property = ephy_location_entry_set_property;
	object_class->finalize = ephy_location_entry_finalize;
	widget_class->get_preferred_width = ephy_location_entry_get_preferred_width;
	entry_class->copy_clipboard = ephy_location_entry_copy_clipboard;
	entry_class->cut_clipboard = ephy_location_entry_cut_clipboard;

	/**
	* EphyLocationEntry:location:
	*
	* The current location.
	*/
	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_string ("location",
							      "Location",
							      "The current location",
							      "",
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyLocationEntry:favicon:
	*
	* The icon corresponding to the current location.
	*/
	g_object_class_install_property (object_class,
					 PROP_FAVICON,
					 g_param_spec_object ("favicon",
							      "Favicon",
							      "The icon corresponding to the current location",
							      GDK_TYPE_PIXBUF,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyLocationEntry:security-level:
	*
	* State of the security icon.
	*/
	g_object_class_install_property (object_class,
					 PROP_SECURITY_LEVEL,
					 g_param_spec_enum  ("security-level",
							     "Security level",
							     "State of the security icon",
							     EPHY_TYPE_SECURITY_LEVEL,
							     EPHY_SECURITY_LEVEL_NO_SECURITY,
							     G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_SHOW_FAVICON,
					 g_param_spec_boolean ("show-favicon",
							       "Show Favicon",
							       "Whether to show the favicon",
							       TRUE,
							       G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

       /**
	* EphyLocationEntry::user-changed:
	* @entry: the object on which the signal is emitted
	*
	* Emitted when the user changes the contents of the internal #GtkEntry
	*
	*/
	signals[USER_CHANGED] = g_signal_new (
		"user_changed", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationEntryClass, user_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0,
		G_TYPE_NONE);

       /**
	* EphyLocationEntry::lock-clicked:
	* @entry: the object on which the signal is emitted
	*
	* Emitted when the user clicks the security icon inside the
	* #EphyLocationEntry.
	*
	*/
	signals[LOCK_CLICKED] = g_signal_new (
		"lock-clicked",
		EPHY_TYPE_LOCATION_ENTRY,
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationEntryClass, lock_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);

       /**
	* EphyLocationEntry::get-location:
	* @entry: the object on which the signal is emitted
	* Returns: the current page address as a string
	*
	* For drag and drop purposes, the location bar will request you the
	* real address of where it is pointing to. The signal handler for this
	* function should return the address of the currently loaded site.
	*
	*/
	signals[GET_LOCATION] = g_signal_new (
		"get-location", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationEntryClass, get_location),
		ephy_signal_accumulator_string, NULL,
		g_cclosure_marshal_generic,
		G_TYPE_STRING,
		0,
		G_TYPE_NONE);

       /**
	* EphyLocationEntry::get-title:
	* @entry: the object on which the signal is emitted
	* Returns: the current page title as a string
	*
	* For drag and drop purposes, the location bar will request you the
	* title of where it is pointing to. The signal handler for this
	* function should return the title of the currently loaded site.
	*
	*/
	signals[GET_TITLE] = g_signal_new (
		"get-title", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationEntryClass, get_title),
		ephy_signal_accumulator_string, NULL,
		g_cclosure_marshal_generic,
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

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	priv->original_address = text != NULL &&
				 g_str_hash (text) == priv->hash;
}

static void
update_favicon (EphyLocationEntry *lentry)
{
	EphyLocationEntryPrivate *priv = lentry->priv;
	GtkEntry *entry = GTK_ENTRY (lentry);

	/* Only show the favicon if the entry's text is the
	 * address of the current page.
	 */
	if (priv->show_favicon && priv->favicon != NULL && priv->original_address)
	{
		gtk_entry_set_icon_from_pixbuf (entry,
						GTK_ENTRY_ICON_PRIMARY,
						priv->favicon);
	}
	else if (priv->show_favicon)
	{
		const char *icon_name;

		/* Here we could consider using fallback favicon that matches
		 * the page MIME type, though text/html should be good enough
		 * most of the time. See #337140
		 */
		if (gtk_entry_get_text_length (entry) > 0)
			icon_name = "text-x-generic-symbolic";
		else
			icon_name = "edit-find-symbolic";

		gtk_entry_set_icon_from_icon_name (entry,
						   GTK_ENTRY_ICON_PRIMARY,
						   icon_name);
	}
	else
	{
		gtk_entry_set_icon_from_icon_name (entry,
						   GTK_ENTRY_ICON_PRIMARY,
						   NULL);
	}
}

static void
editable_changed_cb (GtkEditable *editable,
		     EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	update_address_state (entry);

	if (priv->block_update == TRUE) 
		return;
	else
	{
		priv->user_changed = TRUE;
		priv->can_redo = FALSE;
	}	
	
	g_signal_emit (entry, signals[USER_CHANGED], 0);
}

static gboolean
entry_key_press_cb (GtkEntry *entry,
		    GdkEventKey *event,
		    EphyLocationEntry *location_entry)
{
	guint state = event->state & gtk_accelerator_get_default_mod_mask ();


	if (event->keyval == GDK_KEY_Escape && state == 0)
	{
		ephy_location_entry_reset_internal (location_entry, TRUE);
		/* don't return TRUE since we want to cancel the autocompletion popup too */
	}

	if (event->keyval == GDK_KEY_l && state == GDK_CONTROL_MASK)
	{
		/* Make sure the location is activated on CTRL+l even when the
		 * completion popup is shown and have an active keyboard grab.
		 */
		ephy_location_entry_activate (location_entry);
	}

	return FALSE;
}

static gboolean
entry_key_press_after_cb (GtkEntry *entry,
			  GdkEventKey *event,
			  EphyLocationEntry *lentry)
{
	EphyLocationEntryPrivate *priv = lentry->priv;

	guint state = event->state & gtk_accelerator_get_default_mod_mask ();

	if ((event->keyval == GDK_KEY_Return ||
	     event->keyval == GDK_KEY_KP_Enter ||
	     event->keyval == GDK_KEY_ISO_Enter) &&
	    (state == GDK_CONTROL_MASK ||
	     state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
	{
		//gtk_im_context_reset (entry->im_context);

		priv->needs_reset = TRUE;
		g_signal_emit_by_name (entry, "activate");

		return TRUE;
	}
	
	if ((event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down)
	    && state == 0)
	{
		/* If we are focusing the entry, with the cursor at the end of it
		 * we emit the changed signal, so that the completion popup appears */
		const char *string;
		
		string = gtk_entry_get_text (entry);
		if (gtk_editable_get_position (GTK_EDITABLE (entry)) == strlen (string))
		{
			g_signal_emit_by_name (entry, "changed", 0);
			return TRUE;
		}
	}

	return FALSE;
}

static void
entry_activate_after_cb (GtkEntry *entry,
			 EphyLocationEntry *lentry)
{
	EphyLocationEntryPrivate *priv = lentry->priv;
	
	priv->user_changed = FALSE;

	if (priv->needs_reset)
	{
		ephy_location_entry_reset_internal (lentry, TRUE);
		priv->needs_reset = FALSE;
	}
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel *model,
		   GtkTreeIter *iter,
		   EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	char *item = NULL;
	guint state;

	gtk_tree_model_get (model, iter,
			    priv->action_col, &item, -1);
	if (item == NULL) return FALSE;

	ephy_gui_get_current_event (NULL, &state, NULL);

	priv->needs_reset = (state == GDK_CONTROL_MASK ||
			     state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK));

	ephy_location_entry_set_location (entry, item);
	//gtk_im_context_reset (GTK_ENTRY (entry)->im_context);
	g_signal_emit_by_name (entry, "activate");

	g_free (item);

	return TRUE;
}

static void
action_activated_after_cb (GtkEntryCompletion *completion,
			   gint index,
			   EphyLocationEntry *lentry)
{
	guint state, button;

	ephy_gui_get_current_event (NULL, &state, &button);
	if ((state == GDK_CONTROL_MASK ||
	     state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) ||
	    button == 2)
	{
		ephy_location_entry_reset_internal (lentry, TRUE);
	}
}

static gboolean
entry_drag_motion_cb (GtkWidget        *widget,
		      GdkDragContext   *context,
		      gint              x,
		      gint              y,
		      guint             time)
{
	return FALSE;
}

static gboolean
entry_drag_drop_cb (GtkWidget          *widget,
		    GdkDragContext     *context,
		    gint                x,
		    gint                y,
		    guint               time)
{
	return FALSE;
}

static void
entry_clear_activate_cb (GtkMenuItem *item,
			 EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;

	priv->block_update = TRUE;
	gtk_entry_set_text (GTK_ENTRY (entry), "");
	priv->block_update = FALSE;
	priv->user_changed = TRUE;
}

static void
entry_redo_activate_cb (GtkMenuItem *item,
			EphyLocationEntry *entry)
{
	ephy_location_entry_undo_reset (entry);
}

static void
entry_undo_activate_cb (GtkMenuItem *item,
			EphyLocationEntry *entry)
{
	ephy_location_entry_reset_internal (entry, FALSE);
}

static void
entry_populate_popup_cb (GtkEntry *entry,
			 GtkMenu *menu,
			 EphyLocationEntry *lentry)
{
	EphyLocationEntryPrivate *priv = lentry->priv;
	GtkWidget *clear_menuitem, *undo_menuitem, *redo_menuitem, *separator;
	GList *children, *item;
	int pos = 0, sep = 0;
	gboolean is_editable;

	/* Translators: the mnemonic shouldn't conflict with any of the
	 * standard items in the GtkEntry context menu (Cut, Copy, Paste, Delete,
	 * Select All, Input Methods and Insert Unicode control character.)
	 */
	clear_menuitem = gtk_menu_item_new_with_mnemonic (_("Cl_ear"));
	g_signal_connect (clear_menuitem , "activate",
			  G_CALLBACK (entry_clear_activate_cb), lentry);
	is_editable = gtk_editable_get_editable (GTK_EDITABLE (entry));
	gtk_widget_set_sensitive (clear_menuitem, is_editable);
	gtk_widget_show (clear_menuitem);

	/* search for the 2nd separator (the one after Select All) in the context
	 * menu, and insert this menu item before it.
	 * It's a bit of a hack, but there seems to be no better way to do it :/
	 */
	children = gtk_container_get_children (GTK_CONTAINER (menu));
	for (item = children; item != NULL && sep < 2; item = item->next, pos++)
	{
		if (GTK_IS_SEPARATOR_MENU_ITEM (item->data)) sep++;
	}

	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), clear_menuitem, pos - 1);
	
	undo_menuitem = gtk_menu_item_new_with_mnemonic (_("_Undo"));
	gtk_widget_set_sensitive (undo_menuitem, priv->user_changed);
	g_signal_connect (undo_menuitem, "activate",
			  G_CALLBACK (entry_undo_activate_cb), lentry);
	gtk_widget_show (undo_menuitem);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), undo_menuitem, 0);
	
	redo_menuitem = gtk_menu_item_new_with_mnemonic (_("_Redo"));
	gtk_widget_set_sensitive (redo_menuitem, priv->can_redo);
	g_signal_connect (redo_menuitem, "activate",
			  G_CALLBACK (entry_redo_activate_cb), lentry);
	gtk_widget_show (redo_menuitem);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), redo_menuitem, 1);
	
	separator = gtk_separator_menu_item_new ();
	gtk_widget_show (separator);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), separator, 2);
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
sanitize_location (char **url)
{
	char *str;

	/* Do not show internal ephy-about: protocol to users */
	if (g_str_has_prefix (*url, EPHY_ABOUT_SCHEME)) {
		str = g_strdup_printf ("about:%s", *url + strlen (EPHY_ABOUT_SCHEME) + 1);
		g_free (*url);
		*url = str;
	}
}

#define DRAG_ICON_LAYOUT_PADDING	5
#define DRAG_ICON_ICON_PADDING		10
#define DRAG_ICON_MAX_WIDTH_CHARS	32

static cairo_surface_t *
favicon_create_drag_surface (EphyLocationEntry *entry,
			     GtkWidget *widget)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	char *title = NULL, *address = NULL;
	GString *text;
	GtkStyleContext *style;
	const PangoFontDescription *font_desc;
	cairo_surface_t *surface;
	PangoContext *context;
	PangoLayout  *layout;
	PangoFontMetrics *metrics;
	int surface_height, surface_width;
	int layout_width, layout_height;
	int icon_width = 0, icon_height = 0, favicon_offset_x = 0;
	int char_width;
	cairo_t *cr;
	GtkStateFlags state;
	GdkRGBA color;
	GdkPixbuf *favicon;

	state = gtk_widget_get_state_flags (widget);

	g_signal_emit (entry, signals[GET_LOCATION], 0, &address);
	sanitize_location (&address);
	g_signal_emit (entry, signals[GET_TITLE], 0, &title);
	if (address == NULL || title == NULL) return NULL;

	/* Compute text */
	title = g_strstrip (title);

	text = g_string_sized_new (strlen (address) + strlen (title) + 2);
	if (title[0] != '\0')
	{
		g_string_append (text, title);
		g_string_append (text, "\n");
	}

	if (address[0] != '\0')
	{
		g_string_append (text, address);
	}

	if (priv->favicon != NULL)
		favicon = g_object_ref (priv->favicon);
	else
		favicon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						    "text-x-generic-symbolic",
						    16,
						    0, NULL);
	if (favicon != NULL)
	{
		icon_width = gdk_pixbuf_get_width (favicon);
		icon_height = gdk_pixbuf_get_height (favicon);
	}

	context = gtk_widget_get_pango_context (widget);
	layout = pango_layout_new (context);

	style = gtk_widget_get_style_context (GTK_WIDGET (entry));
	gtk_style_context_get (style, GTK_STATE_FLAG_NORMAL,
			       "font", &font_desc, NULL);
	metrics = pango_context_get_metrics (context,
		                             font_desc,
					     pango_context_get_language (context));

	char_width = pango_font_metrics_get_approximate_digit_width (metrics);
	pango_font_metrics_unref (metrics);

	pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_width (layout, char_width * DRAG_ICON_MAX_WIDTH_CHARS);
	pango_layout_set_text (layout, text->str, text->len);

	pango_layout_get_pixel_size (layout, &layout_width, &layout_height);

	if (favicon != NULL)
	{
		favicon_offset_x = icon_width + (2 * DRAG_ICON_ICON_PADDING);
	}

	surface_width = layout_width + favicon_offset_x +
			(DRAG_ICON_LAYOUT_PADDING * 3);
	surface_height = MAX (layout_height, icon_height) +
			(DRAG_ICON_LAYOUT_PADDING * 2);

	surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
						     CAIRO_CONTENT_COLOR,
						     surface_width + 2,
						     surface_height + 2);
	cr = cairo_create (surface);

	cairo_rectangle (cr, 1, 1, surface_width, surface_height);
	cairo_set_line_width (cr, 1.0);

	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_stroke_preserve (cr);

	gtk_style_context_get_background_color (style, state, &color);
	gdk_cairo_set_source_rgba (cr, &color);
	cairo_fill (cr);

	if (favicon != NULL)
	{
		double x;
		double y;

		x = 1 + DRAG_ICON_LAYOUT_PADDING + DRAG_ICON_ICON_PADDING;
		y = (surface_height - icon_height) / 2;
		gdk_cairo_set_source_pixbuf (cr, favicon, x, y);
		cairo_rectangle (cr, x, y, icon_width, icon_height);
		cairo_fill (cr);
	}

	cairo_move_to (cr,
		       1 + DRAG_ICON_LAYOUT_PADDING + favicon_offset_x,
		       1 + DRAG_ICON_LAYOUT_PADDING);
	gtk_style_context_get_color (style, state, &color);
	gdk_cairo_set_source_rgba (cr, &color);
	pango_cairo_show_layout (cr, layout);

	cairo_destroy (cr);
	g_object_unref (layout);

	g_free (address);
	g_free (title);
	g_string_free (text, TRUE);
	g_clear_object (&favicon);

	return surface;
}

static void
favicon_drag_begin_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       EphyLocationEntry *lentry)
{
	cairo_surface_t *surface;
	GtkEntry *entry;
	gint index;

	entry = GTK_ENTRY (widget);
	
	index = gtk_entry_get_current_icon_drag_source (entry);
	if (index != GTK_ENTRY_ICON_PRIMARY)
		return;

	surface = favicon_create_drag_surface (lentry, widget);

	if (surface != NULL)
	{
		gtk_drag_set_icon_surface (context, surface);
		cairo_surface_destroy (surface);
	}
}

static void
favicon_drag_data_get_cb (GtkWidget *widget,
			  GdkDragContext *context,
			  GtkSelectionData *selection_data,
			  guint info,
			  guint32 time,
			  EphyLocationEntry *lentry)
{
	gint index;
	GtkEntry *entry;

	g_assert (widget != NULL);
	g_return_if_fail (context != NULL);

	entry = GTK_ENTRY (widget);

	index = gtk_entry_get_current_icon_drag_source (entry);
	if (index == GTK_ENTRY_ICON_PRIMARY)
	{
		ephy_dnd_drag_data_get (widget, context, selection_data,
					time, lentry, each_url_get_data_binder);
	}
}

static gboolean
icon_button_press_event_cb (GtkWidget *entry,
			    GtkEntryIconPosition position,
			    GdkEventButton *event,
			    EphyLocationEntry *lentry)
{
	guint state = event->state & gtk_accelerator_get_default_mod_mask ();

	if (event->type == GDK_BUTTON_PRESS && 
	    event->button == 1 &&
	    state == 0 /* left */)
	{
		if (position == GTK_ENTRY_ICON_PRIMARY)
		{
			GtkWidget *toplevel;
		
			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));
			gtk_window_set_focus (GTK_WINDOW (toplevel), entry);

			gtk_editable_select_region (GTK_EDITABLE (entry), 
						    0, -1);
		}
		else
		{
			g_signal_emit (lentry, signals[LOCK_CLICKED], 0);
		}

		return TRUE;
	}

	return FALSE;
}

static void
ephy_location_entry_construct_contents (EphyLocationEntry *lentry)
{
	GtkWidget *entry = GTK_WIDGET (lentry);
	EphyLocationEntryPrivate *priv = lentry->priv;

	LOG ("EphyLocationEntry constructing contents %p", lentry);

	/* Favicon */
	priv->drag_targets = gtk_target_list_new (url_drag_types,
						  G_N_ELEMENTS (url_drag_types));
	priv->drag_actions = GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK;

	gtk_entry_set_icon_drag_source (GTK_ENTRY (entry),
					GTK_ENTRY_ICON_PRIMARY,
					priv->drag_targets,
					priv->drag_actions);

	gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
					 GTK_ENTRY_ICON_PRIMARY,
					 _("Drag and drop this icon to create a link to this page"));

	gtk_drag_dest_set (entry,
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			   url_drag_types,
			   G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_object_connect (entry,
			  "signal::icon-press", G_CALLBACK (icon_button_press_event_cb), lentry,
			  "signal::populate-popup", G_CALLBACK (entry_populate_popup_cb), lentry,
			  "signal::key-press-event", G_CALLBACK (entry_key_press_cb), lentry,
			  "signal::changed", G_CALLBACK (editable_changed_cb), lentry,
			  "signal::drag-motion", G_CALLBACK (entry_drag_motion_cb), lentry,
			  "signal::drag-drop", G_CALLBACK (entry_drag_drop_cb), lentry,
			  "signal::drag-data-get", G_CALLBACK (favicon_drag_data_get_cb), lentry,
			  NULL);

	g_signal_connect_after (entry, "key-press-event",
				G_CALLBACK (entry_key_press_after_cb), lentry);
	g_signal_connect_after (entry, "activate",
				G_CALLBACK (entry_activate_after_cb), lentry);
	g_signal_connect_after (entry, "drag-begin",
				G_CALLBACK (favicon_drag_begin_cb), lentry);
}

static void
ephy_location_entry_init (EphyLocationEntry *le)
{
	EphyLocationEntryPrivate *p;

	LOG ("EphyLocationEntry initialising %p", le);

	p = EPHY_LOCATION_ENTRY_GET_PRIVATE (le);
	le->priv = p;

	p->user_changed = FALSE;
	p->block_update = FALSE;
	p->saved_text = NULL;
	p->show_favicon = TRUE;
	p->dns_prefetch_handler = 0;

	ephy_location_entry_construct_contents (le);
}

GtkWidget *
ephy_location_entry_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

#if 0
/* FIXME: Refactor the DNS prefetch, this is a layering violation */
typedef struct {
	SoupURI *uri;
	EphyLocationEntry *entry;
} PrefetchHelper;

static void
free_prefetch_helper (PrefetchHelper *helper)
{
	soup_uri_free (helper->uri);
	g_object_unref (helper->entry);
	g_slice_free (PrefetchHelper, helper);
}

static gboolean
do_dns_prefetch (PrefetchHelper *helper)
{
	EphyEmbedShell *shell = ephy_embed_shell_get_default ();

	if (helper->uri)
		webkit_web_context_prefetch_dns (ephy_embed_shell_get_web_context (shell), helper->uri->host);

	helper->entry->priv->dns_prefetch_handler = 0;

	return FALSE;
}

static void
schedule_dns_prefetch (EphyLocationEntry *entry, guint interval, const gchar *url)
{
	PrefetchHelper *helper;
	SoupURI *uri;

	uri = soup_uri_new (url);
	if (!uri || !uri->host) {
		soup_uri_free (uri);
		return;
	}

	if (entry->priv->dns_prefetch_handler)
		g_source_remove (entry->priv->dns_prefetch_handler);

	helper = g_slice_new0 (PrefetchHelper);
	helper->entry = g_object_ref (entry);
	helper->uri = uri;

	entry->priv->dns_prefetch_handler =
		g_timeout_add_full (G_PRIORITY_DEFAULT, interval,
				    (GSourceFunc) do_dns_prefetch, helper,
				    (GDestroyNotify) free_prefetch_helper);
	g_source_set_name_by_id (entry->priv->dns_prefetch_handler, "[epiphany] do_dns_prefetch");
}
#endif

static gboolean
cursor_on_match_cb  (GtkEntryCompletion *completion,
		     GtkTreeModel *model,
		     GtkTreeIter *iter,
		     EphyLocationEntry *le)
{
	char *url = NULL;
	GtkWidget *entry;

	gtk_tree_model_get (model, iter,
			    le->priv->url_col,
			    &url, -1);
	entry = gtk_entry_completion_get_entry (completion);

	/* Prevent the update so we keep the highlight from our input.
	 * See textcell_data_func().
	 */
	le->priv->block_update = TRUE;
	gtk_entry_set_text (GTK_ENTRY (entry), url);
	gtk_editable_set_position (GTK_EDITABLE (entry), -1);
	le->priv->block_update = FALSE;

#if 0
/* FIXME: Refactor the DNS prefetch, this is a layering violation */
	schedule_dns_prefetch (le, 250, (const gchar*) url);
#endif

	g_free (url);

	return TRUE;
}

static void
extracell_data_func (GtkCellLayout *cell_layout,
			GtkCellRenderer *cell,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer data)
{
	EphyLocationEntryPrivate *priv;
	gboolean is_bookmark = FALSE;
	GValue visible = { 0, };

	priv = EPHY_LOCATION_ENTRY (data)->priv;
	gtk_tree_model_get (tree_model, iter,
			    priv->extra_col, &is_bookmark,
			    -1);

	if (is_bookmark)
		g_object_set (cell,
			      "icon-name", "user-bookmarks-symbolic",
			      NULL);

	g_value_init (&visible, G_TYPE_BOOLEAN);
	g_value_set_boolean (&visible, is_bookmark);
	g_object_set_property (G_OBJECT (cell), "visible", &visible);
	g_value_unset (&visible);
}

/**
 * ephy_location_entry_set_match_func:
 * @entry: an #EphyLocationEntry widget
 * @match_func: a #GtkEntryCompletionMatchFunc
 * @user_data: user_data to pass to the @match_func
 * @notify: a #GDestroyNotify, like the one given to
 * gtk_entry_completion_set_match_func
 *
 * Sets the match_func for the internal #GtkEntryCompletion to @match_func.
 *
 **/
void
ephy_location_entry_set_match_func (EphyLocationEntry *entry, 
				GtkEntryCompletionMatchFunc match_func,
				gpointer user_data,
				GDestroyNotify notify)
{
	GtkEntryCompletion *completion;
	
	completion = gtk_entry_get_completion (GTK_ENTRY (entry));
	gtk_entry_completion_set_match_func (completion, match_func, user_data, notify);
}

/**
 * ephy_location_entry_set_completion:
 * @entry: an #EphyLocationEntry widget
 * @model: the #GtkModel for the completion
 * @text_col: column id to access #GtkModel relevant data
 * @action_col: column id to access #GtkModel relevant data
 * @keywords_col: column id to access #GtkModel relevant data
 * @relevance_col: column id to access #GtkModel relevant data
 * @url_col: column id to access #GtkModel relevant data
 * @extra_col: column id to access #GtkModel relevant data
 * @favicon_col: column id to access #GtkModel relevant data
 *
 * Initializes @entry to have a #GtkEntryCompletion using @model as the
 * internal #GtkModel. The *_col arguments are for internal data retrieval from
 * @model, like when setting the text property of one of the #GtkCellRenderer
 * of the completion.
 *
 **/
void
ephy_location_entry_set_completion (EphyLocationEntry *entry,
				    GtkTreeModel *model,
				    guint text_col,
				    guint action_col,
				    guint keywords_col,
				    guint relevance_col,
				    guint url_col,
				    guint extra_col,
				    guint favicon_col)
{
	GtkEntryCompletion *completion;
	GtkCellRenderer *cell;
	EphyLocationEntryPrivate *priv = entry->priv;

	priv->text_col = text_col;
	priv->action_col = action_col;
	priv->keywords_col = keywords_col;
	priv->relevance_col = relevance_col;
	priv->url_col = url_col;
	priv->extra_col = extra_col;
	priv->favicon_col = favicon_col;

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, model);
	g_signal_connect (completion, "match-selected",
			  G_CALLBACK (match_selected_cb), entry);
	g_signal_connect_after (completion, "action-activated",
				G_CALLBACK (action_activated_after_cb), entry);

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
				    cell, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
				       cell, "pixbuf", favicon_col);

	/* Pixel-perfect aligment with the location entry favicon
	 * (16x16). Consider that this /might/ depend on the theme.
	 *
	 * The GtkEntryCompletion can not be themed so we work-around
	 * that with padding and fixed sizes.
	 * For the first cell, this is:
	 *
	 * ___+++++iiiiiiiiiiiiiiii++__ttt...bbb++++++__
	 *
	 * _ = widget spacing, can not be handled (3 px)
	 * + = padding (5 px) (ICON_PADDING_LEFT)
	 * i = the icon (16 px) (ICON_CONTENT_WIDTH)
	 * + = padding (2 px) (ICON_PADDING_RIGHT) (cut by the fixed_size)
	 * _ = spacing between cells, can not be handled (2 px)
	 * t = the text (expands)
	 * b = bookmark icon (16 px)
	 * + = padding (6 px) (BKMK_PADDING_RIGHT)
	 * _ = widget spacing, can not be handled (2 px)
	 *
	 * Each character is a pixel.
	 *
	 * The text cell and the bookmark icon cell are much more
	 * flexible in its aligment, because they do not have to align
	 * with anything in the entry.
	 */

#define ROW_PADDING_VERT 4

#define ICON_PADDING_LEFT 5
#define ICON_CONTENT_WIDTH 16
#define ICON_PADDING_RIGHT 9

#define ICON_CONTENT_HEIGHT 16

#define TEXT_PADDING_LEFT 0

#define BKMK_PADDING_RIGHT 6

	gtk_cell_renderer_set_padding
		(cell, ICON_PADDING_LEFT, ROW_PADDING_VERT);
	gtk_cell_renderer_set_fixed_size
		(cell,
		 (ICON_PADDING_LEFT + ICON_CONTENT_WIDTH + ICON_PADDING_RIGHT),
		 ICON_CONTENT_HEIGHT);
	gtk_cell_renderer_set_alignment (cell, 0.0, 0.5);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
				    cell, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
				       cell, "markup", text_col);

	/* Pixel-perfect aligment with the text in the location entry.
	 * See above.
	 */
	gtk_cell_renderer_set_padding
		(cell, TEXT_PADDING_LEFT, ROW_PADDING_VERT);
	gtk_cell_renderer_set_alignment (cell, 0.0, 0.5);

        /*
         * As the width of the entry completion is known in advance 
         * (as big as the entry you are completing on), we can set 
         * any fixed width (the 1 is just this random number here). 
         * Since the height is known too, we avoid computing the actual 
         * sizes of the cells, which takes a lot of CPU time and does
         * not get used anyway.
         */
	gtk_cell_renderer_set_fixed_size (cell, 1, -1);
	gtk_cell_renderer_text_set_fixed_height_from_font (GTK_CELL_RENDERER_TEXT (cell), 2);

	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (completion),
				  cell, FALSE);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion),
					    cell, extracell_data_func,
					    entry,
					    NULL);

	/* Pixel-perfect aligment. This just keeps the same margin from
	 * the border than the favicon on the other side. See above. */
	gtk_cell_renderer_set_padding
		(cell, BKMK_PADDING_RIGHT, ROW_PADDING_VERT);

	g_object_set (completion, "inline-selection", TRUE, NULL);
	g_signal_connect (completion, "cursor-on-match",
			  G_CALLBACK (cursor_on_match_cb), entry);

	gtk_entry_set_completion (GTK_ENTRY (entry), completion);
	g_object_unref (completion);
}

/**
 * ephy_location_entry_set_location:
 * @entry: an #EphyLocationEntry widget
 * @address: new location address
 *
 * Sets the current address of @entry to @address.
 **/
void
ephy_location_entry_set_location (EphyLocationEntry *entry,
				  const char *address)
{
	GtkWidget *widget = GTK_WIDGET (entry);
	EphyLocationEntryPrivate *priv = entry->priv;
	GtkClipboard *clipboard;
	const char *text;
	char *effective_text = NULL, *selection = NULL;
	int start, end;

	/* Setting a new text will clear the clipboard. This makes it impossible
	 * to copy&paste from the location entry of one tab into another tab, see
	 * bug #155824. So we save the selection iff the clipboard was owned by
	 * the location entry.
	 */
	if (gtk_widget_get_realized (widget))
	{
		clipboard = gtk_widget_get_clipboard (widget,
						      GDK_SELECTION_PRIMARY);
		g_return_if_fail (clipboard != NULL);

		if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (widget) &&
		    gtk_editable_get_selection_bounds (GTK_EDITABLE (widget),
	     					       &start, &end))
		{
			selection = gtk_editable_get_chars (GTK_EDITABLE (widget),
							    start, end);
		}
	}

	if (address != NULL)
	{
		if (g_str_has_prefix (address, EPHY_ABOUT_SCHEME))
			effective_text = g_strdup_printf ("about:%s",
							  address + strlen (EPHY_ABOUT_SCHEME) + 1);
		text = address;
		gtk_entry_set_icon_drag_source (GTK_ENTRY (entry),
						GTK_ENTRY_ICON_PRIMARY,
						priv->drag_targets,
						priv->drag_actions);
	}
	else
	{
		text = "";
		gtk_entry_set_icon_drag_source (GTK_ENTRY (entry),
						GTK_ENTRY_ICON_PRIMARY,
						NULL,
						GDK_ACTION_DEFAULT);
	}

	/* First record the new hash, then update the entry text */
	priv->hash = g_str_hash (effective_text ? effective_text : text);

	priv->block_update = TRUE;
	gtk_entry_set_text (GTK_ENTRY (widget), effective_text ? effective_text : text);
	priv->block_update = FALSE;
	g_free (effective_text);

	/* We need to call update_address_state() here, as the 'changed' signal
	 * may not get called if the user has typed in the exact correct url */
	update_address_state (entry);
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

/**
 * ephy_location_entry_get_can_undo:
 * @entry: an #EphyLocationEntry widget
 *
 * Wheter @entry can restore the displayed user modified text to the unmodified 
 * previous text.
 *
 * Return value: TRUE or FALSE indicating if the text can be restored
 *
 **/
gboolean
ephy_location_entry_get_can_undo (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	
	return priv->user_changed;
}

/**
 * ephy_location_entry_get_can_redo:
 * @entry: an #EphyLocationEntry widget
 *
 * Wheter @entry can restore the displayed text to the user modified version
 * before the undo.
 *
 * Return value: TRUE or FALSE indicating if the text can be restored
 *
 **/
gboolean
ephy_location_entry_get_can_redo (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	
	return priv->can_redo;
}

/**
 * ephy_location_entry_get_location:
 * @entry: an #EphyLocationEntry widget
 *
 * Retrieves the text displayed by the internal #GtkEntry of @entry. This is
 * the currently displayed text, like in any #GtkEntry.
 *
 * Return value: the text inside the inner #GtkEntry of @entry, owned by GTK+
 *
 **/
const char *
ephy_location_entry_get_location (EphyLocationEntry *entry)
{
	return gtk_entry_get_text (GTK_ENTRY (entry));
}

static gboolean
ephy_location_entry_reset_internal (EphyLocationEntry *entry,
				    gboolean notify)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	const char *text, *old_text;
	char *url = NULL;
	gboolean retval;

	g_signal_emit (entry, signals[GET_LOCATION], 0, &url);
	text = url != NULL ? url : "";
	old_text = gtk_entry_get_text (GTK_ENTRY (entry));
	old_text = old_text != NULL ? old_text : "";

	g_free (priv->saved_text);
	priv->saved_text = g_strdup (old_text);
	priv->can_redo = TRUE;

	retval = g_str_hash (text) != g_str_hash (old_text);

	ephy_location_entry_set_location (entry, text);
	g_free (url);

	if (notify)
	{
		g_signal_emit (entry, signals[USER_CHANGED], 0);
	}
	
	priv->user_changed = FALSE;

	return retval;
}

/**
 * ephy_location_entry_undo_reset:
 * @entry: an #EphyLocationEntry widget
 *
 * Undo a previous ephy_location_entry_reset.
 *
 **/
void
ephy_location_entry_undo_reset (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	
	gtk_entry_set_text (GTK_ENTRY (entry), priv->saved_text);
	priv->can_redo = FALSE;
	priv->user_changed = TRUE;
}

/**
 * ephy_location_entry_reset:
 * @entry: an #EphyLocationEntry widget
 *
 * Restore the @entry to the text corresponding to the current location, this
 * does not fire the user_changed signal. This is called each time the user
 * presses Escape while the location entry is selected.
 *
 * Return value: TRUE on success, FALSE otherwise
 *
 **/
gboolean
ephy_location_entry_reset (EphyLocationEntry *entry)
{
	return ephy_location_entry_reset_internal (entry, FALSE);
}

/**
 * ephy_location_entry_activate:
 * @entry: an #EphyLocationEntry widget
 *
 * Set focus on @entry and select the text whithin. This is called when the
 * user hits Control+L.
 *
 **/
void
ephy_location_entry_activate (EphyLocationEntry *entry)
{
	GtkWidget *toplevel, *widget = GTK_WIDGET (entry);

	toplevel = gtk_widget_get_toplevel (widget);

	gtk_editable_select_region (GTK_EDITABLE (entry),
				    0, -1);
	gtk_window_set_focus (GTK_WINDOW (toplevel),
			      widget);
}

/**
 * ephy_location_entry_set_favicon:
 * @entry: an #EphyLocationEntry widget
 * @pixbuf: a #GdkPixbuf to be set as the icon of the entry
 *
 * Sets the icon in the internal #EphyIconEntry of @entry
 *
 **/
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
ephy_location_entry_set_show_favicon (EphyLocationEntry *entry,
				      gboolean show_favicon)
{
	EphyLocationEntryPrivate *priv;

	g_return_if_fail (EPHY_IS_LOCATION_ENTRY (entry));

	priv = entry->priv;

	priv->show_favicon = show_favicon != FALSE;

	update_favicon (entry);
}

/**
 * ephy_location_entry_set_security_level:
 * @entry: an #EphyLocationEntry widget
 * @state: the #EphySecurityLevel
 *
 * Set the lock icon to be displayed
 *
 **/
void
ephy_location_entry_set_security_level (EphyLocationEntry *entry,
				        EphySecurityLevel security_level)

{
	const char *icon_name;

	g_return_if_fail (EPHY_IS_LOCATION_ENTRY (entry));

	icon_name = ephy_security_level_to_icon_name (security_level);
	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
					   GTK_ENTRY_ICON_SECONDARY,
					   icon_name);
}

/**
 * ephy_location_entry_set_lock_tooltip:
 * @entry: an #EphyLocationEntry widget
 * @tooltip: the text to be set in the tooltip for the lock icon
 *
 * Set the text to be displayed when hovering the lock icon of @entry.
 *
 **/
void
ephy_location_entry_set_lock_tooltip (EphyLocationEntry *entry,
				      const char *tooltip)
{
	gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
					 GTK_ENTRY_ICON_SECONDARY,
					 tooltip);
}

/**
 * ephy_location_entry_get_search_terms:
 * @entry: an #EphyLocationEntry widget
 *
 * Return the internal #GSList containing the search terms as #GRegex
 * instances, formed in @entry on user changes.
 *
 * Return value: the internal #GSList
 *
 **/
GSList *
ephy_location_entry_get_search_terms (EphyLocationEntry *entry)
{
	EphyLocationEntryPrivate *priv = entry->priv;
	
	return priv->search_terms;
}
