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

#include "history-dialog.h"
#include "ephy-shell.h"
#include "ephy-embed-shell.h"
#include "ephy-string.h"
#include "ephy-gui.h"
#include "ephy-dnd.h"
#include "ephy-node-filter.h"
#include "ephy-history-model.h"
#include "eggtreemodelfilter.h"
#include "eggtreemultidnd.h"
#include "ephy-tree-model-sort.h"
#include "toolbar.h"

#include <gtk/gtktreeview.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtkcellrenderertext.h>
#include <bonobo/bonobo-i18n.h>

#define CONF_HISTORY_SEARCH_TEXT "/apps/epiphany/history/search_text"
#define CONF_HISTORY_SEARCH_TIME "/apps/epiphany/history/search_time"

static void history_dialog_class_init (HistoryDialogClass *klass);
static void history_dialog_init (HistoryDialog *dialog);
static void history_dialog_finalize (GObject *object);
static void history_dialog_set_embedded (HistoryDialog *d,
				         gboolean embedded);


/* Glade callbacks */
void
history_host_checkbutton_toggled_cb (GtkWidget *widget,
				     HistoryDialog *dialog);
void
history_time_optionmenu_changed_cb (GtkWidget *widget,
			            HistoryDialog *dialog);
void
history_entry_changed_cb (GtkWidget *widget,
			  HistoryDialog *dialog);
void
history_go_button_clicked_cb (GtkWidget *button,
			      HistoryDialog *dialog);
void
history_ok_button_clicked_cb (GtkWidget *button,
			      HistoryDialog *dialog);
void
history_clear_button_clicked_cb (GtkWidget *button,
			         HistoryDialog *dialog);


static GObjectClass *parent_class = NULL;

struct HistoryDialogPrivate
{
	EphyHistory *gh;
	EphyNode *root;
	EphyNode *pages;
	EphyNodeFilter *filter;
	GtkTreeView *treeview;
	GtkTreeModel *model;
	EphyHistoryModel *nodemodel;
	GtkTreeModel *filtermodel;
	EphyTreeModelSort *sortmodel;
	GtkWidget *go_button;
	gboolean group;
	gboolean embedded;
};

enum
{
	PROP_0,
	PROP_EMBEDDED
};

enum
{
	PROP_TREEVIEW,
	PROP_WORD,
	PROP_TIME,
	PROP_GO_BUTTON	
};

static const
EphyDialogProperty properties [] =
{
	{ PROP_TREEVIEW, "history_treeview", NULL, PT_NORMAL, NULL },
	{ PROP_WORD, "history_entry", CONF_HISTORY_SEARCH_TEXT, PT_NORMAL, NULL },
	{ PROP_TIME, "history_time_optionmenu", CONF_HISTORY_SEARCH_TIME, PT_NORMAL, NULL },
	{ PROP_GO_BUTTON, "history_go_button", NULL, PT_NORMAL, NULL },
	{ -1, NULL, NULL }
};

GType
history_dialog_get_type (void)
{
        static GType history_dialog_type = 0;

        if (history_dialog_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (HistoryDialogClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) history_dialog_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (HistoryDialog),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) history_dialog_init
                };

                history_dialog_type = g_type_register_static (EPHY_EMBED_DIALOG_TYPE,
						              "HistoryDialog",
						              &our_info, 0);
        }

        return history_dialog_type;

}

static void
history_dialog_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
        HistoryDialog *d = HISTORY_DIALOG (object);

        switch (prop_id)
        {
                case PROP_EMBEDDED:
                        history_dialog_set_embedded
				(d, g_value_get_boolean (value));
                        break;
        }
}

static void
history_dialog_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
        HistoryDialog *d = HISTORY_DIALOG (object);

        switch (prop_id)
        {
                case PROP_EMBEDDED:
                        g_value_set_boolean (value, d->priv->embedded);
                        break;
        }
}


static void
history_dialog_class_init (HistoryDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = history_dialog_finalize;
	object_class->set_property = history_dialog_set_property;
	object_class->get_property = history_dialog_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EMBEDDED,
                                         g_param_spec_boolean ("embedded",
                                                               "Show embedded in another widget",
                                                               "Show embedded in another widget",
                                                               TRUE,
                                                               G_PARAM_READWRITE));
}

static void
add_column (HistoryDialog *dialog,
	    const char  *title,
            EphyHistoryModelColumn column)
{
	GtkTreeViewColumn *gcolumn;
	GtkCellRenderer *renderer;

	gcolumn = (GtkTreeViewColumn *) gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (gcolumn, renderer, TRUE);
	gtk_tree_view_column_set_attributes (gcolumn, renderer,
					     "text", column,
					     NULL);
	gtk_tree_view_column_set_sizing (gcolumn,
					 GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_sort_column_id (gcolumn, column);
	gtk_tree_view_column_set_title (gcolumn, title);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->priv->treeview),
				     gcolumn);
}

static void
history_view_selection_changed_cb (GtkTreeSelection *selection,
		HistoryDialog *dialog)
{
	if (gtk_tree_selection_count_selected_rows (selection))
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->priv->go_button), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->priv->go_button), FALSE);
}

