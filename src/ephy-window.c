/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-window.h"
#include "ephy-type-builtins.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-state.h"
#include "ppview-toolbar.h"
#include "window-commands.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-utils.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-zoom.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-statusbar.h"
#include "egg-editable-toolbar.h"
#include "ephy-toolbar.h"
#include "popup-commands.h"
#include "ephy-encoding-menu.h"
#include "ephy-tabs-menu.h"
#include "ephy-stock-icons.h"
#include "ephy-extension.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-link.h"
#include "ephy-gui.h"
#include "ephy-notebook.h"
#include "ephy-fullscreen-popup.h"
#include "ephy-action-helper.h"
#include "ephy-find-toolbar.h"
#include "ephy-embed-persist.h"
#include "ephy-location-entry.h"
#include "ephy-web-view.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>

#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

static void ephy_window_class_init		(EphyWindowClass *klass);
static void ephy_window_init			(EphyWindow *gs);
static GObject *ephy_window_constructor		(GType type,
						 guint n_construct_properties,
						 GObjectConstructParam *construct_params);
static void ephy_window_finalize		(GObject *object);
static void ephy_window_show			(GtkWidget *widget);
static EphyEmbed *ephy_window_open_link		(EphyLink *link,
						 const char *address,
						 EphyEmbed *embed,
						 EphyLinkFlags flags);
static void notebook_switch_page_cb		(GtkNotebook *notebook,
						 GtkNotebookPage *page,
						 guint page_num,
						 EphyWindow *window);
static void ephy_window_view_statusbar_cb       (GtkAction *action,
						 EphyWindow *window);
static void ephy_window_view_toolbar_cb         (GtkAction *action,
						 EphyWindow *window);
static void ephy_window_view_popup_windows_cb	(GtkAction *action,
						 EphyWindow *window);
static void sync_tab_load_status		(EphyWebView *view,
						 GParamSpec *pspec,
						 EphyWindow *window);
static void sync_tab_security			(EphyWebView  *view,
						 GParamSpec *pspec,
						 EphyWindow *window);
static void sync_tab_zoom			(WebKitWebView *web_view,
						 GParamSpec *pspec,
						 EphyWindow *window);


static const GtkActionEntry ephy_menu_entries [] = {

	/* Toplevel */

	{ "File", NULL, N_("_File") },
	{ "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
	{ "Bookmarks", NULL, N_("_Bookmarks") },
	{ "Go", NULL, N_("_Go") },
	{ "Tools", NULL, N_("T_ools") },
	{ "Tabs", NULL, N_("_Tabs") },
	{ "Help", NULL, N_("_Help") },
	{ "Toolbar", NULL, N_("_Toolbars") },
	{ "PopupAction", NULL, "" },
	{ "NotebookPopupAction", NULL, "" },

	/* File menu */

	{ "FileOpen", GTK_STOCK_OPEN, N_("_Open…"), "<control>O",
	  N_("Open a file"),
	  G_CALLBACK (window_cmd_file_open) },
	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("Save _As…"), "<shift><control>S",
	  N_("Save the current page"),
	  G_CALLBACK (window_cmd_file_save_as) },
	{ "FilePrintSetup", STOCK_PRINT_SETUP, N_("Page Set_up"), NULL,
	  N_("Setup the page settings for printing"),
	  G_CALLBACK (window_cmd_file_print_setup) },
	{ "FilePrintPreview", GTK_STOCK_PRINT_PREVIEW, N_("Print Pre_view"),"<control><shift>P",
	  N_("Print preview"),
	  G_CALLBACK (window_cmd_file_print_preview) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("_Print…"), "<control>P",
	  N_("Print the current page"),
	  G_CALLBACK (window_cmd_file_print) },
	{ "FileSendTo", STOCK_SEND_MAIL, N_("S_end Link by Email…"), NULL,
	  N_("Send a link of the current page"),
	  G_CALLBACK (window_cmd_file_send_to) },
	{ "FileCloseTab", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close this tab"),
	  G_CALLBACK (window_cmd_file_close_window) },

	/* Edit menu */

	{ "EditUndo", GTK_STOCK_UNDO, N_("_Undo"), "<control>Z",
	  N_("Undo the last action"),
	  G_CALLBACK (window_cmd_edit_undo) },
	{ "EditRedo", GTK_STOCK_REDO, N_("Re_do"), "<shift><control>Z",
	  N_("Redo the last undone action"),
	  G_CALLBACK (window_cmd_edit_redo) },
	{ "EditCut", GTK_STOCK_CUT, N_("Cu_t"), "<control>X",
	  N_("Cut the selection"),
	  G_CALLBACK (window_cmd_edit_cut) },
	{ "EditCopy", GTK_STOCK_COPY, N_("_Copy"), "<control>C",
	  N_("Copy the selection"),
	  G_CALLBACK (window_cmd_edit_copy) },
	{ "EditPaste", GTK_STOCK_PASTE, N_("_Paste"), "<control>V",
	  N_("Paste clipboard"),
	  G_CALLBACK (window_cmd_edit_paste) },
	{ "EditDelete", GTK_STOCK_DELETE, NULL, NULL,
	  N_("Delete text"),
	  G_CALLBACK (window_cmd_edit_delete) },
	{ "EditSelectAll", GTK_STOCK_SELECT_ALL, N_("Select _All"), "<control>A",
	  N_("Select the entire page"),
	  G_CALLBACK (window_cmd_edit_select_all) },
	{ "EditFind", GTK_STOCK_FIND, N_("_Find…"), "<control>F",
	  N_("Find a word or phrase in the page"),
	  G_CALLBACK (window_cmd_edit_find) },
	{ "EditFindNext", NULL, N_("Find Ne_xt"), "<control>G",
	  N_("Find next occurrence of the word or phrase"),
	  G_CALLBACK (window_cmd_edit_find_next) },
	{ "EditFindPrev", NULL, N_("Find Pre_vious"), "<shift><control>G",
	  N_("Find previous occurrence of the word or phrase"),
	  G_CALLBACK (window_cmd_edit_find_prev) },
	{ "EditPersonalData", NULL, N_("P_ersonal Data"), NULL,
	  N_("View and remove cookies and passwords"),
	  G_CALLBACK (window_cmd_edit_personal_data) },
#if 0
	{ "EditCertificates", NULL, N_("Certificate_s"), NULL,
	  N_("Manage Certificates"),
	  G_CALLBACK (window_cmd_edit_certificates) },
#endif
	{ "EditPrefs", GTK_STOCK_PREFERENCES, N_("P_references"), NULL,
	  N_("Configure the web browser"),
	  G_CALLBACK (window_cmd_edit_prefs) },

	/* View menu */

	{ "ViewToolbarEditor", NULL, N_("_Customize Toolbars…"), NULL,
	  N_("Customize toolbars"),
	  G_CALLBACK (window_cmd_edit_toolbar) },
	{ "ViewStop", GTK_STOCK_STOP, N_("_Stop"), "Escape",
	  N_("Stop current data transfer"),
	  G_CALLBACK (window_cmd_view_stop) },
	{ "ViewAlwaysStop", GTK_STOCK_STOP, N_("_Stop"), "Escape",
	  NULL, G_CALLBACK (window_cmd_view_stop) },
	{ "ViewReload", GTK_STOCK_REFRESH, N_("_Reload"), "<control>R",
	  N_("Display the latest content of the current page"),
	  G_CALLBACK (window_cmd_view_reload) },
	{ "ViewZoomIn", GTK_STOCK_ZOOM_IN, N_("_Larger Text"), "<control>plus",
	  N_("Increase the text size"),
	  G_CALLBACK (window_cmd_view_zoom_in) },
	{ "ViewZoomOut", GTK_STOCK_ZOOM_OUT, N_("S_maller Text"), "<control>minus",
	  N_("Decrease the text size"),
	  G_CALLBACK (window_cmd_view_zoom_out) },
	{ "ViewZoomNormal", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<control>0",
	  N_("Use the normal text size"),
	  G_CALLBACK (window_cmd_view_zoom_normal) },
	{ "ViewEncoding", NULL, N_("Text _Encoding"), NULL,
	  N_("Change the text encoding"),
	  NULL },
	{ "ViewPageSource", NULL, N_("_Page Source"), "<control>U",
	  N_("View the source code of the page"),
	  G_CALLBACK (window_cmd_view_page_source) },
        { "ViewPageSecurityInfo", NULL, N_("Page _Security Information"), NULL,
          N_("Display security information for the web page"),
          G_CALLBACK (window_cmd_view_page_security_info) },

	/* Bookmarks menu */

	{ "FileBookmarkPage", STOCK_ADD_BOOKMARK, N_("_Add Bookmark…"), "<control>D",
	  N_("Add a bookmark for the current page"),
	  G_CALLBACK (window_cmd_file_bookmark_page) },
	{ "GoBookmarks", EPHY_STOCK_BOOKMARKS, N_("_Edit Bookmarks"), "<control>B",
	  N_("Open the bookmarks window"),
	  G_CALLBACK (window_cmd_go_bookmarks) },

	/* Go menu */

	{ "GoLocation", NULL, N_("_Location…"), "<control>L",
	  N_("Go to a specified location"),
	  G_CALLBACK (window_cmd_go_location) },
	{ "GoHistory", EPHY_STOCK_HISTORY, N_("Hi_story"), "<control>H",
	  N_("Open the history window"),
	  G_CALLBACK (window_cmd_go_history) },

	/* Tabs menu */

	{ "TabsPrevious", NULL, N_("_Previous Tab"), "<control>Page_Up",
	  N_("Activate previous tab"),
	  G_CALLBACK (window_cmd_tabs_previous) },
	{ "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down",
	  N_("Activate next tab"),
	  G_CALLBACK (window_cmd_tabs_next) },
	{ "TabsMoveLeft", NULL, N_("Move Tab _Left"), "<shift><control>Page_Up",
	  N_("Move current tab to left"),
	  G_CALLBACK (window_cmd_tabs_move_left) },
	{ "TabsMoveRight", NULL, N_("Move Tab _Right"), "<shift><control>Page_Down",
	  N_("Move current tab to right"),
	  G_CALLBACK (window_cmd_tabs_move_right) },
        { "TabsDetach", NULL, N_("_Detach Tab"), NULL,
          N_("Detach current tab"),
          G_CALLBACK (window_cmd_tabs_detach) },

	/* Help menu */

	{"HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
	 N_("Display web browser help"),
	 G_CALLBACK (window_cmd_help_contents) },
	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  N_("Display credits for the web browser creators"),
	  G_CALLBACK (window_cmd_help_about) },
};

static const GtkToggleActionEntry ephy_menu_toggle_entries [] =
{
	/* File Menu */

	{ "FileWorkOffline", NULL, N_("_Work Offline"), NULL,
	  N_("Switch to offline mode"),
	  G_CALLBACK (window_cmd_file_work_offline), FALSE },

	/* View Menu */

	{ "ViewToolbar", NULL, N_("_Hide Toolbars"), NULL,
	  N_("Show or hide toolbar"),
	  G_CALLBACK (ephy_window_view_toolbar_cb), FALSE },
	{ "ViewStatusbar", NULL, N_("St_atusbar"), NULL,
	  N_("Show or hide statusbar"),
	  G_CALLBACK (ephy_window_view_statusbar_cb), TRUE },
	{ "ViewFullscreen", GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), "F11",
	  N_("Browse at full screen"),
	  G_CALLBACK (window_cmd_view_fullscreen), FALSE },
	{ "ViewPopupWindows", EPHY_STOCK_POPUPS, N_("Popup _Windows"), NULL,
	  N_("Show or hide unrequested popup windows from this site"),
	  G_CALLBACK (ephy_window_view_popup_windows_cb), FALSE },
	{ "BrowseWithCaret", NULL, N_("Selection Caret"), "F7",
	  "",
	  G_CALLBACK (window_cmd_browse_with_caret), FALSE }
};

