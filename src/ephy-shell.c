/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
#include "config.h"
#endif

#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-embed-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-favicon-cache.h"
#include "ephy-stock-icons.h"
#include "ephy-window.h"
#include "ephy-file-helpers.h"
#include "ephy-thread-helpers.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-history-window.h"
#include "ephy-debug.h"
#include "ephy-extensions-manager.h"
#include "toolbar.h"
#include "ephy-session.h"
#include "downloader-view.h"
#include "ephy-toolbars-model.h"
#include "ephy-automation.h"

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <glib/gi18n.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <dirent.h>
#include <unistd.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <gtk/gtkicontheme.h>
#include <glade/glade-init.h>

#ifdef ENABLE_NAUTILUS_VIEW

#include <bonobo/bonobo-generic-factory.h>
#include "ephy-nautilus-view.h"

#define EPHY_NAUTILUS_VIEW_OAFIID "OAFIID:GNOME_Epiphany_NautilusViewFactory"

#endif

#define EPHY_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SHELL, EphyShellPrivate))

struct EphyShellPrivate
{
	BonoboGenericFactory *automation_factory;
	EphySession *session;
	EphyBookmarks *bookmarks;
	EphyToolbarsModel *toolbars_model;
	EggToolbarsModel *fs_toolbars_model;
	EphyExtensionsManager *extensions_manager;
	GtkWidget *bme;
	GtkWidget *history_window;
	GList *del_on_exit;
};

static void
ephy_shell_class_init (EphyShellClass *klass);
static void
ephy_shell_init (EphyShell *gs);
static void
ephy_shell_finalize (GObject *object);
static void
ephy_init_services (EphyShell *gs);

#ifdef ENABLE_NAUTILUS_VIEW

static void
ephy_nautilus_view_init_factory (EphyShell *gs);
static BonoboObject *
ephy_nautilus_view_new (BonoboGenericFactory *factory,
			  const char *id,
			  EphyShell *gs);

#endif

static GObjectClass *parent_class = NULL;

EphyShell *ephy_shell;

GType
ephy_shell_get_type (void)
{
        static GType ephy_shell_type = 0;

        if (ephy_shell_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyShellClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_shell_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyShell),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_shell_init
                };

		ephy_shell_type = g_type_register_static (EPHY_TYPE_EMBED_SHELL,
							  "EphyShell",
							  &our_info, 0);
        }

        return ephy_shell_type;

}

static void
ephy_shell_class_init (EphyShellClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_shell_finalize;

	g_type_class_add_private (object_class, sizeof(EphyShellPrivate));
}

static void
ephy_shell_init (EphyShell *gs)
{
	EphyEmbedSingle *single;
	EphyShell **ptr = &ephy_shell;
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;
	const char *icon_file;

	gs->priv = EPHY_SHELL_GET_PRIVATE (gs);

	gs->priv->session = NULL;
	gs->priv->bookmarks = NULL;
	gs->priv->bme = NULL;
	gs->priv->history_window = NULL;
	gs->priv->toolbars_model = NULL;
	gs->priv->fs_toolbars_model = NULL;

	ephy_shell = gs;
	g_object_add_weak_pointer (G_OBJECT(ephy_shell),
				   (gpointer *)ptr);

	gnome_vfs_init ();
	glade_gnome_init ();
	ephy_debug_init ();
	ephy_thread_helpers_init ();
	ephy_file_helpers_init ();
	ephy_stock_icons_init ();
	ephy_ensure_dir_exists (ephy_dot_dir ());

	/* This ensures mozilla is fired up */
	single = ephy_embed_shell_get_embed_single (EPHY_EMBED_SHELL (gs));
	if (single != NULL)
	{
		ephy_init_services (gs);
	}
	else
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new
			(NULL,
                         GTK_DIALOG_MODAL,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_CLOSE,
                         _("Epiphany can't be used now. "
                         "Mozilla initialization failed. Check your "
                         "MOZILLA_FIVE_HOME environmental variable."));
		gtk_dialog_run (GTK_DIALOG (dialog));

		exit (0);
	}

	/* FIXME listen on icon changes */
	/* FIXME MultiHead: icon theme is per-display, not global */
	icon_theme = gtk_icon_theme_get_default ();
	icon_info = gtk_icon_theme_lookup_icon (icon_theme, "web-browser", -1, 0);

	if (icon_info)
	{

		icon_file = gtk_icon_info_get_filename (icon_info);
		if (icon_file)
		{
			gtk_window_set_default_icon_from_file (icon_file, NULL);
		}

		gtk_icon_info_free (icon_info);
	}
	else
	{
		g_warning ("Web browser gnome icon not found");
	}

	/* Instantiate extensions manager; this will load the extensions */
	gs->priv->extensions_manager = ephy_extensions_manager_new ();

	/* Instantiate internal extensions */
	gs->priv->session =
		EPHY_SESSION (ephy_extensions_manager_add
			(gs->priv->extensions_manager, EPHY_TYPE_SESSION));

	/* Instantiate the automation factory */
	gs->priv->automation_factory = ephy_automation_factory_new ();
}

