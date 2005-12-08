/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "pdm-dialog.h"
#include "ephy-shell.h"
#include "ephy-cookie-manager.h"
#include "ephy-file-helpers.h"
#include "ephy-password-manager.h"
#include "ephy-gui.h"
#include "ephy-state.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtktogglebutton.h>
#include <glib/gi18n.h>

#include <time.h>
#include <string.h>

typedef struct PdmActionInfo PdmActionInfo;
	
struct PdmActionInfo
{
	/* Methods */
	void (* construct)	(PdmActionInfo *info);
	void (* destruct)	(PdmActionInfo *info);
	void (* fill)		(PdmActionInfo *info);
	void (* add)		(PdmActionInfo *info,
				 gpointer data);
	void (* remove)		(PdmActionInfo *info,
				 gpointer data);
	void (* scroll_to)	(PdmActionInfo *info);

	/* Data */
	PdmDialog *dialog;
	GtkTreeView *treeview;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	int remove_id;
	int data_col;
	char *scroll_to_host;
	gboolean filled;
	gboolean delete_row_on_remove;
};

#define EPHY_PDM_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_PDM_DIALOG, PdmDialogPrivate))

struct PdmDialogPrivate
{
	GtkWidget *notebook;
	GtkTreeModel *model;
	PdmActionInfo *cookies;
	PdmActionInfo *passwords;
};

enum
{
	COL_COOKIES_HOST,
	COL_COOKIES_HOST_KEY,
	COL_COOKIES_NAME,
	COL_COOKIES_DATA,
};

enum
{
	TV_COL_COOKIES_HOST,
	TV_COL_COOKIES_NAME
};

enum
{
	COL_PASSWORDS_HOST,
	COL_PASSWORDS_USER,
	COL_PASSWORDS_PASSWORD,
	COL_PASSWORDS_DATA
};

enum
{
	PROP_WINDOW,
	PROP_NOTEBOOK,
	PROP_COOKIES_TREEVIEW,
	PROP_COOKIES_REMOVE,
	PROP_COOKIES_PROPERTIES,
	PROP_PASSWORDS_TREEVIEW,
	PROP_PASSWORDS_REMOVE,
	PROP_PASSWORDS_SHOW
};

static const
EphyDialogProperty properties [] =
{
	{ "pdm_dialog",			NULL, PT_NORMAL, 0 },
	{ "pdm_notebook",		NULL, PT_NORMAL, 0 },

	{ "cookies_treeview",	   	NULL, PT_NORMAL, 0 },
	{ "cookies_remove_button",     	NULL, PT_NORMAL, 0 },
	{ "cookies_properties_button", 	NULL, PT_NORMAL, 0 },
	{ "passwords_treeview",	       	NULL, PT_NORMAL, 0 },
	{ "passwords_remove_button",   	NULL, PT_NORMAL, 0 },
	{ "passwords_show_button",     	NULL, PT_NORMAL, 0 },

	{ NULL }
};

static void pdm_dialog_class_init	(PdmDialogClass *klass);
static void pdm_dialog_init		(PdmDialog *dialog);
static void pdm_dialog_finalize		(GObject *object);

static GObjectClass *parent_class = NULL;

GType 
pdm_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (PdmDialogClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) pdm_dialog_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (PdmDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) pdm_dialog_init
		};

		type = g_type_register_static (EPHY_TYPE_DIALOG,
					       "PdmDialog",
					       &our_info, 0);
	}

	return type;
}

static void
pdm_dialog_class_init (PdmDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = pdm_dialog_finalize;

	g_type_class_add_private (object_class, sizeof(PdmDialogPrivate));
}

static void
pdm_dialog_show_help (PdmDialog *pd)
{
	GtkWidget *notebook, *window;
	int id;

	static char * const help_preferences[] = {
		"managing-cookies",
		"managing-passwords"
	};

	ephy_dialog_get_controls
		(EPHY_DIALOG (pd),
		 properties[PROP_WINDOW].id, &window,
		 properties[PROP_NOTEBOOK].id, &notebook,
		 NULL);

	id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	g_return_if_fail (id == 0 || id == 1);

	ephy_gui_help (GTK_WINDOW (window), "epiphany", help_preferences[id]);
}