static const GtkActionEntry ephy_popups_entries [] = {
        /* Document */

	{ "ContextBookmarkPage", STOCK_ADD_BOOKMARK, N_("Add Boo_kmark…"), "<control>D",
	  N_("Add a bookmark for the current page"),
	  G_CALLBACK (window_cmd_file_bookmark_page) },
	
	/* Framed document */

	{ "OpenFrame", NULL, N_("Show Only _This Frame"), NULL,
	  N_("Show only this frame in this window"),
	  G_CALLBACK (popup_cmd_open_frame) },

	/* Links */

	{ "OpenLink", GTK_STOCK_JUMP_TO, N_("_Open Link"), NULL,
	  N_("Open link in this window"),
	  G_CALLBACK (popup_cmd_open_link) },
	{ "OpenLinkInNewWindow", NULL, N_("Open Link in New _Window"), NULL,
	  N_("Open link in a new window"),
	  G_CALLBACK (popup_cmd_link_in_new_window) },
	{ "OpenLinkInNewTab", NULL, N_("Open Link in New _Tab"), NULL,
	  N_("Open link in a new tab"),
	  G_CALLBACK (popup_cmd_link_in_new_tab) },
	{ "DownloadLink", NULL, N_("_Download Link"), NULL,
	  NULL, G_CALLBACK (popup_cmd_download_link) },
	{ "DownloadLinkAs", GTK_STOCK_SAVE_AS, N_("_Save Link As…"), NULL,
	  N_("Save link with a different name"),
	  G_CALLBACK (popup_cmd_download_link_as) },
	{ "BookmarkLink", STOCK_ADD_BOOKMARK, N_("_Bookmark Link…"),
	  NULL, NULL, G_CALLBACK (popup_cmd_bookmark_link) },
	{ "CopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_address) },

	/* Email links */

	/* This is on the context menu on a mailto: link and opens the mail program */
	{ "SendEmail", STOCK_NEW_MAIL, N_("_Send Email…"),
	  NULL, NULL, G_CALLBACK (popup_cmd_open_link) },
	{ "CopyEmailAddress", NULL, N_("_Copy Email Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_address) },

	/* Images */

	{ "OpenImage", NULL, N_("Open _Image"), NULL,
	  NULL, G_CALLBACK (popup_cmd_open_image) },
	{ "SaveImageAs", NULL, N_("_Save Image As…"), NULL,
	  NULL, G_CALLBACK (popup_cmd_save_image_as) },
	{ "SetImageAsBackground", NULL, N_("_Use Image As Background"), NULL,
	  NULL, G_CALLBACK (popup_cmd_set_image_as_background) },
	{ "CopyImageLocation", NULL, N_("Copy I_mage Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_image_location) },
	{ "StartImageAnimation", NULL, N_("St_art Animation"), NULL,
	  NULL, NULL },
	{ "StopImageAnimation", NULL, N_("St_op Animation"), NULL,
	  NULL, NULL },
};

static const struct
{
	guint keyval;
	GdkModifierType modifier;
	const gchar *action;
	gboolean fromToolbar;
} extra_keybindings [] = {
	{ GDK_s,		GDK_CONTROL_MASK,	"FileSaveAs",		FALSE },
	{ GDK_R,		GDK_CONTROL_MASK |
				GDK_SHIFT_MASK,		"ViewReload",		FALSE },
	/* Support all the MSIE tricks as well ;) */
	{ GDK_F5,		0,			"ViewReload",		FALSE },
	{ GDK_F5,		GDK_CONTROL_MASK,	"ViewReload",		FALSE },
	{ GDK_F5,		GDK_SHIFT_MASK,		"ViewReload",		FALSE },
	{ GDK_F5,		GDK_CONTROL_MASK |
				GDK_SHIFT_MASK,		"ViewReload",		FALSE },
	{ GDK_KP_Add,		GDK_CONTROL_MASK,	"ViewZoomIn",		FALSE },
	{ GDK_KP_Subtract,	GDK_CONTROL_MASK,	"ViewZoomOut",		FALSE },
	{ GDK_equal,		GDK_CONTROL_MASK,	"ViewZoomIn",		FALSE },
	{ GDK_KP_0,		GDK_CONTROL_MASK,	"ViewZoomNormal",	FALSE },
	/* These keys are a bit strange: when pressed with no modifiers, they emit
	 * KP_PageUp/Down Control; when pressed with Control+Shift they are KP_9/3,
	 * when NumLock is on they are KP_9/3 and with NumLock and Control+Shift
	 * They're KP_PageUp/Down again!
	 */
	{ GDK_KP_Left,		GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KP_4,		GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KP_Right,		GDK_MOD1_MASK /*Alt*/,	"NavigationForward",	TRUE },
	{ GDK_KP_6,		GDK_MOD1_MASK /*Alt*/,	"NavigationForward",	TRUE },
	{ GDK_KP_Up,		GDK_MOD1_MASK /*Alt*/,	"NavigationUp",		TRUE },
	{ GDK_KP_8,		GDK_MOD1_MASK /*Alt*/,	"NavigationUp",		TRUE },
	{ GDK_KP_Page_Up,	GDK_CONTROL_MASK,	"TabsPrevious",		FALSE },
	{ GDK_KP_9,		GDK_CONTROL_MASK,	"TabsPrevious",		FALSE },
	{ GDK_KP_Page_Down,	GDK_CONTROL_MASK,	"TabsNext",		FALSE },
	{ GDK_KP_3,		GDK_CONTROL_MASK,	"TabsNext",		FALSE },
	{ GDK_KP_Page_Up,	GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveLeft",		FALSE },
	{ GDK_KP_9,		GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveLeft",		FALSE },
	{ GDK_KP_Page_Down,	GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveRight",	FALSE },
	{ GDK_KP_3,		GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveRight",	FALSE },
#ifdef HAVE_X11_XF86KEYSYM_H
	{ XF86XK_Back,		0,			"NavigationBack",	TRUE  },
	{ XF86XK_Favorites,	0,			"GoBookmarks",		FALSE },
	{ XF86XK_Forward,	0,			"NavigationForward",	TRUE  },
	{ XF86XK_Go,	 	0,			"GoLocation",		FALSE },
	{ XF86XK_History, 	0,			"GoHistory",		FALSE },
	{ XF86XK_HomePage,	0,			"GoHome",		TRUE  },
	{ XF86XK_OpenURL, 	0,			"GoLocation",		FALSE },
	{ XF86XK_AddFavorite, 	0,			"FileBookmarkPage",	FALSE },
	{ XF86XK_Refresh, 	0,			"ViewReload",		FALSE },
	{ XF86XK_Reload,	0, 			"ViewReload",		FALSE },
	{ XF86XK_Search,	0,			"EditFind",		FALSE },
	{ XF86XK_Send,	 	0,			"FileSendTo",		FALSE },
	{ XF86XK_Start,		0,			"GoHome",		TRUE  },
	{ XF86XK_Stop,		0,			"ViewStop",		FALSE },
	{ XF86XK_ZoomIn,	0, 			"ViewZoomIn",		FALSE },
	{ XF86XK_ZoomOut,	0, 			"ViewZoomOut",		FALSE }
	/* FIXME: what about ScrollUp, ScrollDown, Menu*, Option, LogOff, Save,.. any others? */
#endif /* HAVE_X11_XF86KEYSYM_H */
};

#define CONF_LOCKDOWN_HIDE_MENUBAR "/apps/epiphany/lockdown/hide_menubar"

#define BOOKMARKS_MENU_PATH "/menubar/BookmarksMenu"

#define SETTINGS_CONNECTION_DATA_KEY	"EphyWindowSettings"

/* Until https://bugzilla.mozilla.org/show_bug.cgi?id=296002 is fixed */

#define EPHY_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_WINDOW, EphyWindowPrivate))

struct _EphyWindowPrivate
{
	GtkWidget *main_vbox;
	GtkWidget *menu_dock;
	GtkWidget *fullscreen_popup;
	EphyToolbar *toolbar;
	GtkWidget *statusbar;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GtkActionGroup *popups_action_group;
	EphyEncodingMenu *enc_menu;
	EphyTabsMenu *tabs_menu;
	PPViewToolbar *ppview_toolbar;
	GtkNotebook *notebook;
	EphyEmbed *active_embed;
	EphyFindToolbar *find_toolbar;
	guint num_tabs;
	guint tab_message_cid;
	guint help_message_cid;
	EphyWebViewChrome chrome;
	guint idle_resize_handler;
	GHashTable *tabs_to_remove;
	EphyEmbedEvent *context_event;
	guint idle_worker;
	GtkWidget *entry;

	guint browse_with_caret_notifier_id;
	guint allow_popups_notifier_id;
	guint clear_progress_timeout_id;

	guint menubar_accel_keyval;
	guint menubar_accel_modifier;

	guint closing : 1;
	guint has_size : 1;
	guint fullscreen_mode : 1;
	guint ppv_mode : 1;
	guint should_save_chrome : 1;
	guint is_popup : 1;
	guint present_on_insert : 1;
	guint key_theme_is_emacs : 1;
};

enum
{
	PROP_0,
	PROP_ACTIVE_CHILD,
	PROP_CHROME,
	PROP_PPV_MODE,
	PROP_SINGLE_TAB_MODE
};

/* Make sure not to overlap with those in ephy-lockdown.c */
enum
{
	SENS_FLAG_CHROME	= 1 << 0,
	SENS_FLAG_CONTEXT	= 1 << 1,
	SENS_FLAG_DOCUMENT	= 1 << 2,
	SENS_FLAG_LOADING	= 1 << 3,
	SENS_FLAG_NAVIGATION	= 1 << 4
};

static gint
impl_add_child (EphyEmbedContainer *container,
		EphyEmbed *child,
		gint position,
		gboolean jump_to)
{
	EphyWindow *window = EPHY_WINDOW (container);

	g_return_val_if_fail (!window->priv->is_popup ||
			      gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook)) < 1, -1);

	return ephy_notebook_add_tab (EPHY_NOTEBOOK (window->priv->notebook),
				      child, position, jump_to);
}

static void
impl_set_active_child (EphyEmbedContainer *container,
		       EphyEmbed *child)
{
	int page;
	EphyWindow *window;

	window = EPHY_WINDOW (container);

	page = gtk_notebook_page_num
		(window->priv->notebook, GTK_WIDGET (child));
	gtk_notebook_set_current_page
		(window->priv->notebook, page);
}

static GtkWidget *
construct_confirm_close_dialog (EphyWindow *window)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new
		(GTK_WINDOW (window),
		 GTK_DIALOG_MODAL,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_CANCEL,
		 _("There are unsubmitted changes to form elements"));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("If you close the document anyway, "
		   "you will lose that information."));
	
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("Close _Document"), GTK_RESPONSE_ACCEPT);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	/* FIXME gtk_window_set_title (GTK_WINDOW (dialog), _("Close Document?")); */
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (window)),
				     GTK_WINDOW (dialog));

	return dialog;
}

static gboolean
confirm_close_with_modified_forms (EphyWindow *window)
{
	if (eel_gconf_get_boolean (CONF_WARN_ON_CLOSE_UNSUBMITTED_DATA))
	{
		GtkWidget *dialog;
		int response;

		dialog = construct_confirm_close_dialog (window);
		response = gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy (dialog);

		return response == GTK_RESPONSE_ACCEPT;
	}
	
	return TRUE;
}

static void
impl_remove_child (EphyEmbedContainer *container,
		   EphyEmbed *child)
{
	EphyWindow *window;
	EphyWindowPrivate *priv;
	GtkNotebook *notebook;
	gboolean modified;
	int position;

	window = EPHY_WINDOW (container);
	priv = window->priv;

	modified = ephy_web_view_has_modified_forms (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (child));
	if (modified && confirm_close_with_modified_forms (window) == FALSE)
	{
		/* don't close the tab */
		return;
	}

	notebook = GTK_NOTEBOOK (priv->notebook);
	position = gtk_notebook_page_num (notebook, GTK_WIDGET (child));
	gtk_notebook_remove_page (notebook, position);
}

static EphyEmbed *
impl_get_active_child (EphyEmbedContainer *container)
{
	return EPHY_WINDOW (container)->priv->active_embed;
}

static GList *
impl_get_children (EphyEmbedContainer *container)
{
	EphyWindow *window = EPHY_WINDOW (container);

	return gtk_container_get_children (GTK_CONTAINER (window->priv->notebook));
}

static gboolean
impl_get_is_popup (EphyEmbedContainer *container)
{
	return EPHY_WINDOW (container)->priv->is_popup;
}

static EphyWebViewChrome
impl_get_chrome (EphyEmbedContainer *container)
{
	return EPHY_WINDOW (container)->priv->chrome;
}

static void
ephy_window_embed_container_iface_init (EphyEmbedContainerIface *iface)
{
	iface->add_child = impl_add_child;
	iface->set_active_child = impl_set_active_child;
	iface->remove_child = impl_remove_child;
	iface->get_active_child = impl_get_active_child;
	iface->get_children = impl_get_children;
	iface->get_is_popup = impl_get_is_popup;
	iface->get_chrome = impl_get_chrome;
}

static void
ephy_window_link_iface_init (EphyLinkIface *iface)
{
	iface->open_link = ephy_window_open_link;
}

G_DEFINE_TYPE_WITH_CODE (EphyWindow, ephy_window, GTK_TYPE_WINDOW,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
						ephy_window_link_iface_init)
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_CONTAINER,
						ephy_window_embed_container_iface_init))

/* FIXME: fix this! */
static void
ephy_tab_get_size (EphyEmbed *embed,
                   int *width,
                   int *height)
{
        *width = -1;
        *height = -1;
}

static void
settings_change_notify (GtkSettings *settings,
			EphyWindow  *window)
{
	EphyWindowPrivate *priv = window->priv;
	char *key_theme_name, *menubar_accel_accel;

	g_object_get (settings,
		      "gtk-key-theme-name", &key_theme_name,
		      "gtk-menu-bar-accel", &menubar_accel_accel,
		      NULL);

	g_return_if_fail (menubar_accel_accel != NULL);

	if (menubar_accel_accel != NULL && menubar_accel_accel[0] != '\0')
	{
		gtk_accelerator_parse (menubar_accel_accel,
				       &priv->menubar_accel_keyval,
				       &priv->menubar_accel_modifier);
		if (priv->menubar_accel_keyval == 0)
		{
			g_warning ("Failed to parse menu bar accelerator '%s'\n",
				   menubar_accel_accel);
		}
	}
	else
	{
		priv->menubar_accel_keyval = 0;
		priv->menubar_accel_modifier = 0;
	}

	priv->key_theme_is_emacs =
		key_theme_name &&
		g_ascii_strcasecmp (key_theme_name, "Emacs") == 0;

	g_free (key_theme_name);
	g_free (menubar_accel_accel);
}

static void
settings_changed_cb (GtkSettings *settings)
{
	GList *list, *l;

	/* FIXME: multi-head */
	list = gtk_window_list_toplevels ();

	for (l = list; l != NULL; l = l->next)
	{
		if (EPHY_IS_WINDOW (l->data))
		{
			settings_change_notify (settings, l->data);
		}
	}

	g_list_free (list);
}

static void
destroy_fullscreen_popup (EphyWindow *window)
{
	if (window->priv->fullscreen_popup != NULL)
	{
		gtk_widget_destroy (window->priv->fullscreen_popup);
		window->priv->fullscreen_popup = NULL;
	}
}

static void
add_widget (GtkUIManager *manager,
	    GtkWidget *widget,
	    EphyWindow *window)
{
	gtk_box_pack_start (GTK_BOX (window->priv->menu_dock),
			    widget, FALSE, FALSE, 0);
}

static void
exit_fullscreen_clicked_cb (EphyWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewFullscreen");
	g_return_if_fail (action != NULL);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
}

static gboolean
get_toolbar_visibility (EphyWindow *window)
{
	return ((window->priv->chrome & EPHY_WEB_VIEW_CHROME_TOOLBAR) != 0) &&
	       !window->priv->ppv_mode;
}			

static void
get_chromes_visibility (EphyWindow *window,
			gboolean *show_menubar,
			gboolean *show_statusbar,
			gboolean *show_toolbar,
			gboolean *show_tabsbar)
{
	EphyWindowPrivate *priv = window->priv;
	EphyWebViewChrome flags = priv->chrome;

	if (window->priv->ppv_mode)
	{
		*show_menubar = *show_statusbar
			      = *show_toolbar
			      = *show_tabsbar
			      = FALSE;
	}
	else if (window->priv->fullscreen_mode)
	{
		*show_toolbar = (flags & EPHY_WEB_VIEW_CHROME_TOOLBAR) != 0;
		*show_menubar = *show_statusbar = FALSE;
		*show_tabsbar = !priv->is_popup;
	}
	else
	{
		*show_menubar = (flags & EPHY_WEB_VIEW_CHROME_MENUBAR) != 0;
		*show_statusbar = (flags & EPHY_WEB_VIEW_CHROME_STATUSBAR) != 0;
		*show_toolbar = (flags & EPHY_WEB_VIEW_CHROME_TOOLBAR) != 0;
		*show_tabsbar = !priv->is_popup;
	}
}

static void
sync_chromes_visibility (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkWidget *menubar;
	GtkAction *action;
	gboolean show_statusbar, show_menubar, show_toolbar, show_tabsbar;

	if (priv->closing) return;

	get_chromes_visibility (window, &show_menubar,
				&show_statusbar, &show_toolbar,
				&show_tabsbar);

	menubar = gtk_ui_manager_get_widget (window->priv->manager, "/menubar");
	g_assert (menubar != NULL);

	g_object_set (menubar, "visible", show_menubar, NULL);
	g_object_set (priv->toolbar, "visible", show_toolbar, NULL);
	g_object_set (priv->statusbar, "visible", show_statusbar, NULL);

	ephy_notebook_set_show_tabs (EPHY_NOTEBOOK (priv->notebook), show_tabsbar);

	action = gtk_action_group_get_action (priv->action_group, "ViewToolbarEditor");

	if (priv->fullscreen_popup != NULL)
	{
		g_object_set (priv->fullscreen_popup, "visible", !show_toolbar, NULL);

		ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, TRUE);
	}
	else
	{
		ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, !show_toolbar);
	}
}

static void
ephy_window_fullscreen (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkWidget *popup;
	EphyEmbed *embed;
	GtkAction *action;
	gboolean lockdown_fs;

	priv->fullscreen_mode = TRUE;

	lockdown_fs = eel_gconf_get_boolean (CONF_LOCKDOWN_FULLSCREEN);

	popup = ephy_fullscreen_popup_new (window);
	ephy_fullscreen_popup_set_show_leave
		(EPHY_FULLSCREEN_POPUP (popup), !lockdown_fs);
	priv->fullscreen_popup = popup;
	g_signal_connect_swapped (popup, "exit-clicked",
				  G_CALLBACK (exit_fullscreen_clicked_cb), window);

	action = gtk_action_group_get_action (priv->action_group, "ViewPageSecurityInfo");
	g_signal_connect_swapped (popup, "lock-clicked",
				  G_CALLBACK (gtk_action_activate), action);

	/* sync status */
	embed = window->priv->active_embed;
	sync_tab_load_status (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), NULL, window);
	sync_tab_security (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), NULL, window);

	egg_editable_toolbar_set_model
		(EGG_EDITABLE_TOOLBAR (priv->toolbar),
		 EGG_TOOLBARS_MODEL (
		 	ephy_shell_get_toolbars_model (ephy_shell, TRUE)));

	ephy_toolbar_set_show_leave_fullscreen (priv->toolbar,
						!lockdown_fs);
	
	sync_chromes_visibility (window);
}

