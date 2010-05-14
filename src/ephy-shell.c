/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2006 Christian Persch
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

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <dirent.h>
#include <unistd.h>

#include "ephy-shell.h"
#include "ephy-type-builtins.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-utils.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-favicon-cache.h"
#include "ephy-web-view.h"
#include "ephy-embed-utils.h"
#include "ephy-window.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-history-window.h"
#include "pdm-dialog.h"
#include "prefs-dialog.h"
#include "ephy-debug.h"
#include "ephy-extensions-manager.h"
#include "ephy-session.h"
#include "ephy-lockdown.h"
#include "downloader-view.h"
#include "egg-toolbars-model.h"
#include "ephy-toolbars-model.h"
#include "ephy-toolbar.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "ephy-stock-icons.h"
#include "ephy-web-view.h"

#ifdef ENABLE_NETWORK_MANAGER
#include "ephy-net-monitor.h"
#endif

#define EPHY_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SHELL, EphyShellPrivate))

struct _EphyShellPrivate
{
	EphySession *session;
	GObject *lockdown;
	EphyBookmarks *bookmarks;
	EggToolbarsModel *toolbars_model;
	EggToolbarsModel *fs_toolbars_model;
	EphyExtensionsManager *extensions_manager;
#ifdef ENABLE_NETWORK_MANAGER
	EphyNetMonitor *net_monitor;
#endif
	GtkWidget *bme;
	GtkWidget *history_window;
	GObject *pdm_dialog;
	GObject *prefs_dialog;
	GList *del_on_exit;

	guint embed_single_connected : 1;
};

EphyShell *ephy_shell = NULL;

static void ephy_shell_class_init	(EphyShellClass *klass);
static void ephy_shell_init		(EphyShell *shell);
static void ephy_shell_dispose		(GObject *object);
static void ephy_shell_finalize		(GObject *object);
static GObject *impl_get_embed_single   (EphyEmbedShell *embed_shell);

G_DEFINE_TYPE (EphyShell, ephy_shell, EPHY_TYPE_EMBED_SHELL)

static void
ephy_shell_class_init (EphyShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyEmbedShellClass *embed_shell_class = EPHY_EMBED_SHELL_CLASS (klass);

	object_class->dispose = ephy_shell_dispose;
	object_class->finalize = ephy_shell_finalize;

	embed_shell_class->get_embed_single = impl_get_embed_single;

	g_type_class_add_private (object_class, sizeof(EphyShellPrivate));
}

static EphyEmbed *
ephy_shell_new_window_cb (EphyEmbedSingle *single,
			  EphyEmbed *parent_embed,
			  EphyWebViewChrome chromemask,
			  EphyShell *shell)
{
	GtkWidget *parent = NULL;
	gboolean is_popup;
	EphyNewTabFlags flags = EPHY_NEW_TAB_DONT_SHOW_WINDOW |
				EPHY_NEW_TAB_APPEND_LAST |
				EPHY_NEW_TAB_IN_NEW_WINDOW |
				EPHY_NEW_TAB_JUMP;

	LOG ("ephy_shell_new_window_cb tab chrome %d", chromemask);

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_JAVASCRIPT_CHROME))
	{
		chromemask = EPHY_WEB_VIEW_CHROME_ALL;
	}

	if (parent_embed != NULL)
	{
		/* this will either be a EphyWindow, or the embed itself
		 * (in case it's about to be destroyed, which means it's already
		 * removed from its tab)
		 */
		parent = gtk_widget_get_toplevel (GTK_WIDGET (parent_embed));
	}

	/* what's a popup ? ATM, any window opened with menubar toggled on 
   	 * is *not* a popup 
	 */
	is_popup = (chromemask & EPHY_WEB_VIEW_CHROME_MENUBAR) == 0;

	return ephy_shell_new_tab_full
		(shell,
		 EPHY_IS_WINDOW (parent) ? EPHY_WINDOW (parent) : NULL,
		 NULL, NULL, flags, chromemask, is_popup, 0);
}

#ifdef ENABLE_NETWORK_MANAGER

