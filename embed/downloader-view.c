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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "downloader-view.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-ellipsizing-label.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-embed-shell.h"
#include "ephy-stock-icons.h"

#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeviewcolumn.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtkprogressbar.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

enum
{
	DOWNLOAD_REMOVE,
	DOWNLOAD_PAUSE,
	DOWNLOAD_RESUME,
        LAST_SIGNAL
};

enum
{
	COL_PERCENT,
	COL_FILENAME,
	COL_SIZE,
	COL_REMAINING,
	COL_PERSIST_OBJECT
};

struct DownloaderViewPrivate
{
	GHashTable *details_hash;
	GtkTreeModel *model;
	gboolean show_details;

	/* Widgets */
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *details_location;
	GtkWidget *details_status;
	GtkWidget *details_elapsed;
	GtkWidget *details_remaining;
	GtkWidget *details_progress;
	GtkWidget *details_button;

	GtkWidget *open_button;
	GtkWidget *pause_button;
	GtkWidget *abort_button;
};

typedef struct
{
	glong elapsed;
	glong remaining;
	gfloat speed;
	gint size_total;
	gint size_done;
	gfloat progress;
	gchar *filename;
	gchar *source;
	gchar *dest;
	DownloadStatus status;

	GtkTreeRowReference *ref;
} DownloadDetails;

typedef struct
{
	gboolean is_paused;
	gboolean can_abort;
	gboolean can_open;
	DownloaderViewPrivate *priv;
} ControlsInfo;

enum
{
	PROP_WINDOW,
	PROP_TREEVIEW,
	PROP_DETAILS_FRAME,
	PROP_DETAILS_TABLE,
	PROP_DETAILS_STATUS,
	PROP_DETAILS_ELAPSED,
	PROP_DETAILS_REMAINING,
	PROP_DETAILS_PROGRESS,
	PROP_OPEN_BUTTON,
	PROP_PAUSE_BUTTON,
	PROP_ABORT_BUTTON,
	PROP_DETAILS_BUTTON
};

static const
EphyDialogProperty properties [] =
{
	{ PROP_WINDOW, "download_manager_dialog", NULL, PT_NORMAL, NULL},
        { PROP_TREEVIEW, "clist", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_FRAME, "details_frame", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_TABLE, "details_table", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_STATUS, "details_status", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_ELAPSED, "details_elapsed", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_REMAINING, "details_remaining", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_PROGRESS, "details_progress", NULL, PT_NORMAL, NULL },
	{ PROP_OPEN_BUTTON, "open_button", NULL, PT_NORMAL, NULL },
	{ PROP_PAUSE_BUTTON, "pause_button", NULL, PT_NORMAL, NULL },
	{ PROP_ABORT_BUTTON, "abort_button", NULL, PT_NORMAL, NULL },
	{ PROP_DETAILS_BUTTON, "details_togglebutton", CONF_DOWNLOADING_SHOW_DETAILS, PT_NORMAL, NULL },

        { -1, NULL, NULL }
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
static void
downloader_treeview_selection_changed_cb (GtkTreeSelection *selection,
					  DownloaderView *dv);
gboolean
download_dialog_delete_cb (GtkWidget *window, GdkEventAny *event,
			   DownloaderView *dv);
void
download_dialog_details_cb (GtkToggleButton *button,
			    DownloaderView *dv);
void
download_dialog_open_cb (GtkWidget *button,
			 DownloaderView *dv);

static GObjectClass *parent_class = NULL;
static guint downloader_view_signals[LAST_SIGNAL] = { 0 };

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

                downloader_view_type = g_type_register_static (EPHY_DIALOG_TYPE,
                                                               "DownloaderView",
                                                               &our_info, 0);
        }

        return downloader_view_type;
}

static void
format_time (gchar *buffer, glong time)
{
	gint secs, hours, mins;

	secs = (gint)(time + .5);
	hours = secs / 3600;
	secs -= hours * 3600;
	mins = secs / 60;
	secs -= mins * 60;

	if (hours)
	{
		/* Hours, Minutes, Seconds */
		sprintf (buffer, _("%u:%02u.%02u"), hours, mins, secs);
	}
	else
	{
		/* Minutes, Seconds */
		sprintf (buffer, _("%02u.%02u"), mins, secs);
	}
}

