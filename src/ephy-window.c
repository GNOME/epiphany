/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-window.h"
#include "ephy-favorites-menu.h"
#include "ephy-state.h"
#include "ephy-gobject-misc.h"
#include "ppview-toolbar.h"
#include "window-commands.h"
#include "find-dialog.h"
#include "history-dialog.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "statusbar.h"
#include "toolbar.h"
#include "popup-commands.h"
#include "ephy-encoding-menu.h"
#include "ephy-stock-icons.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <gtk/gtk.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include "egg-action-group.h"
#include "egg-menu-merge.h"
#include "egg-toggle-action.h"

static EggActionGroupEntry ephy_menu_entries [] = {

	/* Toplevel */
	{ "File", N_("_File"), NULL, NULL, NULL, NULL, NULL },
	{ "Edit", N_("_Edit"), NULL, NULL, NULL, NULL, NULL },
	{ "View", N_("_View"), NULL, NULL, NULL, NULL, NULL },
	{ "Go", N_("_Go"), NULL, NULL, NULL, NULL, NULL },
	{ "Tabs", N_("_Tabs"), NULL, NULL, NULL, NULL, NULL },
	{ "Help", N_("_Help"), NULL, NULL, NULL, NULL, NULL },

	/* File menu */
	{ "FileNewWindow", N_("_New Window"), GTK_STOCK_NEW, "<control>N",
	  N_("Create a new window"),
	  G_CALLBACK (window_cmd_file_new_window), NULL },
	{ "FileNewTab", N_("New _Tab"), EPHY_STOCK_NEW_TAB, "<control>T",
	  N_("Create a new tab"),
	  G_CALLBACK (window_cmd_file_new_tab), NULL },
	{ "FileOpen", N_("_Open..."), GTK_STOCK_OPEN, "<control>O",
	  N_("Open a file"),
	  G_CALLBACK (window_cmd_file_open), NULL },
	{ "FileSaveAs", N_("Save _As..."), GTK_STOCK_SAVE, "<shift><control>S",
	  N_("Save the current page"),
	  G_CALLBACK (window_cmd_file_save_as), NULL },
	{ "FilePrint", N_("_Print..."), GTK_STOCK_PRINT, "<control>P",
	  N_("Print the current page"),
	  G_CALLBACK (window_cmd_file_print), NULL },
	{ "FileSendTo", N_("S_end To..."), EPHY_STOCK_SEND_LINK, NULL,
	  N_("Send a link of the current page"),
	  G_CALLBACK (window_cmd_file_send_to), NULL },
	{ "FileBookmarkPage", N_("Boo_kmark Page..."), EPHY_STOCK_BOOKMARK_PAGE, "<control>D",
	  N_("Add a bookmark for the current page"),
	  G_CALLBACK (window_cmd_file_bookmark_page), NULL },
	{ "FileCloseWindow", N_("_Close"), GTK_STOCK_CLOSE, "<control>W",
	  N_("Close this window"),
	  G_CALLBACK (window_cmd_file_close_window), NULL },

	/* Edit menu */
	{ "EditCut", N_("Cu_t"), GTK_STOCK_CUT, "<control>X",
	  N_("Cut the selection"),
	  G_CALLBACK (window_cmd_edit_cut), NULL },
	{ "EditCopy", N_("_Copy"), GTK_STOCK_COPY, "<control>C",
	  N_("Copy the selection"),
	  G_CALLBACK (window_cmd_edit_copy), NULL },
	{ "EditPaste", N_("_Paste"), GTK_STOCK_PASTE, "<control>V",
	  N_("Paste clipboard"),
	  G_CALLBACK (window_cmd_edit_paste), NULL },
	{ "EditSelectAll", N_("Select _All"), NULL, "<control>A",
	  N_("Select the entire page"),
	  G_CALLBACK (window_cmd_edit_select_all), NULL },
	{ "EditFind", N_("_Find"), GTK_STOCK_FIND, "<control>F",
	  N_("Find a string"),
	  G_CALLBACK (window_cmd_edit_find), NULL },
	{ "EditFindNext", N_("Find Ne_xt"), NULL, "<control>G",
	  N_("Find next occurence of the string"),
	  G_CALLBACK (window_cmd_edit_find_next), NULL },
	{ "EditFindPrev", N_("Find Pre_vious"), NULL, "<shift><control>G",
	  N_("Find previous occurence of the string"),
	  G_CALLBACK (window_cmd_edit_find_prev), NULL },
	{ "EditPersonalData", N_("P_ersonal Data"), NULL, NULL,
	  N_("View and remove cookies and passwords"),
	  G_CALLBACK (window_cmd_edit_personal_data), NULL },
	{ "EditToolbar", N_("T_oolbars"), NULL, NULL,
	  N_("Customize toolbars"),
	  G_CALLBACK (window_cmd_edit_toolbar), NULL },
	{ "EditPrefs", N_("P_references"), GTK_STOCK_PREFERENCES, NULL,
	  N_("Configure the web browser"),
	  G_CALLBACK (window_cmd_edit_prefs), NULL },

	/* View menu */
	{ "ViewStop", N_("_Stop"), GTK_STOCK_STOP, "Escape",
	  N_("Stop current data transfer"),
	  G_CALLBACK (window_cmd_view_stop), NULL },
	{ "ViewReload", N_("_Reload"), GTK_STOCK_REFRESH, "<control>R",
	  N_("Display the latest content of the current page"),
	  G_CALLBACK (window_cmd_view_reload), NULL },
	{ "ViewToolbar", N_("_Toolbar"), NULL, "<shift><control>T",
	  N_("Show or hide toolbar"),
	  G_CALLBACK (window_cmd_view_toolbar), NULL, TOGGLE_ACTION },
	{ "ViewStatusbar", N_("St_atusbar"), NULL, NULL,
	  N_("Show or hide statusbar"),
	  G_CALLBACK (window_cmd_view_statusbar), NULL, TOGGLE_ACTION },
	{ "ViewFullscreen", N_("_Fullscreen"), EPHY_STOCK_FULLSCREEN, "F11",
	  N_("Browse at full screen"),
	  G_CALLBACK (window_cmd_view_fullscreen), NULL, TOGGLE_ACTION},
	{ "ViewZoomIn", N_("Zoom _In"), GTK_STOCK_ZOOM_IN, "<control>plus",
	  N_("Show the contents in more detail"),
	  G_CALLBACK (window_cmd_view_zoom_in), NULL },
	{ "ViewZoomOut", N_("Zoom _Out"), GTK_STOCK_ZOOM_OUT, "<control>minus",
	  N_("Show the contents in less detail"),
	  G_CALLBACK (window_cmd_view_zoom_out), NULL },
	{ "ViewZoomNormal", N_("_Normal Size"), GTK_STOCK_ZOOM_100, NULL,
	  N_("Show the contents at the normal size"),
	  G_CALLBACK (window_cmd_view_zoom_normal), NULL },
	{ "ViewEncoding", N_("_Encoding"), NULL, NULL, NULL, NULL, NULL },
	{ "ViewPageSource", N_("_Page Source"), EPHY_STOCK_VIEWSOURCE, NULL,
	  N_("View the source code of the page"),
	  G_CALLBACK (window_cmd_view_page_source), NULL },

	/* Go menu */
	{ "GoBack", N_("_Back"), GTK_STOCK_GO_BACK, "<alt>Left",
	  N_("Go to the previous visited page"),
	  G_CALLBACK (window_cmd_go_back), NULL },
	{ "GoForward", N_("_Forward"), GTK_STOCK_GO_FORWARD, "<alt>Right",
	  N_("Go to the next visited page"),
	  G_CALLBACK (window_cmd_go_forward), NULL },
	{ "GoUp", N_("_Up"), GTK_STOCK_GO_UP, "<alt>Up",
	  N_("Go up one level"),
	  G_CALLBACK (window_cmd_go_up), NULL },
	{ "GoHome", N_("_Home"), GTK_STOCK_HOME, "<alt>Home",
	  N_("Go to the home page"),
	  G_CALLBACK (window_cmd_go_home), NULL },
	{ "GoLocation", N_("_Location..."), NULL, "<control>L",
	  N_("Go to a specified location"),
	  G_CALLBACK (window_cmd_go_location), NULL },
	{ "GoHistory", N_("H_istory"), EPHY_STOCK_HISTORY, "<control>H",
	  N_("Go to an already visited page"),
	  G_CALLBACK (window_cmd_go_history), NULL },
	{ "GoBookmarks", N_("Boo_kmarks"), EPHY_STOCK_BOOKMARKS, "<control>B",
	  N_("Go to a bookmark"),
	  G_CALLBACK (window_cmd_go_bookmarks), NULL },

	/* Tabs menu */
	{ "TabsPrevious", N_("_Previous Tab"), NULL, "<control>Page_Up",
	  N_("Activate previous tab"),
	  G_CALLBACK (window_cmd_tabs_previous), NULL },
	{ "TabsNext", N_("_Next Tab"), NULL, "<control>Page_Down",
	  N_("Activate next tab"),
	  G_CALLBACK (window_cmd_tabs_next), NULL },
	{ "TabsMoveLeft", N_("Move Tab _Left"), NULL, "<shift><control>Page_Up",
	  N_("Move current tab to left"),
	  G_CALLBACK (window_cmd_tabs_move_left), NULL },
	{ "TabsMoveRight", N_("Move Tab _Right"), NULL, "<shift><control>Page_Down",
	  N_("Move current tab to right"),
	  G_CALLBACK (window_cmd_tabs_move_right), NULL },
	{ "TabsDetach", N_("_Detach Tab"), NULL, "<shift><control>M",
	  N_("Detach current tab"),
	  G_CALLBACK (window_cmd_tabs_detach), NULL },

	/* Help menu */
	{"HelpContents", N_("_Contents"), GTK_STOCK_HELP, "F1",
	 N_("Display web browser help"),
	 G_CALLBACK (window_cmd_help_contents), NULL },
	{ "HelpAbout", N_("_About"), GNOME_STOCK_ABOUT, NULL,
	  N_("Display credits for the web browser creators"),
	  G_CALLBACK (window_cmd_help_about), NULL },
};
static guint ephy_menu_n_entries = G_N_ELEMENTS (ephy_menu_entries);

