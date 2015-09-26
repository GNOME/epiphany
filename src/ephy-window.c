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
#include "ephy-certificate-popover.h"
#include "ephy-combined-stop-reload-action.h"
#include "ephy-debug.h"
#include "ephy-download-widget.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-encoding-menu.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-gui.h"
#include "ephy-home-action.h"
#include "ephy-initial-state.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-navigation-history-action.h"
#include "ephy-notebook.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"
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

#include <webkit2/webkit2.h>

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
	{ "EditPreferences", NULL, N_("Pr_eferences"), "<control>e", NULL,
	  G_CALLBACK (window_cmd_edit_preferences) },

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

	{ "HelpContents", NULL, N_("_Help"), NULL, NULL,
	  G_CALLBACK (window_cmd_help_contents) },
	{ "HelpAbout", NULL, N_("_About"), NULL, NULL,
	  G_CALLBACK (window_cmd_help_about) }
};

static const GtkToggleActionEntry ephy_menu_toggle_entries [] =
{
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

	{ "OpenLinkInNewWindow", NULL, N_("Open Link in New _Window"), NULL, NULL,
	  G_CALLBACK (popup_cmd_link_in_new_window) },
	{ "OpenLinkInNewTab", NULL, N_("Open Link in New _Tab"), NULL, NULL,
	  G_CALLBACK (popup_cmd_link_in_new_tab) },
	{ "OpenLinkInIncognitoWindow", NULL, N_("Open Link in I_ncognito Window"), NULL, NULL,
	  G_CALLBACK (popup_cmd_link_in_incognito_window) },
	{ "DownloadLinkAs", NULL, N_("_Save Link As…"), NULL, NULL,
	  G_CALLBACK (popup_cmd_download_link_as) },
	{ "CopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_address) },
	{ "CopyEmailAddress", NULL, N_("_Copy E-mail Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_link_address) },

	/* Images. */

	{ "ViewImage", NULL, N_("View _Image in New Tab"), NULL,
	  NULL, G_CALLBACK (popup_cmd_view_image_in_new_tab) },
	{ "CopyImageLocation", NULL, N_("Copy I_mage Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_image_location) },
	{ "SaveImageAs", NULL, N_("_Save Image As…"), NULL,
	  NULL, G_CALLBACK (popup_cmd_save_image_as) },
	{ "SetImageAsBackground", NULL, N_("Set as _Wallpaper"), NULL,
	  NULL, G_CALLBACK (popup_cmd_set_image_as_background) },

	/* Video. */

	{ "OpenVideoInNewWindow", NULL, N_("Open Video in New _Window"), NULL, NULL,
	  G_CALLBACK (popup_cmd_media_in_new_window) },
	{ "OpenVideoInNewTab", NULL, N_("Open Video in New _Tab"), NULL, NULL,
	  G_CALLBACK (popup_cmd_media_in_new_tab) },
	{ "SaveVideoAs", NULL, N_("_Save Video As…"), NULL,
	  NULL, G_CALLBACK (popup_cmd_save_media_as) },
	{ "CopyVideoLocation", NULL, N_("_Copy Video Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_media_location) },

	/* Audio. */

	{ "OpenAudioInNewWindow", NULL, N_("Open Audio in New _Window"), NULL, NULL,
	  G_CALLBACK (popup_cmd_media_in_new_window) },
	{ "OpenAudioInNewTab", NULL, N_("Open Audio in New _Tab"), NULL, NULL,
	  G_CALLBACK (popup_cmd_media_in_new_tab) },
	{ "SaveAudioAs", NULL, N_("_Save Audio As…"), NULL,
	  NULL, G_CALLBACK (popup_cmd_save_media_as) },
	{ "CopyAudioLocation", NULL, N_("_Copy Audio Address"), NULL,
	  NULL, G_CALLBACK (popup_cmd_copy_media_location) },

	/* Selection */
	{ "SearchSelection", NULL, "_Search Selection", NULL, NULL,
	  G_CALLBACK (popup_cmd_search_selection) },
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
	{ GDK_KEY_r,		GDK_CONTROL_MASK,	"ViewReload",		 FALSE },
	{ GDK_KEY_R,		GDK_CONTROL_MASK,	"ViewReload",		 FALSE },
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
	{ GDK_KEY_F6,           0,			"GoLocation",            FALSE },
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
	{ GDK_KEY_Left,		GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KEY_KP_Left,	GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KEY_KP_4,		GDK_MOD1_MASK /*Alt*/,	"NavigationBack",	TRUE },
	{ GDK_KEY_Right,	GDK_MOD1_MASK /*Alt*/,	"NavigationForward",	TRUE },
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
	EphyWindowChrome chrome;
	EphyEmbedEvent *context_event;
	WebKitHitTestResult *hit_test_result;
	guint idle_worker;
	GtkWidget *downloads_box;

	EphyLocationController *location_controller;

	gulong app_menu_visibility_handler;

	guint closing : 1;
	guint has_size : 1;
	guint fullscreen_mode : 1;
	guint is_popup : 1;
	guint present_on_insert : 1;
	guint key_theme_is_emacs : 1;
	guint updating_address : 1;
	guint force_close : 1;
	guint checking_modified_forms : 1;
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

static void
ephy_window_embed_container_iface_init (EphyEmbedContainerIface *iface)
{
	iface->add_child = impl_add_child;
	iface->set_active_child = impl_set_active_child;
	iface->remove_child = impl_remove_child;
	iface->get_active_child = impl_get_active_child;
	iface->get_children = impl_get_children;
	iface->get_is_popup = impl_get_is_popup;
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
		EphyNewTabFlags ntflags = 0;
		EphyWindow *target_window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed)));

		if (flags & EPHY_LINK_JUMP_TO)
		{
			ntflags |= EPHY_NEW_TAB_JUMP;
		}

		if (flags & EPHY_LINK_NEW_WINDOW ||
		    (flags & EPHY_LINK_NEW_TAB && priv->is_popup))
		{
			target_window = ephy_window_new ();
		}

		if (flags & EPHY_LINK_NEW_TAB_APPEND_AFTER)
			ntflags |= EPHY_NEW_TAB_APPEND_AFTER;

		new_embed = ephy_shell_new_tab
				(ephy_shell_get_default (),
				 target_window,
				 embed, ntflags);
		if (flags & EPHY_LINK_HOME_PAGE)
		{
			ephy_web_view_load_homepage (ephy_embed_get_web_view (new_embed));
			ephy_window_activate_location (window);
		}
		else
		{
			ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), address);
			if (flags & EPHY_LINK_JUMP_TO)
			{
				gtk_widget_grab_focus (GTK_WIDGET (new_embed));
			}
		}
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
sync_chromes_visibility (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	gboolean show_tabsbar, show_downloads_box;

	if (priv->closing)
		return;

	show_tabsbar = (priv->chrome & EPHY_WINDOW_CHROME_TABSBAR);
	show_downloads_box = (priv->chrome & EPHY_WINDOW_CHROME_DOWNLOADS_BOX);

	ephy_notebook_set_tabs_allowed (EPHY_NOTEBOOK (priv->notebook),
					show_tabsbar && !(priv->is_popup || priv->fullscreen_mode));
	gtk_widget_set_visible (priv->downloads_box,
				show_downloads_box && !priv->fullscreen_mode);
}

static void
ephy_window_set_chrome (EphyWindow *window,
			EphyWindowChrome chrome)
{
	if (window->priv->chrome == chrome)
		return;

	window->priv->chrome = chrome;
	if (window->priv->closing)
		return;

	g_object_notify (G_OBJECT (window), "chrome");
	sync_chromes_visibility (window);
}

static void
sync_tab_load_status (EphyWebView *view,
		      WebKitLoadEvent load_event,
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
sync_tab_security (EphyWebView *view,
		   GParamSpec *pspec,
		   EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyTitleBox *title_box;
	EphySecurityLevel security_level;

	if (priv->closing) return;

	ephy_web_view_get_security_level (view, &security_level, NULL, NULL);
	title_box = ephy_toolbar_get_title_box (EPHY_TOOLBAR (priv->toolbar));
	ephy_title_box_set_security_level (title_box, security_level);
}

static void
ephy_window_fullscreen (EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	EphyEmbed *embed;

	priv->fullscreen_mode = TRUE;

	/* sync status */
	embed = window->priv->active_embed;
	sync_tab_load_status (ephy_embed_get_web_view (embed), WEBKIT_LOAD_STARTED, window);
	sync_tab_security (ephy_embed_get_web_view (embed), NULL, window);

	sync_chromes_visibility (window);
	gtk_widget_hide (priv->toolbar);
	ephy_embed_entering_fullscreen (embed);
}

static void
ephy_window_unfullscreen (EphyWindow *window)
{
	window->priv->fullscreen_mode = FALSE;

	gtk_widget_show (window->priv->toolbar);
	sync_chromes_visibility (window);
	ephy_embed_leaving_fullscreen (window->priv->active_embed);
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

	if ((event->state & GDK_CONTROL_MASK ||
	     event->state & GDK_MOD1_MASK ||
             event->state & GDK_SHIFT_MASK) &&
             event->length > 0)
        {
		/* Pass (CTRL|ALT|SHIFT)+letter characters to the widget */
		shortcircuit = TRUE;
        }
	else if (event->keyval == GDK_KEY_Escape && modifier == 0)
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

	action = gtk_action_group_get_action (action_group, "OpenLinkInIncognitoWindow");
	gtk_action_set_sensitive (action, link_has_web_scheme);
}

static void
update_edit_action_sensitivity (EphyWindow *window, const gchar *action_name, gboolean sensitive, gboolean hide)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, action_name);
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_visible (action, !hide || sensitive);
}

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
		CanEditCommandAsyncData *data;

		embed = window->priv->active_embed;
		g_return_if_fail (embed != NULL);

		view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

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
	GtkAccelGroup *accel_group;
	GtkAction *action;
	GtkUIManager *manager;

	window->priv->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (window->priv->main_vbox);
	gtk_container_add (GTK_CONTAINER (window),
			   window->priv->main_vbox);

	manager = gtk_ui_manager_new ();
	accel_group = gtk_ui_manager_get_accel_group (manager);

	action_group = gtk_action_group_new ("WindowActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, ephy_menu_entries,
				      G_N_ELEMENTS (ephy_menu_entries), window);
	gtk_action_group_add_toggle_actions (action_group,
					     ephy_menu_toggle_entries,
					     G_N_ELEMENTS (ephy_menu_toggle_entries),
					     window);
	gtk_action_group_set_accel_group (action_group, accel_group);
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
	gtk_action_group_set_accel_group (action_group, accel_group);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->popups_action_group = action_group;
	g_object_unref (action_group);

	/* Tab accels */
	action_group = gtk_action_group_new ("TabAccelsActions");
	gtk_action_group_set_accel_group (action_group, accel_group);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->tab_accels_action_group = action_group;
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("SpecialToolbarActions");
	action =
		g_object_new (EPHY_TYPE_NAVIGATION_HISTORY_ACTION,
			      "name", "NavigationBack",
			      "label", _("Back"),
			      "icon-name", "go-previous-symbolic",
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
			      "icon-name", "go-next-symbolic",
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
			       "icon-name", "tab-new-symbolic",
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

	gtk_action_group_set_accel_group (action_group, accel_group);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->toolbar_action_group = action_group;
	g_object_unref (action_group);

	window->priv->manager = manager;
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
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

	/* Toolbar */
	action = gtk_action_group_get_action (priv->toolbar_action_group,
					      "ViewCombinedStopReload");
	ephy_action_change_sensitivity_flags (action,
					      flags, set);
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

	address = ephy_web_view_get_display_address (view);
	typed_address = ephy_web_view_get_typed_address (view);

	_ephy_window_set_default_actions_sensitive (window,
						    SENS_FLAG_IS_BLANK,
						    ephy_web_view_is_overview (view));

	location = calculate_location (typed_address, address);
	ephy_window_set_location (window, location);
	g_free (location);
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
		ephy_find_toolbar_request_close (ephy_embed_get_find_toolbar (priv->active_embed));
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
sync_tab_title (EphyEmbed *embed,
		GParamSpec *pspec,
		EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (priv->closing) return;

	gtk_window_set_title (GTK_WINDOW(window),
			      ephy_embed_get_title (embed));
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

static void
add_action_to_context_menu (WebKitContextMenu *context_menu,
			    GtkActionGroup *action_group,
			    const char *action_name)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group, action_name);
	webkit_context_menu_append (context_menu, webkit_context_menu_item_new (action));
}

static void
add_item_to_context_menu (WebKitContextMenu *context_menu,
			  WebKitContextMenuItem *item)
{
	if (!item)
		return;

	webkit_context_menu_append (context_menu, item);
	g_object_unref (item);
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

static GList *
find_spelling_guess_context_menu_items (WebKitContextMenu *context_menu)
{
	GList *items, *iter;
	guint i;
	GList *retval = NULL;

	items = webkit_context_menu_get_items (context_menu);
	for (iter = items, i = 0; iter && i < MAX_SPELL_CHECK_GUESSES; iter = g_list_next (iter), i++)
	{
		WebKitContextMenuItem *item = (WebKitContextMenuItem *)iter->data;

		if (webkit_context_menu_item_get_stock_action (item) == WEBKIT_CONTEXT_MENU_ACTION_SPELLING_GUESS)
		{
			retval = g_list_prepend (retval, g_object_ref (item));
		}
		else
		{
			/* Spelling guesses are always at the beginning of the context menu, so
			 * we can break the loop as soon as we find the first item that is not
			 * spelling guess.
			 */
			break;
		}
	}

	return g_list_reverse (retval);
}

static char *
ellipsize_string (const char *string,
		  glong max_length)
{
	char *ellipsized;
	glong length = g_utf8_strlen (string, -1);

	if (length == 0)
		return NULL;

	if (length < max_length)
	{
		ellipsized = g_strdup (string);
	}
	else
	{
		char *str = g_utf8_substring (string, 0, max_length);
		ellipsized = g_strconcat (str, "…", NULL);
		g_free (str);
	}
	return ellipsized;
}

static void
parse_context_menu_user_data (WebKitContextMenu *context_menu,
			      const char** selected_text)
{
	GVariantDict dict;

	g_variant_dict_init (&dict, webkit_context_menu_get_user_data (context_menu));
	g_variant_dict_lookup (&dict, "SelectedText", "&s", selected_text);
}

static gboolean
populate_context_menu (WebKitWebView *web_view,
		       WebKitContextMenu *context_menu,
		       GdkEvent *event,
		       WebKitHitTestResult *hit_test_result,
		       EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;
	WebKitContextMenuItem *input_methods_item = NULL;
	WebKitContextMenuItem *unicode_item = NULL;
	WebKitContextMenuItem *play_pause_item = NULL;
	WebKitContextMenuItem *mute_item = NULL;
	WebKitContextMenuItem *toggle_controls_item = NULL;
	WebKitContextMenuItem *toggle_loop_item = NULL;
	WebKitContextMenuItem *fullscreen_item = NULL;
	GList *spelling_guess_items = NULL;
	EphyEmbedEvent *embed_event;
	gboolean is_document = FALSE;
	gboolean app_mode, incognito_mode;
	gboolean is_image;
	gboolean is_media = FALSE;
	gboolean is_video = FALSE;
	gboolean is_audio = FALSE;
	gboolean can_search_selection = FALSE;
	const char *selected_text = NULL;

	is_image = webkit_hit_test_result_context_is_image (hit_test_result);

	if (webkit_hit_test_result_context_is_editable (hit_test_result)) {
		input_methods_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_INPUT_METHODS);
		unicode_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_UNICODE);
		spelling_guess_items = find_spelling_guess_context_menu_items (context_menu);
	}

	if (webkit_hit_test_result_context_is_media (hit_test_result))
	{
		WebKitContextMenuItem *item;

		is_media = TRUE;
		play_pause_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PLAY);
		if (!play_pause_item)
			play_pause_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PAUSE);
		mute_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_MUTE);
		toggle_controls_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_CONTROLS);
		toggle_loop_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_LOOP);
		fullscreen_item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_ENTER_VIDEO_FULLSCREEN);

		item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_COPY_VIDEO_LINK_TO_CLIPBOARD);
		if (item)
		{
			is_video = TRUE;
			g_object_unref (item);
		}
		else
		{
			item = find_item_in_context_menu (context_menu, WEBKIT_CONTEXT_MENU_ACTION_COPY_AUDIO_LINK_TO_CLIPBOARD);
			if (item)
			{
				is_audio = TRUE;
				g_object_unref (item);
			}
		}
	}

	parse_context_menu_user_data (context_menu, &selected_text);
	if (selected_text)
	{
		char* ellipsized = ellipsize_string (selected_text, 32);
		if (ellipsized)
		{
			char* label;
			GtkAction *action;

			can_search_selection = TRUE;
			action = gtk_action_group_get_action (priv->popups_action_group,
							      "SearchSelection");
			label = g_strdup_printf (_("Search the Web for '%s'"), ellipsized);
			gtk_action_set_label (action, label);
			g_object_set_data_full (G_OBJECT (action), "selection", g_strdup (selected_text),
						(GDestroyNotify)g_free);
			g_free (ellipsized);
			g_free (label);
			can_search_selection = TRUE;
		}
	}

	webkit_context_menu_remove_all (context_menu);

	embed_event = ephy_embed_event_new ((GdkEventButton *)event, hit_test_result);
	_ephy_window_set_context_event (window, embed_event);
	g_object_unref (embed_event);

	app_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION;
	incognito_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO;

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
						    priv->popups_action_group, "OpenLinkInNewTab");
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "OpenLinkInNewWindow");
			if (!incognito_mode)
				add_action_to_context_menu (context_menu,
							    priv->popups_action_group, "OpenLinkInIncognitoWindow");
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
		}
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditCopy");
		if (can_search_selection)
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "SearchSelection");
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "DownloadLinkAs");

		if (g_str_has_prefix (uri, "mailto:"))
		{
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "CopyEmailAddress");
		}
		else
		{
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "CopyLinkAddress");
		}
	}
	else if (webkit_hit_test_result_context_is_editable (hit_test_result))
	{
		GList *l;
		gboolean has_guesses = FALSE;

		for (l = spelling_guess_items; l; l = g_list_next (l))
		{
			WebKitContextMenuItem *item = WEBKIT_CONTEXT_MENU_ITEM (l->data);

			webkit_context_menu_append (context_menu, item);
			g_object_unref (item);
			has_guesses = TRUE;
		}
		g_list_free (spelling_guess_items);

		if (has_guesses)
		{
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
		}

		update_edit_actions_sensitivity (window, FALSE);

		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditUndo");
		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditRedo");
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
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
		add_item_to_context_menu (context_menu, input_methods_item);
		add_item_to_context_menu (context_menu, unicode_item);
	}
	else
	{
		is_document = TRUE;

		update_edit_actions_sensitivity (window, TRUE);

		if (!is_image && !is_media)
		{
			add_action_to_context_menu (context_menu,
						    priv->toolbar_action_group, "NavigationBack");
			add_action_to_context_menu (context_menu,
						    priv->toolbar_action_group, "NavigationForward");
			add_action_to_context_menu (context_menu,
						    priv->action_group, "ViewReload");
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
		}

		add_action_to_context_menu (context_menu,
					    priv->action_group, "EditCopy");
		if (can_search_selection)
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "SearchSelection");

		if (!app_mode && !is_image && !is_media)
		{
			webkit_context_menu_append (context_menu,
						    webkit_context_menu_item_new_separator ());
			add_action_to_context_menu (context_menu,
						    priv->popups_action_group, "ContextBookmarkPage");
		}
	}

	if (is_image)
	{
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "SaveImageAs");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "CopyImageLocation");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "ViewImage");
		add_action_to_context_menu (context_menu,
					    priv->popups_action_group, "SetImageAsBackground");
	}

	if (is_media)
	{
		add_item_to_context_menu (context_menu, play_pause_item);
		add_item_to_context_menu (context_menu, mute_item);
		add_item_to_context_menu (context_menu, toggle_controls_item);
		add_item_to_context_menu (context_menu, toggle_loop_item);
		add_item_to_context_menu (context_menu, fullscreen_item);
		webkit_context_menu_append (context_menu,
					    webkit_context_menu_item_new_separator ());
		if (is_video)
		{
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "OpenVideoInNewWindow");
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "OpenVideoInNewTab");
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "SaveVideoAs");
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "CopyVideoLocation");
		}
		else if (is_audio)
		{
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "OpenAudioInNewWindow");
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "OpenAudioInNewTab");
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "SaveAudioAs");
			add_action_to_context_menu (context_menu, priv->popups_action_group,
						    "CopyAudioLocation");
		}
	}

	g_signal_connect (web_view, "context-menu-dismissed",
			  G_CALLBACK (context_menu_dismissed_cb),
			  window);

	if (app_mode)
		return FALSE;

	if (is_document && !is_image && !is_media)
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

	return FALSE;
}

