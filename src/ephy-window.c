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
 *  $Id$
 */

#include "config.h"

#include "ephy-window.h"
#include "ephy-type-builtins.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-command-manager.h"
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
#include "ephy-embed-factory.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>

#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

#ifdef ENABLE_PYTHON
#include "ephy-python.h"
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
static void sync_tab_load_status		(EphyEmbed  *embed,
						 GParamSpec *pspec,
						 EphyWindow *window);
static void sync_tab_security			(EphyEmbed  *embed,
						 GParamSpec *pspec,
						 EphyWindow *window);
static void sync_tab_zoom			(EphyEmbed *embed,
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
	{ "FilePrintSetup", STOCK_PRINT_SETUP, N_("Print Set_up…"), NULL,
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
#ifdef ENABLE_CERTIFICATE_MANAGER
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
	EphyEmbedChrome chrome;
	guint idle_resize_handler;
	GHashTable *tabs_to_remove;
	EphyEmbedEvent *context_event;
	guint idle_worker;

	guint browse_with_caret_notifier_id;
	guint allow_popups_notifier_id;

	guint menubar_accel_keyval;
	guint menubar_accel_modifier;

	guint closing : 1;
	guint has_size : 1;
	guint fullscreen_mode : 1;
	guint ppv_mode : 1;
	guint should_save_chrome : 1;
	guint is_popup : 1;

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

	gtk_window_group_add_window (GTK_WINDOW (window)->group, GTK_WINDOW (dialog));

	return dialog;
}

static gboolean
confirm_close_with_modified_forms (EphyWindow *window)
{
	GtkWidget *dialog;
	int response;

	dialog = construct_confirm_close_dialog (window);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return response == GTK_RESPONSE_ACCEPT;
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

	modified = ephy_embed_has_modified_forms (child);
	if (ephy_embed_has_modified_forms (child)
	    && confirm_close_with_modified_forms (window) == FALSE)
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

static EphyEmbedChrome
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
	return ((window->priv->chrome & EPHY_EMBED_CHROME_TOOLBAR) != 0) &&
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
	EphyEmbedChrome flags = priv->chrome;

	if (window->priv->ppv_mode)
	{
		*show_menubar = *show_statusbar
			      = *show_toolbar
			      = *show_tabsbar
			      = FALSE;
	}
	else if (window->priv->fullscreen_mode)
	{
		*show_toolbar = (flags & EPHY_EMBED_CHROME_TOOLBAR) != 0;
		*show_menubar = *show_statusbar = FALSE;
		*show_tabsbar = !priv->is_popup;
	}
	else
	{
		*show_menubar = (flags & EPHY_EMBED_CHROME_MENUBAR) != 0;
		*show_statusbar = (flags & EPHY_EMBED_CHROME_STATUSBAR) != 0;
		*show_toolbar = (flags & EPHY_EMBED_CHROME_TOOLBAR) != 0;
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
	sync_tab_load_status (embed, NULL, window);
	sync_tab_security (embed, NULL, window);

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

		if (!GTK_WIDGET_VISIBLE (menubar))
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
		ephy_embed_set_print_preview_mode (embed, FALSE);

		_ephy_window_set_print_preview (window, FALSE);

		return TRUE;
	}

	tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyEmbed *embed = (EphyEmbed *) l->data;

		g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);

		if (ephy_embed_has_modified_forms (embed))
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

		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (widget), NULL, NULL);

		can_copy = has_selection;
		can_cut = has_selection;
		can_paste = TRUE;
		can_undo = FALSE;
		can_redo = FALSE;
	}
	else
	{
		EphyEmbed *embed;

		embed = window->priv->active_embed;
		g_return_if_fail (embed != NULL);

		can_copy = ephy_command_manager_can_do_command
				(EPHY_COMMAND_MANAGER (embed), "cmd_copy");
		can_cut = ephy_command_manager_can_do_command
				(EPHY_COMMAND_MANAGER (embed), "cmd_cut");
		can_paste = ephy_command_manager_can_do_command
				(EPHY_COMMAND_MANAGER (embed), "cmd_paste");
		can_undo = ephy_command_manager_can_do_command
				(EPHY_COMMAND_MANAGER (embed), "cmd_undo");
		can_redo = ephy_command_manager_can_do_command
				(EPHY_COMMAND_MANAGER (embed), "cmd_redo");
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

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
	g_return_if_fail (action != NULL);
	
	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
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
		
		action = g_object_get_data (G_OBJECT (item),  "gtk-action");
		g_return_val_if_fail (action != NULL, FALSE);
		
		g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
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
sync_tab_address (EphyEmbed *embed,
	          GParamSpec *pspec,
		  EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->closing) return;

	ephy_toolbar_set_location (priv->toolbar,
				   ephy_embed_get_address (embed),
				   ephy_embed_get_typed_address (embed));
	ephy_find_toolbar_request_close (priv->find_toolbar);
}