static EggActionGroupEntry ephy_popups_entries [] = {
	/* Toplevel */
	{ "FakeToplevel", (""), NULL, NULL, NULL, NULL, NULL },

	/* Document */
	{ "SaveBackgroundAs", N_("_Save Background As..."), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_save_background_as), NULL },

	/* Framed document */
	{ "OpenFrame", N_("_Open Frame"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_open_frame), NULL },
	{ "OpenFrameInNewWindow", N_("Open Frame in _New Window"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_frame_in_new_window), NULL },
	{ "OpenFrameInNewTab", N_("Open Frame in New _Tab"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_frame_in_new_tab), NULL },

	/* Links */
	{ "OpenLink", N_("_Open Link"), GTK_STOCK_OPEN, NULL,
	  NULL, G_CALLBACK (popup_cmd_open_link), NULL },
	{ "OpenLinkInNewWindow", N_("Open Link in _New Window"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_link_in_new_window), NULL },
	{ "OpenLinkInNewTab", N_("Open Link in New _Tab"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_link_in_new_tab), NULL },
	{ "DownloadLink", N_("_Download Link"), GTK_STOCK_SAVE, NULL,
	  NULL, G_CALLBACK (popup_cmd_download_link), NULL },
	{ "BookmarkLink", N_("_Bookmark Link..."), EPHY_STOCK_BOOKMARK_PAGE, NULL,
	  NULL, G_CALLBACK (popup_cmd_bookmark_link), NULL },
	{ "CopyLinkLocation", N_("_Copy Link Location"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_location), NULL },
	{ "CopyEmail", N_("Copy _Email"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_email), NULL },

	/* Images */
	{ "OpenImage", N_("_Open Image"), GTK_STOCK_OPEN, NULL,
	  NULL, G_CALLBACK (popup_cmd_open_image), NULL },
	{ "OpenImageInNewWindow", N_("Open Image in _New Window"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_image_in_new_window), NULL },
	{ "OpenImageInNewTab", N_("Open Image in New _Tab"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_image_in_new_tab), NULL },
	{ "SaveImageAs", N_("_Save Image As..."), GTK_STOCK_SAVE_AS, NULL,
	  NULL, G_CALLBACK (popup_cmd_save_image_as), NULL },
	{ "SetImageAsBackground", N_("Use Image As _Background"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_set_image_as_background), NULL },
	{ "CopyImageLocation", N_("_Copy Image Location"), GTK_STOCK_COPY, NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_image_location), NULL },
};
static guint ephy_popups_n_entries = G_N_ELEMENTS (ephy_popups_entries);

