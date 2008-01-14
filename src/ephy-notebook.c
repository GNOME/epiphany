/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
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

#include "ephy-notebook.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-marshal.h"
#include "ephy-file-helpers.h"
#include "ephy-dnd.h"
#include "ephy-embed.h"
#include "ephy-window.h"
#include "ephy-shell.h"
#include "ephy-spinner.h"
#include "ephy-link.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkiconfactory.h>

#define TAB_WIDTH_N_CHARS 15

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2

#define INSANE_NUMBER_OF_URLS 20

#define EPHY_NOTEBOOK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NOTEBOOK, EphyNotebookPrivate))

struct _EphyNotebookPrivate
{
	GList *focused_pages;
	guint tabs_vis_notifier_id;

	guint show_tabs : 1;
	guint dnd_enabled : 1;
};

static void ephy_notebook_init		 (EphyNotebook *notebook);
static void ephy_notebook_class_init	 (EphyNotebookClass *klass);
static void ephy_notebook_finalize	 (GObject *object);
static int  ephy_notebook_insert_page	 (GtkNotebook *notebook,
					  GtkWidget *child,
					  GtkWidget *tab_label,
					  GtkWidget *menu_label,
					  int position);
static void ephy_notebook_remove	 (GtkContainer *container,
					  GtkWidget *tab_widget);

static const GtkTargetEntry url_drag_types [] = 
{
	{ EPHY_DND_URI_LIST_TYPE,   0, 0 },
	{ EPHY_DND_URL_TYPE,	    0, 1 }
};

enum
{
	PROP_0,
	PROP_DND_ENABLED,
	PROP_SHOW_TABS
};

enum
{
	TAB_CLOSE_REQUEST,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EphyNotebook, ephy_notebook, GTK_TYPE_NOTEBOOK,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
						NULL))

void
ephy_notebook_set_dnd_enabled (EphyNotebook *notebook,
			       gboolean enabled)
{
	EphyNotebookPrivate *priv = notebook->priv;

	priv->dnd_enabled = enabled;
	/* FIXME abort any DNDs in progress */

	g_object_notify (G_OBJECT (notebook), "dnd-enabled");
}

static void
ephy_notebook_get_property (GObject *object,
			    guint prop_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	EphyNotebook *notebook = EPHY_NOTEBOOK (object);
	EphyNotebookPrivate *priv = notebook->priv;

	switch (prop_id)
	{
		case PROP_DND_ENABLED:
			g_value_set_boolean (value, priv->dnd_enabled);
			break;
		case PROP_SHOW_TABS:
			g_value_set_boolean (value, priv->show_tabs);
			break;
	}
}

static void
ephy_notebook_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	EphyNotebook *notebook = EPHY_NOTEBOOK (object);

	switch (prop_id)
	{
		case PROP_DND_ENABLED:
			ephy_notebook_set_dnd_enabled (notebook, g_value_get_boolean (value));
			break;
		case PROP_SHOW_TABS:
			ephy_notebook_set_show_tabs (notebook, g_value_get_boolean (value));
			break;
	}
}

static void
ephy_notebook_class_init (EphyNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

	object_class->finalize = ephy_notebook_finalize;
	object_class->get_property = ephy_notebook_get_property;
	object_class->set_property = ephy_notebook_set_property;

	container_class->remove = ephy_notebook_remove;

	notebook_class->insert_page = ephy_notebook_insert_page;
	
	gtk_rc_parse_string ("style \"ephy-tab-close-button-style\"\n"
			     "{\n"
			       "GtkWidget::focus-padding = 0\n"
			       "GtkWidget::focus-line-width = 0\n"
			       "xthickness = 0\n"
			       "ythickness = 0\n"
			     "}\n"
			     "widget \"*.ephy-tab-close-button\" style \"ephy-tab-close-button-style\"");

	signals[TAB_CLOSE_REQUEST] =
		g_signal_new ("tab-close-request",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNotebookClass, tab_close_req),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET /* Can't use an interface type here */);

	g_object_class_install_property (object_class,
					 PROP_DND_ENABLED,
					 g_param_spec_boolean ("dnd-enabled", NULL, NULL,
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_SHOW_TABS,
					 g_param_spec_boolean ("show-tabs", NULL, NULL,
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNotebookPrivate));
}