static void
action_treeview_selection_changed_cb (GtkTreeSelection *selection,
				      PdmActionInfo *action)
{
	GtkWidget *widget;
	EphyDialog *d = EPHY_DIALOG(action->dialog);
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) > 0;

	widget = ephy_dialog_get_control (d, properties[action->remove_id].id);
	gtk_widget_set_sensitive (widget, has_selection);
}

static void
pdm_cmd_delete_selection (PdmActionInfo *action)
{

	GList *llist, *rlist = NULL, *l, *r;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter, iter2;
	GtkTreeRowReference *row_ref = NULL;

	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(action->treeview));
	llist = gtk_tree_selection_get_selected_rows (selection, &model);

	if (llist == NULL)
	{
		/* nothing to delete, return early */
		return;
	}

	for (l = llist;l != NULL; l = l->next)
	{
		rlist = g_list_prepend (rlist, gtk_tree_row_reference_new
					(model, (GtkTreePath *)l->data));
	}

	/* Intelligent selection logic, no actual selection yet */
	
	path = gtk_tree_row_reference_get_path 
		((GtkTreeRowReference *) g_list_first (rlist)->data);
	
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	iter2 = iter;
	
	if (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter))
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
		row_ref = gtk_tree_row_reference_new (model, path);
	}
	else
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter2);
		if (gtk_tree_path_prev (path))
		{
			row_ref = gtk_tree_row_reference_new (model, path);
		}
	}
	gtk_tree_path_free (path);
	
	/* Removal */
	
	for (r = rlist; r != NULL; r = r->next)
	{
		GValue val = { 0, };

		path = gtk_tree_row_reference_get_path
			((GtkTreeRowReference *)r->data);

		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get_value (model, &iter, action->data_col, &val);
		action->remove (action, g_value_get_boxed (&val));
		g_value_unset (&val);

		/* for cookies we delete from callback, for passwords right here */
		if (action->delete_row_on_remove)
		{
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		}

		gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
		gtk_tree_path_free (path);
	}

	g_list_foreach (llist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (llist);
	g_list_free (rlist);

	/* Selection */
	
	if (row_ref != NULL)
	{
		path = gtk_tree_row_reference_get_path (row_ref);

		if (path != NULL)
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (action->treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}

		gtk_tree_row_reference_free (row_ref);
	}
}

static gboolean
pdm_key_pressed_cb (GtkTreeView *treeview,
		    GdkEventKey *event,
		    PdmActionInfo *action)
{
	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete)
	{
		pdm_cmd_delete_selection (action);

		return TRUE;
	}

	return FALSE;
}

static void
pdm_dialog_remove_button_clicked_cb (GtkWidget *button,
				     PdmActionInfo *action)
{
	pdm_cmd_delete_selection (action);
}

static void
setup_action (PdmActionInfo *action)
{
	GtkWidget *widget;
	GtkTreeSelection *selection;

	widget = ephy_dialog_get_control (EPHY_DIALOG(action->dialog),
					  properties[action->remove_id].id);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (pdm_dialog_remove_button_clicked_cb),
			  action);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(action->treeview));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (action_treeview_selection_changed_cb),
			  action);

	g_signal_connect (G_OBJECT (action->treeview),
			  "key_press_event",
			  G_CALLBACK (pdm_key_pressed_cb),
			  action);

}

/* "Cookies" tab */