struct EphyWindowPrivate
{
	GtkWidget *main_vbox;
	GtkWidget *menu_dock;
	GtkWidget *menubar;
	Toolbar *toolbar;
	GList *toolbars;
	GtkWidget *statusbar;
	EggActionGroup *action_group;
	EggActionGroup *popups_action_group;
	EphyFavoritesMenu *fav_menu;
	EphyEncodingMenu *enc_menu;
	PPViewToolbar *ppview_toolbar;
	GtkNotebook *notebook;
	EphyTab *active_tab;
	GtkWidget *sidebar;
	EphyDialog *find_dialog;
	EphyDialog *history_dialog;
	EphyDialog *history_sidebar;
	EmbedChromeMask chrome_mask;
	gboolean closing;
};

static void
ephy_window_class_init (EphyWindowClass *klass);
static void
ephy_window_init (EphyWindow *gs);
static void
ephy_window_finalize (GObject *object);
static void
ephy_window_show (GtkWidget *widget);
static void
ephy_window_notebook_switch_page_cb (GtkNotebook *notebook,
				     GtkNotebookPage *page,
				     guint page_num,
				     EphyWindow *window);

static void
ephy_window_tab_detached_cb    (EphyNotebook *notebook, gint page,
				gint x, gint y, gpointer data);


static GObjectClass *parent_class = NULL;

GType
ephy_window_get_type (void)
{
        static GType ephy_window_type = 0;

        if (ephy_window_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyWindowClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_window_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyWindow),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_window_init
                };

                ephy_window_type = g_type_register_static (GTK_TYPE_WINDOW,
							   "EphyWindow",
							   &our_info, 0);
        }

        return ephy_window_type;
}

static void
ephy_window_class_init (EphyWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_window_finalize;

	widget_class->show = ephy_window_show;
}

static gboolean
ephy_window_key_press_event_cb (GtkWidget *widget,
				GdkEventKey *event,
				EphyWindow *window)
{
        int page;

	if ((event->state & GDK_Shift_L) || (event->state & GDK_Shift_R))
                return FALSE;

        if ((event->state & GDK_Alt_L) || (event->state & GDK_Alt_R))
        {
                page = event->keyval - GDK_0 -1;

                if (page == -1) page = 9;

                if (page>=-1 && page<=9)
                {
                        gtk_notebook_set_current_page
				(GTK_NOTEBOOK (window->priv->notebook),
                                 page == -1 ? -1 : page);
                        return TRUE;
                }
        }

        return FALSE;
}

static void
ephy_window_selection_received_cb (GtkWidget *widget,
				   GtkSelectionData *selection_data,
				   guint time, EphyWindow *window)
{
	EphyTab *tab;

	if (selection_data->length <= 0 || selection_data->data == NULL)
		return;

	tab = ephy_window_get_active_tab (window);
	ephy_shell_new_tab (ephy_shell, window, tab,
			      selection_data->data, 0);
}

static void
add_widget (EggMenuMerge *merge, GtkWidget *widget, EphyWindow *window)
{
	if (GTK_IS_MENU_SHELL (widget))
	{
		window->priv->menubar = widget;
	}
	else
	{
		window->priv->toolbars = g_list_append
			(window->priv->toolbars, widget);
	}

	gtk_box_pack_start (GTK_BOX (window->priv->menu_dock),
			    widget, FALSE, FALSE, 0);
}

