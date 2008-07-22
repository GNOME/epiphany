/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-bookmarks.h"
#include "ephy-bookmarks-type-builtins.h"
#include "ephy-file-helpers.h"
#include "ephy-embed-shell.h"
#include "ephy-shell.h"
#include "ephy-history.h"
#include "ephy-debug.h"
#include "ephy-tree-model-node.h"
#include "ephy-node-common.h"
#include "ephy-bookmarks-export.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmark-properties.h"
#include "ephy-prefs.h"
#include "ephy-marshal.h"
#include "ephy-signal-accumulator.h"
#include "ephy-stock-icons.h"
#include "ephy-glib-compat.h"

#include "eel-gconf-extensions.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef ENABLE_ZEROCONF
#include <avahi-common/error.h>
#include <avahi-gobject/ga-service-browser.h>
#include <avahi-gobject/ga-service-resolver.h>
#include <avahi-gobject/ga-client.h>
#include <avahi-gobject/ga-enums.h>
#endif /* ENABLE_ZEROCONF */

#define EPHY_BOOKMARKS_XML_ROOT    "ephy_bookmarks"
#define EPHY_BOOKMARKS_XML_VERSION "1.03"
#define BOOKMARKS_SAVE_DELAY 3 /* seconds */
#define MAX_FAVORITES_NUM 10
#define UPDATE_URI_DATA_KEY "updated-uri"

#define EPHY_BOOKMARKS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKS, EphyBookmarksPrivate))

static const char zeroconf_protos[3][6] =
{
	"http",
	"https",
	"ftp"
};

struct _EphyBookmarksPrivate
{
	gboolean init_defaults;
	gboolean dirty;
	guint save_timeout_id;
	char *xml_file;
	char *rdf_file;
	EphyNodeDb *db;
	EphyNode *bookmarks;
	EphyNode *keywords;
	EphyNode *favorites;
	EphyNode *notcategorized;
	EphyNode *smartbookmarks;
	EphyNode *lower_fav;
	double lower_score;
	guint disable_bookmark_editing_notifier_id;

#ifdef ENABLE_ZEROCONF
	/* Local sites */
	EphyNode *local;
	GaClient *ga_client;
	GaServiceBrowser *browse_handles[G_N_ELEMENTS (zeroconf_protos)];
	GHashTable *resolve_handles;
#endif
};

typedef struct
{
	const char *title;
	const char *location;
} EphyBookmarksBookmarkInfo;

static const EphyBookmarksBookmarkInfo default_bookmarks [] =
{
	/* Translators you should change these links to respect your locale.
	 * For instance in .nl these should be
	 * "http://www.google.nl" and "http://www.google.nl/search?q=%s"
	 */
	{ N_("Search the web"), N_("http://www.google.com/search?q=%s&ie=UTF-8&oe=UTF-8") }
};

static const char *default_topics [] =
{
	N_("Entertainment"),
	N_("News"),
	N_("Shopping"),
	N_("Sports"),
	N_("Travel"),
	N_("Work")
};

/* Signals */
enum
{
	TREE_CHANGED,
	RESOLVE_ADDRESS,
	LAST_SIGNAL
};

static guint ephy_bookmarks_signals[LAST_SIGNAL] = { 0 };

static void ephy_bookmarks_class_init	(EphyBookmarksClass *klass);
static void ephy_bookmarks_init		(EphyBookmarks *tab);
static void ephy_bookmarks_finalize	(GObject *object);
static char *impl_resolve_address	(EphyBookmarks*, const char*, const char*);
static void ephy_local_bookmarks_start_client (EphyBookmarks *bookmarks);

G_DEFINE_TYPE_WITH_CODE (EphyBookmarks, ephy_bookmarks, G_TYPE_OBJECT,
			 volatile GType flags_type; /* work around gcc's optimiser */
			 /* make sure the flags type is known */
		      	 flags_type = EPHY_TYPE_BOOKMARK_PROPERTY;)

static void
ephy_bookmarks_init_defaults (EphyBookmarks *eb)
{
	int i;

	for (i = 0; i < G_N_ELEMENTS (default_topics); i++)
	{
		ephy_bookmarks_add_keyword (eb, _(default_topics[i]));
	}

	for (i = 0; i < G_N_ELEMENTS (default_bookmarks); i++)
	{
		EphyNode *bmk;

		bmk = ephy_bookmarks_add (eb, _(default_bookmarks[i].title),
					  _(default_bookmarks[i].location));
	}
}

static void
ephy_bookmarks_class_init (EphyBookmarksClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_bookmarks_finalize;

	klass->resolve_address = impl_resolve_address;

	ephy_bookmarks_signals[TREE_CHANGED] =
		g_signal_new ("tree-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyBookmarksClass, tree_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	ephy_bookmarks_signals[RESOLVE_ADDRESS] =
		g_signal_new ("resolve-address",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyBookmarksClass, resolve_address),
			      ephy_signal_accumulator_string, NULL,
			      ephy_marshal_STRING__STRING_STRING,
			      G_TYPE_STRING,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof(EphyBookmarksPrivate));
}

static gboolean
save_filter (EphyNode *node,
	     EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;

	return node != priv->bookmarks &&
	       node != priv->favorites &&
	       node != priv->notcategorized &&
#ifdef ENABLE_ZEROCONF
	       node != priv->local;
#else
	       TRUE;
#endif
}

#ifdef ENABLE_ZEROCONF
static gboolean
save_filter_local (EphyNode *node,
		   EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;

	return !ephy_node_has_child (priv->local, node);
}
#endif

static void
ephy_bookmarks_save (EphyBookmarks *eb)
{

	LOG ("Saving bookmarks");

	ephy_node_db_write_to_xml_safe
		(eb->priv->db,
		 (xmlChar *) eb->priv->xml_file,
		 (xmlChar *) EPHY_BOOKMARKS_XML_ROOT,
		 (xmlChar *) EPHY_BOOKMARKS_XML_VERSION,
		 (xmlChar *) "Do not rely on this file, it's only for internal use. Use bookmarks.rdf instead.",
		 eb->priv->keywords, (EphyNodeFilterFunc) save_filter, eb,
#ifdef ENABLE_ZEROCONF
		 eb->priv->bookmarks, (EphyNodeFilterFunc) save_filter_local, eb,
#else
		 eb->priv->bookmarks, NULL, eb,
#endif
		 NULL);

	/* Export bookmarks in rdf */
	ephy_bookmarks_export_rdf (eb, eb->priv->rdf_file);
}

static gboolean
save_bookmarks_delayed (EphyBookmarks *bookmarks)
{
	ephy_bookmarks_save (bookmarks);
	bookmarks->priv->dirty = FALSE;
	bookmarks->priv->save_timeout_id = 0;

	return FALSE;
}