static void
show_cookies_properties (PdmDialog *dialog,
			 EphyCookie *info)
{
	GtkWidget *gdialog;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *parent;
	GtkWidget *dialog_vbox;
	char *str;

	parent = ephy_dialog_get_control (EPHY_DIALOG(dialog),
					  properties[PROP_WINDOW].id);

	gdialog = gtk_dialog_new_with_buttons
		 (_("Cookie Properties"),
		  GTK_WINDOW (parent),
		  GTK_DIALOG_DESTROY_WITH_PARENT,
		  GTK_STOCK_CLOSE, 0, NULL);
	ephy_state_add_window (GTK_WIDGET (gdialog), "cookie_properties", 
			       -1, -1, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE | EPHY_STATE_WINDOW_SAVE_POSITION);
	gtk_dialog_set_has_separator (GTK_DIALOG(gdialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (gdialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (gdialog)->vbox), 14); /* 24 = 2 * 5 + 14 */

	table = gtk_table_new (2, 4, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_widget_show (table);

	str = g_strconcat ("<b>", _("Content:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (info->value);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 0, 1);

	str = g_strconcat ("<b>", _("Path:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (info->path);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 1, 2);

	str = g_strconcat ("<b>", _("Send for:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
			  GTK_FILL, GTK_FILL, 0, 0);

	label = gtk_label_new (info->is_secure ? _("Encrypted connections only") : _("Any type of connection") );
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 2, 3);

	str = g_strconcat ("<b>", _("Expires:"), "</b>", NULL);
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
			  GTK_FILL, GTK_FILL, 0, 0);

	if (info->is_session)
	{
		str = g_strdup (_("End of current session"));
	}
	else
	{
		struct tm t;
		char s[128];
		const char *fmt_hack = "%c";
		strftime (s, sizeof(s), fmt_hack, localtime_r (&info->expires, &t));
		str = g_locale_to_utf8 (s, -1, NULL, NULL, NULL);
	}
	label = gtk_label_new (str);
	g_free (str);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 3, 4);

	dialog_vbox = GTK_DIALOG(gdialog)->vbox;
	gtk_box_pack_start (GTK_BOX(dialog_vbox),
			    table,
			    FALSE, FALSE, 0);

	gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
				     GTK_WINDOW (gdialog));

	gtk_dialog_run (GTK_DIALOG (gdialog));

	gtk_widget_destroy (gdialog);
}

static void
cookies_properties_clicked_cb (GtkWidget *button,
			       PdmDialog *dialog)
{
	GtkTreeModel *model;
	GValue val = {0, };
	GtkTreeIter iter;
	GtkTreePath *path;
	EphyCookie *cookie;
	GList *l;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (dialog->priv->cookies->treeview);
	l = gtk_tree_selection_get_selected_rows
		(selection, &model);

	path = (GtkTreePath *)l->data;
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get_value
		(model, &iter, COL_COOKIES_DATA, &val);
	cookie = (EphyCookie *) g_value_get_boxed (&val);

	show_cookies_properties (dialog, cookie);

	g_value_unset (&val);

	g_list_foreach (l, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (l);
}

static void
cookies_treeview_selection_changed_cb (GtkTreeSelection *selection,
				       PdmDialog *dialog)
{
	GtkWidget *widget;
	EphyDialog *d = EPHY_DIALOG(dialog);
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) == 1;

	widget = ephy_dialog_get_control (d, properties[PROP_COOKIES_PROPERTIES].id);
	gtk_widget_set_sensitive (widget, has_selection);
}

static gboolean
cookie_search_equal (GtkTreeModel *model,
		     int column,
		     const gchar *key,
		     GtkTreeIter *iter,
		     gpointer search_data)
{
	GValue value = { 0, };
	gboolean retval;

	/* Note that this is function has to return FALSE for a *match* ! */

	gtk_tree_model_get_value (model, iter, column, &value);
	retval = strstr (g_value_get_string (&value), key) == NULL;
	g_value_unset (&value);

	return retval;
}

static void
pdm_dialog_cookies_construct (PdmActionInfo *info)
{
	PdmDialog *dialog = info->dialog;
	GtkTreeView *treeview;
	GtkListStore *liststore;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkWidget *button;

	LOG ("pdm_dialog_cookies_construct");

	ephy_dialog_get_controls (EPHY_DIALOG (dialog),
				  properties[PROP_COOKIES_TREEVIEW].id, &treeview,
				  properties[PROP_COOKIES_PROPERTIES].id, &button,
				  NULL);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (cookies_properties_clicked_cb), dialog);

	/* set tree model */
	liststore = gtk_list_store_new (4,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_STRING,
					EPHY_TYPE_COOKIE);
	gtk_tree_view_set_model (treeview, GTK_TREE_MODEL(liststore));
	gtk_tree_view_set_headers_visible (treeview, TRUE);
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	info->model = GTK_TREE_MODEL (liststore);
	g_object_unref (liststore);

	g_signal_connect (selection, "changed",
			  G_CALLBACK(cookies_treeview_selection_changed_cb),
			  dialog);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (treeview,
						     TV_COL_COOKIES_HOST,
						     _("Domain"),
						     renderer,
						     "text", COL_COOKIES_HOST,
						     NULL);
	column = gtk_tree_view_get_column (treeview, TV_COL_COOKIES_HOST);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_tree_view_insert_column_with_attributes (treeview,
						     TV_COL_COOKIES_NAME,
						     _("Name"),
						     renderer,
						     "text", COL_COOKIES_NAME,
						     NULL);
	column = gtk_tree_view_get_column (treeview, TV_COL_COOKIES_NAME);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	gtk_tree_view_set_enable_search (treeview, TRUE);
	gtk_tree_view_set_search_column (treeview, COL_COOKIES_HOST);
	gtk_tree_view_set_search_equal_func (treeview,
					     (GtkTreeViewSearchEqualFunc) cookie_search_equal,
					     dialog, NULL);

	info->treeview = treeview;
	info->selection = selection;

	setup_action (info);
}

static gboolean
compare_cookies (const EphyCookie *cookie1,
		 const EphyCookie *cookie2)
{
	g_return_val_if_fail (cookie1 != NULL || cookie2 != NULL, FALSE);

	return (strcmp (cookie1->domain, cookie2->domain) == 0
		&& strcmp (cookie1->path, cookie2->path) == 0
		&& strcmp (cookie1->name, cookie2->name) == 0);
}

static gboolean
cookie_to_iter (GtkTreeModel *model,
		const EphyCookie *cookie,
		GtkTreeIter *iter)
{
	gboolean valid;
	gboolean found = FALSE;

	valid = gtk_tree_model_get_iter_first (model, iter);

	while (valid)
	{
		EphyCookie *data;

		gtk_tree_model_get (model, iter,
				    COL_COOKIES_DATA, &data,
				    -1);

		found = compare_cookies (cookie, data);

		ephy_cookie_free (data);

		if (found) break;

		valid = gtk_tree_model_iter_next (model, iter);
	}

	return found;
}

static void
cookie_added_cb (EphyCookieManager *manager,
		 const EphyCookie *cookie,
		 PdmDialog *dialog)
{
	PdmActionInfo *info = dialog->priv->cookies;
	
	LOG ("cookie_added_cb");

	info->add (info, (gpointer) ephy_cookie_copy (cookie));
}

static void
cookie_changed_cb (EphyCookieManager *manager,
		   const EphyCookie *cookie,
		   PdmDialog *dialog)
{
	PdmActionInfo *info = dialog->priv->cookies;
	GtkTreeIter iter;

	LOG ("cookie_changed_cb");

	if (cookie_to_iter (info->model, cookie, &iter))
	{
		gtk_list_store_remove (GTK_LIST_STORE (info->model), &iter);		
		info->add (info, (gpointer) ephy_cookie_copy (cookie));
	}
	else
	{
		g_warning ("Unable to find changed cookie in list!\n");
	}
}

static void
cookie_deleted_cb (EphyCookieManager *manager,
		   const EphyCookie *cookie,
		   PdmDialog *dialog)
{
	PdmActionInfo *info = dialog->priv->cookies;
	GtkTreeIter iter;

	LOG ("cookie_deleted_cb");

	if (cookie_to_iter (info->model, cookie, &iter))
	{
		gtk_list_store_remove (GTK_LIST_STORE (info->model), &iter);		
	}
	else
	{
		g_warning ("Unable to find deleted cookie in list!\n");
	}
}

static void
cookies_cleared_cb (EphyCookieManager *manager,
		    PdmDialog *dialog)
{
	PdmActionInfo *info = dialog->priv->cookies;

	LOG ("cookies_cleared_cb");

	gtk_list_store_clear (GTK_LIST_STORE (info->model));
}

static gboolean
cookie_host_to_iter (GtkTreeModel *model,
		     const char *key1,
		     GtkTreeIter *iter)
{
	GtkTreeIter iter2;
	gboolean valid;
	gssize len;
	int max = 0;

	len = strlen (key1);

	valid = gtk_tree_model_get_iter_first (model, &iter2);

	while (valid)
	{
		const char *p, *q;
		char *key2;
		int n = 0;

		gtk_tree_model_get (model, &iter2, COL_COOKIES_HOST, &key2, -1);

		/* Count the segments (string between successive dots)
		 * that key1 and key2 share.
		 */

		/* Start on the \0 */
		p = key1 + len;
		q = key2 + strlen (key2);
	
		do
		{
			if (*p == '.') ++n;
			--p;
			--q;
		}
		while (p >= key1 && q >= key2 && *p == *q);

		if ((p < key1 && q < key2 && *key1 != '.' && *key2 != '.') ||
		    (p < key1 && q >= key2 && *q == '.') ||
		    (q < key2 && p >= key1 && *p == '.'))
		{
			++n;
		}

		g_free (key2);

		/* Complete match */
		if (p < key1 && q < key2)
		{
			*iter = iter2;
			return TRUE;
		}

		if (n > max)
		{
			max = n;
			*iter = iter2;
		}

		valid = gtk_tree_model_iter_next (model, &iter2);
	}

	return max > 0;
}

static int
compare_cookie_host_keys (GtkTreeModel *model,
			  GtkTreeIter  *a,
			  GtkTreeIter  *b,
			  gpointer user_data)
{
	GValue a_value = {0, };
	GValue b_value = {0, };
	int retval;

	gtk_tree_model_get_value (model, a, COL_COOKIES_HOST_KEY, &a_value);
	gtk_tree_model_get_value (model, b, COL_COOKIES_HOST_KEY, &b_value);

	retval = strcmp (g_value_get_string (&a_value),
			 g_value_get_string (&b_value));

	g_value_unset (&a_value);
	g_value_unset (&b_value);

	return retval;
}

static void
pdm_dialog_fill_cookies_list (PdmActionInfo *info)
{
	EphyCookieManager *manager;
	GList *list, *l;

	g_assert (info->filled == FALSE);

	manager = EPHY_COOKIE_MANAGER (ephy_embed_shell_get_embed_single
			(EPHY_EMBED_SHELL (ephy_shell)));

	list = ephy_cookie_manager_list_cookies (manager);

	for (l = list; l != NULL; l = l->next)
	{
		info->add (info, l->data);
	}

	/* the element data has been consumed, so we need only to free the list */
	g_list_free (list);

	/* Now turn on sorting */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (info->model),
					 COL_COOKIES_HOST_KEY,
					 (GtkTreeIterCompareFunc) compare_cookie_host_keys,
					 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (info->model),
					      COL_COOKIES_HOST_KEY,
					      GTK_SORT_ASCENDING);
	
	info->filled = TRUE;

	/* Now connect the callbacks on the EphyCookieManager */
	g_signal_connect (manager, "cookie-added",
			 G_CALLBACK (cookie_added_cb), info->dialog);
	g_signal_connect (manager, "cookie-changed",
			 G_CALLBACK (cookie_changed_cb), info->dialog);
	g_signal_connect (manager, "cookie-deleted",
			 G_CALLBACK (cookie_deleted_cb), info->dialog);
	g_signal_connect (manager, "cookies-cleared",
			 G_CALLBACK (cookies_cleared_cb), info->dialog);

	info->scroll_to (info);
}

static void
pdm_dialog_cookies_destruct (PdmActionInfo *info)
{
	g_free (info->scroll_to_host);
	info->scroll_to_host = NULL;
}

static void
pdm_dialog_cookie_add (PdmActionInfo *info,
		       gpointer data)
{
	EphyCookie *cookie = (EphyCookie *) data;
	GtkListStore *store;
	GtkTreeIter iter;
	int column[4] = { COL_COOKIES_HOST, COL_COOKIES_HOST_KEY, COL_COOKIES_NAME, COL_COOKIES_DATA };
	GValue value[4] = { { 0, }, { 0, }, { 0, }, { 0, } };

	store = GTK_LIST_STORE(info->model);

	/* NOTE: We use this strange method to insert the row, because
	 * we want to use g_value_take_string but all the row data needs to
	 * be inserted in one call as it's needed when the new row is sorted
	 * into the model.
	 */

	g_value_init (&value[0], G_TYPE_STRING);
	g_value_init (&value[1], G_TYPE_STRING);
	g_value_init (&value[2], G_TYPE_STRING);
	g_value_init (&value[3], EPHY_TYPE_COOKIE);

	g_value_set_static_string (&value[0], cookie->domain);
	g_value_take_string (&value[1], ephy_string_collate_key_for_domain (cookie->domain, -1));
	g_value_set_static_string (&value[2], cookie->name);
	g_value_take_boxed (&value[3], cookie);

	gtk_list_store_insert_with_valuesv (store, &iter, -1,
					    column, value,
					    G_N_ELEMENTS (value));

	g_value_unset (&value[0]);
	g_value_unset (&value[1]);
	g_value_unset (&value[2]);
	g_value_unset (&value[3]);
}

static void
pdm_dialog_cookie_remove (PdmActionInfo *info,
			  gpointer data)
{
	EphyCookie *cookie = (EphyCookie *) data;
	EphyCookieManager *manager;

	manager = EPHY_COOKIE_MANAGER (ephy_embed_shell_get_embed_single
			(EPHY_EMBED_SHELL (ephy_shell)));

	ephy_cookie_manager_remove_cookie (manager, cookie);
}

static void
pdm_dialog_cookie_scroll_to (PdmActionInfo *info)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	if (info->scroll_to_host == NULL || !info->filled) return;

	if (cookie_host_to_iter (info->model, info->scroll_to_host, &iter))
	{
		gtk_tree_selection_unselect_all (info->selection);

		path = gtk_tree_model_get_path (info->model, &iter);
		gtk_tree_view_scroll_to_cell (info->treeview,
					      path, NULL, TRUE,
					      0.5, 0.0);
		gtk_tree_path_free (path);
	}

	g_free (info->scroll_to_host);
	info->scroll_to_host = NULL;
}