static void
setup_window (EphyWindow *window)
{
	EggActionGroup *action_group;
	EggMenuMerge *merge;
	int i;

	window->priv->main_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (window->priv->main_vbox);
	gtk_container_add (GTK_CONTAINER (window),
			   window->priv->main_vbox);

	window->priv->menu_dock = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (window->priv->menu_dock);
	gtk_box_pack_start (GTK_BOX (window->priv->main_vbox),
			    GTK_WIDGET (window->priv->menu_dock),
			    FALSE, TRUE, 0);

	for (i = 0; i < ephy_menu_n_entries; i++)
	{
		ephy_menu_entries[i].user_data = window;
	}

	for (i = 0; i < ephy_popups_n_entries; i++)
	{
		ephy_popups_entries[i].user_data = window;
	}

	merge = egg_menu_merge_new ();

	action_group = egg_action_group_new ("WindowActions");
	egg_action_group_add_actions (action_group, ephy_menu_entries,
				      ephy_menu_n_entries);
	egg_menu_merge_insert_action_group (merge, action_group, 0);
	window->priv->action_group = action_group;

	action_group = egg_action_group_new ("PopupsActions");
	egg_action_group_add_actions (action_group, ephy_popups_entries,
				      ephy_popups_n_entries);
	egg_menu_merge_insert_action_group (merge, action_group, 0);
	window->priv->popups_action_group = action_group;

	window->ui_merge = G_OBJECT (merge);
	g_signal_connect (merge, "add_widget", G_CALLBACK (add_widget), window);
	egg_menu_merge_add_ui_from_file
		(merge, ephy_file ("epiphany-ui.xml"), NULL);
	gtk_window_add_accel_group (GTK_WINDOW (window), merge->accel_group);

	window->priv->toolbar = toolbar_new (window);

	g_signal_connect(window,
			 "key-press-event",
                         G_CALLBACK(ephy_window_key_press_event_cb),
                         window);
	g_signal_connect (window,
			  "selection-received",
			  G_CALLBACK (ephy_window_selection_received_cb),
			  window);
}

static GtkNotebook *
setup_notebook (EphyWindow *window)
{
	GtkNotebook *notebook;

	notebook = GTK_NOTEBOOK (ephy_notebook_new ());
	gtk_notebook_set_scrollable (notebook, TRUE);
	gtk_notebook_set_show_border (notebook, FALSE);
	gtk_notebook_set_show_tabs (notebook, FALSE);

	g_signal_connect_after (G_OBJECT (notebook), "switch_page",
				G_CALLBACK (
				ephy_window_notebook_switch_page_cb),
				window);

	g_signal_connect (G_OBJECT (notebook), "tab_detached",
			  G_CALLBACK (ephy_window_tab_detached_cb),
			  NULL);

	gtk_widget_show (GTK_WIDGET (notebook));

	return notebook;
}

static void
favicon_cache_changed_cb (EphyFaviconCache *cache, char *url, EphyWindow *window)
{
	ephy_window_update_control (window, FaviconControl);
}

static void
ephy_window_init (EphyWindow *window)
{
	Session *session;
	EphyFaviconCache *cache;

	session = ephy_shell_get_session (ephy_shell);

        window->priv = g_new0 (EphyWindowPrivate, 1);
	window->priv->active_tab = NULL;
	window->priv->chrome_mask = 0;
	window->priv->closing = FALSE;
	window->priv->ppview_toolbar = NULL;
	window->priv->toolbars = NULL;

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));
	g_signal_connect_object (G_OBJECT (cache),
				 "changed",
				 G_CALLBACK (favicon_cache_changed_cb),
				 window,
				 0);

	/* Setup the window and connect verbs */
	setup_window (window);

	window->priv->fav_menu = ephy_favorites_menu_new (window);
	window->priv->enc_menu = ephy_encoding_menu_new (window);

	/* Setup window contents */
	window->priv->notebook = setup_notebook (window);
	gtk_box_pack_start (GTK_BOX (window->priv->main_vbox),
			    GTK_WIDGET (window->priv->notebook),
			    TRUE, TRUE, 0);

	window->priv->statusbar = statusbar_new ();
	gtk_widget_show (window->priv->statusbar);
	gtk_box_pack_start (GTK_BOX (window->priv->main_vbox),
			    GTK_WIDGET (window->priv->statusbar),
			    FALSE, TRUE, 0);

	g_object_ref (ephy_shell);

	/* Once window is fully created, add it to the session list*/
	session_add_window (session, window);
}

static void
save_window_chrome (EphyWindow *window)
{
	EmbedChromeMask flags = window->priv->chrome_mask;

	if (flags & EMBED_CHROME_OPENASPOPUP)
	{
	}
	else if (flags & EMBED_CHROME_PPVIEWTOOLBARON)
	{
	}
	else if (flags & EMBED_CHROME_OPENASFULLSCREEN)
	{
		eel_gconf_set_boolean (CONF_WINDOWS_FS_SHOW_TOOLBARS,
				       flags & EMBED_CHROME_TOOLBARON);
		eel_gconf_set_boolean (CONF_WINDOWS_FS_SHOW_STATUSBAR,
				       flags & EMBED_CHROME_STATUSBARON);
	}
	else
	{
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_TOOLBARS,
				       flags & EMBED_CHROME_TOOLBARON);
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_STATUSBAR,
				       flags & EMBED_CHROME_STATUSBARON);
	}
}

static void
remove_from_session (EphyWindow *window)
{
	Session *session;

	session = ephy_shell_get_session (ephy_shell);
	g_return_if_fail (session != NULL);

	session_remove_window (session, window);
}