static void
ephy_window_unfullscreen (EphyWindow *window)
{
	window->priv->fullscreen_mode = FALSE;

	destroy_fullscreen_popup (window);

	ephy_toolbar_set_show_leave_fullscreen (window->priv->toolbar, FALSE);

	egg_editable_toolbar_set_model
		(EGG_EDITABLE_TOOLBAR (window->priv->toolbar),
		 EGG_TOOLBARS_MODEL (
		 	ephy_shell_get_toolbars_model (ephy_shell, FALSE)));

	sync_chromes_visibility (window);
}

static void
menubar_deactivate_cb (GtkWidget *menubar,
		       EphyWindow *window)
{
	g_signal_handlers_disconnect_by_func
		(menubar, G_CALLBACK (menubar_deactivate_cb), window);

	gtk_menu_shell_deselect (GTK_MENU_SHELL (menubar));

	sync_chromes_visibility (window);
}

static gboolean 
scroll_event_cb (GtkWidget *widget,
		 GdkEventScroll *event,
		 EphyWindow *window)
{
	guint modifier = event->state & gtk_accelerator_get_default_mod_mask ();

	if (modifier != GDK_CONTROL_MASK)
		return FALSE;

	if (event->direction == GDK_SCROLL_UP)
		ephy_window_set_zoom (window, ZOOM_IN);
	else if (event->direction == GDK_SCROLL_DOWN)
		ephy_window_set_zoom (window, ZOOM_OUT);

	return TRUE;
}

static gboolean 
ephy_window_key_press_event (GtkWidget *widget,
			     GdkEventKey *event)
{
	EphyWindow *window = EPHY_WINDOW (widget);
	EphyWindowPrivate *priv = window->priv;
	GtkWidget *menubar, *focus_widget;
	gboolean shortcircuit = FALSE, force_chain = FALSE, handled = FALSE;
	guint modifier = event->state & gtk_accelerator_get_default_mod_mask ();
	guint i;

	/* In an attempt to get the mozembed playing nice with things like emacs keybindings
	 * we are passing important events to the focused child widget before letting the window's
	 * base handler see them. This is *completely against* stated gtk2 policy but the 
	 * 'correct' behaviour is exceptionally useless. We need to keep an eye out for 
	 * unexpected consequences of this decision. IME's should be a high concern, but 
	 * considering that the IME folks complained about the upside-down event propagation
	 * rules, we might be doing them a favour.
	 *
	 * We achieve this by first evaluating the event to see if it's important, and if
	 * so, we get the focus widget and attempt to get the widget to handle that event.
	 * If the widget does handle it, we're done (unless force_chain is true, in which
	 * case the event is handled as normal in addition to being sent to the focus
	 * widget), otherwise the event follows the normal handling path.
	 */

	if (event->keyval == GDK_Escape && modifier == 0)
	{
		/* Always pass Escape to both the widget, and the parent */
		shortcircuit = TRUE;
		force_chain = TRUE;
	}
	else if (priv->key_theme_is_emacs && 
		 (modifier == GDK_CONTROL_MASK) &&
		 event->length > 0 &&
		 /* But don't pass Ctrl+Enter twice */
		 event->keyval != GDK_Return &&
		 event->keyval != GDK_KP_Enter &&
		 event->keyval != GDK_ISO_Enter)
	{
		/* Pass CTRL+letter characters to the widget */
		shortcircuit = TRUE;
	}

	if (shortcircuit)
	{
		focus_widget = gtk_window_get_focus (GTK_WINDOW (window));

		if (GTK_IS_WIDGET (focus_widget))
		{
			handled = gtk_widget_event (focus_widget,
						    (GdkEvent*) event);
		}

		if (handled && !force_chain)
		{
			return handled;
		}
	}

	/* Handle accelerators that we want bound, but aren't associated with
	 * an action */
	for (i = 0; i < G_N_ELEMENTS (extra_keybindings); i++)
	{
		if (event->keyval == extra_keybindings[i].keyval &&
		    modifier == extra_keybindings[i].modifier)
		{
			GtkAction * action = gtk_action_group_get_action
				(extra_keybindings[i].fromToolbar ? 
					ephy_toolbar_get_action_group (priv->toolbar) :
					priv->action_group,
				extra_keybindings[i].action);
			if (gtk_action_is_sensitive (action))
			{
				gtk_action_activate (action);
				return TRUE;
			}
			break;
		}
	}

	/* Don't activate menubar in ppv mode, or in lockdown mode */
	if (priv->ppv_mode || eel_gconf_get_boolean (CONF_LOCKDOWN_HIDE_MENUBAR))
	{
		return GTK_WIDGET_CLASS (ephy_window_parent_class)->key_press_event (widget, event);
	}

	/* Show and activate the menubar, if it isn't visible */
	if (priv->menubar_accel_keyval != 0 &&
	    event->keyval == priv->menubar_accel_keyval &&
            modifier == priv->menubar_accel_modifier)
	{
		menubar = gtk_ui_manager_get_widget (window->priv->manager, "/menubar");
		g_return_val_if_fail (menubar != NULL , FALSE);

		if (!gtk_widget_get_visible (menubar))
		{
			g_signal_connect (menubar, "deactivate",
					  G_CALLBACK (menubar_deactivate_cb), window);

			gtk_widget_show (menubar);
			gtk_menu_shell_select_first (GTK_MENU_SHELL (menubar), FALSE);

			return TRUE;
		}
	}

	return GTK_WIDGET_CLASS (ephy_window_parent_class)->key_press_event (widget, event);
}

void
_ephy_window_set_print_preview (EphyWindow *window,
				gboolean enabled)
{
	EphyWindowPrivate *priv = window->priv;
	GtkAccelGroup *accel_group;

	accel_group = gtk_ui_manager_get_accel_group (window->priv->manager);

	if (priv->ppv_mode == enabled) return;

	priv->ppv_mode = enabled;

	sync_chromes_visibility (window);

	if (enabled)
	{
		g_return_if_fail (priv->ppview_toolbar == NULL);

		ephy_find_toolbar_request_close (priv->find_toolbar);

		priv->ppview_toolbar = ppview_toolbar_new (window);
		gtk_window_remove_accel_group (GTK_WINDOW (window), accel_group);
	}
	else
	{
		g_return_if_fail (priv->ppview_toolbar != NULL);

		g_object_unref (priv->ppview_toolbar);
		priv->ppview_toolbar = NULL;
		gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	}

	g_object_notify (G_OBJECT (window), "is-print-preview");
}

static gboolean
ephy_window_delete_event (GtkWidget *widget,
			  GdkEventAny *event)
{
	EphyWindow *window = EPHY_WINDOW (widget);
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *modified_embed = NULL;
	GList *tabs, *l;
	gboolean modified = FALSE;

	/* We ignore the delete_event if the disable_quit lockdown has been set
	 */
	if (eel_gconf_get_boolean("/apps/epiphany/lockdown/disable_quit")) return TRUE;

	/* Workaround a crash when closing a window while in print preview mode. See
	 * mozilla bug #241809. /
	 * Go back to normal mode instead of closing, see bug #326136.
	 */
	if (priv->ppv_mode)
	{
		EphyEmbed *embed;

		embed = window->priv->active_embed;
		ephy_web_view_set_print_preview_mode (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), FALSE);

		_ephy_window_set_print_preview (window, FALSE);

		return TRUE;
	}

	tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyEmbed *embed = (EphyEmbed *) l->data;

		g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);

		if (ephy_web_view_has_modified_forms (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed)))
		{
			modified = TRUE;
			modified_embed = embed;
			break;
		}
	}
	g_list_free (tabs);

	if (modified)
	{
		/* jump to the first tab with modified forms */
		impl_set_active_child (EPHY_EMBED_CONTAINER (window),
				       modified_embed);

		if (confirm_close_with_modified_forms (window) == FALSE)
		{
			/* stop window close */
			return TRUE;
		}
	}
	
	/* See bug #114689 */
	gtk_widget_hide (widget);

	/* proceed with window close */
	if (GTK_WIDGET_CLASS (ephy_window_parent_class)->delete_event)
	{
		return GTK_WIDGET_CLASS (ephy_window_parent_class)->delete_event (widget, event);
	}

	return FALSE;
}

static void
update_popup_actions_visibility (EphyWindow *window,
				 gboolean is_image,
				 gboolean is_frame)
{
	GtkAction *action;
	GtkActionGroup *action_group;

	action_group = window->priv->popups_action_group;

	action = gtk_action_group_get_action (action_group, "OpenImage");
	gtk_action_set_visible (action, is_image);
	action = gtk_action_group_get_action (action_group, "SaveImageAs");
	gtk_action_set_visible (action, is_image);
	action = gtk_action_group_get_action (action_group, "SetImageAsBackground");
	gtk_action_set_visible (action, is_image);
	action = gtk_action_group_get_action (action_group, "CopyImageLocation");
	gtk_action_set_visible (action, is_image);

	action = gtk_action_group_get_action (action_group, "OpenFrame");
	gtk_action_set_visible (action, is_frame);
}

static void
update_edit_actions_sensitivity (EphyWindow *window, gboolean hide)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean can_copy, can_cut, can_undo, can_redo, can_paste;

	if (GTK_IS_EDITABLE (widget))
	{
		gboolean has_selection;
		GtkActionGroup *action_group;
		GtkAction *location_action;
		GSList *proxies;
		GtkWidget *proxy;
		
		action_group = ephy_toolbar_get_action_group (window->priv->toolbar);
		location_action = gtk_action_group_get_action (action_group,
							       "Location");
		proxies = gtk_action_get_proxies (location_action);
		proxy = GTK_WIDGET (proxies->data);
		
		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (widget), NULL, NULL);

		can_copy = has_selection;
		can_cut = has_selection;
		can_paste = TRUE;
		if (proxy != NULL &&
		    EPHY_IS_LOCATION_ENTRY (proxy) &&
		    widget == ephy_location_entry_get_entry (EPHY_LOCATION_ENTRY (proxy)))
		{
			can_undo = ephy_location_entry_get_can_undo (EPHY_LOCATION_ENTRY (proxy));
			can_redo = ephy_location_entry_get_can_redo (EPHY_LOCATION_ENTRY (proxy));
		}
		else
		{
			can_undo = FALSE;
			can_redo = FALSE;
		}
	}
	else
	{
		EphyEmbed *embed;
		WebKitWebView *view;

		embed = window->priv->active_embed;
		g_return_if_fail (embed != NULL);

		view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

		can_copy = webkit_web_view_can_copy_clipboard (view);
		can_cut = webkit_web_view_can_cut_clipboard (view);
		can_paste = webkit_web_view_can_paste_clipboard (view);
		can_undo = webkit_web_view_can_undo (view);
		can_redo = webkit_web_view_can_redo (view);
	}

	action_group = window->priv->action_group;

	action = gtk_action_group_get_action (action_group, "EditCopy");
	gtk_action_set_sensitive (action, can_copy);
	gtk_action_set_visible (action, !hide || can_copy);
	action = gtk_action_group_get_action (action_group, "EditCut");
	gtk_action_set_sensitive (action, can_cut);
	gtk_action_set_visible (action, !hide || can_cut);
	action = gtk_action_group_get_action (action_group, "EditPaste");
	gtk_action_set_sensitive (action, can_paste);
	gtk_action_set_visible (action,  !hide || can_paste);
	action = gtk_action_group_get_action (action_group, "EditUndo");
	gtk_action_set_sensitive (action, can_undo);
	gtk_action_set_visible (action,  !hide || can_undo);
	action = gtk_action_group_get_action (action_group, "EditRedo");
	gtk_action_set_sensitive (action, can_redo);
	gtk_action_set_visible (action, !hide || can_redo);
}

static void
enable_edit_actions_sensitivity (EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = window->priv->action_group;

	action = gtk_action_group_get_action (action_group, "EditCopy");
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, TRUE);
	action = gtk_action_group_get_action (action_group, "EditCut");
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, TRUE);
	action = gtk_action_group_get_action (action_group, "EditPaste");
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, TRUE);
	action = gtk_action_group_get_action (action_group, "EditUndo");
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, TRUE);
	action = gtk_action_group_get_action (action_group, "EditRedo");
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, TRUE);
}

static void
edit_menu_show_cb (GtkWidget *menu,
		   EphyWindow *window)
{
	update_edit_actions_sensitivity (window, FALSE);
}

static void
edit_menu_hide_cb (GtkWidget *menu,
		   EphyWindow *window)
{
	enable_edit_actions_sensitivity (window);
}

static void
init_menu_updaters (EphyWindow *window)
{
	GtkWidget *edit_menu_item, *edit_menu;

	edit_menu_item = gtk_ui_manager_get_widget
		(window->priv->manager, "/menubar/EditMenu");
	edit_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (edit_menu_item));

	g_signal_connect (edit_menu, "show",
			  G_CALLBACK (edit_menu_show_cb), window);
	g_signal_connect (edit_menu, "hide",
			  G_CALLBACK (edit_menu_hide_cb), window);
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
		     EphyWindow *window)
{
	GtkAction *action;
	char *message;

	action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);
	
	g_object_get (action, "tooltip", &message, NULL);
	if (message)
	{
		gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
				    window->priv->help_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
		       EphyWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
			   window->priv->help_message_cid);
}

static gboolean
tool_item_enter_cb (GtkWidget *proxy,
		    GdkEventCrossing *event,
		    EphyWindow *window)
{
	if (event->mode == GDK_CROSSING_NORMAL)
	{
		GtkToolItem *item;
		GtkAction *action;
		char *message;
    
		item = GTK_TOOL_ITEM (gtk_widget_get_ancestor (proxy, GTK_TYPE_TOOL_ITEM));
		
		action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (item));
		g_return_val_if_fail (action != NULL, FALSE);
		
		g_object_get (action, "tooltip", &message, NULL);
		if (message)
		{
			gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
					    window->priv->help_message_cid, message);
			g_free (message);
		}
	}
	
	return FALSE;
}

static gboolean
tool_item_leave_cb (GtkWidget *proxy,
		    GdkEventCrossing *event,
		    EphyWindow *window)
{
	if (event->mode == GDK_CROSSING_NORMAL)
	{
		gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
				   window->priv->help_message_cid);
	}
	
	return FALSE;
}

static void
tool_item_drag_begin_cb (GtkWidget          *widget,
			 GdkDragContext     *context,
			 EphyWindow         *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
			   window->priv->help_message_cid);
}


static void
connect_tool_item (GtkWidget *proxy, EphyWindow *window)
{
	if (GTK_IS_CONTAINER (proxy))
	{
		gtk_container_foreach (GTK_CONTAINER (proxy),
				       (GtkCallback) connect_tool_item,
				       (gpointer) window);
	}

	g_signal_connect (proxy, "drag_begin",
			  G_CALLBACK (tool_item_drag_begin_cb), window);
	g_signal_connect (proxy, "enter-notify-event",
			  G_CALLBACK (tool_item_enter_cb), window);
	g_signal_connect (proxy, "leave-notify-event",
			  G_CALLBACK (tool_item_leave_cb), window);
}