static void
downloader_view_class_init (DownloaderViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EphyDialogClass *ephy_dialog_class;

        parent_class = g_type_class_peek_parent (klass);
        ephy_dialog_class = EPHY_DIALOG_CLASS (klass);

        object_class->finalize = downloader_view_finalize;

        /* init signals */
        downloader_view_signals[DOWNLOAD_REMOVE] =
                g_signal_new ("download_remove",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DownloaderViewClass, download_remove),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	downloader_view_signals[DOWNLOAD_PAUSE] =
                g_signal_new ("download_pause",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DownloaderViewClass, download_pause),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	downloader_view_signals[DOWNLOAD_RESUME] =
                g_signal_new ("download_resume",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (DownloaderViewClass, download_resume),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
destroy_details_cb (DownloadDetails *details)
{
	g_free (details->filename);
	g_free (details->source);
	g_free (details->dest);
	g_free (details);
}

static void
downloader_view_init (DownloaderView *dv)
{
        dv->priv = g_new0 (DownloaderViewPrivate, 1);
	dv->priv->details_hash = g_hash_table_new_full (g_direct_hash,
							g_direct_equal,
							NULL,
							(GDestroyNotify)destroy_details_cb);
	downloader_view_build_ui (dv);

	g_object_ref (embed_shell);
}

static void
downloader_view_finalize (GObject *object)
{
        DownloaderView *dv;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_DOWNLOADER_VIEW (object));

        dv = DOWNLOADER_VIEW (object);

        g_return_if_fail (dv->priv != NULL);

	g_hash_table_destroy (dv->priv->details_hash);

	g_object_unref (embed_shell);

	g_free (dv->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

DownloaderView *
downloader_view_new (void)
{
	return DOWNLOADER_VIEW (g_object_new
			(DOWNLOADER_VIEW_TYPE, NULL));
}

static void
controls_info_foreach (GtkTreeModel *model,
		       GtkTreePath  *path,
		       GtkTreeIter  *iter,
		       ControlsInfo *info)
{
	DownloadDetails *details;
	GValue val = {0, };
	gpointer persist_object;

	gtk_tree_model_get_value (model, iter, COL_PERSIST_OBJECT, &val);
	persist_object = g_value_get_pointer (&val);

	details = g_hash_table_lookup (info->priv->details_hash,
				       persist_object);

	info->is_paused |= (details->status == DOWNLOAD_STATUS_PAUSED);
	info->can_abort |= (details->status != DOWNLOAD_STATUS_COMPLETED);
	info->can_open |= (details->status == DOWNLOAD_STATUS_COMPLETED);
}

static void
downloader_view_update_controls (DownloaderViewPrivate *priv)
{
	GtkTreeSelection *selection;
	ControlsInfo *info;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(priv->treeview));

	info = g_new0 (ControlsInfo, 1);
	info->priv = priv;

	/* initial conditions */
	info->is_paused = info->can_abort = info->can_open = FALSE;

	if (selection)
	{
		gtk_tree_selection_selected_foreach
			(selection,
			 (GtkTreeSelectionForeachFunc)controls_info_foreach,
			 info);
	}

	/* setup buttons */
	gtk_widget_set_sensitive (priv->open_button, info->can_open);
	gtk_widget_set_sensitive (priv->pause_button, info->can_abort);
	/* As long as we can abort, we can pause/resume */
	gtk_widget_set_sensitive (priv->abort_button, info->can_abort);

	if (info->is_paused)
	{
		gtk_button_set_label (GTK_BUTTON (priv->pause_button), _("_Resume"));
	}
	else
	{
		gtk_button_set_label (GTK_BUTTON (priv->pause_button), _("_Pause"));
	}
	
	g_free (info);
}

static void
downloader_view_update_details (DownloaderViewPrivate *priv,
				DownloadDetails *details)
{
	gchar buffer[50];

	ephy_ellipsizing_label_set_text
		(EPHY_ELLIPSIZING_LABEL (priv->details_location),
		 details->source);

	if (details->size_total >= 10000)
	{
		sprintf (buffer, _("%.1f of %.1f MB"),
			 details->size_done / 1024.0,
			 details->size_total / 1024.0);
	}
	else if (details->size_total > 0)
	{
		sprintf (buffer, _("%d of %d KB"),
			 details->size_done,
			 details->size_total);
	}
	else
	{
		sprintf (buffer, _("%d KB"),
			 details->size_done);
	}

	if (details->speed > 0)
	{
		sprintf (buffer, "%s at %.1f KB/s", buffer, details->speed);
	}
	gtk_label_set_text (GTK_LABEL (priv->details_status),
			    buffer);

	format_time (buffer, details->elapsed/1000000);
	gtk_label_set_text (GTK_LABEL (priv->details_elapsed),
			    buffer);

	format_time (buffer, details->remaining);
	gtk_label_set_text (GTK_LABEL (priv->details_remaining),
			    buffer);

	if (details->progress >= 0)
	{
		gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR (priv->details_progress),
			 details->progress);
	}
	else
	{
		gtk_progress_bar_pulse
			(GTK_PROGRESS_BAR (priv->details_progress));
	}
}

static gboolean
get_selected_row (DownloaderViewPrivate *priv, GtkTreeIter *iter)
{
	GList *l;
	GtkTreePath *node;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(priv->treeview));
	l = gtk_tree_selection_get_selected_rows (selection, &model);

	if (l == NULL) return FALSE;

	node = l->data;
	gtk_tree_model_get_iter (model, iter, node);

	g_list_foreach (l, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (l);

	return TRUE;
}

static void
downloader_view_set_download_info (DownloaderViewPrivate *priv,
				   DownloadDetails *details,
				   GtkTreeIter *iter)
{
	gchar buffer[50];
	GtkTreePath *path;
	GtkTreePath *selected_path = NULL;
	GtkTreeIter selected_iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(priv->treeview));

	if (get_selected_row (priv, &selected_iter))
	{
		selected_path = gtk_tree_model_get_path
			(priv->model, &selected_iter);
	}

	path = gtk_tree_row_reference_get_path (details->ref);

	gtk_list_store_set (GTK_LIST_STORE (priv->model),
			    iter,
			    COL_FILENAME, details->filename,
			    -1);

	/* Progress */
	if (details->status == DOWNLOAD_STATUS_COMPLETED)
		details->progress = 1;
	sprintf (buffer, "%.1f%%",
		 details->progress > 0 ?
		 details->progress * 100.0 :
		 0);
	gtk_list_store_set (GTK_LIST_STORE (priv->model),
			    iter,
			    COL_PERCENT, buffer,
			    -1);

	/* Total */
	if (details->size_total >= 10000)
	{
		sprintf (buffer, "%.2f MB", details->size_total / 1024.0);
	}
	else if (details->size_total > 0)
	{
		sprintf (buffer, "%d KB", details->size_total);
	}
	else
	{
		sprintf (buffer, _("Unknown"));
	}

	gtk_list_store_set (GTK_LIST_STORE (priv->model),
			    iter,
			    COL_SIZE, buffer,
			    -1);

	/* Remaining */
	if (details->remaining >= 0)
	{
		format_time (buffer, details->remaining);
	}
	else
	{
		sprintf (buffer,
			 details->progress > 0 ?
		 	 "00.00" :
		 	 _("Unknown"));
	}
	
	gtk_list_store_set (GTK_LIST_STORE (priv->model),
			    iter,
			    COL_REMAINING, buffer,
			    -1);

	if (gtk_tree_path_compare (path, selected_path) == 0)
	{
		downloader_view_update_details (priv, details);
	}
}