static void
ephy_window_finalize (GObject *object)
{
        EphyWindow *window;

        g_return_if_fail (IS_EPHY_WINDOW (object));

	window = EPHY_WINDOW (object);

        g_return_if_fail (window->priv != NULL);

	remove_from_session (window);

	if (window->priv->find_dialog)
	{
		g_object_unref (G_OBJECT (window->priv->find_dialog));
	}

	if (window->priv->history_dialog)
	{
		g_object_remove_weak_pointer
                        (G_OBJECT(window->priv->history_dialog),
                         (gpointer *)&window->priv->history_dialog);
	}

	g_object_unref (window->priv->fav_menu);
	g_object_unref (window->priv->enc_menu);

	if (window->priv->toolbar)
	{
		g_object_unref (window->priv->toolbar);
	}

	if (window->priv->ppview_toolbar)
	{
		g_object_unref (window->priv->ppview_toolbar);
	}

	if (window->priv->toolbars)
	{
		g_list_free (window->priv->toolbars);
	}

	g_object_unref (window->priv->action_group);
	egg_menu_merge_remove_action_group (EGG_MENU_MERGE (window->ui_merge),
					    window->priv->action_group);
	g_object_unref (window->ui_merge);

	g_free (window->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("Ephy Window finalized %p", window)

	g_object_unref (ephy_shell);
}

EphyWindow *
ephy_window_new (void)
{
	return EPHY_WINDOW (g_object_new (EPHY_WINDOW_TYPE, NULL));
}

EmbedChromeMask
ephy_window_get_chrome (EphyWindow *window)
{
	return window->priv->chrome_mask;
}

static void
wmspec_change_state (gboolean   add,
                     GdkWindow *window,
                     GdkAtom    state1,
                     GdkAtom    state2)
{
	XEvent xev;

	#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
	#define _NET_WM_STATE_ADD           1    /* add/set property */
	#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

	xev.xclient.type = ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = True;
	xev.xclient.display = gdk_display;
	xev.xclient.window = GDK_WINDOW_XID (window);
	xev.xclient.message_type = gdk_x11_get_xatom_by_name ("_NET_WM_STATE");
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = gdk_x11_atom_to_xatom (state1);
	xev.xclient.data.l[2] = gdk_x11_atom_to_xatom (state2);

	XSendEvent (gdk_display, GDK_WINDOW_XID (gdk_get_default_root_window ()),
                    False,
                    SubstructureRedirectMask | SubstructureNotifyMask,
                    &xev);
}

static void
window_set_fullscreen_mode (EphyWindow *window, gboolean active)
{
        GdkWindow *gdk_window;

        gdk_window = GTK_WIDGET(window)->window;

        if (gdk_net_wm_supports (gdk_atom_intern ("_NET_WM_STATE_FULLSCREEN",
                                                  FALSE)))
        {
                wmspec_change_state (active,
                                     gdk_window,
                                     gdk_atom_intern ("_NET_WM_STATE_FULLSCREEN",
                                                      FALSE),
                                     GDK_NONE);
        }
        else
        {
                g_warning ("NET_WM_STATE_FULLSCREEN not supported");
	}
}

static void
translate_default_chrome (EmbedChromeMask *chrome_mask)
{
	/* keep only not layout flags */
	*chrome_mask &= (EMBED_CHROME_WINDOWRAISED |
			 EMBED_CHROME_WINDOWLOWERED |
			 EMBED_CHROME_CENTERSCREEN |
			 EMBED_CHROME_OPENASDIALOG |
			 EMBED_CHROME_OPENASCHROME |
			 EMBED_CHROME_OPENASPOPUP |
			 EMBED_CHROME_OPENASFULLSCREEN);

	/* Load defaults */
	if (*chrome_mask & EMBED_CHROME_OPENASFULLSCREEN)
	{
		if (eel_gconf_get_boolean (CONF_WINDOWS_FS_SHOW_STATUSBAR))
		{
			*chrome_mask |= EMBED_CHROME_STATUSBARON;
		}
		if (eel_gconf_get_boolean (CONF_WINDOWS_FS_SHOW_TOOLBARS))
		{
			*chrome_mask |= EMBED_CHROME_TOOLBARON;
		}
	}
	else
	{
		if (eel_gconf_get_boolean (CONF_WINDOWS_SHOW_STATUSBAR))
		{
			*chrome_mask |= EMBED_CHROME_STATUSBARON;
		}
		if (eel_gconf_get_boolean (CONF_WINDOWS_SHOW_TOOLBARS))
		{
			*chrome_mask |= EMBED_CHROME_TOOLBARON;
		}

		*chrome_mask |= EMBED_CHROME_PERSONALTOOLBARON;
		*chrome_mask |= EMBED_CHROME_MENUBARON;
	}
}

static void
update_layout_toggles (EphyWindow *window)
{
	EggActionGroup *action_group = EGG_ACTION_GROUP (window->priv->action_group);
	EmbedChromeMask mask = window->priv->chrome_mask;
	EggAction *action;

	action = egg_action_group_get_action (action_group, "ViewToolbar");
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action),
				      mask & EMBED_CHROME_TOOLBARON);

	action = egg_action_group_get_action (action_group, "ViewStatusbar");
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action),
				      mask & EMBED_CHROME_STATUSBARON);

	action = egg_action_group_get_action (action_group, "ViewFullscreen");
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action),
				      mask & EMBED_CHROME_OPENASFULLSCREEN);
}

