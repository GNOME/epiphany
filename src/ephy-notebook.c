/*
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-notebook.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-marshal.h"
#include "ephy-file-helpers.h"
#include "ephy-dnd.h"
#include "ephy-embed.h"
#include "ephy-window.h"
#include "ephy-shell.h"
#include "ephy-debug.h"
#include "ephy-favicon-cache.h"

#include <glib-object.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkiconfactory.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2
#define TAB_MIN_SIZE 60
#define TAB_NB_MAX    8

#define EPHY_NOTEBOOK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NOTEBOOK, EphyNotebookPrivate))

struct EphyNotebookPrivate
{
	GList *focused_pages;
	GList *opened_tabs;
	GtkTooltips *title_tips;
	guint tabs_vis_notifier_id;
	gulong motion_notify_handler_id;
	gint x_start, y_start;
	gboolean drag_in_progress;
	gboolean show_tabs;
	EphyNotebook *src_notebook;
	gint src_page;
};

static void ephy_notebook_init           (EphyNotebook *notebook);
static void ephy_notebook_class_init     (EphyNotebookClass *klass);
static void ephy_notebook_finalize       (GObject *object);
static void move_tab_to_another_notebook (EphyNotebook *src,
			                  EphyNotebook *dest,
					  gint dest_page);

/* Local variables */
static GdkCursor *cursor = NULL;
static GList *notebooks  = NULL;

static GtkTargetEntry url_drag_types [] = 
{
        { EPHY_DND_URI_LIST_TYPE,   0, 0 },
        { EPHY_DND_URL_TYPE,        0, 1 }
};
static guint n_url_drag_types = G_N_ELEMENTS (url_drag_types);

enum
{
	TAB_ADDED,
	TAB_REMOVED,
	TABS_REORDERED,
	TAB_DETACHED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };

GType
ephy_notebook_get_type (void)
{
        static GType ephy_notebook_type = 0;

        if (ephy_notebook_type == 0)
        {
                static const GTypeInfo our_info =
			{
				sizeof (EphyNotebookClass),
				NULL, /* base_init */
				NULL, /* base_finalize */
				(GClassInitFunc) ephy_notebook_class_init,
				NULL,
				NULL, /* class_data */
				sizeof (EphyNotebook),
				0, /* n_preallocs */
				(GInstanceInitFunc) ephy_notebook_init
			};

                ephy_notebook_type = g_type_register_static (GTK_TYPE_NOTEBOOK,
							     "EphyNotebook",
							     &our_info, 0);
        }

        return ephy_notebook_type;
}

static void
ephy_notebook_class_init (EphyNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_notebook_finalize;

	signals[TAB_ADDED] =
		g_signal_new ("tab_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyNotebookClass, tab_added),
			      NULL, NULL,
			      ephy_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
	signals[TAB_REMOVED] =
		g_signal_new ("tab_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyNotebookClass, tab_removed),
			      NULL, NULL,
			      ephy_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
	signals[TAB_DETACHED] =
		g_signal_new ("tab_detached",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyNotebookClass, tab_detached),
			      NULL, NULL,
			      ephy_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs_reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyNotebookClass, tabs_reordered),
			      NULL, NULL,
			      ephy_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof(EphyNotebookPrivate));
}

static gboolean
is_in_notebook_window (EphyNotebook *notebook,
		       gint abs_x, gint abs_y)
{
	gint x, y;
	gint rel_x, rel_y;
	gint width, height;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET(notebook));
	GdkWindow *window = GTK_WIDGET(toplevel)->window;

	gdk_window_get_origin (window, &x, &y);
	rel_x = abs_x - x;
	rel_y = abs_y - y;

	x = GTK_WIDGET(notebook)->allocation.x;
	y = GTK_WIDGET(notebook)->allocation.y;
	height = GTK_WIDGET(notebook)->allocation.height;
	width  = GTK_WIDGET(notebook)->allocation.width;
	return ((rel_x>=x) && (rel_y>=y) && (rel_x<=x+width) && (rel_y<=y+height));
}

static EphyNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
	GList *l;
	gint x, y;
	GdkWindow *win_at_pointer = gdk_window_at_pointer (&x, &y);
	GdkWindow *parent_at_pointer = NULL;

	if (win_at_pointer == NULL)
	{
		/* We are outside all windows containing a notebook */
		return NULL;
	}

	gdk_window_get_toplevel (win_at_pointer);
	/* When we are in the notebook event window, win_at_pointer will be
	   this event window, and the toplevel window we are interested in
	   will be its parent
	*/
	parent_at_pointer = gdk_window_get_parent (win_at_pointer);

	for (l = notebooks; l != NULL; l = l->next)
	{
		EphyNotebook *nb = EPHY_NOTEBOOK (l->data);
		GdkWindow *win = GTK_WIDGET (nb)->window;

		win = gdk_window_get_toplevel (win);
		if (((win == win_at_pointer) || (win == parent_at_pointer))
		    && is_in_notebook_window (nb, abs_x, abs_y))
		{
			return nb;
		}
	}
	return NULL;
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

static gint find_notebook_and_tab_at_pos (gint abs_x, gint abs_y,
					  EphyNotebook **notebook,
					  gint *page_num)
{
	*notebook = find_notebook_at_pointer (abs_x, abs_y);
	if (*notebook == NULL)
	{
		return NOT_IN_APP_WINDOWS;
	}
	*page_num = find_tab_num_at_pos (*notebook, abs_x, abs_y);

	if (*page_num < 0)
	{
		return *page_num;
	}
	else
	{
		return 0;
	}
}

static void
tab_label_set_size (GtkWidget *window, GtkWidget *label)
{
	int label_width;

	label_width = window->allocation.width/TAB_NB_MAX;

	if (label_width < TAB_MIN_SIZE) label_width = TAB_MIN_SIZE;

	gtk_widget_set_size_request (label, label_width, -1);
}

static void
tab_label_size_request_cb (GtkWidget *window,
			   GtkRequisition *requisition,
			   GtkWidget *child)
{
	GtkWidget *hbox;
	GtkWidget *nb;

	nb = child->parent;

	hbox = gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb),
					   child);
	tab_label_set_size (window, hbox);
}


void
ephy_notebook_move_page (EphyNotebook *src, EphyNotebook *dest,
			 GtkWidget *src_page,  gint dest_page)
{
	if (dest == NULL || src == dest)
	{
		gtk_notebook_reorder_child (GTK_NOTEBOOK (src), src_page, dest_page);
		
		if (src->priv->drag_in_progress == FALSE)
		{
			g_signal_emit (G_OBJECT (src), signals[TABS_REORDERED], 0);
		}
	}
	else
	{
		/* make sure the tab isn't destroyed while we move its embed */
		g_object_ref (G_OBJECT (src_page));
		ephy_notebook_remove_page (EPHY_NOTEBOOK (src), src_page);
		ephy_notebook_insert_page (EPHY_NOTEBOOK (dest), src_page,
					  dest_page, TRUE);
		g_object_unref (G_OBJECT (src_page));
	}
}

static void
drag_start (EphyNotebook *notebook,
	    EphyNotebook *src_notebook,
	    gint src_page)
{
	notebook->priv->drag_in_progress = TRUE;
	notebook->priv->src_notebook = src_notebook;
	notebook->priv->src_page = src_page;

	/* get a new cursor, if necessary */
	if (!cursor) cursor = gdk_cursor_new (GDK_FLEUR);

	/* grab the pointer */
	gtk_grab_add (GTK_WIDGET (notebook));
	if (!gdk_pointer_is_grabbed ()) {
		gdk_pointer_grab (GDK_WINDOW(GTK_WIDGET (notebook)->window),
				  FALSE,
				  GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
				  NULL, cursor, GDK_CURRENT_TIME);
	}
}

static void
drag_stop (EphyNotebook *notebook)
{
	if (notebook->priv->drag_in_progress)
	{
		g_signal_emit (G_OBJECT (notebook), signals[TABS_REORDERED], 0);
	}

	notebook->priv->drag_in_progress = FALSE;
	notebook->priv->src_notebook = NULL;
	notebook->priv->src_page = -1;
	if (notebook->priv->motion_notify_handler_id != 0)
	{
		g_signal_handler_disconnect (G_OBJECT (notebook),
					     notebook->priv->motion_notify_handler_id);
		notebook->priv->motion_notify_handler_id = 0;
	}
}