static gboolean
save_target_uri (EphyWindow *window,
		 WebKitWebView *view,
		 GdkEventButton *event,
		 WebKitHitTestResult *hit_test_result)
{
	guint context;
	char *location = NULL;
	gboolean retval = FALSE;

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

	if (location)
	{
		LOG ("Location: %s", location);

		retval = ephy_embed_utils_address_has_web_scheme (location);
		if (retval)
		{
			EphyDownload *download;

			download = ephy_download_new_for_uri (location, GTK_WINDOW (window));
			ephy_download_set_action (download, EPHY_DOWNLOAD_ACTION_OPEN);
			ephy_window_add_download (window, download);
			g_object_unref (download);
		}

		g_free (location);
	}

	return retval;
}

static gboolean
ephy_window_dom_mouse_click_cb (WebKitWebView *view,
				GdkEventButton *event,
				EphyWindow *window)
{
	WebKitHitTestResult *hit_test_result;
	gboolean handled = FALSE;

	/* Since we're only dealing with shift+click, we can do these
	   checks early. */
	if (!(event->state & GDK_SHIFT_MASK) || event->button != GDK_BUTTON_PRIMARY)
	{
		return FALSE;
	}

	hit_test_result = g_object_ref (window->priv->hit_test_result);
	handled = save_target_uri (window, view, event, hit_test_result);
	g_object_unref (hit_test_result);

	return handled;
}

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