static void
disconnect_tool_item (GtkWidget *proxy, EphyWindow *window)
{
	if (GTK_IS_CONTAINER (proxy))
	{
		gtk_container_foreach (GTK_CONTAINER (proxy),
				       (GtkCallback) disconnect_tool_item,
				       (gpointer) window);
	}

	g_signal_handlers_disconnect_by_func
	  (proxy, G_CALLBACK (tool_item_enter_cb), window);
	g_signal_handlers_disconnect_by_func
	  (proxy, G_CALLBACK (tool_item_leave_cb), window);
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
		     GtkAction *action,
		     GtkWidget *proxy,
		     EphyWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
	else if (GTK_IS_TOOL_ITEM (proxy))
	{
		disconnect_tool_item (proxy, window);
	}
}

static void
connect_proxy_cb (GtkUIManager *manager,
		  GtkAction *action,
		  GtkWidget *proxy,
		  EphyWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), window);
	}
	else if (GTK_IS_TOOL_ITEM (proxy))
	{
		connect_tool_item (proxy, window);
	}
}

static void
update_chromes_actions (EphyWindow *window)
{
	GtkActionGroup *action_group = window->priv->action_group;
	GtkAction *action;
	gboolean show_statusbar, show_menubar, show_toolbar, show_tabsbar;

	get_chromes_visibility (window, &show_menubar,
				&show_statusbar, &show_toolbar,
				&show_tabsbar);

	action = gtk_action_group_get_action (action_group, "ViewToolbar");
	g_signal_handlers_block_by_func (G_OBJECT (action),
		 			 G_CALLBACK (ephy_window_view_toolbar_cb),
		 			 window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !show_toolbar);
	g_signal_handlers_unblock_by_func (G_OBJECT (action),
		 			   G_CALLBACK (ephy_window_view_toolbar_cb),
		 			   window);

	action = gtk_action_group_get_action (action_group, "ViewStatusbar");
	g_signal_handlers_block_by_func (G_OBJECT (action),
		 			 G_CALLBACK (ephy_window_view_statusbar_cb),
		 			 window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_statusbar);
	g_signal_handlers_unblock_by_func (G_OBJECT (action),
		 			   G_CALLBACK (ephy_window_view_statusbar_cb),
		 			   window);
}

static void
setup_ui_manager (EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *manager;

	window->priv->main_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (window->priv->main_vbox);
	gtk_container_add (GTK_CONTAINER (window),
			   window->priv->main_vbox);

	window->priv->menu_dock = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (window->priv->menu_dock);
	gtk_box_pack_start (GTK_BOX (window->priv->main_vbox),
			    GTK_WIDGET (window->priv->menu_dock),
			    FALSE, TRUE, 0);

	manager = gtk_ui_manager_new ();

	g_signal_connect (manager, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), window);
	g_signal_connect (manager, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), window);

	action_group = gtk_action_group_new ("WindowActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, ephy_menu_entries,
				      G_N_ELEMENTS (ephy_menu_entries), window);
	gtk_action_group_add_toggle_actions (action_group,
					     ephy_menu_toggle_entries,
					     G_N_ELEMENTS (ephy_menu_toggle_entries),
					     window);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->action_group = action_group;
	g_object_unref (action_group);

	action = gtk_action_group_get_action (action_group, "FileOpen");
	g_object_set (action, "short_label", _("Open"), NULL);
	action = gtk_action_group_get_action (action_group, "FileSaveAs");
	g_object_set (action, "short_label", _("Save As"), NULL);
	action = gtk_action_group_get_action (action_group, "FilePrint");
	g_object_set (action, "short_label", _("Print"), NULL);
	action = gtk_action_group_get_action (action_group, "FileBookmarkPage");
	g_object_set (action, "short_label", _("Bookmark"), NULL);
	action = gtk_action_group_get_action (action_group, "EditFind");
	g_object_set (action, "short_label", _("Find"), NULL);
	action = gtk_action_group_get_action (action_group, "GoBookmarks");
	g_object_set (action, "short_label", _("Bookmarks"), NULL);

	action = gtk_action_group_get_action (action_group, "EditFind");
	g_object_set (action, "is_important", TRUE, NULL);
	action = gtk_action_group_get_action (action_group, "GoBookmarks");
	g_object_set (action, "is_important", TRUE, NULL);

	action = gtk_action_group_get_action (action_group, "ViewEncoding");
	g_object_set (action, "hide_if_empty", FALSE, NULL);
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");
	/* Translators: This refers to text size */
	g_object_set (action, "short-label", _("Larger"), NULL);
	action = gtk_action_group_get_action (action_group, "ViewZoomOut");
	/* Translators: This refers to text size */
	g_object_set (action, "short-label", _("Smaller"), NULL);

	action_group = gtk_action_group_new ("PopupsActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, ephy_popups_entries,
				      G_N_ELEMENTS (ephy_popups_entries), window);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->popups_action_group = action_group;
	g_object_unref (action_group);

	window->priv->manager = manager;
	g_signal_connect (manager, "add_widget", G_CALLBACK (add_widget), window);
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (manager));
}

static void
sync_tab_address (EphyWebView *view,
	          GParamSpec *pspec,
		  EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->closing) return;

	ephy_toolbar_set_location (priv->toolbar,
				   ephy_web_view_get_address (view),
				   ephy_web_view_get_typed_address (view));
	ephy_find_toolbar_request_close (priv->find_toolbar);
}

static void
sync_tab_document_type (EphyWebView *view,
			GParamSpec *pspec,
			EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group = priv->action_group;
	GtkAction *action;
	EphyWebViewDocumentType type;
	gboolean can_find, disable, is_image;

	if (priv->closing) return;

	/* update zoom actions */
	sync_tab_zoom (WEBKIT_WEB_VIEW (view), NULL, window);
	
	type = ephy_web_view_get_document_type (view);
	can_find = (type != EPHY_WEB_VIEW_DOCUMENT_IMAGE);
	is_image = type == EPHY_WEB_VIEW_DOCUMENT_IMAGE;
	disable = (type != EPHY_WEB_VIEW_DOCUMENT_HTML);

	action = gtk_action_group_get_action (action_group, "ViewEncoding");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_DOCUMENT, disable);
	action = gtk_action_group_get_action (action_group, "ViewPageSource");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_DOCUMENT, is_image);
	action = gtk_action_group_get_action (action_group, "EditFind");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_DOCUMENT, !can_find);
	action = gtk_action_group_get_action (action_group, "EditFindNext");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_DOCUMENT, !can_find);
	action = gtk_action_group_get_action (action_group, "EditFindPrev");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_DOCUMENT, !can_find);

	if (!can_find)
	{
		ephy_find_toolbar_request_close (priv->find_toolbar);
	}
}

static void
sync_tab_icon (EphyWebView *view,
	       GParamSpec *pspec,
	       EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GdkPixbuf *icon;

	if (priv->closing) return;

	icon = ephy_web_view_get_icon (view);

	ephy_toolbar_set_favicon (priv->toolbar, icon);
}

static gboolean
clear_progress_cb (EphyWindow *window)
{
	gtk_entry_set_progress_fraction (GTK_ENTRY (window->priv->entry), 0.0);
	window->priv->clear_progress_timeout_id = 0;

	return FALSE;
}

static void
sync_tab_load_progress (EphyWebView *view, GParamSpec *pspec, EphyWindow *window)
{
	gdouble progress;
	const char *uri;
	gboolean loading;

	if (window->priv->closing) return;

	if (window->priv->clear_progress_timeout_id)
	{
		g_source_remove (window->priv->clear_progress_timeout_id);
		window->priv->clear_progress_timeout_id = 0;
	}

	/* If we are loading about:blank do not show progress, as the
	   blink it causes is annoying. */
	/* FIXME: for some reason webkit_web_view_get_uri returns NULL
	   for about:blank until the load is finished, so assume NULL
	   here means we are loading about:blank. This might not be
	   rigt :) */
	loading = ephy_web_view_is_loading (view);
	uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view));
	if (loading && (!uri || strcmp (uri, "about:blank") == 0))
		return;

	progress = webkit_web_view_get_progress (WEBKIT_WEB_VIEW (view));
	if (progress == 1.0 && loading)
	{
		window->priv->clear_progress_timeout_id =
			g_timeout_add (500,
				       (GSourceFunc)clear_progress_cb,
				       window);
	}

	/* Do not set progress in the entry if the load is already
	   finished */
	gtk_entry_set_progress_fraction (GTK_ENTRY (window->priv->entry),
					 loading ? progress : 0.0);
}

static void
sync_tab_message (EphyWebView *view, GParamSpec *pspec, EphyWindow *window)
{
	GtkStatusbar *s = GTK_STATUSBAR (window->priv->statusbar);
	const char *message;

	if (window->priv->closing) return;

	message = ephy_web_view_get_status_message (view);

	gtk_statusbar_pop (s, window->priv->tab_message_cid);

	if (message)
	{
		gtk_statusbar_push (s, window->priv->tab_message_cid, message);
	}
}

static void
sync_tab_navigation (EphyWebView *view,
		     GParamSpec *pspec,
		     EphyWindow *window)
{
	EphyWebViewNavigationFlags flags;
	WebKitWebHistoryItem *item;
	WebKitWebView *web_view;
	WebKitWebBackForwardList *web_back_forward_list;
	gboolean up = FALSE, back = FALSE, forward = FALSE;
	const char *back_title = NULL, *forward_title = NULL;

	if (window->priv->closing) return;

	flags = ephy_web_view_get_navigation_flags (view);

	if (flags & EPHY_WEB_VIEW_NAV_UP)
	{
		up = TRUE;
	}
	if (flags & EPHY_WEB_VIEW_NAV_BACK)
	{
		back = TRUE;
	}
	if (flags & EPHY_WEB_VIEW_NAV_FORWARD)
	{
		forward = TRUE;
	}

	ephy_toolbar_set_navigation_actions (window->priv->toolbar,
					     back, forward, up);

	web_view = WEBKIT_WEB_VIEW (view);
	web_back_forward_list = webkit_web_view_get_back_forward_list (web_view);

	item = webkit_web_back_forward_list_get_back_item (web_back_forward_list);

	if (item)
	{
		back_title = webkit_web_history_item_get_title (item);
	}

	item = webkit_web_back_forward_list_get_forward_item (web_back_forward_list);

	if (item)
	{
		forward_title = webkit_web_history_item_get_title (item);
	}

	ephy_toolbar_set_navigation_tooltips (window->priv->toolbar,
					      back_title,
					      forward_title);
}

static void
sync_tab_security (EphyWebView *view,
		   GParamSpec *pspec,
		   EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyWebViewSecurityLevel level;
	char *description = NULL;
	char *state = NULL;
	char *tooltip;
	const char *stock_id = STOCK_LOCK_INSECURE;
	gboolean show_lock = FALSE, is_secure = FALSE;
	GtkAction *action;

	if (priv->closing) return;

	ephy_web_view_get_security_level (view, &level, &description);

	switch (level)
	{
		case EPHY_WEB_VIEW_STATE_IS_UNKNOWN:
			state = _("Unknown");
			break;
		case EPHY_WEB_VIEW_STATE_IS_INSECURE:
			state = _("Insecure");
			g_free (description);
			description = NULL;
			break;
		case EPHY_WEB_VIEW_STATE_IS_BROKEN:
			state = _("Broken");
			stock_id = STOCK_LOCK_BROKEN;
                        show_lock = TRUE;
                        g_free (description);
                        description = NULL;
                        break;
		case EPHY_WEB_VIEW_STATE_IS_SECURE_LOW:
		case EPHY_WEB_VIEW_STATE_IS_SECURE_MED:
			state = _("Low");
			/* We deliberately don't show the 'secure' icon
			 * for low & medium secure sites; see bug #151709.
			 */
			stock_id = STOCK_LOCK_INSECURE;
			break;
		case EPHY_WEB_VIEW_STATE_IS_SECURE_HIGH:
			state = _("High");
			stock_id = STOCK_LOCK_SECURE;
			show_lock = TRUE;
			is_secure = TRUE;
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	tooltip = g_strdup_printf (_("Security level: %s"), state);
	if (description != NULL)
	{
		char *tmp = tooltip;

		tooltip = g_strconcat (tmp, "\n", description, NULL);
		g_free (description);
		g_free (tmp);
	}

	ephy_statusbar_set_security_state (EPHY_STATUSBAR (priv->statusbar),
					   stock_id, tooltip);

	ephy_toolbar_set_security_state (priv->toolbar, is_secure,
					 show_lock, stock_id, tooltip);

	if (priv->fullscreen_popup != NULL)
	{
		ephy_fullscreen_popup_set_security_state
			(EPHY_FULLSCREEN_POPUP (priv->fullscreen_popup),
			 show_lock, stock_id, tooltip);
	}

	action = gtk_action_group_get_action (priv->action_group, "ViewPageSecurityInfo");
	gtk_action_set_sensitive (action, is_secure);

	g_free (tooltip);
}

static void
sync_tab_popup_windows (EphyWebView *view,
			GParamSpec *pspec,
			EphyWindow *window)
{
	guint num_popups = 0;
	char *tooltip = NULL;

	g_object_get (view,
		      "hidden-popup-count", &num_popups,
		      NULL);

	if (num_popups > 0)
	{
		tooltip = g_strdup_printf (ngettext ("%d hidden popup window",
						     "%d hidden popup windows",
						     num_popups),
					   num_popups);
	}

	ephy_statusbar_set_popups_state
		(EPHY_STATUSBAR (window->priv->statusbar),
		 tooltip == NULL,
		 tooltip);

	g_free (tooltip);
}

static void
sync_tab_popups_allowed (EphyWebView *view,
			 GParamSpec *pspec,
			 EphyWindow *window)
{
	GtkAction *action;
	gboolean allow;

	g_return_if_fail (EPHY_IS_WEB_VIEW (view));
	g_return_if_fail (EPHY_IS_WINDOW (window));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewPopupWindows");
	g_return_if_fail (GTK_IS_ACTION (action));

	g_object_get (view, "popups-allowed", &allow, NULL);

	g_signal_handlers_block_by_func
		(G_OBJECT (action),
		 G_CALLBACK (ephy_window_view_popup_windows_cb),
		 window);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), allow);

	g_signal_handlers_unblock_by_func
		(G_OBJECT (action),
		 G_CALLBACK (ephy_window_view_popup_windows_cb),
		 window);
}

static void
sync_tab_load_status (EphyWebView  *view,
		      GParamSpec *pspec,
		      EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group = priv->action_group;
	GtkAction *action;
	gboolean loading;

	if (window->priv->closing) return;

	loading = ephy_web_view_is_loading (view);

	action = gtk_action_group_get_action (action_group, "ViewStop");
	gtk_action_set_sensitive (action, loading);

	/* disable print while loading, see bug #116344 */
	action = gtk_action_group_get_action (action_group, "FilePrintPreview");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_LOADING, loading);
	action = gtk_action_group_get_action (action_group, "FilePrint");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_LOADING, loading);

	ephy_toolbar_set_spinning (priv->toolbar, loading);

	if (priv->fullscreen_popup)
	{
		ephy_fullscreen_popup_set_spinning
			 (EPHY_FULLSCREEN_POPUP (priv->fullscreen_popup),
			  loading);
	}
}

static void
sync_tab_title (EphyWebView *view,
		GParamSpec *pspec,
		EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->closing) return;

	gtk_window_set_title (GTK_WINDOW(window),
			      ephy_web_view_get_title_composite (view));
}

