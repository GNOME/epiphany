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

#include "language-editor.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>
#include <glib/gi18n.h>
#include <string.h>

enum
{
	COL_DESCRIPTION,
	COL_DATA
};

enum
{
	TREEVIEW_PROP,
	ADD_PROP,
	REMOVE_PROP,
	LANGUAGE_PROP
};

static const
EphyDialogProperty properties [] =
{
	{ "languages_treeview",	NULL, PT_NORMAL, 0 },
	{ "add_button",		NULL, PT_NORMAL, 0 },
	{ "remove_button",	NULL, PT_NORMAL, 0 },
	{ "languages_combo",	NULL, PT_NORMAL, 0 },

	{ NULL }
};

#define EPHY_LANGUAGE_EDITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LANGUAGE_EDITOR, LanguageEditorPrivate))

struct LanguageEditorPrivate
{
	GtkWidget *treeview;
	GtkTreeModel *model;
};

static void language_editor_class_init	(LanguageEditorClass *klass);
static void language_editor_init	(LanguageEditor *ge);

/* Glade callbacks */

void language_editor_close_button_cb	(GtkWidget *button, 
					 EphyDialog *dialog);

enum
{
	CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

GType
language_editor_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (LanguageEditorClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) language_editor_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LanguageEditor),
			0, /* n_preallocs */
			(GInstanceInitFunc) language_editor_init
		};

		type = g_type_register_static (EPHY_TYPE_DIALOG,
					       "LanguageEditor",
					       &our_info, 0);
	}

	return type;
}

static void
language_editor_class_init (LanguageEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	signals[CHANGED] =
		g_signal_new ("list-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LanguageEditorClass, list_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	g_type_class_add_private (object_class, sizeof(LanguageEditorPrivate));
}

static void
language_editor_update_pref (LanguageEditor *editor)
{
	GtkTreeIter iter;
	GSList *codes = NULL;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (editor->priv->model), &iter))
	{
		return;
	}

	do
	{
		GValue value = {0, };

		gtk_tree_model_get_value (GTK_TREE_MODEL (editor->priv->model),
					  &iter, COL_DATA, &value);

		codes = g_slist_append (codes, g_value_dup_string (&value));

		g_value_unset (&value);
	}
	while (gtk_tree_model_iter_next (GTK_TREE_MODEL (editor->priv->model), &iter));

	g_signal_emit (editor, signals[CHANGED], 0, codes);

	g_slist_foreach (codes, (GFunc) g_free, NULL);
	g_slist_free (codes);
}

static void
language_editor_add_button_clicked_cb (GtkButton *button,
				       LanguageEditor *editor)
{
	GtkWidget *combo;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *code = NULL, *desc = NULL;
	int index;

	combo = ephy_dialog_get_control (EPHY_DIALOG (editor), properties[LANGUAGE_PROP].id);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	index = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

	if (gtk_tree_model_iter_nth_child (model, &iter, NULL, index))
	{
		gtk_tree_model_get (model, &iter,
				    0, &desc,
				    1, &code,
				    -1);

		language_editor_add (editor, code, desc);

		g_free (desc);
		g_free (code);
	}

	language_editor_update_pref (editor);
}

static void
language_editor_remove_button_clicked_cb (GtkButton *button,
					  LanguageEditor *editor)
{
	GList *llist, *rlist = NULL, *l, *r;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(editor->priv->treeview));
	llist = gtk_tree_selection_get_selected_rows (selection, &model);
	for (l = llist;l != NULL; l = l->next)
	{
		rlist = g_list_prepend (rlist, gtk_tree_row_reference_new
					(model, (GtkTreePath *)l->data));
	}

	for (r = rlist; r != NULL; r = r->next)
	{
		GtkTreePath *path;

		path = gtk_tree_row_reference_get_path
			((GtkTreeRowReference *)r->data);

		gtk_tree_model_get_iter (model, &iter, path);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
		gtk_tree_path_free (path);
	}

	g_list_foreach (llist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (llist);
	g_list_free (rlist);

	language_editor_update_pref (editor);
}

static void
language_editor_treeview_drag_end_cb (GtkWidget *widget,
				      GdkDragContext *context,
				      LanguageEditor *editor)
{
	language_editor_update_pref (editor);
}