static void
ephy_window_set_is_popup (EphyWindow *window,
			  gboolean is_popup)
{
	EphyWindowPrivate *priv = window->priv;

	priv->is_popup = is_popup;

	g_object_notify (G_OBJECT (window), "is-popup");
}

static void
window_properties_geometry_changed (WebKitWindowProperties *properties,
				    GParamSpec *pspec,
				    EphyWindow *window)
{
	GdkRectangle geometry;

	webkit_window_properties_get_geometry (properties, &geometry);
	if (geometry.x >= 0 && geometry.y >= 0)
		gtk_window_move (GTK_WINDOW (window), geometry.x, geometry.y);
	if (geometry.width > 0 && geometry.height > 0)
		gtk_window_resize (GTK_WINDOW (window), geometry.width, geometry.height);
}

static void
ephy_window_configure_for_view (EphyWindow *window,
				WebKitWebView *web_view)
{
	WebKitWindowProperties *properties;
	GdkRectangle geometry;
	EphyWindowChrome chrome = 0;

	properties = webkit_web_view_get_window_properties (web_view);

	if (webkit_window_properties_get_toolbar_visible (properties))
		chrome |= EPHY_WINDOW_CHROME_TOOLBAR;

	if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		GtkWidget *entry;

		entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (window->priv->toolbar));
		gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);

		if (webkit_window_properties_get_menubar_visible (properties))
			chrome |= EPHY_WINDOW_CHROME_MENU;
		if (webkit_window_properties_get_locationbar_visible (properties))
			chrome |= EPHY_WINDOW_CHROME_LOCATION;
	}

	webkit_window_properties_get_geometry (properties, &geometry);
	if (geometry.width > 0 && geometry.height > 0)
		gtk_window_set_default_size (GTK_WINDOW (window), geometry.width, geometry.height);

	if (!webkit_window_properties_get_resizable (properties))
		gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

	window->priv->is_popup = TRUE;
	ephy_window_set_chrome (window, chrome);
	g_signal_connect (properties, "notify::geometry",
			  G_CALLBACK (window_properties_geometry_changed),
			  window);
}

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

	if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION &&
	    !webkit_web_view_get_uri(web_view))
	{
		/* Wait until we have a valid URL to decide whether to show the window
		 * or load the URL in the default web browser
		 */
		g_object_set_data_full (G_OBJECT (window), "referrer",
					g_strdup (webkit_web_view_get_uri (parent_web_view)),
					g_free);
		return TRUE;
	}

	gtk_widget_show (GTK_WIDGET (window));

	return TRUE;
}