static void
ephy_shell_sync_network_status (EphyNetMonitor *net_monitor,
				GParamSpec *pspec,
				EphyShell *shell)
{
	EphyShellPrivate *priv = shell->priv;
	EphyEmbedSingle *single;
	gboolean net_status;

	if (!priv->embed_single_connected) return;

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (EPHY_EMBED_SHELL (shell)));

	net_status = ephy_net_monitor_get_net_status (net_monitor);
	ephy_embed_single_set_network_status (single, net_status);
}

#endif /* ENABLE_NETWORK_MANAGER */

static GObject*
impl_get_embed_single (EphyEmbedShell *embed_shell)
{
	EphyShell *shell = EPHY_SHELL (embed_shell);
	EphyShellPrivate *priv = shell->priv;
	GObject *embed_single;

	embed_single = EPHY_EMBED_SHELL_CLASS (ephy_shell_parent_class)->get_embed_single (embed_shell);

	if (embed_single != NULL &&
	    priv->embed_single_connected == FALSE)
	{
		g_signal_connect_object (embed_single, "new-window",
					 G_CALLBACK (ephy_shell_new_window_cb),
					 shell, G_CONNECT_AFTER);

		priv->embed_single_connected = TRUE;

#ifdef ENABLE_NETWORK_MANAGER
		/* Now we need the net monitor */
		ephy_shell_get_net_monitor (shell);
		ephy_shell_sync_network_status (priv->net_monitor, NULL, shell);
#endif
	}
	
	return embed_single;
}

static void
ephy_shell_init (EphyShell *shell)
{
	EphyShell **ptr = &ephy_shell;

	shell->priv = EPHY_SHELL_GET_PRIVATE (shell);

	/* globally accessible singleton */
	g_assert (ephy_shell == NULL);
	ephy_shell = shell;
	g_object_add_weak_pointer (G_OBJECT(ephy_shell),
				   (gpointer *)ptr);
}

static void
ephy_shell_dispose (GObject *object)
{
	EphyShell *shell = EPHY_SHELL (object);
	EphyShellPrivate *priv = shell->priv;

	LOG ("EphyShell disposing");

	if (shell->priv->extensions_manager != NULL)
	{
		LOG ("Unref extension manager");
		/* this will unload the extensions */
		g_object_unref (priv->extensions_manager);
		priv->extensions_manager = NULL;
	}

	if (priv->session != NULL)
	{
		LOG ("Unref session manager");
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->lockdown != NULL)
	{
		LOG ("Unref lockdown controller");
		g_object_unref (priv->lockdown);
		priv->lockdown = NULL;
	}

	if (priv->toolbars_model != NULL)
	{
		LOG ("Unref toolbars model");
		g_object_unref (priv->toolbars_model);
		priv->toolbars_model = NULL;
	}

	if (priv->fs_toolbars_model != NULL)
	{
		LOG ("Unref fullscreen toolbars model");
		g_object_unref (priv->fs_toolbars_model);
		priv->fs_toolbars_model = NULL;
	}

	if (priv->bme != NULL)
	{
		LOG ("Unref Bookmarks Editor");
		gtk_widget_destroy (GTK_WIDGET (priv->bme));
		priv->bme = NULL;
	}

	if (priv->history_window != NULL)
	{
		LOG ("Unref History Window");
		gtk_widget_destroy (GTK_WIDGET (priv->history_window));
		priv->history_window = NULL;
	}

	if (priv->pdm_dialog != NULL)
	{
		LOG ("Unref PDM Dialog");
		g_object_unref (priv->pdm_dialog);
		priv->pdm_dialog = NULL;
	}

	if (priv->prefs_dialog != NULL)
	{
		LOG ("Unref prefs dialog");
		g_object_unref (priv->prefs_dialog);
		priv->prefs_dialog = NULL;
	}

	if (priv->bookmarks != NULL)
	{
		LOG ("Unref bookmarks");
		g_object_unref (priv->bookmarks);
		priv->bookmarks = NULL;
	}

#ifdef ENABLE_NETWORK_MANAGER
	if (priv->net_monitor != NULL)
	{
		LOG ("Unref net monitor");
		g_signal_handlers_disconnect_by_func
			(priv->net_monitor, G_CALLBACK (ephy_shell_sync_network_status), shell);
		g_object_unref (priv->net_monitor);
		priv->net_monitor = NULL;
	}
#endif /* ENABLE_NETWORK_MANAGER */

	G_OBJECT_CLASS (ephy_shell_parent_class)->dispose (object);
}