/* "Passwords" tab */

static void
passwords_show_toggled_cb (GtkWidget *button,
			   PdmDialog *dialog)
{
	GtkTreeView *treeview;
	GtkTreeViewColumn *column;
	gboolean active;

	treeview = GTK_TREE_VIEW (ephy_dialog_get_control
			(EPHY_DIALOG(dialog), properties[PROP_PASSWORDS_TREEVIEW].id));
	column = gtk_tree_view_get_column (treeview, COL_PASSWORDS_PASSWORD);

	active = gtk_toggle_button_get_active ((GTK_TOGGLE_BUTTON (button)));
	
	gtk_tree_view_column_set_visible (column, active);
}

static void
pdm_dialog_passwords_construct (PdmActionInfo *info)
{
	PdmDialog *dialog = info->dialog;
	GtkTreeView *treeview;
	GtkListStore *liststore;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkWidget *button;

	LOG ("pdm_dialog_passwords_construct");

	ephy_dialog_get_controls (EPHY_DIALOG (dialog),
				  properties[PROP_PASSWORDS_TREEVIEW].id, &treeview,
				  properties[PROP_PASSWORDS_SHOW].id, &button,
				  NULL);

	g_signal_connect (button, "toggled",
			  G_CALLBACK (passwords_show_toggled_cb), dialog);

	/* set tree model */
	liststore = gtk_list_store_new (4,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_STRING,
					EPHY_TYPE_PASSWORD_INFO);
	gtk_tree_view_set_model (treeview, GTK_TREE_MODEL(liststore));
	gtk_tree_view_set_headers_visible (treeview, TRUE);
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	info->model = GTK_TREE_MODEL (liststore);
	g_object_unref (liststore);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (treeview,
						     COL_PASSWORDS_HOST,
						     _("Host"),
						     renderer,
						     "text", COL_PASSWORDS_HOST,
						     NULL);
	column = gtk_tree_view_get_column (treeview, COL_PASSWORDS_HOST);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_sort_column_id (column, COL_PASSWORDS_HOST);

	gtk_tree_view_insert_column_with_attributes (treeview,
						     COL_PASSWORDS_USER,
						     _("User Name"),
						     renderer,
						     "text", COL_PASSWORDS_USER,
						     NULL);
	column = gtk_tree_view_get_column (treeview, COL_PASSWORDS_USER);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_sort_column_id (column, COL_PASSWORDS_USER);

	gtk_tree_view_insert_column_with_attributes (treeview,
						     COL_PASSWORDS_PASSWORD,
						     _("User Password"),
						     renderer,
						     "text", COL_PASSWORDS_PASSWORD,
						     NULL);
	column = gtk_tree_view_get_column (treeview, COL_PASSWORDS_PASSWORD);
	/* Hide this info by default */
	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_reorderable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_sort_column_id (column, COL_PASSWORDS_PASSWORD);

	info->treeview = treeview;

	setup_action (info);
}

