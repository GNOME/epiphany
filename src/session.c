/*
 *  Copyright (C) 2002 Jorn Baayen
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

#include "session.h"
#include "ephy-shell.h"
#include "ephy-tab.h"
#include "ephy-window.h"
#include "ephy-prefs.h"
#include "ephy-string.h"
#include "ephy-file-helpers.h"
#include "eel-gconf-extensions.h"

#include <bonobo/bonobo-i18n.h>
#include <string.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomeui/gnome-client.h>

enum
{
	RESTORE_TYPE_PROP
};

enum
{
	RESTORE_SESSION,
	RESTORE_AS_BOOKMARKS,
	DISCARD_SESSION
};

static void session_class_init (SessionClass *klass);
static void session_init (Session *t);
static void session_finalize (GObject *object);
static void session_dispose (GObject *object);

static GObjectClass *parent_class = NULL;

struct SessionPrivate
{
	GList *windows;
	gboolean dont_remove_crashed;
};

enum
{
	NEW_WINDOW,
	CLOSE_WINDOW,
        LAST_SIGNAL
};

static guint session_signals[LAST_SIGNAL] = { 0 };

GType
session_get_type (void)
{
        static GType session_type = 0;

        if (session_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (SessionClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) session_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (Session),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) session_init
                };

                session_type = g_type_register_static (G_TYPE_OBJECT,
						         "Session",
						         &our_info, 0);
        }

        return session_type;

}

static void
session_class_init (SessionClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = session_finalize;
	object_class->dispose = session_dispose;

	session_signals[NEW_WINDOW] =
                g_signal_new ("new_window",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (SessionClass, new_window),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_OBJECT);

	session_signals[CLOSE_WINDOW] =
		g_signal_new ("close_window",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (SessionClass, close_window),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

static char *
get_session_filename (const char *filename)
{
	char *save_to;

	g_return_val_if_fail (filename != NULL, NULL);

	if (strcmp (filename, SESSION_CRASHED) == 0)
	{
		save_to = g_build_filename (ephy_dot_dir (),
					    "session_crashed.xml",
					    NULL);
	}
	else if (strcmp (filename, SESSION_GNOME) == 0)
	{
		char *tmp;

		save_to = g_build_filename (ephy_dot_dir (),
					    "session_gnome-XXXXXX",
					    NULL);
		tmp = ephy_file_tmp_filename (save_to, "xml");
		g_free (save_to);
		save_to = tmp;
	}
	else
	{
		save_to = g_strdup (filename);
	}

	return save_to;
}

static void
do_session_resume (Session *session)
{
	char *crashed_session;

	crashed_session = get_session_filename (SESSION_CRASHED);
	session_load (session, crashed_session);
	g_free (crashed_session);
}

static void
crashed_resume_dialog (Session *session)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	char *str;

	dialog = gtk_dialog_new_with_buttons
		(_("Crash Recovery"), NULL,
		 GTK_DIALOG_NO_SEPARATOR,
		 _("_Don't Recover"), GTK_RESPONSE_CANCEL,
		 _("_Recover"), GTK_RESPONSE_OK,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_BOX (hbox)), 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    TRUE, TRUE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					  GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox), image,
			    TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox,
			    TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	str = g_strconcat ("<b>", _("Epiphany appears to have crashed or been killed the last time it was run."),
			   "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	g_free (str);

	label = gtk_label_new (_("You can recover the opened tabs and windows."));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label,
			    TRUE, TRUE, 0);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
	{
		do_session_resume (session);
	}

	gtk_widget_destroy (dialog);
}

/**
 * session_autoresume:
 * @session: a #Session
 *
 * Resume a crashed session when necessary (interactive)
 *
 * Return value: return false if no window  has been opened
 **/
gboolean
session_autoresume (Session *session)
{
	char *saved_session;
	gboolean loaded = FALSE;

	saved_session = get_session_filename (SESSION_CRASHED);

	if (g_file_test (saved_session, G_FILE_TEST_EXISTS))
	{
		crashed_resume_dialog (session);
		loaded = TRUE;
	}

	g_free (saved_session);

	/* return false if no window has been opened */
	return (session->priv->windows != NULL);
}

static void
create_session_directory (void)
{
	char *dir;

	dir = g_build_filename (ephy_dot_dir (),
			        "/sessions",
			        NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS))
	{
		if (mkdir (dir, 488) != 0)
                {
                        g_error ("couldn't make %s' directory", dir);
                }
	}

	g_free (dir);
}

