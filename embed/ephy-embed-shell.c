/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include <config.h>

#include <glib.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "downloader-view.h"
#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-encodings.h"
#include "ephy-favicon-cache.h"
#include "ephy-file-helpers.h"
#include "ephy-history.h"
#include "ephy-marshal.h"

#include "ephy-print-utils.h"

#define PAGE_SETUP_FILENAME	"page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME	"print-settings.ini"

#define LEGACY_PAGE_SETUP_FILENAME	"page-setup.ini"

#define EPHY_EMBED_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellPrivate))

#define ENABLE_MIGRATION

struct _EphyEmbedShellPrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	EphyFaviconCache *favicon_cache;
	EphyEmbedSingle *embed_single;
	EphyEncodings *encodings;
	EphyAdBlockManager *adblock_manager;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;
	guint object_count;
	guint single_initialised : 1;
};

enum
{
	PREPARE_CLOSE,
	QUIT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void ephy_embed_shell_class_init	(EphyEmbedShellClass *klass);
static void ephy_embed_shell_init	(EphyEmbedShell *shell);

EphyEmbedShell *embed_shell = NULL;

G_DEFINE_TYPE (EphyEmbedShell, ephy_embed_shell, G_TYPE_OBJECT)

static void
ephy_embed_shell_dispose (GObject *object)
{
	EphyEmbedShell *shell = EPHY_EMBED_SHELL (object);
	EphyEmbedShellPrivate *priv = shell->priv;

	if (priv->downloader_view != NULL)
	{
		DownloaderView **downloader_view = &priv->downloader_view;
		LOG ("Unref downloader");
		g_object_remove_weak_pointer
			(G_OBJECT (priv->downloader_view),
			 (gpointer *) downloader_view);
		g_object_unref (priv->downloader_view);
		priv->downloader_view = NULL;
	}

	if (priv->favicon_cache != NULL)
	{
		LOG ("Unref favicon cache");
		g_object_unref (priv->favicon_cache);
		priv->favicon_cache = NULL;
	}

	if (priv->encodings != NULL)
	{
		LOG ("Unref encodings");
		g_object_unref (priv->encodings);
		priv->encodings = NULL;
	}

	if (priv->page_setup != NULL)
	{
		g_object_unref (priv->page_setup);
		priv->page_setup = NULL;
	}

	if (priv->print_settings != NULL)
	{
		g_object_unref (priv->print_settings);
		priv->print_settings = NULL;
	}

	G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static void
ephy_embed_shell_finalize (GObject *object)
{
	EphyEmbedShell *shell = EPHY_EMBED_SHELL (object);

	if (shell->priv->global_history)
	{
		LOG ("Unref history");
		g_object_unref (shell->priv->global_history);
	}

	if (shell->priv->embed_single)
	{
		LOG ("Unref embed single");
		g_object_unref (G_OBJECT (shell->priv->embed_single));
	}

	if (shell->priv->adblock_manager != NULL)
	{
		LOG ("Unref adblock manager");
		g_object_unref (shell->priv->adblock_manager);
		shell->priv->adblock_manager = NULL;
	}

	G_OBJECT_CLASS (ephy_embed_shell_parent_class)->finalize (object);
}

/**
 * ephy_embed_shell_get_favicon_cache:
 * @shell: the #EphyEmbedShell
 *
 * Returns the favicons cache.
 *
 * Return value: the favicons cache
 **/
GObject *
ephy_embed_shell_get_favicon_cache (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->favicon_cache == NULL)
	{
		shell->priv->favicon_cache = ephy_favicon_cache_new ();
	}

	return G_OBJECT (shell->priv->favicon_cache);
}

GObject *
ephy_embed_shell_get_global_history (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->global_history == NULL)
	{
		shell->priv->global_history = ephy_history_new ();
	}

	return G_OBJECT (shell->priv->global_history);
}

