/*
 *  Copyright (C) 2002-2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright (C) 2005, 2006 Peter Harvey <pah06@uow.edu.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include "config.h"

#include "ephy-topics-entry.h"
#include "ephy-nodes-cover.h"
#include "ephy-node-common.h"
#include "ephy-bookmarks.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkentrycompletion.h>
#include <string.h>

static void ephy_topics_entry_class_init (EphyTopicsEntryClass *klass);
static void ephy_topics_entry_init (EphyTopicsEntry *editor);

#define EPHY_TOPICS_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPICS_ENTRY, EphyTopicsEntryPrivate))

struct _EphyTopicsEntryPrivate
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	GtkListStore *store;
	GtkEntryCompletion *completion;
	gboolean update_keywords;
	char *input;
	char *key;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK
};

static GObjectClass *parent_class = NULL;

GType
ephy_topics_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyTopicsEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_topics_entry_class_init,
			NULL,
			NULL,
			sizeof (EphyTopicsEntry),
			0,
			(GInstanceInitFunc) ephy_topics_entry_init
		};

		type = g_type_register_static (GTK_TYPE_ENTRY,
					       "EphyTopicsEntry",
					       &our_info, 0);
	}

	return type;
}

static void
update_widget (EphyTopicsEntry *entry)
{
	EphyTopicsEntryPrivate *priv = entry->priv;
	GtkEditable *editable = GTK_EDITABLE (entry);
	
	EphyNode *node;
	GPtrArray *children, *topics;
	GtkTreeIter iter;
	gint i, priority, pos;
	const char *title;
	char *tmp1, *tmp2;
	gboolean update_text;
	
	priv->update_keywords = FALSE;
	
	node = ephy_bookmarks_get_keywords (priv->bookmarks);
	children = ephy_node_get_children (node);
	topics = g_ptr_array_sized_new (children->len);
	
	for (i = 0; i < children->len; i++)
	{
		node = g_ptr_array_index (children, i);

		priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority != EPHY_NODE_NORMAL_PRIORITY)
		  continue;
		
		g_ptr_array_add (topics, node);
	}
	
	g_ptr_array_sort (topics, ephy_bookmarks_compare_topic_pointers);
		
	gtk_list_store_clear (priv->store);	
	update_text = !GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (entry));
	if (update_text) gtk_editable_delete_text (editable, 0, -1);
	pos = 0;
	
	for (i = 0; i < topics->len; i++)
	{
		node = g_ptr_array_index (topics, i);
		title = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		  
		if (ephy_node_has_child (node, priv->bookmark))
		{
			if (update_text)
			{
				gtk_editable_insert_text (editable, title, -1, &pos);
				gtk_editable_insert_text (editable, "; ", -1, &pos);
			}
		}

		/* We just include all topics, else we get odd behaviour after you
		 * just finish entering a topic (it gets selected, so wouldn't show) */
		tmp1 = g_utf8_casefold (title, -1);
		tmp2 = g_utf8_normalize (tmp1, -1, G_NORMALIZE_DEFAULT);
		gtk_list_store_append (priv->store, &iter);
		gtk_list_store_set (priv->store, &iter, 0, title, 1, tmp2, -1);
		g_free (tmp2);
		g_free (tmp1);
	}
	
	g_ptr_array_free (topics, TRUE);
	
	if (update_text) gtk_editable_set_position (editable, -1);

	priv->update_keywords = TRUE;
}

static void
update_input (EphyTopicsEntry *entry)
{
	GtkEditable *editable = GTK_EDITABLE (entry);
	const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
	char *input;
	gint start, end;

	if (entry->priv->input)
	{
		gtk_entry_completion_delete_action (entry->priv->completion, 0);
		g_free (entry->priv->input);
		g_free (entry->priv->key);
		entry->priv->input = 0;
		entry->priv->key = 0;
	}

	/* Find the start and end locations */
	start = gtk_editable_get_position (editable);
	while (start > 0 && text[start-1] != ';')
	{
		start--;
	}
	if(start > 0 && text[start-1] == ';' && text[start] == ' ')
	{
		start++;
	}
	end = start;
	while (text[end] && text[end] != ';')
	{
		end++;
	}
	
	/* If no text to work with, exit */
	input = g_strndup (text+start, end-start);
	g_strstrip (input);
	if (*input != 0)
	{
		entry->priv->input = input;
		
		input = g_utf8_casefold (input, -1);
		entry->priv->key = g_utf8_normalize (input, -1, G_NORMALIZE_DEFAULT);
		g_free (input);
		
		if (ephy_bookmarks_find_keyword (entry->priv->bookmarks, entry->priv->input, FALSE) == NULL)
		{
			input = g_strdup_printf (_("Create topic “%s”"), entry->priv->input);
			gtk_entry_completion_insert_action_text (entry->priv->completion, 0, input);
			g_free (input);
		}
		else
		{
			g_free (entry->priv->input);
			entry->priv->input = 0;
		}
	}
	else
	{
		g_free (input);
	}
}