static void
ephy_bookmarks_save_delayed (EphyBookmarks *bookmarks, int delay)
{
	if (!bookmarks->priv->dirty)
	{
		bookmarks->priv->dirty = TRUE;

		if (delay > 0)
		{
			bookmarks->priv->save_timeout_id =
				g_timeout_add_seconds (BOOKMARKS_SAVE_DELAY,
					       (GSourceFunc) save_bookmarks_delayed,
					       bookmarks);
		}
		else
		{
			bookmarks->priv->save_timeout_id =
				g_idle_add ((GSourceFunc) save_bookmarks_delayed,
					    bookmarks);
		}
	}
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
	EphyHistory *history;
	EphyNode *result = NULL;

	history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));

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
add_to_favorites (EphyBookmarks *bookmarks,
		  EphyNode *node,
		  EphyHistory *history)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	const char *url;
	gboolean full_menu;
	double score;

	if (ephy_node_db_is_immutable (priv->db)) return FALSE;
	if (ephy_node_has_child (priv->favorites, node)) return FALSE;

	url = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
	score = get_history_item_score (history, url);
	full_menu = ephy_node_get_n_children (priv->favorites)
		    >= MAX_FAVORITES_NUM;
	if (full_menu && score < priv->lower_score) return FALSE;

	if (priv->lower_fav && full_menu)
	{
		ephy_node_remove_child (priv->favorites,
					priv->lower_fav);
	}

	ephy_node_add_child (priv->favorites, node);
	ephy_bookmarks_update_favorites (bookmarks);

	return TRUE;
}

static void
history_site_visited_cb (EphyHistory *history,
			 const char *url,
			 EphyBookmarks *bookmarks)
{
	EphyNode *node;

	node = ephy_bookmarks_find_bookmark (bookmarks, url);
	if (node == NULL) return;

	add_to_favorites (bookmarks, node, history);
}

static void
clear_favorites (EphyBookmarks *bookmarks)
{
	EphyNode *node;
	GPtrArray *children;
	int i;
	gboolean was_immutable;

	/* clear the favourites */

	was_immutable = ephy_node_db_is_immutable (bookmarks->priv->db);
	ephy_node_db_set_immutable (bookmarks->priv->db, FALSE);

	node = bookmarks->priv->favorites;
	children = ephy_node_get_children (node);
	for (i = (int) children->len - 1; i >= 0; i--)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		ephy_node_remove_child (node, kid);
	}

	ephy_node_db_set_immutable (bookmarks->priv->db, was_immutable);

	ephy_bookmarks_update_favorites (bookmarks);
}

static void
history_cleared_cb (EphyHistory *history,
		    EphyBookmarks *bookmarks)
{
	clear_favorites (bookmarks);
}

static void
update_bookmark_response_cb (GtkWidget *dialog,
			     int response,
			     EphyNode *bookmark)
{
	GValue value = { 0, };
	char *to_uri;

	if (response == GTK_RESPONSE_ACCEPT)
	{
		to_uri = (char *) g_object_steal_data (G_OBJECT (dialog),
						       UPDATE_URI_DATA_KEY);
		
		g_value_init (&value, G_TYPE_STRING);
		g_value_take_string (&value, to_uri);
		ephy_node_set_property (bookmark, EPHY_NODE_BMK_PROP_LOCATION,
					&value);
		g_value_unset (&value);
	}

	gtk_widget_destroy (dialog);
}

static void
update_bookmark_destroy_cb (EphyNode *zombie,
			    GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

static void
redirect_cb (EphyHistory *history,
	      const char *from_uri,
	      const char *to_uri,
	      EphyBookmarks *eb)
{
	EphyNode *bookmark;
	GtkWidget *dialog;
	const char *title;

	bookmark = ephy_bookmarks_find_bookmark (eb, from_uri);

	/* FIXME check if there's another update-bookmark dialog up
	 * for from_uri' -> from_uri and update it accordingly
	 */
	if (bookmark == NULL) return;

	title = ephy_node_get_property_string (bookmark, EPHY_NODE_BMK_PROP_TITLE);

	dialog = gtk_message_dialog_new
			(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_NONE,
				/* translators: the %s is the title of the bookmark */
				_("Update bookmark “%s”?"),
				title);
	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		/* translators: the %s is a URL */
		_("The bookmarked page has moved to “%s”."),
		to_uri);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
				_("_Don't Update"), GTK_RESPONSE_REJECT);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
				_("_Update"), GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Update Bookmark?"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	g_object_set_data_full (G_OBJECT (dialog), UPDATE_URI_DATA_KEY,
				g_strdup (to_uri), (GDestroyNotify) g_free);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (update_bookmark_response_cb), bookmark);
	ephy_node_signal_connect_object (bookmark,
					 EPHY_NODE_DESTROY,
					 (EphyNodeCallback) update_bookmark_destroy_cb,
					 G_OBJECT (dialog));

	/* this dialogue is unexpected */
	gtk_window_set_focus_on_map (GTK_WINDOW (dialog), FALSE);

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
icon_updated_cb (EphyHistory *history,
		 const char *address,
		 const char *icon,
		 EphyBookmarks *eb)
{
	ephy_bookmarks_set_icon (eb, address, icon);
}

static void
ephy_setup_history_notifiers (EphyBookmarks *eb)
{
	EphyHistory *history;

	history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));

	if (ephy_history_is_enabled (history) == FALSE)
	{
		clear_favorites (eb);
	}

	g_signal_connect (history, "visited",
			  G_CALLBACK (history_site_visited_cb), eb);
	g_signal_connect (history, "cleared",
			  G_CALLBACK (history_cleared_cb), eb);
	g_signal_connect (history, "redirect",
			  G_CALLBACK (redirect_cb), eb);
	g_signal_connect (history, "icon-updated",
			  G_CALLBACK (icon_updated_cb), eb);
}

static void
update_bookmark_keywords (EphyBookmarks *eb, EphyNode *bookmark)
{
	GPtrArray *children;
	int i;
	GString *list;
	const char *title;
	char *normalized_keywords, *case_normalized_keywords;

	list = g_string_new (NULL);

	children = ephy_node_get_children (eb->priv->keywords);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (kid != eb->priv->notcategorized && 
		    kid != eb->priv->favorites &&
		    kid != eb->priv->bookmarks &&
#ifdef ENABLE_ZEROCONF
		    kid != eb->priv->local &&
#endif
		    ephy_node_has_child (kid, bookmark))
		{
			const char *topic;
			topic = ephy_node_get_property_string
				(kid, EPHY_NODE_KEYWORD_PROP_NAME);
			g_string_append (list, topic);
			g_string_append (list, " ");
		}
	}

	title = ephy_node_get_property_string
		(bookmark, EPHY_NODE_BMK_PROP_TITLE);
	g_string_append (list, " ");
	g_string_append (list, title);

	normalized_keywords = g_utf8_normalize (list->str, -1, G_NORMALIZE_ALL);
	case_normalized_keywords = g_utf8_casefold (normalized_keywords, -1);

	ephy_node_set_property_string (bookmark, EPHY_NODE_BMK_PROP_KEYWORDS,
				       case_normalized_keywords);

	g_string_free (list, TRUE);
	g_free (normalized_keywords);
	g_free (case_normalized_keywords);
}