static void
passwords_changed_cb (EphyPasswordManager *manager,
		      PdmDialog *dialog)
{
	GtkTreeModel *model = dialog->priv->passwords->model;

	LOG ("passwords changed");

	/* since the callback doesn't carry any information about what
	 * exactly has changed, we have to rebuild the list from scratch.
	 */
	gtk_list_store_clear (GTK_LIST_STORE (model));

	/* And turn off sorting */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	dialog->priv->passwords->fill (dialog->priv->passwords);
}

static void
pdm_dialog_fill_passwords_list (PdmActionInfo *info)
{
	EphyPasswordManager *manager;
	GList *list, *l;

	manager = EPHY_PASSWORD_MANAGER (ephy_embed_shell_get_embed_single
			(EPHY_EMBED_SHELL (ephy_shell)));

	list = ephy_password_manager_list_passwords (manager);

	for (l = list; l != NULL; l = l->next)
	{
		info->add (info, l->data);
	}

	/* the element data has been consumed, so we need only to free the list */
	g_list_free (list);

	/* Let's get notified when the list changes */
	if (info->filled == FALSE)
	{
		g_signal_connect (manager, "passwords-changed",
				  G_CALLBACK (passwords_changed_cb), info->dialog);
	}

	info->filled = TRUE;

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (info->model),
					      COL_PASSWORDS_HOST,
					      GTK_SORT_ASCENDING);
}

