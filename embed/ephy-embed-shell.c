/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include <config.h>
#include "ephy-embed-shell.h"

#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-download.h"
#include "ephy-embed-single.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-print-utils.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define PAGE_SETUP_FILENAME	"page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME	"print-settings.ini"

#define LEGACY_PAGE_SETUP_FILENAME	"page-setup.ini"

#define EPHY_EMBED_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellPrivate))

#define ENABLE_MIGRATION

struct _EphyEmbedShellPrivate
{
	EphyHistoryService *global_history_service;
	GList *downloads;
	EphyEmbedSingle *embed_single;
	EphyEncodings *encodings;
	EphyAdBlockManager *adblock_manager;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;
	EphyEmbedShellMode mode;
	guint single_initialised : 1;
};

enum
{
	DOWNLOAD_ADDED,
	DOWNLOAD_REMOVED,
	PREPARE_CLOSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
	PROP_0,
	PROP_MODE,
	N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static void ephy_embed_shell_class_init	(EphyEmbedShellClass *klass);
static void ephy_embed_shell_init	(EphyEmbedShell *shell);

EphyEmbedShell *embed_shell = NULL;

G_DEFINE_TYPE (EphyEmbedShell, ephy_embed_shell, GTK_TYPE_APPLICATION)

static void
ephy_embed_shell_dispose (GObject *object)
{
	EphyEmbedShell *shell = EPHY_EMBED_SHELL (object);
	EphyEmbedShellPrivate *priv = shell->priv;

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

	if (shell->priv->downloads != NULL)
	{
		LOG ("Destroying downloads list");
		g_list_foreach (shell->priv->downloads, (GFunc) g_object_unref, NULL);
		g_list_free (shell->priv->downloads);
		shell->priv->downloads = NULL;
	}

	if (shell->priv->global_history_service)
	{
		LOG ("Unref history service");
		g_object_unref (shell->priv->global_history_service);
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
 * ephy_embed_shell_get_global_history_service:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none): the global #EphyHistoryService
 **/
GObject *
ephy_embed_shell_get_global_history_service (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->global_history_service == NULL)
	{
		char *filename;

		filename = g_build_filename (ephy_dot_dir (), "ephy-history.db", NULL);
		shell->priv->global_history_service = ephy_history_service_new (filename);
		g_free (filename);
		g_return_val_if_fail (shell->priv->global_history_service, NULL);
	}

	return G_OBJECT (shell->priv->global_history_service);
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

/**
 * ephy_embed_shell_get_embed_single:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_embed_shell_get_embed_single (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);

	return klass->get_embed_single (shell);
}

/**
 * ephy_embed_shell_get_encodings:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none):
 **/
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
ephy_embed_shell_set_property (GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (object);

  switch (prop_id)
  {
  case PROP_MODE:
	  embed_shell->priv->mode = g_value_get_enum (value);
	  break;
  default:
	  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_embed_shell_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (object);

  switch (prop_id)
  {
  case PROP_MODE:
	  g_value_set_enum (value, embed_shell->priv->mode);
	  break;
  default:
	  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_embed_shell_init (EphyEmbedShell *shell)
{
	shell->priv = EPHY_EMBED_SHELL_GET_PRIVATE (shell);

	/* globally accessible singleton */
	g_assert (embed_shell == NULL);
	embed_shell = shell;

	shell->priv->downloads = NULL;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ephy_embed_shell_dispose;
	object_class->finalize = ephy_embed_shell_finalize;
	object_class->set_property = ephy_embed_shell_set_property;
	object_class->get_property = ephy_embed_shell_get_property;

	klass->get_embed_single = impl_get_embed_single;

	object_properties[PROP_MODE] =
		g_param_spec_enum ("mode",
				   "Mode",
				   "The	 global mode for this instance of Epiphany .",
				   EPHY_TYPE_EMBED_SHELL_MODE,
				   EPHY_EMBED_SHELL_MODE_BROWSER,
				   G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
				   G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY);
	
	g_object_class_install_properties (object_class,
					   N_PROPERTIES,
					   object_properties);

/**
 * EphyEmbed::download-added:
 * @shell: the #EphyEmbedShell
 * @download: the #EphyDownload added
 *
 * Emitted when a #EphyDownload has been added to the global watch list of
 * @shell, via ephy_embed_shell_add_download.
 **/
	signals[DOWNLOAD_ADDED] =
		g_signal_new ("download-added",
			      EPHY_TYPE_EMBED_SHELL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedShellClass, download_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, EPHY_TYPE_DOWNLOAD);

/**
 * EphyEmbed::download-removed:
 * @shell: the #EphyEmbedShell
 * @download: the #EphyDownload being removed
 *
 * Emitted when a #EphyDownload has been removed from the global watch list of
 * @shell, via ephy_embed_shell_remove_download.
 **/
	signals[DOWNLOAD_REMOVED] =
		g_signal_new ("download-removed",
			      EPHY_TYPE_EMBED_SHELL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedShellClass, download_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, EPHY_TYPE_DOWNLOAD);

/**
 * EphyEmbed::prepare-close:
 * @shell: the #EphyEmbedShell
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

	g_type_class_add_private (object_class, sizeof (EphyEmbedShellPrivate));
}

/**
 * ephy_embed_shell_get_default:
 *
 * Retrieves the default #EphyEmbedShell object
 *
 * Return value: (transfer none): the default #EphyEmbedShell
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
 * Return value: (transfer none): the adblock manager
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

/**
 * ephy_embed_shell_get_page_setup:
 *
 * Return value: (transfer none):
 **/
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

/**
 * ephy_embed_shell_set_print_gettings:
 * @shell: the #EphyEmbedShell
 * @settings: the new #GtkPrintSettings object
 *
 * Sets the global #GtkPrintSettings object.
 *
 **/
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

/**
 * ephy_embed_shell_get_print_settings:
 * @shell: the #EphyEmbedShell
 *
 * Gets the global #GtkPrintSettings object.
 *
 * Returns: (transfer none): a #GtkPrintSettings object
 **/
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

		/* Note: the gtk print settings file format is the same as our
		 * legacy one, so no need to migrate here.
		 */

		if (priv->print_settings == NULL)
		{
			priv->print_settings = gtk_print_settings_new ();
		}
	}

	return priv->print_settings;
}

/**
 * ephy_embed_shell_get_downloads:
 * @shell: the #EphyEmbedShell
 *
 * Gets the global #GList object listing active downloads.
 *
 * Returns: (transfer none) (element-type EphyDownload): a #GList object
 **/
GList *
ephy_embed_shell_get_downloads (EphyEmbedShell *shell)
{
	EphyEmbedShellPrivate *priv;

	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);
	priv = shell->priv;

	return priv->downloads;
}

void
ephy_embed_shell_add_download (EphyEmbedShell *shell, EphyDownload *download)
{
	EphyEmbedShellPrivate *priv;

	g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));

	priv = shell->priv;
	priv->downloads = g_list_prepend (priv->downloads, download);

	g_signal_emit_by_name (shell, "download-added", download, NULL);
}

void
ephy_embed_shell_remove_download (EphyEmbedShell *shell, EphyDownload *download)
{
	EphyEmbedShellPrivate *priv;

	g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));

	priv = shell->priv;
	priv->downloads = g_list_remove (priv->downloads, download);

	g_signal_emit_by_name (shell, "download-removed", download, NULL);
}

/**
 * ephy_embed_shell_get_mode:
 * @shell: an #EphyEmbedShell
 * 
 * Returns: the global mode of the @shell
 **/
EphyEmbedShellMode
ephy_embed_shell_get_mode (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), EPHY_EMBED_SHELL_MODE_BROWSER);
	
	return shell->priv->mode;
}