static void
ensure_selected_row (DownloaderView *dv)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	g_return_if_fail (IS_DOWNLOADER_VIEW(dv));

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(dv->priv->treeview));
	if (get_selected_row (dv->priv, &iter))
	{
		/* there is already a selection */
		return;
	}

	if (gtk_tree_model_get_iter_first (dv->priv->model, &iter))
	{
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

void
downloader_view_add_download (DownloaderView *dv,
			      gchar *filename,
			      gchar *source,
			      gchar *dest,
			      gpointer persist_object)
{
	GtkTreeIter iter;
	DownloadDetails *details;
	GtkTreeSelection *selection;

	details = g_new0 (DownloadDetails, 1);
	details->filename = g_strdup (filename);
	details->source = g_strdup (source);
	details->dest = g_strdup (dest);
	details->elapsed = -1;
	details->remaining = -1;
	details->speed = -1;
	details->size_total = -1;
	details->size_done = -1;
	details->progress = -1;
	dv->priv->show_details = FALSE;

	g_hash_table_insert (dv->priv->details_hash,
			     persist_object,
			     details);

	gtk_list_store_append (GTK_LIST_STORE (dv->priv->model),
			       &iter);

	details->ref = gtk_tree_row_reference_new
		(GTK_TREE_MODEL (dv->priv->model),
		 gtk_tree_model_get_path
		 (GTK_TREE_MODEL (dv->priv->model), &iter));

	gtk_list_store_set (GTK_LIST_STORE (dv->priv->model),
			    &iter,
			    COL_PERSIST_OBJECT, persist_object,
			    -1);

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(dv->priv->treeview));
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	downloader_view_set_download_info (dv->priv, details, &iter);

	ephy_dialog_show (EPHY_DIALOG (dv));
}

