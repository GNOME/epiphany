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
#include <gtk/gtkmain.h>
#include <string.h>

#define EPHY_FIND_TOOLBAR_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),EPHY_TYPE_FIND_TOOLBAR, EphyFindToolbarPrivate))

struct _EphyFindToolbarPrivate
{
	EphyWindow *window;
	GtkWidget *entry;
	GtkToolItem *next;
	GtkToolItem *prev;
	gulong set_focus_handler;
};

enum
{
	PROP_0,
	PROP_TEXT
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

/* public functions */

const char *
ephy_find_toolbar_get_text (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	return gtk_entry_get_text (GTK_ENTRY (priv->entry));
}

void
ephy_find_toolbar_set_controls (EphyFindToolbar *toolbar,
				gboolean can_find_next,
				gboolean can_find_prev)
{
	EphyFindToolbarPrivate *priv = toolbar->priv;

	gtk_widget_set_sensitive (GTK_WIDGET (priv->next), can_find_next);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->prev), can_find_prev);
}

/* private functions */

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
		  GObject *toolbar)
{
	g_object_notify (toolbar, "text");
}

static gboolean
entry_key_press_event_cb (GtkEntry *entry,
			  GdkEventKey *event,
			  EphyFindToolbar *toolbar)
{
	//EphyFindToolbarPrivate *priv = toolbar->priv;
	guint mask = gtk_accelerator_get_default_mod_mask ();
	gboolean handled = FALSE;

	/* Hide the toolbar when ESC is pressed */
	if ((event->state & mask) == 0)
	{
		if (event->keyval == GDK_Escape)
		{
			g_signal_emit (toolbar, signals[CLOSE], 0);

			handled = TRUE;
		}
#if 0
		else if (event->keyval == GDK_Page_Up)
		{
			ephy_command_manager_do_command
				(EPHY_COMMAND_MANAGER (priv->embed),
				 "cmd_movePageUp");
			handled = TRUE;
		}
		else if (event->keyval == GDK_Page_Down)
		{
			ephy_command_manager_do_command
				(EPHY_COMMAND_MANAGER (priv->embed),
				 "cmd_movePageDown");
			handled = TRUE;
		}
#endif
	}

	return handled;
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

	/* if widget == toolbar, the new focus widget is in the toolbar, so we
	 * don't deactivate.
	 */
	if (widget != wtoolbar)
	{
		gtk_widget_hide (wtoolbar);
	}
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

	gtk_widget_grab_focus (priv->entry);
}

static void
ephy_find_toolbar_init (EphyFindToolbar *toolbar)
{
	EphyFindToolbarPrivate *priv;
	GtkToolbar *gtoolbar;
	GtkToolItem *item;
	GtkWidget *arrow, *box, *label;

	priv = toolbar->priv = EPHY_FIND_TOOLBAR_GET_PRIVATE (toolbar);
	gtoolbar = GTK_TOOLBAR (toolbar);

	gtk_toolbar_set_style (gtoolbar, GTK_TOOLBAR_BOTH_HORIZ);

	/* Find: |_____| */
	box = gtk_hbox_new (FALSE, 12);

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _("Find:"));
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	priv->entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (priv->entry), 32);
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry), 512);
	gtk_box_pack_start (GTK_BOX (box), priv->entry, TRUE, TRUE, 0);

	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), box);
	//gtk_tool_item_set_expand (item, TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
	gtk_widget_show_all (GTK_WIDGET (item));

	// FIXME padding

	/* Next */
	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	label = gtk_label_new (_("Find Next"));
	priv->next = gtk_tool_button_new (arrow, _("Find Next"));
	gtk_tool_item_set_is_important (priv->next, TRUE);
	gtk_tool_item_set_tooltip (priv->next, gtoolbar->tooltips,
				   _("Find next occurrence of the search string"),
				   NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->next, -1);
	gtk_widget_show_all (GTK_WIDGET (priv->next));

	/* Prev */
	arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	label = gtk_label_new (_("Find Previous"));
	priv->prev = gtk_tool_button_new (arrow, _("Find Previous"));
	gtk_tool_item_set_is_important (priv->prev, TRUE);
	gtk_tool_item_set_tooltip (priv->prev, gtoolbar->tooltips,
				   _("Find previous occurrence of the search string"),
				   NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->prev, -1);
	gtk_widget_show_all (GTK_WIDGET (priv->prev));

	/* connect signals */
	g_signal_connect (priv->entry, "key-press-event",
			  G_CALLBACK (entry_key_press_event_cb), toolbar);
	g_signal_connect_after (priv->entry, "changed",
				G_CALLBACK (entry_changed_cb), toolbar);
	//g_signal_connect (GTK_ENTRY (priv->entry)->im_context, "preedit-changed",
	//		  G_CALLBACK (entry_preedit_changed_cb), toolbar);
	g_signal_connect_swapped (priv->entry, "activate",
				  G_CALLBACK (find_next_cb), toolbar);
	g_signal_connect_swapped (priv->next, "clicked",
				  G_CALLBACK (find_next_cb), toolbar);
	g_signal_connect_swapped (priv->prev, "clicked",
				  G_CALLBACK (find_prev_cb), toolbar);
}

static void
ephy_find_toolbar_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);
	EphyFindToolbarPrivate *priv = toolbar->priv;

	switch (prop_id)
	{
		case PROP_TEXT:
			gtk_entry_set_text (GTK_ENTRY (priv->entry),
					    g_value_get_string (value));
			break;
	}
}

static void
ephy_find_toolbar_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);

	switch (prop_id)
	{
		case PROP_TEXT:
			g_value_set_string (value, ephy_find_toolbar_get_text (toolbar));
			break;
	}
}

static void
ephy_find_toolbar_class_init (EphyFindToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_find_toolbar_set_property;
	object_class->get_property = ephy_find_toolbar_get_property;

	widget_class->parent_set = ephy_find_toolbar_parent_set;
	widget_class->grab_focus = ephy_find_toolbar_grab_focus;

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
		 PROP_TEXT,
		 g_param_spec_string ("text",
				      "Search string",
				      "Search string",
				      "",
				      G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (EphyFindToolbarPrivate));
}

GType
ephy_find_toolbar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
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
ephy_find_toolbar_new (void)
{
	return EPHY_FIND_TOOLBAR (g_object_new (EPHY_TYPE_FIND_TOOLBAR, NULL));
}
