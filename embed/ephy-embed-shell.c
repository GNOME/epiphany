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

#include <config.h>

#include "ephy-embed-shell.h"
#include "ephy-marshal.h"
#include "ephy-favicon-cache.h"
#include "mozilla-embed-shell.h"
#include "ephy-debug.h"

#include <string.h>

enum
{
	NEW_WINDOW,
	COMMAND,
	LAST_SIGNAL
};

struct EphyEmbedShellPrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	GList *embeds;
	EphyFaviconCache *favicon_cache;
};

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass);
static void
ephy_embed_shell_init (EphyEmbedShell *ges);
static void
ephy_embed_shell_finalize (GObject *object);

static EphyHistory *
impl_get_global_history (EphyEmbedShell *shell);
static DownloaderView *
impl_get_downloader_view (EphyEmbedShell *shell);

static GObjectClass *parent_class = NULL;
static guint ephy_embed_shell_signals[LAST_SIGNAL] = { 0 };

EphyEmbedShell *embed_shell;

GType
ephy_embed_shell_get_impl (void)
{
	return MOZILLA_EMBED_SHELL_TYPE;
}

GType
ephy_embed_shell_get_type (void)
{
       static GType ephy_embed_shell_type = 0;

        if (ephy_embed_shell_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEmbedShellClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_embed_shell_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (EphyEmbedShell),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) ephy_embed_shell_init
                };

                ephy_embed_shell_type = g_type_register_static (G_TYPE_OBJECT,
								"EphyEmbedShell",
								&our_info, 0);
        }

        return ephy_embed_shell_type;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

        object_class->finalize = ephy_embed_shell_finalize;
	klass->get_downloader_view = impl_get_downloader_view;
	klass->get_global_history = impl_get_global_history;

	ephy_embed_shell_signals[NEW_WINDOW] =
                g_signal_new ("new_window_orphan",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedShellClass, new_window),
                              NULL, NULL,
                              ephy_marshal_VOID__POINTER_INT,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_POINTER,
			      G_TYPE_INT);
	ephy_embed_shell_signals[COMMAND] =
                g_signal_new ("command",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedShellClass, command),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING_STRING,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
			      G_TYPE_STRING);
}

static void
ephy_embed_shell_init (EphyEmbedShell *ges)
{

	/* Singleton, globally accessible */
	embed_shell = ges;

	ges->priv = g_new0 (EphyEmbedShellPrivate, 1);

	ges->priv->global_history = NULL;
	ges->priv->downloader_view = NULL;
	ges->priv->embeds = NULL;

	ges->priv->favicon_cache = NULL;
}

static void
ephy_embed_shell_finalize (GObject *object)
{
	EphyEmbedShell *ges;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_SHELL (object));

        ges = EPHY_EMBED_SHELL (object);

        g_return_if_fail (ges->priv != NULL);

	LOG ("Unref history")
	if (ges->priv->global_history)
	{
		g_object_unref (ges->priv->global_history);
	}

	LOG ("Unref downloader")
	if (ges->priv->downloader_view)
	{
		g_object_remove_weak_pointer
			(G_OBJECT(ges->priv->downloader_view),
			 (gpointer *)&ges->priv->downloader_view);
		g_object_unref (ges->priv->downloader_view);
	}

	LOG ("Unref favicon cache")
	if (ges->priv->favicon_cache)
	{
		g_object_unref (G_OBJECT (ges->priv->favicon_cache));
	}

        g_free (ges->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyEmbedShell *
ephy_embed_shell_new (const char *type)
{
	if (strcmp (type, "mozilla") == 0)
	{
		return EPHY_EMBED_SHELL (g_object_new
			(MOZILLA_EMBED_SHELL_TYPE, NULL));
	}

	g_assert_not_reached ();
	return NULL;
}

/**
 * ephy_embed_shell_get_favicon_cache:
 * @gs: a #EphyShell
 *
 * Returns the favicons cache.
 *
 * Return value: the favicons cache
 **/
EphyFaviconCache *
ephy_embed_shell_get_favicon_cache (EphyEmbedShell *ees)
{
	if (ees->priv->favicon_cache == NULL)
	{
		ees->priv->favicon_cache = ephy_favicon_cache_new ();
	}

	return ees->priv->favicon_cache;
}

void
ephy_embed_shell_add_embed (EphyEmbedShell *ges,
                            EphyEmbed *embed)
{
	ges->priv->embeds = g_list_append (ges->priv->embeds, embed);
}

void
ephy_embed_shell_remove_embed (EphyEmbedShell *ges,
                               EphyEmbed *embed)
{
	ges->priv->embeds = g_list_remove (ges->priv->embeds, embed);
}

EphyEmbed *
ephy_embed_shell_get_active_embed (EphyEmbedShell *ges)
{
	GList *list = ges->priv->embeds;

        g_return_val_if_fail (ges->priv->embeds != NULL, NULL);

        return EPHY_EMBED (list->data);
}

GList *
ephy_embed_shell_get_embeds (EphyEmbedShell *ges)
{
	return ges->priv->embeds;
}

void
ephy_embed_shell_get_capabilities (EphyEmbedShell *shell,
				   EmbedShellCapabilities *caps)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_capabilities (shell, caps);
}

