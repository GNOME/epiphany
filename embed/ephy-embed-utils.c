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

#include "eel-gconf-extensions.h"
#include "ephy-embed-utils.h"
#include "ephy-embed-shell.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-ui-util.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

/**
 * ephy_embed_utils_save:
 * @window: the referrer window. Used to parent the dialogs.
 * @default_dir_pref: the gconf path to persist the directory selected by the user.
 * @ask_dest: ask the user the destination path
 * @ask_content: show the user an option to save the content
 * of the web page (images, javascript...)
 * @persist: the #GaleonEmbedPersist referring to the url
 *
 * Download a save an url asking a location to the user when requested
 **/
void
ephy_embed_utils_save (GtkWidget *window,
		       const char *default_dir_pref,
		       gboolean ask_dest, gboolean ask_content,
		       EphyEmbedPersist *persist)
{
	GnomeVFSURI *uri;
        char *retPath = NULL;
        char *fileName = NULL;
        char *dirName = NULL;
        char *retDir;
	char *target;
	const char *source;
        gresult ret;
	EphyEmbed *embed;
	EmbedPersistFlags flags;
	gboolean content;
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
			ask_dest = TRUE;
		}
	}

	if (ask_dest)
	{
		/* show the file picker */
		ret = ephy_embed_single_show_file_picker
					(single, window,
                                         _("Select the destination filename"),
                                         dirName, fileName, modeSave, &retPath,
                                         ask_content ? &content : NULL,
                                         NULL, NULL);
	}

        if (ret == G_OK)
        {
                uri = gnome_vfs_uri_new (retPath);
		g_return_if_fail (uri != NULL);

                retDir = gnome_vfs_uri_extract_dirname (uri);

                if (ask_content && content)
		{
			flags |= EMBED_PERSIST_SAVE_CONTENT;
			ephy_embed_persist_set_flags (persist,
						      flags);
		}

		ephy_embed_persist_set_dest (persist, retPath);

		ephy_embed_persist_save (persist);

                /* set default save dir */
                eel_gconf_set_string (default_dir_pref,
                                     retDir);

                g_free (retDir);
                gnome_vfs_uri_unref (uri);
        }

	g_object_unref (G_OBJECT(persist));

        g_free (dirName);
        g_free (fileName);
        g_free (retPath);
}

static void
build_group (GString *xml_string, const char *group, int index)
{
	char *tmp;

	tmp = g_strdup_printf ("<submenu label=\"%s\" name=\"CharsetGroup%d\">\n", 
			       group, index);
	xml_string = g_string_append (xml_string, tmp);
	g_free (tmp);
}

static void
build_charset (GString *xml_string, const CharsetInfo *info, int index)
{
	char *tmp;
	char *verb;

	verb = g_strdup_printf ("Charset%d", index);
	tmp = g_strdup_printf ("<menuitem label=\"%s\" name=\"%s\" verb=\"%s\"/>\n",
			       info->title, verb, verb);
	xml_string = g_string_append (xml_string, tmp);

	g_free (tmp);
	g_free (verb);
}

static void
add_verbs (BonoboUIComponent *ui_component,
	   BonoboUIVerbFn fn, GList *verbs)
{
	GList *l;
	char verb[15];
	int charset_index = 0;

	for (l = verbs; l != NULL; l = l->next)
	{
		EncodingMenuData *edata = (EncodingMenuData *)l->data;

		sprintf (verb, "Charset%d", charset_index);
		charset_index++;
		bonobo_ui_component_add_verb_full
			(ui_component, verb,
		         g_cclosure_new (G_CALLBACK (fn), edata,
			 (GClosureNotify)g_free));
	}
}

/**
 * ephy_embed_utils_build_charsets_submenu:
 * @ui_component: the parent #BonoboUIComponent
 * @path: the bonoboui path where to create the submenu.
 * It's recommended to use a <placeholder/>
 * @fn: callback to report the selected charsets
 * @data: the data passed to the callback
 *
 * Create a charset submenu using bonobo ui.
 **/
void
ephy_embed_utils_build_charsets_submenu (BonoboUIComponent *ui_component,
					 const char *path,
					 BonoboUIVerbFn fn,
					 gpointer data)
{
	GList *groups, *gl;
	GString *xml_string;
	GList *verbs = NULL;
	int group_index = 0;
	int charset_index = 0;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (embed_shell));

	START_PROFILER ("Charsets menu")

	g_return_if_fail (IS_EPHY_EMBED_SHELL (embed_shell));
	g_return_if_fail (ephy_embed_single_get_charset_groups (single, &groups) == G_OK);

	xml_string = g_string_new (NULL);
	g_string_append (xml_string, "<submenu name=\"Encoding\" _label=\"_Encoding\">");

	for (gl = groups; gl != NULL; gl = gl->next)
        {
		GList *charsets, *cl;
		const char *group = (const char *)gl->data;

		build_group (xml_string, group, group_index);

		ephy_embed_single_get_charset_titles (single,
                                                      group,
                                                      &charsets);

		for (cl = charsets; cl != NULL; cl = cl->next)
                {
			const CharsetInfo *info = cl->data;
			EncodingMenuData *edata;

			edata = g_new0 (EncodingMenuData, 1);
			edata->encoding = info->name;
			edata->data = data;
			verbs = g_list_append (verbs, edata);

			build_charset (xml_string, info, charset_index);
			charset_index++;
		}


		g_list_free (charsets);
		g_string_append (xml_string, "</submenu>");
		group_index++;
	}

	g_string_append (xml_string, "</submenu>");

	bonobo_ui_component_set_translate (ui_component, path,
					   xml_string->str, NULL);
	add_verbs (ui_component, fn, verbs);

	g_list_free (verbs);
	g_list_free (groups);
	g_string_free (xml_string, TRUE);

	STOP_PROFILER ("Charsets menu")
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