static void
ephy_shell_finalize (GObject *object)
{
	G_OBJECT_CLASS (ephy_shell_parent_class)->finalize (object);

	LOG ("Ephy shell finalised");
}

/**
 * ephy_shell_get_default:
 *
 * Retrieve the default #EphyShell object
 *
 * Return value: (transfer none): the default #EphyShell
 **/
EphyShell *
ephy_shell_get_default (void)
{
	return ephy_shell;
}

static gboolean
url_is_empty (const char *location)
{
	gboolean is_empty = FALSE;

        if (location == NULL || location[0] == '\0' ||
            strcmp (location, "about:blank") == 0)
        {
                is_empty = TRUE;
        }

        return is_empty;
}

static gboolean
load_homepage (EphyEmbed *embed)
{
	char *home;
	gboolean is_empty;

	home = eel_gconf_get_string(CONF_GENERAL_HOMEPAGE);

	if (home == NULL || home[0] == '\0')
	{
		g_free (home);

		home = g_strdup ("about:blank");
	}

	is_empty = url_is_empty (home);
	ephy_web_view_load_url (ephy_embed_get_web_view (embed), home);

	g_free (home);

	return is_empty;
}

/**
 * ephy_shell_new_tab_full:
 * @shell: a #EphyShell
 * @parent_window: the target #EphyWindow or %NULL
 * @previous_embed: the referrer embed, or %NULL
 * @request: a #WebKitNetworkRequest to load or %NULL
 * @chrome: a #EphyEmbedChrome mask to use if creating a new window
 * @is_popup: whether the new window is a popup
 * @user_time: a timestamp, or 0
 *
 * Create a new tab and the parent window when necessary.
 * Use this function to open urls in new window/tabs.
 *
 * Return value: (transfer none): the created #EphyEmbed
 **/
