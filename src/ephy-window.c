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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-window.h"
#include "ephy-bookmarks-menu.h"
#include "ephy-favorites-menu.h"
#include "ephy-state.h"
#include "ephy-gobject-misc.h"
#include "ppview-toolbar.h"
#include "window-commands.h"
#include "find-dialog.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-zoom.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "statusbar.h"
#include "toolbar.h"
#include "popup-commands.h"
#include "ephy-encoding-menu.h"
#include "ephy-tabs-menu.h"
#include "ephy-stock-icons.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
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
	{ "Bookmarks", N_("_Bookmarks"), NULL, NULL, NULL, NULL, NULL },
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
	{ "FileSaveAs", N_("Save _As..."), GTK_STOCK_SAVE_AS, "<shift><control>S",
	  N_("Save the current page"),
	  G_CALLBACK (window_cmd_file_save_as), NULL },
	{ "FilePrint", N_("_Print..."), GTK_STOCK_PRINT, "<control>P",
	  N_("Print the current page"),
	  G_CALLBACK (window_cmd_file_print), NULL },
	{ "FileSendTo", N_("S_end To..."), EPHY_STOCK_SEND_LINK, NULL,
	  N_("Send a link of the current page"),
	  G_CALLBACK (window_cmd_file_send_to), NULL },
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
	{ "EditFind", N_("_Find..."), GTK_STOCK_FIND, "<control>F",
	  N_("Find a string"),
	  G_CALLBACK (window_cmd_edit_find), NULL },
	{ "EditFindNext", N_("Find Ne_xt"), NULL, "<control>G",
	  N_("Find next occurrence of the string"),
	  G_CALLBACK (window_cmd_edit_find_next), NULL },
	{ "EditFindPrev", N_("Find Pre_vious"), NULL, "<shift><control>G",
	  N_("Find previous occurrence of the string"),
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
	{ "ViewBookmarksBar", N_("_Bookmarks Bar"), NULL, NULL,
	  N_("Show or hide bookmarks bar"),
	  G_CALLBACK (window_cmd_view_bookmarks_bar), NULL, TOGGLE_ACTION },
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
	{ "ViewPageSource", N_("_Page Source"), EPHY_STOCK_VIEWSOURCE, "<control>U",
	  N_("View the source code of the page"),
	  G_CALLBACK (window_cmd_view_page_source), NULL },

	/* Bookmarks menu */
	{ "FileBookmarkPage", N_("_Add Bookmark"), EPHY_STOCK_BOOKMARK_PAGE, "<control>D",
	  N_("Add a bookmark for the current page"),
	  G_CALLBACK (window_cmd_file_bookmark_page), NULL },
	{ "GoBookmarks", N_("_Edit Bookmarks"), EPHY_STOCK_BOOKMARKS, "<control>B",
	  N_("Go to a bookmark"),
	  G_CALLBACK (window_cmd_go_bookmarks), NULL },

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
	{ "ContextBookmarkPage", N_("Add Boo_kmark"), EPHY_STOCK_BOOKMARK_PAGE, "<control>D",
	  N_("Add a bookmark for the current page"),
	  G_CALLBACK (window_cmd_file_bookmark_page), NULL },

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
	{ "CopyLinkAddress", N_("_Copy Link Address"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_address), NULL },

	/* Images */
	{ "OpenImage", N_("Open _Image"), GTK_STOCK_OPEN, NULL,
	  NULL, G_CALLBACK (popup_cmd_open_image), NULL },
	{ "OpenImageInNewWindow", N_("Open Image in New _Window"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_image_in_new_window), NULL },
	{ "OpenImageInNewTab", N_("Open Image in New T_ab"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_image_in_new_tab), NULL },
	{ "SaveImageAs", N_("_Save Image As..."), GTK_STOCK_SAVE_AS, NULL,
	  NULL, G_CALLBACK (popup_cmd_save_image_as), NULL },
	{ "SetImageAsBackground", N_("_Use Image As Background"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_set_image_as_background), NULL },
	{ "CopyImageLocation", N_("Copy I_mage Address"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_image_location), NULL },
};
static guint ephy_popups_n_entries = G_N_ELEMENTS (ephy_popups_entries);

