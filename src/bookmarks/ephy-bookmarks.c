/*
 *  Copyright (C) 2002-2004 Marco Pesenti Gritti
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-bookmarks.h"
#include "ephy-file-helpers.h"
#include "ephy-embed-shell.h"
#include "ephy-shell.h"
#include "ephy-history.h"
#include "ephy-debug.h"
#include "ephy-tree-model-node.h"
#include "ephy-node-common.h"
#include "ephy-bookmarksbar-model.h"
#include "ephy-bookmarks-export.h"
#include "ephy-bookmarks-import.h"
#include "ephy-prefs.h"

#include "eel-gconf-extensions.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#define EPHY_BOOKMARKS_XML_ROOT    "ephy_bookmarks"
#define EPHY_BOOKMARKS_XML_VERSION "1.03"
#define BOOKMARKS_SAVE_DELAY (3 * 1000)
#define MAX_FAVORITES_NUM 10

#define EPHY_BOOKMARKS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKS, EphyBookmarksPrivate))

struct EphyBookmarksPrivate
{
	EphyBookmarksBarModel *toolbars_model;
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
static int n_default_bookmarks = G_N_ELEMENTS (default_bookmarks);

static const char *default_topics [] =
{
	N_("Entertainment"),
	N_("News"),
	N_("Shopping"),
	N_("Sports"),
	N_("Travel"),
	N_("Work")
};
static int n_default_topics = G_N_ELEMENTS (default_topics);

enum
{
	PROP_0,
	PROP_TOOLBARS_MODEL
};

/* Signals */
enum
{
	TREE_CHANGED,
	LAST_SIGNAL
};

static guint ephy_bookmarks_signals[LAST_SIGNAL] = { 0 };

static void ephy_bookmarks_class_init	(EphyBookmarksClass *klass);
static void ephy_bookmarks_init		(EphyBookmarks *tab);
static void ephy_bookmarks_finalize	(GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_bookmarks_get_type (void)
{
        static GType type = 0;

        if (type == 0)
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

                type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyBookmarks",
					       &our_info, 0);
        }

        return type;
}

static void
ephy_bookmarks_init_defaults (EphyBookmarks *eb)
{
	int i;

	for (i = 0; i < n_default_topics; i++)
	{
		ephy_bookmarks_add_keyword (eb, _(default_topics[i]));
	}

	for (i = 0; i < n_default_bookmarks; i++)
	{
		EphyNode *bmk;

		bmk = ephy_bookmarks_add (eb, _(default_bookmarks[i].title),
				          _(default_bookmarks[i].location));
		ephy_bookmarksbar_model_add_bookmark (eb->priv->toolbars_model, FALSE,
							    ephy_node_get_id (bmk));
	}
}

static char *
get_item_type_forward_cb (EggToolbarsModel *model,
			  GdkAtom type,
			  EggToolbarsModel *bookmarksbar_model)
{
	char *retval;

	g_signal_emit_by_name (bookmarksbar_model, "get_item_type",
			       type, &retval);

	return retval;
}

static char *
get_item_id_forward_cb (EggToolbarsModel *model,
			const char *type,
			const char *name,
			EggToolbarsModel *bookmarksbar_model)
{
	char *retval;

	g_signal_emit_by_name (bookmarksbar_model, "get_item_id",
			       type, name, &retval);

	return retval;
}

static char *
get_item_data_forward_cb (EggToolbarsModel *model,
			  const char *type,
			  const char *id,
			  EggToolbarsModel *bookmarksbar_model)
{
	char *retval;

	g_signal_emit_by_name (bookmarksbar_model, "get_item_data",
			       type, id, &retval);

	return retval;
}

