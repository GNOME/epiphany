/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#include "ephy-bookmarks.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-history.h"

#include <string.h>
#include <libgnome/gnome-i18n.h>

//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

#define EPHY_BOOKMARKS_XML_VERSION "0.1"

#define MAX_FAVORITES_NUM 10

struct EphyBookmarksPrivate
{
	char *xml_file;
	EphyNode *bookmarks;
	EphyNode *keywords;
	EphyNode *favorites;
	EphyNode *lower_fav;
	double lower_score;
	GHashTable *bookmarks_hash;
	GStaticRWLock *bookmarks_hash_lock;
	GHashTable *favorites_hash;
	GStaticRWLock *favorites_hash_lock;
	GHashTable *keywords_hash;
	GStaticRWLock *keywords_hash_lock;
};

static void
ephy_bookmarks_class_init (EphyBookmarksClass *klass);
static void
ephy_bookmarks_init (EphyBookmarks *tab);
static void
ephy_bookmarks_finalize (GObject *object);
static void
ephy_bookmarks_autocompletion_source_init (EphyAutocompletionSourceIface *iface);

static GObjectClass *parent_class = NULL;

GType
ephy_bookmarks_get_type (void)
{
        static GType ephy_bookmarks_type = 0;

        if (ephy_bookmarks_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyBookmarksClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_bookmarks_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyBookmarks),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_bookmarks_init
                };

		static const GInterfaceInfo autocompletion_source_info =
		{
			(GInterfaceInitFunc) ephy_bookmarks_autocompletion_source_init,
			NULL,
			NULL
		};

                ephy_bookmarks_type = g_type_register_static (G_TYPE_OBJECT,
							      "EphyBookmarks",
							      &our_info, 0);

		g_type_add_interface_static (ephy_bookmarks_type,
					     EPHY_TYPE_AUTOCOMPLETION_SOURCE,
					     &autocompletion_source_info);
        }

        return ephy_bookmarks_type;
}

static void
ephy_bookmarks_autocompletion_source_set_basic_key (EphyAutocompletionSource *source,
						    const gchar *basic_key)
{
	/* nothing to do here */
}

static void
ephy_bookmarks_autocompletion_source_foreach (EphyAutocompletionSource *source,
					      const gchar *current_text,
					      EphyAutocompletionSourceForeachFunc func,
					      gpointer data)
{
	GPtrArray *children;
	int i;
	EphyBookmarks *eb = EPHY_BOOKMARKS (source);

	children = ephy_node_get_children (eb->priv->bookmarks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url, *smart_url, *title, *keywords;

		kid = g_ptr_array_index (children, i);
		url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);
		smart_url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_SMART_LOCATION);
		title = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_TITLE);
		keywords = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_KEYWORDS);

		if (smart_url == NULL ||
		    g_utf8_strlen (smart_url, -1) == 0)
		{
			smart_url = NULL;
		}

		func (source,
		      smart_url ? NULL : keywords,
		      title,
		      smart_url ? smart_url : url,
		      (smart_url != NULL),
		      TRUE, 0, data);
	}
	ephy_node_thaw (eb->priv->bookmarks);
}

static void
ephy_bookmarks_emit_data_changed (EphyBookmarks *eb)
{
	g_signal_emit_by_name (eb, "data-changed");
}

static void
ephy_bookmarks_autocompletion_source_init (EphyAutocompletionSourceIface *iface)
{
	iface->foreach = ephy_bookmarks_autocompletion_source_foreach;
	iface->set_basic_key = ephy_bookmarks_autocompletion_source_set_basic_key;
}

static void
ephy_bookmarks_class_init (EphyBookmarksClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_bookmarks_finalize;
}

static gboolean
ephy_bookmarks_clean_empty_keywords (EphyBookmarks *eb)
{
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (eb->priv->keywords);
	ephy_node_thaw (eb->priv->keywords);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (ephy_node_get_n_children (kid) == 0)
		{
			DEBUG_MSG (("Remove empty keyword: %s\n",
				   ephy_node_get_property_string (kid,
				   EPHY_NODE_KEYWORD_PROP_NAME)));
			ephy_node_unref (kid);
		}
	}

	return FALSE;
}