struct EphyWindowPrivate
{
	GtkWidget *main_vbox;
	GtkWidget *menu_dock;
	GtkWidget *menubar;
	GtkWidget *exit_fullscreen_popup;
	Toolbar *toolbar;
	GtkWidget *statusbar;
	EggActionGroup *action_group;
	EggActionGroup *popups_action_group;
	EphyFavoritesMenu *fav_menu;
	EphyEncodingMenu *enc_menu;
	EphyTabsMenu *tabs_menu;
	EphyBookmarksMenu *bmk_menu;
	PPViewToolbar *ppview_toolbar;
	GtkNotebook *notebook;
	EphyTab *active_tab;
	EphyDialog *find_dialog;
	EmbedChromeMask chrome_mask;
	gboolean closing;
	gboolean is_fullscreen;
	gboolean has_size;
	guint num_tabs;
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
ephy_window_destroy (GtkObject *gtkobject)
{
	EphyWindow *window = EPHY_WINDOW (gtkobject);

	LOG ("EphyWindow destroy %p", window)

	window->priv->closing = TRUE;

	if (window->priv->exit_fullscreen_popup)
	{
		gtk_widget_destroy (window->priv->exit_fullscreen_popup);
		window->priv->exit_fullscreen_popup = NULL;
	}

        GTK_OBJECT_CLASS (parent_class)->destroy (gtkobject);
}

static void
ephy_window_class_init (EphyWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkObjectClass *gtkobject_class = GTK_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_window_finalize;

	widget_class->show = ephy_window_show;

	gtkobject_class->destroy = ephy_window_destroy;
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

	ephy_embed_load_url (ephy_tab_get_embed (tab), selection_data->data);
}

static void
add_widget (EggMenuMerge *merge, GtkWidget *widget, EphyWindow *window)
{
	if (GTK_IS_MENU_SHELL (widget))
	{
		window->priv->menubar = widget;
	}

	gtk_box_pack_start (GTK_BOX (window->priv->menu_dock),
			    widget, FALSE, FALSE, 0);
}

static void
menu_activate_cb (GtkWidget *widget,
	          EphyWindow *window)
{
/* FIXME we need to be notified by mozilla on selection
   changes to do this properly */
#if 0
	gboolean cut, copy, paste, select_all;
	EggActionGroup *action_group;
	EggAction *action;
	GtkWidget *focus_widget;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	focus_widget = gtk_window_get_focus (GTK_WINDOW (window));


	if (GTK_IS_EDITABLE (focus_widget))
	{
		gboolean has_selection;

		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (focus_widget), NULL, NULL);
		paste = gtk_clipboard_wait_is_text_available
			(gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
		select_all = TRUE;
		cut = has_selection;
		copy = has_selection;
	}
	else if (focus_widget == GTK_WIDGET (embed) ||
		 gtk_widget_get_ancestor (GTK_WIDGET (embed), EPHY_EMBED_TYPE))
	{
		paste = (ephy_embed_can_paste (embed) == G_OK);
		cut = (ephy_embed_selection_can_cut (embed) == G_OK);
		copy = (ephy_embed_selection_can_copy (embed) == G_OK);
		select_all = TRUE;
	}
	else
	{
		paste = FALSE;
		cut = FALSE;
		copy = FALSE;
		select_all = FALSE;
	}

	action_group = window->priv->action_group;
	action = egg_action_group_get_action (action_group, "EditCut");
	g_object_set (action, "sensitive", cut, NULL);
	action = egg_action_group_get_action (action_group, "EditCopy");
	g_object_set (action, "sensitive", copy, NULL);
	action = egg_action_group_get_action (action_group, "EditPaste");
	g_object_set (action, "sensitive", paste, NULL);
	action = egg_action_group_get_action (action_group, "EditSelectAll");
	g_object_set (action, "sensitive", select_all, NULL);
#endif
}

static void
update_exit_fullscreen_popup_position (EphyWindow *window)
{
	GdkRectangle screen_rect;
	int popup_height;

	gtk_window_get_size (GTK_WINDOW (window->priv->exit_fullscreen_popup),
                             NULL, &popup_height);

	gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
                        gdk_screen_get_monitor_at_window
                        (gdk_screen_get_default (),
                         GTK_WIDGET (window)->window),
                         &screen_rect);

	gtk_window_move (GTK_WINDOW (window->priv->exit_fullscreen_popup),
                        screen_rect.x, screen_rect.height - popup_height);
}

static void
size_changed_cb (GdkScreen *screen, EphyWindow *window)
{
	update_exit_fullscreen_popup_position (window);
}