GObject *
ephy_embed_shell_get_downloader_view (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->downloader_view == NULL)
	{
		DownloaderView **downloader_view;
		shell->priv->downloader_view = downloader_view_new ();
		downloader_view = &shell->priv->downloader_view;
		g_object_add_weak_pointer
			(G_OBJECT(shell->priv->downloader_view),
			 (gpointer *) downloader_view);
	}

	return G_OBJECT (shell->priv->downloader_view);
}

GObject *
ephy_embed_shell_get_downloader_view_nocreate (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	return (GObject *) shell->priv->downloader_view;
}

static GObject *
impl_get_embed_single (EphyEmbedShell *shell)
{
	EphyEmbedShellPrivate *priv;

	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	priv = shell->priv;

	if (priv->embed_single != NULL &&
	    !priv->single_initialised)
	{
		g_warning ("ephy_embed_shell_get_embed_single called while the single is being initialised!\n");
		return G_OBJECT (priv->embed_single);
	}

	if (priv->embed_single == NULL)
	{
		priv->embed_single = EPHY_EMBED_SINGLE
                  (g_object_new (EPHY_TYPE_EMBED_SINGLE, NULL));
		g_assert (priv->embed_single != NULL);

		if (!ephy_embed_single_initialize (priv->embed_single))
		{
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new
					(NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("Epiphany can't be used now. "
					   "Initialization failed."));
			gtk_dialog_run (GTK_DIALOG (dialog));

			exit (0);
		}

		priv->single_initialised = TRUE;
	}

	return G_OBJECT (shell->priv->embed_single);
}

GObject *
ephy_embed_shell_get_embed_single (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);

	return klass->get_embed_single (shell);
}

GObject *
ephy_embed_shell_get_encodings (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->encodings == NULL)
	{
		shell->priv->encodings = ephy_encodings_new ();
	}

	return G_OBJECT (shell->priv->encodings);
}

void
ephy_embed_shell_prepare_close (EphyEmbedShell *shell)
{
	g_signal_emit (shell, signals[PREPARE_CLOSE], 0);
}

static void
ephy_embed_shell_init (EphyEmbedShell *shell)
{
	shell->priv = EPHY_EMBED_SHELL_GET_PRIVATE (shell);

	/* globally accessible singleton */
	g_assert (embed_shell == NULL);
	embed_shell = shell;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ephy_embed_shell_dispose;
	object_class->finalize = ephy_embed_shell_finalize;

	klass->get_embed_single = impl_get_embed_single;

/**
 * EphyEmbed::prepare-close:
 * @shell:
 * 
 * The ::prepare-close signal is emitted when epiphany is preparing to
 * quit on command from the session manager. You can use it when you need
 * to do something special (shut down a service, for example).
 **/
	signals[PREPARE_CLOSE] =
		g_signal_new ("prepare-close",
			      EPHY_TYPE_EMBED_SHELL,
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedShellClass, prepare_close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

/**
 * EphyEmbedShell::quit:
 * @shell: an #EphyEmbedShell
 * 
 * The ::quit is emitted when all windows (browser windows, popups,
 * download windows, etc) are closed and the @shell is ready to be
 * closed.
 *
 * Since: 2.30
 **/
	signals[QUIT] =
		g_signal_new ("quit",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	g_type_class_add_private (object_class, sizeof (EphyEmbedShellPrivate));
}

/**
 * ephy_embed_shell_get_default:
 *
 * Retrieves the default #EphyEmbedShell object
 *
 * ReturnValue: the default #EphyEmbedShell
 **/
EphyEmbedShell *
ephy_embed_shell_get_default (void)
{
	return embed_shell;
}

/**
 * ephy_embed_shell_get_adblock_manager:
 * @shell: the #EphyEmbedShell
 *
 * Returns the adblock manager.
 *
 * Return value: the adblock manager
 **/
GObject *
ephy_embed_shell_get_adblock_manager (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->adblock_manager == NULL)
	{
		shell->priv->adblock_manager = g_object_new (EPHY_TYPE_ADBLOCK_MANAGER, NULL);
	}

	return G_OBJECT (shell->priv->adblock_manager);
}

void
ephy_embed_shell_set_page_setup	(EphyEmbedShell *shell,
				 GtkPageSetup *page_setup)
{
	EphyEmbedShellPrivate *priv;
	char *path;

	g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));
	priv = shell->priv;

	if (page_setup != NULL)
	{
		g_object_ref (page_setup);
	}
	else
	{
		page_setup = gtk_page_setup_new ();
	}

	if (priv->page_setup != NULL)
	{
		g_object_unref (priv->page_setup);
	}

	priv->page_setup = page_setup;

	path = g_build_filename (ephy_dot_dir (), PAGE_SETUP_FILENAME, NULL);
	gtk_page_setup_to_file (page_setup, path, NULL);
	g_free (path);
}
		
