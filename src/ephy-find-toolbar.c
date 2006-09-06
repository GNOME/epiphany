/*
 *  Copyright (C) 2004 Tommi Komulainen
 *  Copyright (C) 2004, 2005 Christian Persch
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-find-toolbar.h"
#include "ephy-embed-find.h"
#include "ephy-embed-factory.h"
#include "ephy-debug.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkseparatortoolitem.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktoolbutton.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkmain.h>
#include <string.h>

#define EPHY_FIND_TOOLBAR_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarPrivate))

struct _EphyFindToolbarPrivate
{
	EphyEmbedFind *find;
	EphyWindow *window;
	EphyEmbed *embed;
	GtkWidget *entry;
	GtkWidget *label;
	GtkToolItem *next;
	GtkToolItem *prev;
	GtkToolItem *sep;
	GtkToolItem *status_item;
	GtkWidget *status_label;
	gulong set_focus_handler;
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

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

/* private functions */

static EphyEmbedFind *
get_find (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	if (priv->find == NULL)
	{
		LOG ("Creating the finder now");

		priv->find = EPHY_EMBED_FIND (ephy_embed_factory_new_object (EPHY_TYPE_EMBED_FIND));

		g_return_val_if_fail (priv->embed == NULL || GTK_WIDGET_REALIZED (GTK_WIDGET (priv->embed)), priv->find);

		ephy_embed_find_set_embed (priv->find, priv->embed);
	}

	return priv->find;
}

static void
set_status (EphyFindToolbar *toolbar,
	    EphyEmbedFindResult result)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	char *text = NULL;

	switch (result)
	{
		case EPHY_EMBED_FIND_FOUND:
			text = NULL;
			break;
		case EPHY_EMBED_FIND_NOTFOUND:
			text = _("Not found");
			break;
		case EPHY_EMBED_FIND_FOUNDWRAPPED:
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
entry_changed_cb (GtkEntry *entry,
		  EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	const char *text;
	char *lowercase;
	EphyEmbedFindResult result;
	gboolean case_sensitive;

	text = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	/* Search case-sensitively iff the string includes 
	 * non-lowercase character.
	 */
	lowercase = g_utf8_strdown (text, -1);
	case_sensitive = g_utf8_collate (text, lowercase) != 0;
	g_free (lowercase);

	ephy_embed_find_set_properties (get_find (toolbar), text, case_sensitive);
	result = ephy_embed_find_find (get_find (toolbar), text, priv->links_only);

	set_status (toolbar, result);
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
			ephy_embed_scroll (priv->embed, -1);
			break;
		case GDK_Down:
                case GDK_KP_Down:
			ephy_embed_scroll (priv->embed, 1);
			break;
		case GDK_Page_Up:
                case GDK_KP_Page_Up:
			ephy_embed_page_scroll (priv->embed, -1);
			break;
		case GDK_Page_Down:
                case GDK_KP_Page_Down:
			ephy_embed_page_scroll (priv->embed, 1);
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
		handled = ephy_embed_find_activate_link (get_find (toolbar), event->state);
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
		ephy_embed_find_activate_link (get_find (toolbar), 0);
	}
	else
	{
		g_signal_emit (toolbar, signals[NEXT], 0);
	}
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
	//gtk_tool_item_set_expand (item, TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
	gtk_widget_show_all (GTK_WIDGET (item));

	/* Prev */
	arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	priv->prev = gtk_tool_button_new (arrow, _("Find Previous"));
	gtk_tool_item_set_is_important (priv->prev, TRUE);
	gtk_tool_item_set_tooltip (priv->prev, gtoolbar->tooltips,
				   _("Find previous occurrence of the search string"),
				   NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->prev, -1);
	gtk_widget_show_all (GTK_WIDGET (priv->prev));

	/* Next */
	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	priv->next = gtk_tool_button_new (arrow, _("Find Next"));
	gtk_tool_item_set_is_important (priv->next, TRUE);
	gtk_tool_item_set_tooltip (priv->next, gtoolbar->tooltips,
				   _("Find next occurrence of the search string"),
				   NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->next, -1);
	gtk_widget_show_all (GTK_WIDGET (priv->next));

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
}

static void
ephy_find_toolbar_dispose (GObject *object)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	if (priv->find != NULL)
	{
		g_object_unref (priv->find);
		priv->find = NULL;
	}

	parent_class->dispose (object);
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
ephy_find_toolbar_class_init (EphyFindToolbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ephy_find_toolbar_dispose;
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

GType
ephy_find_toolbar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyFindToolbarClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_find_toolbar_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyFindToolbar),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_find_toolbar_init
		};
	
		type = g_type_register_static (GTK_TYPE_TOOLBAR,
					       "EphyFindToolbar",
					       &our_info, 0);
	}

	return type;
}

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

	if (priv->embed == embed) return;

	if (priv->embed != NULL)
	{
		g_signal_handlers_disconnect_matched (embed, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, toolbar);
	}

	priv->embed = embed;
	if (embed != NULL)
	{
		clear_status (toolbar);

		g_signal_connect_object (embed, "ge-search-key-press",
					 G_CALLBACK (tab_search_key_press_cb),
					 toolbar, 0);

		if (priv->find != NULL)
		{
			g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (priv->embed)));

			ephy_embed_find_set_embed (priv->find, embed);
		}
	}
}

void
ephy_find_toolbar_find_next (EphyFindToolbar *toolbar)
{
	EphyEmbedFindResult result;

	result = ephy_embed_find_find_again (get_find (toolbar), TRUE);
	set_status (toolbar, result);

	if (!GTK_WIDGET_VISIBLE(toolbar)) {
		gtk_widget_show (GTK_WIDGET (toolbar));
		gtk_widget_grab_focus (GTK_WIDGET (toolbar));
	}
}

void
ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar)
{
	EphyEmbedFindResult result;

	result = ephy_embed_find_find_again (get_find (toolbar), FALSE);
	set_status (toolbar, result);

	if (!GTK_WIDGET_VISIBLE(toolbar)) {
		gtk_widget_show (GTK_WIDGET (toolbar));
		gtk_widget_grab_focus (GTK_WIDGET (toolbar));
	}
}

void
ephy_find_toolbar_open (EphyFindToolbar *toolbar,
			gboolean links_only,
			gboolean typing_ahead)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;
	gboolean clear_search = typing_ahead;

	g_return_if_fail (priv->embed != NULL);

	priv->typing_ahead = typing_ahead;
	priv->links_only = links_only;

	clear_status (toolbar);

	if (clear_search)
	{
		gtk_entry_set_text (GTK_ENTRY (priv->entry), "");
	}
	else
	{
		gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);
	}

	gtk_widget_show (GTK_WIDGET (toolbar));
	gtk_widget_grab_focus (GTK_WIDGET (toolbar));
}

void
ephy_find_toolbar_close (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_hide (GTK_WIDGET (toolbar));

	if (priv->embed == NULL || priv->find == NULL) return;
	ephy_embed_find_set_selection (get_find (toolbar), FALSE);
}

void
ephy_find_toolbar_request_close (EphyFindToolbar *toolbar)
{
	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (toolbar)))
	{
		g_signal_emit (toolbar, signals[CLOSE], 0);
	}
}