static void
sync_tab_document_type (EphyEmbed *embed,
			GParamSpec *pspec,
			EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group = priv->action_group;
	GtkAction *action;
	EphyEmbedDocumentType type;
	gboolean can_find, disable, is_image;

	if (priv->closing) return;

	/* update zoom actions */
	sync_tab_zoom (embed, NULL, window);
	
	type = ephy_embed_get_document_type (embed);
	can_find = (type != EPHY_EMBED_DOCUMENT_IMAGE);
	is_image = type == EPHY_EMBED_DOCUMENT_IMAGE;
	disable = (type != EPHY_EMBED_DOCUMENT_HTML);

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
sync_tab_icon (EphyEmbed *embed,
	       GParamSpec *pspec,
	       EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GdkPixbuf *icon;

	if (priv->closing) return;

	icon = ephy_embed_get_icon (embed);

	ephy_toolbar_set_favicon (priv->toolbar, icon);
}

static void
sync_tab_load_progress (EphyEmbed *embed, GParamSpec *pspec, EphyWindow *window)
{
	if (window->priv->closing) return;

	ephy_statusbar_set_progress (EPHY_STATUSBAR (window->priv->statusbar),
				     ephy_embed_get_load_percent (embed));
}

static void
sync_tab_message (EphyEmbed *embed, GParamSpec *pspec, EphyWindow *window)
{
	GtkStatusbar *s = GTK_STATUSBAR (window->priv->statusbar);
	const char *message;

	if (window->priv->closing) return;

	message = ephy_embed_get_status_message (embed);

	gtk_statusbar_pop (s, window->priv->tab_message_cid);

	if (message)
	{
		gtk_statusbar_push (s, window->priv->tab_message_cid, message);
	}
}

static void
sync_tab_navigation (EphyEmbed *embed,
		     GParamSpec *pspec,
		     EphyWindow *window)
{
	EphyEmbedNavigationFlags flags;
	EphyHistoryItem *item;
	gboolean up = FALSE, back = FALSE, forward = FALSE;
	char *back_title = NULL, *forward_title = NULL;

	if (window->priv->closing) return;

	flags = ephy_embed_get_navigation_flags (embed);

	if (flags & EPHY_EMBED_NAV_UP)
	{
		up = TRUE;
	}
	if (flags & EPHY_EMBED_NAV_BACK)
	{
		back = TRUE;
	}
	if (flags & EPHY_EMBED_NAV_FORWARD)
	{
		forward = TRUE;
	}

	ephy_toolbar_set_navigation_actions (window->priv->toolbar,
					     back, forward, up);

	item = ephy_embed_get_previous_history_item (embed);

	if (item)
	{
		back_title = ephy_history_item_get_title (item);
		g_object_unref (item);
	}


	item = ephy_embed_get_next_history_item (embed);

	if (item)
	{
		forward_title = ephy_history_item_get_title (item);
		g_object_unref (item);
	}

	ephy_toolbar_set_navigation_tooltips (window->priv->toolbar,
					      back_title,
					      forward_title);

	g_free (back_title);
	g_free (forward_title);
}

static void
sync_tab_security (EphyEmbed *embed,
		   GParamSpec *pspec,
		   EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyEmbedSecurityLevel level;
	char *description = NULL;
	char *state = NULL;
	char *tooltip;
	const char *stock_id = STOCK_LOCK_INSECURE;
	gboolean show_lock = FALSE, is_secure = FALSE;
	GtkAction *action;

	if (priv->closing) return;

	ephy_embed_get_security_level (embed, &level, &description);

	switch (level)
	{
		case EPHY_EMBED_STATE_IS_UNKNOWN:
			state = _("Unknown");
			break;
		case EPHY_EMBED_STATE_IS_INSECURE:
			state = _("Insecure");
			g_free (description);
			description = NULL;
			break;
		case EPHY_EMBED_STATE_IS_BROKEN:
			state = _("Broken");
			stock_id = STOCK_LOCK_BROKEN;
			show_lock = TRUE;

#ifndef HAVE_GECKO_1_8_1
			/* In Gecko 1.8.1/1.9, we get a useful tooltip here */
			g_free (description);
			description = NULL;
#endif
			break;
		case EPHY_EMBED_STATE_IS_SECURE_LOW:
		case EPHY_EMBED_STATE_IS_SECURE_MED:
			state = _("Low");
			/* We deliberately don't show the 'secure' icon
			 * for low & medium secure sites; see bug #151709.
			 */
			stock_id = STOCK_LOCK_INSECURE;
			break;
		case EPHY_EMBED_STATE_IS_SECURE_HIGH:
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
sync_tab_popup_windows (EphyEmbed *embed,
			GParamSpec *pspec,
			EphyWindow *window)
{
	guint num_popups = 0;
	char *tooltip = NULL;

	g_object_get (G_OBJECT (embed),
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
sync_tab_popups_allowed (EphyEmbed *embed,
			 GParamSpec *pspec,
			 EphyWindow *window)
{
	GtkAction *action;
	gboolean allow;

	g_return_if_fail (EPHY_IS_EMBED (embed));
	g_return_if_fail (EPHY_IS_WINDOW (window));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewPopupWindows");
	g_return_if_fail (GTK_IS_ACTION (action));

	g_object_get (G_OBJECT (embed), "popups-allowed", &allow, NULL);

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
sync_tab_load_status (EphyEmbed  *embed,
		      GParamSpec *pspec,
		      EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group = priv->action_group;
	GtkAction *action;
	gboolean loading;

	if (window->priv->closing) return;

	loading = ephy_embed_get_load_status (embed);

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
sync_tab_title (EphyEmbed *embed,
		GParamSpec *pspec,
		EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->closing) return;

	gtk_window_set_title (GTK_WINDOW(window),
			      ephy_embed_utils_get_title_composite (embed));
}

static void
sync_tab_zoom (EphyEmbed *embed, GParamSpec *pspec, EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	EphyEmbedDocumentType type;
	gboolean can_zoom_in = TRUE, can_zoom_out = TRUE, can_zoom_normal = FALSE, can_zoom;
	float zoom;

	if (window->priv->closing) return;

	zoom = ephy_embed_get_zoom (embed);
	type = ephy_embed_get_document_type (embed);
	can_zoom = (type != EPHY_EMBED_DOCUMENT_IMAGE);

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
get_name_from_address_value (const GValue *value)
{
	char *name;

	name = g_path_get_basename (g_value_get_string (value));

	return name != NULL ? name : g_strdup ("");
}

static void
update_popups_tooltips (EphyWindow *window, EphyEmbedEvent *event)
{
	EphyEmbedEventContext context;
	GtkActionGroup *group = window->priv->popups_action_group;
	const GValue *value;
	GtkAction *action;
	char *tooltip, *name;

	context = ephy_embed_event_get_context (event);

	if (context & EPHY_EMBED_CONTEXT_IMAGE)
	{
		value = ephy_embed_event_get_property (event, "image");
		name = get_name_from_address_value (value);

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
		tooltip = g_strdup_printf (_("Copy image address “%s”"),
					   g_value_get_string (value));
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);		

		g_free (name);
	}

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

	if (context & EPHY_EMBED_CONTEXT_LINK)
	{
		value = ephy_embed_event_get_property (event, "link");

		action = gtk_action_group_get_action (group, "DownloadLink");
		name = get_name_from_address_value (value);
		tooltip = g_strdup_printf (_("Save link “%s”"), name);
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (name);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "BookmarkLink");
		tooltip = g_strdup_printf (_("Bookmark link “%s”"),
					   g_value_get_string (value));
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);

		action = gtk_action_group_get_action (group, "CopyLinkAddress");
		tooltip = g_strdup_printf (_("Copy link's address “%s”"),
					   g_value_get_string (value));
		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);
	}
}

static void
show_embed_popup (EphyWindow *window,
		  EphyEmbed *embed,
		  EphyEmbedEvent *event)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group;
	GtkAction *action;
	EphyEmbedEventContext context;
	const char *popup;
	const GValue *value;
	gboolean framed, can_open_in_new;
	GtkWidget *widget;
	guint button;

	/* Do not show the menu in print preview mode */
	if (priv->ppv_mode)
	{
		return;
	}

	value = ephy_embed_event_get_property (event, "framed_page");
	framed = g_value_get_int (value);

	can_open_in_new = ephy_embed_event_has_property (event, "link-has-web-scheme");

	context = ephy_embed_event_get_context (event);

	LOG ("show_embed_popup context %x", context);

	if (context & EPHY_EMBED_CONTEXT_EMAIL_LINK)
	{
		popup = "/EphyEmailLinkPopup";
		update_edit_actions_sensitivity (window, TRUE);
	}
	else if (context & EPHY_EMBED_CONTEXT_LINK)
	{
		popup = "/EphyLinkPopup";
		update_edit_actions_sensitivity (window, TRUE);
	}
	else if (context & EPHY_EMBED_CONTEXT_INPUT)
	{
		popup = "/EphyInputPopup";
		update_edit_actions_sensitivity (window, FALSE);
	}
	else
	{
		popup = "/EphyDocumentPopup";
		update_edit_actions_sensitivity (window, TRUE);
	}

	update_popups_tooltips (window, event);

	widget = gtk_ui_manager_get_widget (priv->manager, popup);
	g_return_if_fail (widget != NULL);

	action_group = window->priv->popups_action_group;

	action = gtk_action_group_get_action (action_group, "OpenLinkInNewWindow");
	gtk_action_set_sensitive (action, can_open_in_new);

	action = gtk_action_group_get_action (action_group, "OpenLinkInNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CONTEXT, !can_open_in_new);

	
	update_popup_actions_visibility (window,
					 context & EPHY_EMBED_CONTEXT_IMAGE,
					 framed);

	_ephy_window_set_context_event (window, event);

	g_signal_connect (widget, "deactivate",
			  G_CALLBACK (embed_popup_deactivate_cb), window);

	button = ephy_embed_event_get_button (event);

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

static gboolean
tab_context_menu_cb (EphyEmbed *embed,
		     EphyEmbedEvent *event,
		     EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);
	g_return_val_if_fail (window->priv->active_embed == embed, FALSE);

	show_embed_popup (window, embed, event);

	return TRUE;
}

static gboolean
open_link_in_new (EphyWindow *window,
		  const char *link_address,
		  guint state,
		  EphyEmbed *embed)
{
	EphyEmbed *dest;

	if (!ephy_embed_utils_address_has_web_scheme (link_address)) return FALSE;

	dest = ephy_link_open (EPHY_LINK (window), link_address, embed,
			       state & GDK_SHIFT_MASK ? EPHY_LINK_NEW_WINDOW
						      : EPHY_LINK_NEW_TAB);

	if (dest)
	{
		ephy_embed_shistory_copy (embed,
					  dest,
					  TRUE,   /* back history */
					  FALSE,  /* forward history */
					  FALSE); /* current index */
		return TRUE;
	}

	return FALSE;
}

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
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_flags (persist, 0);
	ephy_embed_persist_set_persist_key (persist, key);
	ephy_embed_persist_set_source (persist, location);

	ephy_embed_persist_save (persist);

	g_object_unref (G_OBJECT(persist));

	return TRUE;
}

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
ephy_window_dom_mouse_click_cb (EphyEmbed *embed,
				EphyEmbedEvent *event,
				EphyWindow *window)
{
	EphyEmbedEventContext context;
	guint button, modifier;
	gboolean handled = TRUE;
	gboolean with_control, with_shift, with_shift_control;
	gboolean is_left_click, is_middle_click;
	gboolean is_link, is_image, is_middle_clickable;
	gboolean middle_click_opens;
	gboolean is_input;

	g_return_val_if_fail (EPHY_IS_EMBED_EVENT (event), FALSE);

	button = ephy_embed_event_get_button (event);
	context = ephy_embed_event_get_context (event);
	modifier = ephy_embed_event_get_modifier (event);

	LOG ("ephy_window_dom_mouse_click_cb: button %d, context %x, modifier %x",
	     button, context, modifier);

	with_control = (modifier & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
	with_shift = (modifier & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;
	with_shift_control = (modifier & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
					== (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
	is_left_click = (button == 1);
	is_middle_click = (button == 2);

	middle_click_opens =
		eel_gconf_get_boolean (CONF_INTERFACE_MIDDLE_CLICK_OPEN_URL) &&
		!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL);

	is_link = (context & EPHY_EMBED_CONTEXT_LINK) != 0;
	is_image = (context & EPHY_EMBED_CONTEXT_IMAGE) != 0;
	is_middle_clickable = !((context & EPHY_EMBED_CONTEXT_LINK)
				|| (context & EPHY_EMBED_CONTEXT_INPUT)
				|| (context & EPHY_EMBED_CONTEXT_EMAIL_LINK));
	is_input = (context & EPHY_EMBED_CONTEXT_INPUT) != 0;

	/* ctrl+click or middle click opens the link in new tab */
	if (is_link &&
	    ((is_left_click && (with_control || with_shift_control)) ||
	     is_middle_click))
	{
		const GValue *value;
		const char *link_address;

		value = ephy_embed_event_get_property (event, "link");
		link_address = g_value_get_string (value);
		handled = open_link_in_new (window, link_address, modifier, embed);
	}
	/* shift+click saves the link target */
	else if (is_link && is_left_click && with_shift)
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
	/* middle click opens the selection url */
	else if (is_middle_clickable && is_middle_click && middle_click_opens)
	{
		/* See bug #133633 for why we do it this way */

		/* We need to make sure we know if the embed is destroyed between
		 * requesting the clipboard contents, and receiving them.
		 */
		ClipboardTextCBData *cb_data;
		EphyEmbed **embed_ptr;
		
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

	visibility = ephy_embed_get_visibility (embed);

	if (visibility)
		gtk_widget_show (GTK_WIDGET (window));
	else
		gtk_widget_hide (GTK_WIDGET (window));
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
		embed = old_embed;

		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_popup_windows),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_popups_allowed),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_security),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_document_type),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_zoom),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_load_progress),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_load_status),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_navigation),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_title),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_address),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_icon),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (sync_tab_message),
						      window);
		g_signal_handlers_disconnect_by_func (embed,
						      G_CALLBACK (ephy_window_visibility_cb),
						      window);

		g_signal_handlers_disconnect_by_func
			(embed, G_CALLBACK (tab_context_menu_cb), window);
		g_signal_handlers_disconnect_by_func
			(embed, G_CALLBACK (ephy_window_dom_mouse_click_cb), window);

	}

	window->priv->active_embed = new_embed;

	if (new_embed != NULL)
	{
		embed = new_embed;

		sync_tab_security	(embed, NULL, window);
		sync_tab_document_type	(embed, NULL, window);
		sync_tab_zoom		(embed, NULL, window);
		sync_tab_load_progress	(embed, NULL, window);
		sync_tab_load_status	(embed, NULL, window);
		sync_tab_navigation	(embed, NULL, window);
		sync_tab_title		(embed, NULL, window);
		sync_tab_address	(embed, NULL, window);
		sync_tab_icon		(embed, NULL, window);
		sync_tab_message	(embed, NULL, window);
		sync_tab_popup_windows	(embed, NULL, window);
		sync_tab_popups_allowed	(embed, NULL, window);

		g_signal_connect_object (embed, "notify::hidden-popup-count",
					 G_CALLBACK (sync_tab_popup_windows),
					 window, 0);
		g_signal_connect_object (embed, "notify::popups-allowed",
					 G_CALLBACK (sync_tab_popups_allowed),
					 window, 0);
		g_signal_connect_object (embed, "notify::title",
					 G_CALLBACK (sync_tab_title),
					 window, 0);
		g_signal_connect_object (embed, "notify::address",
					 G_CALLBACK (sync_tab_address),
					 window, 0);
		g_signal_connect_object (embed, "notify::icon",
					 G_CALLBACK (sync_tab_icon),
					 window, 0);
		g_signal_connect_object (embed, "notify::status-message",
					 G_CALLBACK (sync_tab_message),
					 window, 0);
		g_signal_connect_object (embed, "notify::security-level",
					 G_CALLBACK (sync_tab_security),
					 window, 0);
		g_signal_connect_object (embed, "notify::document-type",
					 G_CALLBACK (sync_tab_document_type),
					 window, 0);
		g_signal_connect_object (embed, "notify::zoom",
					 G_CALLBACK (sync_tab_zoom),
					 window, 0);
		g_signal_connect_object (embed, "notify::load-status",
					 G_CALLBACK (sync_tab_load_status),
					 window, 0);
		g_signal_connect_object (embed, "notify::navigation",
					 G_CALLBACK (sync_tab_navigation),
					 window, 0);
		g_signal_connect_object (embed, "ge-context-menu",
					 G_CALLBACK (tab_context_menu_cb),
					 window, G_CONNECT_AFTER);
		g_signal_connect_object (embed, "notify::load-progress",
					 G_CALLBACK (sync_tab_load_progress),
					 window, 0);
		g_signal_connect_object (embed, "ge_dom_mouse_click",
					 G_CALLBACK (ephy_window_dom_mouse_click_cb),
					 window, 0);
		g_signal_connect_object (embed, "notify::visibility",
					 G_CALLBACK (ephy_window_visibility_cb),
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
	address = ephy_embed_get_address (embed);
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

	g_signal_connect_object (embed, "close-request",
				 G_CALLBACK (embed_close_request_cb),
				 window, 0);
	g_signal_connect_object (embed, "ge-modal-alert",
				 G_CALLBACK (embed_modal_alert_cb), window, G_CONNECT_AFTER);

	/* Let the extensions attach themselves to the tab */
	manager = EPHY_EXTENSION (ephy_shell_get_extensions_manager (ephy_shell));
	ephy_extension_attach_tab (manager, window, embed);
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
		(embed, G_CALLBACK (embed_modal_alert_cb), window);
	g_signal_handlers_disconnect_by_func
		(embed, G_CALLBACK (embed_close_request_cb), window);
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

	if (!ephy_embed_has_modified_forms (embed) ||
	    confirm_close_with_modified_forms (window))
	{
		ephy_embed_close (embed);
	}
}

static GtkNotebook *
setup_notebook (EphyWindow *window)
{
	GtkNotebook *notebook;

	notebook = GTK_NOTEBOOK (g_object_new (EPHY_TYPE_NOTEBOOK, NULL));

	g_signal_connect_after (notebook, "switch-page",
				G_CALLBACK (notebook_switch_page_cb),
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
ephy_window_set_chrome (EphyWindow *window, EphyEmbedChrome mask)
{
	EphyEmbedChrome chrome_mask = mask;

	if (mask == EPHY_EMBED_CHROME_ALL)
	{
		window->priv->should_save_chrome = TRUE;
	}

	if (!eel_gconf_get_boolean (CONF_WINDOWS_SHOW_TOOLBARS))
	{
		chrome_mask &= ~EPHY_EMBED_CHROME_TOOLBAR;
	}

	if (!eel_gconf_get_boolean (CONF_WINDOWS_SHOW_STATUSBAR))
	{
		chrome_mask &= ~EPHY_EMBED_CHROME_STATUSBAR;
	}

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_HIDE_MENUBAR))
	{
		chrome_mask &= ~EPHY_EMBED_CHROME_MENUBAR;
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

static gboolean
remove_true (void)
{
	return TRUE;
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

		g_hash_table_foreach_remove (priv->tabs_to_remove, (GHRFunc) remove_true, NULL);

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

	enabled = eel_gconf_get_boolean (CONF_BROWSE_WITH_CARET);
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

		g_object_notify (G_OBJECT (embed), "popups-allowed");
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
		if (flags & EPHY_LINK_ALLOW_FIXUP)
		{
			ephy_embed_load (embed, 
					 address, 
					 EPHY_EMBED_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
					 NULL);
		}
		else
		{
			ephy_embed_load_url (embed, address);
		}

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
		(CONF_BROWSE_WITH_CARET,
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

	return object;
}

static void
ephy_window_finalize (GObject *object)
{
	EphyWindow *window = EPHY_WINDOW (object);
	EphyWindowPrivate *priv = window->priv;

	g_hash_table_destroy (priv->tabs_to_remove);

	G_OBJECT_CLASS (ephy_window_parent_class)->finalize (object);

	LOG ("EphyWindow finalised %p", object);

#ifdef ENABLE_PYTHON
	ephy_python_schedule_gc ();
#endif

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
 * @chrome: an #EphyEmbedChrome
 * @is_popup: whether the new window is a popup window
 *
 * Identical to ephy_window_new(), but allows you to specify a chrome.
 *
 * Return value: a new #EphyWindow
 **/
EphyWindow *
ephy_window_new_with_chrome (EphyEmbedChrome chrome,
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
 * Unlike ephy_embed_load_url(), this function activates
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

	g_return_if_fail (EPHY_IS_WINDOW (window));

	embed = window->priv->active_embed;
	g_return_if_fail (embed != NULL);

	current_zoom = ephy_embed_get_zoom (embed);

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
		ephy_embed_set_zoom (embed, zoom);
	}
}

static void
sync_prefs_with_chrome (EphyWindow *window)
{
	EphyEmbedChrome flags = window->priv->chrome;

	if (window->priv->should_save_chrome)
	{
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_TOOLBARS,
				       flags & EPHY_EMBED_CHROME_TOOLBAR);
		eel_gconf_set_boolean (CONF_WINDOWS_SHOW_STATUSBAR,
				       flags & EPHY_EMBED_CHROME_STATUSBAR);
	}
}

static void
sync_chrome_with_view_toggle (GtkAction *action,
			      EphyWindow *window,
			      EphyEmbedChrome chrome_flag,
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
				      EPHY_EMBED_CHROME_STATUSBAR, FALSE);
}

static void
ephy_window_view_toolbar_cb (GtkAction *action,
			     EphyWindow *window)
{
	sync_chrome_with_view_toggle (action, window,
				      EPHY_EMBED_CHROME_TOOLBAR, TRUE);
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

	g_object_set (G_OBJECT (embed), "popups-allowed", allow, NULL);
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