/* this function is only called during dnd, we don't need to emit TABS_REORDERED
 * here, instead we do it on drag_stop
 */
static void
move_tab (EphyNotebook *notebook, gint dest_page_num)
{
	gint cur_page_num;

	cur_page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

	if (dest_page_num != cur_page_num)
	{
		GtkWidget *cur_page;
		cur_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
						      cur_page_num);
		ephy_notebook_move_page (EPHY_NOTEBOOK (notebook), NULL, cur_page,
					 dest_page_num);

		/* Reset the list of newly opened tabs when moving tabs. */
		g_list_free (notebook->priv->opened_tabs);
		notebook->priv->opened_tabs = NULL;
	}
}

static gboolean
motion_notify_cb (EphyNotebook *notebook, GdkEventMotion *event,
		  gpointer data)
{
	EphyNotebook *dest;
	gint page_num;
	gint result;

	/* If the notebook only has one tab, we don't want to do
	 * anything since ephy can't handle empty notebooks
	 */
	if (g_list_length (GTK_NOTEBOOK (notebook)->children) <= 1) {
		return FALSE;
	}

	if ((notebook->priv->drag_in_progress == FALSE)
	    && (gtk_drag_check_threshold (GTK_WIDGET (notebook),
					  notebook->priv->x_start,
					  notebook->priv->y_start,
					  event->x_root, event->y_root)))
	{
		gint cur_page;

		cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
		drag_start (notebook, notebook, cur_page);
	}

	result = find_notebook_and_tab_at_pos ((gint)event->x_root,
					       (gint)event->y_root,
					       &dest, &page_num);

	if (result != NOT_IN_APP_WINDOWS)
	{
		if (dest != notebook)
		{
			move_tab_to_another_notebook (notebook, dest,
						      page_num);
		}
		else
		{
			g_assert (page_num >= -1);
			move_tab (notebook, page_num);
		}
	}

	return FALSE;
}

static void
move_tab_to_another_notebook(EphyNotebook *src,
			     EphyNotebook *dest, gint dest_page)
{
	GtkWidget *child;
	gint cur_page;

	/* This is getting tricky, the tab was dragged in a notebook
	 * in another window of the same app, we move the tab
	 * to that new notebook, and let this notebook handle the
	 * drag
	*/
	g_assert (dest != NULL);
	g_assert (dest != src);

	/* Move the widgets (tab label and tab content) to the new
	 * notebook
	 */
	cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (src));
	child    = gtk_notebook_get_nth_page (GTK_NOTEBOOK (src), cur_page);
	ephy_notebook_move_page (src, dest, child, dest_page);

	/* "Give" drag handling to the new notebook */
	drag_start (dest, src->priv->src_notebook, src->priv->src_page);
	drag_stop (src);
	gtk_grab_remove (GTK_WIDGET (src));

	dest->priv->motion_notify_handler_id =
		g_signal_connect (G_OBJECT (dest),
				  "motion-notify-event",
				  G_CALLBACK (motion_notify_cb),
				  NULL);
}

/* Callbacks */
static gboolean
button_release_cb (EphyNotebook *notebook, GdkEventButton *event,
		   gpointer data)
{
	if (notebook->priv->drag_in_progress)
	{
		gint cur_page_num;
		GtkWidget *cur_page;

		cur_page_num =
			gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
		cur_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
						      cur_page_num);

		if (!is_in_notebook_window (notebook, event->x_root, event->y_root))
		{
			/* Tab was detached */
			g_signal_emit (G_OBJECT (notebook),
				       signals[TAB_DETACHED], 0, cur_page);
		}

		/* ungrab the pointer if it's grabbed */
		if (gdk_pointer_is_grabbed ())
		{
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			gtk_grab_remove (GTK_WIDGET (notebook));
		}
	}
	/* This must be called even if a drag isn't happening */
	drag_stop (notebook);
	return FALSE;
}


static gboolean
button_press_cb (EphyNotebook *notebook,
		 GdkEventButton *event,
		 gpointer data)
{
	gint tab_clicked = find_tab_num_at_pos (notebook,
						event->x_root,
						event->y_root);

	if (notebook->priv->drag_in_progress)
	{
		return TRUE;
	}

	if ((event->button == 1) && (event->type == GDK_BUTTON_PRESS)
	    && (tab_clicked >= 0))
	{
		notebook->priv->x_start = event->x_root;
		notebook->priv->y_start = event->y_root;
		notebook->priv->motion_notify_handler_id =
			g_signal_connect (G_OBJECT (notebook),
					  "motion-notify-event",
					  G_CALLBACK (motion_notify_cb), NULL);
	}

	return FALSE;
}