static void
sync_tab_zoom (WebKitWebView *web_view, GParamSpec *pspec, EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	EphyWebViewDocumentType type;
	gboolean can_zoom_in = TRUE, can_zoom_out = TRUE, can_zoom_normal = FALSE, can_zoom;
	float zoom;
	EphyEmbed *embed = window->priv->active_embed;

	if (window->priv->closing) return;

	g_object_get (web_view,
		      "zoom-level", &zoom,
		      NULL);

	type = ephy_web_view_get_document_type (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed));
	can_zoom = (type != EPHY_WEB_VIEW_DOCUMENT_IMAGE);

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

	ephy_toolbar_set_zoom (window->priv->toolbar, can_zoom, zoom);

	action_group = window->priv->action_group;
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");
	gtk_action_set_sensitive (action, can_zoom_in && can_zoom);
	action = gtk_action_group_get_action (action_group, "ViewZoomOut");
	gtk_action_set_sensitive (action, can_zoom_out && can_zoom);
	action = gtk_action_group_get_action (action_group, "ViewZoomNormal");
	gtk_action_set_sensitive (action, can_zoom_normal && can_zoom);
}

static void
sync_network_status (EphyEmbedSingle *single,
		     GParamSpec *pspec,
		     EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkAction *action;
	gboolean is_online;

	is_online = ephy_embed_single_get_network_status (single);

	action = gtk_action_group_get_action (priv->action_group,
					      "FileWorkOffline");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (window_cmd_file_work_offline), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !is_online);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (window_cmd_file_work_offline), window);	
}

static void
popup_menu_at_coords (GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
		      gpointer user_data)
{
	EphyWindow *window = EPHY_WINDOW (user_data);
	EphyWindowPrivate *priv = window->priv;
	guint ux, uy;

	g_return_if_fail (priv->context_event != NULL);

	ephy_embed_event_get_coords (priv->context_event, &ux, &uy);
	*x = ux; *y = uy;

	/* FIXME: better position the popup within the window bounds? */
	ephy_gui_sanitise_popup_position (menu, GTK_WIDGET (window), x, y);

	*push_in = TRUE;
}

static gboolean
idle_unref_context_event (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	LOG ("Idle unreffing context event %p", priv->context_event);

	if (priv->context_event != NULL)
	{
		g_object_unref (priv->context_event);
		priv->context_event = NULL;
	}

	priv->idle_worker = 0;
	return FALSE;
}

void
_ephy_window_set_context_event (EphyWindow *window,
				EphyEmbedEvent *event)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->idle_worker != 0)
	{
		g_source_remove (priv->idle_worker);
		priv->idle_worker = 0;
	}

	if (priv->context_event != NULL)
	{
		g_object_unref (priv->context_event);
	}

	priv->context_event = event != NULL ? g_object_ref (event) : NULL;
}

void
_ephy_window_unset_context_event (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	/* Unref the event from idle since we still need it
	 * from the action callbacks which will run before idle.
	 */
	if (priv->idle_worker == 0 && priv->context_event != NULL)
	{
		priv->idle_worker =
			g_idle_add ((GSourceFunc) idle_unref_context_event, window);
	}
}

static void
embed_popup_deactivate_cb (GtkWidget *popup,
			   EphyWindow *window)
{
	LOG ("Deactivating popup menu");

	enable_edit_actions_sensitivity (window);

	g_signal_handlers_disconnect_by_func
		(popup, G_CALLBACK (embed_popup_deactivate_cb), window);

	_ephy_window_unset_context_event (window);
}

static char *
get_name_from_address_value (const char *path)
{
	char *name;

	name = g_path_get_basename (path);

	return name != NULL ? name : g_strdup ("");
}

static void
update_popups_tooltips (EphyWindow *window, GdkEventButton *event, WebKitHitTestResult *hit_test_result)
{
	guint context;
	GtkActionGroup *group = window->priv->popups_action_group;
	GtkAction *action;
	char *tooltip, *name;

	 g_object_get (hit_test_result, "context", &context, NULL);

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE)
	{
		char *uri;
		g_object_get (hit_test_result, "image-uri", &uri, NULL);
		name = get_name_from_address_value (uri);

		action = gtk_action_group_get_action (group, "OpenImage");
		tooltip = g_strdup_printf (_("Open image “%s”"), name);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "SetImageAsBackground");
		tooltip = g_strdup_printf (_("Use as desktop background “%s”"), name);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "SaveImageAs");
		tooltip = g_strdup_printf (_("Save image “%s”"), name);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "CopyImageLocation");
		tooltip = g_strdup_printf (_("Copy image address “%s”"), uri);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);		

		g_free (uri);
		g_free (name);
	}

#if 0
	if (context & EPHY_EMBED_CONTEXT_EMAIL_LINK)
	{
		value = ephy_embed_event_get_property (event, "link");

		action = gtk_action_group_get_action (group, "SendEmail");
		tooltip = g_strdup_printf (_("Send email to address “%s”"),
					   g_value_get_string (value));
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "CopyEmailAddress");
		tooltip = g_strdup_printf (_("Copy email address “%s”"),
					   g_value_get_string (value));
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);
	}
#endif

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
	{
		char *uri;
		g_object_get (hit_test_result, "link-uri", &uri, NULL);

		action = gtk_action_group_get_action (group, "DownloadLink");
		name = get_name_from_address_value (uri);
		tooltip = g_strdup_printf (_("Save link “%s”"), name);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (name);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "BookmarkLink");
		tooltip = g_strdup_printf (_("Bookmark link “%s”"), uri);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "CopyLinkAddress");
		tooltip = g_strdup_printf (_("Copy link's address “%s”"), uri);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);
		g_free (uri);
	}
}

static void
show_embed_popup (EphyWindow *window,
		  WebKitWebView *view,
		  GdkEventButton *event,
		  WebKitHitTestResult *hit_test_result)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group;
	GtkAction *action;
	guint context;
	const char *popup;
	gboolean framed = FALSE, can_open_in_new;
	GtkWidget *widget;
	guint button;
	char *uri;
	EphyEmbedEvent *embed_event;

	/* Do not show the menu in print preview mode */
	if (priv->ppv_mode)
	{
		return;
	}

#if 0
	value = ephy_embed_event_get_property (event, "framed_page");
	framed = g_value_get_int (value);
#endif

	g_object_get (hit_test_result, "link-uri", &uri, NULL);
	can_open_in_new = uri && ephy_embed_utils_address_has_web_scheme (uri);
	g_free (uri);

	g_object_get (hit_test_result, "context", &context, NULL);

	LOG ("show_embed_popup context %x", context);

#if 0
	if (context & EPHY_EMBED_CONTEXT_EMAIL_LINK)
	{
		popup = "/EphyEmailLinkPopup";
		update_edit_actions_sensitivity (window, TRUE);
	}
#endif
	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
	{
		popup = "/EphyLinkPopup";
		update_edit_actions_sensitivity (window, TRUE);
	}
	else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)
	{
		popup = "/EphyInputPopup";
		update_edit_actions_sensitivity (window, FALSE);
	}
	else
	{
		popup = "/EphyDocumentPopup";
		update_edit_actions_sensitivity (window, TRUE);
	}

	update_popups_tooltips (window, event, hit_test_result);

	widget = gtk_ui_manager_get_widget (priv->manager, popup);
	g_return_if_fail (widget != NULL);

	action_group = window->priv->popups_action_group;

	action = gtk_action_group_get_action (action_group, "OpenLinkInNewWindow");
	gtk_action_set_sensitive (action, can_open_in_new);

	action = gtk_action_group_get_action (action_group, "OpenLinkInNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CONTEXT, !can_open_in_new);

	
	update_popup_actions_visibility (window,
					 context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE,
					 framed);

	embed_event = ephy_embed_event_new (event, hit_test_result);
	_ephy_window_set_context_event (window, embed_event);
	g_object_unref (embed_event);

	g_signal_connect (widget, "deactivate",
			  G_CALLBACK (embed_popup_deactivate_cb), window);

	button = event->button;

	if (button == 0)
	{
		gtk_menu_popup (GTK_MENU (widget), NULL, NULL,
				popup_menu_at_coords, window, 0,
				gtk_get_current_event_time ());
		gtk_menu_shell_select_first (GTK_MENU_SHELL (widget), FALSE);
	}
	else
	{
		gtk_menu_popup (GTK_MENU (widget), NULL, NULL,
				NULL, NULL, button,
				gtk_get_current_event_time ());
	}
}

#if 0
static gboolean
save_property_url (EphyEmbed *embed,
		   EphyEmbedEvent *event,
		   const char *property,
		   const char *key)
{
	const char *location;
	const GValue *value;
	EphyEmbedPersist *persist;

	value = ephy_embed_event_get_property (event, property);
	location = g_value_get_string (value);

	if (!ephy_embed_utils_address_has_web_scheme (location)) return FALSE;

	persist = EPHY_EMBED_PERSIST
		(g_object_new (EPHY_TYPE_EMBED_PERSIST, NULL));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_flags (persist, 0);
	ephy_embed_persist_set_persist_key (persist, key);
	ephy_embed_persist_set_source (persist, location);

	ephy_embed_persist_save (persist);

	g_object_unref (G_OBJECT(persist));

	return TRUE;
}
#endif

typedef struct
{
	EphyWindow *window;
	EphyEmbed *embed;
} ClipboardTextCBData;

static void
clipboard_text_received_cb (GtkClipboard *clipboard,
			    const char *text,
			    ClipboardTextCBData *data)
{
	if (data->embed != NULL && text != NULL)
	{
		ephy_link_open (EPHY_LINK (data->window), text, data->embed, 0);
	}

	if (data->embed != NULL)
	{
		EphyEmbed **embed_ptr = &(data->embed);
		g_object_remove_weak_pointer (G_OBJECT (data->embed), (gpointer *) embed_ptr);
	}

	g_slice_free (ClipboardTextCBData, data);
}

static gboolean
ephy_window_dom_mouse_click_cb (WebKitWebView *view,
				GdkEventButton *event,
				EphyWindow *window)
{
	guint button, modifier, context;
	gboolean handled = TRUE;
	gboolean with_control, with_shift, with_shift_control;
	gboolean is_left_click, is_middle_click;
	gboolean is_link, is_image, is_middle_clickable;
	gboolean middle_click_opens;
	gboolean is_input;
	WebKitHitTestResult *hit_test_result;

	hit_test_result = webkit_web_view_get_hit_test_result (view, event);
	button = event->button;

	if (event->button == 3)
	{
		show_embed_popup (window, view, event, hit_test_result);
		g_object_unref (hit_test_result);
		return TRUE;
	}

	modifier = event->state;
	g_object_get (hit_test_result, "context", &context, NULL);
	g_object_unref (hit_test_result);

	LOG ("ephy_window_dom_mouse_click_cb: button %d, context %d, modifier %d (%d:%d)",
	     button, context, modifier, (int)event->x, (int)event->y);

	with_control = (modifier & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
	with_shift = (modifier & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;
	with_shift_control = (modifier & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
					== (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
	is_left_click = (button == 1);
	is_middle_click = (button == 2);

	middle_click_opens =
		eel_gconf_get_boolean (CONF_INTERFACE_MIDDLE_CLICK_OPEN_URL) &&
		!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL);

	is_link = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) != 0;
	is_image = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) != 0;
	is_middle_clickable = !((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
				|| (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE));
	is_input = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) != 0;

#if 0
	/* shift+click saves the link target */
	if (is_link && is_left_click && with_shift)
	{
		handled = save_property_url (embed, event, "link", CONF_STATE_SAVE_DIR);
	}
	/* shift+click saves the non-link image
	 * Note: pressing enter to submit a form synthesizes a mouse click event
	 */
	else if (is_image && is_left_click && with_shift && !is_input)
	{
		handled = save_property_url (embed, event, "image", CONF_STATE_SAVE_IMAGE_DIR);
	}
#endif
	/* middle click opens the selection url */
	if (is_middle_clickable && is_middle_click && middle_click_opens)
	{
		/* See bug #133633 for why we do it this way */

		/* We need to make sure we know if the embed is destroyed between
		 * requesting the clipboard contents, and receiving them.
		 */
		ClipboardTextCBData *cb_data;
		EphyEmbed *embed;
		EphyEmbed **embed_ptr;

		embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

		cb_data = g_slice_new0 (ClipboardTextCBData);
		cb_data->embed = embed;
		cb_data->window = window;
		embed_ptr = &cb_data->embed;
		
		g_object_add_weak_pointer (G_OBJECT (embed), (gpointer *) embed_ptr);

		gtk_clipboard_request_text
			(gtk_widget_get_clipboard (GTK_WIDGET (embed),
						   GDK_SELECTION_PRIMARY),
			 (GtkClipboardTextReceivedFunc) clipboard_text_received_cb,
			 cb_data);
	}
	/* we didn't handle the event */
	else
	{
		handled = FALSE;
	}

	return handled;
}

static void
ephy_window_visibility_cb (EphyEmbed *embed, GParamSpec *pspec, EphyWindow *window)
{
	gboolean visibility;

	visibility = ephy_web_view_get_visibility (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed));

	if (visibility)
		gtk_widget_show (GTK_WIDGET (window));
	else
		gtk_widget_hide (GTK_WIDGET (window));
}

static gboolean
web_view_ready_cb (WebKitWebView *web_view,
		   gpointer user_data)
{
	EphyWindow *window;
	gboolean using_new_window;

	using_new_window = GPOINTER_TO_INT (user_data);

	window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (web_view)));

	if (using_new_window)
	{
		int width, height;
		gboolean toolbar_visible;
		gboolean statusbar_visible;
		gboolean menubar_visible;
		EphyWebViewChrome chrome_mask;
		WebKitWebWindowFeatures *features;

		toolbar_visible = statusbar_visible = menubar_visible = TRUE;
		features = webkit_web_view_get_window_features (web_view);

		chrome_mask = window->priv->chrome;

		g_object_get (features,
			      "width", &width,
			      "height", &height,
			      "toolbar-visible", &toolbar_visible,
			      "statusbar-visible", &statusbar_visible,
			      "menubar-visible", &menubar_visible,
			      NULL);

		gtk_window_set_default_size (GTK_WINDOW (window), width, height);

		if (!toolbar_visible)
			chrome_mask &= ~EPHY_WEB_VIEW_CHROME_TOOLBAR;

		if (!statusbar_visible)
			chrome_mask &= ~EPHY_WEB_VIEW_CHROME_STATUSBAR;

		if (!menubar_visible)
			chrome_mask &= ~EPHY_WEB_VIEW_CHROME_MENUBAR;

		window->priv->chrome = chrome_mask;

		update_chromes_actions (window);
		sync_chromes_visibility (window);
	}

	gtk_widget_show (GTK_WIDGET (window));

	return TRUE;
}

static WebKitWebView*
create_web_view_cb (WebKitWebView *web_view,
		    WebKitWebFrame *frame,
		    EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *new_web_view;
	EphyNewTabFlags flags;
	EphyWindow *parent_window;
	gboolean using_new_window;

	if (eel_gconf_get_boolean (CONF_INTERFACE_OPEN_NEW_WINDOWS_IN_TAB))
	{
		parent_window = window;
		flags = EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			EPHY_NEW_TAB_JUMP;

	}
	else
	{
		parent_window = NULL;
		flags = EPHY_NEW_TAB_IN_NEW_WINDOW |
			EPHY_NEW_TAB_DONT_SHOW_WINDOW;
	}

	embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
					 parent_window,
					 NULL, NULL,
					 flags,
					 EPHY_WEB_VIEW_CHROME_ALL,
					 FALSE,
					 0);

	using_new_window = parent_window == NULL;

	new_web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	g_signal_connect (new_web_view, "web-view-ready",
			  G_CALLBACK (web_view_ready_cb),
			  GINT_TO_POINTER (using_new_window));

	return new_web_view;
}