/* FIXME remove when gtknotebook's func for this becomes public, bug #.... */
static EphyNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
	GdkWindow *win_at_pointer, *toplevel_win;
	gpointer toplevel = NULL;
	gint x, y;

	/* FIXME multi-head */
	win_at_pointer = gdk_window_at_pointer (&x, &y);
	if (win_at_pointer == NULL)
	{
		/* We are outside all windows containing a notebook */
		return NULL;
	}

	toplevel_win = gdk_window_get_toplevel (win_at_pointer);

	/* get the GtkWidget which owns the toplevel GdkWindow */
	gdk_window_get_user_data (toplevel_win, &toplevel);

	/* toplevel should be an EphyWindow */
	if (toplevel != NULL && EPHY_IS_WINDOW (toplevel))
	{
		return EPHY_NOTEBOOK (ephy_window_get_notebook
					(EPHY_WINDOW (toplevel)));
	}

	return NULL;
}

static gboolean
is_in_notebook_window (EphyNotebook *notebook,
		       gint abs_x, gint abs_y)
{
	EphyNotebook *nb_at_pointer;

	nb_at_pointer = find_notebook_at_pointer (abs_x, abs_y);

	return nb_at_pointer == notebook;
}

static gint
find_tab_num_at_pos (EphyNotebook *notebook, gint abs_x, gint abs_y)
{
	GtkPositionType tab_pos;
	int page_num = 0;
	GtkNotebook *nb = GTK_NOTEBOOK (notebook);
	GtkWidget *page;

	tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

	if (GTK_NOTEBOOK (notebook)->first_tab == NULL)
	{
		return AFTER_ALL_TABS;
	}

	/* For some reason unfullscreen + quick click can
	   cause a wrong click event to be reported to the tab */
	if (!is_in_notebook_window(notebook, abs_x, abs_y))
	{
		return NOT_IN_APP_WINDOWS;
	}

	while ((page = gtk_notebook_get_nth_page (nb, page_num)))
	{
		GtkWidget *tab;
		gint max_x, max_y;
		gint x_root, y_root;

		tab = gtk_notebook_get_tab_label (nb, page);
		g_return_val_if_fail (tab != NULL, -1);

		if (!GTK_WIDGET_MAPPED (GTK_WIDGET (tab)))
		{
			page_num++;
			continue;
		}

		gdk_window_get_origin (GDK_WINDOW (tab->window),
				       &x_root, &y_root);

		max_x = x_root + tab->allocation.x + tab->allocation.width;
		max_y = y_root + tab->allocation.y + tab->allocation.height;

		if (((tab_pos == GTK_POS_TOP)
		     || (tab_pos == GTK_POS_BOTTOM))
		    &&(abs_x<=max_x))
		{
			return page_num;
		}
		else if (((tab_pos == GTK_POS_LEFT)
			  || (tab_pos == GTK_POS_RIGHT))
			 && (abs_y<=max_y))
		{
			return page_num;
		}

		page_num++;
	}
	return AFTER_ALL_TABS;
}

static gboolean
button_press_cb (EphyNotebook *notebook,
		 GdkEventButton *event,
		 gpointer data)
{
	int tab_clicked;

	tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);

	if (event->type == GDK_BUTTON_PRESS &&
	    event->button == 3 &&
		   (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
	{
		if (tab_clicked == -1)
		{
			/* consume event, so that we don't pop up the context menu when
			 * the mouse if not over a tab label
			 */
			return TRUE;
		}

		/* switch to the page the mouse is over, but don't consume the event */
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_clicked);
	}

	return FALSE;
}

static void
ephy_notebook_switch_page_cb (GtkNotebook *notebook,
			      GtkNotebookPage *page,
			      guint page_num,
			      gpointer data)
{
	EphyNotebook *nb = EPHY_NOTEBOOK (notebook);
	EphyNotebookPrivate *priv = nb->priv;
	GtkWidget *child;

	child = gtk_notebook_get_nth_page (notebook, page_num);

	/* Remove the old page, we dont want to grow unnecessarily
	 * the list */
	if (priv->focused_pages)
	{
		priv->focused_pages =
			g_list_remove (priv->focused_pages, child);
	}

	priv->focused_pages = g_list_append (priv->focused_pages, child);
}