EphyHistory *
ephy_embed_shell_get_global_history (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_global_history (shell);
}

DownloaderView *
ephy_embed_shell_get_downloader_view (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_downloader_view (shell);
}

gresult
ephy_embed_shell_clear_cache (EphyEmbedShell *shell,
			      CacheType type)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->clear_cache (shell, type);
}

gresult
ephy_embed_shell_set_offline_mode (EphyEmbedShell *shell,
				   gboolean offline)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->set_offline_mode (shell, offline);
}

gresult
ephy_embed_shell_load_proxy_autoconf (EphyEmbedShell *shell,
				      const char* url)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->load_proxy_autoconf (shell, url);
}

gresult
ephy_embed_shell_get_charset_titles (EphyEmbedShell *shell,
				     const char *group,
				     GList **charsets)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_charset_titles (shell, group, charsets);
}

gresult
ephy_embed_shell_get_charset_groups (EphyEmbedShell *shell,
			             GList **groups)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_charset_groups (shell, groups);
}

gresult
ephy_embed_shell_get_font_list (EphyEmbedShell *shell,
				const char *langGroup,
				const char *fontType,
				GList **fontList,
				char **default_font)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_font_list (shell, langGroup, fontType, fontList,
				     default_font);
}

gresult
ephy_embed_shell_list_cookies (EphyEmbedShell *shell,
			       GList **cookies)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->list_cookies (shell, cookies);
}

gresult
ephy_embed_shell_remove_cookies (EphyEmbedShell *shell,
				 GList *cookies)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->remove_cookies (shell, cookies);
}

gresult
ephy_embed_shell_list_passwords (EphyEmbedShell *shell,
				 PasswordType type,
				 GList **passwords)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->list_passwords (shell, type, passwords);
}

gresult
ephy_embed_shell_remove_passwords (EphyEmbedShell *shell,
				   GList *passwords,
				   PasswordType type)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->remove_passwords (shell, passwords, type);
}

/**
 * show_file_picker: Shows a file picker. Can be configured to select a
 * file or a directory.
 * @parentWidget: Parent Widget for file picker.
 * @title: Title for file picker.
 * @directory: Initial directory to start in.
 * @file: Initial filename to show in filepicker.
 * @mode: Mode to run filepicker in (modeOpen, modeSave, modeGetFolder)
 * @ret_fullpath: On a successful return, will hold the full path to selected
 *		file or directory.
 * @file_formats: an array of FileFormat structures to fill the format chooser
 *              optionmenu. NULL if not needed. The last item must have
 *              description == NULL.
 * @ret_file_format: where to store the index of the format selected (can be
 *              NULL)
 * returns: TRUE for success, FALSE for failure.
 */

gresult
ephy_embed_shell_show_file_picker (EphyEmbedShell *shell,
				   GtkWidget *parentWidget,
				   const char *title,
				   const char *directory,
		                   const char *file,
				   FilePickerMode mode,
				   char **ret_fullpath,
				   gboolean *ret_save_content,
				   FileFormat *file_formats,
				   int *ret_file_format)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->show_file_picker (shell, parentWidget, title,
					directory, file, mode,
					ret_fullpath, ret_save_content,
					file_formats, ret_file_format);
}

static EphyHistory *
impl_get_global_history (EphyEmbedShell *shell)
{
	if (!shell->priv->global_history)
	{
		shell->priv->global_history = ephy_history_new ();
	}

	return shell->priv->global_history;
}

static DownloaderView *
impl_get_downloader_view (EphyEmbedShell *shell)
{
	if (!shell->priv->downloader_view)
	{
		shell->priv->downloader_view = downloader_view_new ();
		g_object_add_weak_pointer
			(G_OBJECT(shell->priv->downloader_view),
			 (gpointer *)&shell->priv->downloader_view);
	}

	return shell->priv->downloader_view;
}

gresult
ephy_embed_shell_free_cookies (EphyEmbedShell *shell,
			       GList *cookies)
{
	GList *l;

	for (l = cookies; l != NULL; l = l->next)
	{
		CookieInfo *info = (CookieInfo *)l->data;

		g_free (info->domain);
		g_free (info->name);
		g_free (info->value);
		g_free (info->path);
		g_free (info->secure);
		g_free (info->expire);
		g_free (info);
	}

	g_list_free (cookies);

	return G_OK;
}

gresult
ephy_embed_shell_free_passwords (EphyEmbedShell *shell,
				 GList *passwords)
{
	GList *l;

	for (l = passwords; l != NULL; l = l->next)
	{
		PasswordInfo *info = (PasswordInfo *)l->data;
		g_free (info->host);
		g_free (info->username);
		g_free (info);
	}

	g_list_free (passwords);

	return G_OK;
}