static void
pdm_dialog_passwords_destruct (PdmActionInfo *info)
{
}

static void
pdm_dialog_password_add (PdmActionInfo *info,
			 gpointer data)
{
	EphyPasswordInfo *pinfo = (EphyPasswordInfo *) data;
	GtkListStore *store;
	GtkTreeIter iter;
	GValue value = { 0, };

	store = GTK_LIST_STORE (info->model);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store,
			    &iter,
			    COL_PASSWORDS_HOST, pinfo->host,
			    COL_PASSWORDS_USER, pinfo->username,
			    COL_PASSWORDS_PASSWORD, pinfo->password,
			    -1);

	g_value_init (&value, EPHY_TYPE_PASSWORD_INFO);
	g_value_take_boxed (&value, pinfo);
	gtk_list_store_set_value (store, &iter, COL_PASSWORDS_DATA, &value);
	g_value_unset (&value);
}

static void
pdm_dialog_password_remove (PdmActionInfo *info,
			    gpointer data)
{
	EphyPasswordInfo *pinfo = (EphyPasswordInfo *) data;
	EphyPasswordManager *manager;

	manager = EPHY_PASSWORD_MANAGER (ephy_embed_shell_get_embed_single
			(EPHY_EMBED_SHELL (ephy_shell)));

	/* we don't remove the password from the liststore in the callback
	 * like we do for cookies, since the callback doesn't carry that
	 * information, and we'd have to reload the whole list, losing the
	 * selection in the process.
	 */
	g_signal_handlers_block_by_func
		(manager, G_CALLBACK (passwords_changed_cb), info->dialog);

	ephy_password_manager_remove_password (manager, pinfo);

	g_signal_handlers_unblock_by_func
		(manager, G_CALLBACK (passwords_changed_cb), info->dialog);
}

