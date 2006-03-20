/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Xan Lopez
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

#include "config.h" 

#include "downloader-view.h"
#include "ephy-file-helpers.h"
#include "ephy-object-helpers.h"
#include "ephy-embed-shell.h"
#include "ephy-stock-icons.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtkversion.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrendererprogress.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomeui/gnome-icon-lookup.h>

#if GTK_CHECK_VERSION(2,9,0)
#define HAVE_STATUS_ICON
#include <gtk/gtkstatusicon.h>
#endif

#define CONF_DOWNLOADING_SHOW_DETAILS "/apps/epiphany/dialogs/downloader_show_details"

enum
{
	COL_STATE,
	COL_PERCENT,
	COL_IMAGE,
	COL_FILE,
	COL_REMAINING,
	COL_DOWNLOAD_OBJECT
};

enum
{
	PROGRESS_COL_POS,
	FILE_COL_POS,
	REMAINING_COL_POS
};

#define EPHY_DOWNLOADER_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderViewPrivate))

struct _DownloaderViewPrivate
{
	GtkTreeModel *model;
	GHashTable *downloads_hash;

	/* Widgets */
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *pause_button;
	GtkWidget *abort_button;

#ifdef HAVE_STATUS_ICON
	GtkStatusIcon *status_icon;
#endif

	guint idle_unref : 1;
};

enum
{
	PROP_WINDOW,
	PROP_TREEVIEW,
	PROP_PAUSE_BUTTON,
	PROP_ABORT_BUTTON,
};

static const
EphyDialogProperty properties [] =
{
	{ "download_manager_dialog",	NULL, PT_NORMAL, 0 },
	{ "clist",			NULL, PT_NORMAL, 0 },
	{ "pause_button",		NULL, PT_NORMAL, 0 },
	{ "abort_button",		NULL, PT_NORMAL, 0 },

	{ NULL }
};

enum
{
	RESPONSE_PAUSE = 1,
	RESPONSE_STOP = 2
};

static void
downloader_view_build_ui (DownloaderView *dv);
static void
downloader_view_class_init (DownloaderViewClass *klass);
static void
downloader_view_finalize (GObject *object);
static void
downloader_view_init (DownloaderView *dv);
static void
download_dialog_response_cb (GtkWidget *dialog,
			     int response,
			     DownloaderView *dv);
static gboolean
download_dialog_delete_event_cb (GtkWidget *window,
				 GdkEventAny *event,
				 DownloaderView *dv);

static GObjectClass *parent_class = NULL;

GType
downloader_view_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (DownloaderViewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) downloader_view_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (DownloaderView),
			0,    /* n_preallocs */
			(GInstanceInitFunc) downloader_view_init
		};

		type = g_type_register_static (EPHY_TYPE_DIALOG,
					       "DownloaderView",
					       &our_info, 0);
	}

	return type;
}

static void
downloader_view_class_init (DownloaderViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = downloader_view_finalize;

	g_type_class_add_private (object_class, sizeof(DownloaderViewPrivate));
}

#ifdef HAVE_STATUS_ICON
static void
show_downloader_cb (DownloaderView *dv)
{
	gtk_window_present (GTK_WINDOW (dv->priv->window));
}

static gboolean
status_icon_popup_menu_cb (GtkStatusIcon *icon,
			   guint button,
			   guint time,
                           DownloaderView *dv)
{
        GtkWidget *menu, *item;

	menu = gtk_menu_new ();

	/* this opens the downloader window, or brings it to the foreground if already open */
	item = gtk_menu_item_new_with_mnemonic (_("_Show Downloads"));
	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (show_downloader_cb), dv);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show_all (menu);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			NULL /* FIXME! see bug #334573 */, NULL,
			button, time);

	return TRUE;
}

static void
show_status_icon (DownloaderView *dv)
{
	DownloaderViewPrivate *priv = dv->priv;

	priv->status_icon = gtk_status_icon_new_from_stock (EPHY_STOCK_DOWNLOAD);
	g_object_ref_sink (priv->status_icon);

	g_signal_connect_swapped (priv->status_icon, "activate",
				  G_CALLBACK (show_downloader_cb), dv);
	g_signal_connect (dv->priv->status_icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb), dv);
}
#endif /* HAVE_STATUS_ICON */