static gboolean
policy_decision_required_cb (WebKitWebView *web_view,
			     WebKitWebFrame *web_frame,
			     WebKitNetworkRequest *request,
			     WebKitWebNavigationAction *action,
			     WebKitWebPolicyDecision *decision,
			     EphyWindow *window)
{
	WebKitWebNavigationReason reason;
	gint button;
	gint state;

	reason = webkit_web_navigation_action_get_reason (action);
	button = webkit_web_navigation_action_get_button (action);
	state = webkit_web_navigation_action_get_modifier_state (action);

	/* Open in new tab for middle click or ctrl+click */
	if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED &&
	    (button == 2 /* middle button */ ||
	     (button == 1 && state == GDK_CONTROL_MASK) /* ctrl + left button */)) {
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child
			(EPHY_EMBED_CONTAINER (window));

		ephy_shell_new_tab_full (ephy_shell_get_default (),
					 window,
					 embed,
					 request,
					 EPHY_NEW_TAB_IN_EXISTING_WINDOW |
					 EPHY_NEW_TAB_OPEN_PAGE,
					 EPHY_WEB_VIEW_CHROME_ALL, FALSE, 0);

		return TRUE;
	}

	return FALSE;
}

/* FIXME: get rid of this stuff when the DOM API lands */
/* FIXME: would be nice to get transparent background/opaque text, but
   not sure if I can be bothered */
const char *add_node_string =
	"var node = document.getElementById('epiphanyWebKitFloatingStatusBar');"\
	"if (node) node.parentNode.removeChild(node);"\
	"var node = document.createElement('div');"\
	"node.id = 'epiphanyWebKitFloatingStatusBar';"\
	"node.style.padding = '0.4em';"\
	"node.style.zIndex = '9999';"\
	"node.style.border = '1px solid black';"\
	"node.style.background = 'rgb(%d,%d,%d)';"\
	"node.style.color = 'rgb(%d,%d,%d)';"\
	"node.style.position = 'fixed';"\
	"node.style.left = '0';"\
	"node.style.bottom = '0';"\
	"node.style.opacity = 0.95;"\
	"var text = document.createTextNode('%s');"\
	"var body = document.getElementsByTagName('body')[0];"\
	"node.appendChild(text);"\
	"body.appendChild(node);";

const char *remove_node_string =
	"var node = document.getElementById('epiphanyWebKitFloatingStatusBar');"\
	"if (node) node.parentNode.removeChild(node);";

static void
ephy_window_link_message_cb (EphyWebView *web_view, GParamSpec *spec, EphyWindow *window)
{
	gboolean visible;
	const char *link_message;
	WebKitWebView *view;

	g_object_get (window->priv->statusbar, "visible", &visible, NULL);

	view = WEBKIT_WEB_VIEW (web_view);
	link_message = ephy_web_view_get_link_message (web_view);

	/* If the statusbar is visible remove the test, it might get
	   stuck otherwise */
	if (link_message && visible == FALSE)
	{
		char *script;
		GdkColor bg, fg;
		GtkWidget *widget;
		GtkAllocation allocation;
		GtkStyle *style;
		GtkStateType state;
		PangoLayout *layout;
		PangoLayoutLine *line;
		PangoLayoutRun *run;
		PangoItem *item;
		const char *text;
		char *freeme;

		widget = GTK_WIDGET (view);
		layout = gtk_widget_create_pango_layout (widget, link_message);
		gtk_widget_get_allocation (widget, &allocation);
		pango_layout_set_width (layout, PANGO_SCALE * (allocation.width * 0.9));
		pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

		line = pango_layout_get_line_readonly (layout, 0);
		run = line->runs->data;
		item = run->item;

		freeme = NULL;
		text = pango_layout_get_text (layout);
		if (item->num_chars < g_utf8_strlen (text, -1))
		{
			char buffer[2048]; /* Should be enough ... */
			g_utf8_strncpy (buffer, text, item->num_chars - 3);
			freeme = g_strconcat (buffer, "...", NULL);
		}
			
		g_utf8_strncpy ((gchar *)text, pango_layout_get_text (layout), item->num_chars);
		style = gtk_widget_get_style (widget);
		state = gtk_widget_get_state (widget);
		bg = style->bg[state];
		fg = style->fg[state];

		script = g_strdup_printf(add_node_string,
					 (int) (bg.red / 65535. * 255),
					 (int) (bg.green / 65535. * 255),
					 (int) (bg.blue / 65535. * 255),
					 (int) (fg.red / 65535. * 255),
					 (int) (fg.green / 65535. * 255),
					 (int) (fg.blue / 65535. * 255),
					 freeme ? freeme : text);
		webkit_web_view_execute_script (view, script);
		g_object_unref (layout);
		g_free (script);
		g_free (freeme);
	}
	else
	{
		const char *script = remove_node_string;
		webkit_web_view_execute_script (view, script);
	}
}

static void
ephy_window_set_active_tab (EphyWindow *window, EphyEmbed *new_embed)
{
	EphyEmbed *old_embed;
	EphyEmbed *embed;

	g_return_if_fail (EPHY_IS_WINDOW (window));
	g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (new_embed)) == GTK_WIDGET (window));

	old_embed = window->priv->active_embed;

	if (old_embed == new_embed) return;

	if (old_embed != NULL)
	{
		WebKitWebView *web_view;
		EphyWebView *view;
		guint sid;

		embed = old_embed;
		web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
		view = EPHY_WEB_VIEW (web_view);

		g_signal_handlers_disconnect_by_func (web_view,
						      G_CALLBACK (sync_tab_zoom),
						      window);
		g_signal_handlers_disconnect_by_func (web_view,
						      G_CALLBACK (scroll_event_cb),
						      window);
		g_signal_handlers_disconnect_by_func (web_view,
						      G_CALLBACK (create_web_view_cb),
						      window);
		sid = g_signal_lookup ("navigation-policy-decision-requested",
				       WEBKIT_TYPE_WEB_VIEW);
		g_signal_handlers_disconnect_matched (web_view,
						      G_SIGNAL_MATCH_ID |
						      G_SIGNAL_MATCH_FUNC,
						      sid,
						      0, NULL,
						      G_CALLBACK (policy_decision_required_cb),
						      NULL);
		sid = g_signal_lookup ("new-window-policy-decision-requested",
				       WEBKIT_TYPE_WEB_VIEW);
		g_signal_handlers_disconnect_matched (web_view,
						      G_SIGNAL_MATCH_ID |
						      G_SIGNAL_MATCH_FUNC,
						      sid,
						      0, NULL,
						      G_CALLBACK (policy_decision_required_cb),
						      NULL);

		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_popup_windows),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_popups_allowed),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_security),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_document_type),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_load_progress),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_load_status),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_navigation),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_title),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_address),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_icon),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (sync_tab_message),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (ephy_window_visibility_cb),
						      window);
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (ephy_window_link_message_cb),
						      window);

		g_signal_handlers_disconnect_by_func
			(view, G_CALLBACK (ephy_window_dom_mouse_click_cb), window);

	}

	window->priv->active_embed = new_embed;

	if (new_embed != NULL)
	{
		WebKitWebView *web_view;
		EphyWebView *view;

		embed = new_embed;
		view = EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed);

		sync_tab_security	(view, NULL, window);
		sync_tab_document_type	(view, NULL, window);
		sync_tab_load_progress	(view, NULL, window);
		sync_tab_load_status	(view, NULL, window);
		sync_tab_navigation	(view, NULL, window);
		sync_tab_title		(view, NULL, window);
		sync_tab_address	(view, NULL, window);
		sync_tab_icon		(view, NULL, window);
		sync_tab_message	(view, NULL, window);
		sync_tab_popup_windows	(view, NULL, window);
		sync_tab_popups_allowed	(view, NULL, window);

		web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
		view = EPHY_WEB_VIEW (web_view);

		sync_tab_zoom		(web_view, NULL, window);

		g_signal_connect_object (web_view, "notify::zoom-level",
					 G_CALLBACK (sync_tab_zoom),
					 window, 0);
		/* FIXME: we should set our own handler for
		   scroll-event, but right now it's pointless because
		   GtkScrolledWindow will eat all the events, even
		   those with modifier keys we want to catch to zoom
		   in and out. See bug #562630
		*/
		g_signal_connect_object (web_view, "scroll-event",
					 G_CALLBACK (scroll_event_cb),
					 window, 0);
		g_signal_connect_object (web_view, "create-web-view",
					 G_CALLBACK (create_web_view_cb),
					 window, 0);
		g_signal_connect_object (web_view, "navigation-policy-decision-requested",
					 G_CALLBACK (policy_decision_required_cb),
					 window, 0);
		g_signal_connect_object (web_view, "new-window-policy-decision-requested",
					 G_CALLBACK (policy_decision_required_cb),
					 window, 0);

		g_signal_connect_object (view, "notify::hidden-popup-count",
					 G_CALLBACK (sync_tab_popup_windows),
					 window, 0);
		g_signal_connect_object (view, "notify::popups-allowed",
					 G_CALLBACK (sync_tab_popups_allowed),
					 window, 0);
		g_signal_connect_object (view, "notify::embed-title",
					 G_CALLBACK (sync_tab_title),
					 window, 0);
		g_signal_connect_object (view, "notify::address",
					 G_CALLBACK (sync_tab_address),
					 window, 0);
		g_signal_connect_object (view, "notify::icon",
					 G_CALLBACK (sync_tab_icon),
					 window, 0);
		g_signal_connect_object (view, "notify::status-message",
					 G_CALLBACK (sync_tab_message),
					 window, 0);
		g_signal_connect_object (view, "notify::security-level",
					 G_CALLBACK (sync_tab_security),
					 window, 0);
		g_signal_connect_object (view, "notify::document-type",
					 G_CALLBACK (sync_tab_document_type),
					 window, 0);
		g_signal_connect_object (view, "notify::load-status",
					 G_CALLBACK (sync_tab_load_status),
					 window, 0);
		g_signal_connect_object (view, "notify::navigation",
					 G_CALLBACK (sync_tab_navigation),
					 window, 0);
		g_signal_connect_object (view, "notify::progress",
					 G_CALLBACK (sync_tab_load_progress),
					 window, 0);
		g_signal_connect_object (view, "button-press-event",
					 G_CALLBACK (ephy_window_dom_mouse_click_cb),
					 window, 0);
		g_signal_connect_object (view, "notify::visibility",
					 G_CALLBACK (ephy_window_visibility_cb),
					 window, 0);
		g_signal_connect_object (view, "notify::link-message",
					 G_CALLBACK (ephy_window_link_message_cb),
					 window, 0);

		g_object_notify (G_OBJECT (window), "active-child");
	}
}

static void
update_tabs_menu_sensitivity (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkNotebook *notebook;
	int page, n_pages;
	gboolean not_first, not_last;

	notebook = GTK_NOTEBOOK (priv->notebook);
	page = gtk_notebook_get_current_page (notebook);
	n_pages = gtk_notebook_get_n_pages (notebook);
	not_first = page > 0;
	not_last = page + 1 < n_pages;

	action_group = priv->action_group;
	action = gtk_action_group_get_action (action_group, "TabsPrevious");
	gtk_action_set_sensitive (action, not_first);
	action = gtk_action_group_get_action (action_group, "TabsNext");
	gtk_action_set_sensitive (action, not_last);
	action = gtk_action_group_get_action (action_group, "TabsMoveLeft");
	gtk_action_set_sensitive (action, not_first);
	action = gtk_action_group_get_action (action_group, "TabsMoveRight");
	gtk_action_set_sensitive (action, not_last);

	action = gtk_action_group_get_action (action_group, "TabsDetach");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, n_pages <= 1);
}

static gboolean
embed_modal_alert_cb (EphyEmbed *embed,
		      EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	const char *address;

	/* if we're in ppv mode, we cannot switch tabs, so inhibit the alert */
	if (priv->ppv_mode) return TRUE;

	/* switch the window to the tab, and bring the window to the foreground
	 * (since the alert is modal, the user won't be able to do anything
	 * with his current window anyway :|)
	 */
	impl_set_active_child (EPHY_EMBED_CONTAINER (window), embed);
	gtk_window_present (GTK_WINDOW (window));

	/* make sure the location entry shows the real URL of the tab's page */
	address = ephy_web_view_get_address (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed));
	ephy_toolbar_set_location (priv->toolbar, address, NULL);

	/* don't suppress alert */
	return FALSE;
}

static gboolean
idle_tab_remove_cb (GtkWidget *tab)
{
	GtkWidget *toplevel;
	EphyWindow *window;
	EphyWindowPrivate *priv;
	GtkNotebook *notebook;
	int position;

	toplevel = gtk_widget_get_toplevel (tab);
	if (!EPHY_IS_WINDOW (toplevel)) return FALSE; /* FIXME should this ever occur? */

	window = EPHY_WINDOW (toplevel);
	priv = window->priv;

	if (priv->closing) return FALSE;

	g_hash_table_remove (priv->tabs_to_remove, tab);

	notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));

	position = gtk_notebook_page_num (notebook, tab);
	gtk_notebook_remove_page (notebook, position);

	/* don't run again */
	return FALSE;
}

static void
schedule_tab_close (EphyWindow *window,
		    EphyEmbed *embed)
{
	EphyWindowPrivate *priv = window->priv;
	guint id;

	LOG ("scheduling close of embed %p in window %p", embed, window);

	if (priv->closing) return;

	if (g_hash_table_lookup (priv->tabs_to_remove, embed) != NULL) return;

	/* do this on idle, because otherwise we'll crash in certain circumstances
	* (see galeon bug #116256)
	*/
	id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
			      (GSourceFunc) idle_tab_remove_cb,
			      embed, NULL);

	g_hash_table_insert (priv->tabs_to_remove, embed, GUINT_TO_POINTER (id));

	/* don't wait until idle to hide the window */
	if (g_hash_table_size (priv->tabs_to_remove) == priv->num_tabs)
	{
		gtk_widget_hide (GTK_WIDGET (window));
	}
}

static gboolean
embed_close_request_cb (EphyEmbed *embed,
			EphyWindow *window)
{
	LOG ("embed_close_request_cb embed %p window %p", embed, window);

	schedule_tab_close (window, embed);

	/* handled */
	return TRUE;
}

static gboolean
show_notebook_popup_menu (GtkNotebook *notebook,
			  EphyWindow *window,
			  GdkEventButton *event)
{
	GtkWidget *menu, *tab, *tab_label;
	GtkAction *action;

	menu = gtk_ui_manager_get_widget (window->priv->manager, "/EphyNotebookPopup");
	g_return_val_if_fail (menu != NULL, FALSE);

	/* allow extensions to sync when showing the popup */
	action = gtk_action_group_get_action (window->priv->action_group,
					      "NotebookPopupAction");
	g_return_val_if_fail (action != NULL, FALSE);
	gtk_action_activate (action);

	if (event != NULL)
	{
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				NULL, NULL,
				event->button, event->time);
	}
	else
	{
		tab = GTK_WIDGET (window->priv->active_embed);
		tab_label = gtk_notebook_get_tab_label (notebook, tab);

		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				ephy_gui_menu_position_under_widget, tab_label,
				0, gtk_get_current_event_time ());
		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	}

	return TRUE;
}

static gboolean
notebook_button_press_cb (GtkNotebook *notebook,
			  GdkEventButton *event,
			  EphyWindow *window)
{
	if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
	{
		return show_notebook_popup_menu (notebook, window, event);
	}

	return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkNotebook *notebook,
			EphyWindow *window)
{
	/* Only respond if the notebook is the actual focus */
	if (EPHY_IS_NOTEBOOK (gtk_window_get_focus (GTK_WINDOW (window))))
	{
		return show_notebook_popup_menu (notebook, window, NULL);
	}

	return FALSE;
}

