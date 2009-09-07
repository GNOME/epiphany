/*
 *  Copyright © 2002-2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2005, 2006 Peter Harvey <pah06@uow.edu.au>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "ephy-topics-entry.h"
#include "ephy-nodes-cover.h"
#include "ephy-node-common.h"
#include "ephy-bookmarks.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

static void ephy_topics_entry_class_init (EphyTopicsEntryClass *klass);
static void ephy_topics_entry_init       (EphyTopicsEntry *editor);

#define EPHY_TOPICS_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TOPICS_ENTRY, EphyTopicsEntryPrivate))

struct _EphyTopicsEntryPrivate
{
	EphyBookmarks *bookmarks;
	EphyNode *bookmark;
	GtkListStore *store;
	GtkEntryCompletion *completion;
	gboolean lock;
	char *create;
	char *key;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS,
	PROP_BOOKMARK
};

enum
{
	COLUMN_NODE,
	COLUMN_KEY,
	COLUMN_TITLE,
	COLUMNS
};

G_DEFINE_TYPE (EphyTopicsEntry, ephy_topics_entry, GTK_TYPE_ENTRY)

static EphyNode *
find_topic (EphyTopicsEntry *entry,
	    const char *key)
{
	EphyNode *node = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value = { 0, };
	gboolean valid;
	
	/* Loop through our table and set/unset topics appropriately */
	model = GTK_TREE_MODEL (entry->priv->store);
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid && node == NULL)
	{
		gtk_tree_model_get_value (model, &iter, COLUMN_KEY, &value);
		if (strcmp(g_value_get_string (&value), key) == 0)
		{
			g_value_unset (&value);
			gtk_tree_model_get_value (model, &iter, COLUMN_NODE, &value);
			node = g_value_get_pointer (&value);
		}
		g_value_unset (&value);
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return node;
}

static void
insert_text (EphyTopicsEntry *entry,
	     const char *title)
{
	GtkEditable *editable = GTK_EDITABLE (entry);

	const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
	const gchar *midpoint = g_utf8_offset_to_pointer (text, gtk_editable_get_position (editable));
	const gchar *start = g_utf8_strrchr (text, (gssize)(midpoint-text), ',');
	const gchar *end = g_utf8_strchr (midpoint, -1, ',');
	int startpos, endpos;
	
	if (start == NULL)
	  startpos = 0;
	else if (g_unichar_isspace (g_utf8_get_char (g_utf8_next_char (start))))
	  startpos = g_utf8_pointer_to_offset (text, start)+2;
	else
	  startpos = g_utf8_pointer_to_offset (text, start)+1;
	  
	if (end == NULL)
	  endpos = -1;
	else if (g_unichar_isspace (g_utf8_get_char (g_utf8_next_char (end))))
	  endpos = g_utf8_pointer_to_offset (text, end)+2;
	else
	  endpos = g_utf8_pointer_to_offset (text, end)+1;
	
	/* Replace the text in the current position with the title */
	gtk_editable_delete_text (editable, startpos, endpos);
	gtk_editable_insert_text (editable, title, strlen(title), &startpos);
	gtk_editable_insert_text (editable, ", ", 2, &startpos);
	gtk_editable_set_position (editable, startpos);
}

/* Updates the text entry and the completion model to match the database */
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
	gboolean is_focus;
	
	/* Prevent any changes to the database */
	if(priv->lock) return;
	priv->lock = TRUE;
	
	node = ephy_bookmarks_get_keywords (priv->bookmarks);
	children = ephy_node_get_children (node);
	topics = g_ptr_array_sized_new (children->len);
	
	for (i = 0; i < children->len; i++)
	{
		node = g_ptr_array_index (children, i);

		priority = ephy_node_get_property_int 
		  (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority != EPHY_NODE_NORMAL_PRIORITY)
		  continue;
		
		g_ptr_array_add (topics, node);
	}
	
	g_ptr_array_sort (topics, ephy_bookmarks_compare_topic_pointers);
	gtk_list_store_clear (priv->store);	

	g_object_get (entry, "is-focus", &is_focus, NULL);
	if (!is_focus)
	{
		gtk_editable_delete_text (editable, 0, -1);
	}
	
	for (pos = 0, i = 0; i < topics->len; i++)
	{
		node = g_ptr_array_index (topics, i);
		title = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
		  
		if (!is_focus && ephy_node_has_child (node, priv->bookmark))
		{
			if (pos > 0)
				gtk_editable_insert_text (editable, ", ", -1, &pos);
			gtk_editable_insert_text (editable, title, -1, &pos);
		}

		tmp1 = g_utf8_casefold (title, -1);
		tmp2 = g_utf8_normalize (tmp1, -1, G_NORMALIZE_DEFAULT);
		gtk_list_store_append (priv->store, &iter);
		gtk_list_store_set (priv->store, &iter, COLUMN_NODE, node,
				    COLUMN_TITLE, title, COLUMN_KEY, tmp2, -1);
		g_free (tmp2);
		g_free (tmp1);
	}
	
	
	if (!is_focus)
	{
		gtk_editable_set_position (editable, -1);
	}

	g_ptr_array_free (topics, TRUE);

	priv->lock = FALSE;
}