static void
ephy_bookmarks_load (EphyBookmarks *eb)
{
	xmlDocPtr doc;
	xmlNodePtr root, child;
	char *tmp;

	if (g_file_test (eb->priv->xml_file, G_FILE_TEST_EXISTS) == FALSE)
		return;

	doc = xmlParseFile (eb->priv->xml_file);
	g_assert (doc != NULL);

	root = xmlDocGetRootElement (doc);

	tmp = xmlGetProp (root, "version");
	g_assert (tmp != NULL && strcmp (tmp, EPHY_BOOKMARKS_XML_VERSION) == 0);
	g_free (tmp);

	for (child = root->children; child != NULL; child = child->next)
	{
		EphyNode *node;

		node = ephy_node_new_from_xml (child);
	}

	xmlFreeDoc (doc);
}

static void
ephy_bookmarks_save (EphyBookmarks *eb)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GPtrArray *children;
	int i;

	DEBUG_MSG (("Saving bookmarks\n"));

	/* save nodes to xml */
	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "ephy_bookmarks", NULL);
	xmlSetProp (root, "version", EPHY_BOOKMARKS_XML_VERSION);
	xmlDocSetRootElement (doc, root);

	children = ephy_node_get_children (eb->priv->keywords);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (kid != eb->priv->bookmarks)
		{
			ephy_node_save_to_xml (kid, root);
		}
	}
	ephy_node_thaw (eb->priv->keywords);

	children = ephy_node_get_children (eb->priv->bookmarks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		ephy_node_save_to_xml (kid, root);
	}
	ephy_node_thaw (eb->priv->bookmarks);

	xmlSaveFormatFile (eb->priv->xml_file, doc, 1);
}

static double
get_history_item_score (EphyHistory *eh, const char *page)
{
	return ephy_history_get_page_visits (eh, page);
}

static EphyNode *
compute_lower_fav (EphyNode *favorites, double *score)
{
	GPtrArray *children;
	int i;
	EphyEmbedShell *embed_shell;
	EphyHistory *history;
	EphyNode *result = NULL;

	embed_shell = ephy_shell_get_embed_shell (ephy_shell);
	history = ephy_embed_shell_get_global_history (embed_shell);

	*score = DBL_MAX;
	children = ephy_node_get_children (favorites);
	for (i = 0; i < children->len; i++)
	{
		const char *url;
		EphyNode *child;
		double item_score;

		child = g_ptr_array_index (children, i);
		url = ephy_node_get_property_string
			(child, EPHY_NODE_BMK_PROP_LOCATION);
		item_score = get_history_item_score (history, url);
		if (*score > item_score)
		{
			*score = item_score;
			result = child;
		}
	}
	ephy_node_thaw (favorites);

	if (result == NULL) *score = 0;

	return result;
}

static void
ephy_bookmarks_update_favorites (EphyBookmarks *eb)
{
	eb->priv->lower_fav = compute_lower_fav (eb->priv->favorites,
						 &eb->priv->lower_score);
}

static gboolean
add_to_favorites (EphyBookmarks *eb, EphyNode *node, EphyHistory *eh)
{
	const char *url;
	EphyNode *fav_node;
	gboolean full_menu;
	double score;

	url = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
	g_static_rw_lock_reader_lock (eb->priv->favorites_hash_lock);
	fav_node = g_hash_table_lookup (eb->priv->favorites_hash, url);
	g_static_rw_lock_reader_unlock (eb->priv->favorites_hash_lock);
	if (fav_node) return FALSE;

	score = get_history_item_score (eh, url);
	full_menu = ephy_node_get_n_children (eb->priv->favorites)
		    > MAX_FAVORITES_NUM;
	if (full_menu && score < eb->priv->lower_score) return FALSE;

	if (eb->priv->lower_fav && full_menu)
	{
		ephy_node_remove_child (eb->priv->favorites,
				        eb->priv->lower_fav);
	}

	ephy_node_add_child (eb->priv->favorites, node);
	ephy_bookmarks_update_favorites (eb);

	return TRUE;
}

static void
update_favorites_menus ()
{
	Session *session;
	GList *l;

	session = ephy_shell_get_session (ephy_shell);
	l = session_get_windows (session);

	for (; l != NULL; l = l->next)
	{
		EphyWindow *window = EPHY_WINDOW (l->data);

		ephy_window_update_control (window, FavoritesControl);
	}
}