static void
notebook_drag_data_received_cb (GtkWidget* widget, GdkDragContext *context,
				gint x, gint y, GtkSelectionData *selection_data,
				guint info, guint time, EphyEmbed *embed)
{
	EphyWindow *window;
	GtkWidget *notebook;

	g_signal_stop_emission_by_name (widget, "drag_data_received");

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL)) return;

	if (selection_data->length <= 0 || selection_data->data == NULL) return;

	window = EPHY_WINDOW (gtk_widget_get_toplevel (widget));
	notebook = ephy_window_get_notebook (window);

	if (selection_data->target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		char **split;

		/* URL_TYPE has format: url \n title */
		split = g_strsplit ((const gchar *)selection_data->data, "\n", 2);
		if (split != NULL && split[0] != NULL && split[0][0] != '\0')
		{
			ephy_link_open (EPHY_LINK (notebook), split[0], embed,
					embed ? 0 : EPHY_LINK_NEW_TAB);
		}
		g_strfreev (split);
	}
	else if (selection_data->target == gdk_atom_intern (EPHY_DND_URI_LIST_TYPE, FALSE))
	{
		char **uris;
		int i;

		uris = gtk_selection_data_get_uris (selection_data);
		if (uris == NULL) return;

		for (i = 0; uris[i] != NULL && i < INSANE_NUMBER_OF_URLS; i++)
		{
			embed = ephy_link_open (EPHY_LINK (notebook), uris[i], embed,
					(embed && i == 0) ? 0 : EPHY_LINK_NEW_TAB);
		}

		g_strfreev (uris);
	}
	else
	{
		char *text;
	       
		text = (char *) gtk_selection_data_get_text (selection_data);
		if (text != NULL) {
			ephy_link_open (EPHY_LINK (notebook), text, embed,
					embed ? 0 : EPHY_LINK_NEW_TAB);
			g_free (text);
		}
	}
}

/*
 * update_tabs_visibility: Hide tabs if there is only one tab
 * and the pref is not set.
 */
static void
update_tabs_visibility (EphyNotebook *nb,
			gboolean before_inserting)
{
	EphyNotebookPrivate *priv = nb->priv;
	gboolean show_tabs;
	guint num;

	num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));

	if (before_inserting) num++;

	show_tabs = (eel_gconf_get_boolean (CONF_ALWAYS_SHOW_TABS_BAR) || num > 1) &&
		    priv->show_tabs == TRUE;

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), show_tabs);
}

static void
tabs_visibility_notifier (GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  EphyNotebook *nb)
{
	update_tabs_visibility (nb, FALSE);
}

static void
ephy_notebook_init (EphyNotebook *notebook)
{
	EphyNotebookPrivate *priv;

	priv = notebook->priv = EPHY_NOTEBOOK_GET_PRIVATE (notebook);

	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

	priv->show_tabs = TRUE;
	priv->dnd_enabled = TRUE;

	g_signal_connect (notebook, "button-press-event",
			  (GCallback)button_press_cb, NULL);
	g_signal_connect_after (notebook, "switch-page",
				G_CALLBACK (ephy_notebook_switch_page_cb),
				NULL);

	/* Set up drag-and-drop target */
	g_signal_connect (notebook, "drag-data-received",
			  G_CALLBACK (notebook_drag_data_received_cb),
			  NULL);
	gtk_drag_dest_set (GTK_WIDGET (notebook),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_DROP,
			   url_drag_types, G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	gtk_drag_dest_add_text_targets (GTK_WIDGET(notebook));

	priv->tabs_vis_notifier_id = eel_gconf_notification_add
		(CONF_ALWAYS_SHOW_TABS_BAR,
		 (GConfClientNotifyFunc)tabs_visibility_notifier, notebook);
}

static void
ephy_notebook_finalize (GObject *object)
{
	EphyNotebook *notebook = EPHY_NOTEBOOK (object);
	EphyNotebookPrivate *priv = notebook->priv;

	eel_gconf_notification_remove (priv->tabs_vis_notifier_id);

	g_list_free (priv->focused_pages);

	G_OBJECT_CLASS (ephy_notebook_parent_class)->finalize (object);
}

static void
sync_load_status (EphyEmbed *embed, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkWidget *spinner, *icon;

	spinner = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "spinner"));
	icon = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "icon"));
	g_return_if_fail (spinner != NULL && icon != NULL);

	if (ephy_embed_get_load_status (embed))
	{
		gtk_widget_hide (icon);
		gtk_widget_show (spinner);
		ephy_spinner_start (EPHY_SPINNER (spinner));
	}
	else
	{
		ephy_spinner_stop (EPHY_SPINNER (spinner));
		gtk_widget_hide (spinner);
		gtk_widget_show (icon);
	}
}