static void
exit_fullscreen_button_clicked_cb (GtkWidget *button, EphyWindow *window)
{
	gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
update_chromes_visibility (EphyWindow *window, EmbedChromeMask flags)
{
	gboolean fullscreen;

	fullscreen = window->priv->is_fullscreen;

	if (!fullscreen && flags & EMBED_CHROME_MENUBARON)
	{
		gtk_widget_show (window->priv->menubar);
	}
	else
	{
		gtk_widget_hide (window->priv->menubar);
	}

	toolbar_set_visibility (window->priv->toolbar,
				flags & EMBED_CHROME_TOOLBARON,
				flags & EMBED_CHROME_BOOKMARKSBARON);

	if (!fullscreen && flags & EMBED_CHROME_STATUSBARON)
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
}

static void
ephy_window_fullscreen (EphyWindow *window)
{
	GtkWidget *popup, *button, *icon, *label, *hbox;
	EphyToolbarsModel *tmodel;

	window->priv->is_fullscreen = TRUE;

	tmodel = ephy_shell_get_toolbars_model (ephy_shell);
	ephy_toolbars_model_set_flag (tmodel, EGG_TB_MODEL_ICONS_ONLY);

	popup = gtk_window_new (GTK_WINDOW_POPUP);
	window->priv->exit_fullscreen_popup = popup;

	button = gtk_button_new ();
	g_signal_connect (button, "clicked",
			  G_CALLBACK (exit_fullscreen_button_clicked_cb),
			  window);
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (popup), button);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	icon = gtk_image_new_from_stock (GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

	label = gtk_label_new (_("Exit Fullscreen"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	update_exit_fullscreen_popup_position (window);

	gtk_widget_show (popup);

	g_signal_connect (G_OBJECT (gdk_screen_get_default ()),
                          "size-changed", G_CALLBACK (size_changed_cb),
			  popup);

	update_chromes_visibility (window, window->priv->chrome_mask);
}

static void
ephy_window_unfullscreen (EphyWindow *window)
{
	EphyToolbarsModel *tmodel;

	window->priv->is_fullscreen = FALSE;

	tmodel = ephy_shell_get_toolbars_model (ephy_shell);
	ephy_toolbars_model_unset_flag (tmodel, EGG_TB_MODEL_ICONS_ONLY);

	g_signal_handlers_disconnect_by_func (G_OBJECT (gdk_screen_get_default ()),
                                              G_CALLBACK (size_changed_cb),
					      window);

	gtk_widget_destroy (window->priv->exit_fullscreen_popup);
	window->priv->exit_fullscreen_popup = NULL;

	update_chromes_visibility (window, window->priv->chrome_mask);
}

static gboolean
ephy_window_state_event_cb (GtkWidget *widget, GdkEventWindowState *event, EphyWindow *window)
{
	if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
	{
		EggAction *action;
		gboolean fullscreen;

		fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

		if (fullscreen)
		{
			ephy_window_fullscreen (window);
		}
		else
		{
			ephy_window_unfullscreen (window);
		}

		action = egg_action_group_get_action (window->priv->action_group,
						      "ViewFullscreen");
		egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action), fullscreen);
	}

	return FALSE;
}

static void
setup_window (EphyWindow *window)
{
	EggActionGroup *action_group;
	EggAction *action;
	EggMenuMerge *merge;
	GtkWidget *menu;
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
	action = egg_action_group_get_action (action_group, "FileOpen");
	g_object_set (action, "short_label", _("Open"), NULL);
	action = egg_action_group_get_action (action_group, "FileSaveAs");
	g_object_set (action, "short_label", _("Save As"), NULL);
	action = egg_action_group_get_action (action_group, "FilePrint");
	g_object_set (action, "short_label", _("Print"), NULL);
	action = egg_action_group_get_action (action_group, "FileBookmarkPage");
	g_object_set (action, "short_label", _("Bookmark"), NULL);
	action = egg_action_group_get_action (action_group, "GoBookmarks");
	g_object_set (action, "short_label", _("Bookmarks"), NULL);

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
	egg_menu_merge_ensure_update (merge);

	menu = egg_menu_merge_get_widget (merge, "/menu/EditMenu");
	g_signal_connect (menu, "activate", G_CALLBACK (menu_activate_cb), window);

	window->priv->toolbar = toolbar_new (window);
	gtk_widget_show (GTK_WIDGET (window->priv->toolbar));
	gtk_box_pack_start (GTK_BOX (window->priv->menu_dock),
			    GTK_WIDGET (window->priv->toolbar),
			    FALSE, FALSE, 0);

	g_signal_connect (window,
			  "selection-received",
			  G_CALLBACK (ephy_window_selection_received_cb),
			  window);
	g_signal_connect (window, "window-state-event",
			  G_CALLBACK (ephy_window_state_event_cb),
			  window);
}

static void
sync_find_dialog (FindDialog *dialog, GParamSpec *spec, EphyWindow *window)
{
	EggActionGroup *action_group;
	EggAction *action;
	FindNavigationFlags flags;
	gboolean can_go_prev = FALSE, can_go_next = FALSE;

	flags = find_dialog_get_navigation_flags (dialog);

	if (flags & FIND_CAN_GO_PREV)
	{
		can_go_prev = TRUE;
	}
	if (flags & FIND_CAN_GO_NEXT)
	{
		can_go_next = TRUE;
	}

	action_group = window->priv->action_group;
	action = egg_action_group_get_action (action_group, "EditFindPrev");
	g_object_set (action, "sensitive", can_go_prev, NULL);
	action = egg_action_group_get_action (action_group, "EditFindNext");
	g_object_set (action, "sensitive", can_go_next, NULL);
}

static void
sync_tab_address (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	const char *address;

	if (window->priv->closing) return;

	address = ephy_tab_get_location (tab);

	if (address == NULL)
	{
		address = "";
	}

	toolbar_set_location (window->priv->toolbar, address);
}

static void
sync_tab_icon (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	const char *address;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	if (window->priv->closing) return;

	cache = ephy_embed_shell_get_favicon_cache
			(EPHY_EMBED_SHELL (ephy_shell));

	address = ephy_tab_get_icon_address (tab);

	if (address)
	{
		pixbuf = ephy_favicon_cache_get (cache, address);
	}

	gtk_window_set_icon (GTK_WINDOW (window), pixbuf);

	toolbar_update_favicon (window->priv->toolbar);

	if (pixbuf)
	{
		g_object_unref (pixbuf);
	}
}

static void
sync_tab_load_progress (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	if (window->priv->closing) return;

	statusbar_set_progress (STATUSBAR (window->priv->statusbar),
				ephy_tab_get_load_percent (tab));
}

static void
sync_tab_load_status (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	gboolean spin = FALSE;
	GList *tabs, *l;

	if (window->priv->closing) return;

	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		g_return_if_fail (IS_EPHY_TAB(tab));

		if (ephy_tab_get_load_status (tab))
		{
			spin = TRUE;
			break;
		}
	}
	g_list_free (tabs);

	if (spin)
	{
		toolbar_spinner_start (window->priv->toolbar);
	}
	else
	{
		toolbar_spinner_stop (window->priv->toolbar);
	}
}