static void
bookmarks_changed_cb (EphyNode *node,
		      EphyNode *child,
		      guint property_id,
		      EphyBookmarks *eb)
{
	if (property_id == EPHY_NODE_BMK_PROP_TITLE)
	{
		update_bookmark_keywords (eb, child);
	}

	ephy_bookmarks_save_delayed (eb, BOOKMARKS_SAVE_DELAY);
}

static void
bookmarks_removed_cb (EphyNode *node,
		      EphyNode *child,
		      guint old_index,
		      EphyBookmarks *eb)
{
	ephy_bookmarks_save_delayed (eb, BOOKMARKS_SAVE_DELAY);
}

static gboolean
bookmark_is_categorized (EphyBookmarks *eb, EphyNode *bookmark)
{
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (eb->priv->keywords);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (kid != eb->priv->notcategorized && 
		    kid != eb->priv->favorites &&
		    kid != eb->priv->bookmarks &&
#ifdef ENABLE_ZEROCONF
		    kid != eb->priv->local &&
#endif
		    ephy_node_has_child (kid, bookmark))
		{
			return TRUE;
		}
	}

	return FALSE;
}

static void
topics_removed_cb (EphyNode *node,
		   EphyNode *child,
		   guint old_index,
		   EphyBookmarks *eb)
{
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (child);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (!bookmark_is_categorized (eb, kid) &&
		    !ephy_node_has_child (eb->priv->notcategorized, kid))
		{
			ephy_node_add_child
				(eb->priv->notcategorized, kid);
		}

		update_bookmark_keywords (eb, kid);
	}
}

static void
update_bookmark_editing (EphyBookmarks *eb)
{
	g_object_set (G_OBJECT (eb->priv->db),
		      "immutable", eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING),
		      NULL);
}

static void
fix_hierarchy_topic (EphyBookmarks *eb,
		     EphyNode *topic)
{
	GPtrArray *children;
	EphyNode *bookmark;
	const char *name;
	char **split;
	int i, j;

	children = ephy_node_get_children (topic);
	name = ephy_node_get_property_string (topic, EPHY_NODE_KEYWORD_PROP_NAME);
	split = g_strsplit (name, "->", -1);
	
	for (i = 0; split[i]; i++)
	{
		if (split[i][0] == '\0') continue;
		
		topic = ephy_bookmarks_find_keyword (eb, split[i], FALSE);
		if (topic == NULL)
		{
			topic = ephy_bookmarks_add_keyword (eb, split[i]);
		}
		for (j = 0; j < children->len; j++)
		{
			bookmark = g_ptr_array_index (children, j);
			ephy_bookmarks_set_keyword (eb, topic, bookmark);
		}
	}
	
	g_strfreev (split);
}

static void
fix_hierarchy (EphyBookmarks *eb)
{
	GPtrArray *topics;
	EphyNode *topic;
	const char *name;
	int i;
	
	topics = ephy_node_get_children (eb->priv->keywords);
	for (i = (int)topics->len - 1; i >= 0; i--)
	{
		topic = (EphyNode *)g_ptr_array_index (topics, i);
		name = ephy_node_get_property_string
			(topic, EPHY_NODE_KEYWORD_PROP_NAME);
		if (strstr (name, "->") != NULL)
		{
			fix_hierarchy_topic (eb, topic);
			ephy_node_remove_child (eb->priv->keywords, topic);
		}
	}
}

static void
disable_bookmark_editing_notifier (GConfClient *client,
				   guint cnxn_id,
				   GConfEntry *entry,
				   EphyBookmarks *eb)
{
	update_bookmark_editing (eb);
}

