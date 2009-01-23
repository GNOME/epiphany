/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8; -*- */
/*
 *  Copyright © 2004 Tommi Komulainen
 *  Copyright © 2004, 2005 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "ephy-embed-utils.h"
#include "ephy-find-toolbar.h"
#include "ephy-debug.h"

#include <webkit/webkit.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define EPHY_FIND_TOOLBAR_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarPrivate))

struct _EphyFindToolbarPrivate
{
	EphyWindow *window;
	WebKitWebView *web_view;
	GtkWidget *entry;
	GtkWidget *label;
	GtkToolItem *next;
	GtkToolItem *prev;
	GtkToolItem *sep;
	GtkToolItem *status_item;
	GtkWidget *status_label;
	GtkWidget *case_sensitive;
	gulong set_focus_handler;
	guint source_id;
	guint find_again_source_id;
	char *find_string;
	guint preedit_changed : 1;
	guint prevent_activate : 1;
	guint activated : 1;
	guint links_only : 1;
	guint typing_ahead : 1;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

enum
{
	NEXT,
	PREVIOUS,
	CLOSE,
	LAST_SIGNAL
};

typedef enum
{
	EPHY_FIND_FOUND		= 0,
	EPHY_FIND_NOTFOUND	= 1,
	EPHY_FIND_FOUNDWRAPPED	= 2
} EphyEmbedFindResult;

static guint signals[LAST_SIGNAL];

/* private functions */

static void
scroll_lines (WebKitWebView *web_view,
              int num_lines)
{
        GtkScrolledWindow *scrolled_window;
        GtkAdjustment *vadj;
        gdouble value;

        scrolled_window = GTK_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (web_view)));
        vadj = gtk_scrolled_window_get_vadjustment (scrolled_window);

        value = vadj->value + (num_lines * vadj->step_increment);
        gtk_adjustment_set_value (vadj, value);
}

static void
scroll_pages (WebKitWebView *web_view,
              int num_pages)
{
        GtkScrolledWindow *scrolled_window;
        GtkAdjustment *vadj;
        gdouble value;

        scrolled_window = GTK_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (web_view)));
        vadj = gtk_scrolled_window_get_vadjustment (scrolled_window);

        value = vadj->value + (num_pages * vadj->page_increment);
        gtk_adjustment_set_value (vadj, value);
}

static gboolean
set_status_notfound_cb (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv;
	PangoFontDescription *pango_desc = NULL;

	priv = toolbar->priv;

	pango_desc = pango_font_description_new ();
	gtk_widget_modify_font (priv->status_label, pango_desc);
	pango_font_description_free (pango_desc);

	priv->source_id = 0;

	return FALSE;
}

static void
set_status (EphyFindToolbar *toolbar,
	    EphyEmbedFindResult result)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	char *text = NULL;
	PangoFontDescription *pango_desc = NULL;

	switch (result)
	{
		case EPHY_FIND_FOUND:
			text = NULL;
			break;
		case EPHY_FIND_NOTFOUND:
			{
				text = _("Not found");

				pango_desc = pango_font_description_new ();
				pango_font_description_set_weight (pango_desc, PANGO_WEIGHT_BOLD);
				gtk_widget_modify_font (priv->status_label, pango_desc);
				pango_font_description_free (pango_desc);

				gtk_widget_error_bell (GTK_WIDGET (priv->window));
				priv->source_id = g_timeout_add (500, (GSourceFunc) set_status_notfound_cb, toolbar);
			}
			break;
		case EPHY_FIND_FOUNDWRAPPED:
			text = _("Wrapped");
			break;
	}

	gtk_label_set_text (GTK_LABEL (priv->status_label),
			    text != NULL ? text : "");

	g_object_set (priv->sep, "visible", text != NULL, NULL);
	g_object_set (priv->status_item, "visible", text != NULL, NULL);
}