static void
history_site_visited_cb (EphyHistory *gh, const char *url, EphyBookmarks *eb)
{
	EphyNode *node;

	g_static_rw_lock_reader_lock (eb->priv->bookmarks_hash_lock);
	node = g_hash_table_lookup (eb->priv->bookmarks_hash, url);
	g_static_rw_lock_reader_unlock (eb->priv->bookmarks_hash_lock);
	if (!node) return;

	if (add_to_favorites (eb, node, gh))
	{
		update_favorites_menus ();
	}
}

static void
ephy_setup_history_notifiers (EphyBookmarks *eb)
{
	EphyEmbedShell *embed_shell;
	EphyHistory *history;

	embed_shell = ephy_shell_get_embed_shell (ephy_shell);
	history = ephy_embed_shell_get_global_history (embed_shell);

	g_signal_connect (history, "visited",
			  G_CALLBACK (history_site_visited_cb), eb);
}

static void
keywords_added_cb (EphyNode *node,
	           EphyNode *child,
	           EphyBookmarks *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->keywords_hash_lock);

	g_hash_table_insert (eb->priv->keywords_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_KEYWORD_PROP_NAME),
			     child);

	g_static_rw_lock_writer_unlock (eb->priv->keywords_hash_lock);
}

static void
keywords_removed_cb (EphyNode *node,
		     EphyNode *child,
		     EphyBookmarks *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->keywords_hash_lock);

	g_hash_table_remove (eb->priv->keywords_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_KEYWORD_PROP_NAME));

	g_static_rw_lock_writer_unlock (eb->priv->keywords_hash_lock);
}

static void
favorites_added_cb (EphyNode *node,
	            EphyNode *child,
	            EphyBookmarks *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->favorites_hash_lock);

	g_hash_table_insert (eb->priv->favorites_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION),
			     child);

	g_static_rw_lock_writer_unlock (eb->priv->favorites_hash_lock);
}

static void
favorites_removed_cb (EphyNode *node,
		      EphyNode *child,
		      EphyBookmarks *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->favorites_hash_lock);

	g_hash_table_remove (eb->priv->favorites_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION));

	g_static_rw_lock_writer_unlock (eb->priv->favorites_hash_lock);
}

static void
bookmarks_added_cb (EphyNode *node,
	            EphyNode *child,
	            EphyBookmarks *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->bookmarks_hash_lock);

	g_hash_table_insert (eb->priv->bookmarks_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION),
			     child);

	g_static_rw_lock_writer_unlock (eb->priv->bookmarks_hash_lock);
}

static void
bookmarks_removed_cb (EphyNode *node,
		      EphyNode *child,
		      EphyBookmarks *eb)
{
	ephy_bookmarks_emit_data_changed (eb);
	g_idle_add ((GSourceFunc)ephy_bookmarks_clean_empty_keywords, eb);

	g_static_rw_lock_writer_lock (eb->priv->bookmarks_hash_lock);

	g_hash_table_remove (eb->priv->bookmarks_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION));

	g_static_rw_lock_writer_unlock (eb->priv->bookmarks_hash_lock);
}