static void
backup_file (const char *original_filename, const char *extension)
{
	char *template, *backup_filename;
	int result = 0;

	if (g_file_test (original_filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		return;
	}

	template = g_strconcat (original_filename, ".backup-XXXXXX", NULL);
	backup_filename = ephy_file_tmp_filename (template, extension);

	if (backup_filename != NULL)
	{
		result = rename (original_filename, backup_filename);
	}

	if (result >= 0)
	{
		g_message ("Your old bookmarks file was backed up as \"%s\".\n",
			   backup_filename);
	}
	else
	{
		g_warning ("Backup failed! Your old bookmarks file was lost.\n");
	}

	g_free (template);
	g_free (backup_filename);
}

#ifdef ENABLE_ZEROCONF

/* C&P adapted from gnome-vfs-dns-sd.c */
static GHashTable *
decode_txt_record (AvahiStringList *input_text)
{
	GHashTable *hash;
	int i;
	int len;
	char *key, *value, *end;
	char *key_dup, *value_dup;
	char *raw_txt;
	size_t raw_txt_len;

	if (!input_text)
		return NULL;

	raw_txt_len = avahi_string_list_serialize (input_text, NULL, 0);
	raw_txt = g_malloc (raw_txt_len);
	raw_txt_len = avahi_string_list_serialize (input_text, raw_txt, raw_txt_len);

	if (raw_txt == NULL)
		return NULL;
	
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	i = 0;
	while (i < raw_txt_len) {
		len = raw_txt[i++];
		
		if (i + len > raw_txt_len) {
			break;
		}
		
		if (len == 0) {
			continue;
		}
		
		key = &raw_txt[i];
		end = &raw_txt[i + len];
		i += len;

		if (*key == '=') {
			/* 6.4 - silently ignore keys starting with = */
			continue;
		}
		
		value = memchr (key, '=', len);
		if (value) {
			key_dup = g_strndup (key, value - key);
			value++; /* Skip '=' */
			value_dup = g_strndup (value, end - value);
		} else {
			key_dup = g_strndup (key, len);
			value_dup = NULL;
		}
		if (!g_hash_table_lookup_extended (hash,
						   key_dup,
						   NULL, NULL)) {
			g_hash_table_insert (hash,
					     key_dup,
					     value_dup);
		} else {
			g_free (key_dup);
			g_free (value_dup);
		}
	}

	return hash;
}

/* End of copied code */

static char *
get_id_for_response (const char *type,
		     const char *domain,
		     const char *name)
{
	/* FIXME: limit length! */
	return g_strdup_printf ("%s\1%s\1%s",
				type,
				domain,
				name);
}

typedef struct
{
	EphyBookmarks *bookmarks;
	GaServiceResolver *resolver;
	EphyNode *node;
	char *name;
	char *type;
	char *domain;
} ResolveData;

static void
resolver_found_cb (GaServiceResolver *resolver,
		   int interface,
		   GaProtocol protocol,
		   const char *name,
		   const char *type,
		   const char *domain,
		   const char *host_name,
		   const AvahiAddress *address,
		   guint16 port,
		   AvahiStringList *txt,
		   glong flags,
		   ResolveData *data)
{
	EphyBookmarks *bookmarks = data->bookmarks;
	EphyBookmarksPrivate *priv = bookmarks->priv;
	GValue value = { 0, };
	const char *path = NULL;
	char host[128];
	GHashTable *text_table;
	char *url;
	gboolean was_immutable;
	gboolean is_new_node = FALSE;
	guint i;

	LOG ("resolver_found_cb resolver %p\n", resolver);

	was_immutable = ephy_node_db_is_immutable (priv->db);
	ephy_node_db_set_immutable (priv->db, FALSE);

	/* Find the protocol */
	for (i = 0; i < G_N_ELEMENTS (zeroconf_protos); ++i)
	{
		char proto[20];

		g_snprintf (proto, sizeof (proto), "_%s._tcp", zeroconf_protos[i]);
		if (strcmp (type, proto) == 0) break;
	}
	if (i == G_N_ELEMENTS (zeroconf_protos)) return;
	
	if (address == NULL)
	{
		g_warning ("Zeroconf failed to resolve host %s", name);
		return;
	}

	text_table = decode_txt_record (txt);

	if (text_table != NULL)
	{
		path = g_hash_table_lookup (text_table, "path");
	}
	if (path == NULL || path[0] == '\0')
	{
		path = "/";
	}
	
	avahi_address_snprint (host, sizeof (host), address);

	LOG ("0conf RESOLVED type=%s domain=%s name=%s => proto=%s host=%s port=%d path=%s\n",
	     type, domain, name,
	     zeroconf_protos[i], host, port, path);

	was_immutable = ephy_node_db_is_immutable (priv->db);
	ephy_node_db_set_immutable (priv->db, FALSE);

	if (data->node == NULL)
	{
		is_new_node = TRUE;

		data->node = ephy_node_new (priv->db);
		g_assert (data->node != NULL);

		/* don't allow dragging this node */
		ephy_node_set_is_drag_source (data->node, FALSE);

		g_value_init (&value, G_TYPE_STRING);
		g_value_take_string (&value,
				     get_id_for_response (data->type, 
							  data->domain,
							  data->name));
		ephy_node_set_property (data->node, EPHY_NODE_BMK_PROP_SERVICE_ID, &value);
		g_value_unset (&value);

		/* FIXME: limit length! */
		ephy_node_set_property_string (data->node,
					       EPHY_NODE_BMK_PROP_TITLE,
					       name);

		ephy_node_set_property_boolean (data->node,
						EPHY_NODE_BMK_PROP_IMMUTABLE,
						TRUE);
	}

	/* FIXME: limit length! */
	url = g_strdup_printf ("%s://%s:%d%s", zeroconf_protos[i], host, port, path);

	g_value_init (&value, G_TYPE_STRING);
	g_value_take_string (&value, url);
	ephy_node_set_property (data->node, EPHY_NODE_BMK_PROP_LOCATION, &value);
	g_value_unset (&value);

	if (is_new_node)
	{
		ephy_node_add_child (priv->bookmarks, data->node);
		ephy_node_add_child (priv->local, data->node);
	}
	
	ephy_node_db_set_immutable (priv->db, was_immutable);

	g_hash_table_unref (text_table);
}

static void
resolver_failure_cb (GaServiceResolver *resolver,
		     GError *error,
		     ResolveData *data)
{
	LOG ("resolver_failure_cb resolver %p: %s\n", resolver, error?error->message:"(null)");

	/* Remove the node, if present */
	if (data->node != NULL)
	{	
		EphyBookmarks *bookmarks = data->bookmarks;
		EphyBookmarksPrivate *priv = bookmarks->priv;
		gboolean was_immutable;

		was_immutable = ephy_node_db_is_immutable (priv->db);
		ephy_node_db_set_immutable (priv->db, FALSE);	
		ephy_node_unref (data->node);
		data->node = NULL;
		ephy_node_db_set_immutable (priv->db, was_immutable);
	}
}

static void
resolve_data_free (ResolveData* data)
{
	if (data->resolver)
		g_object_unref (data->resolver);

	g_free (data->type);
	g_free (data->name);
	g_free (data->domain);
	g_slice_free (ResolveData, data);
}

static void
browser_new_service_cb (GaServiceBrowser *browser,
			int interface,
			GaProtocol protocol,
			const char *name,
			const char *type,
			const char *domain,
			glong flags,
			EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	ResolveData *data;
	char *node_id;
	GError *error = NULL;
	
	node_id = get_id_for_response (type, domain, name);
	
	LOG ("0conf ADD: type=%s domain=%s name=%s\n",
	     type, domain, name);
	
	if (g_hash_table_lookup (priv->resolve_handles, node_id) != NULL)
	{
		g_free (node_id);
		return;
	}

	data = g_slice_new0 (ResolveData);
	data->bookmarks = bookmarks;
	data->node = NULL;
	data->type = g_strdup (type);
	data->name = g_strdup (name);
	data->domain = g_strdup (domain);
	
	data->resolver = ga_service_resolver_new (AVAHI_IF_UNSPEC,
						  AVAHI_PROTO_UNSPEC,
						  name, type, domain,
						  AVAHI_PROTO_UNSPEC,
						  GA_LOOKUP_USE_MULTICAST);
	g_signal_connect (data->resolver, "found",
			  G_CALLBACK (resolver_found_cb), data);
	g_signal_connect (data->resolver, "failure",
			  G_CALLBACK (resolver_failure_cb), data);
	if (!ga_service_resolver_attach (data->resolver,
					 priv->ga_client,
					 &error))
	{
		g_warning ("Unable to resolve Zeroconf service %s: %s", name, error ? error->message : "(null)");
		g_clear_error (&error);
		resolve_data_free (data);
		g_free (node_id);
		return;
	}

	g_hash_table_insert (priv->resolve_handles,
			     node_id /* transfer ownership */, data);
}

static void
browser_removed_service_cb (GaServiceBrowser *browser,
			    int interface,
			    GaProtocol protocol,
			    const char *name,
			    const char *type,
			    const char *domain,
			    glong flags,
			    EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	char *node_id;
	ResolveData *data;

	node_id = get_id_for_response (type, domain, name);
	data = g_hash_table_lookup (priv->resolve_handles, node_id);
	/* shouldn't really happen, but let's play safe */
	if (!data)
	{
		g_free (node_id);
		return;
	}

	if (data->node != NULL)
	{	
		gboolean was_immutable;

		was_immutable = ephy_node_db_is_immutable (priv->db);
		ephy_node_db_set_immutable (priv->db, FALSE);	
		ephy_node_unref (data->node);
		data->node = NULL;
		ephy_node_db_set_immutable (priv->db, was_immutable);
	}

	g_hash_table_remove (priv->resolve_handles, node_id);
	g_free (node_id);
}

static void
start_browsing (GaClient *ga_client,
		EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (zeroconf_protos); ++i)
	{
		GaServiceBrowser *browser = NULL;
		char proto[20];

		g_snprintf (proto, sizeof (proto), "_%s._tcp", zeroconf_protos[i]);

		browser = ga_service_browser_new (proto);
		g_signal_connect (browser, "new-service",
				  G_CALLBACK (browser_new_service_cb), bookmarks);
		g_signal_connect (browser, "removed-service",
				  G_CALLBACK (browser_removed_service_cb), bookmarks);
		if (!ga_service_browser_attach (browser,
						ga_client,
						NULL))
		{
			g_warning ("Unable to start Zeroconf subsystem");
			g_object_unref (browser);
			return;
		}

		priv->browse_handles[i] = browser;
	}
}

