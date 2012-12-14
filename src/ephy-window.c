/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2011 Igalia S.L.
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

#include "ephy-action-helper.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-certificate-dialog.h"
#include "ephy-combined-stop-reload-action.h"
#include "ephy-debug.h"
#include "ephy-download-widget.h"
#include "ephy-download.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-encoding-menu.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-gui.h"
#include "ephy-home-action.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-navigation-history-action.h"
#include "ephy-notebook.h"
#include "ephy-page-menu-action.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-toolbar.h"
#include "ephy-type-builtins.h"
#include "ephy-web-view.h"
#include "ephy-zoom-action.h"
#include "ephy-zoom.h"
#include "popup-commands.h"
#include "window-commands.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

/**
 * SECTION:ephy-window
 * @short_description: Epiphany's main #GtkWindow widget
 *
 * #EphyWindow is Epiphany's main widget.
 */

static void ephy_window_view_popup_windows_cb	(GtkAction *action,
						 EphyWindow *window);

static const GtkActionEntry ephy_menu_entries [] = {

	/* Toplevel */

	{ "Bookmarks", NULL, N_("_Bookmarks") },
	{ "PopupAction", NULL, "" },
	{ "PagePopupAction", NULL, "" },
	{ "NotebookPopupAction", NULL, "" },

	/* File actions. */

	{ "FileNewWindow", NULL, N_("_New Window"), "<control>N", NULL,
	  G_CALLBACK (window_cmd_file_new_window) },
	{ "FileNewWindowIncognito", NULL, N_("New _Incognito Window"), "<control><shift>N", NULL,
	  G_CALLBACK (window_cmd_file_new_incognito_window) },
	{ "FileOpen", NULL, N_("_Open…"), "<control>O", NULL,
	  G_CALLBACK (window_cmd_file_open) },
	{ "FileSaveAs", NULL, N_("Save _As…"), "<shift><control>S", NULL,
	  G_CALLBACK (window_cmd_file_save_as) },
	{ "FileSaveAsApplication", NULL, N_("Save As _Web Application…"), "<shift><control>A", NULL,
	  G_CALLBACK (window_cmd_file_save_as_application) },
	{ "FilePrint", NULL, N_("_Print…"), "<control>P", NULL,
	  G_CALLBACK (window_cmd_file_print) },
	{ "FileSendTo", NULL, N_("S_end Link by Email…"), NULL, NULL,
	  G_CALLBACK (window_cmd_file_send_to) },
	{ "FileCloseTab", NULL, N_("_Close"), "<control>W", NULL,
	  G_CALLBACK (window_cmd_file_close_window) },
	{ "FileQuit", NULL, N_("_Quit"), "<control>Q", NULL,
	  G_CALLBACK (window_cmd_file_quit) },

	/* Edit actions. */

	{ "EditUndo", NULL, N_("_Undo"), "<control>Z", NULL,
	  G_CALLBACK (window_cmd_edit_undo) },
	{ "EditRedo", NULL, N_("Re_do"), "<shift><control>Z", NULL,
	  G_CALLBACK (window_cmd_edit_redo) },
	{ "EditCut", NULL, N_("Cu_t"), "<control>X", NULL,
	  G_CALLBACK (window_cmd_edit_cut) },
	{ "EditCopy", NULL, N_("_Copy"), "<control>C", NULL,
	  G_CALLBACK (window_cmd_edit_copy) },
	{ "EditPaste", NULL, N_("_Paste"), "<control>V", NULL,
	  G_CALLBACK (window_cmd_edit_paste) },
	{ "EditDelete", NULL, NULL, NULL, NULL,
	  G_CALLBACK (window_cmd_edit_delete) },
	{ "EditSelectAll", NULL, N_("Select _All"), "<control>A", NULL,
	  G_CALLBACK (window_cmd_edit_select_all) },
	{ "EditFind", NULL, N_("_Find…"), "<control>F", NULL,
	  G_CALLBACK (window_cmd_edit_find) },
	{ "EditFindNext", NULL, N_("Find Ne_xt"), "<control>G", NULL,
	  G_CALLBACK (window_cmd_edit_find_next) },
	{ "EditFindPrev", NULL, N_("Find Pre_vious"), "<shift><control>G", NULL,
	  G_CALLBACK (window_cmd_edit_find_prev) },
	{ "EditBookmarks", NULL, N_("Edit _Bookmarks"), "<control>B", NULL,
	  G_CALLBACK (window_cmd_edit_bookmarks) },
	{ "EditHistory", NULL, N_("_History"), "<control>H", NULL,
	  G_CALLBACK (window_cmd_edit_history) },
	{ "EditPreferences", NULL, N_("Preferences"), "<control>e", NULL,
	  G_CALLBACK (window_cmd_edit_preferences) },
	{ "EditPersonalData", NULL, N_("Personal Data"), "<control>m", NULL,
	  G_CALLBACK (window_cmd_edit_personal_data) },

	/* View actions. */

	{ "ViewStop", NULL, N_("_Stop"), "Escape", NULL,
	  G_CALLBACK (window_cmd_view_stop) },
	{ "ViewAlwaysStop", NULL, N_("_Stop"), "Escape",
	  NULL, G_CALLBACK (window_cmd_view_stop) },
	{ "ViewReload", NULL, N_("_Reload"), "<control>R", NULL,
	  G_CALLBACK (window_cmd_view_reload) },
	{ "ViewZoomIn", NULL, N_("Zoom _In"), "<control>plus", NULL,
	  G_CALLBACK (window_cmd_view_zoom_in) },
	{ "ViewZoomOut", NULL, N_("Zoom O_ut"), "<control>minus", NULL,
	  G_CALLBACK (window_cmd_view_zoom_out) },
	{ "ViewZoomNormal", NULL, N_("_Normal Size"), "<control>0", NULL,
	  G_CALLBACK (window_cmd_view_zoom_normal) },
	{ "ViewEncoding", NULL, N_("Text _Encoding"), NULL, NULL, NULL },
	{ "ViewPageSource", NULL, N_("_Page Source"), "<control>U", NULL,
	  G_CALLBACK (window_cmd_view_page_source) },

	/* Bookmarks actions. */

	{ "FileBookmarkPage", NULL, N_("_Add Bookmark…"), "<control>D", NULL,
	  G_CALLBACK (window_cmd_file_bookmark_page) },

	/* Go actions. */

	{ "GoLocation", NULL, N_("_Location…"), "<control>L", NULL,
	  G_CALLBACK (window_cmd_go_location) },

	/* Tabs actions. */

	{ "TabsPrevious", NULL, N_("_Previous Tab"), "<control>Page_Up", NULL,
	  G_CALLBACK (window_cmd_tabs_previous) },
	{ "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down", NULL,
	  G_CALLBACK (window_cmd_tabs_next) },
	{ "TabsMoveLeft", NULL, N_("Move Tab _Left"), "<shift><control>Page_Up", NULL,
	  G_CALLBACK (window_cmd_tabs_move_left) },
	{ "TabsMoveRight", NULL, N_("Move Tab _Right"), "<shift><control>Page_Down", NULL,
	  G_CALLBACK (window_cmd_tabs_move_right) },
        { "TabsDetach", NULL, N_("_Detach Tab"), NULL, NULL,
          G_CALLBACK (window_cmd_tabs_detach) },

	/* Help. */

	{ "HelpAbout", NULL, N_("_About"), NULL, NULL,
	  G_CALLBACK (window_cmd_help_about) }
};

static const GtkToggleActionEntry ephy_menu_toggle_entries [] =
{
	/* File actions. */

	{ "FileWorkOffline", NULL, N_("_Work Offline"), NULL, NULL,
	  G_CALLBACK (window_cmd_file_work_offline), FALSE },

	/* View actions. */

	{ "ViewDownloadsBar", NULL, N_("_Downloads Bar"), NULL, NULL,
	  NULL, FALSE },

	{ "ViewFullscreen", NULL, N_("_Fullscreen"), "F11", NULL,
	  G_CALLBACK (window_cmd_view_fullscreen), FALSE },
	{ "ViewPopupWindows", NULL, N_("Popup _Windows"), NULL, NULL,
	  G_CALLBACK (ephy_window_view_popup_windows_cb), FALSE },
	{ "BrowseWithCaret", NULL, N_("Selection Caret"), "F7", NULL,
	  G_CALLBACK (window_cmd_browse_with_caret), FALSE }
};