static void
update_keywords (EphyTopicsEntry *entry)
{
	EphyNode *node;
	GPtrArray *children;
	const char *text;
	char **split;
        char *title, *tmp;
	gint i, j, priority;
	
	if(!entry->priv->update_keywords) return;
	
	/* Get the list of strings input by the user */
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	split = g_strsplit (text, ";", 0);
	for (i=0; split[i]; i++)
	{
		g_strstrip (split[i]);
		
		tmp = g_utf8_casefold (split[i], -1);
		g_free (split[i]);
		
		split[i] = g_utf8_normalize (tmp, -1, G_NORMALIZE_DEFAULT);
		g_free (tmp);
	}

	/* Test each keyword and set/unset as appropriate */
	node = ephy_bookmarks_get_keywords (entry->priv->bookmarks);
	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; i++)
	{
		node = g_ptr_array_index (children, i);

		priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority != EPHY_NODE_NORMAL_PRIORITY)
		  continue;
		
		text = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		tmp = g_utf8_casefold (text, -1);
		title = g_utf8_normalize (tmp, -1, G_NORMALIZE_DEFAULT);
		g_free (tmp);
		
		for (j=0; split[j]; j++)
		  if (strcmp (title, split[j]) == 0)
		    break;
		
		if (split[j])
		{
			split[j][0] = 0;
			ephy_bookmarks_set_keyword (entry->priv->bookmarks, node,
						    entry->priv->bookmark);
		}
		else
		{
			ephy_bookmarks_unset_keyword (entry->priv->bookmarks, node,
						      entry->priv->bookmark);
		}
		
		g_free (title);
	}
		  
	g_strfreev (split);
}

static void
insert_text (EphyTopicsEntry *entry,
	     const char *title)
{
	GtkEditable *editable = GTK_EDITABLE (entry);
	const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
	char *key;
	gint start, end;

	/* Find the start and end locations */
	start = gtk_editable_get_position (editable);
	while (start > 0 && text[start-1] != ';')
	{
		start--;
	}
	if(start > 0 && text[start-1] == ';' && text[start] == ' ')
	{
		start++;
	}
	end = start;
	while (text[end] && text[end] != ';')
	{
		end++;
	}
	end = end;
	if (text[end] == ';')
	{
		end++;
		if (text[end] == ' ')
		{
			end++;
		}
	}
	
	/* If we were provided with nothing, then we're meant to find a topic
	 * or create one from whatever has been entered */
	if (title == NULL)
	{
		/* If no text to work with, exit */
		key = g_strndup (text+start, end-start);
		g_strstrip (key);
		if (*key == 0)
		{
			g_free (key);
			return;
		}
	}

	/* Replace the text in the current position with the title */
	gtk_editable_delete_text (editable, start, end);
	gtk_editable_insert_text (editable, title, strlen(title), &start);
	gtk_editable_insert_text (editable, "; ", 2, &start);
	gtk_editable_set_position (editable, start);
	update_widget(entry);
}

static void
action_cb (GtkEntryCompletion *completion,
	   gint index,
	   gpointer user_data)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (gtk_entry_completion_get_entry (completion));
	char *action = g_strdup(entry->priv->input);

	if (ephy_bookmarks_find_keyword (entry->priv->bookmarks, action, FALSE) == NULL)
	{
		ephy_bookmarks_add_keyword (entry->priv->bookmarks, action);
	}
	
	insert_text (entry, action);
	g_free (action);
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel *model,
		   GtkTreeIter *iter,
		   gpointer user_data)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (gtk_entry_completion_get_entry (completion));
	char *title;
	
	gtk_tree_model_get (model, iter, 0, &title, -1);
	insert_text (entry, title);
	g_free (title);
	
	return TRUE;
}

static void
tree_changed_cb (EphyBookmarks *bookmarks,
		 EphyTopicsEntry *entry)
{
	update_widget(entry);
}

