/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
 *  Copyright (C) 2003  Marco Pesenti Gritti
 *  Copyright (C) 2003  Christian Persch
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

#include "ephy-tree-model-node.h"
#include "ephy-location-entry.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"

#include <gtk/gtktoolbar.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcomboboxentry.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtktreemodelsort.h>
#include <string.h>

#define EPHY_LOCATION_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryPrivate))

struct _EphyLocationEntryPrivate
{
	EphyTreeModelNode *combo_model;
	GtkWidget *combo;
	GtkWidget *entry;
	char *before_completion;
	gboolean user_changed;
	gboolean activation_mode;
	EphyNodeDb *combo_db;
	EphyNode *combo_root;
	char *xml_file;

	guint text_col;
	guint action_col;
	guint keywords_col;
	guint relevance_col;
};

static char *web_prefixes [] =
{
	"http://www.",
        "http://",
        "https://www.",
        "https://",
        "www."
};
static int n_web_prefixes = G_N_ELEMENTS (web_prefixes);

static void ephy_location_entry_class_init (EphyLocationEntryClass *klass);
static void ephy_location_entry_init (EphyLocationEntry *le);
static void ephy_location_entry_finalize (GObject *o);

static GObjectClass *parent_class = NULL;

enum EphyLocationEntrySignalsEnum
{
	USER_CHANGED,
	LAST_SIGNAL
};
static gint EphyLocationEntrySignals[LAST_SIGNAL];

enum
{
	LOCATION_HISTORY_NODE_ID = 1
};

enum
{
	EPHY_NODE_LOC_HISTORY_PROP_TEXT = 1
};

#define MAX_LOC_HISTORY_ITEMS 10
#define EPHY_LOC_HISTORY_XML_ROOT "ephy_location_history"
#define EPHY_LOC_HISTORY_XML_VERSION "0.1"

GType
ephy_location_entry_get_type (void)
{
	static GType ephy_location_entry_type = 0;

	if (ephy_location_entry_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLocationEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_location_entry_class_init,
			NULL,
			NULL,
			sizeof (EphyLocationEntry),
			0,
			(GInstanceInitFunc) ephy_location_entry_init
		};

		ephy_location_entry_type = g_type_register_static (GTK_TYPE_TOOL_ITEM,
							           "EphyLocationEntry",
							           &our_info, 0);
	}

	return ephy_location_entry_type;
}

static gboolean
ephy_location_entry_set_tooltip (GtkToolItem *tool_item,
				 GtkTooltips *tooltips,
				 const char *tip_text,
				 const char *tip_private)
{
	EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (tool_item);

	g_return_val_if_fail (EPHY_IS_LOCATION_ENTRY (entry), FALSE);

	gtk_tooltips_set_tip (tooltips, entry->priv->entry, tip_text, tip_private);

	return TRUE;
}

static void
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkToolItemClass *tool_item_class = GTK_TOOL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_location_entry_finalize;

	tool_item_class->set_tooltip = ephy_location_entry_set_tooltip;

	EphyLocationEntrySignals[USER_CHANGED] = g_signal_new (
		"user_changed", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyLocationEntryClass, user_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0,
		G_TYPE_NONE);

	g_type_class_add_private (object_class, sizeof (EphyLocationEntryPrivate));
}

static void
ephy_location_entry_activation_finished (EphyLocationEntry *entry)
{
	if (entry->priv->activation_mode)
	{
		GtkWidget *toolbar;

		entry->priv->activation_mode = FALSE;

		toolbar = gtk_widget_get_ancestor (GTK_WIDGET (entry),
						   GTK_TYPE_TOOLBAR);
		gtk_widget_hide (toolbar);
	}
}

static gboolean
location_focus_out_cb (GtkWidget *leidget, GdkEventFocus *event, EphyLocationEntry *le)
{
	ephy_location_entry_activation_finished (le);

	return FALSE;
}