static void
sync_tab_message (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	if (window->priv->closing) return;

	statusbar_set_message (STATUSBAR (window->priv->statusbar),
			       ephy_tab_get_status_message (tab));
}

static void
sync_tab_navigation (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	TabNavigationFlags flags;
	EggActionGroup *action_group;
	EggAction *action;
	gboolean up = FALSE, back = FALSE, forward = FALSE;

	if (window->priv->closing) return;

	flags = ephy_tab_get_navigation_flags (tab);

	if (flags & TAB_NAV_UP)
	{
		up = TRUE;
	}
	if (flags & TAB_NAV_BACK)
	{
		back = TRUE;
	}
	if (flags & TAB_NAV_FORWARD)
	{
		forward = TRUE;
	}

	action_group = window->priv->action_group;
	action = egg_action_group_get_action (action_group, "GoUp");
	g_object_set (action, "sensitive", up, NULL);
	action = egg_action_group_get_action (action_group, "GoBack");
	g_object_set (action, "sensitive", back, NULL);
	action = egg_action_group_get_action (action_group, "GoForward");
	g_object_set (action, "sensitive", forward, NULL);

	toolbar_update_navigation_actions (window->priv->toolbar,
					   back, forward, up);
}

static void
sync_tab_security (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	EphyEmbed *embed;
	EmbedSecurityLevel level;
	char *description = NULL;
	char *state = NULL;
	gboolean secure;
	char *tooltip;

	if (window->priv->closing) return;

	embed = ephy_tab_get_embed (tab);

	if (ephy_embed_get_security_level (embed, &level, &description) != G_OK)
	{
		level = STATE_IS_UNKNOWN;
		description = NULL;
	}

	if (level != ephy_tab_get_security_level (tab))
	{
		/* something is VERY wrong here! */
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
sync_tab_stop (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	EggAction *action;

	if (window->priv->closing) return;

	action = egg_action_group_get_action (window->priv->action_group, "ViewStop");

	g_object_set (action, "sensitive", ephy_tab_get_load_status (tab), NULL);
}

static void
sync_tab_title (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	const char *title;

	if (window->priv->closing) return;

	title = ephy_tab_get_title (tab);

	if (title)
	{
		gtk_window_set_title (GTK_WINDOW(window),
				      title);
	}
}

static void
sync_tab_visibility (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	GList *l, *tabs;
	gboolean visible = FALSE;

	if (window->priv->closing) return;

	tabs = ephy_window_get_tabs (window);
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		g_return_if_fail (IS_EPHY_TAB(tab));

		if (ephy_tab_get_visibility (tab))
		{
			visible = TRUE;
			break;
		}
	}
	g_list_free (tabs);

	if (visible)
	{
		gtk_widget_show (GTK_WIDGET(window));
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET (window));
	}
}

static void
sync_tab_zoom (EphyTab *tab, GParamSpec *pspec, EphyWindow *window)
{
	EggActionGroup *action_group;
	EggAction *action;
	gboolean can_zoom_in = TRUE, can_zoom_out = TRUE, can_zoom_normal = FALSE;
	float zoom;

	if (window->priv->closing) return;

	zoom = ephy_tab_get_zoom (tab);

	if (zoom >= ZOOM_MAXIMAL)
	{
		can_zoom_in = FALSE;
	}
	if (zoom <= ZOOM_MINIMAL)
	{
		can_zoom_out = FALSE;
	}
	if (zoom != 1.0)
	{
		can_zoom_normal = TRUE;
	}

	toolbar_update_zoom (window->priv->toolbar, zoom);

	action_group = window->priv->action_group;
	action = egg_action_group_get_action (action_group, "ViewZoomIn");
	g_object_set (action, "sensitive", can_zoom_in, NULL);
	action = egg_action_group_get_action (action_group, "ViewZoomOut");
	g_object_set (action, "sensitive", can_zoom_out, NULL);
	action = egg_action_group_get_action (action_group, "ViewZoomNormal");
	g_object_set (action, "sensitive", can_zoom_normal, NULL);
}

static void
popup_menu_at_coords (GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
		      gpointer user_data)
{
	EphyEmbedEvent *event = user_data;

	*x = event->x;
	*y = event->y;
	*push_in = TRUE;
}