static WebKitWebView *
create_web_view_cb (WebKitWebView *web_view,
		    WebKitNavigationAction *navigation_action,
		    EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *new_web_view;
	EphyNewTabFlags flags;
	EphyWindow *target_window;

	if ((ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_APPLICATION) &&
	    (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
				     EPHY_PREFS_NEW_WINDOWS_IN_TABS) ||
	     g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
				     EPHY_PREFS_LOCKDOWN_FULLSCREEN)))
	{
		target_window = window;
		flags = EPHY_NEW_TAB_JUMP |
			EPHY_NEW_TAB_APPEND_AFTER;
	}
	else
	{
		target_window = ephy_window_new ();
		flags = EPHY_NEW_TAB_DONT_SHOW_WINDOW;
	}

	embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
					 NULL,
					 web_view,
					 target_window,
					 EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
					 flags,
					 0);
	if (target_window == window)
		gtk_widget_grab_focus (GTK_WIDGET (embed));

	new_web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	g_signal_connect (new_web_view, "ready-to-show",
			  G_CALLBACK (web_view_ready_cb),
			  web_view);

	return new_web_view;
}

static gboolean
decide_policy_cb (WebKitWebView *web_view,
		  WebKitPolicyDecision *decision,
		  WebKitPolicyDecisionType decision_type,
		  EphyWindow *window)
{
	WebKitNavigationPolicyDecision *navigation_decision;
	WebKitNavigationAction *navigation_action;
	WebKitNavigationType navigation_type;
	WebKitURIRequest *request;
	const char *uri;
	EphyEmbed *embed;

	if (decision_type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
		return FALSE;

	navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
	navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
	request = webkit_navigation_action_get_request (navigation_action);
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

	if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
	{
		const char *frame_name = webkit_navigation_policy_decision_get_frame_name (navigation_decision);

		if (g_strcmp0 (frame_name, "_evince_download") == 0)
		{
			/* The Evince Browser Plugin is requesting us to downlod the document */
			webkit_policy_decision_download (decision);
			return TRUE;
		}

		if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_POPUPS) &&
		    !webkit_navigation_action_is_user_gesture (navigation_action))
		{
			webkit_policy_decision_ignore (decision);
			return TRUE;
		}
	}

	navigation_type = webkit_navigation_action_get_navigation_type (navigation_action);

	if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION)
	{
		if (!gtk_widget_is_visible (GTK_WIDGET (window)))
		{
			char *referrer;

			referrer = (char *)g_object_get_data (G_OBJECT (window), "referrer");

			if (ephy_embed_utils_urls_have_same_origin (uri, referrer))
			{
				gtk_widget_show (GTK_WIDGET (window));
			}
			else
			{
				ephy_file_open_uri_in_default_browser (uri, GDK_CURRENT_TIME,
								       gtk_window_get_screen (GTK_WINDOW (window)));
				webkit_policy_decision_ignore (decision);

				gtk_widget_destroy (GTK_WIDGET (window));

				return TRUE;
			}
		}

		if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED)
		{
			if (ephy_embed_utils_urls_have_same_origin (uri, webkit_web_view_get_uri (web_view)))
			{
				return FALSE;
			}

			ephy_file_open_uri_in_default_browser (uri, GDK_CURRENT_TIME,
							       gtk_window_get_screen (GTK_WINDOW (window)));
			webkit_policy_decision_ignore (decision);

			return TRUE;
		}
	}

	if (navigation_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED)
	{
		gint button;
		gint state;
		EphyEmbed *new_embed;
		EphyNewTabFlags flags = 0;
		EphyWindow *target_window = window;

		button = webkit_navigation_action_get_mouse_button (navigation_action);
		state = webkit_navigation_action_get_modifiers (navigation_action);

		ephy_web_view_set_visit_type (EPHY_WEB_VIEW (web_view),
					      EPHY_PAGE_VISIT_LINK);

		/* New tab in new window for control+shift+click */
		if (button == 1 && state == (GDK_SHIFT_MASK | GDK_CONTROL_MASK) &&
		    !g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
					     EPHY_PREFS_LOCKDOWN_FULLSCREEN))
		{
			target_window = ephy_window_new ();
		}
		/* New tab in existing window for middle click and
		 * control+click */
		else if (button == 2 || (button == 1 && state == GDK_CONTROL_MASK))
		{
			flags |= EPHY_NEW_TAB_APPEND_AFTER;
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

		new_embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
						     NULL, NULL,
						     target_window,
						     embed,
						     flags,
						     0);
		ephy_web_view_load_request (ephy_embed_get_web_view (new_embed), request);

		webkit_policy_decision_ignore (decision);

		return TRUE;
	}

	return FALSE;
}