void
ephy_window_set_chrome (EphyWindow *window,
			EmbedChromeMask flags)
{
	if (flags & EMBED_CHROME_DEFAULT)
	{
		translate_default_chrome (&flags);
	}

	if (flags & EMBED_CHROME_MENUBARON)
	{
		gtk_widget_show (window->priv->menubar);
	}
	else
	{
		gtk_widget_hide (window->priv->menubar);
	}

	if (flags & EMBED_CHROME_TOOLBARON)
	{
		g_list_foreach (window->priv->toolbars,
				(GFunc)gtk_widget_show, NULL);
	}
	else
	{
		g_list_foreach (window->priv->toolbars,
				(GFunc)gtk_widget_hide, NULL);
	}

	if (flags & EMBED_CHROME_STATUSBARON)
	{
		gtk_widget_show (window->priv->statusbar);
	}
	else
	{
		gtk_widget_hide (window->priv->statusbar);
	}

	if ((flags & EMBED_CHROME_PPVIEWTOOLBARON) != FALSE)
	{
		if (!window->priv->ppview_toolbar)
		{
			window->priv->ppview_toolbar = ppview_toolbar_new (window);
		}
	}
	else
	{
		if (window->priv->ppview_toolbar)
		{
			g_object_unref (window->priv->ppview_toolbar);
			window->priv->ppview_toolbar = NULL;
		}
	}

	/* set fullscreen only when it's really changed */
	if ((window->priv->chrome_mask & EMBED_CHROME_OPENASFULLSCREEN) !=
	    (flags & EMBED_CHROME_OPENASFULLSCREEN))
	{
		save_window_chrome (window);
		window_set_fullscreen_mode (window,
					    flags & EMBED_CHROME_OPENASFULLSCREEN);
	}

	window->priv->chrome_mask = flags;

	update_layout_toggles (window);

	save_window_chrome (window);
}

GtkWidget *
ephy_window_get_notebook (EphyWindow *window)
{
	return GTK_WIDGET (window->priv->notebook);
}

void
ephy_window_add_tab (EphyWindow *window,
		       EphyTab *tab,
		       gboolean jump_to)
{
	GtkWidget *widget;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	g_return_if_fail (IS_EPHY_TAB (tab));

	ephy_tab_set_window (tab, window);

	widget = GTK_WIDGET(ephy_tab_get_embed (tab));

	ephy_notebook_insert_page (EPHY_NOTEBOOK (window->priv->notebook),
				  widget,
				  EPHY_NOTEBOOK_INSERT_GROUPED,
				  jump_to);
}

void
ephy_window_jump_to_tab (EphyWindow *window,
			   EphyTab *tab)
{
	GtkWidget *widget;
	int page;

	widget = GTK_WIDGET(ephy_tab_get_embed (tab));

	page = gtk_notebook_page_num
		(window->priv->notebook, widget);
	gtk_notebook_set_current_page
		(window->priv->notebook, page);
}

static EphyTab *
get_tab_from_page_num (GtkNotebook *notebook, gint page_num)
{
	EphyTab *tab;
	GtkWidget *embed_widget;

	if (page_num < 0) return NULL;

	embed_widget = gtk_notebook_get_nth_page (notebook, page_num);

	g_return_val_if_fail (GTK_IS_WIDGET (embed_widget), NULL);
	tab = g_object_get_data (G_OBJECT (embed_widget), "EphyTab");
	g_return_val_if_fail (IS_EPHY_TAB (G_OBJECT (tab)), NULL);

	return tab;
}

static EphyTab *
real_get_active_tab (EphyWindow *window, int page_num)
{
	if (page_num == -1)
	{
		page_num = gtk_notebook_get_current_page (window->priv->notebook);
	}

	return get_tab_from_page_num (window->priv->notebook, page_num);
}

void
ephy_window_remove_tab (EphyWindow *window,
		        EphyTab *tab)
{
	GtkWidget *embed;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	g_return_if_fail (IS_EPHY_TAB (tab));

	window->priv->active_tab = NULL;

	embed  = GTK_WIDGET (ephy_tab_get_embed (tab));

	ephy_notebook_remove_page (EPHY_NOTEBOOK (window->priv->notebook),
				  embed);
}

void
ephy_window_load_url (EphyWindow *window,
			const char *url)
{
	EphyEmbed *embed;

        g_return_if_fail (IS_EPHY_WINDOW(window));
	embed = ephy_window_get_active_embed (window);
        g_return_if_fail (embed != NULL);
        g_return_if_fail (url != NULL);

        ephy_embed_load_url (embed, url);
}

void ephy_window_activate_location (EphyWindow *window)
{
	toolbar_activate_location (window->priv->toolbar);
}