static void
language_editor_set_view (LanguageEditor *ge,
			  GtkWidget *treeview,
			  GtkWidget *add_button,
			  GtkWidget *remove_button)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkListStore *liststore;
	GtkTreeSelection *selection;

	ge->priv->treeview = treeview;

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW(ge->priv->treeview), TRUE);

	liststore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	ge->priv->model = GTK_TREE_MODEL (liststore);

	gtk_tree_view_set_model (GTK_TREE_VIEW(ge->priv->treeview),
				 ge->priv->model);
	g_object_unref (ge->priv->model);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(ge->priv->treeview),
					   FALSE);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(ge->priv->treeview),
						     0, "Language",
						     renderer,
						     "text", 0,
						     NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(ge->priv->treeview), 0);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_DESCRIPTION);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(ge->priv->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	/* Connect treeview signals */
	g_signal_connect
		(G_OBJECT (ge->priv->treeview),
		 "drag_end",
		 G_CALLBACK (language_editor_treeview_drag_end_cb),
		 ge);

	/* Connect buttons signals */
	g_signal_connect
		(G_OBJECT (add_button),
		 "clicked",
		 G_CALLBACK (language_editor_add_button_clicked_cb),
		 ge);

	g_signal_connect
		(G_OBJECT (remove_button),
		 "clicked",
		 G_CALLBACK (language_editor_remove_button_clicked_cb),
		 ge);
}

static void
language_editor_init (LanguageEditor *le)
{
	GtkWidget *treeview;
	GtkWidget *add_button;
	GtkWidget *remove_button;

	le->priv = EPHY_LANGUAGE_EDITOR_GET_PRIVATE (le);

	ephy_dialog_construct (EPHY_DIALOG(le),
			       properties,
			       "prefs-dialog.glade",
			       "languages_dialog");

	treeview = ephy_dialog_get_control (EPHY_DIALOG(le), properties[TREEVIEW_PROP].id);
	add_button = ephy_dialog_get_control (EPHY_DIALOG(le), properties[ADD_PROP].id);
	remove_button = ephy_dialog_get_control (EPHY_DIALOG(le), properties[REMOVE_PROP].id);

	language_editor_set_view (le, treeview, add_button, remove_button);
}

LanguageEditor *
language_editor_new (GtkWidget *parent)
{
	return EPHY_LANGUAGE_EDITOR (g_object_new (EPHY_TYPE_LANGUAGE_EDITOR,
						   "parent-window", parent,
						   NULL));
}

void
language_editor_set_model (LanguageEditor *editor,
			   GtkTreeModel *model)
{
	GtkWidget *combo;
	GtkCellRenderer *renderer;

	combo = ephy_dialog_get_control (EPHY_DIALOG (editor), properties[LANGUAGE_PROP].id);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), model);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo),
                                    renderer,
                                    TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                        "text", COL_DESCRIPTION,
                                        NULL);

	ephy_dialog_set_data_column (EPHY_DIALOG (editor), properties[LANGUAGE_PROP].id, COL_DATA);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
}

void
language_editor_add (LanguageEditor *editor,
		     const char *code,
		     const char *desc)
{
	GtkTreeIter iter;

	g_return_if_fail (code != NULL && desc != NULL);

	/* first check that the code isn't already in the list */
	if (gtk_tree_model_get_iter_first (editor->priv->model, &iter))
	{
		do
		{
			char *c;

			gtk_tree_model_get (editor->priv->model, &iter,
					    COL_DATA, &c,
					    -1);

			if (c && strcmp (code, c) == 0)
			{
				/* already in list, no need to add again */
				g_free (c);
				return;
			}
			g_free (c);
		}
		while (gtk_tree_model_iter_next (editor->priv->model, &iter));
	}

	gtk_list_store_append (GTK_LIST_STORE (editor->priv->model), &iter);

	gtk_list_store_set (GTK_LIST_STORE (editor->priv->model), &iter,
			    COL_DESCRIPTION, desc,
			    COL_DATA, code,
			    -1);
}

void
language_editor_close_button_cb (GtkWidget *button, EphyDialog *dialog)
{
	g_object_unref (dialog);
}