static void
popup_destroy_cb (GtkWidget *widget, EphyWindow *window)
{
	EphyEmbedEvent *event;

	event = EPHY_EMBED_EVENT (g_object_get_data (G_OBJECT (window), "context_event"));
	g_object_set_data (G_OBJECT (window), "context_event", NULL);

	g_object_unref (event);
}

static void
show_embed_popup (EphyWindow *window, EphyTab *tab, EphyEmbedEvent *event)
{
	EggActionGroup *action_group;
	EggAction *action;
	EmbedEventContext context;
	const char *popup;
	char *path;
	const GValue *value;
	gboolean framed, has_background;
	GtkWidget *widget;
	EphyEmbedEventType type;

	ephy_embed_event_get_property (event, "framed_page", &value);
	framed = g_value_get_int (value);

	has_background = ephy_embed_event_has_property (event, "background_image");

	ephy_embed_event_get_context (event, &context);

	if ((context & EMBED_CONTEXT_LINK) &&
	    (context & EMBED_CONTEXT_IMAGE))
	{
		popup = "EphyImageLinkPopup";
	}
	else if (context & EMBED_CONTEXT_LINK)
	{
		popup = "EphyLinkPopup";
	}
	else if (context & EMBED_CONTEXT_IMAGE)
	{
		popup = "EphyImagePopup";
	}
	else if (context & EMBED_CONTEXT_INPUT)
	{
		popup = "EphyInputPopup";
	}
	else
	{
		popup = framed ? "EphyFramedDocumentPopup" :
				 "EphyDocumentPopup";
	}

	action_group = window->priv->popups_action_group;
	action = egg_action_group_get_action (action_group, "SaveBackgroundAs");
	g_object_set (action, "sensitive", has_background,
			      "visible", has_background, NULL);

	path = g_strconcat ("/popups/", popup, NULL);
	widget = egg_menu_merge_get_widget (EGG_MENU_MERGE (window->ui_merge),
				            path);
	g_free (path);

	g_return_if_fail (widget != NULL);

	g_object_ref (event);
	g_object_set_data (G_OBJECT (window), "context_event", event);
	g_signal_connect (widget, "destroy",
			  G_CALLBACK (popup_destroy_cb), window);

	ephy_embed_event_get_event_type (event, &type);
	if (type == EPHY_EMBED_EVENT_KEY)
	{
		gtk_menu_popup (GTK_MENU (widget), NULL, NULL,
				popup_menu_at_coords, event, 2,
				gtk_get_current_event_time ());
	}
	else
	{
		gtk_menu_popup (GTK_MENU (widget), NULL, NULL,
				NULL, NULL, 2,
				gtk_get_current_event_time ());
	}
}

static gint
tab_context_menu_cb (EphyEmbed *embed,
		     EphyEmbedEvent *event,
		     EphyWindow *window)
{
	EphyTab *tab;

	g_return_val_if_fail (IS_EPHY_WINDOW (window), FALSE);
	g_return_val_if_fail (IS_EPHY_EMBED (embed), FALSE);
	g_assert (IS_EPHY_EMBED_EVENT(event));

	tab = EPHY_TAB (g_object_get_data (G_OBJECT (embed), "EphyTab"));
	g_return_val_if_fail (IS_EPHY_TAB (tab), FALSE);
	g_return_val_if_fail (window->priv->active_tab == tab, FALSE);

	window = ephy_tab_get_window (tab);
	g_return_val_if_fail (window != NULL, FALSE);

	show_embed_popup (window, tab, event);

	return FALSE;
}

static void
ephy_window_set_active_tab (EphyWindow *window, EphyTab *new_tab)
{
	EphyTab *old_tab;
	EphyEmbed *embed;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	if (ephy_tab_get_window (new_tab) != window) return;

	old_tab = window->priv->active_tab;

	if (old_tab == new_tab) return;

	if (old_tab)
	{
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_address),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_icon),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_load_progress),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_stop),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_message),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_navigation),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_security),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_title),
						      window);
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_tab),
						      G_CALLBACK (sync_tab_zoom),
						      window);

		embed = ephy_tab_get_embed (old_tab);
		g_signal_handlers_disconnect_by_func (G_OBJECT (embed),
						      G_CALLBACK (tab_context_menu_cb),
						      window);
	}

	window->priv->active_tab = new_tab;

	if (new_tab)
	{
		sync_tab_address	(new_tab, NULL, window);
		sync_tab_icon		(new_tab, NULL, window);
		sync_tab_load_progress	(new_tab, NULL, window);
		sync_tab_stop		(new_tab, NULL, window);
		sync_tab_message	(new_tab, NULL, window);
		sync_tab_navigation	(new_tab, NULL, window);
		sync_tab_security	(new_tab, NULL, window);
		sync_tab_title		(new_tab, NULL, window);
		sync_tab_zoom		(new_tab, NULL, window);

		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::address",
					 G_CALLBACK (sync_tab_address),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::icon",
					 G_CALLBACK (sync_tab_icon),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::load-progress",
					 G_CALLBACK (sync_tab_load_progress),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::load-status",
					 G_CALLBACK (sync_tab_stop),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::message",
					 G_CALLBACK (sync_tab_message),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::navigation",
					 G_CALLBACK (sync_tab_navigation),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::security-level",
					 G_CALLBACK (sync_tab_security),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::title",
					 G_CALLBACK (sync_tab_title),
					 window, 0);
		g_signal_connect_object (G_OBJECT (new_tab),
					 "notify::zoom",
					 G_CALLBACK (sync_tab_zoom),
					 window, 0);

		embed = ephy_tab_get_embed (new_tab);
		g_signal_connect_object (embed, "ge_context_menu",
					 G_CALLBACK (tab_context_menu_cb),
					 window, 0);
	}
}