static gboolean
remove_download (EphyDownload *download,
		 gpointer rowref,
		 DownloaderView *view)
{
	g_signal_handlers_disconnect_matched
		(download, G_SIGNAL_MATCH_DATA ,
		 0, 0, NULL, NULL, view);
	ephy_download_cancel (download);

	g_object_unref (download);
	return TRUE;
}

static void
prepare_close_cb (EphyEmbedShell *shell,
		  DownloaderView *view)
{
	DownloaderViewPrivate *priv = view->priv;

	/* the downloader owns a ref to itself, no need for another ref */

	/* hide window already */
	gtk_widget_hide (priv->window);

	priv->idle_unref = FALSE;

	/* cancel pending downloads */
	g_hash_table_foreach_remove (priv->downloads_hash,
				     (GHRFunc) remove_download, view);

	gtk_list_store_clear (GTK_LIST_STORE (priv->model));

	/* drop the self reference */
	g_object_unref (view);
}

static void
downloader_view_init (DownloaderView *dv)
{
	g_object_ref (embed_shell);

	dv->priv = EPHY_DOWNLOADER_VIEW_GET_PRIVATE (dv);

	dv->priv->downloads_hash = g_hash_table_new_full
		(g_direct_hash, g_direct_equal, NULL,
		 (GDestroyNotify)gtk_tree_row_reference_free);
	dv->priv->idle_unref = TRUE;

	downloader_view_build_ui (dv);

#ifdef HAVE_STATUS_ICON
	show_status_icon (dv);
#endif

	g_signal_connect_object (embed_shell, "prepare_close",
				 G_CALLBACK (prepare_close_cb), dv, 0);
}

static void
downloader_view_finalize (GObject *object)
{
	DownloaderView *dv = EPHY_DOWNLOADER_VIEW (object);
	gboolean idle_unref = dv->priv->idle_unref;

#ifdef HAVE_STATUS_ICON
	g_object_unref (dv->priv->status_icon);
	g_hash_table_destroy (dv->priv->downloads_hash);
#endif

	G_OBJECT_CLASS (parent_class)->finalize (object);

	if (idle_unref)
	{
		ephy_object_idle_unref (embed_shell);
	}
	else
	{
		g_object_unref (embed_shell);
	}
}

DownloaderView *
downloader_view_new (void)
{
	return EPHY_DOWNLOADER_VIEW (g_object_new (EPHY_TYPE_DOWNLOADER_VIEW,
			 			   "persist-position", TRUE,
						   "default-width", 420,
						   "default-height", 250,
			 			   NULL));
}

static char *
format_interval (gint64 interval)
{
	int hours, mins, secs;

	hours = (int) interval / 3600;
	interval -= hours * 3600;
	mins = (int) interval / 60;
	interval -= mins * 60;
	secs = (int) interval;

	if (hours > 0)
	{
		return g_strdup_printf (_("%u:%02u.%02u"), hours, mins, secs);
	}
	else
	{
		return g_strdup_printf (_("%02u.%02u"), mins, secs);
	}
}

static GtkTreeRowReference *
get_row_from_download (DownloaderView *dv, EphyDownload *download)
{
	return  g_hash_table_lookup (dv->priv->downloads_hash, download);
}

static void
update_buttons (DownloaderView *dv)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue val = {0, };
	EphyDownload *download;
	EphyDownloadState state;
	gboolean pause_enabled = FALSE;
	gboolean abort_enabled = FALSE;
	gboolean label_pause = TRUE;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(dv->priv->treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		gtk_tree_model_get_value (model, &iter, COL_DOWNLOAD_OBJECT, &val);
		download = g_value_get_object (&val);
		g_value_unset (&val);

		state = ephy_download_get_state (download);

		switch (state)
		{
		case EPHY_DOWNLOAD_DOWNLOADING:
			pause_enabled = TRUE;
			abort_enabled = TRUE;
			break;
		case EPHY_DOWNLOAD_PAUSED:
			pause_enabled = TRUE;
			abort_enabled = TRUE;
			label_pause = FALSE;
			break;
		default:
			abort_enabled = TRUE;
			break;
		}
	}
	gtk_widget_set_sensitive (dv->priv->pause_button, pause_enabled);
	gtk_widget_set_sensitive (dv->priv->abort_button, abort_enabled);
	gtk_button_set_label (GTK_BUTTON (dv->priv->pause_button),
			      label_pause ? _("_Pause") : _("_Resume"));
}

