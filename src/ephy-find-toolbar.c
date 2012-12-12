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
#include "ephy-find-toolbar.h"

#include "ephy-debug.h"
#include "ephy-embed-utils.h"

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#define EPHY_FIND_TOOLBAR_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarPrivate))

struct _EphyFindToolbarPrivate
{
	EphyWindow *window;
	WebKitWebView *web_view;
#ifdef HAVE_WEBKIT2
        WebKitFindController *controller;
#endif
	GtkWidget *entry;
	GtkWidget *next;
	GtkWidget *prev;
	guint find_again_source_id;
	guint find_source_id;
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
	EPHY_FIND_RESULT_FOUND		= 0,
	EPHY_FIND_RESULT_NOTFOUND	= 1,
	EPHY_FIND_RESULT_FOUNDWRAPPED	= 2
} EphyFindResult;

typedef enum
{
	EPHY_FIND_DIRECTION_NEXT,
	EPHY_FIND_DIRECTION_PREV
} EphyFindDirection;

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyFindToolbar, ephy_find_toolbar, GTK_TYPE_BOX)

/* private functions */

static void
scroll_lines (WebKitWebView *web_view,
              int num_lines)
{
#ifdef HAVE_WEBKIT2
        /* TODO: Scroll API? */
#else
        GtkScrolledWindow *scrolled_window;
        GtkAdjustment *vadj;
        gdouble value;

        scrolled_window = GTK_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (web_view)));
        vadj = gtk_scrolled_window_get_vadjustment (scrolled_window);

        value = gtk_adjustment_get_value (vadj) + (num_lines * gtk_adjustment_get_step_increment (vadj));
        gtk_adjustment_set_value (vadj, value);
#endif
}

static void
scroll_pages (WebKitWebView *web_view,
              int num_pages)
{
#ifdef HAVE_WEBKIT2
        /* TODO: Scroll API */
#else
        GtkScrolledWindow *scrolled_window;
        GtkAdjustment *vadj;
        gdouble value;

        scrolled_window = GTK_SCROLLED_WINDOW (gtk_widget_get_parent (GTK_WIDGET (web_view)));
        vadj = gtk_scrolled_window_get_vadjustment (scrolled_window);

        value = gtk_adjustment_get_value (vadj) + (num_pages * gtk_adjustment_get_page_increment (vadj));
        gtk_adjustment_set_value (vadj, value);
#endif
}

static void
set_status (EphyFindToolbar *toolbar,
	    EphyFindResult result)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	const char *icon_name = "edit-find-symbolic";
	const char *tooltip = NULL;

	switch (result)
	{
		case EPHY_FIND_RESULT_FOUND:
			break;
		case EPHY_FIND_RESULT_NOTFOUND:
			icon_name = "face-uncertain-symbolic";
			tooltip = _("Text not found");
			gtk_widget_error_bell (GTK_WIDGET (priv->window));

			break;
		case EPHY_FIND_RESULT_FOUNDWRAPPED:
			icon_name = "view-wrapped-symbolic";
			tooltip = _("Search wrapped back to the top");
			break;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (priv->prev), result != EPHY_FIND_RESULT_NOTFOUND);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->next), result != EPHY_FIND_RESULT_NOTFOUND);

	g_object_set (priv->entry,
		      "primary-icon-name", icon_name,
		      "primary-icon-activatable", FALSE,
		      "primary-icon-sensitive", FALSE,
		      "primary-icon-tooltip-text", tooltip,
		      NULL);
}

static void
clear_status (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	g_object_set (priv->entry,
		      "primary-icon-name", "edit-find-symbolic",
		      NULL);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->prev), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->next), FALSE);

	webkit_web_view_unmark_text_matches (priv->web_view);
}

/* Code adapted from gtktreeview.c:gtk_tree_view_key_press() and
 * gtk_tree_view_real_start_interactive_seach()
 */
static gboolean
tab_search_key_press_cb (EphyEmbed *embed,
			 GdkEventKey *event,
			 EphyFindToolbar *toolbar)
{
	GtkWidget *widget = (GtkWidget *) toolbar;

	g_return_val_if_fail (event != NULL, FALSE);

	/* check for / and ' which open the find toolbar in text resp. link mode */
	if (gtk_widget_get_visible (widget) == FALSE)
	{
		if (event->keyval == GDK_KEY_slash)
		{
			ephy_find_toolbar_open (toolbar, FALSE, TRUE);
			return TRUE;
		}
		else if (event->keyval == GDK_KEY_apostrophe)
		{
			ephy_find_toolbar_open (toolbar, TRUE, TRUE);
			return TRUE;
		}
	}

	return FALSE;
}

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