static void
ephy_bookmarks_init (EphyBookmarks *eb)
{
	GValue value = { 0, };

        eb->priv = g_new0 (EphyBookmarksPrivate, 1);

	eb->priv->xml_file = g_build_filename (ephy_dot_dir (),
					       "bookmarks.xml",
					       NULL);

	eb->priv->bookmarks_hash = g_hash_table_new (g_str_hash,
			                             g_str_equal);
	eb->priv->bookmarks_hash_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (eb->priv->bookmarks_hash_lock);

	eb->priv->keywords_hash = g_hash_table_new (g_str_hash,
			                            g_str_equal);
	eb->priv->keywords_hash_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (eb->priv->keywords_hash_lock);

	eb->priv->favorites_hash = g_hash_table_new (g_str_hash,
			                             g_str_equal);
	eb->priv->favorites_hash_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (eb->priv->favorites_hash_lock);

	/* Bookmarks */
	eb->priv->bookmarks = ephy_node_new ();
	ephy_node_ref (eb->priv->bookmarks);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, _("All"));
	ephy_node_set_property (eb->priv->bookmarks,
			        EPHY_NODE_KEYWORD_PROP_NAME,
			        &value);
	g_value_unset (&value);
	g_signal_connect_object (G_OBJECT (eb->priv->bookmarks),
				 "child_added",
				 G_CALLBACK (bookmarks_added_cb),
				 G_OBJECT (eb),
				 0);
	g_signal_connect_object (G_OBJECT (eb->priv->bookmarks),
				 "child_removed",
				 G_CALLBACK (bookmarks_removed_cb),
				 G_OBJECT (eb),
				 0);

	/* Keywords */
	eb->priv->keywords = ephy_node_new ();
	ephy_node_ref (eb->priv->keywords);

	ephy_node_add_child (eb->priv->keywords,
			     eb->priv->bookmarks);
	g_signal_connect_object (G_OBJECT (eb->priv->keywords),
				 "child_added",
				 G_CALLBACK (keywords_added_cb),
				 G_OBJECT (eb),
				 0);
	g_signal_connect_object (G_OBJECT (eb->priv->keywords),
				 "child_removed",
				 G_CALLBACK (keywords_removed_cb),
				 G_OBJECT (eb),
				 0);

	eb->priv->favorites = ephy_node_new ();
	ephy_node_ref (eb->priv->favorites);
	g_signal_connect_object (G_OBJECT (eb->priv->favorites),
				 "child_added",
				 G_CALLBACK (favorites_added_cb),
				 G_OBJECT (eb),
				 0);
	g_signal_connect_object (G_OBJECT (eb->priv->favorites),
				 "child_removed",
				 G_CALLBACK (favorites_removed_cb),
				 G_OBJECT (eb),
				 0);

	ephy_bookmarks_load (eb);
	ephy_bookmarks_emit_data_changed (eb);

	ephy_setup_history_notifiers (eb);
	ephy_bookmarks_update_favorites (eb);
}

static void
ephy_bookmarks_finalize (GObject *object)
{
        EphyBookmarks *eb;

	g_return_if_fail (IS_EPHY_BOOKMARKS (object));

	eb = EPHY_BOOKMARKS (object);

        g_return_if_fail (eb->priv != NULL);

	ephy_bookmarks_save (eb);

	ephy_node_unref (eb->priv->bookmarks);
	ephy_node_unref (eb->priv->keywords);
	ephy_node_unref (eb->priv->favorites);

	g_hash_table_destroy (eb->priv->bookmarks_hash);
	g_static_rw_lock_free (eb->priv->bookmarks_hash_lock);
	g_hash_table_destroy (eb->priv->favorites_hash);
	g_static_rw_lock_free (eb->priv->favorites_hash_lock);
	g_hash_table_destroy (eb->priv->keywords_hash);
	g_static_rw_lock_free (eb->priv->keywords_hash_lock);

        g_free (eb->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyBookmarks *
ephy_bookmarks_new ()
{
	EphyBookmarks *tab;

	tab = EPHY_BOOKMARKS (g_object_new (EPHY_BOOKMARKS_TYPE, NULL));

	return tab;
}

EphyNode *
ephy_bookmarks_add (EphyBookmarks *eb,
		    const char *title,
		    const char *url,
		    const char *smart_url,
		    const char *keywords)
{
	EphyNode *bm;
	GValue value = { 0, };

	bm = ephy_node_new ();

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, title);
	ephy_node_set_property (bm, EPHY_NODE_BMK_PROP_TITLE,
			        &value);
	g_value_unset (&value);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, url);
	ephy_node_set_property (bm, EPHY_NODE_BMK_PROP_LOCATION,
			        &value);
	g_value_unset (&value);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, smart_url);
	ephy_node_set_property (bm, EPHY_NODE_BMK_PROP_SMART_LOCATION,
			        &value);
	g_value_unset (&value);

	ephy_bookmarks_update_keywords (eb, keywords, bm);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, keywords);
	ephy_node_set_property (bm, EPHY_NODE_BMK_PROP_KEYWORDS,
			        &value);
	g_value_unset (&value);

	ephy_node_add_child (eb->priv->bookmarks, bm);

	ephy_bookmarks_emit_data_changed (eb);

	return bm;
}