static void
sync_icon (EphyEmbed *embed,
	   GParamSpec *pspec,
	   GtkImage *icon)
{
	gtk_image_set_from_pixbuf (icon, ephy_embed_get_icon (embed));
}

static void
sync_label (EphyEmbed *embed, GParamSpec *pspec, GtkWidget *label)
{
	const char *title;

	title = ephy_embed_get_title (embed);

	gtk_label_set_text (GTK_LABEL (label), title);

	/* Set the tooltip on the label's parent (the tab label hbox),
	 * so it covers all of the tab label.
	 */
	gtk_widget_set_tooltip_text (label->parent, title);
}

static void
close_button_clicked_cb (GtkWidget *widget, GtkWidget *tab)
{
	GtkWidget *notebook;

	notebook = gtk_widget_get_parent (tab);
	g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, tab);
}

static void
tab_label_style_set_cb (GtkWidget *hbox,
			GtkStyle *previous_style,
			gpointer user_data)
{
	PangoFontMetrics *metrics;
	PangoContext *context;
	GtkWidget *button;
	int char_width, h, w;

	context = gtk_widget_get_pango_context (hbox);
	metrics = pango_context_get_metrics (context,
					     hbox->style->font_desc,
					     pango_context_get_language (context));

	char_width = pango_font_metrics_get_approximate_digit_width (metrics);
	pango_font_metrics_unref (metrics);

	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (hbox),
					   GTK_ICON_SIZE_MENU, &w, &h);

	gtk_widget_set_size_request
		(hbox, TAB_WIDTH_N_CHARS * PANGO_PIXELS(char_width) + 2 * w, -1);

	button = g_object_get_data (G_OBJECT (hbox), "close-button");
	gtk_widget_set_size_request (button, w + 2, h + 2);
}

static GtkWidget *
build_tab_label (EphyNotebook *nb, EphyEmbed *embed)
{
	GtkWidget *hbox, *label, *close_button, *image, *spinner, *icon;

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);

	/* setup load feedback */
	spinner = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (spinner), GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);

	/* setup site icon, empty by default */
	icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	/* don't show the icon */

	/* setup label */
	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* setup close button */
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button),
			       GTK_RELIEF_NONE);
	/* don't allow focus on the close button */
	gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);

	gtk_widget_set_name (close_button, "ephy-tab-close-button");

	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (close_button, _("Close tab"));
	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (close_button_clicked_cb), embed);

	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);

	gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);

	/* Set minimal size */
	g_signal_connect (hbox, "style-set",
			  G_CALLBACK (tab_label_style_set_cb), NULL);

	/* Set up drag-and-drop target */
	g_signal_connect (hbox, "drag-data-received",
			  G_CALLBACK (notebook_drag_data_received_cb), embed);
	gtk_drag_dest_set (hbox, GTK_DEST_DEFAULT_ALL,
			   url_drag_types, G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	gtk_drag_dest_add_text_targets (hbox);

	g_object_set_data (G_OBJECT (hbox), "label", label);
	g_object_set_data (G_OBJECT (hbox), "spinner", spinner);
	g_object_set_data (G_OBJECT (hbox), "icon", icon);
	g_object_set_data (G_OBJECT (hbox), "close-button", close_button);

	/* Hook the label up to the tab properties */
	sync_icon (embed, NULL, GTK_IMAGE (icon));
	sync_label (embed, NULL, label);
	sync_load_status (embed, NULL, hbox);

	g_signal_connect_object (embed, "notify::icon",
				 G_CALLBACK (sync_icon), icon, 0);
	g_signal_connect_object (embed, "notify::title",
				 G_CALLBACK (sync_label), label, 0);
	g_signal_connect_object (embed, "notify::load-status",
				 G_CALLBACK (sync_load_status), hbox, 0);

	return hbox;
}

void
ephy_notebook_set_show_tabs (EphyNotebook *nb,
			     gboolean show_tabs)
{
	EphyNotebookPrivate *priv = nb->priv;

	priv->show_tabs = show_tabs != FALSE;

	update_tabs_visibility (nb, FALSE);
}