GtkWidget *
ephy_notebook_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_NOTEBOOK, NULL));
}

static void
ephy_notebook_switch_page_cb (GtkNotebook *notebook,
                             GtkNotebookPage *page,
                             guint page_num,
                             gpointer data)
{
	EphyNotebook *nb = EPHY_NOTEBOOK (notebook);
	GtkWidget *child;

	child = gtk_notebook_get_nth_page (notebook, page_num);

	/* Remove the old page, we dont want to grow unnecessarily
	 * the list */
	if (nb->priv->focused_pages)
	{
		nb->priv->focused_pages =
			g_list_remove (nb->priv->focused_pages, child);
	}

	nb->priv->focused_pages = g_list_append (nb->priv->focused_pages,
						 child);

	/* Reset the list of newly opened tabs when switching tabs. */
	g_list_free (nb->priv->opened_tabs);
	nb->priv->opened_tabs = NULL;
}

#define INSANE_NUMBER_OF_URLS	20

static void
notebook_drag_data_received_cb (GtkWidget* widget, GdkDragContext *context,
				gint x, gint y, GtkSelectionData *selection_data,
				guint info, guint time, GtkWidget *child)
{
	EphyEmbed *embed = NULL;
	EphyWindow *window = NULL;
	EphyTab *tab = NULL;
	GtkWidget *toplevel;
	GList *uris = NULL, *l;
	GnomeVFSURI *uri;
	gchar *url = NULL;
	guint num = 0;
	gchar **tmp;

	g_signal_stop_emission_by_name (widget, "drag_data_received");

	if (selection_data->length <= 0 || selection_data->data == NULL) return;

	if (selection_data->target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		/* URL_TYPE has format: url \n title */
		tmp = g_strsplit (selection_data->data, "\n", 2);
		if (tmp)
		{
			uri = gnome_vfs_uri_new (tmp[0]);
			if (uri) uris = g_list_append (uris, uri);
			g_strfreev (tmp);
		}
	}
	else if (selection_data->target == gdk_atom_intern (EPHY_DND_URI_LIST_TYPE, FALSE))
	{
		uris = gnome_vfs_uri_list_parse (selection_data->data);
	}
	else
	{
		g_assert_not_reached ();
	}

	if (uris == NULL) return;

	toplevel = gtk_widget_get_toplevel (widget);
	g_return_if_fail (EPHY_IS_WINDOW (toplevel));
	window = EPHY_WINDOW (toplevel);

	if (child)
	{
		embed = EPHY_EMBED (child);
		tab = EPHY_TAB (g_object_get_data (G_OBJECT (embed), "EphyTab"));
	}

	l = uris;
	while (l != NULL && num < INSANE_NUMBER_OF_URLS)
	{
		uri = (GnomeVFSURI*) l->data;
		url = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);

		if (num == 0 && embed != NULL)
		{
			/**
			 * The first url is special: if the drag was to an
			 * existing tab, load it there
			 */
			ephy_embed_load_url (embed, url);
		}
		else
		{
			tab = ephy_shell_new_tab (ephy_shell, window,
						  tab, url,
						  EPHY_NEW_TAB_OPEN_PAGE |
						  EPHY_NEW_TAB_IN_EXISTING_WINDOW |
						  (tab ? EPHY_NEW_TAB_APPEND_AFTER :
							 EPHY_NEW_TAB_APPEND_LAST));
		}

		g_free (url);
		url = NULL;
		l = l->next;
		++num;
	}

	gnome_vfs_uri_list_free (uris);
}

/*
 * update_tabs_visibility: Hide tabs if there is only one tab
 * and the pref is not set.
 */