static void
ga_client_state_changed_cb (GaClient *ga_client,
			    GaClientState state,
			    EphyBookmarks *bookmarks)
{
	if (state == GA_CLIENT_STATE_FAILURE)
	{
		if (avahi_client_errno (ga_client->avahi_client) == AVAHI_ERR_DISCONNECTED)
		{
                        EphyBookmarksPrivate *priv = bookmarks->priv;

                        g_object_unref (priv->ga_client);
                        priv->ga_client = NULL;

                        ephy_local_bookmarks_start_client (bookmarks);
		}
	}
	if (state == GA_CLIENT_STATE_S_RUNNING)
	{
		start_browsing (ga_client, bookmarks);
	}
}

static void
ephy_local_bookmarks_start_client (EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	GaClient *ga_client;

	ga_client = ga_client_new (GA_CLIENT_FLAG_NO_FAIL);
	g_signal_connect (ga_client, "state-changed",
			  G_CALLBACK (ga_client_state_changed_cb),
			  bookmarks);
	if (!ga_client_start (ga_client, NULL))
	{
		g_warning ("Unable to start Zeroconf subsystem");
                g_object_unref (ga_client);
		return;
	}
	priv->ga_client = ga_client;
}

static void
ephy_local_bookmarks_init (EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	priv->resolve_handles =	g_hash_table_new_full (g_str_hash, g_str_equal,
						       g_free,
						       (GDestroyNotify) resolve_data_free);
        ephy_local_bookmarks_start_client (bookmarks);
}

static void
ephy_local_bookmarks_stop (EphyBookmarks *bookmarks)
{
	EphyBookmarksPrivate *priv = bookmarks->priv;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (zeroconf_protos); ++i)
	{
		if (priv->browse_handles[i] != NULL)
		{
			g_object_unref (priv->browse_handles[i]);
			priv->browse_handles[i] = NULL;
		}
	}

	if (priv->resolve_handles != NULL)
	{
		g_hash_table_destroy (priv->resolve_handles);
		priv->resolve_handles = NULL;
	}

	if (priv->local != NULL)
	{
		ephy_node_unref (priv->local);
		priv->local = NULL;
	}
	
	if (priv->ga_client != NULL)
	{
		g_object_unref (priv->ga_client);
		priv->ga_client = NULL;
	}
}

#endif /* ENABLE_ZEROCONF */

static void
ephy_bookmarks_init (EphyBookmarks *eb)
{
	EphyNodeDb *db;

	/* Translators: The text before the "|" is context to help you
	 * decide on the correct translation. You MUST OMIT it in the
	 * translated string. */
	/* Translators: this topic contains all bookmarks */
	const char *bk_all = Q_("bookmarks|All");

	/* Translators: The text before the "|" is context to help you
	 * decide on the correct translation. You MUST OMIT it in the
	 * translated string. */
	/* Translators: this topic contains the most used bookmarks */
	const char *bk_most_visited = Q_("bookmarks|Most Visited");

	/* Translators: The text before the "|" is context to help you
	 * decide on the correct translation. You MUST OMIT it in the
	 * translated string. */
	/* Translators: this topic contains the not categorized
	   bookmarks */
	const char *bk_not_categorized = Q_("bookmarks|Not Categorized");
	
#ifdef ENABLE_ZEROCONF	
	/* Translators: The text before the "|" is context to help you
	 * decide on the correct translation. You MUST OMIT it in the
	 * translated string. */
	/* Translators: this is an automatic topic containing local
	 * websites bookmarks autodiscovered with zeroconf. */
	const char *bk_local_sites = Q_("bookmarks|Nearby Sites");
#endif

	eb->priv = EPHY_BOOKMARKS_GET_PRIVATE (eb);

	db = ephy_node_db_new (EPHY_NODE_DB_BOOKMARKS);
	eb->priv->db = db;

	eb->priv->xml_file = g_build_filename (ephy_dot_dir (),
					       "ephy-bookmarks.xml",
					       NULL);
	eb->priv->rdf_file = g_build_filename (ephy_dot_dir (),
					       "bookmarks.rdf",
					       NULL);

	/* Bookmarks */
	eb->priv->bookmarks = ephy_node_new_with_id (db, BOOKMARKS_NODE_ID);
	
	ephy_node_set_property_string (eb->priv->bookmarks,
				       EPHY_NODE_KEYWORD_PROP_NAME,
				       bk_all);
	ephy_node_signal_connect_object (eb->priv->bookmarks,
					 EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) bookmarks_removed_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->bookmarks,
					 EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback) bookmarks_changed_cb,
					 G_OBJECT (eb));

	/* Keywords */
	eb->priv->keywords = ephy_node_new_with_id (db, KEYWORDS_NODE_ID);
	ephy_node_set_property_int (eb->priv->bookmarks,
				    EPHY_NODE_KEYWORD_PROP_PRIORITY,
				    EPHY_NODE_ALL_PRIORITY);
	
	ephy_node_signal_connect_object (eb->priv->keywords,
					 EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) topics_removed_cb,
					 G_OBJECT (eb));

	ephy_node_add_child (eb->priv->keywords,
			     eb->priv->bookmarks);

	/* Favorites */
	eb->priv->favorites = ephy_node_new_with_id (db, FAVORITES_NODE_ID);
	
	
	ephy_node_set_property_string (eb->priv->favorites,
				       EPHY_NODE_KEYWORD_PROP_NAME,
				       bk_most_visited);
	
	ephy_node_set_property_int (eb->priv->favorites,
				    EPHY_NODE_KEYWORD_PROP_PRIORITY,
				    EPHY_NODE_SPECIAL_PRIORITY);
	ephy_node_add_child (eb->priv->keywords, eb->priv->favorites);

	/* Not categorized */
	eb->priv->notcategorized = ephy_node_new_with_id (db, BMKS_NOTCATEGORIZED_NODE_ID);
	
	
	ephy_node_set_property_string (eb->priv->notcategorized,
				       EPHY_NODE_KEYWORD_PROP_NAME,
				       bk_not_categorized);

	ephy_node_set_property_int (eb->priv->notcategorized,
				    EPHY_NODE_KEYWORD_PROP_PRIORITY,
				    EPHY_NODE_SPECIAL_PRIORITY);
	
	ephy_node_add_child (eb->priv->keywords, eb->priv->notcategorized);