EggToolbarsModel *
ephy_bookmarks_get_toolbars_model (EphyBookmarks *eb)
{
	g_return_val_if_fail (EPHY_IS_BOOKMARKS (eb), NULL);

	if (eb->priv->toolbars_model == NULL)
	{
		GObject *toolbars_model;

		eb->priv->toolbars_model = EPHY_BOOKMARKSBAR_MODEL
			(ephy_bookmarksbar_model_new (eb));

		/* forward those signals, so that bookmarks can also be on the main model */
		toolbars_model = ephy_shell_get_toolbars_model (ephy_shell, FALSE);

		g_signal_connect_after (toolbars_model, "get_item_type",
					G_CALLBACK (get_item_type_forward_cb),
					eb->priv->toolbars_model);
		g_signal_connect_after (toolbars_model, "get_item_id",
					G_CALLBACK (get_item_id_forward_cb),
					eb->priv->toolbars_model);
		g_signal_connect_after (toolbars_model, "get_item_data",
					G_CALLBACK (get_item_data_forward_cb),
					eb->priv->toolbars_model);

		if (eb->priv->init_defaults)
		{
			ephy_bookmarks_init_defaults (eb);
		}
	}

	return EGG_TOOLBARS_MODEL (eb->priv->toolbars_model);
}

static void
ephy_bookmarks_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	/* no writable properties */
	g_assert_not_reached ();
}

static void
ephy_bookmarks_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	EphyBookmarks *eb = EPHY_BOOKMARKS (object);

	switch (prop_id)
	{
		case PROP_TOOLBARS_MODEL:
			g_value_set_object (value, ephy_bookmarks_get_toolbars_model (eb));
			break;
	}
}