/* common routines */

static void
sync_notebook_tab (GtkWidget *notebook,
		   GtkNotebookPage *page,
		   int page_num,
		   PdmDialog *dialog)
{
	PdmDialogPrivate *priv = dialog->priv;

	/* Lazily fill the list store */
	if (page_num == 0 && priv->cookies->filled == FALSE)
	{
		priv->cookies->fill (priv->cookies);

		priv->cookies->scroll_to (priv->cookies);
	}
	else if (page_num == 1 && priv->passwords->filled == FALSE)
	{
		priv->passwords->fill (priv->passwords);
	}
}

static void
pdm_dialog_response_cb (GtkDialog *widget,
			int response,
			PdmDialog *dialog)
{
	if (response == GTK_RESPONSE_HELP)
	{
		pdm_dialog_show_help (dialog);
		return;
	}

	g_object_unref (dialog);
}

static void
pdm_dialog_init (PdmDialog *dialog)
{
	PdmDialogPrivate *priv;
	PdmActionInfo *cookies, *passwords;
	GtkWidget *window;

	priv = dialog->priv = EPHY_PDM_DIALOG_GET_PRIVATE (dialog);

	ephy_dialog_construct (EPHY_DIALOG(dialog),
			       properties,
			       ephy_file ("epiphany.glade"),
			       "pdm_dialog",
			       NULL);

	ephy_dialog_get_controls (EPHY_DIALOG (dialog),
				  properties[PROP_WINDOW].id, &window,
				  properties[PROP_NOTEBOOK].id, &priv->notebook,
				  NULL);

	ephy_gui_ensure_window_group (GTK_WINDOW (window));

	gtk_window_set_icon_name (GTK_WINDOW (window), "web-browser");

	g_signal_connect (window, "response",
			  G_CALLBACK (pdm_dialog_response_cb), dialog);
	/**
	 * Group all Properties and Remove buttons in the same size group to
	 * avoid the little jerk you get otherwise when switching pages because
	 * one set of buttons is wider than another.
	 */
	ephy_dialog_set_size_group (EPHY_DIALOG (dialog),
				    properties[PROP_COOKIES_REMOVE].id,
				    properties[PROP_COOKIES_PROPERTIES].id,
				    properties[PROP_PASSWORDS_REMOVE].id,
				    NULL);

	cookies = g_new0 (PdmActionInfo, 1);
	cookies->construct = pdm_dialog_cookies_construct;
	cookies->destruct = pdm_dialog_cookies_destruct;
	cookies->fill = pdm_dialog_fill_cookies_list;
	cookies->add = pdm_dialog_cookie_add;
	cookies->remove = pdm_dialog_cookie_remove;
	cookies->scroll_to = pdm_dialog_cookie_scroll_to;
	cookies->dialog = dialog;
	cookies->remove_id = PROP_COOKIES_REMOVE;
	cookies->data_col = COL_COOKIES_DATA;
	cookies->scroll_to_host = NULL;
	cookies->filled = FALSE;
	cookies->delete_row_on_remove = FALSE;

	passwords = g_new0 (PdmActionInfo, 1);
	passwords->construct = pdm_dialog_passwords_construct;
	passwords->destruct = pdm_dialog_passwords_destruct;
	passwords->fill = pdm_dialog_fill_passwords_list;
	passwords->add = pdm_dialog_password_add;
	passwords->remove = pdm_dialog_password_remove;
	passwords->dialog = dialog;
	passwords->remove_id = PROP_PASSWORDS_REMOVE;
	passwords->data_col = COL_PASSWORDS_DATA;
	passwords->scroll_to_host = NULL;
	passwords->filled = FALSE;
	passwords->delete_row_on_remove = TRUE;

	priv->cookies = cookies;
	priv->passwords = passwords;

	cookies->construct (cookies);
	passwords->construct (passwords);

	sync_notebook_tab (priv->notebook, NULL, 0, dialog);
	g_signal_connect (G_OBJECT (priv->notebook), "switch_page",
			  G_CALLBACK (sync_notebook_tab), dialog);
}

static void
pdm_dialog_finalize (GObject *object)
{
	PdmDialog *dialog = EPHY_PDM_DIALOG (object);
	GObject *single;

	single = ephy_embed_shell_get_embed_single (embed_shell);

	g_signal_handlers_disconnect_matched
		(single, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, object);

	dialog->priv->cookies->destruct (dialog->priv->cookies);
	dialog->priv->passwords->destruct (dialog->priv->passwords);

	g_free (dialog->priv->passwords);
	g_free (dialog->priv->cookies);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
pdm_dialog_open (PdmDialog *dialog,
		 const char *host)
{
	PdmDialogPrivate *priv = dialog->priv;

	/* Switch to cookies tab */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);

	g_free (priv->cookies->scroll_to_host);
	priv->cookies->scroll_to_host = g_strdup (host);

	priv->cookies->scroll_to (priv->cookies);
	gtk_widget_grab_focus (GTK_WIDGET (priv->cookies->treeview));

	ephy_dialog_show (EPHY_DIALOG (dialog));
}