GList *
ephy_notebook_get_focused_pages (EphyNotebook *nb)
{
	g_return_val_if_fail (EPHY_IS_NOTEBOOK (nb), NULL);

	return nb->priv->focused_pages;
}

static int
ephy_notebook_insert_page (GtkNotebook *gnotebook,
			   GtkWidget *tab_widget,
			   GtkWidget *tab_label,
			   GtkWidget *menu_label,
			   int position)
{
	EphyNotebook *notebook = EPHY_NOTEBOOK (gnotebook);

	/* Destroy passed-in tab label */
	if (tab_label != NULL)
	{
		g_object_ref_sink (tab_label);
		g_object_unref (tab_label);
	}

	g_assert (EPHY_IS_EMBED (tab_widget));

	tab_label = build_tab_label (notebook, EPHY_EMBED (tab_widget));

	update_tabs_visibility (notebook, TRUE);

	position = GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->insert_page (gnotebook,
										 tab_widget,
										 tab_label,
										 menu_label,
										 position);

	gtk_notebook_set_tab_reorderable (gnotebook, tab_widget, TRUE);

	return position;
}

int
ephy_notebook_add_tab (EphyNotebook *notebook,
		       EphyEmbed *embed,
		       int position,
		       gboolean jump_to)
{
	GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);

	g_return_val_if_fail (EPHY_IS_NOTEBOOK (notebook), -1);

	position = gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
					     GTK_WIDGET (embed),
					     NULL,
					     position);

	/* FIXME gtk bug! */
	/* FIXME: this should be fixed in gtk 2.12; check & remove this! */
	/* The signal handler may have reordered the tabs */
	position = gtk_notebook_page_num (gnotebook, GTK_WIDGET (embed));

	if (jump_to)
	{
		gtk_notebook_set_current_page (gnotebook, position);
		g_object_set_data (G_OBJECT (embed), "jump_to",
				   GINT_TO_POINTER (jump_to));

	}

	return position;
}

static void
smart_tab_switching_on_closure (EphyNotebook *notebook,
				GtkWidget *tab)
{
	EphyNotebookPrivate *priv = notebook->priv;
	gboolean jump_to;

	jump_to = GPOINTER_TO_INT (g_object_get_data
				   (G_OBJECT (tab), "jump_to"));

	if (!jump_to || !priv->focused_pages)
	{
		gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
	}
	else
	{
		GList *l;
		GtkWidget *child;
		int page_num;

		/* activate the last focused tab */
		l = g_list_last (priv->focused_pages);
		child = GTK_WIDGET (l->data);
		page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook),
						  child);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
					       page_num);
	}
}

static void
ephy_notebook_remove (GtkContainer *container,
		      GtkWidget *tab_widget)
{
	GtkNotebook *gnotebook = GTK_NOTEBOOK (container);
	EphyNotebook *notebook = EPHY_NOTEBOOK (container);
	EphyNotebookPrivate *priv = notebook->priv;
	GtkWidget *tab_label, *tab_label_label, *tab_label_icon;
	int position, curr;

	g_assert (EPHY_IS_EMBED (tab_widget));

	/* Remove the page from the focused pages list */
	priv->focused_pages =  g_list_remove (priv->focused_pages, tab_widget);

	position = gtk_notebook_page_num (gnotebook, tab_widget);
	curr = gtk_notebook_get_current_page (gnotebook);

	if (position == curr)
	{
		smart_tab_switching_on_closure (notebook, tab_widget);
	}

	/* Prepare tab label for destruction */
	tab_label = gtk_notebook_get_tab_label (gnotebook, tab_widget);
	tab_label_icon = g_object_get_data (G_OBJECT (tab_label), "icon");
	tab_label_label = g_object_get_data (G_OBJECT (tab_label), "label");

	g_signal_handlers_disconnect_by_func
		(tab_widget, G_CALLBACK (sync_icon), tab_label_icon);
	g_signal_handlers_disconnect_by_func
		(tab_widget, G_CALLBACK (sync_label), tab_label_label);
	g_signal_handlers_disconnect_by_func
	  (tab_widget, G_CALLBACK (sync_load_status), tab_label);

	GTK_CONTAINER_CLASS (ephy_notebook_parent_class)->remove (container, tab_widget);

	update_tabs_visibility (notebook, FALSE);

	/* if that was the last tab, destroy the window */
	if (gtk_notebook_get_n_pages (gnotebook) == 0)
	{
		gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (notebook)));
	}
}