static const GtkActionEntry ephy_popups_entries [] = {
        /* Document. */

	{ "ContextBookmarkPage", NULL, N_("Add Boo_kmark…"), "<control>D", NULL,
	  G_CALLBACK (window_cmd_file_bookmark_page) },
	
	/* Links. */

	{ "OpenLink", NULL, N_("_Open Link"), NULL, NULL,
	  G_CALLBACK (popup_cmd_open_link) },
	{ "OpenLinkInNewWindow", NULL, N_("Open Link in New _Window"), NULL, NULL,
	  G_CALLBACK (popup_cmd_link_in_new_window) },
	{ "OpenLinkInNewTab", NULL, N_("Open Link in New _Tab"), NULL, NULL,
	  G_CALLBACK (popup_cmd_link_in_new_tab) },
	{ "DownloadLink", NULL, N_("_Download Link"), NULL,
	  NULL, G_CALLBACK (popup_cmd_download_link) },
	{ "DownloadLinkAs", NULL, N_("_Save Link As…"), NULL, NULL,
	  G_CALLBACK (popup_cmd_download_link_as) },
	{ "BookmarkLink", NULL, N_("_Bookmark Link…"),
	  NULL, NULL, G_CALLBACK (popup_cmd_bookmark_link) },
	{ "CopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_address) },

	/* Images. */

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

	/* Spelling. */

	{ "ReplaceWithSpellingSuggestion0", NULL, NULL, NULL,
	  NULL, G_CALLBACK (popup_replace_spelling), },
	{ "ReplaceWithSpellingSuggestion1", NULL, NULL, NULL,
	  NULL, G_CALLBACK (popup_replace_spelling), },
	{ "ReplaceWithSpellingSuggestion2", NULL, NULL, NULL,
	  NULL, G_CALLBACK (popup_replace_spelling), },
	{ "ReplaceWithSpellingSuggestion3", NULL, NULL, NULL,
	  NULL, G_CALLBACK (popup_replace_spelling), },


	/* Inspector. */
	{ "InspectElement", NULL, N_("Inspect _Element"), NULL,
	  NULL, G_CALLBACK (popup_cmd_inspect_element) },
};

static const struct
{
	guint keyval;
	GdkModifierType modifier;
	const gchar *action;
	gboolean fromToolbar;
} extra_keybindings [] = {
	/* FIXME: PageMenu should have its accel without being in the
	 * extra keybindings, but does not seem to work for some
	 * reason. */
	{ GDK_KEY_F10,		0,			"PageMenu",		 TRUE },
	{ GDK_KEY_Home,		GDK_MOD1_MASK,	        "FileHome",		 TRUE },
	/* FIXME: these are not in any menu for now, so add them here. */
	{ GDK_KEY_F11,          0,                      "ViewFullscreen",        FALSE },
	{ GDK_KEY_plus,         GDK_CONTROL_MASK,       "ViewZoomIn",            FALSE },
	{ GDK_KEY_minus,        GDK_CONTROL_MASK,       "ViewZoomOut",           FALSE },
	{ GDK_KEY_0,            GDK_CONTROL_MASK,       "ViewZoomNormal",        FALSE },
	{ GDK_KEY_g,            GDK_CONTROL_MASK,       "EditFindNext",          FALSE },
	{ GDK_KEY_G,            GDK_CONTROL_MASK |
	                        GDK_SHIFT_MASK,         "EditFindPrev",          FALSE },

	{ GDK_KEY_s,		GDK_CONTROL_MASK,	"FileSaveAs",		 FALSE },
	{ GDK_KEY_R,		GDK_CONTROL_MASK |
				GDK_SHIFT_MASK,		"ViewReload",		 FALSE },
	/* Tab navigation */
	{ GDK_KEY_Page_Up,      GDK_CONTROL_MASK,       "TabsPrevious",          FALSE },
	{ GDK_KEY_Page_Down,    GDK_CONTROL_MASK,       "TabsNext",              FALSE },
	{ GDK_KEY_Page_Up,      GDK_CONTROL_MASK |
	                        GDK_SHIFT_MASK,         "TabsMoveLeft",          FALSE },
	{ GDK_KEY_Page_Down,    GDK_CONTROL_MASK |
	                        GDK_SHIFT_MASK,         "TabsMoveRight",         FALSE },
	/* Go */
	{ GDK_KEY_l,            GDK_CONTROL_MASK,       "GoLocation",            FALSE },
	/* Support all the MSIE tricks as well ;) */
	{ GDK_KEY_F5,		0,			"ViewReload",		 FALSE },
	{ GDK_KEY_F5,		GDK_CONTROL_MASK,	"ViewReload",		 FALSE },
	{ GDK_KEY_F5,		GDK_SHIFT_MASK,		"ViewReload",		 FALSE },
	{ GDK_KEY_F5,		GDK_CONTROL_MASK |
				GDK_SHIFT_MASK,		"ViewReload",		 FALSE },
	{ GDK_KEY_KP_Add,	GDK_CONTROL_MASK,	"ViewZoomIn",		 FALSE },
	{ GDK_KEY_KP_Subtract,	GDK_CONTROL_MASK,	"ViewZoomOut",		 FALSE },
	{ GDK_KEY_equal,	GDK_CONTROL_MASK,	"ViewZoomIn",		 FALSE },
	{ GDK_KEY_KP_0,		GDK_CONTROL_MASK,	"ViewZoomNormal",	 FALSE },
	/* These keys are a bit strange: when pressed with no modifiers, they emit
	 * KP_PageUp/Down Control; when pressed with Control+Shift they are KP_9/3,
	 * when NumLock is on they are KP_9/3 and with NumLock and Control+Shift
	 * They're KP_PageUp/Down again!
	 */
	{ GDK_KEY_KP_Left,	GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KEY_KP_4,		GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KEY_KP_Right,	GDK_MOD1_MASK /*Alt*/,	"NavigationForward",	TRUE },
	{ GDK_KEY_KP_6,		GDK_MOD1_MASK /*Alt*/,	"NavigationForward",	TRUE },
	{ GDK_KEY_KP_Page_Up,	GDK_CONTROL_MASK,	"TabsPrevious",		FALSE },
	{ GDK_KEY_KP_9,		GDK_CONTROL_MASK,	"TabsPrevious",		FALSE },
	{ GDK_KEY_KP_Page_Down,	GDK_CONTROL_MASK,	"TabsNext",		FALSE },
	{ GDK_KEY_KP_3,		GDK_CONTROL_MASK,	"TabsNext",		FALSE },
	{ GDK_KEY_KP_Page_Up,	GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveLeft",		FALSE },
	{ GDK_KEY_KP_9,		GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveLeft",		FALSE },
	{ GDK_KEY_KP_Page_Down,	GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveRight",	FALSE },
	{ GDK_KEY_KP_3,		GDK_SHIFT_MASK | GDK_CONTROL_MASK,	"TabsMoveRight",	FALSE },
#ifdef HAVE_X11_XF86KEYSYM_H
	{ XF86XK_Back,		0,			"NavigationBack",	TRUE  },
	{ XF86XK_Forward,	0,			"NavigationForward",	TRUE  },
	{ XF86XK_Go,	 	0,			"GoLocation",		FALSE },
	{ XF86XK_OpenURL, 	0,			"GoLocation",		FALSE },
	{ XF86XK_AddFavorite, 	0,			"FileBookmarkPage",	FALSE },
	{ XF86XK_Refresh, 	0,			"ViewReload",		FALSE },
	{ XF86XK_Reload,	0, 			"ViewReload",		FALSE },
	{ XF86XK_Search,	0,			"EditFind",		FALSE },
	{ XF86XK_Send,	 	0,			"FileSendTo",		FALSE },
	{ XF86XK_Stop,		0,			"ViewStop",		FALSE },
	{ XF86XK_ZoomIn,	0, 			"ViewZoomIn",		FALSE },
	{ XF86XK_ZoomOut,	0, 			"ViewZoomOut",		FALSE }
	/* FIXME: what about ScrollUp, ScrollDown, Menu*, Option, LogOff, Save,.. any others? */
#endif /* HAVE_X11_XF86KEYSYM_H */
};

#define SETTINGS_CONNECTION_DATA_KEY	"EphyWindowSettings"

#define EPHY_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_WINDOW, EphyWindowPrivate))

struct _EphyWindowPrivate
{
	GtkWidget *main_vbox;
	GtkWidget *toolbar;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GtkActionGroup *popups_action_group;
	GtkActionGroup *toolbar_action_group;
	GtkActionGroup *tab_accels_action_group;
	EphyEncodingMenu *enc_menu;
	GtkNotebook *notebook;
	EphyEmbed *active_embed;
	EphyFindToolbar *find_toolbar;
	EphyWebViewChrome chrome;
	EphyWebViewChrome pre_fullscreen_chrome;
	EphyEmbedEvent *context_event;
	WebKitHitTestResult *hit_test_result;
	guint idle_worker;
	GtkWidget *downloads_box;

	EphyLocationController *location_controller;

	gulong set_focus_handler;
	gulong app_menu_visibility_handler;

	guint closing : 1;
	guint has_size : 1;
	guint fullscreen_mode : 1;
	guint is_popup : 1;
	guint present_on_insert : 1;
	guint key_theme_is_emacs : 1;
	guint updating_address : 1;
	guint show_lock : 1;
};

enum
{
	PROP_0,
	PROP_ACTIVE_CHILD,
	PROP_CHROME,
	PROP_SINGLE_TAB_MODE
};

/* Make sure not to overlap with those in ephy-lockdown.c */
enum
{
	SENS_FLAG_CHROME	= 1 << 0,
	SENS_FLAG_CONTEXT	= 1 << 1,
	SENS_FLAG_DOCUMENT	= 1 << 2,
	SENS_FLAG_LOADING	= 1 << 3,
	SENS_FLAG_NAVIGATION	= 1 << 4,
	SENS_FLAG_IS_BLANK	= 1 << 5
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
construct_confirm_close_dialog (EphyWindow *window,
				const char *title,
				const char *info,
				const char *action)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CANCEL,
					 "%s", title);

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog), "%s", info);
	
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       action, GTK_RESPONSE_ACCEPT);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	/* FIXME gtk_window_set_title (GTK_WINDOW (dialog), _("Close Document?")); */
	gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (window)),
				     GTK_WINDOW (dialog));

	return dialog;
}

static gboolean
confirm_close_with_modified_forms (EphyWindow *window)
{
	if (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
				    EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA))
	{
		GtkWidget *dialog;
		int response;

		dialog = construct_confirm_close_dialog (window,
				_("There are unsubmitted changes to form elements"),
				_("If you close the document anyway, "
				  "you will lose that information."),
				_("Close _Document"));
		response = gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy (dialog);

		return response == GTK_RESPONSE_ACCEPT;
	}
	
	return TRUE;
}

static gboolean
confirm_close_with_downloads (EphyWindow *window)
{
	GtkWidget *dialog;
	int response;

	dialog = construct_confirm_close_dialog (window,
			_("There are ongoing downloads in this window"),
			_("If you close this window, the downloads will be cancelled"),
			_("Close window and cancel downloads"));
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return response == GTK_RESPONSE_ACCEPT;
}

static void
impl_remove_child (EphyEmbedContainer *container,
		   EphyEmbed *child)
{
	EphyWindow *window;

	window = EPHY_WINDOW (container);
	g_signal_emit_by_name (window->priv->notebook,
			       "tab-close-request",
			       child, window);
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

	if (embed == NULL)
	{
		embed = window->priv->active_embed;
	}

	if (flags & EPHY_LINK_BOOKMARK)
		ephy_web_view_set_visit_type (ephy_embed_get_web_view (embed),
					      EPHY_PAGE_VISIT_BOOKMARK);
	else if (flags & EPHY_LINK_TYPED)
		ephy_web_view_set_visit_type (ephy_embed_get_web_view (embed),
					      EPHY_PAGE_VISIT_TYPED);
		
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

		if (flags & EPHY_LINK_NEW_TAB_APPEND_AFTER)
			ntflags |= EPHY_NEW_TAB_APPEND_AFTER;

		if (flags & EPHY_LINK_HOME_PAGE)
		{
			ntflags |= EPHY_NEW_TAB_HOME_PAGE;
			if (flags & EPHY_LINK_NEW_TAB)
				ntflags |= EPHY_NEW_TAB_DONT_COPY_HISTORY;
		}

		new_embed = ephy_shell_new_tab
				(ephy_shell_get_default (),
				 EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
				 embed, address, ntflags);
	}
	else
	{
		ephy_web_view_load_url (ephy_embed_get_web_view (embed), address);

		if (address == NULL || address[0] == '\0' || g_str_equal (address, "about:blank"))
		{
			ephy_window_activate_location (window);
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
ephy_window_link_iface_init (EphyLinkIface *iface)
{
	iface->open_link = ephy_window_open_link;
}

G_DEFINE_TYPE_WITH_CODE (EphyWindow, ephy_window, GTK_TYPE_APPLICATION_WINDOW,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
						ephy_window_link_iface_init)
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED_CONTAINER,
						ephy_window_embed_container_iface_init))

static void
settings_change_notify (GtkSettings *settings,
			EphyWindow  *window)
{
	EphyWindowPrivate *priv = window->priv;
	char *key_theme_name;

	g_object_get (settings,
		      "gtk-key-theme-name", &key_theme_name,
		      NULL);

	priv->key_theme_is_emacs =
		key_theme_name &&
		g_ascii_strcasecmp (key_theme_name, "Emacs") == 0;

	g_free (key_theme_name);
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
get_chromes_visibility (EphyWindow *window,
			gboolean *show_toolbar,
			gboolean *show_tabsbar,
			gboolean *show_downloads_box)
{
	EphyWindowPrivate *priv = window->priv;
	EphyWebViewChrome flags = priv->chrome;

	if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		*show_toolbar = FALSE;
		*show_tabsbar = FALSE;
	}
	else
	{
		*show_toolbar = (flags & EPHY_WEB_VIEW_CHROME_TOOLBAR) != 0;
		*show_tabsbar = !(priv->is_popup || priv->fullscreen_mode);
	}

	*show_downloads_box = (flags & EPHY_WEB_VIEW_CHROME_DOWNLOADS_BOX);
}

static void
sync_chromes_visibility (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	gboolean show_toolbar, show_tabsbar, show_downloads_box;

	if (priv->closing) return;

	get_chromes_visibility (window,
				&show_toolbar,
				&show_tabsbar,
				&show_downloads_box);

	g_object_set (priv->toolbar, "visible", show_toolbar, NULL);

	ephy_notebook_set_tabs_allowed (EPHY_NOTEBOOK (priv->notebook), show_tabsbar);
	gtk_widget_set_visible (priv->downloads_box, show_downloads_box);
}

static void
set_toolbar_visibility (EphyWindow *window, gboolean show_toolbar)
{
	EphyWindowPrivate *priv = window->priv;

	if (show_toolbar)
		priv->chrome |= EPHY_WEB_VIEW_CHROME_TOOLBAR;
	else
		priv->chrome &= ~EPHY_WEB_VIEW_CHROME_TOOLBAR;

	sync_chromes_visibility (window);
}

static void
sync_tab_load_status (EphyWebView *view,
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
	action = gtk_action_group_get_action (action_group, "FilePrint");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_LOADING, loading);

	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "ViewCombinedStopReload");
	ephy_combined_stop_reload_action_set_loading (EPHY_COMBINED_STOP_RELOAD_ACTION (action),
						      loading);
}

static void
_ephy_window_set_security_state (EphyWindow *window,
				 gboolean show_lock,
				 const char *stock_id)
{
	EphyWindowPrivate *priv = window->priv;

	priv->show_lock = show_lock != FALSE;

	g_object_set (priv->location_controller,
		      "lock-stock-id", stock_id,
		      "show-lock", priv->show_lock,
		      NULL);
}