static void
update_tabs_visibility (EphyNotebook *nb, gboolean before_inserting)
{
	gboolean show_tabs;
	guint num;

	num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));

	if (before_inserting) num++;

	show_tabs = (eel_gconf_get_boolean (CONF_ALWAYS_SHOW_TABS_BAR) || num > 1) &&
		    nb->priv->show_tabs == TRUE;

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
	notebook->priv = EPHY_NOTEBOOK_GET_PRIVATE (notebook);

	notebook->priv->title_tips = gtk_tooltips_new ();
	g_object_ref (G_OBJECT (notebook->priv->title_tips));
	gtk_object_sink (GTK_OBJECT (notebook->priv->title_tips));

	notebook->priv->drag_in_progress = FALSE;
	notebook->priv->motion_notify_handler_id = 0;
	notebook->priv->src_notebook = NULL;
	notebook->priv->src_page = -1;
	notebook->priv->focused_pages = NULL;
	notebook->priv->opened_tabs = NULL;
	notebook->priv->show_tabs = TRUE;

	notebooks = g_list_append (notebooks, notebook);

	g_signal_connect (notebook, "button-press-event",
			  (GCallback)button_press_cb, NULL);
	g_signal_connect (notebook, "button-release-event",
			  (GCallback)button_release_cb, NULL);
	gtk_widget_add_events (GTK_WIDGET (notebook), GDK_BUTTON1_MOTION_MASK);

	g_signal_connect_after (G_OBJECT (notebook), "switch_page",
                                G_CALLBACK (ephy_notebook_switch_page_cb),
                                NULL);

	/* Set up drag-and-drop target */
	g_signal_connect (G_OBJECT(notebook), "drag_data_received",
			  G_CALLBACK(notebook_drag_data_received_cb),
			  NULL);
        gtk_drag_dest_set (GTK_WIDGET(notebook), GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_DROP,
                           url_drag_types,n_url_drag_types,
                           GDK_ACTION_MOVE | GDK_ACTION_COPY);

	notebook->priv->tabs_vis_notifier_id = eel_gconf_notification_add
		(CONF_ALWAYS_SHOW_TABS_BAR,
		 (GConfClientNotifyFunc)tabs_visibility_notifier, notebook);
}

static void
ephy_notebook_finalize (GObject *object)
{
	EphyNotebook *notebook = EPHY_NOTEBOOK (object);

	notebooks = g_list_remove (notebooks, notebook);

	eel_gconf_notification_remove (notebook->priv->tabs_vis_notifier_id);

	if (notebook->priv->focused_pages)
	{
		g_list_free (notebook->priv->focused_pages);
	}
	g_list_free (notebook->priv->opened_tabs);
	g_object_unref (notebook->priv->title_tips);

	LOG ("EphyNotebook finalised %p", object)
}

static void
sync_load_status (EphyTab *tab, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkWidget *animation = NULL, *icon = NULL;

	animation = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "loading-image"));
	icon = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "icon"));
	g_return_if_fail (animation != NULL && icon != NULL);

	switch (ephy_tab_get_load_status (tab))
	{
		case TRUE:
			gtk_widget_hide (icon);
			gtk_widget_show (animation);
			break;
		case FALSE:
			gtk_widget_hide (animation);
			gtk_widget_show (icon);
			break;
	}
}

static void
sync_icon (EphyTab *tab, GParamSpec *pspec, GtkWidget *proxy)
{
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;
	GtkImage *icon = NULL;
	const char *address;

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell)));
	address = ephy_tab_get_icon_address (tab);

	if (address)
	{
		pixbuf = ephy_favicon_cache_get (cache, address);
	}

	icon = GTK_IMAGE (g_object_get_data (G_OBJECT (proxy), "icon"));
	if (icon)
	{
		gtk_image_set_from_pixbuf (icon, pixbuf);
	}

	if (pixbuf)
	{
		g_object_unref (pixbuf);
	}
}

static void
sync_label (EphyTab *tab, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkWidget *label, *ebox;
	GtkTooltips *tips;	
	const char *title;

	ebox = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "label-ebox"));
	tips = GTK_TOOLTIPS (g_object_get_data (G_OBJECT (proxy), "tooltips"));
	label = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "label"));

	g_return_if_fail (ebox != NULL && tips != NULL && label != NULL);

	title = ephy_tab_get_title (tab);

	if (title)
	{
		gtk_label_set_label (GTK_LABEL (label), title);
		gtk_tooltips_set_tip (tips, ebox, title, NULL);
	}
}

