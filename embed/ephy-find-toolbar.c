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
#include "ephy-web-view.h"

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define EPHY_FIND_TOOLBAR_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarPrivate))

struct _EphyFindToolbarPrivate
{
	WebKitWebView *web_view;
        WebKitFindController *controller;
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
	PROP_WEB_VIEW
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

G_DEFINE_TYPE (EphyFindToolbar, ephy_find_toolbar, GTK_TYPE_SEARCH_BAR)

/* private functions */

static void ephy_find_toolbar_set_web_view (EphyFindToolbar *toolbar, WebKitWebView *web_view);

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
			gtk_widget_error_bell (GTK_WIDGET (toolbar));

			break;
		case EPHY_FIND_RESULT_FOUNDWRAPPED:
			icon_name = "view-wrapped-symbolic";
			tooltip = _("Search wrapped back to the top");
			break;
	}

	gtk_widget_set_sensitive (priv->prev, result != EPHY_FIND_RESULT_NOTFOUND);
	gtk_widget_set_sensitive (priv->next, result != EPHY_FIND_RESULT_NOTFOUND);

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

	gtk_widget_set_sensitive (priv->prev, FALSE);
	gtk_widget_set_sensitive (priv->next, FALSE);

        if (priv->web_view == NULL) return;

        webkit_find_controller_search_finish (priv->controller);
}

/* Code adapted from gtktreeview.c:gtk_tree_view_key_press() and
 * gtk_tree_view_real_start_interactive_seach()
 */
static gboolean
tab_search_key_press_cb (WebKitWebView *web_view,
			 GdkEventKey *event,
			 EphyFindToolbar *toolbar)
{
	GtkWidget *widget = (GtkWidget *) toolbar;

	g_return_val_if_fail (event != NULL, FALSE);

	/* check for / and ' which open the find toolbar in text resp. link mode */
	if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (widget)) == FALSE)
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

static gboolean
do_search (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	priv->find_source_id = 0;

        real_find (priv, EPHY_FIND_DIRECTION_NEXT);

	return FALSE;
}

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
	g_source_set_name_by_id (priv->find_source_id, "[epiphany] do_search");
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
	guint mask = gtk_accelerator_get_default_mod_mask ();
	gboolean handled = FALSE;

	if ((event->state & mask) == 0)
	{
		handled = TRUE;
		switch (event->keyval)
		{
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
ephy_find_toolbar_grab_focus (GtkWidget *widget)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (widget);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_grab_focus (priv->entry);
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
ephy_find_toolbar_load_changed_cb (WebKitWebView   *web_view,
				   WebKitLoadEvent  load_event,
				   EphyFindToolbar *toolbar)
{
	if (load_event == WEBKIT_LOAD_STARTED &&
	    gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (toolbar)))
	{
		ephy_find_toolbar_close (toolbar);
	}
}

static void
ephy_find_toolbar_init (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv;
	GtkWidget *box;
	GtkSizeGroup *size_group;

	priv = toolbar->priv = EPHY_FIND_TOOLBAR_GET_PRIVATE (toolbar);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_RAISED);
	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_LINKED);
	gtk_container_add (GTK_CONTAINER (toolbar), box);

	priv->entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (priv->entry), 32);
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry), 512);
	gtk_entry_set_placeholder_text (GTK_ENTRY (priv->entry), _("Type to search…"));
	gtk_container_add (GTK_CONTAINER (box), priv->entry);

	/* Prev */
	priv->prev = gtk_button_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (priv->prev,
				     _("Find previous occurrence of the search string"));
	gtk_container_add (GTK_CONTAINER (box), priv->prev);
	gtk_widget_show_all (priv->prev);
	gtk_widget_set_sensitive (priv->prev, FALSE);

	/* Next */
	priv->next = gtk_button_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (priv->next,
				     _("Find next occurrence of the search string"));
	gtk_container_add (GTK_CONTAINER (box), priv->next);
	gtk_widget_set_sensitive (priv->next, FALSE);

	gtk_size_group_add_widget (size_group, priv->entry);
	gtk_size_group_add_widget (size_group, priv->next);
	gtk_size_group_add_widget (size_group, priv->prev);
	g_object_unref (size_group);

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
	gtk_search_bar_connect_entry (GTK_SEARCH_BAR (toolbar),
				      GTK_ENTRY (priv->entry));

	search_entry_changed_cb (GTK_ENTRY (priv->entry), toolbar);

	gtk_widget_show_all (GTK_WIDGET (toolbar));
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

#ifndef G_DISABLE_ASSERT
G_GNUC_NORETURN
#endif
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
		case PROP_WEB_VIEW:
			ephy_find_toolbar_set_web_view (toolbar, (WebKitWebView *) g_value_get_object (value));
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
		 PROP_WEB_VIEW,
		 g_param_spec_object ("web-view",
				      "WebView",
				      "Parent web view",
				      WEBKIT_TYPE_WEB_VIEW,
				      (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY)));

	g_type_class_add_private (klass, sizeof (EphyFindToolbarPrivate));
}

/* public functions */

EphyFindToolbar *
ephy_find_toolbar_new (WebKitWebView *web_view)
{
	return g_object_new (EPHY_TYPE_FIND_TOOLBAR,
			     "web-view", web_view,
			     NULL);
}

const char *
ephy_find_toolbar_get_text (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	return gtk_entry_get_text (GTK_ENTRY (priv->entry));
}

static void
ephy_find_toolbar_set_web_view (EphyFindToolbar *toolbar,
				WebKitWebView *web_view)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	if (priv->web_view == web_view) return;

	if (priv->web_view != NULL)
	{
                g_signal_handlers_disconnect_matched (priv->controller,
                                                      G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL, NULL, toolbar);
	}

	priv->web_view = web_view;
	if (web_view != NULL)
	{
                priv->controller = webkit_web_view_get_find_controller (web_view);
                g_signal_connect_object (priv->controller, "found-text",
                                         G_CALLBACK (found_text_cb),
                                         toolbar, 0);
                g_signal_connect_object (priv->controller, "failed-to-find-text",
                                         G_CALLBACK (failed_to_find_text_cb),
                                         toolbar, 0);
                g_signal_connect (web_view, "load-changed",
				  G_CALLBACK (ephy_find_toolbar_load_changed_cb),
				  toolbar);

		clear_status (toolbar);

		g_signal_connect_object (EPHY_WEB_VIEW (web_view), "search-key-press",
					 G_CALLBACK (tab_search_key_press_cb),
					 toolbar, 0);
	}
}

void
ephy_find_toolbar_find_next (EphyFindToolbar *toolbar)
{
        webkit_find_controller_search_next (toolbar->priv->controller);
}

void
ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar)
{
        webkit_find_controller_search_previous (toolbar->priv->controller);
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

	gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (toolbar), TRUE);
	gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (toolbar), TRUE);
	gtk_widget_grab_focus (priv->entry);
}

void
ephy_find_toolbar_close (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (toolbar), FALSE);

	if (priv->web_view == NULL) return;

        webkit_find_controller_search_finish (priv->controller);
}

void
ephy_find_toolbar_request_close (EphyFindToolbar *toolbar)
{
	if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (toolbar)))
	{
		g_signal_emit (toolbar, signals[CLOSE], 0);
	}
}

void
ephy_find_toolbar_toggle_state (EphyFindToolbar *toolbar)
{
	if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (toolbar)))
	{
		ephy_find_toolbar_close (toolbar);
	}
	else
	{
		ephy_find_toolbar_open (toolbar, FALSE, FALSE);
	}
}