static void
ephy_window_connect_active_embed (EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *web_view;
	EphyWebView *view;

	g_return_if_fail (window->priv->active_embed != NULL);

	embed = window->priv->active_embed;
	view = ephy_embed_get_web_view (embed);
	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	sync_tab_security	(view, NULL, window);
	sync_tab_document_type	(view, NULL, window);
	sync_tab_load_status    (view, WEBKIT_LOAD_STARTED, window);
	sync_tab_is_blank	(view, NULL, window);
	sync_tab_navigation	(view, NULL, window);
	sync_tab_title		(embed, NULL, window);
	sync_tab_address	(view, NULL, window);
	sync_tab_icon		(view, NULL, window);
	sync_tab_popup_windows	(view, NULL, window);
	sync_tab_popups_allowed	(view, NULL, window);

	sync_tab_zoom		(web_view, NULL, window);

	g_signal_connect_object (web_view, "notify::zoom-level",
				 G_CALLBACK (sync_tab_zoom),
				 window, 0);

	g_signal_connect_object (web_view, "create",
				 G_CALLBACK (create_web_view_cb),
				 window, 0);
	g_signal_connect_object (web_view, "decide-policy",
				 G_CALLBACK (decide_policy_cb),
				 window, 0);
	g_signal_connect_object (view, "notify::hidden-popup-count",
				 G_CALLBACK (sync_tab_popup_windows),
				 window, 0);
	g_signal_connect_object (view, "notify::popups-allowed",
				 G_CALLBACK (sync_tab_popups_allowed),
				 window, 0);
	g_signal_connect_object (embed, "notify::title",
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
	g_signal_connect_object (view, "load-changed",
				 G_CALLBACK (sync_tab_load_status),
				 window, 0);
	g_signal_connect_object (view, "notify::navigation",
				 G_CALLBACK (sync_tab_navigation),
				 window, 0);
	g_signal_connect_object (view, "notify::is-blank",
				 G_CALLBACK (sync_tab_is_blank),
				 window, 0);
	g_signal_connect_object (view, "button-press-event",
				 G_CALLBACK (ephy_window_dom_mouse_click_cb),
				 window, 0);
	g_signal_connect_object (view, "context-menu",
				 G_CALLBACK (populate_context_menu),
				 window, 0);
	g_signal_connect_object (view, "mouse-target-changed",
				 G_CALLBACK (ephy_window_mouse_target_changed_cb),
				 window, 0);

	g_object_notify (G_OBJECT (window), "active-child");
}

static void
ephy_window_disconnect_active_embed (EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *web_view;
	EphyWebView *view;

	g_return_if_fail (window->priv->active_embed != NULL);

	embed = window->priv->active_embed;
	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	view = EPHY_WEB_VIEW (web_view);

	g_signal_handlers_disconnect_by_func (web_view,
					      G_CALLBACK (sync_tab_zoom),
					      window);
	g_signal_handlers_disconnect_by_func (web_view,
					      G_CALLBACK (create_web_view_cb),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (decide_policy_cb),
					      window);
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
	g_signal_handlers_disconnect_by_func (embed,
					      G_CALLBACK (sync_tab_title),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (sync_tab_address),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (sync_tab_icon),
					      window);

	g_signal_handlers_disconnect_by_func
		(view, G_CALLBACK (ephy_window_dom_mouse_click_cb), window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (populate_context_menu),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (ephy_window_mouse_target_changed_cb),
					      window);
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
	address = ephy_web_view_get_display_address (ephy_embed_get_web_view (embed));
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
		gint n_pages, page_num;

		tab = GTK_WIDGET (window->priv->active_embed);
		n_pages = gtk_notebook_get_n_pages (notebook);
		page_num = gtk_notebook_page_num (notebook, tab);

		/* enable/disable move left/right items*/
		action = gtk_action_group_get_action (window->priv->action_group,
						      "TabsMoveLeft");
		gtk_action_set_sensitive (action, page_num > 0);

		action = gtk_action_group_get_action (window->priv->action_group,
						      "TabsMoveRight");
		gtk_action_set_sensitive (action, page_num < n_pages - 1);

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

static gboolean
delayed_remove_child (gpointer data)
{
	GtkWidget *widget = GTK_WIDGET (data);
	EphyEmbedContainer *container = EPHY_EMBED_CONTAINER (gtk_widget_get_toplevel (widget));

	ephy_embed_container_remove_child (container, EPHY_EMBED (widget));

	return FALSE;
}

static void
download_only_load_cb (EphyWebView *view,
		       EphyWindow *window)
{
	EphyWindowPrivate *priv = window->priv;

	if (gtk_notebook_get_n_pages (priv->notebook) == 1) {
		ephy_web_view_load_homepage (view);
		return;
	}

	g_idle_add (delayed_remove_child, EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view));
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

	g_signal_connect_object (ephy_embed_get_web_view (embed), "download-only-load",
				 G_CALLBACK (download_only_load_cb), window, G_CONNECT_AFTER);

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
		(ephy_embed_get_web_view (embed), G_CALLBACK (download_only_load_cb), window);

	g_signal_handlers_disconnect_by_func
		(ephy_embed_get_web_view (embed), G_CALLBACK (embed_modal_alert_cb), window);

	tab_accels_update (window);
}

static void
ephy_window_close_tab (EphyWindow *window,
		       EphyEmbed *tab)
{
	gtk_widget_destroy (GTK_WIDGET (tab));

	/* If that was the last tab, destroy the window. */
	if (gtk_notebook_get_n_pages (window->priv->notebook) == 0)
	{
		gtk_widget_destroy (GTK_WIDGET (window));
	}
}

static void
tab_has_modified_forms_cb (EphyWebView *view,
			   GAsyncResult *result,
			   EphyWindow *window)
{
	gboolean has_modified_forms;

	has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
	if (!has_modified_forms || confirm_close_with_modified_forms (window))
	{
		ephy_window_close_tab (window, EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view));
	}
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

	if (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
				    EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA))
	{
		ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
						  NULL,
						  (GAsyncReadyCallback)tab_has_modified_forms_cb,
						  window);
	}
	else
	{
		ephy_window_close_tab (window, embed);
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

	ephy_title_box_set_web_view (ephy_toolbar_get_title_box (EPHY_TOOLBAR (priv->toolbar)),
				     EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
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

	g_signal_connect_swapped (notebook, "open-link",
				  G_CALLBACK (ephy_link_open), window);

	return notebook;
}

static void
ephy_window_set_downloads_box_visibility (EphyWindow *window,
					  gboolean show)
{
	EphyWindowChrome chrome = window->priv->chrome;

	if (show)
		chrome |= EPHY_WINDOW_CHROME_DOWNLOADS_BOX;
	else
		chrome &= ~EPHY_WINDOW_CHROME_DOWNLOADS_BOX;

	ephy_window_set_chrome (window, chrome);
}

void
ephy_window_add_download (EphyWindow *window,
			  EphyDownload *download)
{
	GtkWidget *widget;

	widget = ephy_download_widget_new (download);
	gtk_box_pack_start (GTK_BOX (window->priv->downloads_box),
			    widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	ephy_window_set_downloads_box_visibility (window, TRUE);
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

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	close_button = gtk_button_new ();
	gtk_container_set_border_width (GTK_CONTAINER (close_button), 6);
	gtk_widget_set_valign (close_button, GTK_ALIGN_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (close_button), "image-button");
	gtk_style_context_add_class (gtk_widget_get_style_context (close_button), "close");
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "download-box");

	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);

	image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
	g_object_set (image, "use-fallback", TRUE, NULL);

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
	GSList *popups;

	LOG ("EphyWindow dispose %p", window);

	/* Only do these once */
	if (window->priv->closing == FALSE)
	{
		window->priv->closing = TRUE;

		ephy_bookmarks_ui_detach_window (window);

		/* Deactivate menus */
		popups = gtk_ui_manager_get_toplevels (window->priv->manager, GTK_UI_MANAGER_POPUP);
		g_slist_foreach (popups, (GFunc) gtk_menu_shell_deactivate, NULL);
		g_slist_free (popups);
	
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

	if (priv->app_menu_visibility_handler != 0)
		g_signal_handler_disconnect (gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window))),
					     priv->app_menu_visibility_handler);

	G_OBJECT_CLASS (ephy_window_parent_class)->finalize (object);

	LOG ("EphyWindow finalised %p", object);
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
open_certificate_popover (EphyWindow *window,
			  GtkWidget *relative_to,
			  GdkRectangle *lock_position)
{
	EphyWindowPrivate *priv = window->priv;
	EphyWebView *view;
	GTlsCertificate *certificate;
	GTlsCertificateFlags tls_errors;
	EphySecurityLevel security_level;
	GtkWidget *location_entry;
	GtkWidget *certificate_popover;

	view = ephy_embed_get_web_view (priv->active_embed);
	ephy_web_view_get_security_level (view, &security_level, &certificate, &tls_errors);
	location_entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (priv->toolbar));

	certificate_popover = ephy_certificate_popover_new (relative_to,
							    ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (location_entry)),
							    certificate,
							    tls_errors,
							    security_level);

	g_signal_connect (certificate_popover, "closed",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_popover_set_pointing_to (GTK_POPOVER (certificate_popover), lock_position);
	gtk_widget_show (certificate_popover);
}