static void
history_view_row_activated_cb (GtkTreeView *treeview,
			       GtkTreePath *path,
			       GtkTreeViewColumn *column,
			       HistoryDialog *dialog)
{
	GtkTreeIter iter, iter2;
	EphyNode *node;
	EphyEmbed *embed;
	const char *location;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->priv->sortmodel), &iter, path);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (dialog->priv->sortmodel), &iter2, &iter);
	egg_tree_model_filter_convert_iter_to_child_iter
		(EGG_TREE_MODEL_FILTER (dialog->priv->filtermodel), &iter, &iter2);

	node = ephy_history_model_node_from_iter (dialog->priv->nodemodel, &iter);
	location = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_LOCATION);
	g_return_if_fail (location != NULL);
	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	ephy_embed_load_url (embed, location);
}

static void
node_from_sort_iter_cb (EphyTreeModelSort *model,
		        GtkTreeIter *iter,
		        void **node,
		        HistoryDialog *dialog)
{
	GtkTreeIter filter_iter, node_iter;

	gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
							&filter_iter, iter);
	egg_tree_model_filter_convert_iter_to_child_iter (EGG_TREE_MODEL_FILTER (dialog->priv->filtermodel),
							  &node_iter, &filter_iter);
	*node = ephy_history_model_node_from_iter (EPHY_HISTORY_MODEL (dialog->priv->nodemodel), &node_iter);
	g_return_if_fail (*node != NULL);
}

static void
history_dialog_setup_view (HistoryDialog *dialog)
{
	dialog->priv->nodemodel = ephy_history_model_new (dialog->priv->root,
							  dialog->priv->pages,
			                                  dialog->priv->filter);
	dialog->priv->filtermodel = egg_tree_model_filter_new (GTK_TREE_MODEL (dialog->priv->nodemodel),
			                                       NULL);
	egg_tree_model_filter_set_visible_column (EGG_TREE_MODEL_FILTER (dialog->priv->filtermodel),
				                  EPHY_HISTORY_MODEL_COL_VISIBLE);
	dialog->priv->sortmodel = EPHY_TREE_MODEL_SORT (
			ephy_tree_model_sort_new (GTK_TREE_MODEL (dialog->priv->filtermodel)));
	g_signal_connect_object (G_OBJECT (dialog->priv->sortmodel),
				 "node_from_iter",
				 G_CALLBACK (node_from_sort_iter_cb),
				 dialog,
				 0);
	gtk_tree_view_set_model (dialog->priv->treeview,
				 GTK_TREE_MODEL (dialog->priv->sortmodel));

	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (dialog->priv->treeview));
	ephy_dnd_enable_model_drag_source (GTK_WIDGET (dialog->priv->treeview));

	add_column (dialog, _("Title"), EPHY_HISTORY_MODEL_COL_TITLE);
	add_column (dialog, _("Location"), EPHY_HISTORY_MODEL_COL_LOCATION);
	add_column (dialog, _("Last Visit"), EPHY_HISTORY_MODEL_COL_LAST_VISIT);

	g_signal_connect (dialog->priv->treeview,
			  "row_activated",
			  G_CALLBACK (history_view_row_activated_cb),
			  dialog);
	
	g_signal_connect (gtk_tree_view_get_selection (dialog->priv->treeview),
			"changed",
			G_CALLBACK (history_view_selection_changed_cb),
			dialog);
}

static GTime
get_date_filter (int filter_type,
		 GTime atime)
{
        GDate date, current_date;
	struct tm tm;

        g_date_clear (&current_date, 1);
        g_date_set_time (&current_date, time (NULL));

        g_date_clear (&date, 1);
        g_date_set_time (&date, atime);

        switch (filter_type)
        {
                /* Always */
        case 0:
                return 0;
                /* Today */
        case 1:
                break;
                /* Last two days */
        case 2:
                g_date_subtract_days (&current_date, 1);
                break;
                /* Last three days */
        case 3:
                g_date_subtract_days (&current_date, 2);
                break;
                /* Week */
        case 4:
                g_date_subtract_days (&current_date, 7);
                break;
                /* Two weeks */
        case 5:
                g_date_subtract_days (&current_date, 14);
                break;
        default:
                break;
        }

	g_date_to_struct_tm (&current_date, &tm);
        return mktime (&tm);
}

static void
history_dialog_setup_filter (HistoryDialog *dialog)
{
	GValue word = {0, };
	GValue atime = {0, };
	const char *search_text;
	GTime date_filter;

	ephy_dialog_get_value (EPHY_DIALOG(dialog), PROP_WORD, &word);
	search_text = g_value_get_string (&word);
	ephy_dialog_get_value (EPHY_DIALOG(dialog), PROP_TIME, &atime);
	date_filter = get_date_filter (g_value_get_int (&atime), time (NULL));

	GDK_THREADS_ENTER ();

	ephy_node_filter_empty (dialog->priv->filter);
	ephy_node_filter_add_expression (dialog->priv->filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
								          EPHY_NODE_PAGE_PROP_TITLE,
								          search_text),
				         0);
	ephy_node_filter_add_expression (dialog->priv->filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
								          EPHY_NODE_PAGE_PROP_LOCATION,
								          search_text),
				         0);
	ephy_node_filter_add_expression (dialog->priv->filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_INT_PROP_BIGGER_THAN,
								          EPHY_NODE_PAGE_PROP_LAST_VISIT,
								          date_filter),
				         1);

	ephy_node_filter_done_changing (dialog->priv->filter);

	GDK_THREADS_LEAVE ();
}