static void
editable_changed_cb (GtkEditable *editable, EphyLocationEntry *e)
{
	EphyLocationEntryPrivate *p = e->priv;

	if (p->user_changed)
	{
		g_signal_emit (e, EphyLocationEntrySignals[USER_CHANGED], 0);
	}
}

static gboolean
entry_button_press_cb (GtkWidget *entry, GdkEventButton *event, EphyLocationEntry *le)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
		ephy_location_entry_activate (le);
		return TRUE;
	}

	return FALSE;
}

static void
add_to_history (EphyLocationEntry *le, const char *text)
{
	GPtrArray *children;
	int i, l, n_items, index = -1;
	int *order;

	/* check if it already exists */
	children = ephy_node_get_children (le->priv->combo_root);
	n_items = children->len;
        for (i = 0; i < n_items; i++)
        {
                 EphyNode *kid;
                 const char *node_text;
                                                                                                                             
                 kid = g_ptr_array_index (children, i);
                 node_text = ephy_node_get_property_string
			(kid, EPHY_NODE_LOC_HISTORY_PROP_TEXT);
 
                 if (strcmp (text, node_text) == 0)
                 {
			index = i;
			break;
                 }
        }
        ephy_node_thaw (le->priv->combo_root);

	/* it doesnt exist, add it */
	if (index < 0)
	{
		GValue value = { 0, };
		EphyNode *node;

		node = ephy_node_new (le->priv->combo_db);
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, text);
	        ephy_node_set_property (node, EPHY_NODE_LOC_HISTORY_PROP_TEXT,
	                                &value);

		if (n_items >= MAX_LOC_HISTORY_ITEMS)
		{
			EphyNode *last;

			last = ephy_node_get_nth_child
				(le->priv->combo_root, n_items - 1);
			ephy_node_remove_child (le->priv->combo_root, last);
		}

		ephy_node_add_child (le->priv->combo_root, node);
	}

	/* move it at the top */
	n_items = ephy_node_get_n_children (le->priv->combo_root);
	order = g_new0 (int, n_items);
	l = 1;
	if (index == -1) index = n_items -1;
	for (i = 0; i < n_items; i++)
	{
		if (index != i)
		{
			order[i] = l;
			l++;
		}
		else
		{
			order[i] = 0;
		}
	}
	ephy_node_reorder_children (le->priv->combo_root, order);
	g_free (order);
}

static void
entry_activate_cb (GtkEntry *entry, EphyLocationEntry *le)
{
	char *content;

	content = gtk_editable_get_chars (GTK_EDITABLE(entry), 0, -1);
	if (content)
	{
		add_to_history (le, content);
		g_free (content);
	}
}

static gboolean
completion_func (GtkEntryCompletion *completion,
                 const char *key,
		 GtkTreeIter *iter,
		 gpointer data)
{
	int i;
	char *item = NULL;
	char *keywords = NULL;
	char *normalized_string, *normalized_keywords;
	char *case_normalized_string, *case_normalized_keywords;
	gboolean ret = FALSE;
	EphyLocationEntry *le = EPHY_LOCATION_ENTRY (data);
	GtkTreeModel *model;

	model = gtk_entry_completion_get_model (completion);

	gtk_tree_model_get (model, iter,
			    le->priv->text_col, &item, -1);
	gtk_tree_model_get (model, iter,
			    le->priv->keywords_col, &keywords, -1);

	normalized_string = g_utf8_normalize (item, -1, G_NORMALIZE_ALL);
	case_normalized_string = g_utf8_casefold (normalized_string, -1);
	normalized_keywords = g_utf8_normalize (keywords, -1, G_NORMALIZE_ALL);
	case_normalized_keywords = g_utf8_casefold (normalized_keywords, -1);

	if (!strncmp (key, case_normalized_string, strlen (key)))
	{
		ret = TRUE;
	}
	else if (strstr (case_normalized_keywords, key))
	{
		ret = TRUE;
	}
	else
	{
		for (i = 0; i < n_web_prefixes; i++)
		{
			char *key_prefixed;

			key_prefixed = g_strconcat (web_prefixes[i], key, NULL);

			if (!strncmp (key_prefixed, case_normalized_string,
				      strlen (key_prefixed)))
			{
				ret = TRUE;
				break;
			}

			g_free (key_prefixed);
		}
	}

	g_free (item);
	g_free (normalized_string);
	g_free (case_normalized_string);

	return ret;
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel *model,
		   GtkTreeIter *iter,
		   EphyLocationEntry *le)
{
	char *item = NULL;

	gtk_tree_model_get (model, iter,
			    le->priv->action_col, &item, -1);

	ephy_location_entry_set_location (le, item);
	g_signal_emit_by_name (le->priv->entry, "activate");

	g_free (item);

	return TRUE;
}