static void
location_controller_lock_clicked_cb (EphyLocationController *controller,
				     gpointer user_data)
{
	EphyWindow *window = EPHY_WINDOW (user_data);
	EphyWindowPrivate *priv = window->priv;
	GtkWidget *location_entry;
	GdkRectangle lock_position;

	location_entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (priv->toolbar));
	gtk_entry_get_icon_area (GTK_ENTRY (location_entry), GTK_ENTRY_ICON_SECONDARY, &lock_position);
	open_certificate_popover (window, location_entry, &lock_position);
}

static void
title_box_lock_clicked_cb (EphyTitleBox *title_box,
			   GdkRectangle *lock_position,
			   gpointer user_data)
{
	EphyWindow *window = EPHY_WINDOW (user_data);

	open_certificate_popover (window, GTK_WIDGET (title_box), lock_position);
}

static GtkWidget *
setup_toolbar (EphyWindow *window)
{
	GtkWidget *toolbar;
	GtkAction *action;
	EphyWindowPrivate *priv = window->priv;
	EphyEmbedShellMode app_mode;
	EphyTitleBox *title_box;

	toolbar = ephy_toolbar_new (window);
	gtk_window_set_titlebar (GTK_WINDOW (window), toolbar);
	gtk_widget_show (toolbar);

	app_mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());
	if (app_mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)
		gtk_style_context_add_class (gtk_widget_get_style_context (toolbar), "incognito-mode");

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

	title_box = ephy_toolbar_get_title_box (EPHY_TOOLBAR (toolbar));
	g_signal_connect (title_box, "lock-clicked",
                          G_CALLBACK (title_box_lock_clicked_cb), window);

	return toolbar;
}