static void
sync_tab_security (EphyWebView *view,
		   GParamSpec *pspec,
		   EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyWebViewSecurityLevel level;
	const char *stock_id = STOCK_LOCK_INSECURE;
	gboolean show_lock = FALSE;

	if (priv->closing) return;

	ephy_web_view_get_security_level (view, &level, NULL, NULL);

	switch (level)
	{
		case EPHY_WEB_VIEW_STATE_IS_UNKNOWN:
		case EPHY_WEB_VIEW_STATE_IS_INSECURE:
			/* Nothing to do. */
			break;
		case EPHY_WEB_VIEW_STATE_IS_BROKEN:
			stock_id = STOCK_LOCK_BROKEN;
                        show_lock = TRUE;
                        break;
		case EPHY_WEB_VIEW_STATE_IS_SECURE_LOW:
		case EPHY_WEB_VIEW_STATE_IS_SECURE_MED:
			/* We deliberately don't show the 'secure' icon
			 * for low & medium secure sites; see bug #151709.
			 */
			stock_id = STOCK_LOCK_INSECURE;
			break;
		case EPHY_WEB_VIEW_STATE_IS_SECURE_HIGH:
			stock_id = STOCK_LOCK_SECURE;
			show_lock = TRUE;
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	_ephy_window_set_security_state (window, show_lock, stock_id);
}

static void
ephy_window_fullscreen (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *embed;

	priv->fullscreen_mode = TRUE;
	priv->pre_fullscreen_chrome = priv->chrome;
	priv->chrome = 0;

	/* sync status */
	embed = window->priv->active_embed;
	sync_tab_load_status (ephy_embed_get_web_view (embed), NULL, window);
	sync_tab_security (ephy_embed_get_web_view (embed), NULL, window);

	sync_chromes_visibility (window);
	ephy_embed_entering_fullscreen (embed);
}

static void
ephy_window_unfullscreen (EphyWindow *window)
{
	window->priv->fullscreen_mode = FALSE;
	window->priv->chrome = window->priv->pre_fullscreen_chrome;

	sync_chromes_visibility (window);
	ephy_embed_leaving_fullscreen (window->priv->active_embed);
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
	GtkWidget *focus_widget;
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

	if (event->keyval == GDK_KEY_Escape && modifier == 0)
	{
		/* Always pass Escape to both the widget, and the parent */
		shortcircuit = TRUE;
		force_chain = TRUE;
	}
	else if (priv->key_theme_is_emacs && 
		 (modifier == GDK_CONTROL_MASK) &&
		 event->length > 0 &&
		 /* But don't pass Ctrl+Enter twice */
		 event->keyval != GDK_KEY_Return &&
		 event->keyval != GDK_KEY_KP_Enter &&
		 event->keyval != GDK_KEY_ISO_Enter)
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
					priv->toolbar_action_group :
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

	return GTK_WIDGET_CLASS (ephy_window_parent_class)->key_press_event (widget, event);
}

static gboolean
window_has_ongoing_downloads (EphyWindow *window)
{
	GList *l, *downloads;
	gboolean downloading = FALSE;

	downloads = gtk_container_get_children (GTK_CONTAINER (window->priv->downloads_box));

	for (l = downloads; l != NULL; l = l->next)
	{
		if (EPHY_IS_DOWNLOAD_WIDGET (l->data) != TRUE)
			continue;

		if (!ephy_download_widget_download_is_finished (EPHY_DOWNLOAD_WIDGET (l->data)))
		{
			downloading = TRUE;
			break;
		}
	}
	g_list_free (downloads);

	return downloading;
}

static gboolean
ephy_window_delete_event (GtkWidget *widget,
			  GdkEventAny *event)
{
	if (!ephy_window_close (EPHY_WINDOW (widget)))
	    return TRUE;

	/* proceed with window close */
	if (GTK_WIDGET_CLASS (ephy_window_parent_class)->delete_event)
	{
		return GTK_WIDGET_CLASS (ephy_window_parent_class)->delete_event (widget, event);
	}

	return FALSE;
}

#define MAX_SPELL_CHECK_GUESSES 4

static void
update_link_actions_sensitivity (EphyWindow *window,
				 gboolean link_has_web_scheme)
{
	GtkAction *action;
	GtkActionGroup *action_group;

	action_group = window->priv->popups_action_group;

	action = gtk_action_group_get_action (action_group, "OpenLinkInNewWindow");
	gtk_action_set_sensitive (action, link_has_web_scheme);

	action = gtk_action_group_get_action (action_group, "OpenLinkInNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CONTEXT, !link_has_web_scheme);
}

#ifndef HAVE_WEBKIT2
static void
update_popup_actions_visibility (EphyWindow *window,
				 WebKitWebView *view,
				 guint context)
{
	GtkAction *action;
	GtkActionGroup *action_group;
	gboolean is_image = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
	gboolean is_editable = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
	GtkWidget *separator;
	char **guesses = NULL;
	int i;

	action_group = window->priv->popups_action_group;

	if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		action = gtk_action_group_get_action (action_group, "OpenLinkInNewTab");
		gtk_action_set_visible (action, FALSE);
		action = gtk_action_group_get_action (action_group, "OpenLinkInNewWindow");
		gtk_action_set_visible (action, FALSE);
		action = gtk_action_group_get_action (action_group, "ContextBookmarkPage");
		gtk_action_set_visible (action, FALSE);
		action = gtk_action_group_get_action (action_group, "BookmarkLink");
		gtk_action_set_visible (action, FALSE);
	}

	action = gtk_action_group_get_action (action_group, "OpenImage");
	gtk_action_set_visible (action, is_image);
	action = gtk_action_group_get_action (action_group, "SaveImageAs");
	gtk_action_set_visible (action, is_image);
	action = gtk_action_group_get_action (action_group, "SetImageAsBackground");
	gtk_action_set_visible (action, is_image);
	action = gtk_action_group_get_action (action_group, "CopyImageLocation");
	gtk_action_set_visible (action, is_image);

	if (is_editable)
	{
		char *text = NULL;
		WebKitWebFrame *frame;
		WebKitDOMRange *range;

		frame = webkit_web_view_get_focused_frame (view);
		range = webkit_web_frame_get_range_for_word_around_caret (frame);
		text = webkit_dom_range_get_text (range);

		if (text && *text != '\0')
		{
			int location, length;
			WebKitSpellChecker *checker = (WebKitSpellChecker*)webkit_get_text_checker();
			webkit_spell_checker_check_spelling_of_string (checker, text, &location, &length);
			if (length)
				guesses = webkit_spell_checker_get_guesses_for_word (checker, text, NULL);
			
		}

		g_free (text);
	}

	for (i = 0; i < MAX_SPELL_CHECK_GUESSES; i++)
	{
		char *action_name;

		action_name = g_strdup_printf("ReplaceWithSpellingSuggestion%d", i);
		action = gtk_action_group_get_action (action_group, action_name);

		if (guesses && i <= g_strv_length (guesses)) {
			gtk_action_set_visible (action, TRUE);
			gtk_action_set_label (action, guesses[i]);
		} else
			gtk_action_set_visible (action, FALSE);

		g_free (action_name);
	}

	/* The separator! There must be a better way to do this? */
	separator = gtk_ui_manager_get_widget (window->priv->manager,
					       "/EphyInputPopup/SpellingSeparator");
	if (guesses)
		gtk_widget_show (separator);
	else
		gtk_widget_hide (separator);

	if (guesses)
		g_strfreev (guesses);
}
#endif

static void
update_edit_action_sensitivity (EphyWindow *window, const gchar *action_name, gboolean sensitive, gboolean hide)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, action_name);
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_visible (action, !hide || sensitive);
}

#ifdef HAVE_WEBKIT2
typedef struct
{
	EphyWindow *window;
	const gchar *action_name;
	gboolean hide;
} CanEditCommandAsyncData;

static CanEditCommandAsyncData *
can_edit_command_async_data_new (EphyWindow *window, const gchar *action_name, gboolean hide)
{
	CanEditCommandAsyncData *data;

	data = g_slice_new (CanEditCommandAsyncData);
	data->window = g_object_ref (window);
	data->action_name = action_name;
	data->hide = hide;

	return data;
}

static void
can_edit_command_async_data_free (CanEditCommandAsyncData *data)
{
	if (G_UNLIKELY (!data))
		return;

	g_object_unref (data->window);
	g_slice_free (CanEditCommandAsyncData, data);
}

static void
can_edit_command_callback (GObject *object, GAsyncResult *result, CanEditCommandAsyncData *data)
{
	gboolean sensitive;
	GError *error = NULL;

	sensitive = webkit_web_view_can_execute_editing_command_finish (WEBKIT_WEB_VIEW (object), result, &error);
	if (!error)
	{
		update_edit_action_sensitivity (data->window, data->action_name, sensitive, data->hide);

	}
	else
	{
		g_error_free (error);
	}

	can_edit_command_async_data_free (data);
}
#endif

static void
update_edit_actions_sensitivity (EphyWindow *window, gboolean hide)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));
	gboolean can_copy, can_cut, can_undo, can_redo, can_paste;

	if (GTK_IS_EDITABLE (widget))
	{
		GtkWidget *entry;
		gboolean has_selection;

		entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->priv->toolbar));
		
		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (widget), NULL, NULL);

		can_copy = has_selection;
		can_cut = has_selection;
		can_paste = TRUE;
		can_undo = ephy_location_entry_get_can_undo (EPHY_LOCATION_ENTRY (entry));
		can_redo = ephy_location_entry_get_can_redo (EPHY_LOCATION_ENTRY (entry));
	}
	else
	{
		EphyEmbed *embed;
		WebKitWebView *view;
#ifdef HAVE_WEBKIT2
		CanEditCommandAsyncData *data;
#endif

		embed = window->priv->active_embed;
		g_return_if_fail (embed != NULL);

		view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

#ifdef HAVE_WEBKIT2
		data = can_edit_command_async_data_new (window, "EditCopy", hide);
		webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_COPY, NULL,
							     (GAsyncReadyCallback)can_edit_command_callback,
							     data);
		data = can_edit_command_async_data_new (window, "EditCut", hide);
		webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_CUT, NULL,
							     (GAsyncReadyCallback)can_edit_command_callback,
							     data);
		data = can_edit_command_async_data_new (window, "EditPaste", hide);
		webkit_web_view_can_execute_editing_command (view, WEBKIT_EDITING_COMMAND_PASTE, NULL,
							     (GAsyncReadyCallback)can_edit_command_callback,
							     data);
		data = can_edit_command_async_data_new (window, "EditUndo", hide);
		webkit_web_view_can_execute_editing_command (view, "Undo", NULL,
							     (GAsyncReadyCallback)can_edit_command_callback,
							     data);
		data = can_edit_command_async_data_new (window, "EditRedo", hide);
		webkit_web_view_can_execute_editing_command (view, "Redo", NULL,
							     (GAsyncReadyCallback)can_edit_command_callback,
							     data);
		return;
#else
		can_copy = webkit_web_view_can_copy_clipboard (view);
		can_cut = webkit_web_view_can_cut_clipboard (view);
		can_paste = webkit_web_view_can_paste_clipboard (view);
		can_undo = webkit_web_view_can_undo (view);
		can_redo = webkit_web_view_can_redo (view);
#endif
	}

	update_edit_action_sensitivity (window, "EditCopy", can_copy, hide);
	update_edit_action_sensitivity (window, "EditCut", can_cut, hide);
	update_edit_action_sensitivity (window, "EditPaste", can_paste, hide);
	update_edit_action_sensitivity (window, "EditUndo", can_undo, hide);
	update_edit_action_sensitivity (window, "EditRedo", can_redo, hide);
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
       GtkWidget *edit_menu;

       edit_menu = gtk_ui_manager_get_widget
               (window->priv->manager, "/ui/PagePopup");

       g_signal_connect (edit_menu, "show",
                         G_CALLBACK (edit_menu_show_cb), window);
       g_signal_connect (edit_menu, "hide",
                         G_CALLBACK (edit_menu_hide_cb), window);
}