static void
delete_files (GList *l)
{
	for (; l != NULL; l = l->next)
	{
		unlink (l->data);
	}
}

static void
ephy_shell_finalize (GObject *object)
{
        EphyShell *gs = EPHY_SHELL (object);

	g_assert (ephy_shell == NULL);

	/* this will unload the extensions */
	LOG ("Unref extension manager")
	g_object_unref (gs->priv->extensions_manager);

	delete_files (gs->priv->del_on_exit);
	g_list_foreach (gs->priv->del_on_exit, (GFunc)g_free, NULL);
	g_list_free (gs->priv->del_on_exit);

	LOG ("Unref toolbars model")
	if (gs->priv->toolbars_model)
	{
		g_object_unref (G_OBJECT (gs->priv->toolbars_model));
	}

	LOG ("Unref fullscreen toolbars model")
	if (gs->priv->fs_toolbars_model)
	{
		g_object_unref (G_OBJECT (gs->priv->fs_toolbars_model));
	}

	LOG ("Unref Bookmarks Editor");
	if (gs->priv->bme)
	{
		gtk_widget_destroy (GTK_WIDGET (gs->priv->bme));
	}

	LOG ("Unref History Window");
	if (gs->priv->history_window)
	{
		gtk_widget_destroy (GTK_WIDGET (gs->priv->history_window));
	}

	LOG ("Unref bookmarks")
	if (gs->priv->bookmarks)
	{
		g_object_unref (gs->priv->bookmarks);
	}

        G_OBJECT_CLASS (parent_class)->finalize (object);

	ephy_state_save ();
	ephy_file_helpers_shutdown ();
	gnome_vfs_shutdown ();

	if (gs->priv->automation_factory)
	{
		bonobo_object_unref (gs->priv->automation_factory);
	}

	LOG ("Ephy shell finalized")

	bonobo_main_quit ();

	LOG ("Bonobo quit done")
}

EphyShell *
ephy_shell_new (void)
{
	return EPHY_SHELL (g_object_new (EPHY_TYPE_SHELL, NULL));
}

static void
ephy_init_services (EphyShell *gs)
{
	/* preload the prefs */
	/* it also enables notifiers support */
	eel_gconf_monitor_add ("/apps/epiphany");
	eel_gconf_monitor_add ("/system/proxy");

#ifdef ENABLE_NAUTILUS_VIEW

	ephy_nautilus_view_init_factory (gs);

#endif

}

static void
load_homepage (EphyEmbed *embed)
{
	char *home;

	home = eel_gconf_get_string(CONF_GENERAL_HOMEPAGE);

	if (home == NULL || home[0] == '\0')
	{
		g_free (home);

		home = g_strdup ("about:blank");
	}

	ephy_embed_load_url (embed, home);

	g_free (home);
}

/**
 * ephy_shell_new_tab:
 * @shell: a #EphyShell
 * @parent_window: the target #EphyWindow or %NULL
 * @previous_tab: the referrer tab or %NULL
 * @url: an url to load or %NULL
 *
 * Create a new tab and the parent window when necessary.
 * Ever use this function to open urls in new window/tabs.
 *
 * ReturnValue: the created #EphyTab
 **/