static void
update_download_row (DownloaderView *dv, EphyDownload *download)
{
	GtkTreeRowReference *row_ref;
	GtkTreePath *path;
	GtkTreeIter iter;
	EphyDownloadState state;
	gint64 remaining_secs = 0, total, current;
	char *remaining, *file, *cur_progress, *name;
	struct tm;
	int percent = 0;

	row_ref = get_row_from_download (dv, download);
	g_return_if_fail (row_ref != NULL);

	/* State special casing */
	state = ephy_download_get_state (download);
	switch (state)
	{
	case EPHY_DOWNLOAD_COMPLETED:
		downloader_view_remove_download (dv, download);
		return;
	case EPHY_DOWNLOAD_PAUSED:
	case EPHY_DOWNLOAD_DOWNLOADING:
		percent = ephy_download_get_percent (download);
		remaining_secs = ephy_download_get_remaining_time (download);
		break;
	default:
		break;
	}

	total = ephy_download_get_total_progress (download);
	current = ephy_download_get_current_progress (download);

	cur_progress = gnome_vfs_format_file_size_for_display (current);

	name = ephy_download_get_name (download);

	if (total != -1 && current != -1)
	{
		char *total_progress;

		total_progress = gnome_vfs_format_file_size_for_display (total);
		/* translators: first %s is filename, "%s of %s" is current/total file size */
		file = g_strdup_printf (_("%s\n%s of %s"), name,
					cur_progress, total_progress);
		g_free (total_progress);
	}
	else if (current != -1)
	{
		file = g_strdup_printf ("%s\n%s", name, cur_progress);
	}
	else
	{
		file = g_strdup_printf ("%s\n%s", name, _("Unknown"));
	}

	if (remaining_secs < 0)
	{
		remaining = g_strdup (_("Unknown"));
	}
	else
	{
		remaining = format_interval (remaining_secs);
	}

	path = gtk_tree_row_reference_get_path (row_ref);
	gtk_tree_model_get_iter (dv->priv->model, &iter, path);
	gtk_list_store_set (GTK_LIST_STORE (dv->priv->model),
			    &iter,
			    COL_STATE, state,
			    COL_PERCENT, percent,
			    COL_FILE, file,
			    COL_REMAINING, remaining,
			    -1);
	gtk_tree_path_free (path);

	g_free (name);
	g_free (cur_progress);
	g_free (file);
	g_free (remaining);

	update_buttons (dv);
}

#ifdef HAVE_STATUS_ICON
static void
update_status_icon (DownloaderView *dv)
{
	char *downloadstring;
	int downloads;

	downloads = g_hash_table_size (dv->priv->downloads_hash);

	downloadstring = g_strdup_printf (ngettext ("%d download", 
					"%d downloads", downloads), 
					downloads);

	gtk_status_icon_set_tooltip (dv->priv->status_icon,
				     downloadstring);
	g_free (downloadstring);
}
#endif

static void
download_changed_cb (EphyDownload *download, DownloaderView *dv)
{
	update_download_row (dv, download);
#ifdef HAVE_STATUS_ICON
	update_status_icon (dv);
#endif
}

void
downloader_view_add_download (DownloaderView *dv,
			      EphyDownload *download)
{
	GtkTreeRowReference *row_ref;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkIconTheme *theme;
	GtkIconInfo *icon_info;
	GdkPixbuf *pixbuf;
	char *mime, *icon_name;
	int width = 16, height = 16;

	g_object_ref (download);

	gtk_list_store_append (GTK_LIST_STORE (dv->priv->model),
			       &iter);
	gtk_list_store_set (GTK_LIST_STORE (dv->priv->model),
			    &iter, COL_DOWNLOAD_OBJECT, download, -1);

	path =  gtk_tree_model_get_path (GTK_TREE_MODEL (dv->priv->model), &iter);
	row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (dv->priv->model), path);
	gtk_tree_path_free (path);

	g_hash_table_insert (dv->priv->downloads_hash,
			     download,
			     row_ref);

	update_download_row (dv, download);
#ifdef HAVE_STATUS_ICON
	update_status_icon (dv);