static void
setup_ui_manager (EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *manager;
	const char *prev_icon, *next_icon;

	window->priv->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (window->priv->main_vbox);
	gtk_container_add (GTK_CONTAINER (window),
			   window->priv->main_vbox);

	manager = gtk_ui_manager_new ();

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
	action = gtk_action_group_get_action (action_group, "FileSaveAsApplication");
	g_object_set (action, "short_label", _("Save As Application"), NULL);
	action = gtk_action_group_get_action (action_group, "FilePrint");
	g_object_set (action, "short_label", _("Print"), NULL);
	action = gtk_action_group_get_action (action_group, "FileBookmarkPage");
	g_object_set (action, "short_label", _("Bookmark"), NULL);
	action = gtk_action_group_get_action (action_group, "EditFind");
	g_object_set (action, "short_label", _("Find"), NULL);

	action = gtk_action_group_get_action (action_group, "EditFind");
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

	/* Tab accels */
	action_group = gtk_action_group_new ("TabAccelsActions");
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->tab_accels_action_group = action_group;
	g_object_unref (action_group);

	if (gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL) {
		prev_icon = "go-previous-rtl-symbolic";
		next_icon = "go-next-rtl-symbolic";
	} else {
		prev_icon = "go-previous-symbolic";
		next_icon = "go-next-symbolic";
	}

	action_group = gtk_action_group_new ("SpecialToolbarActions");
	action =
		g_object_new (EPHY_TYPE_NAVIGATION_HISTORY_ACTION,
			      "name", "NavigationBack",
			      "label", _("Back"),
			      "icon-name", prev_icon,
			      "window", window,
			      "direction", EPHY_NAVIGATION_HISTORY_DIRECTION_BACK,
			      NULL);
	gtk_action_group_add_action_with_accel (action_group, action,
						"<alt>Left");
	g_object_unref (action);

	action =
		g_object_new (EPHY_TYPE_NAVIGATION_HISTORY_ACTION,
			      "name", "NavigationForward",
			      "label", _("Forward"),
			      "icon-name", next_icon,
			      "window", window,
			      "direction", EPHY_NAVIGATION_HISTORY_DIRECTION_FORWARD,
			      NULL);
	gtk_action_group_add_action_with_accel (action_group, action,
						"<alt>Right");
	g_object_unref (action);

	action =
		g_object_new (EPHY_TYPE_ZOOM_ACTION,
			      "name", "Zoom",
			      "label", _("Zoom"),
			      "zoom", 1.0,
			      NULL);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_HOME_ACTION,
			       "name", "FileNewTab",
			       "label", _("New _Tab"),
			       NULL);
	gtk_action_group_add_action_with_accel (action_group, action, "<control>T");
	g_object_unref (action);

	action =
		g_object_new (EPHY_TYPE_HOME_ACTION,
			      "name", "FileHome",
			      "label", _("Go to most visited"),
			      NULL);
	gtk_action_group_add_action_with_accel (action_group, action, "<alt>Home");
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), window);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION,
			       "name", "ViewCombinedStopReload",
			       "loading", FALSE,
			       "window", window,
			       NULL);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_PAGE_MENU_ACTION,
			       "name", "PageMenu",
			       "icon-name", "emblem-system-symbolic",
			       "window", window,
			       NULL);
	gtk_action_group_add_action_with_accel (action_group, action, "<alt>E");
	g_object_unref (action);

	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->toolbar_action_group = action_group;
	g_object_unref (action_group);

	window->priv->manager = manager;
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (manager));
}

static char *
calculate_location (const char *typed_address, const char *address)
{
	const char *location;

	/* If there's a typed address, use that over address. Never
	 * show URIs in the 'do_not_show_address' array. */
	location = typed_address ? typed_address : address;
	location = ephy_embed_utils_is_no_show_address (location) ? NULL : location;

	return g_strdup (location);
}

static void
sync_tab_address (EphyWebView *view,
	          GParamSpec *pspec,
		  EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	const char *address;
	const char *typed_address;
	char *location;

	if (priv->closing) return;

	address = ephy_web_view_get_address (view);
	typed_address = ephy_web_view_get_typed_address (view);

	location = calculate_location (typed_address, address);
	ephy_window_set_location (window, location);
	g_free (location);
	ephy_find_toolbar_request_close (priv->find_toolbar);
}

static void
sync_tab_zoom (WebKitWebView *web_view, GParamSpec *pspec, EphyWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean can_zoom_in = TRUE, can_zoom_out = TRUE, can_zoom_normal = FALSE;
	double zoom;

	if (window->priv->closing) return;

	zoom = webkit_web_view_get_zoom_level (web_view);

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

	action_group = window->priv->action_group;
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");
	gtk_action_set_sensitive (action, can_zoom_in);
	action = gtk_action_group_get_action (action_group, "ViewZoomOut");
	gtk_action_set_sensitive (action, can_zoom_out);
	action = gtk_action_group_get_action (action_group, "ViewZoomNormal");
	gtk_action_set_sensitive (action, can_zoom_normal);
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
_ephy_window_action_set_favicon (EphyWindow *window,
				 GdkPixbuf *icon)
{
	EphyWindowPrivate *priv = window->priv;

	g_object_set (priv->location_controller, "icon", icon, NULL);
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

	_ephy_window_action_set_favicon (window, icon);
}

static void
_ephy_window_set_navigation_flags (EphyWindow *window,
				   EphyWebViewNavigationFlags flags)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->toolbar_action_group, "NavigationBack");
	gtk_action_set_sensitive (action, flags & EPHY_WEB_VIEW_NAV_BACK);
	action = gtk_action_group_get_action (window->priv->toolbar_action_group, "NavigationForward");
	gtk_action_set_sensitive (action, flags & EPHY_WEB_VIEW_NAV_FORWARD);
}

static void
sync_tab_navigation (EphyWebView *view,
		     GParamSpec *pspec,
		     EphyWindow *window)
{
	if (window->priv->closing) return;

	_ephy_window_set_navigation_flags (window,
					   ephy_web_view_get_navigation_flags (view));
}

static void
_ephy_window_set_default_actions_sensitive (EphyWindow *window,
					    guint flags,
					    gboolean set)
{
	EphyWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group;
	GtkAction *action;
	int i;
	const char *action_group_actions[] = { "FileSaveAs", "FileSaveAsApplication", "FilePrint",
					       "FileSendTo", "FileBookmarkPage", "EditFind",
					       "EditFindPrev", "EditFindNext", "ViewEncoding",
					       "ViewZoomIn", "ViewZoomOut", "ViewPageSource",
					       NULL };

	action_group = priv->action_group;

	/* Page menu */
	for (i = 0; action_group_actions[i] != NULL; i++)
	{
		action = gtk_action_group_get_action (action_group,
						      action_group_actions[i]);
		ephy_action_change_sensitivity_flags (action,
						      flags, set);
	}

	/* Page context popup */
	action = gtk_action_group_get_action (priv->popups_action_group,
					      "ContextBookmarkPage");
	ephy_action_change_sensitivity_flags (action,
					      flags, set);

	action = gtk_action_group_get_action (priv->popups_action_group,
					      "InspectElement");
	ephy_action_change_sensitivity_flags (action,
					      flags, set);

	/* Toolbar */
	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "ViewCombinedStopReload");
	ephy_action_change_sensitivity_flags (action,
					      flags, set);
}

static void
sync_tab_is_blank (EphyWebView *view,
		   GParamSpec *pspec,
		   EphyWindow *window)
{
	if (window->priv->closing) return;

	_ephy_window_set_default_actions_sensitive (window,
						    SENS_FLAG_IS_BLANK,
						    ephy_web_view_get_is_blank (view));
}

static void
sync_tab_popup_windows (EphyWebView *view,
			GParamSpec *pspec,
			EphyWindow *window)
{
	/* FIXME: show popup count somehow */
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
sync_network_status (EphyEmbedSingle *single,
		     GParamSpec *pspec,
		     EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkAction *action;
	gboolean is_online;

	GNetworkMonitor *monitor = G_NETWORK_MONITOR (ephy_shell_get_net_monitor (ephy_shell_get_default ()));
	is_online = g_network_monitor_get_network_available (monitor);

	action = gtk_action_group_get_action (priv->action_group,
					      "FileWorkOffline");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (window_cmd_file_work_offline), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !is_online);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (window_cmd_file_work_offline), window);	
}

#ifndef HAVE_WEBKIT2
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
#endif

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

static void
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

static void
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

#ifdef HAVE_WEBKIT2
static void
context_menu_dismissed_cb (WebKitWebView *webView,
			   EphyWindow *window)
{
	LOG ("Deactivating popup menu");

	enable_edit_actions_sensitivity (window);

	g_signal_handlers_disconnect_by_func
		(webView, G_CALLBACK (context_menu_dismissed_cb), window);

	_ephy_window_unset_context_event (window);
}
#else
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
#endif

#ifdef HAVE_WEBKIT2
static void
add_action_to_context_menu (WebKitContextMenu *context_menu,
			    GtkActionGroup *action_group,
			    const char *action_name)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group, action_name);
	webkit_context_menu_append (context_menu, webkit_context_menu_item_new (action));
}

/* FIXME: Add webkit_context_menu_find() ? */
static WebKitContextMenuItem *
find_item_in_context_menu (WebKitContextMenu *context_menu,
			   WebKitContextMenuAction action)
{
	GList *items, *iter;

	items = webkit_context_menu_get_items (context_menu);
	for (iter = items; iter; iter = g_list_next (iter))
	{
		WebKitContextMenuItem *item = (WebKitContextMenuItem *)iter->data;

		if (webkit_context_menu_item_get_stock_action (item) == action)
			return g_object_ref (item);
	}

	return NULL;
}

static gboolean
populate_context_menu (WebKitWebView *web_view,
		       WebKitContextMenu *context_menu,
		       GdkEvent *event,
		       WebKitHitTestResult *hit_test_result,
		       EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	WebKitContextMenuItem *input_methods_item;
	WebKitContextMenuItem *unicode_item;
	EphyEmbedEvent *embed_event;
	gboolean is_document = FALSE;
	gboolean app_mode;

	input_methods_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_INPUT_METHODS);
	unicode_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_UNICODE);

	webkit_context_menu_remove_all (context_menu);

	embed_event = ephy_embed_event_new ((GdkEventButton *)event, hit_test_result);
	_ephy_window_set_context_event (window, embed_event);
	g_object_unref (embed_event);

	app_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION;

	update_edit_actions_sensitivity (window, FALSE);

	if (webkit_hit_test_result_context_is_link (hit_test_result))
	{
		const char *uri;
		gboolean link_has_web_scheme;

		uri = webkit_hit_test_result_get_link_uri (hit_test_result);
		link_has_web_scheme = ephy_embed_utils_address_has_web_scheme (uri);

		update_edit_actions_sensitivity (window, TRUE);
		update_link_actions_sensitivity (window, link_has_web_scheme);

		if (!app_mode)
		{
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "OpenLink");
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "OpenLinkInNewTab");
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "OpenLinkInNewWindow");
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
		}
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditCopy");
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "DownloadLink");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "DownloadLinkAs");
		if (!app_mode)
		{
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "BookmarkLink");
		}
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "CopyLinkAddress");
	}
	else if (webkit_hit_test_result_context_is_editable (hit_test_result))
	{
		update_edit_actions_sensitivity (window, FALSE);

		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditCut");
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditCopy");
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditPaste");
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditSelectAll");
		if (input_methods_item || unicode_item)
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
		if (input_methods_item)
		{
			webkit_context_menu_append (context_menu, input_methods_item);
			g_object_unref (input_methods_item);
		}

		if (unicode_item)
		{
			webkit_context_menu_append (context_menu, unicode_item);
			g_object_unref (unicode_item);
		}
	}
	else
	{
		is_document = TRUE;

		update_edit_actions_sensitivity (window, TRUE);

		add_action_to_context_menu (context_menu,
					    priv->toolbar_action_group, "NavigationBack");
		add_action_to_context_menu (context_menu,
					    priv->toolbar_action_group, "NavigationForward");
		add_action_to_context_menu (context_menu,
					    priv->action_group, "ViewReload");
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditCopy");
		if (!app_mode)
		{
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "ContextBookmarkPage");
		}
	}

	if (webkit_hit_test_result_context_is_image (hit_test_result))
	{
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "OpenImage");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "SaveImageAs");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "SetImageAsBackground");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "CopyImageLocation");
	}

	if (is_document)
	{
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->action_group, "FileSendTo");
	}

	webkit_context_menu_append (context_menu,
				    webkit_context_menu_item_new_separator ());
	webkit_context_menu_append (context_menu,
				    webkit_context_menu_item_new_from_stock_action (WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT));

	g_signal_connect (web_view, "context-menu-dismissed",
			  G_CALLBACK (context_menu_dismissed_cb),
			  window);

	return FALSE;
}
#else
static void
show_embed_popup (EphyWindow *window,
		  WebKitWebView *view,
		  GdkEventButton *event,
		  WebKitHitTestResult *hit_test_result)
{
	EphyWindowPrivate *priv = window->priv;
	guint context;
	const char *popup;
	gboolean can_open_in_new;
	GtkWidget *widget;
	guint button;
	char *uri;
	EphyEmbedEvent *embed_event;

	g_object_get (hit_test_result, "link-uri", &uri, NULL);
	can_open_in_new = uri && ephy_embed_utils_address_has_web_scheme (uri);
	g_free (uri);

	g_object_get (hit_test_result, "context", &context, NULL);

	LOG ("show_embed_popup context %x", context);

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

	widget = gtk_ui_manager_get_widget (priv->manager, popup);
	g_return_if_fail (widget != NULL);

	update_link_actions_sensitivity (window, can_open_in_new);
	update_popup_actions_visibility (window,
					 view,
					 context);

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
#endif

static gboolean
save_target_uri (EphyWindow *window,
		 WebKitWebView *view,
		 GdkEventButton *event,
		 WebKitHitTestResult *hit_test_result)
{
	guint context;
	char *location = NULL;
	gboolean retval = FALSE;

	if ((event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK)
	{
		return FALSE;
	}

	g_object_get (hit_test_result, "context", &context, NULL);

	LOG ("ephy_window_dom_mouse_click_cb: button %d, context %d, modifier %d (%d:%d)",
	     event->button, context, event->state, (int)event->x, (int)event->y);

	/* shift+click saves the link target */
	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
	{
		g_object_get (G_OBJECT (hit_test_result), "link-uri", &location, NULL);
	}
	/* Note: pressing enter to submit a form synthesizes a mouse
	 * click event
	 */
	/* shift+click saves the non-link image */
	else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE &&
		 !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE))
	{
		g_object_get (G_OBJECT (hit_test_result), "image-uri", &location, NULL);
	}

	if (!location)
	{
		LOG ("Location: %s", location);

		retval = ephy_embed_utils_address_has_web_scheme (location);
		if (retval)
		{
			ephy_embed_auto_download_url (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view), location);
		}

		g_free (location);
	}

	return retval;
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
open_selected_url (EphyWindow *window,
		   WebKitWebView *view,
		   GdkEventButton *event,
		   WebKitHitTestResult *hit_test_result)
{
	guint context;
	ClipboardTextCBData *cb_data;
	EphyEmbed *embed;
	EphyEmbed **embed_ptr;

	if (!g_settings_get_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_MIDDLE_CLICK_OPENS_URL) ||
	    g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_ARBITRARY_URL))
	{
		return FALSE;
	}

	g_object_get (hit_test_result, "context", &context, NULL);

	LOG ("ephy_window_dom_mouse_click_cb: button %d, context %d, modifier %d (%d:%d)",
	     event->button, context, event->state, (int)event->x, (int)event->y);

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK ||
	    context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)
	{
		return FALSE;
	}

	/* See bug #133633 for why we do it this way */

	/* We need to make sure we know if the embed is destroyed
	 * between requesting the clipboard contents, and receiving
	 * them.
	 */
	embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

	cb_data = g_slice_new0 (ClipboardTextCBData);
	cb_data->embed = embed;
	cb_data->window = window;
	embed_ptr = &cb_data->embed;

	g_object_add_weak_pointer (G_OBJECT (embed), (gpointer *) embed_ptr);

	gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (embed),
							      GDK_SELECTION_PRIMARY),
				    (GtkClipboardTextReceivedFunc) clipboard_text_received_cb,
				    cb_data);
	return TRUE;
}