void
ephy_window_show (GtkWidget *widget)
{
	EphyWindow *window = EPHY_WINDOW(widget);
	EphyTab *tab;
	int w = -1, h = -1;

	if (!window->priv->chrome_mask)
	{
		ephy_window_set_chrome (window, EMBED_CHROME_DEFAULT);
	}

	tab = ephy_window_get_active_tab (window);
	if (tab) ephy_tab_get_size (tab, &w, &h);

	if (!(window->priv->chrome_mask & EMBED_CHROME_OPENASPOPUP) &&
	    !(window->priv->chrome_mask & EMBED_CHROME_OPENASFULLSCREEN) &&
	    !GTK_WIDGET_VISIBLE (widget))
	{
		ephy_state_add_window (GTK_WIDGET(window),
				       "main_window",
			               600, 500);
	}

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
update_status_message (EphyWindow *window)
{
	const char *message;
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	message = ephy_tab_get_status_message (tab);
	g_return_if_fail (message != NULL);

	statusbar_set_message (STATUSBAR(window->priv->statusbar),
			       message);
}

static void
update_progress (EphyWindow *window)
{
	EphyTab *tab;
	int load_percent;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	load_percent = ephy_tab_get_load_percent (tab);

	statusbar_set_progress (STATUSBAR(window->priv->statusbar),
			        load_percent);
}

static void
update_security (EphyWindow *window)
{
	EphyEmbed *embed;
	EmbedSecurityLevel level;
	char *description;
	char *state = NULL;
	gboolean secure;
	char *tooltip;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	if (ephy_embed_get_security_level (embed, &level, &description) != G_OK)
	{
		level = STATE_IS_UNKNOWN;
		description = NULL;
	}

	secure = FALSE;
	switch (level)
	{
	case STATE_IS_UNKNOWN:
		state = _("Unknown");
		break;
	case STATE_IS_INSECURE:
		state = _("Insecure");
		break;
	case STATE_IS_BROKEN:
		state = _("Broken");
		break;
	case STATE_IS_SECURE_MED:
		state = _("Medium");
		secure = TRUE;
		break;
	case STATE_IS_SECURE_LOW:
		state = _("Low");
		secure = TRUE;
		break;
	case STATE_IS_SECURE_HIGH:
		state = _("High");
		secure = TRUE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (description != NULL)
	{
		tooltip = g_strdup_printf (_("Security level: %s\n%s"),
					   state, description);
		g_free (description);
	}
	else
	{
		tooltip = g_strdup_printf (_("Security level: %s"), state);

	}

	statusbar_set_security_state (STATUSBAR (window->priv->statusbar),
			              secure, tooltip);
	g_free (tooltip);
}

static void
update_nav_control (EphyWindow *window)
{
	gresult back, forward, up, stop;
	EphyEmbed *embed;
	EphyTab *tab;
	EggActionGroup *action_group;
	EggAction *action;

	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	back = ephy_embed_can_go_back (embed);
	forward = ephy_embed_can_go_forward (embed);
	up = ephy_embed_can_go_up (embed);
	stop = ephy_tab_get_load_status (tab) & TAB_LOAD_STARTED;

	action_group = window->priv->action_group;
	action = egg_action_group_get_action (action_group, "GoBack");
	g_object_set (action, "sensitive", !back, NULL);
	action = egg_action_group_get_action (action_group, "GoForward");
	g_object_set (action, "sensitive", !forward, NULL);
	action = egg_action_group_get_action (action_group, "GoUp");
	g_object_set (action, "sensitive", !up, NULL);
	action = egg_action_group_get_action (action_group, "ViewStop");
	g_object_set (action, "sensitive", stop, NULL);

	toolbar_update_navigation_actions (window->priv->toolbar,
					   back, forward, up);
}

static void
update_title_control (EphyWindow *window)
{
	EphyTab *tab;
	const char *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	title = ephy_tab_get_title (tab);

	if (title)
	{
		gtk_window_set_title (GTK_WINDOW(window),
				      title);
	}
}

static void
update_location_control (EphyWindow *window)
{
	EphyTab *tab;
	const char *location;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	location = ephy_tab_get_location (tab);

	if (!location) location = "";

	toolbar_set_location (window->priv->toolbar, location);
}

static void
update_favorites_control (EphyWindow *window)
{
	ephy_favorites_menu_update (window->priv->fav_menu);
}

static void
update_favicon_control (EphyWindow *window)
{
	const char *location;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	cache = ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell));

	location = ephy_tab_get_favicon_url (tab);
	if (location)
	{
		pixbuf = ephy_favicon_cache_get (cache, location);
	}
	gtk_window_set_icon (GTK_WINDOW (window), pixbuf);

	toolbar_update_favicon (window->priv->toolbar);
}

static void
update_find_control (EphyWindow *window)
{
	gboolean can_go_next, can_go_prev;

	if (window->priv->find_dialog)
	{
		can_go_next = find_dialog_can_go_next
			(FIND_DIALOG(window->priv->find_dialog));
		can_go_prev = find_dialog_can_go_prev
			(FIND_DIALOG(window->priv->find_dialog));
/*
		ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
					  EDIT_FIND_NEXT_CMD_PATH, can_go_next);
		ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
					  EDIT_FIND_PREV_CMD_PATH, can_go_prev);*/
	}
}

static void
update_window_visibility (EphyWindow *window)
{
	GList *l, *tabs;

	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		g_return_if_fail (IS_EPHY_TAB(tab));

		if (ephy_tab_get_visibility (tab))
		{
			gtk_widget_show (GTK_WIDGET(window));
			return;
		}
	}
	g_list_free (tabs);

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (window)))
	{
		gtk_widget_hide (GTK_WIDGET (window));
	}
}

static void
update_spinner_control (EphyWindow *window)
{
	GList *l, *tabs;

	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		g_return_if_fail (IS_EPHY_TAB(tab));

		if (ephy_tab_get_load_status (tab) & TAB_LOAD_STARTED)
		{
			toolbar_spinner_start (window->priv->toolbar);
			return;
		}
	}
	g_list_free (tabs);

	toolbar_spinner_stop (window->priv->toolbar);
}

void
ephy_window_update_control (EphyWindow *window,
			      ControlID control)
{
	g_return_if_fail (IS_EPHY_WINDOW (window));

	switch (control)
	{
	case StatusbarMessageControl:
		update_status_message (window);
		break;
	case StatusbarProgressControl:
		update_progress (window);
		break;
	case StatusbarSecurityControl:
		update_security (window);
		break;
	case FindControl:
		update_find_control (window);
		break;
	case ZoomControl:
		 /* the zoom control is updated at the same time than the navigation
		    controls. This keeps it synched most of the time, but not always,
		    because we don't get a notification when zoom changes */
	case NavControl:
		update_nav_control (window);
		break;
	case TitleControl:
		update_title_control (window);
		break;
	case WindowVisibilityControl:
		update_window_visibility (window);
		break;
	case SpinnerControl:
		update_spinner_control (window);
		break;
	case LocationControl:
		update_location_control (window);
		break;
	case FaviconControl:
		update_favicon_control (window);
		break;
	case FavoritesControl:
		update_favorites_control (window);
		break;
	default:
		g_warning ("unknown control specified for updating");
		break;
	}
}