static void
close_button_clicked_cb (GtkWidget *widget, GtkWidget *child)
{
	EphyNotebook *notebook;

	notebook = EPHY_NOTEBOOK (gtk_widget_get_parent (child));
	ephy_notebook_remove_page (notebook, child);
}

static GtkWidget *
build_tab_label (EphyNotebook *nb, GtkWidget *child)
{
	GtkWidget *label, *hbox, *close_button, *image;
	int h, w;
	GClosure *closure;
	GtkWidget *window;
	GtkWidget *loading_image, *icon;
	GtkWidget *label_ebox;
	GdkPixbufAnimation *loading_pixbuf;

	window = gtk_widget_get_toplevel (GTK_WIDGET (nb));

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = gtk_hbox_new (FALSE, 4);

	/* setup close button */
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button),
			       GTK_RELIEF_NONE);
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE,
					  GTK_ICON_SIZE_MENU);
	gtk_widget_set_size_request (close_button, w, h);
	gtk_container_add (GTK_CONTAINER (close_button),
			   image);

	/* setup load feedback image */
	/* FIXME: make the animation themeable */
	loading_pixbuf = gdk_pixbuf_animation_new_from_file (ephy_file ("epiphany-tab-loading.gif"), NULL);
	loading_image = gtk_image_new_from_animation (loading_pixbuf);
	g_object_unref (loading_pixbuf);
	gtk_box_pack_start (GTK_BOX (hbox), loading_image, FALSE, FALSE, 0);

	/* setup site icon, empty by default */
	icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

	/* setup label */
	label_ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (label_ebox), FALSE);
        label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.5);
        gtk_misc_set_padding (GTK_MISC (label), 4, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_ebox, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (label_ebox), label);

	tab_label_set_size (GTK_WIDGET (window), hbox);

	closure = g_cclosure_new (G_CALLBACK (tab_label_size_request_cb),
				  child, NULL);
	g_object_watch_closure (G_OBJECT (label), closure);
	g_signal_connect_closure_by_id (G_OBJECT (window),
		g_signal_lookup ("size_request",
				 G_OBJECT_TYPE (G_OBJECT (window))), 0,
                                 closure,
                                 FALSE);

	/* setup button */
	gtk_box_pack_start (GTK_BOX (hbox), close_button,
                            FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (close_button), "clicked",
                          G_CALLBACK (close_button_clicked_cb),
                          child);

	gtk_widget_show (hbox);
	gtk_widget_show (label_ebox);
	gtk_widget_show (label);
	gtk_widget_show (image);
	gtk_widget_show (close_button);

	g_object_set_data (G_OBJECT (hbox), "label", label);
	g_object_set_data (G_OBJECT (hbox), "label-ebox", label_ebox);
	g_object_set_data (G_OBJECT (hbox), "loading-image", loading_image);
	g_object_set_data (G_OBJECT (hbox), "icon", icon);
	g_object_set_data (G_OBJECT (hbox), "tooltips", nb->priv->title_tips);

	return hbox;
}

void
ephy_notebook_set_show_tabs (EphyNotebook *nb, gboolean show_tabs)
{
	nb->priv->show_tabs = show_tabs;

	update_tabs_visibility (nb, FALSE);
}

