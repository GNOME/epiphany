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

#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-helpers.h"
#include "ephy-link.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define TAB_WIDTH_N_CHARS 15

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2

#define INSANE_NUMBER_OF_URLS 20

#define EPHY_NOTEBOOK_TAB_GROUP_ID "0x42"

#define EPHY_NOTEBOOK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NOTEBOOK, EphyNotebookPrivate))

struct _EphyNotebookPrivate
{
	GList *focused_pages;
	guint tabs_vis_notifier_id;

	guint tabs_allowed : 1;
};

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
        { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, 0 },
	{ EPHY_DND_URI_LIST_TYPE,   0, 0 },
	{ EPHY_DND_URL_TYPE,	    0, 1 },
};

enum
{
	PROP_0,
	PROP_TABS_ALLOWED
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
		case PROP_TABS_ALLOWED:
			g_value_set_boolean (value, priv->tabs_allowed);
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
		case PROP_TABS_ALLOWED:
			ephy_notebook_set_tabs_allowed (notebook, g_value_get_boolean (value));
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
					 PROP_TABS_ALLOWED,
					 g_param_spec_boolean ("tabs-allowed", NULL, NULL,
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNotebookPrivate));
}


/* FIXME remove when gtknotebook's func for this becomes public, bug #.... */
static EphyNotebook *
find_notebook_at_pointer (GdkDisplay *display, gint abs_x, gint abs_y)
{
	GdkWindow *win_at_pointer, *toplevel_win;
	gpointer toplevel = NULL;
	gint x, y;

	win_at_pointer = gdk_device_get_window_at_position (
		gdk_device_manager_get_client_pointer (
			gdk_display_get_device_manager (display)),
		&x, &y);
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

	nb_at_pointer = find_notebook_at_pointer (gtk_widget_get_display (GTK_WIDGET (notebook)),
						  abs_x, abs_y);

	return nb_at_pointer == notebook;
}

static gint
find_tab_num_at_pos (EphyNotebook *notebook, gint abs_x, gint abs_y)
{
	int page_num = 0;
	GtkNotebook *nb = GTK_NOTEBOOK (notebook);
	GtkWidget *page;

	/* For some reason unfullscreen + quick click can
	   cause a wrong click event to be reported to the tab */
	if (!is_in_notebook_window (notebook, abs_x, abs_y))
	{
		return NOT_IN_APP_WINDOWS;
	}

	while ((page = gtk_notebook_get_nth_page (nb, page_num)))
	{
		GtkWidget *tab;
		GtkAllocation allocation;
		gint max_x, max_y;
		gint x_root, y_root;

		tab = gtk_notebook_get_tab_label (nb, page);
		g_return_val_if_fail (tab != NULL, -1);

		if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
		{
			page_num++;
			continue;
		}

		gdk_window_get_origin (gtk_widget_get_window (tab),
				       &x_root, &y_root);

		gtk_widget_get_allocation (tab, &allocation);
		max_x = x_root + allocation.x + allocation.width;
		max_y = y_root + allocation.y + allocation.height;

		if (abs_y <= max_y && abs_x <= max_x)
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
			/* Consume event so that we don't pop up the context
			 * menu when the mouse is not over a tab label.
			 */
			return TRUE;
		}

		/* Switch to the page where the mouse is over, but don't consume the
		 * event. */
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_clicked);
	}

	return FALSE;
}

