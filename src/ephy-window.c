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
#include "statusbar.h"
#include "toolbar.h"
#include "ppview-toolbar.h"
#include "window-commands.h"
#include "find-dialog.h"
#include "history-dialog.h"
#include "popup-commands.h"
#include "ephy-shell.h"
#include "ephy-bonobo-extensions.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-utils.h"

#include <string.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-ui-component.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <gtk/gtk.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#define CHARSET_MENU_PATH "/menu/View/EncodingMenuPlaceholder"
#define GO_FAVORITES_PATH "/menu/Go/Favorites"

#define GO_BACK_CMD_PATH "/commands/GoBack"
#define GO_FORWARD_CMD_PATH "/commands/GoForward"
#define GO_UP_CMD_PATH "/commands/GoUp"
#define EDIT_FIND_NEXT_CMD_PATH "/commands/EditFindNext"
#define EDIT_FIND_PREV_CMD_PATH "/commands/EditFindPrev"
#define VIEW_MENUBAR_PATH "/commands/View Menubar"
#define VIEW_STATUSBAR_PATH "/commands/View Statusbar"
#define VIEW_TOOLBAR_PATH "/commands/View Toolbar"
#define VIEW_BOOKMARKSBAR_PATH "/commands/View BookmarksBar"
#define VIEW_FULLSCREEN_PATH "/commands/View Fullscreen"

#define ID_VIEW_MENUBAR "View Menubar"
#define ID_VIEW_STATUSBAR "View Statusbar"
#define ID_VIEW_TOOLBAR "View Toolbar"
#define ID_VIEW_BOOKMARKSBAR "View BookmarksBar"
#define ID_VIEW_FULLSCREEN "View Fullscreen"

static BonoboUIVerb ephy_verbs [] = {
        BONOBO_UI_VERB ("EditFind", (BonoboUIVerbFn)window_cmd_edit_find),
	BONOBO_UI_VERB ("FilePrint", (BonoboUIVerbFn)window_cmd_file_print),
	BONOBO_UI_VERB ("GoStop", (BonoboUIVerbFn)window_cmd_go_stop),
	BONOBO_UI_VERB ("GoReload", (BonoboUIVerbFn)window_cmd_go_reload),
	BONOBO_UI_VERB ("GoBack", (BonoboUIVerbFn)window_cmd_go_back),
	BONOBO_UI_VERB ("GoForward", (BonoboUIVerbFn)window_cmd_go_forward),
	BONOBO_UI_VERB ("GoGo", (BonoboUIVerbFn)window_cmd_go_go),
	BONOBO_UI_VERB ("GoUp", (BonoboUIVerbFn)window_cmd_go_up),
	BONOBO_UI_VERB ("GoHome", (BonoboUIVerbFn)window_cmd_go_home),
	BONOBO_UI_VERB ("GoMyportal", (BonoboUIVerbFn)window_cmd_go_myportal),
	BONOBO_UI_VERB ("GoLocation", (BonoboUIVerbFn)window_cmd_go_location),
	BONOBO_UI_VERB ("FileNew", (BonoboUIVerbFn)window_cmd_new),
	BONOBO_UI_VERB ("FileNewWindow", (BonoboUIVerbFn)window_cmd_new_window),
	BONOBO_UI_VERB ("FileNewTab", (BonoboUIVerbFn)window_cmd_new_tab),
	BONOBO_UI_VERB ("FileOpen", (BonoboUIVerbFn)window_cmd_file_open),
	BONOBO_UI_VERB ("FileSaveAs", (BonoboUIVerbFn)window_cmd_file_save_as),
	BONOBO_UI_VERB ("FileCloseTab", (BonoboUIVerbFn)window_cmd_file_close_tab),
	BONOBO_UI_VERB ("FileCloseWindow", (BonoboUIVerbFn)window_cmd_file_close_window),
	BONOBO_UI_VERB ("FileSendTo", (BonoboUIVerbFn)window_cmd_file_send_to),
	BONOBO_UI_VERB ("EditCut", (BonoboUIVerbFn)window_cmd_edit_cut),
	BONOBO_UI_VERB ("EditCopy", (BonoboUIVerbFn)window_cmd_edit_copy),
	BONOBO_UI_VERB ("EditPaste", (BonoboUIVerbFn)window_cmd_edit_paste),
	BONOBO_UI_VERB ("EditSelectAll", (BonoboUIVerbFn)window_cmd_edit_select_all),
	BONOBO_UI_VERB ("EditPrefs", (BonoboUIVerbFn)window_cmd_edit_prefs),
	BONOBO_UI_VERB ("SettingsToolbarEditor", (BonoboUIVerbFn)window_cmd_settings_toolbar_editor),
	BONOBO_UI_VERB ("Zoom In", (BonoboUIVerbFn)window_cmd_view_zoom_in),
	BONOBO_UI_VERB ("EditFindNext", (BonoboUIVerbFn)window_cmd_edit_find_next),
	BONOBO_UI_VERB ("EditFindPrev", (BonoboUIVerbFn)window_cmd_edit_find_prev),
	BONOBO_UI_VERB ("Zoom Out", (BonoboUIVerbFn)window_cmd_view_zoom_out),
	BONOBO_UI_VERB ("Zoom Normal", (BonoboUIVerbFn)window_cmd_view_zoom_normal),
	BONOBO_UI_VERB ("ViewPageSource", (BonoboUIVerbFn)window_cmd_view_page_source),
	BONOBO_UI_VERB ("BookmarksAddDefault", (BonoboUIVerbFn)window_cmd_bookmarks_add_default),
	BONOBO_UI_VERB ("BookmarksEdit", (BonoboUIVerbFn)window_cmd_bookmarks_edit),
	BONOBO_UI_VERB ("ToolsHistory", (BonoboUIVerbFn)window_cmd_tools_history),
	BONOBO_UI_VERB ("ToolsPDM", (BonoboUIVerbFn)window_cmd_tools_pdm),
	BONOBO_UI_VERB ("TabsNext", (BonoboUIVerbFn)window_cmd_tabs_next),
	BONOBO_UI_VERB ("TabsPrevious", (BonoboUIVerbFn)window_cmd_tabs_previous),
	BONOBO_UI_VERB ("TabsMoveLeft", (BonoboUIVerbFn)window_cmd_tabs_move_left),
	BONOBO_UI_VERB ("TabsMoveRight", (BonoboUIVerbFn)window_cmd_tabs_move_right),
	BONOBO_UI_VERB ("TabsDetach", (BonoboUIVerbFn)window_cmd_tabs_detach),
	BONOBO_UI_VERB ("HelpContents", (BonoboUIVerbFn)window_cmd_help_manual),
	BONOBO_UI_VERB ("About", (BonoboUIVerbFn)window_cmd_help_about),

        BONOBO_UI_VERB_END
};