static gboolean
ephy_window_dom_mouse_click_cb (WebKitWebView *view,
				GdkEventButton *event,
				EphyWindow *window)
{
	WebKitHitTestResult *hit_test_result;
	gboolean handled = FALSE;

#ifdef HAVE_WEBKIT2
	hit_test_result = g_object_ref (window->priv->hit_test_result);
#else
	hit_test_result = webkit_web_view_get_hit_test_result (view, event);
#endif

	switch (event->button)
	{
	        case GDK_BUTTON_PRIMARY:
			handled = save_target_uri (window, view, event, hit_test_result);
			break;
	        case GDK_BUTTON_MIDDLE:
			handled = open_selected_url (window, view, event, hit_test_result);
			break;
#ifndef HAVE_WEBKIT2
	        case GDK_BUTTON_SECONDARY:
			show_embed_popup (window, view, event, hit_test_result);
			handled = TRUE;
			break;
#endif
	        default:
			break;
	}

	g_object_unref (hit_test_result);

	return handled;
}

#ifdef HAVE_WEBKIT2
static void
ephy_window_mouse_target_changed_cb (WebKitWebView *web_view,
				     WebKitHitTestResult *hit_test_result,
				     guint modifiers,
				     EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->hit_test_result)
		g_object_unref (priv->hit_test_result);
	priv->hit_test_result = g_object_ref (hit_test_result);
}
#endif

static void
ephy_window_visibility_cb (EphyEmbed *embed, GParamSpec *pspec, EphyWindow *window)
{
	gboolean visibility;

	visibility = ephy_web_view_get_visibility (ephy_embed_get_web_view (embed));

	if (visibility)
		gtk_widget_show (GTK_WIDGET (window));
	else
		gtk_widget_hide (GTK_WIDGET (window));
}

static void
sync_embed_is_overview (EphyEmbed *embed, GParamSpec *pspec, EphyWindow *window)
{
	if (window->priv->closing) return;

	_ephy_window_set_default_actions_sensitive (window,
						    SENS_FLAG_IS_BLANK,
						    ephy_embed_get_overview_mode (embed));;
}

static void
overview_open_link_cb (EphyOverview *overview,
		       const char *url,
		       EphyWindow *window)
{
	ephy_link_open (EPHY_LINK (window), url, NULL, ephy_link_flags_from_current_event ());
}

static void
ephy_window_set_is_popup (EphyWindow *window,
			  gboolean is_popup)
{
	EphyWindowPrivate *priv = window->priv;

	priv->is_popup = is_popup;

	g_object_notify (G_OBJECT (window), "is-popup");
}

#ifdef HAVE_WEBKIT2
static void
ephy_window_configure_for_view (EphyWindow *window,
				WebKitWebView *web_view)
{
	WebKitWindowProperties *properties;
	EphyWebViewChrome chrome_mask;

	properties = webkit_web_view_get_window_properties (web_view);

	chrome_mask = window->priv->chrome;
	if (!webkit_window_properties_get_toolbar_visible (properties))
		chrome_mask &= ~EPHY_WEB_VIEW_CHROME_TOOLBAR;

	/* We will consider windows with different chrome settings popups. */
	if (chrome_mask != window->priv->chrome) {
		GdkRectangle geometry;

		webkit_window_properties_get_geometry (properties, &geometry);
		gtk_window_set_default_size (GTK_WINDOW (window), geometry.width, geometry.height);

		if (!webkit_window_properties_get_resizable (properties))
			gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

		window->priv->is_popup = TRUE;
		window->priv->chrome = chrome_mask;

		sync_chromes_visibility (window);
	}
}
#else
static void
ephy_window_configure_for_view (EphyWindow *window,
				WebKitWebView *web_view)
{
	int width, height;
	gboolean toolbar_visible;
	EphyWebViewChrome chrome_mask;
	WebKitWebWindowFeatures *features;

	toolbar_visible = TRUE;
	features = webkit_web_view_get_window_features (web_view);

	chrome_mask = window->priv->chrome;

	g_object_get (features,
		      "width", &width,
		      "height", &height,
		      "toolbar-visible", &toolbar_visible,
		      NULL);

	if (!toolbar_visible)
		chrome_mask &= ~EPHY_WEB_VIEW_CHROME_TOOLBAR;

	/* We will consider windows with different chrome settings popups. */
	if (chrome_mask != window->priv->chrome) {
		gtk_window_set_default_size (GTK_WINDOW (window), width, height);

		window->priv->is_popup = TRUE;
		window->priv->chrome = chrome_mask;

		sync_chromes_visibility (window);
	}
}
#endif

static gboolean
web_view_ready_cb (WebKitWebView *web_view,
		   WebKitWebView *parent_web_view)
{
	EphyWindow *window, *parent_view_window;
	gboolean using_new_window;

	window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (web_view)));
	parent_view_window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (parent_web_view)));

	using_new_window = window != parent_view_window;

	if (using_new_window)
	{
		ephy_window_configure_for_view (window, web_view);
		g_signal_emit_by_name (parent_web_view, "new-window", web_view);
	}

	gtk_widget_show (GTK_WIDGET (window));

	return TRUE;
}

#ifdef HAVE_WEBKIT2
static WebKitWebView *
create_web_view_cb (WebKitWebView *web_view,
		    EphyWindow *window)
#else
static WebKitWebView *
create_web_view_cb (WebKitWebView *web_view,
		    WebKitWebFrame *frame,
		    EphyWindow *window)
#endif
{
	EphyEmbed *embed;
	WebKitWebView *new_web_view;
	EphyNewTabFlags flags;
	EphyWindow *parent_window;

	if (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
				    EPHY_PREFS_NEW_WINDOWS_IN_TABS))
	{
		parent_window = window;
		flags = EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			EPHY_NEW_TAB_JUMP |
			EPHY_NEW_TAB_APPEND_AFTER;
	}
	else
	{
		parent_window = NULL;
		flags = EPHY_NEW_TAB_IN_NEW_WINDOW |
			EPHY_NEW_TAB_DONT_SHOW_WINDOW;
	}

	embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
					 parent_window,
					 EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
					 NULL,
					 flags,
					 EPHY_WEB_VIEW_CHROME_ALL,
					 FALSE, /* is popup? */
					 0);

	new_web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
#ifdef HAVE_WEBKIT2
	g_signal_connect (new_web_view, "ready-to-show",
			  G_CALLBACK (web_view_ready_cb),
			  web_view);
#else
	g_signal_connect (new_web_view, "web-view-ready",
			  G_CALLBACK (web_view_ready_cb),
			  web_view);
#endif

	return new_web_view;
}