static void
ephy_notebook_switch_page_cb (GtkNotebook *notebook,
			      GtkWidget *page,
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
notebook_drag_data_received_cb (GtkWidget* widget,
                                GdkDragContext *context,
				int x,
                                int y,
                                GtkSelectionData *selection_data,
				guint info,
                                guint time,
                                EphyEmbed *embed)
{
	EphyWindow *window;
	GtkWidget *notebook;
	GdkAtom target;
	const guchar *data;

	target = gtk_selection_data_get_target (selection_data);
        if (target == gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"))
                return;

	g_signal_stop_emission_by_name (widget, "drag_data_received");

	if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
				    EPHY_PREFS_LOCKDOWN_ARBITRARY_URL)) return;

	data = gtk_selection_data_get_data (selection_data);
	if (gtk_selection_data_get_length (selection_data) <= 0 || data == NULL) return;

	window = EPHY_WINDOW (gtk_widget_get_toplevel (widget));
	notebook = ephy_window_get_notebook (window);

	if (target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		char **split;

		/* URL_TYPE has format: url \n title */
		split = g_strsplit ((const gchar *) data, "\n", 2);
		if (split != NULL && split[0] != NULL && split[0][0] != '\0')
		{
			ephy_link_open (EPHY_LINK (notebook), split[0], embed,
					embed ? 0 : EPHY_LINK_NEW_TAB);
		}
		g_strfreev (split);
	}
	else if (target == gdk_atom_intern (EPHY_DND_URI_LIST_TYPE, FALSE))
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
			char *address;

			address = ephy_embed_utils_normalize_or_autosearch_address (text);
			ephy_link_open (EPHY_LINK (notebook), address, embed,
					embed ? 0 : EPHY_LINK_NEW_TAB);
			g_free (address);
			g_free (text);
		}
	}
}

/*
 * update_tabs_visibility: Hide tabs if there is only one tab
 * and the pref is not set or when in application mode.
 */
static void
update_tabs_visibility (EphyNotebook *nb,
			gboolean before_inserting)
{
	EphyEmbedShellMode mode;
	gboolean show_tabs = FALSE;
	guint num;
	EphyPrefsUITabsBarVisibilityPolicy policy;

	mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
	num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));

	if (before_inserting) num++;

	policy = g_settings_get_enum (EPHY_SETTINGS_UI,
				      EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY);

	if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION &&
	    ((policy == EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_MORE_THAN_ONE && num > 1) ||
	     policy == EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_ALWAYS))
		show_tabs = TRUE;

	/* Only show the tabs when the "tabs-allowed" property is TRUE. */
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), nb->priv->tabs_allowed && show_tabs);
}

static void
show_tabs_changed_cb (GSettings *settings,
		      char *key,
		      EphyNotebook *nb)
{
	update_tabs_visibility (nb, FALSE);
}

static void
ephy_notebook_init (EphyNotebook *notebook)
{
	EphyNotebookPrivate *priv;
        GtkWidget *widget = GTK_WIDGET (notebook);
        GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);

	priv = notebook->priv = EPHY_NOTEBOOK_GET_PRIVATE (notebook);

	gtk_notebook_set_scrollable (gnotebook, TRUE);
	gtk_notebook_set_show_border (gnotebook, FALSE);
	gtk_notebook_set_show_tabs (gnotebook, FALSE);
	gtk_notebook_set_group_name (gnotebook, EPHY_NOTEBOOK_TAB_GROUP_ID);

	priv->tabs_allowed = TRUE;

	g_signal_connect (notebook, "button-press-event",
			  (GCallback)button_press_cb, NULL);
	g_signal_connect_after (notebook, "switch-page",
				G_CALLBACK (ephy_notebook_switch_page_cb),
				NULL);

	/* Set up drag-and-drop target */
	g_signal_connect (notebook, "drag-data-received",
			  G_CALLBACK (notebook_drag_data_received_cb),
			  NULL);
	gtk_drag_dest_set (widget, 0,
			   url_drag_types, G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	gtk_drag_dest_add_text_targets (widget);

	g_signal_connect (EPHY_SETTINGS_UI,
			  "changed::" EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY,
			  G_CALLBACK (show_tabs_changed_cb), notebook);
}

static void
ephy_notebook_finalize (GObject *object)
{
	EphyNotebook *notebook = EPHY_NOTEBOOK (object);
	EphyNotebookPrivate *priv = notebook->priv;

	g_signal_handlers_disconnect_by_func (EPHY_SETTINGS_UI,
					      show_tabs_changed_cb,
					      notebook);
	g_list_free (priv->focused_pages);

	G_OBJECT_CLASS (ephy_notebook_parent_class)->finalize (object);
}

