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

#include "language-editor.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>

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
	{ TREEVIEW_PROP, "languages_treeview", NULL, PT_NORMAL, NULL },
	{ ADD_PROP, "add_button", NULL, PT_NORMAL, NULL },
	{ REMOVE_PROP, "remove_button", NULL, PT_NORMAL, NULL },
        { LANGUAGE_PROP, "languages_optionmenu", NULL, PT_NORMAL, NULL },

        { -1, NULL, NULL }
};


struct LanguageEditorPrivate
{
	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkWidget *optionmenu;
};

enum
{
	CHANGED,
	LAST_SIGNAL
};

static void
language_editor_class_init (LanguageEditorClass *klass);
static void
language_editor_init (LanguageEditor *ge);
static void
language_editor_finalize (GObject *object);

/* Glade callbacks */

void
language_editor_close_button_cb (GtkWidget *button, EphyDialog *dialog);

static GObjectClass *parent_class = NULL;

static gint signals[LAST_SIGNAL];

GType
language_editor_get_type (void)
{
        static GType language_editor_type = 0;

        if (language_editor_type == 0)
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

                language_editor_type = g_type_register_static (EPHY_DIALOG_TYPE,
							       "LanguageEditor",
							       &our_info, 0);
        }

        return language_editor_type;

}

static void
language_editor_class_init (LanguageEditorClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = language_editor_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (LanguageEditorClass, changed),
                              NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);
}

static void
language_editor_update_pref (LanguageEditor *editor)
{
	GtkTreeIter iter;
	int index;
	GSList *strings = NULL;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (editor->priv->model), &iter))
	{
		return;
	}

	do
	{
		GValue val = {0, };
		gtk_tree_model_get_value (GTK_TREE_MODEL (editor->priv->model),
					  &iter, COL_DATA, &val);
		index = g_value_get_int (&val);
		g_value_unset (&val);

		strings = g_slist_append(strings, GINT_TO_POINTER(index));
	}
	while (gtk_tree_model_iter_next (GTK_TREE_MODEL (editor->priv->model), &iter));

	g_signal_emit (editor, signals[CHANGED], 0, strings);

        g_slist_free (strings);
}

static void
language_editor_add_button_clicked_cb (GtkButton *button,
				       LanguageEditor *editor)
{
	const char *description;
	GtkTreeIter iter;
	GtkWidget *menu;
	GtkWidget *item;
	int history;

	history = gtk_option_menu_get_history (GTK_OPTION_MENU(editor->priv->optionmenu));
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU(editor->priv->optionmenu));
	item = gtk_menu_get_active (GTK_MENU(menu));
	description = (const char *) g_object_get_data (G_OBJECT(item), "desc");

	g_return_if_fail (description != NULL);

	gtk_list_store_append (GTK_LIST_STORE (editor->priv->model),
			       &iter);

	gtk_list_store_set (GTK_LIST_STORE (editor->priv->model),
			    &iter,
			    COL_DESCRIPTION, description,
			    COL_DATA, history,
			    -1);

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
			  GtkWidget *remove_button,
			  GtkWidget *optionmenu)
{
	GtkTreeViewColumn *column;
        GtkCellRenderer *renderer;
	GtkListStore *liststore;
	GtkTreeSelection *selection;

	ge->priv->treeview = treeview;
	ge->priv->optionmenu = optionmenu;

	gtk_tree_view_set_reorderable (GTK_TREE_VIEW(ge->priv->treeview), TRUE);

	liststore = gtk_list_store_new (2,
					G_TYPE_STRING,
					G_TYPE_INT);

	ge->priv->model = GTK_TREE_MODEL (liststore);

        gtk_tree_view_set_model (GTK_TREE_VIEW(ge->priv->treeview),
                                 ge->priv->model);
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
                 G_CALLBACK(language_editor_treeview_drag_end_cb),
		 ge);

	/* Connect buttons signals */
	g_signal_connect
                (G_OBJECT (add_button),
		 "clicked",
                 G_CALLBACK(language_editor_add_button_clicked_cb),
		 ge);

	g_signal_connect
                (G_OBJECT (remove_button),
		 "clicked",
                 G_CALLBACK(language_editor_remove_button_clicked_cb),
		 ge);
}

static void
language_editor_init (LanguageEditor *le)
{
	GtkWidget *treeview;
	GtkWidget *optionmenu;
	GtkWidget *add_button;
	GtkWidget *remove_button;

        le->priv = g_new0 (LanguageEditorPrivate, 1);

	ephy_dialog_construct (EPHY_DIALOG(le),
                                 properties,
                                 "prefs-dialog.glade",
                                 "languages_dialog");

	treeview = ephy_dialog_get_control (EPHY_DIALOG(le),
					    TREEVIEW_PROP);
	add_button = ephy_dialog_get_control (EPHY_DIALOG(le),
					      ADD_PROP);
	remove_button = ephy_dialog_get_control (EPHY_DIALOG(le),
					         REMOVE_PROP);
	optionmenu = ephy_dialog_get_control (EPHY_DIALOG(le),
					      LANGUAGE_PROP);

	language_editor_set_view (le, treeview, add_button, remove_button,
				  optionmenu);
}

static void
language_editor_finalize (GObject *object)
{
        LanguageEditor *ge;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_LANGUAGE_EDITOR (object));

	ge = LANGUAGE_EDITOR (object);

        g_return_if_fail (ge->priv != NULL);

        g_free (ge->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

LanguageEditor *
language_editor_new (GtkWidget *parent)
{
	return LANGUAGE_EDITOR (g_object_new (LANGUAGE_EDITOR_TYPE,
				"ParentWindow", parent,
				NULL));
}

void
language_editor_set_menu (LanguageEditor *editor,
			  GtkWidget *menu)
{
	gtk_option_menu_set_menu (GTK_OPTION_MENU(editor->priv->optionmenu),
				  menu);
}

void
language_editor_add (LanguageEditor *ge,
		     const char *language,
		     int id)
{
	GtkTreeIter iter;

	gtk_list_store_append (GTK_LIST_STORE (ge->priv->model),
			       &iter);

	gtk_list_store_set (GTK_LIST_STORE (ge->priv->model),
			    &iter,
			    COL_DESCRIPTION, language,
			    COL_DATA, id,
			    -1);
}

void
language_editor_close_button_cb (GtkWidget *button, EphyDialog *dialog)
{
	g_object_unref (dialog);
}