#ifdef ENABLE_ZEROCONF
	/* Local Websites */
	eb->priv->local = ephy_node_new_with_id (db, BMKS_LOCAL_NODE_ID);

	/* don't allow drags to this topic */
	ephy_node_set_is_drag_dest (eb->priv->local, FALSE);

	
	ephy_node_set_property_string (eb->priv->local,
				       EPHY_NODE_KEYWORD_PROP_NAME,
				       bk_local_sites);
	ephy_node_set_property_int (eb->priv->local,
				    EPHY_NODE_KEYWORD_PROP_PRIORITY,
				    EPHY_NODE_SPECIAL_PRIORITY);
	
	ephy_node_add_child (eb->priv->keywords, eb->priv->local);
	ephy_local_bookmarks_init (eb);
#endif /* ENABLE_ZEROCONF */

	/* Smart bookmarks */
	eb->priv->smartbookmarks = ephy_node_new_with_id (db, SMARTBOOKMARKS_NODE_ID);

	if (g_file_test (eb->priv->xml_file, G_FILE_TEST_EXISTS) == FALSE
	    && g_file_test (eb->priv->rdf_file, G_FILE_TEST_EXISTS) == FALSE)
	{
		eb->priv->init_defaults = TRUE;
	}
	else if (ephy_node_db_load_from_file (eb->priv->db, eb->priv->xml_file,
					      (xmlChar *) EPHY_BOOKMARKS_XML_ROOT,
					      (xmlChar *) EPHY_BOOKMARKS_XML_VERSION) == FALSE)
	{
		/* save the corrupted files so the user can late try to
		 * manually recover them. See bug #128308.
		 */

		g_warning ("Could not read bookmarks file \"%s\", trying to "
			   "re-import bookmarks from \"%s\"\n",
			   eb->priv->xml_file, eb->priv->rdf_file);

		backup_file (eb->priv->xml_file, "xml");

		if (ephy_bookmarks_import_rdf (eb, eb->priv->rdf_file) == FALSE)
		{
			backup_file (eb->priv->rdf_file, "rdf");

			eb->priv->init_defaults = TRUE;
		}
	}
	
	if (eb->priv->init_defaults)
	{
		ephy_bookmarks_init_defaults (eb);
	}
	
	fix_hierarchy (eb);

	eb->priv->disable_bookmark_editing_notifier_id = eel_gconf_notification_add
		(CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING,
		 (GConfClientNotifyFunc)disable_bookmark_editing_notifier, eb);
	update_bookmark_editing (eb);

	ephy_setup_history_notifiers (eb);
	ephy_bookmarks_update_favorites (eb);
}

static void
ephy_bookmarks_finalize (GObject *object)
{
	EphyBookmarks *eb = EPHY_BOOKMARKS (object);
	EphyBookmarksPrivate *priv = eb->priv;

	eel_gconf_notification_remove (priv->disable_bookmark_editing_notifier_id);

	if (priv->save_timeout_id != 0)
	{
		g_source_remove (priv->save_timeout_id);
	}

	ephy_bookmarks_save (eb);

#ifdef ENABLE_ZEROCONF
	ephy_local_bookmarks_stop (eb);
#endif

	ephy_node_unref (priv->bookmarks);
	ephy_node_unref (priv->keywords);
	ephy_node_unref (priv->favorites);
	ephy_node_unref (priv->notcategorized);

	g_object_unref (priv->db);

	g_free (priv->xml_file);
	g_free (priv->rdf_file);

	LOG ("Bookmarks finalized");

	G_OBJECT_CLASS (ephy_bookmarks_parent_class)->finalize (object);
}

EphyBookmarks *
ephy_bookmarks_new (void)
{
	return EPHY_BOOKMARKS (g_object_new (EPHY_TYPE_BOOKMARKS, NULL));
}

static void
update_has_smart_address (EphyBookmarks *bookmarks, EphyNode *bmk, const char *address)
{
	EphyNode *smart_bmks;
	gboolean smart = FALSE, with_options = FALSE;

	smart_bmks = bookmarks->priv->smartbookmarks;

	if (address)
	{
		smart = strstr (address, "%s") != NULL;
		with_options = strstr (address, "%s%{") != NULL;
	}

	/* if we have a smart bookmark with width specification,
	 * remove from smart bookmarks first to force an update
	 * in the toolbar
	 */
	if (smart && with_options)
	{
		if (ephy_node_has_child (smart_bmks, bmk))
		{
			ephy_node_remove_child (smart_bmks, bmk);
		}
		ephy_node_add_child (smart_bmks, bmk);
	}
	else if (smart)
	{
		if (!ephy_node_has_child (smart_bmks, bmk))
		{
			ephy_node_add_child (smart_bmks, bmk);
		}
	}
	else
	{
		if (ephy_node_has_child (smart_bmks, bmk))
		{
			ephy_node_remove_child (smart_bmks, bmk);
		}
	}
}

EphyNode *
ephy_bookmarks_add (EphyBookmarks *eb,
		    const char *title,
		    const char *url)
{
	EphyHistory *history;
	EphyNode *bm;

	bm = ephy_node_new (eb->priv->db);

	if (bm == NULL) return NULL;
	
	if (url == NULL) return NULL;
	ephy_node_set_property_string (bm, EPHY_NODE_BMK_PROP_LOCATION, url);
	
	if (title == NULL || title[0] == '\0')
	{
		title = _("Untitled");
	}
	ephy_node_set_property_string (bm, EPHY_NODE_BMK_PROP_TITLE, title);

	history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
	if (history != NULL)
	{
		const char *icon = ephy_history_get_icon (history, url);
		if (icon != NULL)
		{
			ephy_node_set_property_string
				(bm, EPHY_NODE_BMK_PROP_ICON, icon);
		}
	}

	update_has_smart_address (eb, bm, url);
	update_bookmark_keywords (eb, bm);

	ephy_node_add_child (eb->priv->bookmarks, bm);
	ephy_node_add_child (eb->priv->notcategorized, bm);

	ephy_bookmarks_save_delayed (eb, 0);

	return bm;
}

void
ephy_bookmarks_set_address (EphyBookmarks *eb,
			    EphyNode *bookmark,
			    const char *address)
{
	ephy_node_set_property_string (bookmark, EPHY_NODE_BMK_PROP_LOCATION,
				       address);

	update_has_smart_address (eb, bookmark, address);
}

EphyNode *
ephy_bookmarks_find_bookmark (EphyBookmarks *eb,
			      const char *url)
{
	GPtrArray *children;
	int i;

	g_return_val_if_fail (EPHY_IS_BOOKMARKS (eb), NULL);
	g_return_val_if_fail (eb->priv->bookmarks != NULL, NULL);
	g_return_val_if_fail (url != NULL, NULL);

	children = ephy_node_get_children (eb->priv->bookmarks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *location;

		kid = g_ptr_array_index (children, i);
		location = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);

		if (location != NULL && strcmp (url, location) == 0)
		{
			return kid;
		}
	}

	return NULL;
}