static void
clear_status (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_hide (GTK_WIDGET (priv->sep));
	gtk_widget_hide (GTK_WIDGET (priv->status_item));
	gtk_label_set_text (GTK_LABEL (priv->status_label), "");
	gtk_label_set_text (GTK_LABEL (priv->label),
			    priv->links_only ? _("Find links:") : _("Find:"));
}

#if 0
/* Code adapted from gtktreeview.c:gtk_tree_view_key_press() and
 * gtk_tree_view_real_start_interactive_seach()
 */
static gboolean
tab_search_key_press_cb (EphyEmbed *embed,
			 GdkEventKey *event,
			 EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	GtkWidget *widget = (GtkWidget *) toolbar;

	g_return_val_if_fail (event != NULL, FALSE);

	/* don't do anything in PPV mode */
	if (ephy_window_get_is_print_preview (priv->window)) return FALSE;

	/* check for / and ' which open the find toolbar in text resp. link mode */
	if (GTK_WIDGET_VISIBLE (widget) == FALSE)
	{
		if (event->keyval == GDK_slash)
		{
			ephy_find_toolbar_open (toolbar, FALSE, TRUE);
			return TRUE;
		}
		else if (event->keyval == GDK_apostrophe)
		{
			ephy_find_toolbar_open (toolbar, TRUE, TRUE);
			return TRUE;
		}
	}

	return FALSE;
}
#endif

static void
find_next_cb (EphyFindToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[NEXT], 0);
}

static void
find_prev_cb (EphyFindToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[PREVIOUS], 0);
}

static void
set_string_and_highlight (EphyFindToolbarPrivate *priv, const char *find_string)
{
        WebKitWebView *web_view = priv->web_view;
        gboolean case_sensitive;

        if (g_strcmp0 (priv->find_string, find_string) != 0) {
                g_free (priv->find_string);
                priv->find_string = g_strdup (find_string);
        }

        case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive));

        webkit_web_view_unmark_text_matches (web_view);
        webkit_web_view_mark_text_matches (web_view,
                                           priv->find_string,
                                           case_sensitive,
                                           0);
        webkit_web_view_set_highlight_text_matches (web_view, TRUE);
}

static void
ephy_find_toolbar_set_properties (EphyFindToolbar *toolbar,
                                  const char *find_string)
{
        EphyFindToolbarPrivate *priv = toolbar->priv;

        set_string_and_highlight (priv, find_string);
}

static EphyEmbedFindResult
real_find (EphyFindToolbarPrivate *priv,
	   gboolean forward)
{
        WebKitWebView *web_view = priv->web_view;
        gboolean case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive));

        if (!webkit_web_view_search_text
            (web_view, priv->find_string, case_sensitive, TRUE, FALSE)) {
                /* not found, try to wrap */
                if (!webkit_web_view_search_text
                    (web_view, priv->find_string, case_sensitive, TRUE, TRUE)) {
                        /* there's no result */
                        return EPHY_FIND_NOTFOUND;
                } else {
                        /* found wrapped */
                        return EPHY_FIND_FOUNDWRAPPED;
                }
        }

        return EPHY_FIND_FOUND;
}

static EphyEmbedFindResult
ephy_find_toolbar_find (EphyFindToolbar *toolbar,
                        const char *find_string,
                        gboolean links_only)
{
        EphyFindToolbarPrivate *priv = toolbar->priv;

        set_string_and_highlight (priv, find_string);

        return real_find (priv, TRUE);
}

static void
entry_changed_cb (GtkEntry *entry,
		  EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	const char *text;
	EphyEmbedFindResult result;
	gboolean case_sensitive;

	text = gtk_entry_get_text (GTK_ENTRY (priv->entry));
	case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive));

	ephy_find_toolbar_set_properties (toolbar, text);
	result = ephy_find_toolbar_find (toolbar, text, priv->links_only);

	set_status (toolbar, result);
}