#ifdef HAVE_WEBKIT2
static gboolean
decide_policy_cb (WebKitWebView *web_view,
		  WebKitPolicyDecision *decision,
		  WebKitPolicyDecisionType decision_type,
		  EphyWindow *window)
{
	WebKitNavigationPolicyDecision *navigation_decision;
	WebKitNavigationType navigation_type;
	WebKitURIRequest *request;
	gint button;
	gint state;
	const char *uri;
	EphyEmbed *embed;
	EphyNewTabFlags flags;

	if (decision_type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
		return FALSE;

	navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
	request = webkit_navigation_policy_decision_get_request (navigation_decision);
	uri = webkit_uri_request_get_uri (request);

	if (!ephy_embed_utils_address_has_web_scheme (uri))
	{
		GError *error = NULL;
		GdkScreen *screen;

		screen = gtk_widget_get_screen (GTK_WIDGET (web_view));
		gtk_show_uri (screen, uri, GDK_CURRENT_TIME, &error);

		if (error)
		{
			LOG ("failed to handle non web scheme: %s", error->message);
			g_error_free (error);

			return FALSE;
		}

		webkit_policy_decision_ignore (decision);

		return TRUE;
	}

	navigation_type = webkit_navigation_policy_decision_get_navigation_type (navigation_decision);

	if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED &&
	    ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		/* The only thing we allow here is to either navigate
		 * in the same window and tab to the current domain,
		 * or launch a new (non app mode) epiphany instance
		 * for all the other cases. */
		gboolean return_value;
		SoupURI *soup_uri = soup_uri_new (uri);
		SoupURI *current_soup_uri = soup_uri_new (webkit_web_view_get_uri (web_view));

		if (g_str_equal (soup_uri->host, current_soup_uri->host))
		{
			return_value = FALSE;
		}
		else
		{
			char *command_line;
			GError *error = NULL;

			return_value = TRUE;

			command_line = g_strdup_printf ("gvfs-open %s", uri);
			g_spawn_command_line_async (command_line, &error);

			if (error)
			{
				g_debug ("Error opening %s: %s", uri, error->message);
				g_error_free (error);
			}

			g_free (command_line);

			webkit_policy_decision_ignore (decision);
		}

		soup_uri_free (soup_uri);
		soup_uri_free (current_soup_uri);

		return return_value;
	}

	if (navigation_type != WEBKIT_NAVIGATION_TYPE_LINK_CLICKED)
		return FALSE;


	button = webkit_navigation_policy_decision_get_mouse_button (navigation_decision);
	state = webkit_navigation_policy_decision_get_modifiers (navigation_decision);
	flags = EPHY_NEW_TAB_OPEN_PAGE;

	ephy_web_view_set_visit_type (EPHY_WEB_VIEW (web_view),
				      EPHY_PAGE_VISIT_LINK);

	/* New tab in new window for control+shift+click */
	if (button == 1 && state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
	{
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}
	/* New tab in existing window for middle click and
	 * control+click */
	else if (button == 2 || (button == 1 && state == GDK_CONTROL_MASK))
	{
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW | EPHY_NEW_TAB_APPEND_AFTER;
	}
	/* Because we connect to button-press-event *after*
	 * (G_CONNECT_AFTER) we need to prevent WebKit from browsing to
	 * a link when you shift+click it. Otherwise when you
	 * shift+click a link to download it you would also be taken to
	 * the link destination. */
	else if (button == 1 && state == GDK_SHIFT_MASK)
	{
		webkit_policy_decision_ignore (decision);

		return TRUE;
	}
	/* Those were our special cases, we won't handle this */
	else
	{
		return FALSE;
	}

	embed = ephy_embed_container_get_active_child
		(EPHY_EMBED_CONTAINER (window));

	ephy_shell_new_tab_full (ephy_shell_get_default (),
				 window,
				 embed,
				 request,
				 flags,
				 EPHY_WEB_VIEW_CHROME_ALL, FALSE, 0);

	webkit_policy_decision_ignore (decision);

	return TRUE;
}
#else
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
	const char *uri;

	reason = webkit_web_navigation_action_get_reason (action);
	button = webkit_web_navigation_action_get_button (action);
	state = webkit_web_navigation_action_get_modifier_state (action);
	uri = webkit_network_request_get_uri (request);

	if (!ephy_embed_utils_address_has_web_scheme (uri))
	{
		GError *error = NULL;
		GdkScreen *screen;

		screen = gtk_widget_get_screen (GTK_WIDGET (web_view));
		gtk_show_uri (screen, uri, GDK_CURRENT_TIME, &error);

		if (error)
		{
			LOG ("failed to handle non web scheme: %s", error->message);
			g_error_free (error);

			return FALSE;
		}

		webkit_web_policy_decision_ignore (decision);

		return TRUE;
	}

	if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED &&
	    ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		/* The only thing we allow here is to either navigate
		 * in the same window and tab to the current domain,
		 * or launch a new (non app mode) epiphany instance
		 * for all the other cases. */
		gboolean return_value;
		SoupURI *soup_uri = soup_uri_new (uri);
		SoupURI *current_soup_uri = soup_uri_new (webkit_web_view_get_uri (web_view));

		if (g_str_equal (soup_uri->host, current_soup_uri->host))
		{
			return_value = FALSE;
		}
		else
		{
			char *command_line;
			GError *error = NULL;

			return_value = TRUE;

			command_line = g_strdup_printf ("gvfs-open %s", uri);
			g_spawn_command_line_async (command_line, &error);

			if (error)
			{
				g_debug ("Error opening %s: %s", uri, error->message);
				g_error_free (error);
			}

			g_free (command_line);
		}

		soup_uri_free (soup_uri);
		soup_uri_free (current_soup_uri);

		return return_value;
	}

	if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED) {
		EphyEmbed *embed;
		EphyNewTabFlags flags;

		flags = EPHY_NEW_TAB_OPEN_PAGE;

		ephy_web_view_set_visit_type (EPHY_WEB_VIEW (web_view),
					      EPHY_PAGE_VISIT_LINK);

		/* New tab in new window for control+shift+click */
		if (button == 1 &&
		    state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
		{
			flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
		}
		/* New tab in existing window for middle click and
		 * control+click */
		else if (button == 2 ||
			 (button == 1 && state == GDK_CONTROL_MASK))
		{
			flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW | EPHY_NEW_TAB_APPEND_AFTER;
		}
		/* Because we connect to button-press-event *after*
		 * (G_CONNECT_AFTER) we need to prevent WebKit from browsing to
		 * a link when you shift+click it. Otherwise when you
		 * shift+click a link to download it you would also be taken to
		 * the link destination. */
		else if (button == 1 && state == GDK_SHIFT_MASK)
		{
			return TRUE;
		}
		/* Those were our special cases, we won't handle this */
		else
		{
			return FALSE;
		}

		embed = ephy_embed_container_get_active_child
			(EPHY_EMBED_CONTAINER (window));

		ephy_shell_new_tab_full (ephy_shell_get_default (),
					 window,
					 embed,
					 request,
					 flags,
					 EPHY_WEB_VIEW_CHROME_ALL, FALSE, 0);

		return TRUE;
	}

	return FALSE;
}
#endif

static void
ephy_window_connect_active_embed (EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *web_view;
	EphyWebView *view;
	EphyOverview *overview;

	g_return_if_fail (window->priv->active_embed != NULL);

	embed = window->priv->active_embed;
	view = ephy_embed_get_web_view (embed);
	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	sync_tab_security	(view, NULL, window);
	sync_tab_document_type	(view, NULL, window);
	sync_tab_load_status	(view, NULL, window);
	sync_tab_is_blank	(view, NULL, window);
	sync_tab_navigation	(view, NULL, window);
	sync_tab_title		(view, NULL, window);
	sync_tab_address	(view, NULL, window);
	sync_tab_icon		(view, NULL, window);
	sync_tab_popup_windows	(view, NULL, window);
	sync_tab_popups_allowed	(view, NULL, window);
	sync_embed_is_overview  (embed, NULL, window);

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
#ifdef HAVE_WEBKIT2
	g_signal_connect_object (web_view, "create",
				 G_CALLBACK (create_web_view_cb),
				 window, 0);
#else
	g_signal_connect_object (web_view, "create-web-view",
				 G_CALLBACK (create_web_view_cb),
				 window, 0);
#endif
#ifdef HAVE_WEBKIT2
	g_signal_connect_object (web_view, "decide-policy",
				 G_CALLBACK (decide_policy_cb),
				 window, 0);
#else
	g_signal_connect_object (web_view, "navigation-policy-decision-requested",
				 G_CALLBACK (policy_decision_required_cb),
				 window, 0);
	g_signal_connect_object (web_view, "new-window-policy-decision-requested",
				 G_CALLBACK (policy_decision_required_cb),
				 window, 0);
#endif

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
	g_signal_connect_object (view, "notify::is-blank",
				 G_CALLBACK (sync_tab_is_blank),
				 window, 0);
#ifdef HAVE_WEBKIT2
	g_signal_connect_object (view, "button-press-event",
				 G_CALLBACK (ephy_window_dom_mouse_click_cb),
				 window, 0);
	g_signal_connect_object (view, "context-menu",
				 G_CALLBACK (populate_context_menu),
				 window, 0);
	g_signal_connect_object (view, "mouse-target-changed",
				 G_CALLBACK (ephy_window_mouse_target_changed_cb),
				 window, 0);
#else
	/* We run our button-press-event after the default
	 * handler to make sure pages have a chance to perform
	 * their own handling - for instance, have their own
	 * context menus, or provide specific functionality
	 * for the right mouse button */
	g_signal_connect_object (view, "button-press-event",
				 G_CALLBACK (ephy_window_dom_mouse_click_cb),
				 window, G_CONNECT_AFTER);
#endif
	g_signal_connect_object (view, "notify::visibility",
				 G_CALLBACK (ephy_window_visibility_cb),
				 window, 0);

	g_signal_connect_object (embed, "notify::overview-mode",
				 G_CALLBACK (sync_embed_is_overview),
				 window, 0);

	overview = ephy_embed_get_overview (embed);
	g_signal_connect_object (overview, "open-link",
				 G_CALLBACK (overview_open_link_cb),
				 window, 0);

	g_object_notify (G_OBJECT (window), "active-child");
}

static void
ephy_window_disconnect_active_embed (EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *web_view;
	EphyWebView *view;
	EphyOverview *overview;
	guint sid;

	g_return_if_fail (window->priv->active_embed != NULL);

	embed = window->priv->active_embed;
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
#ifdef HAVE_WEBKIT2
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (decide_policy_cb),
					      window);
#else
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
#endif

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
					      G_CALLBACK (sync_tab_load_status),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (sync_tab_is_blank),
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
					      G_CALLBACK (ephy_window_visibility_cb),
					      window);

	g_signal_handlers_disconnect_by_func (embed,
					      G_CALLBACK (sync_embed_is_overview),
					      window);

	overview = ephy_embed_get_overview (embed);
	g_signal_handlers_disconnect_by_func (overview,
					      G_CALLBACK (overview_open_link_cb),
					      window);

	g_signal_handlers_disconnect_by_func
		(view, G_CALLBACK (ephy_window_dom_mouse_click_cb), window);
#ifdef HAVE_WEBKIT2
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (populate_context_menu),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (ephy_window_mouse_target_changed_cb),
					      window);
#endif
}

static void
ephy_window_set_active_tab (EphyWindow *window, EphyEmbed *new_embed)
{
	EphyEmbed *old_embed;

	g_return_if_fail (EPHY_IS_WINDOW (window));
	g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (new_embed)) == GTK_WIDGET (window));

	old_embed = window->priv->active_embed;

	if (old_embed == new_embed) return;

	if (old_embed != NULL)
		ephy_window_disconnect_active_embed (window);

	window->priv->active_embed = new_embed;

	if (new_embed != NULL)
		ephy_window_connect_active_embed (window);
}

static gboolean
embed_modal_alert_cb (EphyEmbed *embed,
		      EphyWindow *window)
{
	const char *address;

	/* switch the window to the tab, and bring the window to the foreground
	 * (since the alert is modal, the user won't be able to do anything
	 * with his current window anyway :|)
	 */
	impl_set_active_child (EPHY_EMBED_CONTAINER (window), embed);
	gtk_window_present (GTK_WINDOW (window));

	/* make sure the location entry shows the real URL of the tab's page */
	address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
	ephy_window_set_location (window, address);

	/* don't suppress alert */
	return FALSE;
}

static void
tab_accels_item_activate (GtkAction *action,
			  EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	const char *name;
	int tab_number;

	name = gtk_action_get_name (action);
	tab_number = atoi (name + strlen ("TabAccel"));

	gtk_notebook_set_current_page (priv->notebook, tab_number);
}

static void
tab_accels_update (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	int pages, i = 0;
	GList *actions, *l;

	actions = gtk_action_group_list_actions (priv->tab_accels_action_group);
	pages = gtk_notebook_get_n_pages (priv->notebook);
	for (l = actions; l != NULL; l = l->next)
	{
		GtkAction *action = GTK_ACTION (l->data);

		gtk_action_set_sensitive (action, (i < pages));

		i++;
	}
	g_list_free (actions);
}

#define TAB_ACCELS_N 10

static void
setup_tab_accels (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	guint id;
	int i;

	id = gtk_ui_manager_new_merge_id (priv->manager);

	for (i = 0; i < TAB_ACCELS_N; i++)
	{
		GtkAction *action;
		char *name;
		char *accel;

		name = g_strdup_printf ("TabAccel%d", i);
		accel = g_strdup_printf ("<alt>%d", (i + 1) % TAB_ACCELS_N);
		action = gtk_action_new (name, NULL, NULL, NULL);

		gtk_action_group_add_action_with_accel (priv->tab_accels_action_group,
							action, accel);

		g_signal_connect (action, "activate",
				  G_CALLBACK (tab_accels_item_activate), window);
		gtk_ui_manager_add_ui (priv->manager, id, "/",
				       name, name,
				       GTK_UI_MANAGER_ACCELERATOR,
				       FALSE);

		g_object_unref (action);
		g_free (accel);
		g_free (name);
	}
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

	LOG ("page-added notebook %p embed %p position %u\n", notebook, embed, position);

	g_return_if_fail (EPHY_IS_EMBED (embed));

#if 0
	g_signal_connect_object (embed, "open-link",
				 G_CALLBACK (ephy_link_open), window,
				 G_CONNECT_SWAPPED);
#endif

	g_signal_connect_object (ephy_embed_get_web_view (embed), "ge-modal-alert",
				 G_CALLBACK (embed_modal_alert_cb), window, G_CONNECT_AFTER);

        if (priv->present_on_insert)
        {
                priv->present_on_insert = FALSE;
                g_idle_add ((GSourceFunc) present_on_idle_cb, g_object_ref (window));
        }

	tab_accels_update (window);
}

static void
notebook_page_removed_cb (EphyNotebook *notebook,
			  EphyEmbed *embed,
			  guint position,
			  EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	LOG ("page-removed notebook %p embed %p position %u\n", notebook, embed, position);

	if (priv->closing) return;

	g_return_if_fail (EPHY_IS_EMBED (embed));

#if 0
	g_signal_handlers_disconnect_by_func (G_OBJECT (embed),
					      G_CALLBACK (ephy_link_open),
					      window);	
#endif

	g_signal_handlers_disconnect_by_func
		(ephy_embed_get_web_view (embed), G_CALLBACK (embed_modal_alert_cb), window);

	tab_accels_update (window);
}