EphyEmbed *
ephy_shell_new_tab_full (EphyShell *shell,
			 EphyWindow *parent_window,
			 EphyEmbed *previous_embed,
			 WebKitNetworkRequest *request,
			 EphyNewTabFlags flags,
			 EphyWebViewChrome chrome,
			 gboolean is_popup,
			 guint32 user_time)
{
	EphyWindow *window;
	EphyEmbed *embed = NULL;
	gboolean in_new_window = TRUE;
	gboolean open_page = FALSE;
	gboolean jump_to;
	gboolean active_is_blank = FALSE;
	gboolean copy_history = TRUE;
	GtkWidget *nb;
	int position = -1;
	gboolean is_empty = FALSE;
	EphyToolbar *toolbar;

	if (flags & EPHY_NEW_TAB_OPEN_PAGE) open_page = TRUE;
	if (flags & EPHY_NEW_TAB_IN_NEW_WINDOW) in_new_window = TRUE;
	if (flags & EPHY_NEW_TAB_IN_EXISTING_WINDOW) in_new_window = FALSE;
	if (flags & EPHY_NEW_TAB_DONT_COPY_HISTORY) copy_history = FALSE;

	in_new_window = in_new_window && !eel_gconf_get_boolean (CONF_LOCKDOWN_FULLSCREEN);
	g_return_val_if_fail (open_page == (gboolean)(request != NULL), NULL);

	jump_to = (flags & EPHY_NEW_TAB_JUMP) != 0;

	LOG ("Opening new tab parent-window %p parent-embed %p in-new-window:%s jump-to:%s",
	     parent_window, previous_embed, in_new_window ? "t" : "f", jump_to ? "t" : "f");

	if (!in_new_window && parent_window != NULL)
	{
		window = parent_window;
	}
	else
	{
		window = ephy_window_new_with_chrome (chrome, is_popup);
	}

	toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));

	if ((flags & EPHY_NEW_TAB_APPEND_AFTER) && previous_embed != NULL)
	{
		nb = ephy_window_get_notebook (window);
		/* FIXME this assumes the tab is the direct notebook child */
		position = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
						  GTK_WIDGET (previous_embed)) + 1;
	}
	
	if (flags & EPHY_NEW_TAB_FROM_EXTERNAL)
	{
		/* If the active embed is blank, use that to open the url and jump to it */
		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		if (embed != NULL)
		{
			if (ephy_web_view_get_is_blank (ephy_embed_get_web_view (embed)) &&
			    ephy_web_view_is_loading (ephy_embed_get_web_view (embed)) == FALSE)
			{
				active_is_blank = TRUE;
			}
		}
	}
	if (active_is_blank == FALSE)
	{
		embed = EPHY_EMBED (g_object_new (EPHY_TYPE_EMBED, NULL));
		g_assert (embed != NULL);
		gtk_widget_show (GTK_WIDGET (embed));
		
		ephy_embed_container_add_child (EPHY_EMBED_CONTAINER (window), embed, position, jump_to);
	}

	if (copy_history && previous_embed != NULL)
	{
		ephy_web_view_copy_back_history (ephy_embed_get_web_view (previous_embed),
						 ephy_embed_get_web_view (embed));
	}

	ephy_gui_window_update_user_time (GTK_WIDGET (window), user_time);

	if ((flags & EPHY_NEW_TAB_DONT_SHOW_WINDOW) == 0)
	{
		gtk_widget_show (GTK_WIDGET (window));
	}

	if (flags & EPHY_NEW_TAB_FULLSCREEN_MODE)
	{
		gtk_window_fullscreen (GTK_WINDOW (window));
	}

	if (flags & EPHY_NEW_TAB_HOME_PAGE ||
	    flags & EPHY_NEW_TAB_NEW_PAGE)
	{
		ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), "");
		ephy_toolbar_activate_location (toolbar);
		is_empty = load_homepage (embed);
	}
	else if (flags & EPHY_NEW_TAB_OPEN_PAGE)
	{
		ephy_web_view_load_request (ephy_embed_get_web_view (embed),
					    request);

		is_empty = url_is_empty (webkit_network_request_get_uri (request));
	}

	/* Make sure the initial focus is somewhere sensible and not, for
	 * example, on the reload button.
	 */
	if (in_new_window || jump_to)
	{
		/* If the location entry is blank, focus that, except if the
		 * page was a copy */
		if (is_empty)
		{
			/* empty page, focus location entry */
			toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));
			ephy_toolbar_activate_location (toolbar);
		}
		else if (embed != NULL)
		{
			/* non-empty page, focus the page. but make sure the widget is realised first! */
			gtk_widget_realize (GTK_WIDGET (embed));
			gtk_widget_grab_focus (GTK_WIDGET (embed));
		}
	}

	return embed;
}

/**
 * ephy_shell_new_tab:
 * @shell: a #EphyShell
 * @parent_window: the target #EphyWindow or %NULL
 * @previous_embed: the referrer embed, or %NULL
 * @url: an url to load or %NULL
 *
 * Create a new tab and the parent window when necessary.
 * Use this function to open urls in new window/tabs.
 *
 * Return value: (transfer none): the created #EphyEmbed
 **/
EphyEmbed *
ephy_shell_new_tab (EphyShell *shell,
		    EphyWindow *parent_window,
		    EphyEmbed *previous_embed,
		    const char *url,
		    EphyNewTabFlags flags)
{
	EphyEmbed *embed;
	WebKitNetworkRequest *request = url ? webkit_network_request_new (url) : NULL;

	embed = ephy_shell_new_tab_full (shell, parent_window,
					 previous_embed, request, flags,
					 EPHY_WEB_VIEW_CHROME_ALL, FALSE, 0);

	if (request)
		g_object_unref (request);

	return embed;
}

/**
 * ephy_shell_get_session:
 * @shell: the #EphyShell
 *
 * Returns current session.
 *
 * Return value: (transfer none): the current session.
 **/