static EphyLocationController *
setup_location_controller (EphyWindow *window,
			   EphyToolbar *toolbar)
{
	EphyLocationController *location_controller;

	location_controller =
		g_object_new (EPHY_TYPE_LOCATION_CONTROLLER,
			      "window", window,
			      "location-entry", ephy_toolbar_get_location_entry (toolbar),
			      "title-box", ephy_toolbar_get_title_box (toolbar),
			      NULL);
	g_signal_connect (location_controller, "notify::address",
			  G_CALLBACK (sync_user_input_cb), window);
	g_signal_connect_swapped (location_controller, "open-link",
				  G_CALLBACK (ephy_link_open), window);
	g_signal_connect (location_controller, "lock-clicked",
			  G_CALLBACK (location_controller_lock_clicked_cb), window);

	return location_controller;
}

static const char* disabled_actions_for_app_mode[] = { "FileOpen",
						       "FileNewWindow",
						       "FileNewWindowIncognito",
						       "FileSaveAs",
						       "FileSaveAsApplication",
						       "ViewEncoding",
						       "ViewPageSource",
						       "FileBookmarkPage",
						       "EditBookmarks",
						       "EditHistory",
						       "EditPreferences"};

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
	"EditBookmarks",
	"EditHistory",
	"FileQuit",
	"HelpContents",
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
	GtkSettings *settings;
	GtkAction *action;
	GtkActionGroup *toolbar_action_group;
	GError *error = NULL;
	guint settings_connection;
	GtkCssProvider *css_provider;
	int i;
	EphyEmbedShellMode mode;
	EphyWindowChrome chrome = EPHY_WINDOW_CHROME_DEFAULT;

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

	priv->notebook = setup_notebook (window);

	/* Now load the UI definition (needed by EphyToolbar). */
	gtk_ui_manager_add_ui_from_resource (priv->manager,
					     "/org/gnome/epiphany/epiphany-ui.xml",
					     &error);
	if (error != NULL)
	{
		g_warning ("Could not merge epiphany-ui.xml: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* Setup the toolbar. */
	priv->toolbar = setup_toolbar (window);
	priv->location_controller = setup_location_controller (window, EPHY_TOOLBAR (priv->toolbar));
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
		g_object_set(priv->location_controller, "editable", FALSE, NULL);

		/* We don't need to show the page menu in web application mode. */
		action = gtk_action_group_get_action (toolbar_action_group, "PageMenu");
		ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, TRUE);
		gtk_action_set_visible (action, FALSE);

		action = gtk_action_group_get_action (toolbar_action_group, "FileNewTab");
		ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME,
						      TRUE);
		gtk_action_set_visible (action, FALSE);

		action = gtk_action_group_get_action (priv->popups_action_group, "ContextBookmarkPage");
		ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, TRUE);
		gtk_action_set_visible (action, FALSE);

		for (i = 0; i < G_N_ELEMENTS (disabled_actions_for_app_mode); i++)
		{
			action = gtk_action_group_get_action (priv->action_group,
							      disabled_actions_for_app_mode[i]);
			ephy_action_change_sensitivity_flags (action, SENS_FLAG_CHROME, TRUE);
			gtk_action_set_visible (action, FALSE);
		}
		chrome &= ~(EPHY_WINDOW_CHROME_MENU | EPHY_WINDOW_CHROME_TABSBAR);
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

	ephy_window_set_chrome (window, chrome);

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
			flags = EPHY_INITIAL_STATE_WINDOW_SAVE_SIZE;

		ephy_initial_state_add_window (widget, "main_window", 600, 500,
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

	g_object_class_install_property (object_class,
					 PROP_CHROME,
					 g_param_spec_flags ("chrome",
							     NULL,
							     NULL,
							     EPHY_TYPE_WINDOW_CHROME,
							     EPHY_WINDOW_CHROME_DEFAULT,
							     G_PARAM_READWRITE |
							     G_PARAM_STATIC_NAME |
							     G_PARAM_STATIC_NICK |
							     G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyWindowPrivate));
}