static void
history_dialog_init (HistoryDialog *dialog)
{
	EphyEmbedShell *ges;

	dialog->priv = g_new0 (HistoryDialogPrivate, 1);

	ges = EPHY_EMBED_SHELL (ephy_shell);
	dialog->priv->gh = ephy_embed_shell_get_global_history (ges);
	g_return_if_fail (dialog->priv->gh != NULL);

	dialog->priv->root = ephy_history_get_hosts (dialog->priv->gh);
	dialog->priv->pages = ephy_history_get_pages (dialog->priv->gh);
	dialog->priv->filter = ephy_node_filter_new ();
}

static void
history_dialog_set_embedded (HistoryDialog *dialog,
			     gboolean embedded)
{
	dialog->priv->embedded = embedded;

	ephy_dialog_construct (EPHY_DIALOG (dialog),
			       properties,
			       "epiphany.glade",
			       embedded ?
			       "history_dock_box" :
			       "history_dialog");
	dialog->priv->go_button = ephy_dialog_get_control (EPHY_DIALOG (dialog), PROP_GO_BUTTON);

	dialog->priv->treeview = GTK_TREE_VIEW (
				 ephy_dialog_get_control (EPHY_DIALOG(dialog),
							    PROP_TREEVIEW));
	history_dialog_setup_view (dialog);
	history_dialog_setup_filter (dialog);
}

static void
history_dialog_finalize (GObject *object)
{
	HistoryDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_HISTORY_DIALOG (object));

	dialog = HISTORY_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

	g_object_unref (G_OBJECT (dialog->priv->sortmodel));
	g_object_unref (G_OBJECT (dialog->priv->filtermodel));
	g_object_unref (G_OBJECT (dialog->priv->nodemodel));

        g_free (dialog->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyDialog *
history_dialog_new (EphyEmbed *embed,
		    gboolean embedded)
{
	HistoryDialog *dialog;

	dialog = HISTORY_DIALOG (g_object_new (HISTORY_DIALOG_TYPE,
					       "embedded", embedded,
					       "EphyEmbed", embed,
				               NULL));

	return EPHY_DIALOG(dialog);
}

EphyDialog *
history_dialog_new_with_parent (GtkWidget *window,
		                EphyEmbed *embed,
		                gboolean embedded)
{
	HistoryDialog *dialog;

	dialog = HISTORY_DIALOG (g_object_new (HISTORY_DIALOG_TYPE,
					       "embedded", embedded,
					       "EphyEmbed", embed,
					       "ParentWindow", window,
				               NULL));

	return EPHY_DIALOG(dialog);
}

void
history_entry_changed_cb (GtkWidget *widget,
			  HistoryDialog *dialog)
{
	if (dialog->priv->treeview == NULL) return;
	history_dialog_setup_filter (dialog);
}

void
history_time_optionmenu_changed_cb (GtkWidget *widget,
			            HistoryDialog *dialog)
{
	if (dialog->priv->treeview == NULL) return;
	history_dialog_setup_filter (dialog);
}

void
history_go_button_clicked_cb (GtkWidget *button,
		HistoryDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter, iter2;
	EphyNode *node;
	EphyEmbed *embed;
	const char *location;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview);
	g_return_if_fail (selection != NULL);
	gtk_tree_selection_get_selected (selection, NULL, &iter);
	gtk_tree_model_sort_convert_iter_to_child_iter
		(GTK_TREE_MODEL_SORT (dialog->priv->sortmodel), &iter2, &iter);
	egg_tree_model_filter_convert_iter_to_child_iter
		(EGG_TREE_MODEL_FILTER (dialog->priv->filtermodel), &iter, &iter2);

	node = ephy_history_model_node_from_iter (dialog->priv->nodemodel, &iter);
	location = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_LOCATION);
	g_return_if_fail (location != NULL);
	embed = ephy_embed_dialog_get_embed (EPHY_EMBED_DIALOG (dialog));
	ephy_embed_load_url (embed, location);
}

void
history_ok_button_clicked_cb (GtkWidget *button,
			      HistoryDialog *dialog)
{
	g_object_unref (G_OBJECT(dialog));
}

void
history_clear_button_clicked_cb (GtkWidget *button,
			         HistoryDialog *dialog)
{
	const GList *windows;
	Session *session;

	session = ephy_shell_get_session (ephy_shell);
	windows = session_get_windows (session);

	for (; windows != NULL; windows = windows->next)
	{
		Toolbar *t;

		t = ephy_window_get_toolbar (EPHY_WINDOW (windows->data));
		toolbar_clear_location_history (t);
	}

	ephy_history_clear (dialog->priv->gh);
}