static gboolean
present_on_idle_cb (GtkWindow *window)
{
      gtk_window_present (window);
      return FALSE;
}

static void
notebook_page_added_cb (EphyNotebook *notebook,
			EphyEmbed *embed,
			guint position,
			EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyExtension *manager;

	LOG ("page-added   notebook %p embed %p position %u\n", notebook, embed, position);

	g_return_if_fail (EPHY_IS_EMBED (embed));

	priv->num_tabs++;

	update_tabs_menu_sensitivity (window);

#if 0
	g_signal_connect_object (embed, "open-link",
				 G_CALLBACK (ephy_link_open), window,
				 G_CONNECT_SWAPPED);
#endif

	g_signal_connect_object (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), "close-request",
				 G_CALLBACK (embed_close_request_cb),
				 window, 0);
	g_signal_connect_object (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), "ge-modal-alert",
				 G_CALLBACK (embed_modal_alert_cb), window, G_CONNECT_AFTER);

	/* Let the extensions attach themselves to the tab */
	manager = EPHY_EXTENSION (ephy_shell_get_extensions_manager (ephy_shell));
	ephy_extension_attach_tab (manager, window, embed);

        if (priv->present_on_insert)
        {
                priv->present_on_insert = FALSE;
                g_idle_add ((GSourceFunc) present_on_idle_cb, g_object_ref (window));
        }
}

static void
notebook_page_removed_cb (EphyNotebook *notebook,
			  EphyEmbed *embed,
			  guint position,
			  EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyExtension *manager;

	LOG ("page-removed notebook %p embed %p position %u\n", notebook, embed, position);

	if (priv->closing) return;

	g_return_if_fail (EPHY_IS_EMBED (embed));

	/* Let the extensions remove themselves from the tab */
	manager = EPHY_EXTENSION (ephy_shell_get_extensions_manager (ephy_shell));
	ephy_extension_detach_tab (manager, window, embed);

#if 0
	g_signal_handlers_disconnect_by_func (G_OBJECT (embed),
					      G_CALLBACK (ephy_link_open),
					      window);	
#endif

	priv->num_tabs--;

	if (priv->num_tabs > 0)
	{
		update_tabs_menu_sensitivity (window);
	}

	g_signal_handlers_disconnect_by_func
		(EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), G_CALLBACK (embed_modal_alert_cb), window);
	g_signal_handlers_disconnect_by_func
		(EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), G_CALLBACK (embed_close_request_cb), window);
}

static void
notebook_page_reordered_cb (EphyNotebook *notebook,
			    EphyEmbed *embed,
			    guint position,
			    EphyWindow *window)
{
	update_tabs_menu_sensitivity (window);
}

static void
notebook_page_close_request_cb (EphyNotebook *notebook,
				EphyEmbed *embed,
				EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_QUIT) &&
	    gtk_notebook_get_n_pages (priv->notebook) == 1)
	{
		return;
	}

	if (!ephy_web_view_has_modified_forms (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed)) ||
	    confirm_close_with_modified_forms (window))
	{
		gtk_widget_destroy (GTK_WIDGET (embed));
	}
}

static GtkWidget *
notebook_create_window_cb (GtkNotebook *notebook,
			   GtkWidget *page,
                           int x,
                           int y,
                           EphyWindow *window)
{
  EphyWindow *new_window;
  EphyWindowPrivate *new_priv;

  new_window = ephy_window_new ();
  new_priv = new_window->priv;

  new_priv->present_on_insert = TRUE;

  return ephy_window_get_notebook (new_window);
}

static GtkNotebook *
setup_notebook (EphyWindow *window)
{
	GtkNotebook *notebook;

	notebook = GTK_NOTEBOOK (g_object_new (EPHY_TYPE_NOTEBOOK, NULL));

	g_signal_connect_after (notebook, "switch-page",
				G_CALLBACK (notebook_switch_page_cb),
				window);
        g_signal_connect (notebook, "create-window",
                          G_CALLBACK (notebook_create_window_cb),
                          window);

	g_signal_connect (notebook, "popup-menu",
			  G_CALLBACK (notebook_popup_menu_cb), window);
	g_signal_connect (notebook, "button-press-event",
			  G_CALLBACK (notebook_button_press_cb), window);

	g_signal_connect (notebook, "page-added",
			  G_CALLBACK (notebook_page_added_cb), window);
	g_signal_connect (notebook, "page-removed",
			  G_CALLBACK (notebook_page_removed_cb), window);
	g_signal_connect (notebook, "page-reordered",
			  G_CALLBACK (notebook_page_reordered_cb), window);
	g_signal_connect (notebook, "tab-close-request",
			  G_CALLBACK (notebook_page_close_request_cb), window);

	return notebook;
}

static void
ephy_window_set_chrome (EphyWindow *window, EphyWebViewChrome mask)
{
	EphyWebViewChrome chrome_mask = mask;

	if (mask == EPHY_WEB_VIEW_CHROME_ALL)
	{
		window->priv->should_save_chrome = TRUE;
	}

	if (!eel_gconf_get_boolean (CONF_WINDOWS_SHOW_TOOLBARS))
	{
		chrome_mask &= ~EPHY_WEB_VIEW_CHROME_TOOLBAR;
	}

	if (!eel_gconf_get_boolean (CONF_WINDOWS_SHOW_STATUSBAR))
	{
		chrome_mask &= ~EPHY_WEB_VIEW_CHROME_STATUSBAR;
	}

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_HIDE_MENUBAR))
	{
		chrome_mask &= ~EPHY_WEB_VIEW_CHROME_MENUBAR;
	}

	window->priv->chrome = chrome_mask;
}

static void
ephy_window_set_is_popup (EphyWindow *window,
			  gboolean is_popup)
{
	EphyWindowPrivate *priv = window->priv;

	priv->is_popup = is_popup;

	g_object_notify (G_OBJECT (window), "is-popup");
}

static void
ephy_window_dispose (GObject *object)
{
	EphyWindow *window = EPHY_WINDOW (object);
	EphyWindowPrivate *priv = window->priv;
	GObject *single;
	GSList *popups;

	LOG ("EphyWindow dispose %p", window);

	/* Only do these once */
	if (window->priv->closing == FALSE)
	{
		EphyExtension *manager;

		window->priv->closing = TRUE;

		/* Let the extensions detach themselves from the window */
		manager = EPHY_EXTENSION (ephy_shell_get_extensions_manager (ephy_shell));
		ephy_extension_detach_window (manager, window);
		ephy_bookmarks_ui_detach_window (window);

		/* Deactivate menus */
		popups = gtk_ui_manager_get_toplevels (window->priv->manager, GTK_UI_MANAGER_POPUP);
		g_slist_foreach (popups, (GFunc) gtk_menu_shell_deactivate, NULL);
		g_slist_free (popups);
	
		single = ephy_embed_shell_get_embed_single (embed_shell);
		g_signal_handlers_disconnect_by_func
			(single, G_CALLBACK (sync_network_status), window);
	
		eel_gconf_notification_remove (priv->browse_with_caret_notifier_id);
		eel_gconf_notification_remove (priv->allow_popups_notifier_id);
		priv->browse_with_caret_notifier_id = 0;
		priv->allow_popups_notifier_id = 0;

		if (priv->idle_resize_handler != 0)
		{
			g_source_remove (priv->idle_resize_handler);
			priv->idle_resize_handler = 0;
		}

		g_hash_table_remove_all (priv->tabs_to_remove);

		g_object_unref (priv->enc_menu);
		priv->enc_menu = NULL;

		g_object_unref (priv->tabs_menu);
		priv->tabs_menu = NULL;

		if (priv->ppview_toolbar)
		{
			g_object_unref (priv->ppview_toolbar);
			priv->ppview_toolbar = NULL;
		}

		priv->action_group = NULL;
		priv->popups_action_group = NULL;

		g_object_unref (priv->manager);
		priv->manager = NULL;

		_ephy_window_set_context_event (window, NULL);
	}

	destroy_fullscreen_popup (window);

	G_OBJECT_CLASS (ephy_window_parent_class)->dispose (object);
}

static void
ephy_window_set_property (GObject *object,
			  guint prop_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	EphyWindow *window = EPHY_WINDOW (object);

	switch (prop_id)
	{
		case PROP_PPV_MODE:
			/* Read only */
			break;
		case PROP_ACTIVE_CHILD:
			impl_set_active_child (EPHY_EMBED_CONTAINER (window),
					       g_value_get_object (value));
			break;
		case PROP_CHROME:
			ephy_window_set_chrome (window, g_value_get_flags (value));
			break;
		case PROP_SINGLE_TAB_MODE:
			ephy_window_set_is_popup (window, g_value_get_boolean (value));
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec); 
			break;
	}
}

static void
ephy_window_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	EphyWindow *window = EPHY_WINDOW (object);

	switch (prop_id)
	{
		case PROP_ACTIVE_CHILD:
			g_value_set_object (value, window->priv->active_embed);
			break;
		case PROP_CHROME:
			g_value_set_flags (value, window->priv->chrome);
			break;
		case PROP_PPV_MODE:
			g_value_set_boolean (value, window->priv->ppv_mode);
			break;
		case PROP_SINGLE_TAB_MODE:
			g_value_set_boolean (value, window->priv->is_popup);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec); 
			break;
	}
}

static gboolean
ephy_window_focus_in_event (GtkWidget *widget,
			    GdkEventFocus *event)
{
	EphyWindow *window = EPHY_WINDOW (widget);
	EphyWindowPrivate *priv = window->priv;

	if (priv->fullscreen_popup && !get_toolbar_visibility (window))
	{
		gtk_widget_show (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (ephy_window_parent_class)->focus_in_event (widget, event);
}

static gboolean
ephy_window_focus_out_event (GtkWidget *widget,
			     GdkEventFocus *event)
{
	EphyWindow *window = EPHY_WINDOW (widget);
	EphyWindowPrivate *priv = window->priv;

	if (priv->fullscreen_popup)
	{
		gtk_widget_hide (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (ephy_window_parent_class)->focus_out_event (widget, event);
}

static gboolean
ephy_window_state_event (GtkWidget *widget,
			 GdkEventWindowState *event)
{
	EphyWindow *window = EPHY_WINDOW (widget);
	EphyWindowPrivate *priv = window->priv;
	gboolean (* window_state_event) (GtkWidget *, GdkEventWindowState *);

	window_state_event = GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event;
	if (window_state_event)
	{
		window_state_event (widget, event);
	}

	if (event->changed_mask & (GDK_WINDOW_STATE_MAXIMIZED))
	{
		gboolean show;

		show = (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) == 0;

		gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (priv->statusbar), show);
	}

	if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
	{
		GtkActionGroup *action_group;
		GtkAction *action;
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

		action_group = priv->action_group;

		action = gtk_action_group_get_action (action_group, "ViewFullscreen");
		g_signal_handlers_block_by_func
			(action, G_CALLBACK (window_cmd_view_fullscreen), window);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), fullscreen);
		g_signal_handlers_unblock_by_func
			(action, G_CALLBACK (window_cmd_view_fullscreen), window);
	}

	return FALSE;
}

static void
ephy_window_class_init (EphyWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructor = ephy_window_constructor;
	object_class->dispose = ephy_window_dispose;
	object_class->finalize = ephy_window_finalize;
	object_class->get_property = ephy_window_get_property;
	object_class->set_property = ephy_window_set_property;

	widget_class->show = ephy_window_show;
	widget_class->key_press_event = ephy_window_key_press_event;
	widget_class->focus_in_event = ephy_window_focus_in_event;
	widget_class->focus_out_event = ephy_window_focus_out_event;
	widget_class->window_state_event = ephy_window_state_event;
	widget_class->delete_event = ephy_window_delete_event;

	g_object_class_override_property (object_class,
					  PROP_ACTIVE_CHILD,
					  "active-child");

	g_object_class_override_property (object_class,
					  PROP_SINGLE_TAB_MODE,
					  "is-popup");

	g_object_class_override_property (object_class,
					  PROP_PPV_MODE,
					  "is-print-preview");

	g_object_class_override_property (object_class,
					  PROP_CHROME,
					  "chrome");

	g_type_class_add_private (object_class, sizeof (EphyWindowPrivate));
}

static void
browse_with_caret_notifier (GConfClient *client,
			    guint cnxn_id,
			    GConfEntry *entry,
			    EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkAction *action;
	gboolean enabled;

	enabled = eel_gconf_get_boolean (CONF_CARET_BROWSING_ENABLED);
	action = gtk_action_group_get_action (window->priv->action_group,
					      "BrowseWithCaret");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), enabled);

	ephy_statusbar_set_caret_mode (EPHY_STATUSBAR (priv->statusbar), enabled);
}

static void
allow_popups_notifier (GConfClient *client,
		       guint cnxn_id,
		       GConfEntry *entry,
		       EphyWindow *window)
{
	GList *tabs;
	EphyEmbed *embed;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));

	for (; tabs; tabs = g_list_next (tabs))
	{
		embed = EPHY_EMBED (tabs->data);
		g_return_if_fail (EPHY_IS_EMBED (embed));

		g_object_notify (G_OBJECT (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed)), "popups-allowed");
	}
}

static EphyEmbed *
ephy_window_open_link (EphyLink *link,
		       const char *address,
		       EphyEmbed *embed,
		       EphyLinkFlags flags)
{
	EphyWindow *window = EPHY_WINDOW (link);
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *new_embed;

	g_return_val_if_fail (address != NULL, NULL);

	/* don't do anything in ppv mode */
	if (window->priv->ppv_mode) return NULL;

	if (embed == NULL)
	{
		embed = window->priv->active_embed;
	}

	if (flags  & (EPHY_LINK_JUMP_TO | 
		      EPHY_LINK_NEW_TAB | 
		      EPHY_LINK_NEW_WINDOW))
	{
		EphyNewTabFlags ntflags = EPHY_NEW_TAB_OPEN_PAGE;

		if (flags & EPHY_LINK_JUMP_TO)
		{
			ntflags |= EPHY_NEW_TAB_JUMP;
		}
		if (flags & EPHY_LINK_NEW_WINDOW ||
		    (flags & EPHY_LINK_NEW_TAB && priv->is_popup))
		{
			ntflags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
		}
		else
		{
			ntflags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW;
		}
		if (flags & EPHY_LINK_ALLOW_FIXUP)
		{
			ntflags |= EPHY_NEW_TAB_ALLOW_FIXUP;
		}

		new_embed = ephy_shell_new_tab
				(ephy_shell,
				 EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
				 embed, address, ntflags);
	}
	else
	{
		ephy_web_view_load_url (EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed)), address);

		if (address == NULL || address[0] == '\0' || strcmp (address, "about:blank") == 0)
		{
			ephy_toolbar_activate_location (priv->toolbar);
		}
		else
		{
			gtk_widget_grab_focus (GTK_WIDGET (embed));
		}

		new_embed = embed;
	}

	return new_embed;
}

static void
find_toolbar_close_cb (EphyFindToolbar *toolbar,
		       EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *embed;

	if (priv->closing) return;

	ephy_find_toolbar_close (priv->find_toolbar);

	embed = priv->active_embed;
	if (embed == NULL) return;

	gtk_widget_grab_focus (GTK_WIDGET (embed));
}

static void
cancel_handler (gpointer idptr)
{
	guint id = GPOINTER_TO_UINT (idptr);

	g_source_remove (id);
}

static void
ephy_window_init (EphyWindow *window)
{
	LOG ("EphyWindow initialising %p", window);

	g_object_ref (ephy_shell);

	window->priv = EPHY_WINDOW_GET_PRIVATE (window);
}