static gboolean
is_similar (const char *url1, const char *url2)
{
	int i, j;
	
	for (i = 0; url1[i]; i++)
	  if (url1[i] == '#' || url1[i] == '?')
	    break;
	while(i>0 && url1[i] == '/')
	  i--;
	
	for (j = 0; url2[j]; j++)
	  if (url2[j] == '#' || url2[j] == '?')
	    break;
	while(j>0 && url2[j] == '/') 
	  j--;
	
	if (i != j) return FALSE;
	if (strncmp (url1, url2, i) != 0) return FALSE;
	return TRUE;
}

gint
ephy_bookmarks_get_similar (EphyBookmarks *eb,
			    EphyNode *bookmark,
			    GPtrArray *identical,
			    GPtrArray *similar)
{
	GPtrArray *children;
	const char *url;
	int i, result;

	g_return_val_if_fail (EPHY_IS_BOOKMARKS (eb), -1);
	g_return_val_if_fail (eb->priv->bookmarks != NULL, -1);
	g_return_val_if_fail (bookmark != NULL, -1);
	
	url = ephy_node_get_property_string 
	  (bookmark, EPHY_NODE_BMK_PROP_LOCATION);

	g_return_val_if_fail (url != NULL, -1);
	
	result = 0;
	
	children = ephy_node_get_children (eb->priv->bookmarks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *location;

		kid = g_ptr_array_index (children, i);
		if (kid == bookmark)
		{
			continue;
		}
		
		location = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);

		if (location != NULL)
		{
			if(identical != NULL && strcmp (url, location) == 0)
			{
				g_ptr_array_add (identical, kid);
				result++;
			}
			else if(is_similar (url, location))
			{
				if (similar != NULL)
				{
					g_ptr_array_add (similar, kid);
				}
				result++;
			}
		}
	}
	
	return result;
}

void
ephy_bookmarks_set_icon	(EphyBookmarks *eb,
			 const char *url,
			 const char *icon)
{
	EphyNode *node;

	g_return_if_fail (icon != NULL);

	node = ephy_bookmarks_find_bookmark (eb, url);
	if (node == NULL) return;

	ephy_node_set_property_string (node, EPHY_NODE_BMK_PROP_ICON, icon);
}


void
ephy_bookmarks_set_usericon (EphyBookmarks *eb,
			     const char *url,
			     const char *icon)
{
	EphyNode *node;

	g_return_if_fail (icon != NULL);

	node = ephy_bookmarks_find_bookmark (eb, url);
	if (node == NULL) return;

	ephy_node_set_property_string (node, EPHY_NODE_BMK_PROP_USERICON,
				       icon);
}


/* name must end with '=' */
static char *
get_option (char *start,
	    const char *name,
	    char **optionsend)
{
	char *end, *p;

	*optionsend = start;

	if (start == NULL || start[0] != '%' || start[1] != '{') return NULL;
	start += 2;

	end = strstr (start, "}");
	if (end == NULL) return NULL;
	*optionsend = end + 1;

	start = strstr (start, name);
	if (start == NULL || start > end) return NULL;
	start += strlen (name);

	/* Find end of option, either ',' or '}' */
	end = strstr (start, ",");
	if (end == NULL || end >= *optionsend) end = *optionsend - 1;

	/* limit option length and sanity-check it */
	if (end - start > 32) return NULL;
	for (p = start; p < end; ++p)
	{
		if (!g_ascii_isalnum (*p)) return NULL;
	}

	return g_strndup (start, end - start);
}

static char *
impl_resolve_address (EphyBookmarks *eb,
		      const char *address,
		      const char *content)
{
	GString *result;
	char *pos, *oldpos, *arg, *escaped_arg, *encoding, *optionsend;

	if (address == NULL) return NULL;
	if (content == NULL) content = "";

	result = g_string_new_len (NULL, strlen (content) + strlen (address));

	/* substitute %s's */
	oldpos = (char*) address;
	while ((pos = strstr (oldpos, "%s")) != NULL)
	{
		g_string_append_len (result, oldpos, pos - oldpos);
		pos += 2;

		encoding = get_option (pos, "encoding=", &optionsend);
		if (encoding != NULL)
		{
			GError *error = NULL;

			arg = g_convert (content, strlen (content), encoding,
					 "UTF-8", NULL, NULL, &error);
			if (error != NULL)
			{
				g_warning ("Error when converting arg to encoding '%s': %s\n",
					   encoding, error->message);
				g_error_free (error);
			}
			else
			{
				escaped_arg = g_uri_escape_string (arg, NULL, TRUE);
				g_string_append (result, escaped_arg);
				g_free (escaped_arg);
				g_free (arg);
			}

			g_free (encoding);
		}
		else
		{
			arg = g_uri_escape_string (content, NULL, TRUE);
			g_string_append (result, arg);
			g_free (arg);
		}

		oldpos = optionsend;
	}
	g_string_append (result, oldpos);

	return g_string_free (result, FALSE);
}

char *
ephy_bookmarks_resolve_address (EphyBookmarks *eb,
				const char *address,
				const char *parameter)
{
	char *retval = NULL;

	g_return_val_if_fail (EPHY_IS_BOOKMARKS (eb), NULL);
	g_return_val_if_fail (address != NULL, NULL);

	g_signal_emit (eb, ephy_bookmarks_signals[RESOLVE_ADDRESS], 0,
		       address, parameter, &retval);

	return retval;
}

guint
ephy_bookmarks_get_smart_bookmark_width (EphyNode *bookmark)
{
	const char *url;
	char *option, *end, *number;
	guint width;

	url = ephy_node_get_property_string (bookmark, EPHY_NODE_BMK_PROP_LOCATION);
	if (url == NULL) return 0;

	/* this takes the first %s, but that's okay since we only support one text entry box */
	option = strstr (url, "%s%{");
	if (option == NULL) return 0;
	option += 2;

	number = get_option (option, "width=", &end);
	if (number == NULL) return 0;

	width = (guint) g_ascii_strtoull (number, NULL, 10);
	g_free (number);

	return CLAMP (width, 1, 64);
}

EphyNode *
ephy_bookmarks_add_keyword (EphyBookmarks *eb,
			    const char *name)
{
	EphyNode *key;

	key = ephy_node_new (eb->priv->db);

	if (key == NULL) return NULL;

	ephy_node_set_property_string (key, EPHY_NODE_KEYWORD_PROP_NAME,
				       name);
	ephy_node_set_property_int (key, EPHY_NODE_KEYWORD_PROP_PRIORITY,
				    EPHY_NODE_NORMAL_PRIORITY);
	
	ephy_node_add_child (eb->priv->keywords, key);

	return key;
}

void
ephy_bookmarks_remove_keyword (EphyBookmarks *eb,
			       EphyNode *keyword)
{
	ephy_node_remove_child (eb->priv->keywords, keyword);
}