static void
sync_load_status (EphyWebView *view, GParamSpec *pspec, GtkWidget *proxy)
{
	GtkWidget *spinner, *icon;
	EphyEmbed *embed;

	spinner = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "spinner"));
	icon = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "icon"));
	g_return_if_fail (spinner != NULL && icon != NULL);

	embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);
	if (ephy_web_view_is_loading (view) && !ephy_embed_has_load_pending (embed))
	{
		gtk_widget_hide (icon);
		gtk_widget_show (spinner);
		gtk_spinner_start (GTK_SPINNER (spinner));
	}
	else
	{
		gtk_spinner_stop (GTK_SPINNER (spinner));
		gtk_widget_hide (spinner);
		gtk_widget_show (icon);
	}
}

static void
load_changed_cb (EphyWebView *view, WebKitLoadEvent load_event, GtkWidget *proxy)
{
	sync_load_status (view, NULL, proxy);
}

static void
sync_icon (EphyWebView *view,
	   GParamSpec *pspec,
	   GtkImage *icon)
{
	gtk_image_set_from_pixbuf (icon, ephy_web_view_get_icon (view));
}

static void
sync_label (EphyEmbed *embed, GParamSpec *pspec, GtkWidget *label)
{
	const char *title;

	title = ephy_embed_get_title (embed);
	gtk_label_set_text (GTK_LABEL (label), title);
	gtk_widget_set_tooltip_text (label, title);
}

static void
sync_is_playing_audio (WebKitWebView *view,
		       GParamSpec *pspec,
		       GtkWidget *speaker_icon)
{
	gtk_widget_set_visible (speaker_icon, webkit_web_view_is_playing_audio (view));
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
	GtkStyleContext *style;
	PangoFontDescription *font_desc;
	GtkWidget *button;
	int char_width, h, w;

	context = gtk_widget_get_pango_context (hbox);
	style = gtk_widget_get_style_context (hbox);
	gtk_style_context_get (style, GTK_STATE_FLAG_NORMAL,
			       "font", &font_desc, NULL);
	metrics = pango_context_get_metrics (context,
					     font_desc,
					     pango_context_get_language (context));
	pango_font_description_free (font_desc);
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
	GtkWidget *hbox, *label, *close_button, *image, *spinner, *icon, *speaker_icon;
	GtkWidget *box;
	EphyWebView *view;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show (box);

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);
	gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 0);

	/* setup load feedback */
	spinner = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);

	/* setup site icon, empty by default */
	icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	/* don't show the icon */

	/* setup label */
	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
	gtk_misc_set_padding (GTK_MISC (label), 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	/* setup speaker icon */
	speaker_icon = gtk_image_new_from_icon_name ("audio-volume-high-symbolic",
						     GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (hbox), speaker_icon, FALSE, FALSE, 0);

	/* setup close button */
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button),
			       GTK_RELIEF_NONE);
	/* don't allow focus on the close button */
	gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);

	gtk_widget_set_name (close_button, "ephy-tab-close-button");

	image = gtk_image_new_from_icon_name ("window-close-symbolic",
					      GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (close_button, _("Close tab"));
	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (close_button_clicked_cb), embed);

	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);

	gtk_box_pack_start (GTK_BOX (box), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);

	/* Set minimal size */
	g_signal_connect (box, "style-set",
			  G_CALLBACK (tab_label_style_set_cb), NULL);

	/* Set up drag-and-drop target */
	g_signal_connect (box, "drag-data-received",
			  G_CALLBACK (notebook_drag_data_received_cb), embed);
	gtk_drag_dest_set (box, GTK_DEST_DEFAULT_ALL,
			   url_drag_types, G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	gtk_drag_dest_add_text_targets (box);

	g_object_set_data (G_OBJECT (box), "label", label);
	g_object_set_data (G_OBJECT (box), "spinner", spinner);
	g_object_set_data (G_OBJECT (box), "icon", icon);
	g_object_set_data (G_OBJECT (box), "close-button", close_button);
	g_object_set_data (G_OBJECT (box), "speaker-icon", speaker_icon);

	/* Hook the label up to the tab properties */
	view = ephy_embed_get_web_view (embed);
	sync_icon (view, NULL, GTK_IMAGE (icon));
	sync_label (embed, NULL, label);
	sync_load_status (view, NULL, box);
	sync_is_playing_audio (WEBKIT_WEB_VIEW (view), NULL, speaker_icon);

	g_signal_connect_object (view, "notify::icon",
				 G_CALLBACK (sync_icon), icon, 0);
	g_signal_connect_object (embed, "notify::title",
				 G_CALLBACK (sync_label), label, 0);
	g_signal_connect_object (view, "load-changed",
				 G_CALLBACK (load_changed_cb), box, 0);
	g_signal_connect_object (view, "notify::is-playing-audio",
				 G_CALLBACK (sync_is_playing_audio), speaker_icon, 0);
	return box;
}