GObject *
ephy_shell_get_session (EphyShell *shell)
{
	g_return_val_if_fail (EPHY_IS_SHELL (shell), NULL);

	if (shell->priv->session == NULL)
	{
		EphyExtensionsManager *manager;

		shell->priv->session = g_object_new (EPHY_TYPE_SESSION, NULL);

		manager = EPHY_EXTENSIONS_MANAGER
			(ephy_shell_get_extensions_manager (shell));
		ephy_extensions_manager_register (manager,
						  G_OBJECT (shell->priv->session));
	}

	return G_OBJECT (shell->priv->session);
}

/**
 * ephy_shell_get_lockdown:
 * @shell: the #EphyShell
 *
 * Returns the lockdown controller.
 *
 * Return value: the lockdown controller
 **/
static GObject *
ephy_shell_get_lockdown (EphyShell *shell)
{
	g_return_val_if_fail (EPHY_IS_SHELL (shell), NULL);

	if (shell->priv->lockdown == NULL)
	{
		EphyExtensionsManager *manager;

		shell->priv->lockdown = g_object_new (EPHY_TYPE_LOCKDOWN, NULL);

		manager = EPHY_EXTENSIONS_MANAGER
			(ephy_shell_get_extensions_manager (shell));
		ephy_extensions_manager_register (manager,
						  G_OBJECT (shell->priv->lockdown));
	}

	return G_OBJECT (shell->priv->session);
}

/**
 * ephy_shell_get_bookmarks:
 *
 * Return value: (transfer none):
 **/
EphyBookmarks *
ephy_shell_get_bookmarks (EphyShell *shell)
{
	if (shell->priv->bookmarks == NULL)
	{
		shell->priv->bookmarks = ephy_bookmarks_new ();
	}

	return shell->priv->bookmarks;
}

/**
 * ephy_shell_get_toolbars_model:
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_shell_get_toolbars_model (EphyShell *shell, gboolean fullscreen)
{
	LOG ("ephy_shell_get_toolbars_model fs=%d", fullscreen);

	if (fullscreen)
	{
		if (shell->priv->fs_toolbars_model == NULL)
		{
			EggTbModelFlags flags;
			gboolean success;
			const char *xml;

			shell->priv->fs_toolbars_model = egg_toolbars_model_new ();
			xml = ephy_file ("epiphany-fs-toolbar.xml");
			g_return_val_if_fail (xml != NULL, NULL);

			success = egg_toolbars_model_load_toolbars
				(shell->priv->fs_toolbars_model, xml);
			g_return_val_if_fail (success, NULL);

			flags = egg_toolbars_model_get_flags 
			  (shell->priv->fs_toolbars_model, 0);
			egg_toolbars_model_set_flags
			  (shell->priv->fs_toolbars_model, 0, 
			   flags | EGG_TB_MODEL_NOT_REMOVABLE);
		}

		return G_OBJECT (shell->priv->fs_toolbars_model);
	}
	else
	{
		if (shell->priv->toolbars_model == NULL)
		{
			shell->priv->toolbars_model = ephy_toolbars_model_new ();
			
			ephy_bookmarks_ui_attach_toolbar_model (shell->priv->toolbars_model);
			
			ephy_toolbars_model_load
				(EPHY_TOOLBARS_MODEL (shell->priv->toolbars_model));
		}

		return G_OBJECT (shell->priv->toolbars_model);
	}
}

/**
 * ephy_shell_get_extensions_manager:
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_shell_get_extensions_manager (EphyShell *es)
{
	g_return_val_if_fail (EPHY_IS_SHELL (es), NULL);

	if (es->priv->extensions_manager == NULL)
	{
		/* Instantiate extensions manager */
		es->priv->extensions_manager =
			g_object_new (EPHY_TYPE_EXTENSIONS_MANAGER, NULL);

		ephy_extensions_manager_startup (es->priv->extensions_manager);

		/* FIXME */
		ephy_shell_get_lockdown (es);
		ephy_embed_shell_get_adblock_manager (embed_shell);
	}

	return G_OBJECT (es->priv->extensions_manager);
}