static gboolean
str_has_uppercase (const char *str)
{
	while (str != NULL && *str != '\0') {
		gunichar c;

		c = g_utf8_get_char (str);

		if (g_unichar_isupper (c))
			return TRUE;

		str = g_utf8_next_char (str);
	}

	return FALSE;
}

#ifndef HAVE_WEBKIT2
static void
ephy_find_toolbar_mark_matches (EphyFindToolbar *toolbar)
{
        EphyFindToolbarPrivate *priv = toolbar->priv;
        WebKitWebView *web_view = priv->web_view;
        gboolean case_sensitive;

        case_sensitive = str_has_uppercase (priv->find_string);

        webkit_web_view_unmark_text_matches (web_view);
        if (priv->find_string != NULL && priv->find_string[0] != '\0')
                webkit_web_view_mark_text_matches (web_view,
                                                   priv->find_string,
                                                   case_sensitive,
                                                   0);
        webkit_web_view_set_highlight_text_matches (web_view, TRUE);
}
#endif

#ifdef HAVE_WEBKIT2
static void
real_find (EphyFindToolbarPrivate *priv,
           EphyFindDirection direction)
{
        WebKitFindOptions options = WEBKIT_FIND_OPTIONS_NONE;

        if (!g_strcmp0 (priv->find_string, ""))
                return;

        if (!str_has_uppercase (priv->find_string))
                options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;
        if (direction == EPHY_FIND_DIRECTION_PREV)
                options |= WEBKIT_FIND_OPTIONS_BACKWARDS;

        webkit_find_controller_search (priv->controller, priv->find_string, options, G_MAXUINT);
}

#else
static EphyFindResult
real_find (EphyFindToolbarPrivate *priv,
	   EphyFindDirection direction)
{
        WebKitWebView *web_view = priv->web_view;
        gboolean case_sensitive;
        gboolean forward = (direction == EPHY_FIND_DIRECTION_NEXT);

        case_sensitive = str_has_uppercase (priv->find_string);
        if (!priv->find_string || !g_strcmp0 (priv->find_string, ""))
                return EPHY_FIND_RESULT_NOTFOUND;

        if (!webkit_web_view_search_text
            (web_view, priv->find_string, case_sensitive, forward, FALSE)) {
                /* not found, try to wrap */
                if (!webkit_web_view_search_text
                    (web_view, priv->find_string, case_sensitive, forward, TRUE)) {
                        /* there's no result */
                        return EPHY_FIND_RESULT_NOTFOUND;
                } else {
                        /* found wrapped */
                        return EPHY_FIND_RESULT_FOUNDWRAPPED;
                }
        }

        return EPHY_FIND_RESULT_FOUND;
}
#endif

static gboolean
do_search (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
#ifndef HAVE_WEBKIT2
	EphyFindResult result;
#endif

	priv->find_source_id = 0;

#ifdef HAVE_WEBKIT2
        real_find (priv, EPHY_FIND_DIRECTION_NEXT);
#else
	ephy_find_toolbar_mark_matches (toolbar);

	result = real_find (priv, EPHY_FIND_DIRECTION_NEXT);
	set_status (toolbar, result);
#endif

	return FALSE;
}

#ifdef HAVE_WEBKIT2
static void
found_text_cb (WebKitFindController *controller,
               guint n_matches,
               EphyFindToolbar *toolbar)
{
        WebKitFindOptions options;
        EphyFindResult result;

        options = webkit_find_controller_get_options (controller);
        /* FIXME: it's not possible to remove the wrap flag, so the status is now always wrapped. */
        result = options & WEBKIT_FIND_OPTIONS_WRAP_AROUND ? EPHY_FIND_RESULT_FOUNDWRAPPED : EPHY_FIND_RESULT_FOUND;
        set_status (toolbar, result);
}

static void
failed_to_find_text_cb (WebKitFindController *controller,
                        EphyFindToolbar *toolbar)
{
        EphyFindToolbarPrivate *priv = toolbar->priv;
        WebKitFindOptions options;

        options = webkit_find_controller_get_options (controller);
        if (options & WEBKIT_FIND_OPTIONS_WRAP_AROUND) {
                set_status (toolbar, EPHY_FIND_RESULT_NOTFOUND);
                return;
        }

        options |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;
        webkit_find_controller_search (controller, priv->find_string, options, G_MAXUINT);
}
#endif