void
ephy_notebook_set_tabs_allowed (EphyNotebook *nb,
				gboolean tabs_allowed)
{
	EphyNotebookPrivate *priv = nb->priv;

	priv->tabs_allowed = tabs_allowed != FALSE;

	update_tabs_visibility (nb, FALSE);

	g_object_notify (G_OBJECT (nb), "tabs-allowed");
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
        gtk_notebook_set_tab_detachable (gnotebook, tab_widget, TRUE);
	gtk_container_child_set (GTK_CONTAINER (gnotebook),
				 GTK_WIDGET (tab_widget),
				 "tab-expand", TRUE,
				 NULL);

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

	gtk_container_child_set (GTK_CONTAINER (notebook),
				 GTK_WIDGET (embed),
				 "tab-expand", TRUE,
				 NULL);

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
	GtkWidget *tab_label, *tab_label_label, *tab_label_icon, *tab_label_speaker_icon;
	int position, curr;
	EphyWebView *view;

	if (!EPHY_IS_EMBED (tab_widget))
		return;

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
	tab_label_speaker_icon = g_object_get_data (G_OBJECT (tab_label), "speaker-icon");

	view = ephy_embed_get_web_view (EPHY_EMBED (tab_widget));

	g_signal_handlers_disconnect_by_func
		(view, G_CALLBACK (sync_icon), tab_label_icon);
	g_signal_handlers_disconnect_by_func
		(tab_widget, G_CALLBACK (sync_label), tab_label_label);
	g_signal_handlers_disconnect_by_func
	  (view, G_CALLBACK (sync_load_status), tab_label);
	g_signal_handlers_disconnect_by_func
		(view, G_CALLBACK (sync_is_playing_audio), tab_label_speaker_icon);

	GTK_CONTAINER_CLASS (ephy_notebook_parent_class)->remove (container, tab_widget);

	update_tabs_visibility (notebook, FALSE);
}

/**
 * ephy_notebook_next_page:
 * @notebook: an #EphyNotebook
 * 
 * Advances to the next page in the @notebook. Note that unlike
 * gtk_notebook_next_page() this method will wrap around if
 * #GtkSettings:gtk-keynav-wrap-around is set.
 **/
void
ephy_notebook_next_page (EphyNotebook *notebook)
{
	gint current_page, n_pages;

	g_return_if_fail (EPHY_IS_NOTEBOOK (notebook));

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

	if (current_page < n_pages - 1)
		gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
	else {
		gboolean  wrap_around;
		
		g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
			      "gtk-keynav-wrap-around", &wrap_around,
			      NULL);
		
		if (wrap_around)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
	}
}

/**
 * ephy_notebook_prev_page:
 * @notebook: an #EphyNotebook
 *
 * Advances to the previous page in the @notebook. Note that unlike
 * gtk_notebook_next_page() this method will wrap around if
 * #GtkSettings:gtk-keynav-wrap-around is set.
 **/
void
ephy_notebook_prev_page (EphyNotebook *notebook)
{
	gint current_page;

	g_return_if_fail (EPHY_IS_NOTEBOOK (notebook));

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

	if (current_page > 0)
		gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
	else {
		gboolean  wrap_around;
		
		g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
			      "gtk-keynav-wrap-around", &wrap_around,
			      NULL);
		
		if (wrap_around)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), -1);
	}
}