static BonoboUIVerb ephy_popup_verbs [] = {
        BONOBO_UI_VERB ("EPOpenInNewWindow", (BonoboUIVerbFn)popup_cmd_new_window),
	BONOBO_UI_VERB ("EPOpenInNewTab", (BonoboUIVerbFn)popup_cmd_new_tab),
	BONOBO_UI_VERB ("EPAddBookmark", (BonoboUIVerbFn)popup_cmd_add_bookmark),
	BONOBO_UI_VERB ("EPOpenImageInNewWindow", (BonoboUIVerbFn)popup_cmd_image_in_new_window),
	BONOBO_UI_VERB ("EPOpenImageInNewTab", (BonoboUIVerbFn)popup_cmd_image_in_new_tab),
	BONOBO_UI_VERB ("DPOpenFrameInNewWindow", (BonoboUIVerbFn)popup_cmd_frame_in_new_window),
	BONOBO_UI_VERB ("DPOpenFrameInNewTab", (BonoboUIVerbFn)popup_cmd_frame_in_new_tab),
	BONOBO_UI_VERB ("DPAddFrameBookmark", (BonoboUIVerbFn)popup_cmd_add_frame_bookmark),
	BONOBO_UI_VERB ("DPViewSource", (BonoboUIVerbFn)popup_cmd_view_source),

        BONOBO_UI_VERB_END
};

struct EphyWindowPrivate
{
	EphyFavoritesMenu *fav_menu;
	PPViewToolbar *ppview_toolbar;
	Toolbar *toolbar;
	Statusbar *statusbar;
	GtkNotebook *notebook;
	EphyTab *active_tab;
	GtkWidget *sidebar;
	EphyEmbedPopupBW *embed_popup;
	EphyDialog *find_dialog;
	EphyDialog *history_dialog;
	EphyDialog *history_sidebar;
	EmbedChromeMask chrome_mask;
	gboolean ignore_layout_toggles;
	gboolean has_default_size;
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