static void
ephy_window_init (EphyWindow *window)
{
	LOG ("EphyWindow initialising %p", window);

	window->priv = EPHY_WINDOW_GET_PRIVATE (window);
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
			     "icon-name", "web-browser",
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
ephy_window_get_current_find_toolbar (EphyWindow *window)
{
       g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

       return GTK_WIDGET (ephy_embed_get_find_toolbar (window->priv->active_embed));
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
	EphyWindowPrivate *priv = window->priv;
	GtkWidget *entry;

	if (!(priv->chrome & EPHY_WINDOW_CHROME_LOCATION))
		return;

	ephy_title_box_set_mode (ephy_toolbar_get_title_box (EPHY_TOOLBAR (priv->toolbar)),
				 EPHY_TITLE_BOX_MODE_LOCATION_ENTRY);

	entry = ephy_toolbar_get_location_entry (EPHY_TOOLBAR (priv->toolbar));
	ephy_location_entry_activate (EPHY_LOCATION_ENTRY (entry));
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
 * @address: a decoded URI, suitable for display to the user
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

	gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

	if(!GDK_IS_X11_WINDOW (gdk_window))
		return TRUE;

	workspace = wnck_screen_get_active_workspace (wnck_screen_get_default ());

	/* From WNCK docs:
	 * "May return NULL sometimes, if libwnck is in a weird state due to
	 *  the asynchronous nature of the interaction with the window manager."
	 * In such a case we cannot really check, so assume we are.
	 */
	if (!workspace)
		return TRUE;

	wnck_window = wnck_window_get (GDK_WINDOW_XID (gdk_window));

	return wnck_window_is_on_workspace (wnck_window, workspace);
}

typedef struct {
	EphyWindow *window;
	GCancellable *cancellable;

	guint embeds_to_check;
	EphyEmbed *modified_embed;
} ModifiedFormsData;

static void
modified_forms_data_free (ModifiedFormsData *data)
{
	g_object_unref (data->cancellable);

	g_slice_free (ModifiedFormsData, data);
}

static void
continue_window_close_after_modified_forms_check (ModifiedFormsData *data)
{
	gboolean should_close;

	data->window->priv->checking_modified_forms = FALSE;

	if (data->modified_embed)
	{
		/* jump to the first tab with modified forms */
		impl_set_active_child (EPHY_EMBED_CONTAINER (data->window),
				       data->modified_embed);
		if (!confirm_close_with_modified_forms (data->window))
			return;
	}

	data->window->priv->force_close = TRUE;
	should_close = ephy_window_close (data->window);
	data->window->priv->force_close = FALSE;
	if (should_close)
		gtk_widget_destroy (GTK_WIDGET (data->window));
}

static void
has_modified_forms_cb (EphyWebView *view,
		       GAsyncResult *result,
		       ModifiedFormsData *data)
{
	gboolean has_modified_forms;

	data->embeds_to_check--;
	has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
	if (has_modified_forms)
	{
		/* Cancel all others */
		g_cancellable_cancel (data->cancellable);
		data->modified_embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);
	}

	if (data->embeds_to_check > 0)
		return;

	continue_window_close_after_modified_forms_check (data);
	modified_forms_data_free (data);
}

static void
ephy_window_check_modified_forms (EphyWindow *window)
{
	GList *tabs, *l;
	ModifiedFormsData *data;

	window->priv->checking_modified_forms = TRUE;

	data = g_slice_new0 (ModifiedFormsData);
	data->window = window;
	data->cancellable = g_cancellable_new ();
	data->embeds_to_check = gtk_notebook_get_n_pages (window->priv->notebook);

	tabs = impl_get_children (EPHY_EMBED_CONTAINER (window));
	for (l = tabs; l != NULL; l = l->next)
	{
		EphyEmbed *embed = (EphyEmbed *) l->data;

		ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
						  data->cancellable,
						  (GAsyncReadyCallback)has_modified_forms_cb,
						  data);
	}
	g_list_free (tabs);
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
	/* We ignore the delete_event if the disable_quit lockdown has been set
	 */
	if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
				    EPHY_PREFS_LOCKDOWN_QUIT)) return FALSE;

	if (window->priv->checking_modified_forms) {
		/* stop window close */
		return FALSE;
	}

	if (!window->priv->force_close &&
	    g_settings_get_boolean (EPHY_SETTINGS_MAIN,
				    EPHY_PREFS_WARN_ON_CLOSE_UNSUBMITTED_DATA) &&
	    gtk_notebook_get_n_pages (window->priv->notebook) > 0)
	{
		ephy_window_check_modified_forms (window);
		/* stop window close */
		return FALSE;
	}

	if (window_has_ongoing_downloads (window) && confirm_close_with_downloads (window) == FALSE)
	{
		/* stop window close */
		return FALSE;
	}

	/* If this is the last window, save its state in the session. */
	if (ephy_shell_get_n_windows (ephy_shell_get_default ()) == 1)
	{
		ephy_session_close (ephy_shell_get_session (ephy_shell_get_default ()));
	}

	/* See bug #114689 */
	gtk_widget_hide (GTK_WIDGET (window));

	return TRUE;
}

EphyWindowChrome
ephy_window_get_chrome (EphyWindow *window)
{
	g_return_val_if_fail (EPHY_IS_WINDOW (window), EPHY_WINDOW_CHROME_DEFAULT);

	return window->priv->chrome;
}