static gboolean
save_yourself_cb  (GnomeClient *client,
                   gint phase,
                   GnomeSaveStyle save_style,
                   gboolean shutdown,
                   GnomeInteractStyle interact_style,
                   gboolean fast,
		   Session *session)
{
	char *argv[] = { "ephy", "--load-session", NULL };
	char *discard_argv[] = { "rm", "-r", NULL };

	argv[2] = get_session_filename (SESSION_GNOME);
	gnome_client_set_restart_command
		(client, 3, argv);

	discard_argv[2] = argv[2];
	gnome_client_set_discard_command (client, 3,
					  discard_argv);

	session_save (session, argv[2]);

	g_free (argv[2]);

	return TRUE;
}

static void
session_die_cb (GnomeClient* client,
		Session *session)
{
	session_close (session);
}

static void
gnome_session_init (Session *session)
{
	GnomeClient *client;

	client = gnome_master_client ();

	g_signal_connect (G_OBJECT (client),
			  "save_yourself",
			  G_CALLBACK (save_yourself_cb),
			  session);

	g_signal_connect (G_OBJECT (client),
			  "die",
			  G_CALLBACK (session_die_cb),
			  session);
}

static void
session_init (Session *session)
{
        session->priv = g_new0 (SessionPrivate, 1);
	session->priv->windows = NULL;
	session->priv->dont_remove_crashed = FALSE;

	create_session_directory ();

	gnome_session_init (session);
}

/*
 * session_close:
 * @session: a #Session
 *
 * Close the session and all the owned windows
 **/
void
session_close (Session *session)
{
	GList *l, *windows;

	/* close all windows */
	windows	= g_list_copy (session->priv->windows);
	for (l = windows; l != NULL; l = l->next)
	{
		EphyWindow *window = EPHY_WINDOW(l->data);
		gtk_widget_destroy (GTK_WIDGET(window));
	}
	g_list_free (windows);
}

static void
session_delete (Session *session,
		const char *filename)
{
	char *save_to;

	save_to = get_session_filename (filename);

	gnome_vfs_unlink (save_to);

	g_free (save_to);
}

static void
session_dispose (GObject *object)
{
	Session *session = SESSION(object);

	if (!session->priv->dont_remove_crashed)
	{
		session_delete (session, SESSION_CRASHED);
	}
}