static gboolean
ephy_find_toolbar_activate_link (EphyFindToolbar *toolbar,
                                 GdkModifierType mask)
{
        return FALSE;
}

static gboolean
entry_key_press_event_cb (GtkEntry *entry,
			  GdkEventKey *event,
			  EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	guint mask = gtk_accelerator_get_default_mod_mask ();
	gboolean handled = FALSE;

	if ((event->state & mask) == 0)
	{
		handled = TRUE;
		switch (event->keyval)
		{
		case GDK_Up:
		case GDK_KP_Up:
			scroll_lines (priv->web_view, -1);
			break;
		case GDK_Down:
		case GDK_KP_Down:
			scroll_lines (priv->web_view, 1);
			break;
		case GDK_Page_Up:
		case GDK_KP_Page_Up:
			scroll_pages (priv->web_view, -1);
			break;
		case GDK_Page_Down:
		case GDK_KP_Page_Down:
			scroll_pages (priv->web_view, 1);
			break;
		case GDK_Escape:
			/* Hide the toolbar when ESC is pressed */
			ephy_find_toolbar_request_close (toolbar);
			break;
		default:
			handled = FALSE;
			break;
		}
	}
	else if ((event->state & mask) == GDK_CONTROL_MASK &&
		 (event->keyval == GDK_Return ||
		  event->keyval == GDK_KP_Enter ||
		  event->keyval == GDK_ISO_Enter))
	{
		handled = ephy_find_toolbar_activate_link (toolbar, event->state);
	}
	else if ((event->state & mask) == GDK_SHIFT_MASK &&
		 (event->keyval == GDK_Return ||
		  event->keyval == GDK_KP_Enter ||
		  event->keyval == GDK_ISO_Enter))
	{
		handled = TRUE;
		g_signal_emit (toolbar, signals[PREVIOUS], 0);
	}

	return handled;
}

static void
entry_activate_cb (GtkWidget *entry,
		   EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	if (priv->typing_ahead)
	{
		ephy_find_toolbar_activate_link (toolbar, 0);
	}
	else
	{
		g_signal_emit (toolbar, signals[NEXT], 0);
	}
}

static void
case_sensitive_menu_toggled_cb (GtkWidget *check,
				EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	gboolean case_sensitive;

	case_sensitive = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (check));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->case_sensitive),
				      case_sensitive);
}

static void
case_sensitive_toggled_cb (GtkWidget *check,
			   EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	const char *text;
	gboolean case_sensitive;
	GtkWidget *proxy;

	text = gtk_entry_get_text (GTK_ENTRY (priv->entry));
	case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));

	proxy = gtk_tool_item_get_proxy_menu_item (GTK_TOOL_ITEM (gtk_widget_get_parent (check)),
						   "menu-proxy");

	if (proxy != NULL)
	{
		g_signal_handlers_block_by_func
			(proxy, G_CALLBACK (case_sensitive_menu_toggled_cb), toolbar);

		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (proxy),
						case_sensitive);
		g_signal_handlers_unblock_by_func
			(proxy, G_CALLBACK (case_sensitive_menu_toggled_cb), toolbar);
	}

	ephy_find_toolbar_set_properties (toolbar, text);
}

static gboolean
toolitem_create_menu_proxy_cb (GtkToolItem *toolitem,
			       EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	GtkWidget *checkbox_menu;
	gboolean case_sensitive;

	/* Create a menu item, and sync it */
	checkbox_menu = gtk_check_menu_item_new_with_mnemonic (_("_Case sensitive"));
	case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (checkbox_menu),
					case_sensitive);

	g_signal_connect (checkbox_menu, "toggled",
			  G_CALLBACK (case_sensitive_menu_toggled_cb), toolbar);

	gtk_tool_item_set_proxy_menu_item (toolitem, "menu-proxy",
					   checkbox_menu);

	return TRUE;
}