void
ephy_window_update_all_controls (EphyWindow *window)
{
	g_return_if_fail (IS_EPHY_WINDOW (window));

	if (ephy_window_get_active_tab (window) != NULL)
	{
		update_nav_control (window);
		update_title_control (window);
		update_location_control (window);
		update_favicon_control (window);
		update_status_message (window);
		update_progress (window);
		update_security (window);
		update_find_control (window);
		update_spinner_control (window);
	}
}

EphyTab *
ephy_window_get_active_tab (EphyWindow *window)
{
	g_return_val_if_fail (IS_EPHY_WINDOW (window), NULL);

	return window->priv->active_tab;
}

EphyEmbed *
ephy_window_get_active_embed (EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);

	if (tab)
	{
		g_return_val_if_fail (IS_EPHY_WINDOW (G_OBJECT (window)),
				      NULL);
		return ephy_tab_get_embed (tab);
	}
	else return NULL;
}

GList *
ephy_window_get_tabs (EphyWindow *window)
{
	GList *tabs = NULL;
	GtkWidget *w;
	int i = 0;

	while ((w = gtk_notebook_get_nth_page (window->priv->notebook, i)) != NULL)
	{
		EphyTab *tab;

		tab = g_object_get_data (G_OBJECT (w), "EphyTab");
		g_return_val_if_fail (IS_EPHY_TAB (G_OBJECT (tab)), NULL);

		tabs = g_list_append (tabs, tab);
		i++;
	}

	return tabs;
}

static void
save_old_embed_status (EphyTab *tab, EphyWindow *window)
{
	char *location;

	location = toolbar_get_location (window->priv->toolbar);
	ephy_tab_set_location (tab, location);
	g_free (location);
}

static void
update_embed_dialogs (EphyWindow *window,
		      EphyTab *tab)
{
	EphyEmbed *embed;
	EphyDialog *find_dialog = window->priv->find_dialog;
	EphyDialog *history_dialog = window->priv->history_dialog;
	EphyDialog *history_sidebar = window->priv->history_sidebar;

	embed = ephy_tab_get_embed (tab);

	if (find_dialog)
	{
		ephy_embed_dialog_set_embed
			(EPHY_EMBED_DIALOG(find_dialog),
			 embed);
	}

	if (history_dialog)
	{
		ephy_embed_dialog_set_embed
			(EPHY_EMBED_DIALOG(history_dialog),
			 embed);
	}

	if (history_sidebar)
	{
		ephy_embed_dialog_set_embed
			(EPHY_EMBED_DIALOG(history_sidebar),
			 embed);
	}
}

static void
ephy_window_notebook_switch_page_cb (GtkNotebook *notebook,
				     GtkNotebookPage *page,
				     guint page_num,
				     EphyWindow *window)
{
	EphyTab *tab, *old_tab;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	if (window->priv->closing) return;

	/* get the new tab */
	tab = real_get_active_tab (window, page_num);

	/* update old tab */
	old_tab = window->priv->active_tab;
	if (old_tab && tab != old_tab)
	{
		g_return_if_fail (IS_EPHY_TAB (G_OBJECT (old_tab)));
		ephy_tab_set_is_active (old_tab, FALSE);
		save_old_embed_status (old_tab, window);
	}

	/* update new tab */
	window->priv->active_tab = tab;
	ephy_tab_set_is_active (tab, TRUE);

	update_embed_dialogs (window, tab);

	/* update window controls */
	ephy_window_update_all_controls (window);
}

static void
find_dialog_search_cb (FindDialog *dialog, EphyWindow *window)
{
	ephy_window_update_control (window, FindControl);
}

EphyDialog *
ephy_window_get_find_dialog (EphyWindow *window)
{
	EphyDialog *dialog;
	EphyEmbed *embed;

	if (window->priv->find_dialog)
	{
		return window->priv->find_dialog;
	}

	embed = ephy_window_get_active_embed (window);
	g_return_val_if_fail (GTK_IS_WINDOW(window), NULL);
	dialog = find_dialog_new_with_parent (GTK_WIDGET(window),
					      embed);

	g_signal_connect (dialog, "search",
			  G_CALLBACK (find_dialog_search_cb),
			  window);

	window->priv->find_dialog = dialog;

	return dialog;
}

void
ephy_window_show_history (EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	window->priv->history_dialog = history_dialog_new_with_parent
						(GTK_WIDGET(window),
						 embed,
						 FALSE);
	g_object_add_weak_pointer
                        (G_OBJECT(window->priv->history_dialog),
                         (gpointer *)&window->priv->history_dialog);

	ephy_dialog_show (window->priv->history_dialog);
}

void
ephy_window_set_zoom (EphyWindow *window,
		      gint zoom)
{
	EphyEmbed *embed;

        g_return_if_fail (IS_EPHY_WINDOW (window));

	embed = ephy_window_get_active_embed (window);
        g_return_if_fail (embed != NULL);

        ephy_embed_zoom_set (embed, zoom, TRUE);
}

Toolbar *
ephy_window_get_toolbar (EphyWindow *window)
{
	return window->priv->toolbar;
}

void
ephy_window_tab_detached_cb (EphyNotebook *notebook, gint page,
			     gint x, gint y, gpointer data)
{
	EphyTab *tab;
	EphyWindow *window;
	GtkWidget *src_page;

	src_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);
	tab = get_tab_from_page_num (GTK_NOTEBOOK (notebook), page);
	window = ephy_window_new ();
	ephy_notebook_move_page (notebook,
				EPHY_NOTEBOOK (ephy_window_get_notebook (window)),
				src_page, 0);
	ephy_tab_set_window (tab, window);
	gtk_widget_show (GTK_WIDGET (window));
}