static void
ephy_location_entry_construct_contents (EphyLocationEntry *le)
{
	EphyLocationEntryPrivate *p = le->priv;
	int combo_text_col;

	LOG ("EphyLocationEntry constructing contents %p", le)

	p->combo_model = ephy_tree_model_node_new (p->combo_root, NULL);
	combo_text_col = ephy_tree_model_node_add_prop_column
		(p->combo_model, G_TYPE_STRING, EPHY_NODE_LOC_HISTORY_PROP_TEXT);

	p->combo = gtk_combo_box_entry_new (GTK_TREE_MODEL (p->combo_model),
					    combo_text_col);
	gtk_container_add (GTK_CONTAINER (le), p->combo);
	gtk_widget_show (p->combo);
	p->entry = GTK_BIN (p->combo)->child;

	g_signal_connect (p->entry, "button_press_event",
			  G_CALLBACK (entry_button_press_cb), le);
	g_signal_connect (p->entry, "changed",
			  G_CALLBACK (editable_changed_cb), le);
	g_signal_connect (p->entry, "activate",
			  G_CALLBACK (entry_activate_cb), le);
}

static void
ephy_location_entry_init (EphyLocationEntry *le)
{
	EphyLocationEntryPrivate *p;

	LOG ("EphyLocationEntry initialising %p", le)

	p = EPHY_LOCATION_ENTRY_GET_PRIVATE (le);
	le->priv = p;

	p->user_changed = TRUE;
	p->activation_mode = FALSE;
	p->combo_db = ephy_node_db_new ("NodeDB");
	p->combo_root = ephy_node_new_with_id
		(p->combo_db, LOCATION_HISTORY_NODE_ID);
	p->xml_file = g_build_filename (ephy_dot_dir (),
                                        "ephy-location-history.xml",
                                        NULL);
	ephy_node_db_load_from_file (p->combo_db, p->xml_file,
                                     EPHY_LOC_HISTORY_XML_ROOT,
                                     EPHY_LOC_HISTORY_XML_VERSION);

	ephy_location_entry_construct_contents (le);

	gtk_tool_item_set_expand (GTK_TOOL_ITEM (le), TRUE);

	g_signal_connect (le->priv->entry,
	       		  "focus_out_event",
                          G_CALLBACK (location_focus_out_cb), le);
}

static void
save_location_history (EphyLocationEntry *le)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GPtrArray *children;
	int i;

	xmlIndentTreeOutput = TRUE;

	doc = xmlNewDoc ("1.0");
	root = xmlNewDocNode (doc, NULL, EPHY_LOC_HISTORY_XML_ROOT, NULL);
	xmlSetProp (root, "version", EPHY_LOC_HISTORY_XML_VERSION);
	xmlDocSetRootElement (doc, root);
        
	children = ephy_node_get_children (le->priv->combo_root);
	for (i = 0; i < children->len; i++)
        {
                EphyNode *kid;
                                                                                                                             
                kid = g_ptr_array_index (children, i);
                                                                                                                             
		ephy_node_save_to_xml (kid, root);
        }
        ephy_node_thaw (le->priv->combo_root);

	ephy_file_save_xml (le->priv->xml_file, doc);
	xmlFreeDoc(doc);
}

