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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "downloader-view.h"
#include "ephy-gui.h"
#include "ephy-ellipsizing-label.h"
#include "ephy-file-helpers.h"
#include "ephy-embed-shell.h"
#include "ephy-cell-renderer-progress.h"
#include "ephy-stock-icons.h"

#include <libgnomevfs/gnome-vfs-utils.h>
#include <eggstatusicon.h>
#include <eggtraymanager.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeviewcolumn.h>
#include <glib/gi18n.h>

#define CONF_DOWNLOADING_SHOW_DETAILS "/apps/epiphany/dialogs/downloader_show_details"

enum
{
	COL_PERCENT,
	COL_FILE,
	COL_REMAINING,
	COL_DOWNLOAD_OBJECT
};

#define EPHY_DOWNLOADER_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_DOWNLOADER_VIEW, DownloaderViewPrivate))

struct DownloaderViewPrivate
{
	GtkTreeModel *model;
	GHashTable *downloads_hash;

	/* Widgets */
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *pause_button;
	GtkWidget *abort_button;

	EggStatusIcon *status_icon;

	long remaining_secs;
};

typedef struct
{
	gboolean is_paused;
	gboolean can_abort;
	DownloaderViewPrivate *priv;
} ControlsInfo;

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

static void
downloader_view_build_ui (DownloaderView *dv);
static void
downloader_view_class_init (DownloaderViewClass *klass);
static void
downloader_view_finalize (GObject *object);
static void
downloader_view_init (DownloaderView *dv);

/* Callbacks */
void
download_dialog_pause_cb (GtkButton *button, DownloaderView *dv);
void
download_dialog_abort_cb (GtkButton *button, DownloaderView *dv);
gboolean
download_dialog_delete_cb (GtkWidget *window, GdkEventAny *event,
			   DownloaderView *dv);

static GObjectClass *parent_class = NULL;

GType
downloader_view_get_type (void)
{
       static GType downloader_view_type = 0;

        if (downloader_view_type == 0)
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

                downloader_view_type = g_type_register_static (EPHY_TYPE_DIALOG,
                                                               "DownloaderView",
                                                               &our_info, 0);
        }

        return downloader_view_type;
}

static void
downloader_view_class_init (DownloaderViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = downloader_view_finalize;

	g_type_class_add_private (object_class, sizeof(DownloaderViewPrivate));
}

static void
status_icon_activated (EggStatusIcon *icon, DownloaderView *dv)
{
	gtk_window_present (GTK_WINDOW (dv->priv->window));
}

static void
show_status_icon (DownloaderView *dv)
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_file (ephy_file ("epiphany-download.png"), NULL);
	dv->priv->status_icon = egg_status_icon_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);

	g_signal_connect (dv->priv->status_icon, "activate",
			  G_CALLBACK (status_icon_activated), dv);
}

static void
downloader_view_init (DownloaderView *dv)
{
        dv->priv = EPHY_DOWNLOADER_VIEW_GET_PRIVATE (dv);

	dv->priv->downloads_hash = g_hash_table_new_full
		(g_direct_hash, g_direct_equal, NULL,
		 (GDestroyNotify)gtk_tree_row_reference_free);

	downloader_view_build_ui (dv);

	show_status_icon (dv);

	g_object_ref (embed_shell);
}