static void
notebook_page_close_request_cb (EphyNotebook *notebook,
				EphyEmbed *embed,
				EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (gtk_notebook_get_n_pages (priv->notebook) == 1)
	{
		if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
					    EPHY_PREFS_LOCKDOWN_QUIT))
		{
			return;
		}
		if (window_has_ongoing_downloads (window) &&
		    !confirm_close_with_downloads (window))
		{
			return;
		}
	}

	if ((!ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed)) ||
	     confirm_close_with_modified_forms (window)))
	{
		gtk_widget_destroy (GTK_WIDGET (embed));
	}

	/* If that was the last tab, destroy the window. */
	if (gtk_notebook_get_n_pages (priv->notebook) == 0)
	{
		gtk_widget_destroy (GTK_WIDGET (window));
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

static void
notebook_switch_page_cb (GtkNotebook *notebook,
			 GtkWidget *page,
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
	g_signal_connect (notebook, "tab-close-request",
			  G_CALLBACK (notebook_page_close_request_cb), window);

	return notebook;
}

static void
ephy_window_set_chrome (EphyWindow *window, EphyWebViewChrome mask)
{
	EphyWebViewChrome chrome_mask = mask;

	if (!g_settings_get_boolean (EPHY_SETTINGS_UI,
				     EPHY_PREFS_UI_SHOW_TOOLBARS))
	{
		chrome_mask &= ~EPHY_WEB_VIEW_CHROME_TOOLBAR;
	}

	window->priv->chrome = chrome_mask;
}

static void
ephy_window_set_downloads_box_visibility (EphyWindow *window,
					  gboolean show)
{

	if (show) {
		gtk_widget_show (window->priv->downloads_box);
		window->priv->chrome |= EPHY_WEB_VIEW_CHROME_DOWNLOADS_BOX;
	} else {
		gtk_widget_hide (window->priv->downloads_box);
		window->priv->chrome &= ~EPHY_WEB_VIEW_CHROME_DOWNLOADS_BOX;
	}
}

static void
download_added_cb (EphyEmbedShell *shell,
		   EphyDownload *download,
		   gpointer data)
{
	EphyWindow *window = EPHY_WINDOW (data);
	GtkWidget *download_window;
	GtkWidget *widget;

	download_window = ephy_download_get_window (download);
	widget = ephy_download_get_widget (download);

	if (widget == NULL &&
	    (download_window == NULL || download_window == GTK_WIDGET (window)))
	{
		widget = ephy_download_widget_new (download);
		gtk_box_pack_start (GTK_BOX (window->priv->downloads_box),
				    widget, FALSE, FALSE, 0);
		gtk_widget_show (widget);
		ephy_window_set_downloads_box_visibility (window, TRUE);
	}
}

static void
downloads_removed_cb (GtkContainer *container,
		      GtkWidget *widget,
		      gpointer data)
{
	EphyWindow *window = EPHY_WINDOW (data);
	GList *children = NULL;

	children = gtk_container_get_children (container);
	if (g_list_length (children) == 1)
		ephy_window_set_downloads_box_visibility (window, FALSE);

	g_list_free (children);
}

static void
downloads_close_cb (GtkButton *button, EphyWindow *window)
{
	GList *l, *downloads;

	downloads = gtk_container_get_children (GTK_CONTAINER (window->priv->downloads_box));

	for (l = downloads; l != NULL; l = l->next)
	{
		if (EPHY_IS_DOWNLOAD_WIDGET (l->data) != TRUE)
			continue;

		if (ephy_download_widget_download_is_finished (EPHY_DOWNLOAD_WIDGET (l->data)))
		{
			gtk_widget_destroy (GTK_WIDGET (l->data));
		}
	}
	g_list_free (downloads);

	ephy_window_set_downloads_box_visibility (window, FALSE);
}

static GtkWidget *
setup_downloads_box (EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *close_button;
	GtkWidget *image;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	close_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);

	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);

	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_box_pack_end (GTK_BOX (widget), close_button, FALSE, FALSE, 4);

	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (downloads_close_cb), window);
	g_signal_connect (widget, "remove",
			  G_CALLBACK (downloads_removed_cb), window);

	gtk_widget_show_all (close_button);

	return widget;
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
		window->priv->closing = TRUE;

		ephy_bookmarks_ui_detach_window (window);

		g_signal_handlers_disconnect_by_func
			(ephy_embed_shell_get_default (),
			 download_added_cb, window);

		/* Deactivate menus */
		popups = gtk_ui_manager_get_toplevels (window->priv->manager, GTK_UI_MANAGER_POPUP);
		g_slist_foreach (popups, (GFunc) gtk_menu_shell_deactivate, NULL);
		g_slist_free (popups);
	
		single = ephy_embed_shell_get_embed_single (ephy_embed_shell_get_default ());
		g_signal_handlers_disconnect_by_func
			(single, G_CALLBACK (sync_network_status), window);

		g_object_unref (priv->enc_menu);
		priv->enc_menu = NULL;

		priv->action_group = NULL;
		priv->popups_action_group = NULL;
		priv->tab_accels_action_group = NULL;

		g_object_unref (priv->manager);
		priv->manager = NULL;

		_ephy_window_set_context_event (window, NULL);

		g_clear_object (&priv->hit_test_result);
	}

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
		case PROP_SINGLE_TAB_MODE:
			g_value_set_boolean (value, window->priv->is_popup);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec); 
			break;
	}
}

static gboolean
ephy_window_state_event (GtkWidget *widget,
			 GdkEventWindowState *event)
{
	EphyWindow *window = EPHY_WINDOW (widget);
	EphyWindowPrivate *priv = window->priv;

	if (GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event)
	{
		GTK_WIDGET_CLASS (ephy_window_parent_class)->window_state_event (widget, event);
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
ephy_window_finalize (GObject *object)
{
	EphyWindow *window = EPHY_WINDOW (object);
	EphyWindowPrivate *priv = window->priv;

	if (priv->set_focus_handler != 0)
		g_signal_handler_disconnect (window,
					     priv->set_focus_handler);

	if (priv->app_menu_visibility_handler != 0)
		g_signal_handler_disconnect (gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window))),
					     priv->app_menu_visibility_handler);

	G_OBJECT_CLASS (ephy_window_parent_class)->finalize (object);

	LOG ("EphyWindow finalised %p", object);
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
allow_popups_notifier (GSettings *settings,
		       char *key,
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

		g_object_notify (G_OBJECT (ephy_embed_get_web_view (embed)), "popups-allowed");
	}
	g_list_free (tabs);
}

static void
show_toolbars_setting_cb (GSettings *settings,
			  char *key,
			  EphyWindow *window)
{
	gboolean show_toolbars;

	show_toolbars = g_settings_get_boolean (EPHY_SETTINGS_UI,
						EPHY_PREFS_UI_SHOW_TOOLBARS);

	set_toolbar_visibility (window, show_toolbars);
}

static void
sync_user_input_cb (EphyLocationController *action,
		    GParamSpec *pspec,
		    EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *embed;
	const char *address;

	LOG ("sync_user_input_cb");

	if (priv->updating_address) return;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_assert (EPHY_IS_EMBED (embed));

	address = ephy_location_controller_get_address (action);

	priv->updating_address = TRUE;
	ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), address);
	priv->updating_address = FALSE;
}

static void
zoom_to_level_cb (GtkAction *action,
		  float zoom,
		  EphyWindow *window)
{
	ephy_window_set_zoom (window, zoom);
}

static void
lock_clicked_cb (EphyLocationController *controller,
		 EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyWebView *view;
	GTlsCertificate *certificate;
	GTlsCertificateFlags tls_errors;
	GtkWidget *certificate_dialog;

	view = ephy_embed_get_web_view (priv->active_embed);
	ephy_web_view_get_security_level (view, NULL, &certificate, &tls_errors);

	certificate_dialog = ephy_certificate_dialog_new (GTK_WINDOW (window),
							  ephy_location_controller_get_address (controller),
							  certificate,
							  tls_errors);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (certificate_dialog), TRUE);
	g_signal_connect (certificate_dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);
	gtk_widget_show (certificate_dialog);
}

static GtkWidget *
setup_toolbar (EphyWindow *window)
{
	GtkWidget *toolbar;
	GtkAction *action;
	EphyWindowPrivate *priv = window->priv;

	toolbar = ephy_toolbar_new (window);
	gtk_box_pack_start (GTK_BOX (priv->main_vbox),
			    toolbar, FALSE, FALSE, 0);

	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "NavigationBack");
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), window);

	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "NavigationForward");
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), window);

	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "FileNewTab");
	g_signal_connect_swapped (action, "open-link",
				  G_CALLBACK (ephy_link_open), window);

	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "Zoom");
	g_signal_connect (action, "zoom-to-level",
			  G_CALLBACK (zoom_to_level_cb), window);

	return toolbar;
}

static const char* disabled_actions_for_app_mode[] = { "FileOpen",
                                                       "FileSaveAs",
                                                       "FileSaveAsApplication",
                                                       "ViewEncoding",
                                                       "FileBookmarkPage" };

static gboolean
_gtk_css_provider_load_from_resource (GtkCssProvider* provider,
				      const char *resource_path,
				      GError **error)
{
	GBytes *data;
	gboolean res;
	
	g_return_val_if_fail (GTK_IS_CSS_PROVIDER (provider), FALSE);
	g_return_val_if_fail (resource_path != NULL, FALSE);

	data = g_resources_lookup_data (resource_path, 0, error);
	if (data == NULL)
		return FALSE;

	res = gtk_css_provider_load_from_data (provider,
					       g_bytes_get_data (data, NULL),
					       g_bytes_get_size (data),
					       error);
	g_bytes_unref (data);
	
	return res;
}

static const gchar* app_actions[] = {
	"FileNewWindow",
	"FileNewWindowIncognito",
	"EditPreferences",
	"EditPersonalData",
	"EditBookmarks",
	"EditHistory",
	"FileQuit",
	"HelpAbout"
};

static void
ephy_window_toggle_visibility_for_app_menu (EphyWindow *window)
{
	const gchar *action_name;
	gboolean shows_app_menu;
	GtkSettings *settings;
	GtkAction *action;
	gint idx;

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window)));
	g_object_get (settings,
		      "gtk-shell-shows-app-menu", &shows_app_menu,
		      NULL);

	for (idx = 0; idx < G_N_ELEMENTS (app_actions); idx++) {
		action_name = app_actions[idx];
		action = gtk_action_group_get_action (window->priv->action_group, action_name);

		gtk_action_set_visible (action, !shows_app_menu);
	}
}