void
downloader_view_remove_download (DownloaderView *dv,
				 gpointer persist_object)
{
	DownloadDetails *details;
	GtkTreeIter iter;

	details = g_hash_table_lookup (dv->priv->details_hash,
				       persist_object);
	g_return_if_fail (details);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (dv->priv->model),
				 &iter,
				 gtk_tree_row_reference_get_path (details->ref));

	gtk_list_store_remove (GTK_LIST_STORE (dv->priv->model), &iter);

	g_hash_table_remove (dv->priv->details_hash,
			     persist_object);

	ensure_selected_row (dv);
}

void
downloader_view_set_download_progress (DownloaderView *dv,
				       glong elapsed,
				       glong remaining,
				       gfloat speed,
				       gint size_total,
				       gint size_done,
				       gfloat progress,
				       gpointer persist_object)
{
	DownloadDetails *details;
	GtkTreeIter iter;

	details = g_hash_table_lookup (dv->priv->details_hash,
				       persist_object);
	g_return_if_fail (details);

	details->elapsed = elapsed;
	details->remaining = remaining;
	details->speed = speed;
	details->size_total = size_total;
	details->size_done = size_done;
	details->progress = progress;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (dv->priv->model),
				 &iter,
				 gtk_tree_row_reference_get_path (details->ref));

	downloader_view_set_download_info (dv->priv, details, &iter);
}

void
downloader_view_set_download_status (DownloaderView *dv,
				     DownloadStatus status,
				     gpointer persist_object)
{
	DownloadDetails *details;
	GtkTreeIter iter;

	details = g_hash_table_lookup (dv->priv->details_hash,
				       persist_object);
	g_return_if_fail (details);

	details->status = status;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (dv->priv->model),
				 &iter,
				 gtk_tree_row_reference_get_path (details->ref));

	downloader_view_set_download_info (dv->priv, details, &iter);
	downloader_view_update_controls (dv->priv);

/*	if (status == DOWNLOAD_STATUS_COMPLETED)
	{
		downloader_view_remove_download (dv, persist_object);
	}*/
}

static void
downloader_view_build_ui (DownloaderView *dv)
{
	DownloaderViewPrivate *priv = dv->priv;
	GtkListStore *liststore;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkWidget *details_table;
	GdkPixbuf *icon;
	EphyDialog *d = EPHY_DIALOG (dv);

	ephy_dialog_construct (d,
                               properties,
                               "epiphany.glade",
                               "download_manager_dialog");

	/* lookup needed widgets */
	priv->window = ephy_dialog_get_control(d, PROP_WINDOW);
	priv->treeview = ephy_dialog_get_control (d, PROP_TREEVIEW);
	priv->details_status = ephy_dialog_get_control (d, PROP_DETAILS_STATUS);
	priv->details_elapsed = ephy_dialog_get_control (d, PROP_DETAILS_ELAPSED);
	priv->details_remaining = ephy_dialog_get_control (d, PROP_DETAILS_REMAINING);
	priv->details_progress = ephy_dialog_get_control (d, PROP_DETAILS_PROGRESS);
	priv->details_button = ephy_dialog_get_control (d, PROP_DETAILS_BUTTON);
	priv->open_button = ephy_dialog_get_control (d, PROP_OPEN_BUTTON);
	priv->pause_button = ephy_dialog_get_control (d, PROP_PAUSE_BUTTON);
	priv->abort_button = ephy_dialog_get_control (d, PROP_ABORT_BUTTON);
	details_table = ephy_dialog_get_control (d, PROP_DETAILS_TABLE);

	/* create file and location details labels */
	priv->details_location = ephy_ellipsizing_label_new ("");
	gtk_table_attach_defaults (GTK_TABLE(details_table),
				   priv->details_location,
				   1, 2, 1, 2);
	gtk_misc_set_alignment (GTK_MISC(priv->details_location), 0, 0);
	gtk_label_set_selectable (GTK_LABEL(priv->details_location), TRUE);
	gtk_widget_show (priv->details_location);

	liststore = gtk_list_store_new (5,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_POINTER);

	gtk_tree_view_set_model (GTK_TREE_VIEW(priv->treeview),
				 GTK_TREE_MODEL (liststore));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(priv->treeview),
					   TRUE);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     0, _("%"),
						     renderer,
						     "text", 0,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview), 0);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_PERCENT);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     COL_FILENAME, _("Filename"),
						     renderer,
						     "text", COL_FILENAME,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   COL_FILENAME);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_FILENAME);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     COL_SIZE, _("Size"),
						     renderer,
						     "text", COL_SIZE,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   COL_SIZE);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_SIZE);

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(priv->treeview),
						     COL_REMAINING, _("Remaining"),
						     renderer,
						     "text", COL_REMAINING,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(priv->treeview),
					   COL_REMAINING);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_column_set_reorderable (column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COL_REMAINING);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(priv->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        g_signal_connect (G_OBJECT (selection), "changed",
                          G_CALLBACK (downloader_treeview_selection_changed_cb), dv);

	priv->model = GTK_TREE_MODEL (liststore);

	icon = gtk_widget_render_icon (GTK_WIDGET (priv->window),
				       EPHY_STOCK_DOWNLOAD,
				       GTK_ICON_SIZE_MENU,
				       NULL);
	gtk_window_set_icon (GTK_WINDOW(priv->window), icon);
}