EphyTab *
ephy_shell_new_tab (EphyShell *shell,
		    EphyWindow *parent_window,
		    EphyTab *previous_tab,
		    const char *url,
		    EphyNewTabFlags flags)
{
	EphyWindow *window;
	EphyTab *tab;
	EphyEmbed *embed;
	gboolean in_new_window = TRUE;
	gboolean grouped;
	gboolean jump_to;
	EphyEmbed *previous_embed = NULL;
	GtkWidget *nb;
	gint position;
	Toolbar *toolbar;

	if (flags & EPHY_NEW_TAB_IN_NEW_WINDOW) in_new_window = TRUE;
	if (flags & EPHY_NEW_TAB_IN_EXISTING_WINDOW) in_new_window = FALSE;

	jump_to = (flags & EPHY_NEW_TAB_JUMP);

	if (!in_new_window && parent_window != NULL)
	{
		window = parent_window;
	}
	else
	{
		window = ephy_window_new ();
	}

	toolbar = ephy_window_get_toolbar (window);

	if (previous_tab)
	{
		previous_embed = ephy_tab_get_embed (previous_tab);
	}

	grouped = ((flags & EPHY_NEW_TAB_OPEN_PAGE ||
	            flags & EPHY_NEW_TAB_APPEND_GROUPED)) &&
		  !(flags & EPHY_NEW_TAB_APPEND_LAST);

	if ((flags & EPHY_NEW_TAB_APPEND_AFTER) && previous_embed != NULL)
	{
		nb = ephy_window_get_notebook (window);
		position = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
						  GTK_WIDGET (previous_embed)) + 1;
	}
	else
	{
		position = grouped ? EPHY_NOTEBOOK_INSERT_GROUPED : EPHY_NOTEBOOK_INSERT_LAST;
	}

	tab = ephy_tab_new ();
	embed = ephy_tab_get_embed (tab);
	gtk_widget_show (GTK_WIDGET(embed));
	ephy_window_add_tab (window, tab,
			     position,
			     jump_to);
	gtk_widget_show (GTK_WIDGET(window));

	if (flags & EPHY_NEW_TAB_HOME_PAGE ||
	    flags & EPHY_NEW_TAB_NEW_PAGE)
	{
		ephy_tab_set_location (tab, "", TAB_ADDRESS_EXPIRE_NEXT);
		toolbar_activate_location (toolbar);
		load_homepage (embed);
	}
	else if (flags & EPHY_NEW_TAB_OPEN_PAGE)
	{
		g_assert (url != NULL);
		ephy_embed_load_url (embed, url);
	}

	if (flags & EPHY_NEW_TAB_FULLSCREEN_MODE)
	{
		gtk_window_fullscreen (GTK_WINDOW (window));
	}

        return tab;
}

#ifdef ENABLE_NAUTILUS_VIEW

static void
ephy_nautilus_view_all_controls_dead (void)
{
        if (!bonobo_control_life_get_count ())
	{
		g_object_unref (G_OBJECT (ephy_shell));
	}
}

static void
ephy_nautilus_view_init_factory (EphyShell *gs)
{
	BonoboGenericFactory *ephy_nautilusview_factory;
	
	ephy_nautilusview_factory = bonobo_generic_factory_new
		(EPHY_NAUTILUS_VIEW_OAFIID,
		 (BonoboFactoryCallback) ephy_nautilus_view_new, gs);
	if (!BONOBO_IS_GENERIC_FACTORY (ephy_nautilusview_factory))
	{
		g_warning ("Couldn't create the factory!");
		return;
	}

	bonobo_control_life_set_callback (ephy_nautilus_view_all_controls_dead);
}

static BonoboObject *
ephy_nautilus_view_new (BonoboGenericFactory *factory, const char *id,
			  EphyShell *gs)
{
	EphyNautilusView *view;

	view = EPHY_NAUTILUS_VIEW (
		ephy_nautilus_view_new_component (gs));

	bonobo_control_life_instrument (
		nautilus_view_get_bonobo_control (NAUTILUS_VIEW (view)));

	return BONOBO_OBJECT (view);
}