static gchar *
options_skip_spaces (const gchar *str)
{
	const gchar *ret = str;
	while (*ret && g_ascii_isspace (*ret))
	{
		++ret;
	}
	return (gchar *) ret;
}

static char *
options_find_value_end (const gchar *value_start)
{
	const gchar *value_end;
	if (*value_start == '"')
	{
		for (value_end = value_start + 1;
		     *value_end && (*value_end != '"' || *(value_end - 1) == '\\');
		     ++value_end) ;
	}
	else
	{
		for (value_end = value_start;
		     *value_end && ! (g_ascii_isspace (*value_end)
				      || *value_end == ','
				      || *value_end == ';');
		     ++value_end) ;
	}
	return (gchar *) value_end;
}

static char *
options_find_next_option (const char *current)
{
	const gchar *value_start;
	const gchar *value_end;
	const gchar *ret;
	value_start = strchr (current, '=');
	if (!value_start) return NULL;
	value_start = options_skip_spaces (value_start + 1);
	value_end = options_find_value_end (value_start);
	if (! (*value_end)) return NULL;
	for (ret = value_end + 1;
	     *ret && (g_ascii_isspace (*ret)
		      || *ret == ','
		      || *ret == ';');
	     ++ret);
	return (char *) ret;
}

/**
 * Very simple parser for option strings in the
 * form a=b,c=d,e="f g",...
 */
static gchar *
smart_url_options_get (const gchar *options, const gchar *option)
{
	gchar *ret = NULL;
	gsize optionlen = strlen (option);
	const gchar *current = options_skip_spaces (options);

	while (current)
	{
		if (!strncmp (option, current, optionlen))
		{
			if (g_ascii_isspace (*(current + optionlen)) || *(current + optionlen) == '=')
			{
				const gchar *value_start;
				const gchar *value_end;
				value_start = strchr (current + optionlen, '=');
				if (!value_start) continue;
				value_start = options_skip_spaces (value_start + 1);
				value_end = options_find_value_end (value_start);
				if (*value_start == '"') value_start++;
				if (value_end >= value_start)
				{
					ret = g_strndup (value_start, value_end - value_start);
					break;
				}
			}
		}
		current = options_find_next_option (current);
	}

	return ret;
}

static char *
get_smarturl_only (const char *smarturl)
{
	const gchar *openbrace;
	const gchar *closebrace;
	const gchar *c;

	openbrace = strchr (smarturl, '{');
	if (!openbrace) return g_strdup (smarturl);
	for (c = smarturl; c < openbrace; ++c)
	{
		if (!strchr (" \t\n", *c)) return g_strdup (smarturl);
	}

	closebrace = strchr (openbrace + 1, '}');
	if (!closebrace) return g_strdup (smarturl);

	return g_strdup (closebrace + 1);
}

char *
ephy_bookmarks_solve_smart_url (EphyBookmarks *eb,
				const char *smart_url,
				const char *content)
{
	gchar *ret;
	GString *s;
	gchar *t1;
	gchar *t2;
	gchar *encoding;
	gchar *smarturl_only;
	gchar *arg;

	g_return_val_if_fail (content != NULL, NULL);

	smarturl_only = get_smarturl_only (smart_url);

	s = g_string_new ("");

	encoding = smart_url_options_get (smart_url, "encoding");
	if (!encoding)
	{
		encoding = g_strdup ("UTF-8");
	}

	arg = g_convert (content, strlen (content),
			 encoding, "UTF-8", NULL, NULL, NULL);

	t1 = smarturl_only;
	t2 = strstr (t1, "%s");
	g_return_val_if_fail (t2 != NULL, NULL);
	g_string_append_len (s, t1, t2 - t1);
	g_string_append (s, arg);
	t1 = t2 + 2;
	g_string_append (s, t1);
	ret = s->str;
	g_string_free (s, FALSE);

	g_free (arg);
	g_free (encoding);
	g_free (smarturl_only);

	return ret;
}

EphyNode *
ephy_bookmarks_add_keyword (EphyBookmarks *eb,
			    const char *name)
{
	EphyNode *key;
	GValue value = { 0, };

	key = ephy_node_new ();

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, name);
	ephy_node_set_property (key, EPHY_NODE_KEYWORD_PROP_NAME,
			        &value);
	g_value_unset (&value);

	ephy_node_add_child (eb->priv->keywords, key);

	return key;
}