char *
ephy_bookmarks_get_topic_uri (EphyBookmarks *eb,
			      EphyNode *node)
{
	char *uri;

	if (ephy_bookmarks_get_bookmarks (eb) == node)
	{
		uri = g_strdup ("topic://Special/All");
	}
	else if (ephy_bookmarks_get_not_categorized (eb) == node)
	{
		uri = g_strdup ("topic://Special/NotCategorized");
	}
	else if (ephy_bookmarks_get_favorites (eb) == node)
	{
		uri = g_strdup ("topic://Special/Favorites");
	}
#ifdef ENABLE_ZEROCONF
	else if (ephy_bookmarks_get_local (eb) == node)
	{
		/* Note: do not change to "Nearby" because of existing custom toolbars */
		uri = g_strdup ("topic://Special/Local");
	}
#endif
	else
	{
		const char *name;

		name = ephy_node_get_property_string
			(node, EPHY_NODE_KEYWORD_PROP_NAME);

		uri = g_strdup_printf ("topic://%s", name);
	}

	return uri;
}

EphyNode *
ephy_bookmarks_find_keyword (EphyBookmarks *eb,
			     const char *name,
			     gboolean partial_match)
{
	EphyNode *node;
	GPtrArray *children;
	int i;
	const char *topic_name;

	g_return_val_if_fail (name != NULL, NULL);

	topic_name = name;

	if (g_utf8_strlen (name, -1) == 0)
	{
		LOG ("Empty name, no keyword matches.");
		return NULL;
	}

	if (strcmp (name, "topic://Special/All") == 0)
	{
		return ephy_bookmarks_get_bookmarks (eb);
	}
	else if (strcmp (name, "topic://Special/NotCategorized") == 0)
	{
		return ephy_bookmarks_get_not_categorized (eb);
	}
	else if (strcmp (name, "topic://Special/Favorites") == 0)
	{
		return ephy_bookmarks_get_favorites (eb);
	}
#ifdef ENABLE_ZEROCONF
	else if (strcmp (name, "topic://Special/Local") == 0)
	{
		return ephy_bookmarks_get_local (eb);
	}
#endif
	else if (g_str_has_prefix (name, "topic://"))
	{
		topic_name += strlen ("topic://");
	}

	children = ephy_node_get_children (eb->priv->keywords);
	node = NULL;
	for (i = 0; i < children->len; i++)
	{
		 EphyNode *kid;
		 const char *key;

		 kid = g_ptr_array_index (children, i);
		 key = ephy_node_get_property_string (kid, EPHY_NODE_KEYWORD_PROP_NAME);

		 if ((partial_match && g_str_has_prefix (key, topic_name) > 0) ||
		     (!partial_match && strcmp (key, topic_name) == 0))
		 {
			 node = kid;
		 }
	}

	return node;
}

gboolean
ephy_bookmarks_has_keyword (EphyBookmarks *eb,
			    EphyNode *keyword,
			    EphyNode *bookmark)
{
	return ephy_node_has_child (keyword, bookmark);
}

void
ephy_bookmarks_set_keyword (EphyBookmarks *eb,
			    EphyNode *keyword,
			    EphyNode *bookmark)
{
	if (ephy_node_has_child (keyword, bookmark)) return;

	ephy_node_add_child (keyword, bookmark);

	if (ephy_node_has_child (eb->priv->notcategorized, bookmark))
	{
		LOG ("Remove from categorized bookmarks");
		ephy_node_remove_child
			(eb->priv->notcategorized, bookmark);
	}

	update_bookmark_keywords (eb, bookmark);

	g_signal_emit (G_OBJECT (eb), ephy_bookmarks_signals[TREE_CHANGED], 0);
}

void
ephy_bookmarks_unset_keyword (EphyBookmarks *eb,
			      EphyNode *keyword,
			      EphyNode *bookmark)
{
	if (!ephy_node_has_child (keyword, bookmark)) return;

	ephy_node_remove_child (keyword, bookmark);

	if (!bookmark_is_categorized (eb, bookmark) &&
	    !ephy_node_has_child (eb->priv->notcategorized, bookmark))
	{
		LOG ("Add to not categorized bookmarks");
		ephy_node_add_child
			(eb->priv->notcategorized, bookmark);
	}

	update_bookmark_keywords (eb, bookmark);

	g_signal_emit (G_OBJECT (eb), ephy_bookmarks_signals[TREE_CHANGED], 0);
}

EphyNode *
ephy_bookmarks_get_smart_bookmarks (EphyBookmarks *eb)
{
	return eb->priv->smartbookmarks;
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

EphyNode *
ephy_bookmarks_get_local (EphyBookmarks *eb)
{
#ifdef ENABLE_ZEROCONF
	return eb->priv->local;
#else
	return NULL;
#endif
}

EphyNode *
ephy_bookmarks_get_not_categorized (EphyBookmarks *eb)
{
	return eb->priv->notcategorized;
}

EphyNode *
ephy_bookmarks_get_from_id (EphyBookmarks *eb, long id)
{
	return ephy_node_db_get_node_from_id (eb->priv->db, id);
}

int
ephy_bookmarks_compare_topics (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = (EphyNode *)a;
	EphyNode *node_b = (EphyNode *)b;
	const char *title1, *title2;
	int priority1, priority2;

	priority1 = ephy_node_get_property_int (node_a, EPHY_NODE_KEYWORD_PROP_PRIORITY);
	priority2 = ephy_node_get_property_int (node_b, EPHY_NODE_KEYWORD_PROP_PRIORITY);
	
	if (priority1 > priority2) return 1;
	if (priority1 < priority2) return -1;

	title1 = ephy_node_get_property_string (node_a, EPHY_NODE_KEYWORD_PROP_NAME);
	title2 = ephy_node_get_property_string (node_b, EPHY_NODE_KEYWORD_PROP_NAME);

	if (title1 == title2) return 0;
	if (title1 == NULL) return -1;
	if (title2 == NULL) return 1;
	return g_utf8_collate (title1, title2);
}

int
ephy_bookmarks_compare_topic_pointers (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = *(EphyNode **)a;
	EphyNode *node_b = *(EphyNode **)b;
	
	return ephy_bookmarks_compare_topics (node_a, node_b);
}

int
ephy_bookmarks_compare_bookmarks (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = (EphyNode *)a;
	EphyNode *node_b = (EphyNode *)b;
	const char *title1, *title2;
	
	title1 = ephy_node_get_property_string (node_a, EPHY_NODE_BMK_PROP_TITLE);
	title2 = ephy_node_get_property_string (node_b, EPHY_NODE_BMK_PROP_TITLE);

	if (title1 == title2) return 0;
	if (title1 == NULL) return -1;
	if (title2 == NULL) return 1;
	return g_utf8_collate (title1, title2);
}

int
ephy_bookmarks_compare_bookmark_pointers (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = *(EphyNode **)a;
	EphyNode *node_b = *(EphyNode **)b;
	
	return ephy_bookmarks_compare_bookmarks (node_a, node_b);
}
