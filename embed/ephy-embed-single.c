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
#include "mozilla-embed-single.h"
#include "ephy-debug.h"
#include "downloader-view.h"

#include <string.h>

enum
{
	NEW_WINDOW,
	LAST_SIGNAL
};

struct EphyEmbedSinglePrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	GList *embeds;
	EphyFaviconCache *favicon_cache;
};

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass);
static void
ephy_embed_single_init (EphyEmbedSingle *ges);
static void
ephy_embed_single_finalize (GObject *object);

static GObjectClass *parent_class = NULL;
static guint ephy_embed_single_signals[LAST_SIGNAL] = { 0 };

GType
ephy_embed_single_get_type (void)
{
       static GType ephy_embed_single_type = 0;

        if (ephy_embed_single_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEmbedSingleClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_embed_single_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (EphyEmbedSingle),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) ephy_embed_single_init
                };

                ephy_embed_single_type = g_type_register_static (G_TYPE_OBJECT,
								"EphyEmbedSingle",
								&our_info, 0);
        }

        return ephy_embed_single_type;
}

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

        object_class->finalize = ephy_embed_single_finalize;

	ephy_embed_single_signals[NEW_WINDOW] =
                g_signal_new ("new_window_orphan",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedSingleClass, new_window),
                              NULL, NULL,
                              ephy_marshal_VOID__POINTER_INT,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_POINTER,
			      G_TYPE_INT);
}

static void
ephy_embed_single_init (EphyEmbedSingle *ges)
{
	ges->priv = g_new0 (EphyEmbedSinglePrivate, 1);

	ges->priv->global_history = NULL;
	ges->priv->downloader_view = NULL;
	ges->priv->embeds = NULL;

	ges->priv->favicon_cache = NULL;
}

static void
ephy_embed_single_finalize (GObject *object)
{
	EphyEmbedSingle *ges;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_SINGLE (object));

        ges = EPHY_EMBED_SINGLE (object);

        g_return_if_fail (ges->priv != NULL);

        g_free (ges->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

gresult
ephy_embed_single_clear_cache (EphyEmbedSingle *shell)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->clear_cache (shell);
}

gresult
ephy_embed_single_set_offline_mode (EphyEmbedSingle *shell,
				   gboolean offline)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->set_offline_mode (shell, offline);
}

gresult
ephy_embed_single_load_proxy_autoconf (EphyEmbedSingle *shell,
				      const char* url)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->load_proxy_autoconf (shell, url);
}

gresult
ephy_embed_single_get_encodings (EphyEmbedSingle *shell,
				 LanguageGroup group,
				 gboolean elide_underscores,
				 GList **encodings)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	return klass->get_encodings (shell, group, elide_underscores, encodings);
}

gresult
ephy_embed_single_get_language_groups (EphyEmbedSingle *shell,
			              GList **groups)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	return klass->get_language_groups (shell, groups);
}

gresult
ephy_embed_single_get_font_list (EphyEmbedSingle *shell,
				const char *langGroup,
				const char *fontType,
				GList **fontList,
				char **default_font)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->get_font_list (shell, langGroup, fontType, fontList,
				     default_font);
}

gresult
ephy_embed_single_list_cookies (EphyEmbedSingle *shell,
			       GList **cookies)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->list_cookies (shell, cookies);
}

gresult
ephy_embed_single_remove_cookies (EphyEmbedSingle *shell,
				 GList *cookies)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->remove_cookies (shell, cookies);
}

gresult
ephy_embed_single_list_passwords (EphyEmbedSingle *shell,
				 PasswordType type,
				 GList **passwords)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->list_passwords (shell, type, passwords);
}

gresult
ephy_embed_single_remove_passwords (EphyEmbedSingle *shell,
				   GList *passwords,
				   PasswordType type)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
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
ephy_embed_single_show_file_picker (EphyEmbedSingle *shell,
				   GtkWidget *parentWidget,
				   const char *title,
				   const char *directory,
		                   const char *file,
				   FilePickerMode mode,
				   char **ret_fullpath,
				   FileFormat *file_formats,
				   int *ret_file_format)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
        return klass->show_file_picker (shell, parentWidget, title,
					directory, file, mode,
					ret_fullpath,
					file_formats, ret_file_format);
}

gresult
ephy_embed_single_free_cookies (EphyEmbedSingle *shell,
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
ephy_embed_single_free_passwords (EphyEmbedSingle *shell,
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