static void
update_tabs_menu_sensitivity (EphyWindow *window)
{
	gboolean prev_tab, next_tab, move_left, move_right, detach;
	EggActionGroup *action_group;
	EggAction *action;
	int current;
	int last;

	current = gtk_notebook_get_current_page
		(GTK_NOTEBOOK (window->priv->notebook));
	last = gtk_notebook_get_n_pages
		(GTK_NOTEBOOK (window->priv->notebook)) - 1;
	prev_tab = move_left = (current > 0);
	next_tab = move_right = (current < last);
	detach = gtk_notebook_get_n_pages
		(GTK_NOTEBOOK (window->priv->notebook)) > 1;

	action_group = window->priv->action_group;
	action = egg_action_group_get_action (action_group, "TabsPrevious");
	g_object_set (action, "sensitive", prev_tab, NULL);
	action = egg_action_group_get_action (action_group, "TabsNext");
	g_object_set (action, "sensitive", next_tab, NULL);
	action = egg_action_group_get_action (action_group, "TabsMoveLeft");
	g_object_set (action, "sensitive", move_left, NULL);
	action = egg_action_group_get_action (action_group, "TabsMoveRight");
	g_object_set (action, "sensitive", move_right, NULL);
	action = egg_action_group_get_action (action_group, "TabsDetach");
	g_object_set (action, "sensitive", detach, NULL);
}

static void
update_tabs_menu (EphyWindow *window)
{
	update_tabs_menu_sensitivity (window);
	ephy_tabs_menu_update (window->priv->tabs_menu);
}

static void
tab_added_cb (EphyNotebook *notebook, GtkWidget *child, EphyWindow *window)
{
	EphyTab *tab;

	g_return_if_fail (IS_EPHY_EMBED (child));
	tab = EPHY_TAB (g_object_get_data (G_OBJECT (child), "EphyTab"));

	window->priv->num_tabs++;

	update_tabs_menu (window);

	sync_tab_load_status (tab, NULL, window);
	g_signal_connect_object (G_OBJECT (tab), "notify::load-status",
				 G_CALLBACK (sync_tab_load_status), window, 0);
	sync_tab_visibility (tab, NULL, window);
	g_signal_connect_object (G_OBJECT (tab), "notify::visible",
				 G_CALLBACK (sync_tab_visibility), window, 0);
}

static void
tab_removed_cb (EphyNotebook *notebook, GtkWidget *child, EphyWindow *window)
{
	EphyTab *tab;

	g_return_if_fail (IS_EPHY_EMBED (child));
	tab = EPHY_TAB (g_object_get_data (G_OBJECT (child), "EphyTab"));

	g_signal_handlers_disconnect_by_func (G_OBJECT (tab),
					      G_CALLBACK (sync_tab_load_status),
					      window);	
	g_signal_handlers_disconnect_by_func (G_OBJECT (tab),
					      G_CALLBACK (sync_tab_visibility),
					      window);	

	window->priv->num_tabs--;

	if (window->priv->num_tabs == 0)
	{
		/* removed the last tab, close the window */
		gtk_widget_destroy (GTK_WIDGET (window));
	}
	else
	{
		update_tabs_menu (window);
	}
}

static void
tab_detached_cb (EphyNotebook *notebook, GtkWidget *child,
		 gpointer data)
{
	EphyWindow *window;

	g_return_if_fail (IS_EPHY_NOTEBOOK (notebook));
	g_return_if_fail (IS_EPHY_EMBED (child));

	window = ephy_window_new ();
	ephy_notebook_move_page (notebook,
				 EPHY_NOTEBOOK (ephy_window_get_notebook (window)),
				 child, 0);
	gtk_widget_show (GTK_WIDGET (window));
}

static void
tabs_reordered_cb (EphyNotebook *notebook, EphyWindow *window)
{
	update_tabs_menu (window);
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

	g_signal_connect (G_OBJECT (notebook), "tab_added",
			  G_CALLBACK (tab_added_cb), window);
	g_signal_connect (G_OBJECT (notebook), "tab_removed",
			  G_CALLBACK (tab_removed_cb), window);
	g_signal_connect (G_OBJECT (notebook), "tab_detached",
			  G_CALLBACK (tab_detached_cb), NULL);
	g_signal_connect (G_OBJECT (notebook), "tabs_reordered",
			  G_CALLBACK (tabs_reordered_cb), window);

	gtk_widget_show (GTK_WIDGET (notebook));

	return notebook;
}

