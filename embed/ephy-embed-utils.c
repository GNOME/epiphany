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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eel-gconf-extensions.h"
#include "ephy-embed-utils.h"
#include "ephy-embed-shell.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-gui.h"
#include "ephy-debug.h"
#include "ephy-langs.h"
#include "ephy-encodings.h"

#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-ui-util.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

/**
 * ephy_embed_utils_save:
 * @window: the referrer window. Used to parent the dialogs.
 * @title: title of the file picker
 * @default_dir_pref: the gconf path to persist the directory selected by the user.
 * @ask_dest: ask the user the destination path
 * @ask_content: show the user an option to save the content
 * of the web page (images, javascript...)
 * @persist: the #EphyEmbedPersist referring to the url
 *
 * Download a save an url asking a location to the user when requested
 **/
void
ephy_embed_utils_save (GtkWidget *window,
		       const char *title,
		       const char *default_dir_pref,
		       gboolean ask_dest,
		       EphyEmbedPersist *persist)
{
	GnomeVFSURI *uri;
        char *retPath = NULL;
        char *fileName = NULL;
        char *dirName = NULL;
	char *target;
	const char *source;
        gresult ret;
	EphyEmbed *embed;
	EmbedPersistFlags flags;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (embed_shell));

	g_object_ref (G_OBJECT(persist));

	ephy_embed_persist_get_flags (persist, &flags);

	ephy_embed_persist_get_source (persist, &source);

	if (source)
	{
		target = g_strdup (source);
	}
	else
	{
		ephy_embed_persist_get_embed (persist, &embed);
		g_return_if_fail (embed != NULL);

		ephy_embed_get_location (embed,
					 flags &
					 EMBED_PERSIST_MAINDOC,
					 &target);
	}

        /* Get a filename from the target url */
        uri = gnome_vfs_uri_new (target);
        if (uri)
        {
                fileName = gnome_vfs_uri_extract_short_name (uri);
                gnome_vfs_uri_unref (uri);
        }

        dirName = eel_gconf_get_string (default_dir_pref);
        if (dirName && dirName[0] == '\0')
        {
		g_free (dirName);
                dirName = NULL;
        }

        if (!dirName || strcmp (dirName,"~") == 0)
        {
                g_free (dirName);
                dirName = g_strdup(g_get_home_dir ());
        }

	/* If we aren't asking for downloading dir, check that we aren't
	 * overwriting anything.  If we are, pop up a warning dialog and show
	 * the filepicker if the user doesn't want to overwrite the file.
	 */
	ret = G_FAILED;
	if (!ask_dest)
	{
		retPath = g_build_filename (dirName, fileName, NULL);
		if (ephy_gui_confirm_overwrite_file (window, retPath))
		{
			ret = G_OK;
		}
		else
		{
			g_free (retPath);
			retPath = NULL;
			ask_dest = TRUE;
		}
	}

	if (ask_dest)
	{
		char *ret_dir;

		/* show the file picker */
		ret = ephy_embed_single_show_file_picker
					(single, window, title,
                                         dirName, fileName, modeSave, &retPath,
                                         NULL, NULL);

		if (g_file_test (retPath, G_FILE_TEST_IS_DIR))
		{
			ret_dir = g_strdup (retPath);
		}
		else
		{
			ret_dir = g_path_get_dirname (retPath);
		}
	
		/* set default save dir */
		eel_gconf_set_string (default_dir_pref, ret_dir);
		g_free (ret_dir);
	}

        if (ret == G_OK)
        {
		ephy_embed_persist_set_flags (persist, flags);

		ephy_embed_persist_set_dest (persist, retPath);

		if (ephy_embed_persist_save (persist) == G_FAILED)
		{
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new
				(GTK_WINDOW (window),
	                         GTK_DIALOG_MODAL,
	                         GTK_MESSAGE_ERROR,
	                         GTK_BUTTONS_CLOSE,
	                         _("The file has not been saved."));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}

	}

	g_free (retPath);

	g_object_unref (G_OBJECT(persist));

        g_free (dirName);
        g_free (fileName);
	g_free (target);
}

/**
 * ephy_embed_utils_build_encodings_submenu:
 * @ui_component: the parent #BonoboUIComponent
 * @path: the bonoboui path where to create the submenu.
 * It's recommended to use a <placeholder/>
 * @fn: callback to report the selected encodings
 * @data: the data passed to the callback
 *
 * Create a encoding submenu using bonobo ui.
 **/
void
ephy_embed_utils_build_encodings_submenu (BonoboUIComponent *ui_component,
					  const char *path,
					  BonoboUIVerbFn fn,
					  gpointer view)
{
	gchar *tmp, *verb;
	GString *xml_string;
	GList *groups, *gl, *encodings, *l;
	GList *verbs = NULL;

	START_PROFILER ("Encodings menu")

	xml_string = g_string_new (NULL);
	g_string_append (xml_string, "<submenu name=\"Encoding\" _label=\"_Encoding\">");

	groups = ephy_lang_get_group_list ();
	for (gl = groups; gl != NULL; gl = gl->next)
        {
		const EphyLanguageGroupInfo *lang_info = (EphyLanguageGroupInfo *) gl->data;

		tmp = g_strdup_printf ("<submenu label=\"%s\" name=\"EncodingGroup%d\">\n", 
					lang_info->title, lang_info->group);
		xml_string = g_string_append (xml_string, tmp);
		g_free (tmp);

		encodings = ephy_encodings_get_list (lang_info->group, FALSE);
		for (l = encodings; l != NULL; l = l->next)
                {
			const EphyEncodingInfo *info = (EphyEncodingInfo *) l->data;

			verb = g_strdup_printf ("Encoding%s", info->encoding);
			tmp = g_strdup_printf ("<menuitem label=\"%s\" name=\"%s\" verb=\"%s\"/>\n",
						info->title, verb, verb);
			xml_string = g_string_append (xml_string, tmp);

			verbs = g_list_prepend (verbs, verb);

			g_free (tmp);
		}

		g_list_foreach (encodings, (GFunc) ephy_encoding_info_free, NULL);
		g_list_free (encodings);

		g_string_append (xml_string, "</submenu>");
	}

	g_list_foreach (groups, (GFunc) ephy_lang_group_info_free, NULL);
	g_list_free (groups);

	g_string_append (xml_string, "</submenu>");

	bonobo_ui_component_set_translate (ui_component, path,
					   xml_string->str, NULL);

	for (l = verbs; l != NULL; l = l->next)
	{
		bonobo_ui_component_add_verb (ui_component,
					      (const char *) l->data,
					      fn, view);
	}

	g_list_foreach (verbs, (GFunc) g_free, NULL);
	g_list_free (verbs);

	g_string_free (xml_string, TRUE);

	STOP_PROFILER ("Encodings menu")
}

/**
 * ephy_embed_utils_handlernotfound_dialog_run:
 * @parent: the dialog parent window
 *
 * Show a dialog to warn the user that no application capable
 * to open the specified file are found. Used in the downloader
 * and in the mime type dialog.
 **/
void
ephy_embed_utils_nohandler_dialog_run (GtkWidget *parent)
{
        GtkWidget *dialog;
        
	/* FIXME mime db shortcut */
	
	dialog = gtk_message_dialog_new 
		(GTK_WINDOW(parent), 
                 GTK_DIALOG_MODAL,
                 GTK_MESSAGE_ERROR,
                 GTK_BUTTONS_OK,
                 _("No available applications to open "
                   "the specified file."));
        gtk_dialog_run (GTK_DIALOG(dialog));
	gtk_widget_destroy (dialog);
}