void
download_dialog_pause_cb (GtkButton *button, DownloaderView *dv)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	DownloadDetails *details;
	GValue val = {0, };
	gpointer *persist_object;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(dv->priv->treeview));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) return;

	gtk_tree_model_get_value (model, &iter, COL_PERSIST_OBJECT, &val);
	persist_object = g_value_get_pointer (&val);

	details = g_hash_table_lookup (dv->priv->details_hash,
				       persist_object);
	g_return_if_fail (details);

	if (details->status == DOWNLOAD_STATUS_COMPLETED) 
	{
		return;
	}
	else if (details->status == DOWNLOAD_STATUS_DOWNLOADING ||
		 details->status == DOWNLOAD_STATUS_RESUMING)
	{
		g_signal_emit (G_OBJECT (dv), downloader_view_signals[DOWNLOAD_PAUSE], 0, persist_object);
		downloader_view_set_download_status (dv, DOWNLOAD_STATUS_PAUSED, persist_object);
	}
	else if (details->status == DOWNLOAD_STATUS_PAUSED)
	{
		g_signal_emit (G_OBJECT (dv), downloader_view_signals[DOWNLOAD_RESUME], 0, persist_object);
		downloader_view_set_download_status (dv, DOWNLOAD_STATUS_RESUMING, persist_object);
	}
}

void
download_dialog_abort_cb (GtkButton *button, DownloaderView *dv)
{
	GList *l, *r = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(dv->priv->treeview));
	l = gtk_tree_selection_get_selected_rows (selection, &model);
	for (;l != NULL; l = l->next)
	{
		r = g_list_append (r, gtk_tree_row_reference_new
				   (model, (GtkTreePath *)l->data));
	}

        for (; r != NULL; r = g_list_next (r))
        {
                GtkTreeRowReference *node = r->data;
		GValue val = {0, };
		gpointer *persist_object;
		GtkTreeIter iter;
		DownloadDetails *details;

                gtk_tree_model_get_iter (model, &iter,
                                         gtk_tree_row_reference_get_path (node));

		gtk_tree_model_get_value (model, &iter,
					  COL_PERSIST_OBJECT, &val);
		persist_object = g_value_get_pointer (&val);

		details = g_hash_table_lookup (dv->priv->details_hash,
					       persist_object);
		g_return_if_fail (details);

		g_signal_emit (G_OBJECT (dv), downloader_view_signals[DOWNLOAD_REMOVE], 0, persist_object);

		downloader_view_remove_download (dv, persist_object);

                gtk_tree_row_reference_free (node);
        }

	l = g_list_first (l);
	g_list_foreach (l, (GFunc)gtk_tree_path_free, NULL);
        g_list_free (l);
	g_list_free (r);
}