static void
ephy_location_entry_finalize (GObject *o)
{
	EphyLocationEntry *le;
	
	le = EPHY_LOCATION_ENTRY (o);

	save_location_history (le);
	g_free (le->priv->xml_file);

	LOG ("EphyLocationEntry finalized")

	G_OBJECT_CLASS (parent_class)->finalize (o);
}

GtkWidget *
ephy_location_entry_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

static gint
sort_func (GtkTreeModel *model,
	   GtkTreeIter *a,
	   GtkTreeIter *b,
	   gpointer data)
{
	gint valuea, valueb;
	EphyLocationEntry *le = EPHY_LOCATION_ENTRY (data);

	gtk_tree_model_get (model, a,
			    le->priv->relevance_col, &valuea, -1);
	gtk_tree_model_get (model, b,
			    le->priv->relevance_col, &valueb, -1);

	return valueb - valuea;
}

void
ephy_location_entry_set_completion (EphyLocationEntry *le,
				    GtkTreeModel *model,
				    guint text_col,
				    guint action_col,
				    guint keywords_col,
				    guint relevance_col)
{
	GtkTreeModel *sort_model;
	GtkEntryCompletion *completion;
	GtkCellRenderer *cell;

	le->priv->text_col = text_col;
	le->priv->action_col = action_col;
	le->priv->keywords_col = keywords_col;
	le->priv->relevance_col = relevance_col;

	sort_model = gtk_tree_model_sort_new_with_model (model);
	g_object_unref (model);
	gtk_tree_sortable_set_default_sort_func
		(GTK_TREE_SORTABLE (sort_model),
		 sort_func, le, NULL);

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, sort_model);
	g_object_unref (sort_model);
	gtk_entry_completion_set_match_func (completion, completion_func, le, NULL);
	g_signal_connect (completion, "match_selected",
			  G_CALLBACK (match_selected_cb), le);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
				    cell, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
				       cell, "text", text_col);

	gtk_entry_set_completion (GTK_ENTRY (le->priv->entry), completion);
}

void
ephy_location_entry_set_location (EphyLocationEntry *le,
				  const gchar *new_location)
{
	EphyLocationEntryPrivate *p = le->priv;

	g_return_if_fail (new_location != NULL);

	p->user_changed = FALSE;

	gtk_entry_set_text (GTK_ENTRY (p->entry), new_location);

	p->user_changed = TRUE;
}

const char *
ephy_location_entry_get_location (EphyLocationEntry *le)
{
	return gtk_entry_get_text (GTK_ENTRY (le->priv->entry));
}

void
ephy_location_entry_activate (EphyLocationEntry *le)
{
	GtkWidget *toplevel, *toolbar;

	toolbar = gtk_widget_get_ancestor (GTK_WIDGET (le), GTK_TYPE_TOOLBAR);

	if (!GTK_WIDGET_VISIBLE (toolbar))
	{
		le->priv->activation_mode = TRUE;

		gtk_widget_show (toolbar);
	}

	toplevel = gtk_widget_get_toplevel (le->priv->entry);

	gtk_editable_select_region (GTK_EDITABLE(le->priv->entry),
				    0, -1);
        gtk_window_set_focus (GTK_WINDOW(toplevel),
                              le->priv->entry);
}

void
ephy_location_entry_clear_history (EphyLocationEntry *le)
{
	EphyNode *node;
                                                                                                                             
	while ((node = ephy_node_get_nth_child (le->priv->combo_root, 0)) != NULL)
	{
		ephy_node_unref (node);
	}

	save_location_history (le);
}

GtkWidget *
ephy_location_entry_get_entry (EphyLocationEntry *le)
{
	return le->priv->entry;
}