static void
session_finalize (GObject *object)
{
	Session *t;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_SESSION (object));

	t = SESSION (object);

        g_return_if_fail (t->priv != NULL);

	g_list_free (t->priv->windows);

        g_free (t->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * session_new:
 *
 * Create a #Session. A session hold the information
 * about the windows currently opened and is able to persist
 * and restore his status.
 **/
Session *
session_new (void)
{
	Session *t;

	t = SESSION (g_object_new (SESSION_TYPE, NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

static void
save_tab (EphyWindow *window,
	  EphyTab *tab,
	  xmlDocPtr doc,
	  xmlNodePtr window_node)
{
	EmbedChromeMask chrome;
	const char *location;
	const char *title;
        xmlNodePtr embed_node;

	chrome = ephy_window_get_chrome (window);

	/* skip if it's a XUL dialog */
        if (chrome & EMBED_CHROME_OPENASCHROME) return;

	/* make a new XML node */
        embed_node = xmlNewDocNode (doc, NULL,
                                    "embed", NULL);

        /* store title in the node */
	title = ephy_tab_get_title (tab);
	xmlSetProp (embed_node, "title", title);

        /* otherwise, use the actual location. */
	location = ephy_tab_get_location (tab);
        xmlSetProp (embed_node, "url", location);

	/* insert node into the tree */
	xmlAddChild (window_node, embed_node);
}

/*
 * session_save:
 * @session: a #Session
 * @filename: path of the xml file where the session is saved.
 *
 * Save the session on disk. Keep information about window size,
 * opened urls ...
 **/
void
session_save (Session *session,
	      const char *filename)
{
	const GList *w;
        xmlNodePtr root_node;
        xmlNodePtr window_node;
        xmlDocPtr doc;
        gchar buffer[32];
	char *save_to;

	save_to = get_session_filename (filename);

        doc = xmlNewDoc ("1.0");

        /* create and set the root node for the session */
        root_node = xmlNewDocNode (doc, NULL, "session", NULL);
        xmlDocSetRootElement (doc, root_node);

	w = session_get_windows (session);

        /* iterate through all the windows */
        for (; w != NULL; w = w->next)
        {
		GList *tabs, *l;
		int x = 0, y = 0, width = 0, height = 0;
		EphyWindow *window = EPHY_WINDOW(w->data);
		GtkWidget *wmain;

		tabs = ephy_window_get_tabs (window);
		g_return_if_fail (tabs != NULL);

                /* make a new XML node */
                window_node = xmlNewDocNode (doc, NULL, "window", NULL);

                /* get window geometry */
		wmain = GTK_WIDGET (window);
                gtk_window_get_size (GTK_WINDOW(wmain), &width, &height);
                gtk_window_get_position (GTK_WINDOW(wmain), &x, &y);

                /* set window properties */
                snprintf(buffer, 32, "%d", x);
                xmlSetProp (window_node, "x", buffer);
                snprintf(buffer, 32, "%d", y);

		xmlSetProp (window_node, "y", buffer);
                snprintf(buffer, 32, "%d", width);
                xmlSetProp (window_node, "width", buffer);
                snprintf(buffer, 32, "%d", height);
                xmlSetProp (window_node, "height", buffer);

		for (l = tabs; l != NULL; l = l->next)
	        {
			EphyTab *tab = EPHY_TAB(l->data);
			save_tab (window, tab, doc, window_node);
		}
		g_list_free (tabs);

		xmlAddChild (root_node, window_node);
        }

        /* save it all out to disk */
        xmlSaveFile (save_to, doc);
        xmlFreeDoc (doc);

	g_free (save_to);
}

static void
parse_embed (xmlNodePtr child, EphyWindow *window)
{
	while (child != NULL)
	{
		if (strcmp (child->name, "embed") == 0)
		{
			char *url;
			char *title;

			g_return_if_fail (window != NULL);

			url = xmlGetProp (child, "url");
			title = xmlGetProp (child, "title");

			ephy_shell_new_tab (ephy_shell, window, NULL, url,
					    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
					    EPHY_NEW_TAB_OPEN_PAGE |
					    EPHY_NEW_TAB_APPEND_LAST);

			xmlFree (url);
			xmlFree (title);
		}

		child = child->next;
	}
}

/*
 * session_load:
 * @session: a #Session
 * @filename: the path of the source file
 *
 * Load a session from disk, restoring the windows and their state
 **/
void
session_load (Session *session,
	      const char *filename)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	GtkWidget *wmain;
	EphyWindow *window;
	char *save_to;

	save_to = get_session_filename (filename);

	doc = xmlParseFile (save_to);

	child = xmlDocGetRootElement (doc);

	/* skip the session node */
	child = child->children;

	while (child != NULL)
	{
		if (strcmp (child->name, "window") == 0)
		{
			gulong x = 0, y = 0, width = 0, height = 0;
			xmlChar *tmp;

			tmp = xmlGetProp (child, "x");
			ephy_str_to_int (tmp, &x);
			xmlFree (tmp);
			tmp = xmlGetProp (child, "y");
			ephy_str_to_int (tmp, &y);
			xmlFree (tmp);
			tmp = xmlGetProp (child, "width");
			ephy_str_to_int (tmp, &width);
			xmlFree (tmp);
			tmp = xmlGetProp (child, "height");
			ephy_str_to_int (tmp, &height);
			xmlFree (tmp);

			window = ephy_window_new ();
			wmain = GTK_WIDGET (window);
			gtk_widget_show (GTK_WIDGET(window));

			gtk_window_move (GTK_WINDOW(wmain), x, y);
			gtk_window_set_default_size (GTK_WINDOW (wmain),
				                     width, height);

			parse_embed (child->children, window);
		}

		child = child->next;
	}

	xmlFreeDoc (doc);

	g_free (save_to);
}

const GList *
session_get_windows (Session *session)
{
	g_return_val_if_fail (IS_SESSION (session), NULL);

	return session->priv->windows;
}

/**
 * session_add_window:
 * @session: a #Session
 * @window: a #EphyWindow
 *
 * Add a window to the session. #EphyWindow take care of adding
 * itself to session.
 **/
void
session_add_window (Session *session,
		    EphyWindow *window)
{
	session->priv->windows = g_list_append (session->priv->windows, window);

	g_signal_emit (G_OBJECT (session),
		       session_signals[NEW_WINDOW],
		       0, window);
}

/**
 * session_remove_window:
 * @session: a #Session
 * @window: a #EphyWindow
 *
 * Remove a window from the session. #EphyWindow take care of removing
 * itself to session.
 **/
void
session_remove_window (Session *session,
		       EphyWindow *window)
{
	g_signal_emit (G_OBJECT (session),
		       session_signals[CLOSE_WINDOW],
		       0);

	session->priv->windows = g_list_remove (session->priv->windows, window);

	/* autodestroy of the session, necessay to avoid
	 * conflicts with the nautilus view */
	if (session->priv->windows == NULL)
	{
		g_object_unref (session);
	}
}