void
ephy_notebook_insert_page (EphyNotebook *nb,
			   GtkWidget *child,
			   int position,
			   gboolean jump_to)
{
	EphyTab *tab;
	GtkWidget *label;

	g_return_if_fail (EPHY_IS_EMBED (child));
	tab = EPHY_TAB (g_object_get_data (G_OBJECT (child), "EphyTab"));
	g_return_if_fail (EPHY_IS_TAB (tab));

	label = build_tab_label (nb, child);

	update_tabs_visibility (nb, TRUE);

	if (position == EPHY_NOTEBOOK_INSERT_GROUPED)
	{
		/* Keep a list of newly opened tabs, if the list is empty open the new
		 * tab after the current one. If it's not, add it after the newly
		 * opened tabs.
		 */
		if (nb->priv->opened_tabs != NULL)
		{
			GList *last = g_list_last (nb->priv->opened_tabs);
			GtkWidget *last_tab = last->data;
			position = gtk_notebook_page_num
				    (GTK_NOTEBOOK (nb), last_tab) + 1;
		}
		else
		{
			position = gtk_notebook_get_current_page
				    (GTK_NOTEBOOK (nb)) + 1;
		}
		nb->priv->opened_tabs =
			g_list_append (nb->priv->opened_tabs, child);
	}


	gtk_notebook_insert_page (GTK_NOTEBOOK (nb), child,
				  label, position);

	/* Set up drag-and-drop target */
	g_signal_connect (G_OBJECT(label), "drag_data_received",
			  G_CALLBACK(notebook_drag_data_received_cb), child);
	gtk_drag_dest_set (label, GTK_DEST_DEFAULT_ALL,
			   url_drag_types,n_url_drag_types,
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);

	sync_icon (tab, NULL, label);
	sync_label (tab, NULL, label);
	sync_load_status (tab, NULL, label);

	g_signal_connect_object (tab, "notify::icon",
			         G_CALLBACK (sync_icon), label, 0);
	g_signal_connect_object (tab, "notify::title",
			         G_CALLBACK (sync_label), label, 0);
	g_signal_connect_object (tab, "notify::load-status",
				 G_CALLBACK (sync_load_status), label, 0);

	if (jump_to)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (nb),
					       position);
		g_object_set_data (G_OBJECT (child), "jump_to",
				   GINT_TO_POINTER (jump_to));
	}

	g_signal_emit (G_OBJECT (nb), signals[TAB_ADDED], 0, child);
}

static void
smart_tab_switching_on_closure (EphyNotebook *nb,
				GtkWidget *child)
{
	gboolean jump_to;

	jump_to = GPOINTER_TO_INT (g_object_get_data
				   (G_OBJECT (child), "jump_to"));

	if (!jump_to || !nb->priv->focused_pages)
	{
		gtk_notebook_next_page (GTK_NOTEBOOK (nb));
	}
	else
	{
		GList *l;
		GtkWidget *child;
		int page_num;

		/* activate the last focused tab */
		l = g_list_last (nb->priv->focused_pages);
		child = GTK_WIDGET (l->data);
		page_num = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
						  child);
		gtk_notebook_set_current_page
			(GTK_NOTEBOOK (nb), page_num);
	}
}

void
ephy_notebook_remove_page (EphyNotebook *nb,
			  GtkWidget *child)
{
	int num, position, curr;
	EphyTab *tab;
	GtkWidget *label, *ebox;

	g_return_if_fail (EPHY_IS_NOTEBOOK (nb));
	g_return_if_fail (EPHY_IS_EMBED (child));

	tab = EPHY_TAB (g_object_get_data (G_OBJECT (child), "EphyTab"));
	g_return_if_fail (EPHY_IS_TAB (tab));

	num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));
	if (num <= 1)
	{
		GtkWidget *window;
		window = gtk_widget_get_toplevel (GTK_WIDGET (nb));
		gtk_widget_destroy (window);
		return;
	}

	/* Remove the page from the focused pages list */
	nb->priv->focused_pages =  g_list_remove (nb->priv->focused_pages,
						  child);
	nb->priv->opened_tabs = g_list_remove (nb->priv->opened_tabs, child);

	position = gtk_notebook_page_num (GTK_NOTEBOOK (nb), child);
	curr = gtk_notebook_get_current_page (GTK_NOTEBOOK (nb));

	if (position == curr)
	{
		smart_tab_switching_on_closure (nb, child);
	}

	label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb), child);
	ebox = GTK_WIDGET (g_object_get_data (G_OBJECT (label), "label-ebox"));
	gtk_tooltips_set_tip (GTK_TOOLTIPS (nb->priv->title_tips), ebox, NULL, NULL);

	g_signal_handlers_disconnect_by_func (label,
					      G_CALLBACK (sync_icon), tab);
	g_signal_handlers_disconnect_by_func (label,
					      G_CALLBACK (sync_label), tab);
	g_signal_handlers_disconnect_by_func (label,
					      G_CALLBACK (sync_load_status), tab);

	/**
	 * we ref the child so that it's still alive while the tabs_removed
	 * signal is processed.
	 */
	g_object_ref (child);

	gtk_notebook_remove_page (GTK_NOTEBOOK (nb), position);

	update_tabs_visibility (nb, FALSE);

	g_signal_emit (G_OBJECT (nb), signals[TAB_REMOVED], 0, child);

	g_object_unref (child);
}