static void
update_find_string (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	g_free (priv->find_string);
	priv->find_string = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry)));

	if (priv->find_source_id != 0) {
		g_source_remove (priv->find_source_id);
		priv->find_source_id = 0;
	}

	if (strlen (priv->find_string) == 0) {
		clear_status (toolbar);
		return;
	}

	priv->find_source_id = g_timeout_add (300, (GSourceFunc)do_search, toolbar);
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
		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
			scroll_lines (priv->web_view, -1);
			break;
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
			scroll_lines (priv->web_view, 1);
			break;
		case GDK_KEY_Page_Up:
		case GDK_KEY_KP_Page_Up:
			scroll_pages (priv->web_view, -1);
			break;
		case GDK_KEY_Page_Down:
		case GDK_KEY_KP_Page_Down:
			scroll_pages (priv->web_view, 1);
			break;
		case GDK_KEY_Escape:
			/* Hide the toolbar when ESC is pressed */
			ephy_find_toolbar_request_close (toolbar);
			break;
		default:
			handled = FALSE;
			break;
		}
	}
	else if ((event->state & mask) == GDK_CONTROL_MASK &&
		 (event->keyval == GDK_KEY_Return ||
		  event->keyval == GDK_KEY_KP_Enter ||
		  event->keyval == GDK_KEY_ISO_Enter))
	{
		handled = ephy_find_toolbar_activate_link (toolbar, event->state);
	}
	else if ((event->state & mask) == GDK_SHIFT_MASK &&
		 (event->keyval == GDK_KEY_Return ||
		  event->keyval == GDK_KEY_KP_Enter ||
		  event->keyval == GDK_KEY_ISO_Enter))
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
ephy_find_toolbar_set_window (EphyFindToolbar *toolbar,
			      EphyWindow *window)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	priv->window = window;
}

static void
ephy_find_toolbar_grab_focus (GtkWidget *widget)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (widget);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_grab_focus (GTK_WIDGET (priv->entry));
}

static void
close_button_clicked_cb (GtkButton *button, EphyFindToolbar *toolbar)
{
        ephy_find_toolbar_request_close (toolbar);
}

static gboolean
ephy_find_toolbar_draw (GtkWidget *widget,
			cairo_t *cr)
{
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);

	gtk_style_context_save (context);
	gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));

	gtk_render_background (context, cr, 0, 0,
			       gtk_widget_get_allocated_width (widget),
			       gtk_widget_get_allocated_height (widget));

	gtk_render_frame (context, cr, 0, 0,
			  gtk_widget_get_allocated_width (widget),
			  gtk_widget_get_allocated_height (widget));

	gtk_style_context_restore (context);

	return GTK_WIDGET_CLASS (ephy_find_toolbar_parent_class)->draw (widget, cr);
}

static void
search_entry_clear_cb (GtkEntry *entry,
                       gpointer  user_data)
{
  gtk_entry_set_text (entry, "");
}

static void
search_entry_changed_cb (GtkEntry *entry,
                         EphyFindToolbar *toolbar)
{
        const char *str;
        const char *primary_icon_name = "edit-find-symbolic";
        const char *secondary_icon_name = NULL;
        gboolean primary_active = FALSE;
        gboolean secondary_active = FALSE;

        str = gtk_entry_get_text (entry);

        if (str == NULL || *str == '\0') {
                primary_icon_name = "edit-find-symbolic";
        } else {
                if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
                        secondary_icon_name = "edit-clear-rtl-symbolic";
                else
                        secondary_icon_name = "edit-clear-symbolic";
                secondary_active = TRUE;
        }

        g_object_set (entry,
                      "primary-icon-name", primary_icon_name,
                      "primary-icon-activatable", primary_active,
                      "primary-icon-sensitive", primary_active,
                      "secondary-icon-name", secondary_icon_name,
                      "secondary-icon-activatable", secondary_active,
                      "secondary-icon-sensitive", secondary_active,
                      NULL);

        update_find_string (toolbar);
}