static void
set_focus_cb (EphyWindow *window,
	      GtkWidget *widget,
	      EphyFindToolbar *toolbar)
{
	GtkWidget *wtoolbar = GTK_WIDGET (toolbar);

	while (widget != NULL && widget != wtoolbar)
	{
		widget = widget->parent;
	}

	/* if widget == toolbar, the new focus widget is in the toolbar */
	if (widget != wtoolbar)
	{
		ephy_find_toolbar_request_close (toolbar);
	}
}

static void
ephy_find_toolbar_set_window (EphyFindToolbar *toolbar,
			      EphyWindow *window)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	priv->window = window;
}

static void
ephy_find_toolbar_parent_set (GtkWidget *widget,
			      GtkWidget *previous_parent)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (widget);
	EphyFindToolbarPrivate *priv = toolbar->priv;
	GtkWidget *toplevel;

	if (widget->parent != NULL && priv->set_focus_handler == 0)
	{
		toplevel = gtk_widget_get_toplevel (widget);
		priv->set_focus_handler =
			g_signal_connect (toplevel, "set-focus",
					  G_CALLBACK (set_focus_cb), toolbar);
	}
}

static void
ephy_find_toolbar_grab_focus (GtkWidget *widget)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (widget);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_grab_focus (GTK_WIDGET (priv->entry));
}

static void
ephy_find_toolbar_init (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv;
	GtkToolbar *gtoolbar;
	GtkToolItem *item;
	GtkWidget *alignment, *arrow, *box;
	GtkWidget *checkbox;

	priv = toolbar->priv = EPHY_FIND_TOOLBAR_GET_PRIVATE (toolbar);
	gtoolbar = GTK_TOOLBAR (toolbar);

	gtk_toolbar_set_style (gtoolbar, GTK_TOOLBAR_BOTH_HORIZ);

	/* Find: |_____| */
	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 2, 2);

	box = gtk_hbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (alignment), box);

	priv->label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (box), priv->label, FALSE, FALSE, 0);

	priv->entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (priv->entry), 32);
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry), 512);
	gtk_box_pack_start (GTK_BOX (box), priv->entry, TRUE, TRUE, 0);

	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), alignment);
	/* gtk_tool_item_set_expand (item, TRUE); */
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
	gtk_widget_show_all (GTK_WIDGET (item));

	/* Prev */
	arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	priv->prev = gtk_tool_button_new (arrow, _("Find Previous"));
	gtk_tool_item_set_is_important (priv->prev, TRUE);
	gtk_tool_item_set_tooltip_text (priv->prev,
					_("Find previous occurrence of the search string"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->prev, -1);
	gtk_widget_show_all (GTK_WIDGET (priv->prev));

	/* Next */
	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	priv->next = gtk_tool_button_new (arrow, _("Find Next"));
	gtk_tool_item_set_is_important (priv->next, TRUE);
	gtk_tool_item_set_tooltip_text (priv->next,
					_("Find next occurrence of the search string"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->next, -1);
	gtk_widget_show_all (GTK_WIDGET (priv->next));

	/* Case sensitivity */
	checkbox = gtk_check_button_new_with_mnemonic (_("_Case sensitive"));
	priv->case_sensitive = checkbox;
	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), checkbox);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
	gtk_widget_show_all (GTK_WIDGET (item));

	/* Populate the overflow menu */
	g_signal_connect (item, "create-menu-proxy",
			  G_CALLBACK (toolitem_create_menu_proxy_cb), toolbar);

	priv->sep = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->sep, -1);

	priv->status_item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (priv->status_item, TRUE);
	priv->status_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->status_label), 0.0, 0.5);
	gtk_label_set_ellipsize (GTK_LABEL (priv->status_label), PANGO_ELLIPSIZE_END);
	gtk_container_add (GTK_CONTAINER (priv->status_item), priv->status_label);
	gtk_widget_show (priv->status_label);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->status_item, -1);

	/* connect signals */
	g_signal_connect (priv->entry, "key-press-event",
			  G_CALLBACK (entry_key_press_event_cb), toolbar);
	g_signal_connect_after (priv->entry, "changed",
				G_CALLBACK (entry_changed_cb), toolbar);
	g_signal_connect (priv->entry, "activate",
			  G_CALLBACK (entry_activate_cb), toolbar);
	g_signal_connect_swapped (priv->next, "clicked",
				  G_CALLBACK (find_next_cb), toolbar);
	g_signal_connect_swapped (priv->prev, "clicked",
				  G_CALLBACK (find_prev_cb), toolbar);
	g_signal_connect (priv->case_sensitive, "toggled",
			  G_CALLBACK (case_sensitive_toggled_cb), toolbar);
}