EphyNode *
ephy_bookmarks_find_keyword (EphyBookmarks *eb,
			     const char *name,
			     gboolean partial_match)
{
	EphyNode *node;

	g_return_val_if_fail (name != NULL, NULL);

	if (!partial_match)
	{
		g_static_rw_lock_reader_lock (eb->priv->keywords_hash_lock);
		node = g_hash_table_lookup (eb->priv->keywords_hash, name);
		g_static_rw_lock_reader_unlock (eb->priv->keywords_hash_lock);
	}
	else
	{
		GPtrArray *children;
		int i;

		if (g_utf8_strlen (name, -1) == 0)
		{
			DEBUG_MSG (("Empty name, no keyword matches.\n"));
			return NULL;
		}

		children = ephy_node_get_children (eb->priv->keywords);
		node = NULL;
		for (i = 0; i < children->len; i++)
		{
			 EphyNode *kid;
			 const char *key;

			 kid = g_ptr_array_index (children, i);
			 key = ephy_node_get_property_string (kid, EPHY_NODE_KEYWORD_PROP_NAME);

			 if (g_str_has_prefix (key, name) > 0)
			 {
				 node = kid;
			 }
		}
		ephy_node_thaw (eb->priv->keywords);
	}

	return node;
}

void
ephy_bookmarks_set_keyword (EphyBookmarks *eb,
			    EphyNode *keyword,
			    EphyNode *bookmark)
{
	ephy_node_add_child (keyword, bookmark);
}

void
ephy_bookmarks_unset_keyword (EphyBookmarks *eb,
			      EphyNode *keyword,
			      EphyNode *bookmark)
{
	ephy_node_remove_child (keyword, bookmark);
	ephy_bookmarks_clean_empty_keywords (eb);
}

static GList *
diff_keywords (char **ks1, char **ks2)
{
	GList *result = NULL;
	int i, j;

	for (i = 0; ks1 != NULL && ks1[i] != NULL; i++)
	{
		gboolean found = FALSE;

		DEBUG_MSG (("Diff keywords, keyword:\"%s\"\n", ks1[i]));

		for (j = 0; ks2 != NULL && ks2[j] != NULL; j++)
		{
			if (strcmp (ks1[i], ks2[j]) == 0)
			{
				found = TRUE;
			}
		}

		if (!found && g_utf8_strlen (ks1[i], -1) > 0)
		{
			result = g_list_append (result, ks1[i]);
		}
	}

	return result;
}

void
ephy_bookmarks_update_keywords (EphyBookmarks *eb,
		                const char *keywords,
		                EphyNode *node)
{
	const char *prop;
	char **ks, **old_ks = NULL;
	GList *diffs, *l;
	EphyNode *keyword;

	prop = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_KEYWORDS);
	ks = g_strsplit (keywords, " ", 10);
	if (prop != NULL)
	{
		old_ks = g_strsplit (prop, " ", 10);
	}

	diffs = diff_keywords (ks, old_ks);
	for (l = diffs; l != NULL; l = l->next)
	{
		char *word = (char *)l->data;

		keyword = ephy_bookmarks_find_keyword
			(eb, word, FALSE);

		if (!keyword)
		{
			keyword = ephy_bookmarks_add_keyword
				(eb, word);
		}

		ephy_bookmarks_set_keyword (eb, keyword, node);
	}
	g_list_free (diffs);

	diffs = diff_keywords (old_ks, ks);
	for (l = diffs; l != NULL; l = l->next)
	{
		keyword = ephy_bookmarks_find_keyword (eb,
						      (char *)l->data, FALSE);
		g_return_if_fail (keyword != NULL);

		ephy_bookmarks_unset_keyword (eb, keyword, node);
	}
	g_list_free (diffs);

	g_strfreev (ks);
	g_strfreev (old_ks);
}

EphyNode *
ephy_bookmarks_get_keywords (EphyBookmarks *eb)
{
	return eb->priv->keywords;
}

EphyNode *
ephy_bookmarks_get_bookmarks (EphyBookmarks *eb)
{
	return eb->priv->bookmarks;
}

EphyNode *
ephy_bookmarks_get_favorites (EphyBookmarks *eb)
{
	return eb->priv->favorites;
}