/* Updates the bookmarks database to match what is in the text entry */
static void
update_database (EphyTopicsEntry *entry)
{
	EphyTopicsEntryPrivate *priv = entry->priv;

	EphyNode *node;
	const char *text;
	char **split;
        char *tmp;
	gint i;
	
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value = { 0, };
	gboolean valid;

	/* Prevent any changes to the text entry or completion model */
	if(priv->lock) return;
	priv->lock = TRUE;
	
	/* Get the list of strings input by the user */
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	split = g_strsplit (text, ",", 0);
	for (i=0; split[i]; i++)
	{
		g_strstrip (split[i]);
		
		tmp = g_utf8_casefold (split[i], -1);
		g_free (split[i]);
		
		split[i] = g_utf8_normalize (tmp, -1, G_NORMALIZE_DEFAULT);
		g_free (tmp);
	}

	/* Loop through the completion model and set/unset topics appropriately */
	model = GTK_TREE_MODEL (priv->store);
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid)
	{
		gtk_tree_model_get_value (model, &iter, COLUMN_NODE, &value);
		node = g_value_get_pointer (&value);
		g_value_unset (&value);
		
		gtk_tree_model_get_value (model, &iter, COLUMN_KEY, &value);
		text = g_value_get_string (&value);
		
		for (i=0; split[i]; i++)
		  if (strcmp (text, split[i]) == 0)
		    break;
		
		if (split[i])
		{
			split[i][0] = 0;
			ephy_bookmarks_set_keyword (priv->bookmarks, node,
						    priv->bookmark);
		}
		else
		{
			ephy_bookmarks_unset_keyword (priv->bookmarks, node,
						      priv->bookmark);
		}
		
		g_value_unset (&value);
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	g_strfreev (split);

	priv->lock = FALSE;
}

/* Updates the search key and topic creation action */
static void
update_key (EphyTopicsEntry *entry)
{
	EphyTopicsEntryPrivate *priv = entry->priv;
	GtkEditable *editable = GTK_EDITABLE (entry);
	char *input;
	
	const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
	const gchar *midpoint = g_utf8_offset_to_pointer (text, gtk_editable_get_position (editable));
	const gchar *start = g_utf8_strrchr (text, (gssize)(midpoint-text), ',');
	const gchar *end = g_utf8_strchr (midpoint, -1, ',');
	
	if (start == NULL)
	  start = text;
	else if (g_unichar_isspace (g_utf8_get_char (g_utf8_next_char (start))))
	  start = g_utf8_next_char (g_utf8_next_char (start));
	else
	  start = g_utf8_next_char (start);
	  
	if (end == NULL)
	  end = text+strlen(text);

	/* If there was something we could create, then delete the action. */
	if (priv->create)
	{
		gtk_entry_completion_delete_action (priv->completion, 0);
	}
	
	g_free (priv->create);
	g_free (priv->key);
	priv->create = 0;
	priv->key = 0;

	/* Set the priv->create and priv->key appropriately. */
	if (start != end)
	{
		input = g_strndup (start, end-start);
		g_strstrip (input);
		priv->create = input;
		
		input = g_utf8_casefold (input, -1);
		priv->key = g_utf8_normalize (input, -1, G_NORMALIZE_DEFAULT);
		g_free (input);
		
		if (priv->create[0] == '\0' ||
		    find_topic (entry, priv->key) != NULL)
		{
			g_free (priv->create);
			priv->create = 0;
		}

		/* If there is something we can create, then setup the action. */
		else
		{
			input = g_strdup_printf (_("Create topic “%s”"), priv->create);
			gtk_entry_completion_insert_action_text (priv->completion, 0, input);
			g_free (input);
		}
	}
}

static gboolean
match_func (GtkEntryCompletion *completion,
	    const gchar *key,
	    GtkTreeIter *iter,
	    gpointer user_data)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (gtk_entry_completion_get_entry (completion));
	EphyTopicsEntryPrivate *priv = entry->priv;
	GtkTreeModel *model = gtk_entry_completion_get_model (completion);
	
	gboolean result;
	GValue value = { 0, };
	EphyNode *node;
	
	if (priv->key == NULL)
	{
		return FALSE;
	}
	
	/* If no node at all (this happens for unknown reasons) then don't show. */
	gtk_tree_model_get_value (model, iter, COLUMN_NODE, &value);
	node = g_value_get_pointer (&value);
	g_value_unset (&value);
	if (node == NULL)
	{
		result = FALSE;
	}

	/* If it's already selected, don't show it unless we're editing it. */
	else if (ephy_node_has_child (node, priv->bookmark))
	{
		gtk_tree_model_get_value (model, iter, COLUMN_KEY, &value);
		result = (strcmp (g_value_get_string (&value), priv->key) == 0);
		g_value_unset (&value);
	}
	
	/* If it's not selected, show it if it matches. */
	else
	{
		gtk_tree_model_get_value (model, iter, COLUMN_KEY, &value);
		result = (g_str_has_prefix (g_value_get_string (&value), priv->key));
		g_value_unset (&value);
	}
	
	return result;
}