static void
ephy_bookmarks_class_init (EphyBookmarksClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_bookmarks_finalize;
	object_class->set_property = ephy_bookmarks_set_property;
	object_class->get_property = ephy_bookmarks_get_property;

	ephy_bookmarks_signals[TREE_CHANGED] =
		g_signal_new ("tree_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyBookmarksClass, tree_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
                                         PROP_TOOLBARS_MODEL,
                                         g_param_spec_object ("toolbars-model",
                                                              "Toolbars model",
                                                              "Toolbars model",
                                                              EPHY_TYPE_BOOKMARKSBAR_MODEL,
                                                              G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof(EphyBookmarksPrivate));
}

static void
ephy_bookmarks_save (EphyBookmarks *eb)
{

	LOG ("Saving bookmarks")

	ephy_node_db_write_to_xml_safe
		(eb->priv->db, eb->priv->xml_file,
		 EPHY_BOOKMARKS_XML_ROOT,
		 EPHY_BOOKMARKS_XML_VERSION,
		 "Do not rely on this file, it's only for internal use. Use bookmarks.rdf instead.",
		 eb->priv->keywords,
		 3, eb->priv->bookmarks, eb->priv->favorites, eb->priv->notcategorized,
		 eb->priv->bookmarks,
		 0,
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
				g_timeout_add (BOOKMARKS_SAVE_DELAY,
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
add_to_favorites (EphyBookmarks *eb, EphyNode *node, EphyHistory *eh)
{
	const char *url;
	gboolean full_menu;
	double score;

	if (ephy_node_has_child (eb->priv->favorites, node)) return FALSE;

	url = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
	score = get_history_item_score (eh, url);
	full_menu = ephy_node_get_n_children (eb->priv->favorites)
		    >= MAX_FAVORITES_NUM;
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
history_site_visited_cb (EphyHistory *gh, const char *url, EphyBookmarks *eb)
{
	EphyNode *node;

	node = ephy_bookmarks_find_bookmark (eb, url);
	if (node == NULL) return;

	add_to_favorites (eb, node, gh);
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
	for (i = children->len - 1; i >= 0; i--)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		ephy_node_remove_child (node, kid);
	}

	ephy_node_db_set_immutable (bookmarks->priv->db, was_immutable);

	ephy_bookmarks_update_favorites (bookmarks);
}

static void
history_cleared_cb (EphyHistory *history, EphyBookmarks *bookmarks)
{
	clear_favorites (bookmarks);
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
}

static void
update_bookmark_keywords (EphyBookmarks *eb, EphyNode *bookmark)
{
	GValue value = { 0, };
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

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, case_normalized_keywords);
	ephy_node_set_property (bookmark, EPHY_NODE_BMK_PROP_KEYWORDS,
			        &value);
	g_value_unset (&value);

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
	g_signal_emit (G_OBJECT (eb), ephy_bookmarks_signals[TREE_CHANGED], 0);
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

	g_signal_emit (G_OBJECT (eb), ephy_bookmarks_signals[TREE_CHANGED], 0);
}

static void
update_bookmark_editing (EphyBookmarks *eb)
{
	g_object_set (G_OBJECT (eb->priv->db),
		      "immutable", eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING),
		      NULL);
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

static void
ephy_bookmarks_init (EphyBookmarks *eb)
{
	GValue value = { 0, };
	EphyNodeDb *db;

	eb->priv = EPHY_BOOKMARKS_GET_PRIVATE (eb);
	eb->priv->toolbars_model = NULL;

	db = ephy_node_db_new (EPHY_NODE_DB_BOOKMARKS);
	eb->priv->db = db;
	eb->priv->dirty = FALSE;
	eb->priv->save_timeout_id = 0;
	eb->priv->xml_file = g_build_filename (ephy_dot_dir (),
					       "ephy-bookmarks.xml",
					       NULL);
	eb->priv->rdf_file = g_build_filename (ephy_dot_dir (),
					       "bookmarks.rdf",
					       NULL);

	/* Bookmarks */
	eb->priv->bookmarks = ephy_node_new_with_id (db, BOOKMARKS_NODE_ID);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, _("All"));
	ephy_node_set_property (eb->priv->bookmarks,
			        EPHY_NODE_KEYWORD_PROP_NAME,
			        &value);
	g_value_unset (&value);
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
	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, EPHY_NODE_ALL_PRIORITY);
	ephy_node_set_property (eb->priv->bookmarks,
			        EPHY_NODE_KEYWORD_PROP_PRIORITY,
			        &value);
	g_value_unset (&value);
	ephy_node_signal_connect_object (eb->priv->keywords,
				         EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) topics_removed_cb,
				         G_OBJECT (eb));

	ephy_node_add_child (eb->priv->keywords,
			     eb->priv->bookmarks);

	/* Favorites */
	eb->priv->favorites = ephy_node_new_with_id (db, FAVORITES_NODE_ID);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, _("Most Visited"));
	ephy_node_set_property (eb->priv->favorites,
			        EPHY_NODE_KEYWORD_PROP_NAME,
			        &value);
	g_value_unset (&value);
	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, EPHY_NODE_SPECIAL_PRIORITY);
	ephy_node_set_property (eb->priv->favorites,
				EPHY_NODE_KEYWORD_PROP_PRIORITY,
				&value);
	g_value_unset (&value);
	ephy_node_add_child (eb->priv->keywords, eb->priv->favorites);

	/* Not categorized */
	eb->priv->notcategorized = ephy_node_new_with_id (db, BMKS_NOTCATEGORIZED_NODE_ID);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, _("Not Categorized"));
	ephy_node_set_property (eb->priv->notcategorized,
			        EPHY_NODE_KEYWORD_PROP_NAME,
			        &value);
	g_value_unset (&value);
	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, EPHY_NODE_SPECIAL_PRIORITY);
	ephy_node_set_property (eb->priv->notcategorized,
				EPHY_NODE_KEYWORD_PROP_PRIORITY,
				&value);
	g_value_unset (&value);
	ephy_node_add_child (eb->priv->keywords, eb->priv->notcategorized);

	/* Smart bookmarks */
	eb->priv->smartbookmarks = ephy_node_new_with_id (db, SMARTBOOKMARKS_NODE_ID);

	if (g_file_test (eb->priv->xml_file, G_FILE_TEST_EXISTS) == FALSE
	    && g_file_test (eb->priv->rdf_file, G_FILE_TEST_EXISTS) == FALSE)
	{
		eb->priv->init_defaults = TRUE;
	}
	else if (ephy_node_db_load_from_file (eb->priv->db, eb->priv->xml_file,
					      EPHY_BOOKMARKS_XML_ROOT,
					      EPHY_BOOKMARKS_XML_VERSION) == FALSE)
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

	eel_gconf_notification_remove (eb->priv->disable_bookmark_editing_notifier_id);

	if (eb->priv->save_timeout_id != 0)
	{
		g_source_remove (eb->priv->save_timeout_id);
	}

	ephy_bookmarks_save (eb);

	ephy_node_unref (eb->priv->bookmarks);
	ephy_node_unref (eb->priv->keywords);
	ephy_node_unref (eb->priv->favorites);
	ephy_node_unref (eb->priv->notcategorized);

	g_object_unref (eb->priv->db);

	LOG ("Unref bookmarks toolbars model")
	if (eb->priv->toolbars_model != NULL)
	{
		g_object_unref (eb->priv->toolbars_model);
	}

	g_free (eb->priv->xml_file);
	g_free (eb->priv->rdf_file);

	LOG ("Bookmarks finalized")

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyBookmarks *
ephy_bookmarks_new ()
{
	EphyBookmarks *eb;

	eb = EPHY_BOOKMARKS (g_object_new (EPHY_TYPE_BOOKMARKS, NULL));

	return eb;
}