#endif

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(dv->priv->treeview));
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	g_signal_connect_object (download, "changed",
				 G_CALLBACK (download_changed_cb), dv, 0);

	/* Show it already */
	ephy_dialog_show (EPHY_DIALOG (dv));
	
	mime =  ephy_download_get_mime (download);

	theme = gtk_icon_theme_get_default ();
	icon_name = gnome_icon_lookup (theme, NULL, NULL, NULL, NULL,
				       mime, GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
	g_free (mime);
	
	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (GTK_WIDGET (dv->priv->window)),
					   GTK_ICON_SIZE_MENU, &width, &height);
	width *= 2;

	icon_info = gtk_icon_theme_lookup_icon (theme, icon_name, width, 0);
	g_free (icon_name);
	if (icon_info == NULL) return;

	pixbuf = gdk_pixbuf_new_from_file_at_size
		(gtk_icon_info_get_filename (icon_info), width, width, NULL);
	gtk_icon_info_free (icon_info);

	gtk_list_store_set (GTK_LIST_STORE (dv->priv->model),
			    &iter, COL_IMAGE, pixbuf, -1);
	if (pixbuf != NULL)
	{
		g_object_unref (pixbuf);
	}

}

static void
selection_changed (GtkTreeSelection *selection, DownloaderView *dv)
{
	update_buttons (dv);
}

static void
progress_cell_data_func (GtkTreeViewColumn *col,
			 GtkCellRenderer   *renderer,
			 GtkTreeModel      *model,
			 GtkTreeIter       *iter,
			 gpointer           user_data)
{
	EphyDownloadState state;
	const char *text = NULL;
	int percent;

	gtk_tree_model_get (model, iter,
			    COL_STATE, &state,
			    COL_PERCENT, &percent,
			    -1);

	switch (state)
	{
		case EPHY_DOWNLOAD_INITIALISING:
			/* Translators: The text before the "|" is context to help you decide on
			 * the correct translation. You MUST OMIT it in the translated string. */
			text = Q_("download status|Unknown");
			break;
		case EPHY_DOWNLOAD_FAILED:
			/* Translators: The text before the "|" is context to help you decide on
			 * the correct translation. You MUST OMIT it in the translated string. */
			text = Q_("download status|Failed");
			break;
		case EPHY_DOWNLOAD_DOWNLOADING:
		case EPHY_DOWNLOAD_PAUSED:
			if (percent == -1)
			{
				/* Translators: The text before the "|" is context to help you decide on
				 * the correct translation. You MUST OMIT it in the translated string. */
				text = Q_("download status|Unknown");
				percent = 0;
			}
			break;
		default:
			g_return_if_reached ();
	}

	g_object_set (renderer, "text", text, "value", percent, NULL);
}