#endif

/**
 * ephy_shell_get_session:
 * @gs: a #EphyShell
 *
 * Returns current session.
 *
 * Return value: the current session.
 **/
GObject *
ephy_shell_get_session (EphyShell *gs)
{
	g_return_val_if_fail (EPHY_IS_SHELL (gs), NULL);

	return G_OBJECT (gs->priv->session);
}

EphyBookmarks *
ephy_shell_get_bookmarks (EphyShell *gs)
{
	if (gs->priv->bookmarks == NULL)
	{
		gs->priv->bookmarks = ephy_bookmarks_new ();
	}

	return gs->priv->bookmarks;
}

GObject *
ephy_shell_get_toolbars_model (EphyShell *gs, gboolean fullscreen)
{
	if (fullscreen)
	{
		if (gs->priv->fs_toolbars_model == NULL)
		{
			const char *xml;

			gs->priv->fs_toolbars_model = egg_toolbars_model_new ();
			xml = ephy_file ("epiphany-fs-toolbar.xml");
			egg_toolbars_model_load (gs->priv->fs_toolbars_model, xml);
		}

		return G_OBJECT (gs->priv->fs_toolbars_model);
	}
	else
	{
		if (gs->priv->toolbars_model == NULL)
		{
			EphyBookmarks *bookmarks;

			bookmarks = ephy_shell_get_bookmarks (gs);

			gs->priv->toolbars_model = ephy_toolbars_model_new (bookmarks);

			g_object_set (bookmarks, "toolbars_model",
				      gs->priv->toolbars_model, NULL);
		}


		return G_OBJECT (gs->priv->toolbars_model);
	}
}

GObject *
ephy_shell_get_extensions_manager (EphyShell *es)
{
	g_return_val_if_fail (EPHY_IS_SHELL (es), NULL);

	return G_OBJECT (es->priv->extensions_manager);
}

static void
toolwindow_show_cb (GtkWidget *widget)
{
	LOG ("Ref shell for %s", G_OBJECT_TYPE_NAME (widget))
	ephy_session_add_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_ref (ephy_shell);
}

static void
toolwindow_hide_cb (GtkWidget *widget)
{
	LOG ("Unref shell for %s", G_OBJECT_TYPE_NAME (widget))
	ephy_session_remove_window (ephy_shell->priv->session, GTK_WINDOW (widget));
	g_object_unref (ephy_shell);
}

GtkWidget *
ephy_shell_get_bookmarks_editor (EphyShell *gs)
{
	EphyBookmarks *bookmarks;

	if (gs->priv->bme == NULL)
	{
		bookmarks = ephy_shell_get_bookmarks (ephy_shell);
		g_assert (bookmarks != NULL);
		gs->priv->bme = ephy_bookmarks_editor_new (bookmarks);

		g_signal_connect (gs->priv->bme, "show", 
				  G_CALLBACK (toolwindow_show_cb), NULL);
		g_signal_connect (gs->priv->bme, "hide", 
				  G_CALLBACK (toolwindow_hide_cb), NULL);
	}

	return gs->priv->bme;
}

GtkWidget *
ephy_shell_get_history_window (EphyShell *gs)
{
	EphyHistory *history;

	if (gs->priv->history_window == NULL)
	{
		history = ephy_embed_shell_get_global_history
			(EPHY_EMBED_SHELL (ephy_shell));
		g_assert (history != NULL);
		gs->priv->history_window = ephy_history_window_new (history);

		g_signal_connect (gs->priv->history_window, "show",
				  G_CALLBACK (toolwindow_show_cb), NULL);
		g_signal_connect (gs->priv->history_window, "hide",
				  G_CALLBACK (toolwindow_hide_cb), NULL);
	}

	return gs->priv->history_window;
}

void
ephy_shell_delete_on_exit (EphyShell *gs, const char *path)
{
	gs->priv->del_on_exit = g_list_append (gs->priv->del_on_exit,
					       g_strdup (path));
}