G_DEFINE_TYPE (EphyFindToolbar, ephy_find_toolbar, GTK_TYPE_TOOLBAR)

static void
ephy_find_toolbar_dispose (GObject *object)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	if (priv->source_id != 0)
	{
		g_source_remove (priv->source_id);
		priv->source_id = 0;
	}

	if (priv->find_again_source_id != 0)
	{
		g_source_remove (priv->find_again_source_id);
		priv->find_again_source_id = 0;
	}

	G_OBJECT_CLASS (ephy_find_toolbar_parent_class)->dispose (object);
}

static void
ephy_find_toolbar_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	/* no readable properties */
	g_assert_not_reached ();
}

static void
ephy_find_toolbar_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			ephy_find_toolbar_set_window (toolbar, (EphyWindow *) g_value_get_object (value));
			break;
	}
}

static void
ephy_find_toolbar_finalize (GObject *o)
{
  EphyFindToolbarPrivate *priv = EPHY_FIND_TOOLBAR (o)->priv;

  g_free (priv->find_string);
  G_OBJECT_CLASS (ephy_find_toolbar_parent_class)->finalize (o);
}

static void
ephy_find_toolbar_class_init (EphyFindToolbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ephy_find_toolbar_dispose;
	object_class->finalize = ephy_find_toolbar_finalize;
	object_class->get_property = ephy_find_toolbar_get_property;
	object_class->set_property = ephy_find_toolbar_set_property;

	widget_class->parent_set = ephy_find_toolbar_parent_set;
	widget_class->grab_focus = ephy_find_toolbar_grab_focus;

	klass->next = ephy_find_toolbar_find_next;
	klass->previous = ephy_find_toolbar_find_previous;

	signals[NEXT] =
		g_signal_new ("next",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyFindToolbarClass, next),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[PREVIOUS] =
		g_signal_new ("previous",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyFindToolbarClass, previous),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[CLOSE] =
		g_signal_new ("close",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EphyFindToolbarClass, close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_object_class_install_property
		(object_class,
		 PROP_WINDOW,
		 g_param_spec_object ("window",
				      "Window",
				      "Parent window",
				      EPHY_TYPE_WINDOW,
				      (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY)));

	g_type_class_add_private (klass, sizeof (EphyFindToolbarPrivate));
}

/* public functions */

EphyFindToolbar *
ephy_find_toolbar_new (EphyWindow *window)
{
	return EPHY_FIND_TOOLBAR (g_object_new (EPHY_TYPE_FIND_TOOLBAR,
						"window", window,
						NULL));
}

const char *
ephy_find_toolbar_get_text (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	return gtk_entry_get_text (GTK_ENTRY (priv->entry));
}

void
ephy_find_toolbar_set_embed (EphyFindToolbar *toolbar,
			     EphyEmbed *embed)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	WebKitWebView *web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED(embed);

	if (priv->web_view == web_view) return;

	if (priv->web_view != NULL)
	{
                /*
		g_signal_handlers_disconnect_matched (embed, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, toolbar);
                */
	}

	priv->web_view = web_view;
	if (web_view != NULL)
	{
		clear_status (toolbar);

		/* FIXME: this is never emitted
		g_signal_connect_object (embed, "ge-search-key-press",
					 G_CALLBACK (tab_search_key_press_cb),
					 toolbar, 0);
                */
	}
}