static void
downloader_view_finalize (GObject *object)
{
	DownloaderView *dv = EPHY_DOWNLOADER_VIEW (object);

	g_object_unref (dv->priv->status_icon);
	g_hash_table_destroy (dv->priv->downloads_hash);
	g_object_unref (embed_shell);

        G_OBJECT_CLASS (parent_class)->finalize (object);
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
format_interval (long interval)
{
	int secs, hours, mins;                                                                                                                     
	secs = (int)(interval + .5);
	hours = secs / 3600;
	secs -= hours * 3600;
	mins = secs / 60;
	secs -= mins * 60;

	if (hours)
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
	gtk_button_set_label (GTK_BUTTON (dv->priv->pause_button), label_pause ? _("_Pause") : _("_Resume"));
}

static void
update_download_row (DownloaderView *dv, EphyDownload *download)
{
	GtkTreeRowReference *row_ref;
	GtkTreePath *path;
	GtkTreeIter iter;
	EphyDownloadState state;
	long total, current, remaining_secs;
	char *remaining, *file, *cur_progress, *name;
	struct tm;
	int percent;

	row_ref = get_row_from_download (dv, download);
	g_return_if_fail (row_ref != NULL);

	/* State special casing */
	state = ephy_download_get_state (download);
	switch (state)
	{
	case EPHY_DOWNLOAD_FAILED:
		percent = -2;
		remaining_secs = 0;	
		break;
	case EPHY_DOWNLOAD_COMPLETED:
		downloader_view_remove_download (dv, download);
		return;
	case EPHY_DOWNLOAD_DOWNLOADING:
	case EPHY_DOWNLOAD_PAUSED:	
		percent = ephy_download_get_percent (download);
		remaining_secs = ephy_download_get_remaining_time (download);
		break;
	default:
		percent = 0;
		remaining_secs = 0;
		break;
	}

	total = ephy_download_get_total_progress (download);
	current = ephy_download_get_current_progress (download);

	cur_progress = gnome_vfs_format_file_size_for_display (current);

	name = ephy_download_get_name (download);

	if (total != -1)
	{
		char *total_progress;

		total_progress = gnome_vfs_format_file_size_for_display (total);
		file = g_strdup_printf ("%s\n%s of %s", name,
					cur_progress, total_progress);
		g_free (total_progress);
	}
	else
	{
		file = g_strdup_printf ("%s\n%s", name, cur_progress);
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

static void
seconds_remaining_total (EphyDownload *download, gpointer data, DownloaderView *dv)
{
	long secs;

	secs = ephy_download_get_remaining_time (download);
	if (secs > 0)
	{
		dv->priv->remaining_secs += secs;
	}
}

static void
update_status_icon (DownloaderView *dv)
{
	char *tooltip, *downloadstring, *remainingstring;
	int downloads, remaining;

	dv->priv->remaining_secs = 0;
	g_hash_table_foreach (dv->priv->downloads_hash,
			      (GHFunc) seconds_remaining_total, dv);
	
	remaining = (dv->priv->remaining_secs);

	if (remaining < 60)
	{
		remainingstring = g_strdup_printf (ngettext ("About %d second left",
					"About %d seconds left", remaining),
					remaining);
	}
	else
	{
		remaining /= 60;
		
		remainingstring = g_strdup_printf (ngettext ("About %d minute left",
					"About %d minutes left", remaining),
					remaining);
	}

	downloads = g_hash_table_size (dv->priv->downloads_hash);

	downloadstring = g_strdup_printf (ngettext ("%d download", 
					"%d downloads", downloads), 
					downloads);

	tooltip = g_strdup_printf ("%s\n%s",
					downloadstring, remainingstring);

	egg_status_icon_set_tooltip (dv->priv->status_icon,
				     tooltip, NULL);

	g_free (tooltip);
	g_free (downloadstring);
	g_free (remainingstring);
}

static void
download_changed_cb (EphyDownload *download, DownloaderView *dv)
{
	update_download_row (dv, download);
	update_status_icon (dv);
}

void
downloader_view_add_download (DownloaderView *dv,
			      EphyDownload *download)
{
	GtkTreeRowReference *row_ref;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreePath *path;

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
	update_status_icon (dv);

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(dv->priv->treeview));
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	g_signal_connect_object (download, "changed",
				 G_CALLBACK (download_changed_cb), dv, 0);

	ephy_dialog_show (EPHY_DIALOG (dv));
}

static void
selection_changed (GtkTreeSelection *selection, DownloaderView *dv)
{
	update_buttons (dv);
}

static void
downloader_view_build_ui (DownloaderView *dv)
{
	DownloaderViewPrivate *priv = dv->priv;
	GtkListStore *liststore;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GdkPixbuf *icon;
	EphyDialog *d = EPHY_DIALOG (dv);
	GtkTreeSelection *selection;

	ephy_dialog_construct (d,
                               properties,
			       ephy_file ("epiphany.glade"),
                               "download_manager_dialog",
			       NULL);

	/* lookup needed widgets */
	priv->window = ephy_dialog_get_control(d, properties[PROP_WINDOW].id);
	priv->treeview = ephy_dialog_get_control (d, properties[PROP_TREEVIEW].id);
	priv->pause_button = ephy_dialog_get_control (d, properties[PROP_PAUSE_BUTTON].id);
	priv->abort_button = ephy_dialog_get_control (d, properties[PROP_ABORT_BUTTON].id);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview)),
				     GTK_SELECTION_BROWSE);

	liststore = gtk_list_store_new (4,
					G_TYPE_INT,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_OBJECT);

	gtk_tree_view_set_model (GTK_TREE_VIEW(priv->treeview),
				 GTK_TREE_MODEL (liststore));
	g_object_unref (liststore);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(priv->treeview),
					   TRUE);

	renderer = ephy_cell_renderer_progress_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     0, _("%"),
						     renderer,
						     "value", 0,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview), 0);
        gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_PERCENT);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     COL_FILE, _("File"),
						     renderer,
						     "text", COL_FILE,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   COL_FILE);
        gtk_tree_view_column_set_expand (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_FILE);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 0.5, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     COL_REMAINING, _("Remaining"),
						     renderer,
						     "text", COL_REMAINING,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   COL_REMAINING);
        gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_REMAINING);

	priv->model = GTK_TREE_MODEL (liststore);

	icon = gtk_widget_render_icon (GTK_WIDGET (priv->window),
				       EPHY_STOCK_DOWNLOAD,
				       GTK_ICON_SIZE_MENU,
				       NULL);
	gtk_window_set_icon (GTK_WINDOW(priv->window), icon);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
	g_signal_connect (selection, "changed", G_CALLBACK (selection_changed), dv);
}

void
download_dialog_pause_cb (GtkButton *button, DownloaderView *dv)
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
	g_value_unset (&val);

	state = ephy_download_get_state (download);

	if (state == EPHY_DOWNLOAD_DOWNLOADING)
	{
		ephy_download_pause (download);
	}
	else if (state == EPHY_DOWNLOAD_PAUSED)
	{
		ephy_download_resume (download);
	}
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

	update_status_icon (dv);

	/* Close the dialog if there are no more downloads */

	if (!g_hash_table_size (dv->priv->downloads_hash))
		g_object_unref (G_OBJECT (dv));
}

void
download_dialog_abort_cb (GtkButton *button, DownloaderView *dv)
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
	g_value_unset (&val);
	g_return_if_fail (download != NULL);
	
	ephy_download_cancel ((EphyDownload*)download);
	downloader_view_remove_download (dv, download);
}

gboolean
download_dialog_delete_cb (GtkWidget *window, GdkEventAny *event,
			   DownloaderView *dv)
{
	if (egg_tray_manager_check_running (gdk_screen_get_default ()))
	{
		gtk_widget_hide (dv->priv->window);
	}

	return TRUE;
}