static void
action_cb (GtkEntryCompletion *completion,
	   gint index,
	   gpointer user_data)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (gtk_entry_completion_get_entry (completion));
	EphyTopicsEntryPrivate *priv = entry->priv;
	char *title;
	
	title = g_strdup (priv->create);
	
	ephy_bookmarks_add_keyword (priv->bookmarks, title);
	update_widget (entry);
	
	insert_text (entry, title);
	g_free (title);
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel *model,
		   GtkTreeIter *iter,
		   gpointer user_data)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (gtk_entry_completion_get_entry (completion));
	GValue value = { 0, };
	
	gtk_tree_model_get_value (model, iter, COLUMN_TITLE, &value);
	insert_text (entry, g_value_get_string (&value));
	g_value_unset (&value);
	
	return TRUE;
}

static void
activate_cb (GtkEditable *editable,
	     gpointer user_data)
{	
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (editable);
	EphyTopicsEntryPrivate *priv = entry->priv;
	GtkEntryCompletion *completion = gtk_entry_get_completion (GTK_ENTRY (entry));

	GValue value = { 0, };
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	
	if (priv->key == NULL || priv->key[0] == '\0')
	{
		gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
		return;
	}
	else
	{
		gtk_entry_set_activates_default (GTK_ENTRY (entry), FALSE);
	}
	
	/* Loop through the completion model and find the first item to use, if any. */
	model = GTK_TREE_MODEL (priv->store);
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid && !match_func (completion, NULL, &iter, NULL))
	{
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	if (valid)
	{
		gtk_tree_model_get_value (model, &iter, COLUMN_TITLE, &value);
		
		/* See if there were any others. */
		valid = gtk_tree_model_iter_next (model, &iter);
		while (valid && !match_func (completion, NULL, &iter, NULL))
		{
			valid = gtk_tree_model_iter_next (model, &iter);
		}
		
		if (!valid)
		{
			insert_text (EPHY_TOPICS_ENTRY (editable), g_value_get_string (&value));
			g_value_unset (&value);
		}
	}
}

static void
tree_changed_cb (EphyBookmarks *bookmarks,
		 EphyTopicsEntry *entry)
{
	update_widget (entry);
}

static void
node_added_cb (EphyNode *parent,
	       EphyNode *child,
	       GObject *object)
{
	update_widget (EPHY_TOPICS_ENTRY (object));
}

static void
node_changed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint property_id,
		 GObject *object)
{
	update_widget (EPHY_TOPICS_ENTRY (object));
}

static void
node_removed_cb (EphyNode *parent,
		 EphyNode *child,
		 guint index,
		 GObject *object)
{
	update_widget (EPHY_TOPICS_ENTRY (object));
}

static void
ephy_topics_entry_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyTopicsEntry *entry = EPHY_TOPICS_ENTRY (object);
	EphyNode *node;

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		entry->priv->bookmarks = g_value_get_object (value);
		node = ephy_bookmarks_get_keywords (entry->priv->bookmarks);
		ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
				   (EphyNodeCallback) node_added_cb, object);
		ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
				   (EphyNodeCallback) node_changed_cb, object);
		ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
				   (EphyNodeCallback) node_removed_cb, object);
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

	object = G_OBJECT_CLASS (ephy_topics_entry_parent_class)->constructor (type,
                                                                               n_construct_properties,
                                                                               construct_params);
	entry = EPHY_TOPICS_ENTRY (object);
	priv = EPHY_TOPICS_ENTRY_GET_PRIVATE (object);

	priv->store = gtk_list_store_new (3, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);
	priv->completion = gtk_entry_completion_new ();
	
	gtk_entry_completion_set_model (priv->completion, GTK_TREE_MODEL (priv->store));
	gtk_entry_completion_set_text_column (priv->completion, COLUMN_TITLE);
	gtk_entry_completion_set_popup_completion (priv->completion, TRUE);
	gtk_entry_completion_set_popup_single_match (priv->completion, TRUE);
	gtk_entry_completion_set_match_func (priv->completion, match_func, NULL, NULL);
	gtk_entry_set_completion (GTK_ENTRY (entry), priv->completion);
	
	g_signal_connect (priv->completion, "match-selected",
			  G_CALLBACK (match_selected_cb), NULL);
	g_signal_connect (priv->completion, "action-activated",
			  G_CALLBACK (action_cb), NULL);
	
	g_signal_connect (object, "activate",
			  G_CALLBACK (activate_cb), NULL);
	
	g_signal_connect (object, "changed",
			  G_CALLBACK (update_database), NULL);
	g_signal_connect (object, "notify::is-focus",
			  G_CALLBACK (update_widget), NULL);
	g_signal_connect (object, "notify::cursor-position",
			  G_CALLBACK (update_key), NULL);
	g_signal_connect (object, "notify::text",
			  G_CALLBACK (update_key), NULL);
	
	update_key (entry);
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
	
	g_free (entry->priv->create);
	g_free (entry->priv->key);

	G_OBJECT_CLASS (ephy_topics_entry_parent_class)->finalize (object);
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