/**
 * ephy_shell_get_net_monitor:
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_shell_get_net_monitor (EphyShell *shell)
{
#ifdef ENABLE_NETWORK_MANAGER
	EphyShellPrivate *priv = shell->priv;

	if (priv->net_monitor == NULL)
	{
		priv->net_monitor = ephy_net_monitor_new ();
		g_signal_connect (priv->net_monitor, "notify::network-status",
				  G_CALLBACK (ephy_shell_sync_network_status), shell);
	}

	return G_OBJECT (priv->net_monitor);
#else
	return NULL;
#endif /* ENABLE_NETWORK_MANAGER */
}

static void
toolwindow_show_cb (GtkWidget *widget, EphyShell *es)
{
	EphySession *session;

	LOG ("Ref shell for %s", G_OBJECT_TYPE_NAME (widget));

	session = EPHY_SESSION (ephy_shell_get_session (es));
	ephy_session_add_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_ref (ephy_shell);
}

static void
toolwindow_hide_cb (GtkWidget *widget, EphyShell *es)
{
	EphySession *session;

	LOG ("Unref shell for %s", G_OBJECT_TYPE_NAME (widget));

	session = EPHY_SESSION (ephy_shell_get_session (es));
	ephy_session_remove_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_unref (ephy_shell);
}

/**
 * ephy_shell_get_bookmarks_editor:
 *
 * Return value: (transfer none):
 **/
GtkWidget *
ephy_shell_get_bookmarks_editor (EphyShell *shell)
{
	EphyBookmarks *bookmarks;

	if (shell->priv->bme == NULL)
	{
		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		g_assert (bookmarks != NULL);
		shell->priv->bme = ephy_bookmarks_editor_new (bookmarks);

		g_signal_connect (shell->priv->bme, "show", 
				  G_CALLBACK (toolwindow_show_cb), shell);
		g_signal_connect (shell->priv->bme, "hide", 
				  G_CALLBACK (toolwindow_hide_cb), shell);
	}

	return shell->priv->bme;
}

/**
 * ephy_shell_get_history_window:
 *
 * Return value: (transfer none):
 **/
GtkWidget *
ephy_shell_get_history_window (EphyShell *shell)
{
	EphyHistory *history;

	if (shell->priv->history_window == NULL)
	{
		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		g_assert (history != NULL);
		shell->priv->history_window = ephy_history_window_new (history);

		g_signal_connect (shell->priv->history_window, "show",
				  G_CALLBACK (toolwindow_show_cb), shell);
		g_signal_connect (shell->priv->history_window, "hide",
				  G_CALLBACK (toolwindow_hide_cb), shell);
	}

	return shell->priv->history_window;
}

/**
 * ephy_shell_get_pdm_dialog:
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_shell_get_pdm_dialog (EphyShell *shell)
{
	if (shell->priv->pdm_dialog == NULL)
	{
		GObject **dialog;

		shell->priv->pdm_dialog = g_object_new (EPHY_TYPE_PDM_DIALOG, NULL);

		dialog = &shell->priv->pdm_dialog;

		g_object_add_weak_pointer (shell->priv->pdm_dialog,
					   (gpointer *) dialog);
	}

	return shell->priv->pdm_dialog;
}

/**
 * ephy_shell_get_prefs_dialog:
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_shell_get_prefs_dialog (EphyShell *shell)
{
	if (shell->priv->prefs_dialog == NULL)
	{
		GObject **dialog;

		shell->priv->prefs_dialog = g_object_new (EPHY_TYPE_PREFS_DIALOG, NULL);

		dialog  = &shell->priv->prefs_dialog;

		g_object_add_weak_pointer (shell->priv->prefs_dialog,
					   (gpointer *) dialog);
	}

	return shell->priv->prefs_dialog;
}

void
_ephy_shell_create_instance (void)
{
	g_assert (ephy_shell == NULL);

	ephy_shell = EPHY_SHELL (g_object_new (EPHY_TYPE_SHELL, NULL));
	/* FIXME weak ref */

	g_assert (ephy_shell != NULL);
}