static void
downloader_view_build_ui (DownloaderView *dv)
{
	DownloaderViewPrivate *priv = dv->priv;
	GtkListStore *liststore;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	EphyDialog *d = EPHY_DIALOG (dv);
	GtkTreeSelection *selection;

	ephy_dialog_construct (d,
			       properties,
			       ephy_file ("epiphany.glade"),
			       "download_manager_dialog",
			       NULL);

	/* lookup needed widgets */
	ephy_dialog_get_controls
		(d,
		 properties[PROP_WINDOW].id, &priv->window,
		 properties[PROP_TREEVIEW].id, &priv->treeview,
		 properties[PROP_PAUSE_BUTTON].id, &priv->pause_button,
		 properties[PROP_ABORT_BUTTON].id, &priv->abort_button,
		 NULL);

	g_signal_connect (priv->window, "response",
			  G_CALLBACK (download_dialog_response_cb), dv);
	g_signal_connect (priv->window, "delete-event",
			  G_CALLBACK (download_dialog_delete_event_cb), dv);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview)),
				     GTK_SELECTION_BROWSE);

	liststore = gtk_list_store_new (6,
					G_TYPE_INT,
					G_TYPE_INT,
					GDK_TYPE_PIXBUF,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_OBJECT);

	gtk_tree_view_set_model (GTK_TREE_VIEW(priv->treeview),
				 GTK_TREE_MODEL (liststore));
	g_object_unref (liststore);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(priv->treeview),
					   TRUE);
	/* Icon and filename column*/
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("File"));
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "xpad", 3, NULL);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					    "pixbuf", COL_IMAGE,
					    NULL);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", COL_FILE,
					     NULL);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (priv->treeview), column, FILE_COL_POS);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_FILE);
	gtk_tree_view_column_set_spacing (column, 3);

	/* Progress column */
	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "xalign", 0.5, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     PROGRESS_COL_POS, _("%"),
						     renderer,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview), PROGRESS_COL_POS);
	gtk_tree_view_column_set_cell_data_func(column, renderer, progress_cell_data_func, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (column, COL_PERCENT);

	/* Remainng time column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 0.5, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     REMAINING_COL_POS, _("Remaining"),
						     renderer,
						     "text", COL_REMAINING,
						     NULL);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   REMAINING_COL_POS);
	gtk_tree_view_column_set_sort_column_id (column, COL_REMAINING);

	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->treeview), FALSE);

	priv->model = GTK_TREE_MODEL (liststore);

	gtk_window_set_icon_name (GTK_WINDOW (priv->window), EPHY_STOCK_DOWNLOAD);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	g_signal_connect (selection, "changed", G_CALLBACK (selection_changed), dv);
}

static void
download_dialog_pause (DownloaderView *dv)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue val = {0, };
	EphyDownload *download;
	EphyDownloadState state;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(dv->priv->treeview));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) return;

	gtk_tree_model_get_value (model, &iter, COL_DOWNLOAD_OBJECT, &val);
	download = g_value_get_object (&val);

	state = ephy_download_get_state (download);

	if (state == EPHY_DOWNLOAD_DOWNLOADING)
	{
		ephy_download_pause (download);
	}
	else if (state == EPHY_DOWNLOAD_PAUSED)
	{
		ephy_download_resume (download);
	}

	g_value_unset (&val);

	update_buttons (dv);
}

void
downloader_view_remove_download (DownloaderView *dv, EphyDownload *download)
{
	GtkTreeRowReference *row_ref;
	GtkTreePath *path = NULL;
	GtkTreeIter iter, iter2;

	row_ref = get_row_from_download (dv, download);
	g_return_if_fail (row_ref);

	/* Get the row we'll select after removal ("smart" selection) */
	
	path = gtk_tree_row_reference_get_path (row_ref);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (dv->priv->model),
				 &iter, path);
	gtk_tree_path_free (path);

	row_ref = NULL;
	iter2 = iter;			 
	if (gtk_tree_model_iter_next (GTK_TREE_MODEL (dv->priv->model), &iter))
	{
		path = gtk_tree_model_get_path  (GTK_TREE_MODEL (dv->priv->model), &iter);
		row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (dv->priv->model), path);
	}
	else
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (dv->priv->model), &iter2);
		if (gtk_tree_path_prev (path))
		{
			row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (dv->priv->model),
							      path);
		}
	}
	gtk_tree_path_free (path);

	/* Removal */

	gtk_list_store_remove (GTK_LIST_STORE (dv->priv->model), &iter2);
	g_hash_table_remove (dv->priv->downloads_hash,
			     download);
	g_object_unref (download);

	/* Actual selection */

	if (row_ref != NULL)
	{
		path = gtk_tree_row_reference_get_path (row_ref);
		if (path != NULL)
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (dv->priv->treeview),
						  path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		gtk_tree_row_reference_free (row_ref);
	}

#ifdef HAVE_STATUS_ICON
	update_status_icon (dv);
#endif

	/* Close the dialog if there are no more downloads */

	if (!g_hash_table_size (dv->priv->downloads_hash))
	{
		g_object_unref (dv);
	}
}

static void
download_dialog_stop (DownloaderView *dv)
{
	GValue val = {0, };
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gpointer download;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(dv->priv->treeview));
	
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) return;

	gtk_tree_model_get_value (model, &iter, COL_DOWNLOAD_OBJECT, &val);
	
	download = g_value_get_object (&val);
	g_return_if_fail (download != NULL);

	ephy_download_cancel ((EphyDownload*)download);
	downloader_view_remove_download (dv, download);

	g_value_unset (&val);
}

static void
download_dialog_response_cb (GtkWidget *dialog,
			     int response,
			     DownloaderView *dv)
{
	if (response == RESPONSE_PAUSE)
	{
		download_dialog_pause (dv);
	}
	else if (response == RESPONSE_STOP)
	{
		download_dialog_stop (dv);
	}
}

static gboolean
download_dialog_delete_event_cb (GtkWidget *window,
				 GdkEventAny *event,
				 DownloaderView *dv)
{
#ifdef HAVE_STATUS_ICON
	DownloaderViewPrivate *priv = dv->priv;

	if (gtk_status_icon_is_embedded (priv->status_icon))
	{
		gtk_widget_hide (window);
	}
#endif

	return TRUE;
}