static void
update_has_smart_address (EphyBookmarks *bookmarks, EphyNode *bmk, const char *address)
{
	EphyNode *smart_bmks;
	gboolean smart = FALSE;

	smart_bmks = bookmarks->priv->smartbookmarks;

	if (address)
	{
		smart = strstr (address, "%s") != NULL;
	}

	if (smart)
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
	EphyNode *bm;
	GValue value = { 0, };

	bm = ephy_node_new (eb->priv->db);

	if (bm == NULL) return NULL;

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

	update_has_smart_address (eb, bm, url);
	update_bookmark_keywords (eb, bm);

	ephy_node_add_child (eb->priv->bookmarks, bm);
	ephy_node_add_child (eb->priv->notcategorized, bm);

	g_signal_emit (G_OBJECT (eb), ephy_bookmarks_signals[TREE_CHANGED], 0);
	ephy_bookmarks_save_delayed (eb, 0);

	return bm;
}

void
ephy_bookmarks_set_address (EphyBookmarks *eb,
			    EphyNode *bookmark,
			    const char *address)
{
	GValue value = { 0, };

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, address);
	ephy_node_set_property (bookmark, EPHY_NODE_BMK_PROP_LOCATION,
			        &value);
	g_value_unset (&value);

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

void
ephy_bookmarks_set_icon	(EphyBookmarks *eb,
			 const char *url,
			 const char *icon)
{
	EphyNode *node;
	GValue value = { 0, };

	g_return_if_fail (icon != NULL);

	node = ephy_bookmarks_find_bookmark (eb, url);
	if (node == NULL) return;

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, icon);
	ephy_node_set_property (node, EPHY_NODE_BMK_PROP_ICON,
			        &value);
	g_value_unset (&value);
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

static char *
options_find_first_option (const char *input)
{
	const char *start;

	if (!input) return NULL;
	start = strrchr (input, '{');
	if (!start || !(*start)) return NULL;
	return options_skip_spaces (start+1);
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
	const gchar *current = options_find_first_option (options_skip_spaces (options));

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
	gchar *escaped_arg;

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
	if (arg == NULL) return NULL;
	escaped_arg = gnome_vfs_escape_string (arg);

	t1 = smarturl_only;
	t2 = strstr (t1, "%s");
	g_return_val_if_fail (t2 != NULL, NULL);
	g_string_append_len (s, t1, t2 - t1);
	g_string_append (s, escaped_arg);
	t1 = t2 + 2;
	g_string_append (s, t1);
	ret = g_string_free (s, FALSE);

	g_free (arg);
	g_free (escaped_arg);
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

	key = ephy_node_new (eb->priv->db);

	if (key == NULL) return NULL;

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, name);
	ephy_node_set_property (key, EPHY_NODE_KEYWORD_PROP_NAME,
			        &value);
	g_value_unset (&value);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, EPHY_NODE_NORMAL_PRIORITY);
	ephy_node_set_property (key, EPHY_NODE_KEYWORD_PROP_PRIORITY,
			        &value);
	g_value_unset (&value);

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
		LOG ("Empty name, no keyword matches.")
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
		LOG ("Remove from categorized bookmarks")
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
		LOG ("Add to not categorized bookmarks")
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
ephy_bookmarks_get_not_categorized (EphyBookmarks *eb)
{
	return eb->priv->notcategorized;
}

EphyNode *
ephy_bookmarks_get_from_id (EphyBookmarks *eb, long id)
{
	return ephy_node_db_get_node_from_id (eb->priv->db, id);
}