static void
ephy_find_toolbar_init (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv;
	GtkWidget *inner_box;
	GtkWidget *box;
	GtkWidget *center_box;
	GtkWidget *left_box;
	GtkWidget *right_box;
	GtkWidget *close_button, *image;
	GtkSizeGroup *size_group;

	priv = toolbar->priv = EPHY_FIND_TOOLBAR_GET_PRIVATE (toolbar);

	gtk_container_set_border_width (GTK_CONTAINER (toolbar), 0);
	gtk_box_set_spacing (GTK_BOX (toolbar), 5);
	gtk_widget_set_hexpand (GTK_WIDGET (toolbar), TRUE);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (toolbar)),
				     GTK_STYLE_CLASS_TOOLBAR);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (toolbar)),
				     GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	inner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (inner_box), 3);
	gtk_widget_show (inner_box);
	gtk_widget_set_hexpand (GTK_WIDGET (toolbar), TRUE);
	gtk_container_add (GTK_CONTAINER (toolbar), inner_box);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add (GTK_CONTAINER (inner_box), left_box);
	gtk_widget_show (GTK_WIDGET (left_box));
	gtk_widget_set_halign (left_box, GTK_ALIGN_START);
	gtk_widget_set_hexpand (left_box, TRUE);
	gtk_size_group_add_widget (size_group, left_box);

	center_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add (GTK_CONTAINER (inner_box), center_box);
	gtk_widget_show (GTK_WIDGET (center_box));
	gtk_widget_set_halign (center_box, GTK_ALIGN_CENTER);

	right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add (GTK_CONTAINER (inner_box), right_box);
	gtk_widget_show (GTK_WIDGET (right_box));
	gtk_widget_set_halign (right_box, GTK_ALIGN_END);
	gtk_widget_set_hexpand (right_box, TRUE);
	gtk_size_group_add_widget (size_group, right_box);

	/* Find: |_____| */
	priv->entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (priv->entry), 32);
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry), 512);

	gtk_container_add (GTK_CONTAINER (center_box), priv->entry);
	gtk_widget_show (priv->entry);

	/* Prev & Next */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_RAISED);
	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_LINKED);
	gtk_container_add (GTK_CONTAINER (center_box), box);
	gtk_widget_show (box);

	/* Prev */
	priv->prev = gtk_button_new ();
	image = gtk_image_new ();
	g_object_set (image, "margin", 2, NULL);
	gtk_image_set_from_icon_name (GTK_IMAGE (image), "go-up-symbolic",
				      GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (priv->prev), image);
	gtk_widget_set_tooltip_text (priv->prev,
				     _("Find previous occurrence of the search string"));
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->prev));
	gtk_widget_show_all (GTK_WIDGET (priv->prev));
	gtk_widget_set_sensitive (GTK_WIDGET (priv->prev), FALSE);

	/* Next */
	priv->next = gtk_button_new ();
	image = gtk_image_new ();
	g_object_set (image, "margin", 2, NULL);
	gtk_image_set_from_icon_name (GTK_IMAGE (image), "go-down-symbolic",
				      GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (priv->next), image);
	gtk_widget_set_tooltip_text (priv->next,
                                     _("Find next occurrence of the search string"));
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->next));
	gtk_widget_show_all (GTK_WIDGET (priv->next));
	gtk_widget_set_sensitive (GTK_WIDGET (priv->next), FALSE);

	/* Close button */
	close_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
	gtk_box_pack_end (GTK_BOX (right_box), close_button, FALSE, FALSE, 0);
	gtk_widget_show_all (close_button);
	gtk_style_context_add_class (gtk_widget_get_style_context (close_button),
				     GTK_STYLE_CLASS_RAISED);
	gtk_style_context_add_class (gtk_widget_get_style_context (close_button),
				     "close");

	/* connect signals */
	g_signal_connect (priv->entry, "icon-release",
			  G_CALLBACK (search_entry_clear_cb), toolbar);
	g_signal_connect (priv->entry, "key-press-event",
			  G_CALLBACK (entry_key_press_event_cb), toolbar);
	g_signal_connect_after (priv->entry, "changed",
				G_CALLBACK (search_entry_changed_cb), toolbar);
	g_signal_connect (priv->entry, "activate",
			  G_CALLBACK (entry_activate_cb), toolbar);
	g_signal_connect_swapped (priv->next, "clicked",
				  G_CALLBACK (find_next_cb), toolbar);
	g_signal_connect_swapped (priv->prev, "clicked",
				  G_CALLBACK (find_prev_cb), toolbar);
	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (close_button_clicked_cb), toolbar);

	search_entry_changed_cb (GTK_ENTRY (priv->entry), toolbar);
}