                ephy_window_type = g_type_register_static (BONOBO_TYPE_WINDOW,
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

static
gboolean
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
save_window_state (GtkWidget *win)
{
	EphyWindow *window = EPHY_WINDOW (win);

	if (!(window->priv->chrome_mask & EMBED_CHROME_OPENASPOPUP) &&
	    !(window->priv->chrome_mask & EMBED_CHROME_OPENASFULLSCREEN))
	{
		ephy_state_save_window (win, "main_window");
	}
}

static gboolean
ephy_window_configure_event_cb (GtkWidget *widget,
				GdkEventConfigure *event,
				gpointer data)
{
	save_window_state (widget);
	return FALSE;
}

static gboolean
ephy_window_state_event_cb (GtkWidget *widget,
			    GdkEventWindowState *event,
			    gpointer data)
{
	save_window_state (widget);
	return FALSE;
}

static void
setup_bonobo_window (EphyWindow *window,
		     BonoboUIComponent **ui_component)
{
	BonoboWindow *win = BONOBO_WINDOW(window);
	BonoboUIContainer *container;
	Bonobo_UIContainer corba_container;

	container = bonobo_window_get_ui_container (win);

        bonobo_ui_engine_config_set_path (bonobo_window_get_ui_engine (win),
                                          "/apps/ephy/UIConfig/kvps");

        corba_container = BONOBO_OBJREF (container);

	*ui_component = bonobo_ui_component_new_default ();

	bonobo_ui_component_set_container (*ui_component,
					   corba_container,
					   NULL);

	bonobo_ui_util_set_ui (*ui_component,
			       DATADIR,
			       "epiphany-ui.xml",
			       "ephy", NULL);

	/* Key handling */
	g_signal_connect(G_OBJECT(win),
			 "key-press-event",
                         G_CALLBACK(ephy_window_key_press_event_cb),
                         window);

	g_signal_connect (G_OBJECT(win), "configure_event",
			  G_CALLBACK (ephy_window_configure_event_cb), NULL);
	g_signal_connect (G_OBJECT(win), "window_state_event",
			  G_CALLBACK (ephy_window_state_event_cb), NULL);

	g_signal_connect (G_OBJECT(win),
			  "selection-received",
			  G_CALLBACK (ephy_window_selection_received_cb),
			  window);
}

static EphyEmbedPopupBW *
setup_popup_factory (EphyWindow *window,
		     BonoboUIComponent *ui_component)
{
	EphyEmbedPopupBW *popup;

	popup = ephy_window_get_popup_factory (window);
	g_object_set_data (G_OBJECT(popup), "EphyWindow", window);
	bonobo_ui_component_add_verb_list_with_data (ui_component,
						     ephy_popup_verbs,
						     popup);

	return popup;
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
view_toolbar_changed_cb (BonoboUIComponent *component,
                         const char *path,
                         Bonobo_UIComponent_EventType type,
                         const char *state,
                         EphyWindow *window)
{
	EmbedChromeMask mask;

	if (window->priv->ignore_layout_toggles) return;

        mask = ephy_window_get_chrome (window);
        mask ^= EMBED_CHROME_TOOLBARON;
        ephy_window_set_chrome (window, mask);
}

static void
view_statusbar_changed_cb (BonoboUIComponent *component,
                           const char *path,
                           Bonobo_UIComponent_EventType type,
                           const char *state,
                           EphyWindow *window)
{
	EmbedChromeMask mask;

	if (window->priv->ignore_layout_toggles) return;

        mask = ephy_window_get_chrome (window);
        mask ^= EMBED_CHROME_STATUSBARON;
        ephy_window_set_chrome (window, mask);
}

static void
view_fullscreen_changed_cb (BonoboUIComponent *component,
                            const char *path,
                            Bonobo_UIComponent_EventType type,
                            const char *state,
                            EphyWindow *window)
{
	EmbedChromeMask mask;

	if (window->priv->ignore_layout_toggles) return;

	mask = ephy_window_get_chrome (window);
	mask ^= EMBED_CHROME_OPENASFULLSCREEN;
	mask |= EMBED_CHROME_DEFAULT;
	ephy_window_set_chrome (window, mask);
}

static void
update_layout_toggles (EphyWindow *window)
{
	BonoboUIComponent *ui_component = BONOBO_UI_COMPONENT (window->ui_component);
	EmbedChromeMask mask = window->priv->chrome_mask;

	window->priv->ignore_layout_toggles = TRUE;

	ephy_bonobo_set_toggle_state (ui_component, VIEW_TOOLBAR_PATH,
				     mask & EMBED_CHROME_TOOLBARON);
	ephy_bonobo_set_toggle_state (ui_component, VIEW_STATUSBAR_PATH,
				     mask & EMBED_CHROME_STATUSBARON);
	ephy_bonobo_set_toggle_state (ui_component, VIEW_FULLSCREEN_PATH,
				     mask & EMBED_CHROME_OPENASFULLSCREEN);

	window->priv->ignore_layout_toggles = FALSE;
}

static void
setup_layout_menus (EphyWindow *window)
{
	BonoboUIComponent *ui_component = BONOBO_UI_COMPONENT (window->ui_component);

	bonobo_ui_component_add_listener (ui_component, ID_VIEW_TOOLBAR,
					  (BonoboUIListenerFn)view_toolbar_changed_cb,
					  window);
	bonobo_ui_component_add_listener (ui_component, ID_VIEW_STATUSBAR,
					  (BonoboUIListenerFn)view_statusbar_changed_cb,
					  window);
	bonobo_ui_component_add_listener (ui_component, ID_VIEW_FULLSCREEN,
					  (BonoboUIListenerFn)view_fullscreen_changed_cb,
					  window);
}

static void
favicon_cache_changed_cb (EphyFaviconCache *cache, char *url, EphyWindow *window)
{
	ephy_window_update_control (window, FaviconControl);
}

static void
ephy_window_init (EphyWindow *window)
{
	BonoboUIComponent *ui_component;
	Session *session;
	EphyFaviconCache *cache;

	session = ephy_shell_get_session (ephy_shell);

        window->priv = g_new0 (EphyWindowPrivate, 1);
	window->priv->embed_popup = NULL;
	window->priv->active_tab = NULL;
	window->priv->chrome_mask = 0;
	window->priv->ignore_layout_toggles = FALSE;
	window->priv->closing = FALSE;
	window->priv->has_default_size = FALSE;

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));
	g_signal_connect_object (G_OBJECT (cache),
				 "changed",
				 G_CALLBACK (favicon_cache_changed_cb),
				 window,
				 0);

	/* Setup the window and connect verbs */
	setup_bonobo_window (window, &ui_component);
	window->ui_component = G_OBJECT (ui_component);
	bonobo_ui_component_add_verb_list_with_data (ui_component,
						     ephy_verbs,
						     window);
	setup_layout_menus (window);

	/* Setup the embed popups factory */
	window->priv->embed_popup = setup_popup_factory (window,
							 ui_component);

	bonobo_ui_component_freeze (ui_component, NULL);

	/* Setup menubar */
	ephy_embed_utils_build_charsets_submenu (ui_component,
                                                 CHARSET_MENU_PATH,
                                                 (BonoboUIVerbFn)window_cmd_set_charset,
                                                 window);
	window->priv->fav_menu = ephy_favorites_menu_new (window);
	ephy_favorites_menu_set_path (window->priv->fav_menu, GO_FAVORITES_PATH);

	/* Setup toolbar and statusbar */
	window->priv->toolbar = toolbar_new (window);
	window->priv->statusbar = statusbar_new (window);
	window->priv->ppview_toolbar = ppview_toolbar_new (window);

	/* Setup window contents */
	window->priv->notebook = setup_notebook (window);
	bonobo_window_set_contents (BONOBO_WINDOW (window),
				    GTK_WIDGET (window->priv->notebook));

	bonobo_ui_component_thaw (ui_component, NULL);

	g_object_ref (ephy_shell);

	/*Once window is fully created, add it to the session list*/
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

	if (window->priv->embed_popup)
	{
		g_object_unref (G_OBJECT (window->priv->embed_popup));
	}

	g_object_unref (G_OBJECT (window->priv->toolbar));
	g_object_unref (G_OBJECT (window->priv->statusbar));

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

        g_free (window->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);

#ifdef DEBUG_MARCO
	g_print ("Ephy Window finalized %p\n", window);
#endif

	g_object_unref (ephy_shell);
}

EphyWindow *
ephy_window_new (void)
{
	return EPHY_WINDOW (g_object_new (EPHY_WINDOW_TYPE, NULL));
}

static void
dock_item_set_visibility (EphyWindow *window,
			  const char *path,
			  gboolean visibility)
{
	ephy_bonobo_set_hidden (BONOBO_UI_COMPONENT(window->ui_component),
                               path,
                               !visibility);
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

void
ephy_window_set_chrome (EphyWindow *window,
			EmbedChromeMask flags)
{
	gboolean toolbar, ppvtoolbar, statusbar;

	if (flags & EMBED_CHROME_DEFAULT)
	{
		translate_default_chrome (&flags);
	}

	dock_item_set_visibility (window, "/menu",
				  flags & EMBED_CHROME_MENUBARON);

	toolbar = (flags & EMBED_CHROME_TOOLBARON) != FALSE;
	toolbar_set_visibility (window->priv->toolbar,
				toolbar);

	statusbar = (flags & EMBED_CHROME_STATUSBARON) != FALSE;
	statusbar_set_visibility (window->priv->statusbar,
				  statusbar);

	ppvtoolbar = (flags & EMBED_CHROME_PPVIEWTOOLBARON) != FALSE;
	ppview_toolbar_set_old_chrome (window->priv->ppview_toolbar,
				       window->priv->chrome_mask);
	ppview_toolbar_set_visibility (window->priv->ppview_toolbar,
				       ppvtoolbar);

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

	if (!window->priv->has_default_size && w == -1 && h == -1)
	{
		ephy_state_load_window (GTK_WIDGET(window),
				       "main_window",
			               600, 500, TRUE);
		window->priv->has_default_size = TRUE;
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
	/* the zoom control is updated at the same time than the navigation
	   controls. This keeps it synched most of the time, but not always,
	   because we don't get a notification when zoom changes */

	gresult back, forward, up, stop;
	EphyEmbed *embed;
	EphyTab *tab;
	gint zoom;

	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	back = ephy_embed_can_go_back (embed);
	forward = ephy_embed_can_go_forward (embed);
	up = ephy_embed_can_go_up (embed);
	stop = ephy_tab_get_load_status (tab) & TAB_LOAD_STARTED;

	toolbar_button_set_sensitive (window->priv->toolbar,
				      TOOLBAR_BACK_BUTTON,
				      back == G_OK ? TRUE : FALSE);
	toolbar_button_set_sensitive (window->priv->toolbar,
				      TOOLBAR_FORWARD_BUTTON,
				      forward == G_OK ? TRUE : FALSE);
	toolbar_button_set_sensitive (window->priv->toolbar,
				      TOOLBAR_UP_BUTTON,
				      up == G_OK ? TRUE : FALSE);
	toolbar_button_set_sensitive (window->priv->toolbar,
				      TOOLBAR_STOP_BUTTON,
				      stop);
	if (ephy_embed_zoom_get (embed, &zoom) == G_OK)
	{
		toolbar_set_zoom (window->priv->toolbar, zoom);
	}

	ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
			          GO_BACK_CMD_PATH, !back);
	ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
			          GO_FORWARD_CMD_PATH, !forward);
	ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
			          GO_UP_CMD_PATH, !up);
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