GtkPageSetup *
ephy_embed_shell_get_page_setup	(EphyEmbedShell *shell)
{
	EphyEmbedShellPrivate *priv;

	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);
	priv = shell->priv;

	if (priv->page_setup == NULL)
	{
		GError *error = NULL;
		char *path;

		path = g_build_filename (ephy_dot_dir (), PAGE_SETUP_FILENAME, NULL);
		priv->page_setup = gtk_page_setup_new_from_file (path, &error);
		g_free (path);

#ifdef ENABLE_MIGRATION
		/* If the file doesn't exist, try to fall back to the old format */
		if (error != NULL &&
		    error->domain == G_FILE_ERROR &&
		    error->code == G_FILE_ERROR_NOENT)
		{
			path = g_build_filename (ephy_dot_dir (), LEGACY_PAGE_SETUP_FILENAME, NULL);
			priv->page_setup = ephy_print_utils_page_setup_new_from_file (path, NULL);
			if (priv->page_setup != NULL)
			{
				/* Delete the old file, so we don't migrate again */
				g_unlink (path);
			}
			g_free (path);
		} else if (error != NULL)
			g_warning ("error: %s\n", error->message);
#endif /* ENABLE_MIGRATION */

		if (error)
		{
			g_error_free (error);
		}

		/* If that still didn't work, create a new, empty one */
		if (priv->page_setup == NULL)
		{
			priv->page_setup = gtk_page_setup_new ();
		}
	}

	return priv->page_setup;
}

void
ephy_embed_shell_set_print_settings (EphyEmbedShell *shell,
				     GtkPrintSettings *settings)
{
	EphyEmbedShellPrivate *priv;
	char *path;

	g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));
	priv = shell->priv;

	if (settings != NULL)
	{
		g_object_ref (settings);
	}

	if (priv->print_settings != NULL)
	{
		g_object_unref (priv->print_settings);
	}

	priv->print_settings = settings ? settings : gtk_print_settings_new ();

	path = g_build_filename (ephy_dot_dir (), PRINT_SETTINGS_FILENAME, NULL);
	gtk_print_settings_to_file (settings, path, NULL);
	g_free (path);
}
		
GtkPrintSettings *
ephy_embed_shell_get_print_settings (EphyEmbedShell *shell)
{
	EphyEmbedShellPrivate *priv;

	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);
	priv = shell->priv;

	if (priv->print_settings == NULL)
	{
		GError *error = NULL;
		char *path;

		path = g_build_filename (ephy_dot_dir (), PRINT_SETTINGS_FILENAME, NULL);
		priv->print_settings = gtk_print_settings_new_from_file (path, &error);
		g_free (path);

		/* Note: the gtk print settings file format is the same as our legacy one,
		 * so no need to migrate here.
		 */

		if (priv->print_settings == NULL)
		{
			priv->print_settings = gtk_print_settings_new ();
		}
	}

	return priv->print_settings;
}


static void
object_notify_cb (EphyEmbedShell *shell, GObject *object)
{
	shell->priv->object_count--;
	if (shell->priv->object_count == 0)
		g_signal_emit (shell, signals[QUIT], 0);
}

void
_ephy_embed_shell_track_object (EphyEmbedShell *shell, GObject *object)
{
	g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));
	g_return_if_fail (G_IS_OBJECT (object));

	g_object_weak_ref (object, (GWeakNotify)object_notify_cb, shell);
	shell->priv->object_count++;
}