static void
downloader_treeview_selection_changed_cb (GtkTreeSelection *selection,
					  DownloaderView *dv)
{
	GtkTreeIter iter;
	GValue val = {0, };
	gpointer *persist_object;
	DownloadDetails *details = NULL;
	GtkWidget *details_button;
	GtkWidget *details_frame;
	DownloaderViewPrivate *priv= dv->priv;

	details_button = ephy_dialog_get_control (EPHY_DIALOG(dv),
						  PROP_DETAILS_BUTTON);
	details_frame = ephy_dialog_get_control (EPHY_DIALOG(dv),
						 PROP_DETAILS_FRAME);

	if (get_selected_row (priv, &iter))
	{
		gtk_tree_model_get_value (priv->model, &iter, COL_PERSIST_OBJECT, &val);
		persist_object = g_value_get_pointer (&val);

		details = g_hash_table_lookup (priv->details_hash,
					       persist_object);
		g_return_if_fail (details);

		gtk_widget_set_sensitive (details_button, TRUE);
		gtk_widget_set_sensitive (details_frame, TRUE);

		downloader_view_update_details (priv, details);
		downloader_view_update_controls (priv);
	}
	else
	{
		gtk_label_set_text (GTK_LABEL (priv->details_location), "");
		gtk_label_set_text (GTK_LABEL (priv->details_status), "");
		gtk_label_set_text (GTK_LABEL (priv->details_elapsed), "");
		gtk_label_set_text (GTK_LABEL (priv->details_remaining), "");
		gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR (priv->details_progress),
			 0);

		gtk_widget_set_sensitive (details_frame, FALSE);
		if (!gtk_tree_model_get_iter_first (priv->model, &iter))
			gtk_widget_set_sensitive (details_button, FALSE);
	}
}

static void
alive_download_foreach (gpointer persist_object,
			DownloadDetails *details,
			gboolean *alive)
{
	if (details->status != DOWNLOAD_STATUS_COMPLETED)
	{
		*alive = TRUE;
	}
}

static gboolean
delete_pending_foreach  (gpointer persist_object,
			 DownloadDetails *details,
			 DownloaderView *dv)
{
	g_signal_emit (G_OBJECT (dv), downloader_view_signals[DOWNLOAD_REMOVE],
		       0, persist_object);

	return TRUE;
}

gboolean
download_dialog_delete_cb (GtkWidget *window, GdkEventAny *event,
			   DownloaderView *dv)
{
	GtkWidget *dialog;
	gboolean choice;
	gboolean alive_download = FALSE;

	g_hash_table_foreach (dv->priv->details_hash,
			      (GHFunc)alive_download_foreach,
			      &alive_download);

	if (!alive_download) return FALSE;

	/* build question dialog */
	dialog = gtk_message_dialog_new (
		 GTK_WINDOW (window),
		 GTK_DIALOG_MODAL,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_YES_NO,
		 _("Cancel all pending downloads?"));

	/* run it */
	choice = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* do the appropriate thing */
	if (choice == GTK_RESPONSE_YES)
	{
		g_hash_table_foreach_remove (dv->priv->details_hash,
				             (GHRFunc)delete_pending_foreach,
				             dv);
		return FALSE;
	}

	return TRUE;
}

void
download_dialog_details_cb (GtkToggleButton *button,
			    DownloaderView *dv)
{
	GtkWidget *details_frame;

	details_frame = ephy_dialog_get_control (EPHY_DIALOG(dv),
						   PROP_DETAILS_FRAME);
	if (gtk_toggle_button_get_active (button))
	{
		gtk_widget_show (GTK_WIDGET (details_frame));
		dv->priv->show_details = TRUE;
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET (details_frame));
		dv->priv->show_details = FALSE;
	}

}

static void
open_selection_foreach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
			DownloaderView *dv)
{
	DownloadDetails *details;
	GValue val = {0, };
	gpointer *persist_object;
	GnomeVFSMimeApplication *app;
	char *mime;

	gtk_tree_model_get_value (model, iter, COL_PERSIST_OBJECT, &val);
	persist_object = g_value_get_pointer (&val);

	details = g_hash_table_lookup (dv->priv->details_hash,
				       persist_object);
	g_return_if_fail (details);

	if (details->status != DOWNLOAD_STATUS_COMPLETED) return;

	mime = gnome_vfs_get_mime_type (details->dest);
	g_return_if_fail (mime != NULL);

	app = gnome_vfs_mime_get_default_application (mime);
	if (app)
	{
		ephy_file_launch_application (app->command,
					      details->dest,
					      app->requires_terminal);
	}
	else
	{
		GtkWidget *parent;
		parent = gtk_widget_get_toplevel (dv->priv->open_button);
		ephy_embed_utils_nohandler_dialog_run (parent);
	}

	g_free(mime);
}

void
download_dialog_open_cb (GtkWidget *button,
			 DownloaderView *dv)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(dv->priv->treeview));

	gtk_tree_selection_selected_foreach
		(selection,
		 (GtkTreeSelectionForeachFunc)open_selection_foreach,
		 (gpointer)dv);
}