static void
ephy_find_toolbar_dispose (GObject *object)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	if (priv->find_again_source_id != 0)
	{
		g_source_remove (priv->find_again_source_id);
		priv->find_again_source_id = 0;
	}

	if (priv->find_source_id != 0)
	{
		g_source_remove (priv->find_source_id);
		priv->find_source_id = 0;
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

	widget_class->draw = ephy_find_toolbar_draw;
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
	return g_object_new (EPHY_TYPE_FIND_TOOLBAR,
			     "window", window,
			     NULL);
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
#ifdef HAVE_WEBKIT2
                g_signal_handlers_disconnect_matched (priv->controller,
                                                      G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL, NULL, toolbar);
#endif

                g_signal_handlers_disconnect_matched (EPHY_WEB_VIEW (web_view),
                                                      G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL, NULL, toolbar);
	}

	priv->web_view = web_view;
	if (web_view != NULL)
	{
#ifdef HAVE_WEBKIT2
                priv->controller = webkit_web_view_get_find_controller (web_view);
                g_signal_connect_object (priv->controller, "found-text",
                                         G_CALLBACK (found_text_cb),
                                         toolbar, 0);
                g_signal_connect_object (priv->controller, "failed-to-find-text",
                                         G_CALLBACK (failed_to_find_text_cb),
                                         toolbar, 0);
#endif

		clear_status (toolbar);

		g_signal_connect_object (EPHY_WEB_VIEW (web_view), "search-key-press",
					 G_CALLBACK (tab_search_key_press_cb),
					 toolbar, 0);
	}
}

#ifndef HAVE_WEBKIT2
typedef struct
{
	EphyFindToolbar *toolbar;
	gboolean direction;
	gboolean highlight;
} FindAgainCBStruct;

static void
find_again_data_destroy_cb (FindAgainCBStruct *data)
{
	g_slice_free (FindAgainCBStruct, data);
}

static gboolean
find_again_cb (FindAgainCBStruct *data)
{
	EphyFindResult result;
	EphyFindToolbarPrivate *priv = data->toolbar->priv;

	result = real_find (priv, data->direction);

	/* Highlight matches again if the toolbar was hidden when the user
	 * requested find-again. */
	if (result != EPHY_FIND_RESULT_NOTFOUND && data->highlight)
		ephy_find_toolbar_mark_matches (data->toolbar);

	set_status (data->toolbar, result);

	priv->find_again_source_id = 0;

	return FALSE;
}

static void
find_again (EphyFindToolbar *toolbar, EphyFindDirection direction)
{
	GtkWidget *widget = GTK_WIDGET (toolbar);
	EphyFindToolbarPrivate *priv = toolbar->priv;
	FindAgainCBStruct *data;
	gboolean visible;

	visible = gtk_widget_get_visible (widget);
	if (!visible) {
		gtk_widget_show (widget);
		gtk_widget_grab_focus (widget);
	}

	/* We need to do this to give time to the embed to sync with the size
	 * change due to the toolbar being shown, otherwise the toolbar can
	 * obscure the result. See GNOME bug #415074.
	 */
	if (priv->find_again_source_id != 0) return;

	data = g_slice_new0 (FindAgainCBStruct);
	data->toolbar = toolbar;
	data->direction = direction;
	data->highlight = !visible;

	priv->find_again_source_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
						      (GSourceFunc) find_again_cb,
						      data,
						      (GDestroyNotify) find_again_data_destroy_cb);
}
#endif

void
ephy_find_toolbar_find_next (EphyFindToolbar *toolbar)
{
#ifdef HAVE_WEBKIT2
        webkit_find_controller_search_next (toolbar->priv->controller);
#else
	find_again (toolbar, EPHY_FIND_DIRECTION_NEXT);
#endif
}

void
ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar)
{
#ifdef HAVE_WEBKIT2
        webkit_find_controller_search_previous (toolbar->priv->controller);
#else
	find_again (toolbar, EPHY_FIND_DIRECTION_PREV);
#endif
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

void
ephy_find_toolbar_close (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_hide (GTK_WIDGET (toolbar));

	if (priv->web_view == NULL) return;
#ifdef HAVE_WEBKIT2
        webkit_find_controller_search_finish (priv->controller);
#else
	webkit_web_view_set_highlight_text_matches (priv->web_view, FALSE);
#endif
}

void
ephy_find_toolbar_request_close (EphyFindToolbar *toolbar)
{
	if (gtk_widget_get_visible (GTK_WIDGET (toolbar)))
	{
		g_signal_emit (toolbar, signals[CLOSE], 0);
	}
}