static gboolean
focus_out_cb (GtkEditable *editable,
	      GdkEventFocus *event,
	      gpointer user_data)
{
	update_widget (EPHY_TOPICS_ENTRY (editable));
	return FALSE;
}

static gboolean
match_func (GtkEntryCompletion *completion,
	    const gchar *key,
	    GtkTreeIter *iter,
	    gpointer user_data)
{
	GtkEntry *entry = gtk_entry_completion_get_entry (completion);
	GtkTreeModel *model = gtk_entry_completion_get_model (completion);
	EphyTopicsEntryPrivate *priv = EPHY_TOPICS_ENTRY(entry)->priv;
	
	gboolean result;
	char *text;

	if (priv->key == NULL) return TRUE;
	gtk_tree_model_get (model, iter, 1, &text, -1);
	if (text == NULL) return FALSE;
	result = g_str_has_prefix (text, priv->key);
	g_free (text);
	
	return result;
}

static void
ephy_topics_entry_set_property (GObject *object,
		                   guint prop_id,
		                   const GValue *value,
		                   GParamSpec *pspec)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		entry->priv->bookmarks = g_value_get_object (value);
		g_signal_connect_object (entry->priv->bookmarks, "tree-changed",
					 G_CALLBACK (tree_changed_cb), entry,
					 G_CONNECT_AFTER);
		break;
	case PROP_BOOKMARK:
		entry->priv->bookmark = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GObject *
ephy_topics_entry_constructor (GType type,
			       guint n_construct_properties,
			       GObjectConstructParam *construct_params)
{
	GObject *object;
	EphyTopicsEntry *entry;
	EphyTopicsEntryPrivate *priv;

	object = parent_class->constructor (type, n_construct_properties,
                                            construct_params);
	entry = EPHY_TOPICS_ENTRY (object);
	priv = EPHY_TOPICS_ENTRY_GET_PRIVATE (object);

	priv->store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	priv->completion = gtk_entry_completion_new ();
	priv->update_keywords = TRUE;
	
	gtk_entry_completion_set_model (priv->completion, GTK_TREE_MODEL (priv->store));
	gtk_entry_completion_set_text_column (priv->completion, 0);
	gtk_entry_completion_set_popup_completion (priv->completion, TRUE);
	gtk_entry_completion_set_popup_single_match (priv->completion, TRUE);
	gtk_entry_completion_set_match_func (priv->completion, match_func, NULL, NULL);
	gtk_entry_set_completion (GTK_ENTRY (entry), priv->completion);
	
	g_signal_connect (priv->completion, "match-selected",
			  G_CALLBACK (match_selected_cb), NULL);
	g_signal_connect (priv->completion, "action-activated",
			  G_CALLBACK (action_cb), NULL);
	
	g_signal_connect (object, "focus-out-event",
			  G_CALLBACK (focus_out_cb), NULL);
	g_signal_connect (object, "changed",
			  G_CALLBACK (update_keywords), NULL);

	g_signal_connect (object, "notify::cursor-position",
			  G_CALLBACK (update_input), NULL);
	g_signal_connect (object, "notify::text",
			  G_CALLBACK (update_input), NULL);
	
	update_widget (entry);
	
	return object;
}

static void
ephy_topics_entry_init (EphyTopicsEntry *entry)
{
	entry->priv = EPHY_TOPICS_ENTRY_GET_PRIVATE (entry);

}

static void
ephy_topics_entry_finalize (GObject *object)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (object);
	
	g_free (entry->priv->input);
	g_free (entry->priv->key);

	parent_class->finalize (object);
}

GtkWidget *
ephy_topics_entry_new (EphyBookmarks *bookmarks,
		       EphyNode *bookmark)
{
	EphyTopicsEntry *entry;

	g_assert (bookmarks != NULL);

	entry = EPHY_TOPICS_ENTRY (g_object_new
				       (EPHY_TYPE_TOPICS_ENTRY,
					"bookmarks", bookmarks,
					"bookmark", bookmark,
					NULL));

	return GTK_WIDGET (entry);
}

static void
ephy_topics_entry_class_init (EphyTopicsEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_topics_entry_set_property;
	object_class->constructor = ephy_topics_entry_constructor;
	object_class->finalize = ephy_topics_entry_finalize;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | 
							      G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_BOOKMARK,
					 g_param_spec_pointer ("bookmark",
							       "Bookmark",
							       "Bookmark",
							       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
							       G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | 
							       G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyTopicsEntryPrivate));
}