typedef struct
{
	EphyFindToolbar *toolbar;
	gboolean next;
} FindAgainCBStruct;

static void
find_again_data_destroy_cb (FindAgainCBStruct *data)
{
	g_slice_free (FindAgainCBStruct, data);
}

static EphyEmbedFindResult
ephy_find_toolbar_find_again (EphyFindToolbar *toolbar,
                              gboolean forward,
                              gboolean links_only)
{
  EphyFindToolbarPrivate *priv = toolbar->priv;

  return real_find (priv, forward);
}

static gboolean
find_again_cb (FindAgainCBStruct *data)
{
	EphyEmbedFindResult result;
	EphyFindToolbarPrivate *priv = data->toolbar->priv;

	result = ephy_find_toolbar_find_again (data->toolbar, data->next,
				  priv->links_only);
	set_status (data->toolbar, result);

	priv->find_again_source_id = 0;

	return FALSE;
}

void
ephy_find_toolbar_find_next (EphyFindToolbar *toolbar)
{
	GtkWidget *widget = GTK_WIDGET (toolbar);
	EphyFindToolbarPrivate *priv = toolbar->priv;
	FindAgainCBStruct *data;

	if (!GTK_WIDGET_VISIBLE (widget)) {
		gtk_widget_show (widget);
		gtk_widget_grab_focus (widget);
	}

	/* We need to do this here (and in find_previous) to give time to the embed
	 * to sync with the size change due to the toolbar being shown, otherwise
	 * the toolbar can obscure the result. See GNOME bug #415074.
	 */

	if (priv->find_again_source_id != 0) return;

	data = g_slice_new0 (FindAgainCBStruct);
	data->toolbar = toolbar;
	data->next = TRUE;

	priv->find_again_source_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
						      (GSourceFunc) find_again_cb,
						      data,
						      (GDestroyNotify) find_again_data_destroy_cb);
}

void
ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar)
{
	GtkWidget *widget = GTK_WIDGET (toolbar);
	EphyFindToolbarPrivate *priv = toolbar->priv;
	FindAgainCBStruct *data;

	if (!GTK_WIDGET_VISIBLE (widget)) {
		gtk_widget_show (widget);
		gtk_widget_grab_focus (widget);
	}

	if (priv->find_again_source_id != 0) return;

	data = g_slice_new0 (FindAgainCBStruct);
	data->toolbar = toolbar;
	data->next = FALSE;

	priv->find_again_source_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
						      (GSourceFunc) find_again_cb,
						      data,
						      (GDestroyNotify) find_again_data_destroy_cb);
}

void
ephy_find_toolbar_open (EphyFindToolbar *toolbar,
			gboolean links_only,
			gboolean typing_ahead)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	g_return_if_fail (priv->web_view != NULL);

	priv->typing_ahead = typing_ahead;
	priv->links_only = links_only;

	clear_status (toolbar);

	gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

	gtk_widget_show (GTK_WIDGET (toolbar));
	gtk_widget_grab_focus (GTK_WIDGET (toolbar));
}

static void
ephy_find_toolbar_set_selection (EphyFindToolbar *toolbar,
                                 gboolean attention)
{
        WebKitWebView *web_view = toolbar->priv->web_view;

        webkit_web_view_set_highlight_text_matches (web_view, attention);
}

void
ephy_find_toolbar_close (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_hide (GTK_WIDGET (toolbar));

	if (priv->web_view == NULL) return;
	ephy_find_toolbar_set_selection (toolbar, FALSE);
}

void
ephy_find_toolbar_request_close (EphyFindToolbar *toolbar)
{
	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (toolbar)))
	{
		g_signal_emit (toolbar, signals[CLOSE], 0);
	}
}