static GObject *
ephy_window_constructor (GType type,
			 guint n_construct_properties,
			 GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyWindow *window;
	EphyWindowPrivate *priv;
	EphyEmbedSingle *single;
	GtkSettings *settings;
	GtkAction *action;
	GtkActionGroup *toolbar_action_group;
	GError *error = NULL;
	guint settings_connection;
	GtkCssProvider *css_provider;
	int i;
	EphyEmbedShellMode mode;

	object = G_OBJECT_CLASS (ephy_window_parent_class)->constructor
		(type, n_construct_properties, construct_params);

	window = EPHY_WINDOW (object);

	priv = window->priv;

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
	setup_tab_accels (window);

	/* Create the notebook. */
	/* FIXME: the notebook needs to exist before the toolbar,
	 * because EphyLocationEntry uses it... */
	priv->notebook = setup_notebook (window);

	/* Setup the toolbar. */
	priv->toolbar = setup_toolbar (window);
	priv->location_controller =
		g_object_new (EPHY_TYPE_LOCATION_CONTROLLER,
			      "window", window,
			      "location-entry", ephy_toolbar_get_location_entry (EPHY_TOOLBAR (priv->toolbar)),
			      NULL);
	g_signal_connect (priv->location_controller, "notify::address",
			  G_CALLBACK (sync_user_input_cb), window);
	g_signal_connect_swapped (priv->location_controller, "open-link",
				  G_CALLBACK (ephy_link_open), window);
	g_signal_connect (priv->location_controller, "lock-clicked",
			  G_CALLBACK (lock_clicked_cb), window);

	priv->find_toolbar = ephy_find_toolbar_new (window);
	g_signal_connect (priv->find_toolbar, "close",
			  G_CALLBACK (find_toolbar_close_cb), window);

	gtk_box_pack_start (GTK_BOX (priv->main_vbox),
			    GTK_WIDGET (priv->find_toolbar), FALSE, FALSE, 0);

	g_signal_connect_swapped (priv->notebook, "open-link",
				  G_CALLBACK (ephy_link_open), window);
	gtk_box_pack_start (GTK_BOX (priv->main_vbox),
			    GTK_WIDGET (priv->notebook),
			    TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (priv->notebook));

	priv->downloads_box = setup_downloads_box (window);
	gtk_box_pack_start (GTK_BOX (priv->main_vbox),
			    GTK_WIDGET (priv->downloads_box), FALSE, FALSE, 0);
	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewDownloadsBar");

	g_object_bind_property (action, "active",
				priv->downloads_box, "visible",
				G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	
	/* Now load the UI definition. */
	gtk_ui_manager_add_ui_from_resource (priv->manager,
					     "/org/gnome/epiphany/epiphany-ui.xml",
					     &error);
	if (error != NULL)
	{
		g_warning ("Could not merge epiphany-ui.xml: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* Attach the CSS provider to the window. */
	css_provider = gtk_css_provider_new ();
	_gtk_css_provider_load_from_resource (css_provider,
					      "/org/gnome/epiphany/epiphany.css",
					      &error);
	if (error == NULL)
	{
		gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (window)),
		                                           GTK_STYLE_PROVIDER (css_provider),
		                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	else
	{
		g_warning ("Could not attach css style: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (css_provider);

	/* Initialize the menus */
	priv->enc_menu = ephy_encoding_menu_new (window);

	ephy_bookmarks_ui_attach_window (window);

	/* other notifiers */
	action = gtk_action_group_get_action (window->priv->action_group,
					      "BrowseWithCaret");

	g_settings_bind (EPHY_SETTINGS_MAIN,
			 EPHY_PREFS_ENABLE_CARET_BROWSING,
			 action, "active",
			 G_SETTINGS_BIND_GET);

	g_signal_connect (EPHY_SETTINGS_WEB,
			  "changed::" EPHY_PREFS_WEB_ENABLE_POPUPS,
			  G_CALLBACK (allow_popups_notifier), window);

	g_signal_connect (EPHY_SETTINGS_UI,
			  "changed::" EPHY_PREFS_UI_SHOW_TOOLBARS,
			  G_CALLBACK (show_toolbars_setting_cb), window);

	/* network status */
	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (ephy_embed_shell_get_default ()));
	sync_network_status (single, NULL, window);
	g_signal_connect (single, "notify::network-status",
			  G_CALLBACK (sync_network_status), window);

	/* Disable actions not needed for popup mode. */
	toolbar_action_group = priv->toolbar_action_group;
	action = gtk_action_group_get_action (toolbar_action_group, "FileNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME,
					      priv->is_popup);

	action = gtk_action_group_get_action (priv->popups_action_group, "OpenLinkInNewTab");
	ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME,
					      priv->is_popup);

	/* Disabled actions not needed for application mode. */
	mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());
	if (mode == EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		/* FileNewTab and FileNewWindow are sort of special. */
		action = gtk_action_group_get_action (toolbar_action_group, "FileNewTab");
		ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME,
						      TRUE);

		for (i = 0; i < G_N_ELEMENTS (disabled_actions_for_app_mode); i++)
		{
			action = gtk_action_group_get_action (priv->action_group,
							      disabled_actions_for_app_mode[i]);
			ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, TRUE);
		}
	}

	/* We never want the menubar shown, we merge the app menu into
	 * our super menu manually when running outside the Shell. */
	gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), FALSE);

	ephy_window_toggle_visibility_for_app_menu (window);
	priv->app_menu_visibility_handler =  g_signal_connect_swapped (gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window))),
								       "notify::gtk-shell-shows-app-menu",
								       G_CALLBACK (ephy_window_toggle_visibility_for_app_menu), window);

	/* ensure the UI is updated */
	gtk_ui_manager_ensure_update (priv->manager);

	init_menu_updaters (window);

	sync_chromes_visibility (window);

	return object;
}

static void
ephy_window_show (GtkWidget *widget)
{
	EphyWindow *window = EPHY_WINDOW(widget);
	EphyWindowPrivate *priv = window->priv;

	if (!priv->has_size)
	{
		EphyEmbed *embed;
		int flags = 0;

		embed = priv->active_embed;
		g_return_if_fail (EPHY_IS_EMBED (embed));

		if (!priv->is_popup)
			flags = EPHY_STATE_WINDOW_SAVE_SIZE;

		ephy_state_add_window (widget, "main_window", 600, 500,
				       TRUE, flags);
		priv->has_size = TRUE;
	}

	GTK_WIDGET_CLASS (ephy_window_parent_class)->show (widget);
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
	widget_class->window_state_event = ephy_window_state_event;
	widget_class->delete_event = ephy_window_delete_event;

	g_object_class_override_property (object_class,
					  PROP_ACTIVE_CHILD,
					  "active-child");

	g_object_class_override_property (object_class,
					  PROP_SINGLE_TAB_MODE,
					  "is-popup");

	g_object_class_override_property (object_class,
					  PROP_CHROME,
					  "chrome");

	g_type_class_add_private (object_class, sizeof (EphyWindowPrivate));
}

static void 
maybe_finish_activation_cb (EphyWindow *window,
			    GtkWidget *widget,
			    GtkWidget *toolbar)
{
	while (widget != NULL && widget != toolbar)
	{
		widget = gtk_widget_get_parent (widget);
	}

	/* if widget == toolbar, the new focus widget is in the toolbar, so we
	 * don't deactivate.
	 */
	if (widget != toolbar)
	{
		g_signal_handler_disconnect (window, window->priv->set_focus_handler);
		window->priv->set_focus_handler = 0;
		sync_chromes_visibility (window);
	}
}

static void
_ephy_window_activate_location (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	GtkWidget *entry;
	gboolean visible;

	entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (priv->toolbar));

	g_object_get (G_OBJECT (priv->toolbar), "visible", &visible, NULL);
	if (visible == FALSE)
	{
		gtk_widget_show (GTK_WIDGET (priv->toolbar));
		window->priv->set_focus_handler =
			g_signal_connect (window, "set-focus",
					  G_CALLBACK (maybe_finish_activation_cb),
					  priv->toolbar);
	}

	ephy_location_entry_activate (EPHY_LOCATION_ENTRY (entry));
}

static void
ephy_window_init (EphyWindow *window)
{
	LOG ("EphyWindow initialising %p", window);

	window->priv = EPHY_WINDOW_GET_PRIVATE (window);

	g_signal_connect (ephy_embed_shell_get_default (),
			 "download-added", G_CALLBACK (download_added_cb),
			 window);

	gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (window),
						     TRUE);
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
	return g_object_new (EPHY_TYPE_WINDOW,
			     "application", GTK_APPLICATION (ephy_shell_get_default ()),
			     NULL);
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
	return g_object_new (EPHY_TYPE_WINDOW,
			     "chrome", chrome,
			     "is-popup", is_popup,
			     "application", GTK_APPLICATION (ephy_shell_get_default ()),
			     NULL);
}

/**
 * ephy_window_get_ui_manager:
 * @window: an #EphyWindow
 *
 * Returns this window's UI manager.
 *
 * Return value: (transfer none): an #GtkUIManager
 **/
GtkUIManager *
ephy_window_get_ui_manager (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return window->priv->manager;
}

/**
 * ephy_window_get_notebook:
 * @window: an #EphyWindow
 *
 * Returns the #GtkNotebook used by this window.
 *
 * Return value: (transfer none): the @window's #GtkNotebook
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
 * Return value: (transfer none): the @window's #EphyFindToolbar
 **/
GtkWidget *
ephy_window_get_find_toolbar (EphyWindow *window)
{
       g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

       return GTK_WIDGET (window->priv->find_toolbar);
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
	_ephy_window_activate_location (window);
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
	double current_zoom = 1.0;
	WebKitWebView *web_view;

	g_return_if_fail (EPHY_IS_WINDOW (window));

	embed = window->priv->active_embed;
	g_return_if_fail (embed != NULL);

	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	current_zoom = webkit_web_view_get_zoom_level (web_view);

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
		webkit_web_view_set_zoom_level (web_view, zoom);
	}
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

	g_object_set (G_OBJECT (ephy_embed_get_web_view (embed)), "popups-allowed", allow, NULL);
}

/**
 * ephy_window_get_context_event:
 * @window: an #EphyWindow
 *
 * Returns the #EphyEmbedEvent for the current context menu.
 * Use this to get the event from the action callback.
 *
 * Return value: (transfer none): an #EphyEmbedEvent, or %NULL
 **/
EphyEmbedEvent *
ephy_window_get_context_event (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return window->priv->context_event;
}

/**
 * ephy_window_get_location:
 * @window: an #EphyWindow widget
 *
 * Gets the current address according to @window's #EphyLocationController.
 *
 * Returns: current @window address
 **/
const char *
ephy_window_get_location (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	return ephy_location_controller_get_address (priv->location_controller);
}

/**
 * ephy_window_set_location:
 * @window: an #EphyWindow widget
 * @address: new address
 *
 * Sets the internal #EphyLocationController address to @address.
 **/
void
ephy_window_set_location (EphyWindow *window,
			  const char *address)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->updating_address) return;

	priv->updating_address = TRUE;
	ephy_location_controller_set_address (priv->location_controller, address);
	priv->updating_address = FALSE;
}

/**
 * ephy_window_get_toolbar_action_group:
 * @window: an #EphyWindow
 * 
 * Returns the toolbar #GtkActionGroup for this @window
 * 
 * Returns: (transfer none): the #GtkActionGroup for this @window's
 * toolbar actions
 **/
GtkActionGroup *
ephy_window_get_toolbar_action_group (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return window->priv->toolbar_action_group;
}

/**
 * ephy_window_get_location_controller:
 * @window: an #EphyWindow
 * 
 * Returns the @window #EphyLocationController
 * 
 * Returns: (transfer none): the @window #EphyLocationController
 **/
EphyLocationController *
ephy_window_get_location_controller (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

	return window->priv->location_controller;
}

/**
 * ephy_window_is_on_current_workspace:
 * @window: an #EphyWindow
 *
 * Returns whether @window is on the current workspace
 *
 * Returns: %TRUE if the window is on the current workspace, %FALSE otherwise
 **/
gboolean
ephy_window_is_on_current_workspace (EphyWindow *window)
{
	GdkWindow *gdk_window = NULL;
	WnckWorkspace *workspace = NULL;
	WnckWindow *wnck_window = NULL;

	if (!gtk_widget_get_realized (GTK_WIDGET (window)))
		return TRUE;

	workspace = wnck_screen_get_active_workspace (wnck_screen_get_default ());

	/* From WNCK docs:
	 * "May return NULL sometimes, if libwnck is in a weird state due to
	 *  the asynchronous nature of the interaction with the window manager."
	 * In such a case we cannot really check, so assume we are.
	 */
	if (!workspace)
		return TRUE;

	gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
	wnck_window = wnck_window_get (GDK_WINDOW_XID (gdk_window));

	return wnck_window_is_on_workspace (wnck_window, workspace);
}

/**
 * ephy_window_close:
 * @window: an #EphyWindow
 *
 * Try to close the window. The window might refuse to close
 * if there are ongoing download operations or unsubmitted
 * modifed forms.
 *
 * Returns: %TRUE if the window is closed, or %FALSE otherwise
 **/
gboolean
ephy_window_close (EphyWindow *window)
{
	EphyEmbed *modified_embed = NULL;
	GList *tabs, *l;
	gboolean modified = FALSE;

	/* We ignore the delete_event if the disable_quit lockdown has been set
	 */
	if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
				    EPHY_PREFS_LOCKDOWN_QUIT)) return FALSE;

	tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyEmbed *embed = (EphyEmbed *) l->data;

		g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);

		if (ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed)))
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
			return FALSE;
		}
	}


	if (window_has_ongoing_downloads (window) && confirm_close_with_downloads (window) == FALSE)
	{
		/* stop window close */
		return FALSE;
	}

	/* If this is the last window, save its state in the session. */
	if (ephy_shell_get_n_windows (ephy_shell_get_default ()) == 1)
	{
		ephy_session_close (EPHY_SESSION (ephy_shell_get_session (ephy_shell_get_default ())));
	}

	/* See bug #114689 */
	gtk_widget_hide (GTK_WIDGET (window));

	return TRUE;
}