static void
ephy_window_init (EphyWindow *window)
{
	Session *session;

	LOG ("EphyWindow initialising %p", window)

	session = ephy_shell_get_session (ephy_shell);

        window->priv = g_new0 (EphyWindowPrivate, 1);
	window->priv->active_tab = NULL;
	window->priv->chrome_mask = 0;
	window->priv->closing = FALSE;
	window->priv->ppview_toolbar = NULL;
	window->priv->exit_fullscreen_popup = NULL;
	window->priv->num_tabs = 0;
	window->priv->is_fullscreen = FALSE;
	window->priv->has_size = FALSE;

	/* Setup the window and connect verbs */
	setup_window (window);

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

	/* Initializ the menus */
	window->priv->tabs_menu = ephy_tabs_menu_new (window);
	window->priv->fav_menu = ephy_favorites_menu_new (window);
	window->priv->enc_menu = ephy_encoding_menu_new (window);
	window->priv->bmk_menu = ephy_bookmarks_menu_new (window);

	/* Once window is fully created, add it to the session list*/
	session_add_window (session, window);
}

static void
save_window_chrome (EphyWindow *window)
{
	EmbedChromeMask flags = window->priv->chrome_mask;

	if (!(flags & EMBED_CHROME_OPENASPOPUP) &&
	    !(flags & EMBED_CHROME_PPVIEWTOOLBARON))
	{
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_BOOKMARKS_BAR,
				       flags & EMBED_CHROME_BOOKMARKSBARON);
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
		g_signal_handlers_disconnect_by_func (window->priv->find_dialog,
						      G_CALLBACK (sync_find_dialog),
						      window);
		g_object_unref (G_OBJECT (window->priv->find_dialog));
	}

	g_object_unref (window->priv->fav_menu);
	g_object_unref (window->priv->enc_menu);
	g_object_unref (window->priv->tabs_menu);
	g_object_unref (window->priv->bmk_menu);

	if (window->priv->ppview_toolbar)
	{
		g_object_unref (window->priv->ppview_toolbar);
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
translate_default_chrome (EmbedChromeMask *chrome_mask)
{
	gboolean bbar;

	bbar = (*chrome_mask & EMBED_CHROME_BOOKMARKSBAR_DEFAULT) ||
	       (*chrome_mask & EMBED_CHROME_DEFAULT);

	/* keep only not layout flags */
	*chrome_mask &= (EMBED_CHROME_WINDOWRAISED |
			 EMBED_CHROME_WINDOWLOWERED |
			 EMBED_CHROME_CENTERSCREEN |
			 EMBED_CHROME_OPENASDIALOG |
			 EMBED_CHROME_OPENASCHROME |
			 EMBED_CHROME_OPENASPOPUP);

	/* Load defaults */
	if (eel_gconf_get_boolean (CONF_WINDOWS_SHOW_STATUSBAR))
	{
		*chrome_mask |= EMBED_CHROME_STATUSBARON;
	}
	if (eel_gconf_get_boolean (CONF_WINDOWS_SHOW_TOOLBARS))
	{
		*chrome_mask |= EMBED_CHROME_TOOLBARON;
	}
	if (eel_gconf_get_boolean (CONF_WINDOWS_SHOW_BOOKMARKS_BAR) && bbar)
	{
		*chrome_mask |= EMBED_CHROME_BOOKMARKSBARON;
	}

	*chrome_mask |= EMBED_CHROME_MENUBARON;
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

	action = egg_action_group_get_action (action_group, "ViewBookmarksBar");
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action),
				      mask & EMBED_CHROME_BOOKMARKSBARON);

	action = egg_action_group_get_action (action_group, "ViewStatusbar");
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action),
				      mask & EMBED_CHROME_STATUSBARON);
}

void
ephy_window_set_chrome (EphyWindow *window,
			EmbedChromeMask flags)
{
	if ((flags & EMBED_CHROME_DEFAULT) ||
	    (flags & EMBED_CHROME_BOOKMARKSBAR_DEFAULT))
	{
		translate_default_chrome (&flags);
	}

	update_chromes_visibility (window, flags);

	window->priv->chrome_mask = flags;

	update_layout_toggles (window);

	save_window_chrome (window);
}

GtkWidget *
ephy_window_get_notebook (EphyWindow *window)
{
	g_return_val_if_fail (IS_EPHY_WINDOW (window), NULL);

	return GTK_WIDGET (window->priv->notebook);
}