static GObject *
ephy_window_constructor (GType type,
			 guint n_construct_properties,
			 GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyWindow *window;
	EphyWindowPrivate *priv;
	EphyExtension *manager;
	EphyEmbedSingle *single;
	EggToolbarsModel *model;
	GtkSettings *settings;
	GtkAction *action;
	GtkActionGroup *toolbar_action_group;
	GError *error = NULL;
	guint settings_connection;
	GSList *proxies;
	GtkWidget *proxy;

	object = G_OBJECT_CLASS (ephy_window_parent_class)->constructor
		(type, n_construct_properties, construct_params);

	window = EPHY_WINDOW (object);

	priv = window->priv;

	priv->tabs_to_remove = g_hash_table_new_full (g_direct_hash, g_direct_equal,
						              NULL, cancel_handler);

	ephy_gui_ensure_window_group (GTK_WINDOW (window));

	/* initialize the listener for the key theme
	 * FIXME: Need to handle multi-head and migration.
	 */
	settings = gtk_settings_get_default ();
	settings_connection = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (settings),
								   SETTINGS_CONNECTION_DATA_KEY));
	if (settings_connection == 0)
	{
		settings_connection =
			g_signal_connect (settings, "notify::gtk-key-theme-name",
					  G_CALLBACK (settings_changed_cb), NULL);
		g_object_set_data (G_OBJECT (settings), SETTINGS_CONNECTION_DATA_KEY,
				   GUINT_TO_POINTER (settings_connection));

	}

	settings_change_notify (settings, window);

	/* Setup the UI manager and connect verbs */
	setup_ui_manager (window);

	priv->notebook = setup_notebook (window);
	g_signal_connect_swapped (priv->notebook, "open-link",
				  G_CALLBACK (ephy_link_open), window);
	gtk_box_pack_start (GTK_BOX (priv->main_vbox),
			    GTK_WIDGET (priv->notebook),
			    TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (priv->notebook));
	ephy_notebook_set_dnd_enabled (EPHY_NOTEBOOK (priv->notebook), !priv->is_popup);

	priv->find_toolbar = ephy_find_toolbar_new (window);
	g_signal_connect (priv->find_toolbar, "close",
			  G_CALLBACK (find_toolbar_close_cb), window);
	gtk_box_pack_start (GTK_BOX (priv->main_vbox),
			    GTK_WIDGET (priv->find_toolbar), FALSE, FALSE, 0);
	/* don't show the find toolbar here! */
	
	priv->statusbar = ephy_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (priv->main_vbox),
			  GTK_WIDGET (priv->statusbar),
			  FALSE, TRUE, 0);
	priv->tab_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (priv->statusbar), "tab_message");
	priv->help_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (priv->statusbar), "help_message");

	/* get the toolbars model *before* getting the bookmarksbar model
	 * (via ephy_bookmarsbar_new()), so that the toolbars model is
	 * instantiated *before* the bookmarksbarmodel, to make forwarding
	 * works. See bug #151267.
	 */
	model= EGG_TOOLBARS_MODEL (ephy_shell_get_toolbars_model (ephy_shell, FALSE));

	/* create the toolbars */
	priv->toolbar = ephy_toolbar_new (window);
	g_signal_connect_swapped (priv->toolbar, "open-link",
				  G_CALLBACK (ephy_link_open), window);
	g_signal_connect_swapped (priv->toolbar, "exit-clicked",
				  G_CALLBACK (exit_fullscreen_clicked_cb), window);
	g_signal_connect_swapped (priv->toolbar, "activation-finished",
				  G_CALLBACK (sync_chromes_visibility), window);

	/* now load the UI definition */
	gtk_ui_manager_add_ui_from_file
		(priv->manager, ephy_file ("epiphany-ui.xml"), &error);
	if (error != NULL)
	{
		g_warning ("Could not merge epiphany-ui.xml: %s", error->message);
		g_error_free (error);
	}
#if ENABLE_CERTIFICATE_MANAGER
{
	guint ui_id;
	ui_id = gtk_ui_manager_new_merge_id (priv->manager);
	gtk_ui_manager_add_ui (priv->manager, ui_id,
			       "/menubar/EditMenu/EditPersonalDataMenu",
			       "EditCertificates", "EditCertificates",
			       GTK_UI_MANAGER_MENUITEM, FALSE);
}
#endif

	/* Initialize the menus */
	priv->tabs_menu = ephy_tabs_menu_new (window);
	priv->enc_menu = ephy_encoding_menu_new (window);

	/* Add the toolbars to the window */
	gtk_box_pack_end (GTK_BOX (priv->menu_dock),
			  GTK_WIDGET (priv->toolbar),
			  FALSE, FALSE, 0);

	/* Once the window is sufficiently created let the extensions attach to it */
	manager = EPHY_EXTENSION (ephy_shell_get_extensions_manager (ephy_shell));
	ephy_extension_attach_window (manager, window);
	ephy_bookmarks_ui_attach_window (window);

	/* We only set the model now after attaching the extensions, so that
	 * extensions already have created their actions which may be on
	 * the toolbar
	 */
	egg_editable_toolbar_set_model
		(EGG_EDITABLE_TOOLBAR (priv->toolbar), model);

	/* other notifiers */
	browse_with_caret_notifier (NULL, 0, NULL, window);
	priv->browse_with_caret_notifier_id = eel_gconf_notification_add
		(CONF_CARET_BROWSING_ENABLED,
		 (GConfClientNotifyFunc)browse_with_caret_notifier, window);

	priv->allow_popups_notifier_id = eel_gconf_notification_add
		(CONF_SECURITY_ALLOW_POPUPS,
		 (GConfClientNotifyFunc)allow_popups_notifier, window);

	/* network status */
	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	sync_network_status (single, NULL, window);
	g_signal_connect (single, "notify::network-status",
			  G_CALLBACK (sync_network_status), window);

	/* Popup part */
	toolbar_action_group = ephy_toolbar_get_action_group (priv->toolbar);
	action = gtk_action_group_get_action (toolbar_action_group, "FileNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, priv->is_popup);

	action = gtk_action_group_get_action (priv->popups_action_group, "OpenLinkInNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, priv->is_popup);

	/* Connect lock clicks */
	action = gtk_action_group_get_action (priv->action_group, "ViewPageSecurityInfo");
	g_signal_connect_swapped (priv->statusbar, "lock-clicked",
				  G_CALLBACK (gtk_action_activate), action);
	g_signal_connect_swapped (priv->toolbar, "lock-clicked",
				  G_CALLBACK (gtk_action_activate), action);

	/* ensure the UI is updated */
	gtk_ui_manager_ensure_update (priv->manager);

	init_menu_updaters (window);

	update_chromes_actions (window);

	sync_chromes_visibility (window);

	/* Cache GtkEntry inside EphyLocationEntry */
	action = gtk_action_group_get_action (toolbar_action_group,
					      "Location");
	proxies = gtk_action_get_proxies (action);
	proxy = GTK_WIDGET (proxies->data);
	priv->entry = ephy_location_entry_get_entry (EPHY_LOCATION_ENTRY (proxy));
	
	return object;
}

static void
ephy_window_finalize (GObject *object)
{
	EphyWindow *window = EPHY_WINDOW (object);
	EphyWindowPrivate *priv = window->priv;

	g_hash_table_destroy (priv->tabs_to_remove);

	if (priv->clear_progress_timeout_id)
		g_source_remove (priv->clear_progress_timeout_id);

	G_OBJECT_CLASS (ephy_window_parent_class)->finalize (object);

	LOG ("EphyWindow finalised %p", object);

	g_object_unref (ephy_shell);
}

/**
 * ephy_window_new:
 *
 * Equivalent to g_object_new() but returns an #EphyWindow so you don't have
 * to cast it.
 *
 * Return value: a new #EphyWindow
 **/
EphyWindow *
ephy_window_new (void)
{
	return EPHY_WINDOW (g_object_new (EPHY_TYPE_WINDOW, NULL));
}

/**
 * ephy_window_new_with_chrome:
 * @chrome: an #EphyWebViewChrome
 * @is_popup: whether the new window is a popup window
 *
 * Identical to ephy_window_new(), but allows you to specify a chrome.
 *
 * Return value: a new #EphyWindow
 **/
EphyWindow *
ephy_window_new_with_chrome (EphyWebViewChrome chrome,
			     gboolean is_popup)
{
	return EPHY_WINDOW (g_object_new (EPHY_TYPE_WINDOW,
					  "chrome", chrome,
					  "is-popup", is_popup,
					  NULL));
}

/**
 * ephy_window_get_ui_manager:
 * @window: an #EphyWindow
 *
 * Returns this window's UI manager.
 *
 * Return value: an #GtkUIManager
 **/
GObject *
ephy_window_get_ui_manager (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return G_OBJECT (window->priv->manager);
}

/**
 * ephy_window_get_toolbar:
 * @window: an #EphyWindow
 *
 * Returns this window's toolbar as an #EggEditableToolbar.
 *
 * Return value: an #EggEditableToolbar
 **/
GtkWidget *
ephy_window_get_toolbar (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return GTK_WIDGET (window->priv->toolbar);
}

/**
 * ephy_window_get_notebook:
 * @window: an #EphyWindow
 *
 * Returns the #GtkNotebook used by this window.
 *
 * Return value: the @window's #GtkNotebook
 **/
GtkWidget *
ephy_window_get_notebook (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return GTK_WIDGET (window->priv->notebook);
}

/**
 * ephy_window_get_find_toolbar:
 * @window: an #EphyWindow
 *
 * Returns the #EphyFindToolbar used by this window.
 *
 * Return value: the @window's #EphyFindToolbar
 **/
GtkWidget *
ephy_window_get_find_toolbar (EphyWindow *window)
{
       g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

       return GTK_WIDGET (window->priv->find_toolbar);
}

/**
 * ephy_window_get_statusbar:
 * @window: an #EphyWindow
 *
 * Returns this window's statusbar as an #EphyStatusbar.
 *
 * Return value: This window's statusbar
 **/
GtkWidget *
ephy_window_get_statusbar (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return GTK_WIDGET (window->priv->statusbar);
}

static EphyEmbed *
real_get_active_tab (EphyWindow *window, int page_num)
{
	GtkWidget *embed;

	if (page_num == -1)
	{
		page_num = gtk_notebook_get_current_page (window->priv->notebook);
	}
	embed = gtk_notebook_get_nth_page (window->priv->notebook, page_num);

	g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

	return EPHY_EMBED (embed);
}

/**
 * ephy_window_load_url:
 * @window: a #EphyWindow
 * @url: the url to load
 *
 * Loads a new url in the active tab of @window.
 * Unlike ephy_web_view_load_url(), this function activates
 * the embed.
 *
 **/
void
ephy_window_load_url (EphyWindow *window,
		      const char *url)
{
	g_return_if_fail (url != NULL);

	ephy_link_open (EPHY_LINK (window), url, NULL, 0);
}

/**
 * ephy_window_activate_location:
 * @window: an #EphyWindow
 *
 * Activates the location entry on @window's toolbar.
 **/
void
ephy_window_activate_location (EphyWindow *window)
{
	if (window->priv->fullscreen_popup)
	{
		gtk_widget_hide (window->priv->fullscreen_popup);
	}

	ephy_toolbar_activate_location (window->priv->toolbar);
}

static void
ephy_window_show (GtkWidget *widget)
{
	EphyWindow *window = EPHY_WINDOW(widget);
	EphyWindowPrivate *priv = window->priv;

	if (!priv->has_size)
	{
		EphyEmbed *embed;
		int width, height;

		embed = priv->active_embed;
		g_return_if_fail (EPHY_IS_EMBED (embed));

		ephy_tab_get_size (embed, &width, &height);
		if (width == -1 && height == -1)
		{
			ephy_state_add_window (widget, "main_window", 600, 500,
					       TRUE, EPHY_STATE_WINDOW_SAVE_SIZE);
		}

		priv->has_size = TRUE;
	}

	GTK_WIDGET_CLASS (ephy_window_parent_class)->show (widget);
}

static void
notebook_switch_page_cb (GtkNotebook *notebook,
			 GtkNotebookPage *page,
			 guint page_num,
			 EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *embed;

	LOG ("switch-page notebook %p position %u\n", notebook, page_num);

	if (priv->closing) return;

	/* get the new tab */
	embed = real_get_active_tab (window, page_num);

	/* update new tab */
	ephy_window_set_active_tab (window, embed);

	ephy_find_toolbar_set_embed (priv->find_toolbar, embed);

	/* update window controls */
	update_tabs_menu_sensitivity (window);
}

/**
 * ephy_window_set_zoom:
 * @window: an #EphyWindow
 * @zoom: the desired zoom level
 *
 * Sets the zoom on @window's active #EphyEmbed. A @zoom of 1.0 corresponds to
 * 100% zoom (normal size).
 **/
void
ephy_window_set_zoom (EphyWindow *window,
		      float zoom)
{
	EphyEmbed *embed;
	float current_zoom = 1.0;
	WebKitWebView *web_view;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	embed = window->priv->active_embed;
	g_return_if_fail (embed != NULL);

	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	g_object_get (web_view, "zoom-level", &current_zoom, NULL);

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
		g_object_set (G_OBJECT (web_view), "zoom-level", zoom, NULL);
	}
}

static void
sync_prefs_with_chrome (EphyWindow *window)
{
	EphyWebViewChrome flags = window->priv->chrome;

	if (window->priv->should_save_chrome)
	{
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_TOOLBARS,
				       flags & EPHY_WEB_VIEW_CHROME_TOOLBAR);
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_STATUSBAR,
				       flags & EPHY_WEB_VIEW_CHROME_STATUSBAR);
	}
}

static void
sync_chrome_with_view_toggle (GtkAction *action,
			      EphyWindow *window,
			      EphyWebViewChrome chrome_flag,
			      gboolean invert)
{
	gboolean active;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	window->priv->chrome = (active != invert) ?
	  				window->priv->chrome | chrome_flag :
	  				window->priv->chrome & (~chrome_flag);

	sync_chromes_visibility (window);
	sync_prefs_with_chrome (window);
}

static void
ephy_window_view_statusbar_cb (GtkAction *action,
			       EphyWindow *window)
{
	sync_chrome_with_view_toggle (action, window,
				      EPHY_WEB_VIEW_CHROME_STATUSBAR, FALSE);
}

static void
ephy_window_view_toolbar_cb (GtkAction *action,
			     EphyWindow *window)
{
	sync_chrome_with_view_toggle (action, window,
				      EPHY_WEB_VIEW_CHROME_TOOLBAR, TRUE);
}

static void
ephy_window_view_popup_windows_cb (GtkAction *action,
				   EphyWindow *window)
{
	EphyEmbed *embed;
	gboolean allow;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	embed = window->priv->active_embed;
	g_return_if_fail (EPHY_IS_EMBED (embed));

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
	{
		allow = TRUE;
	}
	else
	{
		allow = FALSE;
	}

	g_object_set (G_OBJECT (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed)), "popups-allowed", allow, NULL);
}

/**
 * ephy_window_get_is_print_preview:
 * @window: an #EphyWindow
 *
 * Returns whether this window is in print preview mode.
 *
 * Return value: %TRUE if it is in print preview mode
 **/
gboolean
ephy_window_get_is_print_preview (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), FALSE);

	return window->priv->ppv_mode;
}

/**
 * ephy_window_get_context_event:
 * @window: an #EphyWindow
 *
 * Returns the #EphyEmbedEvent for the current context menu.
 * Use this to get the event from the action callback.
 *
 * Return value: an #EphyEmbedEvent, or %NULL
 **/
EphyEmbedEvent *
ephy_window_get_context_event (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return window->priv->context_event;
}