	toolbar_set_location (window->priv->toolbar,
			      location);
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

	cache = ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell));

	location = ephy_tab_get_favicon_url (window->priv->active_tab);
	if (location)
	{
		GdkPixbuf *pixbuf;

		pixbuf = ephy_favicon_cache_get (cache, location);
		gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
	}

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

		ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
					  EDIT_FIND_NEXT_CMD_PATH, can_go_next);
		ephy_bonobo_set_sensitive (BONOBO_UI_COMPONENT(window->ui_component),
					  EDIT_FIND_PREV_CMD_PATH, can_go_prev);
	}
}

static void
update_window_visibility (EphyWindow *window)
{
	GList *l;

	l = ephy_window_get_tabs (window);
	for (; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		g_return_if_fail (IS_EPHY_TAB(tab));

		if (ephy_tab_get_visibility (tab))
		{
			gtk_widget_show (GTK_WIDGET(window));
			return;
		}
	}
	g_list_free (l);

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (window)))
	{
		gtk_widget_hide (GTK_WIDGET (window));
	}
}

static void
update_spinner_control (EphyWindow *window)
{
	GList *l;

	l = ephy_window_get_tabs (window);
	for (; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		g_return_if_fail (IS_EPHY_TAB(tab));

		if (ephy_tab_get_load_status (tab) & TAB_LOAD_STARTED)
		{
			toolbar_spinner_start (window->priv->toolbar);
			return;
		}
	}
	g_list_free (l);

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

EphyEmbedPopupBW *
ephy_window_get_popup_factory (EphyWindow *window)
{
	if (!window->priv->embed_popup)
	{
		window->priv->embed_popup = ephy_embed_popup_bw_new
			(BONOBO_WINDOW(window));
		ephy_embed_popup_connect_verbs
			(EPHY_EMBED_POPUP (window->priv->embed_popup),
			 BONOBO_UI_COMPONENT (window->ui_component));
	}

	return window->priv->embed_popup;
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
	/* save old tab location status */
	ephy_tab_set_location (tab, toolbar_get_location (window->priv->toolbar));
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