void
ephy_window_add_tab (EphyWindow *window,
		     EphyTab *tab,
		     gint position,
		     gboolean jump_to)
{
	GtkWidget *widget;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	g_return_if_fail (IS_EPHY_TAB (tab));

	widget = GTK_WIDGET(ephy_tab_get_embed (tab));

	ephy_notebook_insert_page (EPHY_NOTEBOOK (window->priv->notebook),
				   widget, position, jump_to);
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
real_get_active_tab (EphyWindow *window, int page_num)
{
	EphyTab *tab;
	GtkWidget *embed_widget;

	if (page_num == -1)
	{
		page_num = gtk_notebook_get_current_page (window->priv->notebook);
	}
	embed_widget = gtk_notebook_get_nth_page (window->priv->notebook,
						  page_num);

	g_return_val_if_fail (GTK_IS_WIDGET (embed_widget), NULL);
	tab = g_object_get_data (G_OBJECT (embed_widget), "EphyTab");
	g_return_val_if_fail (IS_EPHY_TAB (tab), NULL);

	return tab;
}

void
ephy_window_remove_tab (EphyWindow *window,
		        EphyTab *tab)
{
	GtkWidget *embed;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	g_return_if_fail (IS_EPHY_TAB (tab));

	embed = GTK_WIDGET (ephy_tab_get_embed (tab));

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

	if (!window->priv->chrome_mask)
	{
		ephy_window_set_chrome (window, EMBED_CHROME_DEFAULT);
	}

	if (!window->priv->has_size)
	{
		gboolean keep_state = TRUE;

		/* Do not keep state of sized popups */
		if (window->priv->chrome_mask & EMBED_CHROME_OPENASPOPUP)
		{
			EphyTab *tab;
			int width, height;

			tab = ephy_window_get_active_tab (EPHY_WINDOW (window));
			g_return_if_fail (tab != NULL);

			ephy_tab_get_size (tab, &width, &height);
			if (width != -1 || height != -1)
			{
				keep_state = FALSE;
			}
		}

		if (keep_state)
		{
			ephy_state_add_window (widget, "main_window", 600, 500,
					       EPHY_STATE_WINDOW_SAVE_SIZE);
		}

		window->priv->has_size = TRUE;
	}

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
update_favorites_control (EphyWindow *window)
{
	ephy_favorites_menu_update (window->priv->fav_menu);
}

void
ephy_window_update_control (EphyWindow *window,
			      ControlID control)
{
	g_return_if_fail (IS_EPHY_WINDOW (window));

	switch (control)
	{
	case FavoritesControl:
		update_favorites_control (window);
		break;
	default:
		g_warning ("unknown control specified for updating");
		break;
	}
}

EphyTab *
ephy_window_get_active_tab (EphyWindow *window)
{
	g_return_val_if_fail (IS_EPHY_WINDOW (window), NULL);
	g_return_val_if_fail (window->priv->active_tab != NULL, NULL);

	return window->priv->active_tab;
}

EphyEmbed *
ephy_window_get_active_embed (EphyWindow *window)
{
	EphyTab *tab;

	g_return_val_if_fail (IS_EPHY_WINDOW (window), NULL);

	tab = ephy_window_get_active_tab (window);

	if (tab)
	{
		return ephy_tab_get_embed (tab);
	}

	return NULL;
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

		tabs = g_list_prepend (tabs, tab);
		i++;
	}

	return g_list_reverse (tabs);
}

static void
update_embed_dialogs (EphyWindow *window,
		      EphyTab *tab)
{
	EphyEmbed *embed;
	EphyDialog *find_dialog = window->priv->find_dialog;

	embed = ephy_tab_get_embed (tab);

	if (find_dialog)
	{
		ephy_embed_dialog_set_embed
			(EPHY_EMBED_DIALOG(find_dialog),
			 embed);
	}
}

static void
ephy_window_notebook_switch_page_cb (GtkNotebook *notebook,
				     GtkNotebookPage *page,
				     guint page_num,
				     EphyWindow *window)
{
	EphyTab *tab;

	g_return_if_fail (IS_EPHY_WINDOW (window));
	if (window->priv->closing) return;

	/* get the new tab */
	tab = real_get_active_tab (window, page_num);

	/* update new tab */
	ephy_window_set_active_tab (window, tab);

	update_embed_dialogs (window, tab);

	/* update window controls */
	update_tabs_menu_sensitivity (window);
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

	sync_find_dialog (FIND_DIALOG (dialog), NULL, window);
	g_signal_connect_object (dialog, "notify::embed",
				 G_CALLBACK (sync_find_dialog), window, 0);
	g_signal_connect_object (dialog, "notify::navigation",
				 G_CALLBACK (sync_find_dialog), window, 0);

	window->priv->find_dialog = dialog;

	return dialog;
}

void
ephy_window_set_zoom (EphyWindow *window,
		      float zoom)
{
	EphyEmbed *embed;
	float current_zoom = 1.0;

        g_return_if_fail (IS_EPHY_WINDOW (window));

	embed = ephy_window_get_active_embed (window);
        g_return_if_fail (embed != NULL);

	ephy_embed_zoom_get (embed, &current_zoom);

	if (zoom == ZOOM_IN)
	{
		zoom = ephy_zoom_get_changed_zoom_level (current_zoom, 1);
	}
	else if (zoom == ZOOM_OUT)
	{
		zoom = ephy_zoom_get_changed_zoom_level (current_zoom, -1);
	}

	if (zoom != current_zoom)
	{
		ephy_embed_zoom_set (embed, zoom, TRUE);
	}
}

Toolbar *
ephy_window_get_toolbar (EphyWindow *window)
{
	return window->priv->toolbar;
}
