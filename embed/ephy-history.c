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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-types.h"
#include "ephy-history.h"
#include "ephy-file-helpers.h"
#include "ephy-autocompletion-source.h"
#include "ephy-debug.h"
#include "ephy-node-common.h"

#include <time.h>
#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>

#define EPHY_HISTORY_XML_VERSION "0.1"

/* how often to save the history, in milliseconds */
#define HISTORY_SAVE_INTERVAL (60 * 5 * 1000)

#define HISTORY_PAGE_OBSOLETE_DAYS 10

struct EphyHistoryPrivate
{
	char *xml_file;
	EphyNodeDb *db;
	EphyNode *hosts;
	EphyNode *pages;
	EphyNode *last_page;
	GHashTable *hosts_hash;
	GStaticRWLock *hosts_hash_lock;
	GHashTable *pages_hash;
	GStaticRWLock *pages_hash_lock;
	int autosave_timeout;
};

enum
{
        ADD,
	UPDATE,
	REMOVE,
	VISITED,
        LAST_SIGNAL
};

static void
ephy_history_class_init (EphyHistoryClass *klass);
static void
ephy_history_init (EphyHistory *tab);
static void
ephy_history_finalize (GObject *object);
static void
ephy_history_autocompletion_source_init (EphyAutocompletionSourceIface *iface);

static GObjectClass *parent_class = NULL;

static guint ephy_history_signals[LAST_SIGNAL] = { 0 };

GType
ephy_history_get_type (void)
{
        static GType ephy_history_type = 0;

        if (ephy_history_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyHistoryClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_history_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyHistory),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_history_init
                };

		static const GInterfaceInfo autocompletion_source_info =
		{
			(GInterfaceInitFunc) ephy_history_autocompletion_source_init,
			NULL,
			NULL
		};

                ephy_history_type = g_type_register_static (G_TYPE_OBJECT,
							      "EphyHistory",
							      &our_info, 0);

		g_type_add_interface_static (ephy_history_type,
					     EPHY_TYPE_AUTOCOMPLETION_SOURCE,
					     &autocompletion_source_info);
        }

        return ephy_history_type;
}

static void
ephy_history_autocompletion_source_set_basic_key (EphyAutocompletionSource *source,
						  const gchar *basic_key)
{
	/* nothing to do here */
}

static void
ephy_history_autocompletion_source_foreach (EphyAutocompletionSource *source,
					    const gchar *current_text,
					    EphyAutocompletionSourceForeachFunc func,
					    gpointer data)
{
	GPtrArray *children;
	int i;
	EphyHistory *eb = EPHY_HISTORY (source);
	GTime now;

	now = time (NULL);

	children = ephy_node_get_children (eb->priv->pages);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url, *title;
		int last_visit, visits;
		guint32 score;

		kid = g_ptr_array_index (children, i);
		g_assert (kid != NULL);

		url = ephy_node_get_property_string
			(kid, EPHY_NODE_PAGE_PROP_LOCATION);
		title = ephy_node_get_property_string
			(kid, EPHY_NODE_PAGE_PROP_TITLE);
		last_visit = ephy_node_get_property_int
			(kid, EPHY_NODE_PAGE_PROP_LAST_VISIT);
		visits = ephy_node_get_property_int
			(kid, EPHY_NODE_PAGE_PROP_VISITS);
		score = MAX (visits - ((now - last_visit) >> 15), 1);

		func (source, url,
		      url, url, FALSE,
		      FALSE, score, data);
	}
	ephy_node_thaw (eb->priv->pages);
}

static void
ephy_history_emit_data_changed (EphyHistory *eb)
{
	g_signal_emit_by_name (eb, "data-changed");
}

static void
ephy_history_autocompletion_source_init (EphyAutocompletionSourceIface *iface)
{
	iface->foreach = ephy_history_autocompletion_source_foreach;
	iface->set_basic_key = ephy_history_autocompletion_source_set_basic_key;
}

static void
ephy_history_class_init (EphyHistoryClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_history_finalize;

	ephy_history_signals[VISITED] =
                g_signal_new ("visited",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyHistoryClass, visited),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
}

static void
ephy_history_load (EphyHistory *eb)
{
	xmlDocPtr doc;
	xmlNodePtr root, child;
	char *tmp;

	if (g_file_test (eb->priv->xml_file, G_FILE_TEST_EXISTS) == FALSE)
		return;

	doc = xmlParseFile (eb->priv->xml_file);
	g_return_if_fail (doc != NULL);

	root = xmlDocGetRootElement (doc);

	tmp = xmlGetProp (root, "version");
	g_assert (tmp != NULL && strcmp (tmp, EPHY_HISTORY_XML_VERSION) == 0);
	g_free (tmp);

	for (child = root->children; child != NULL; child = child->next)
	{
		EphyNode *node;

		node = ephy_node_new_from_xml (eb->priv->db, child);
	}

	xmlFreeDoc (doc);
}

static gboolean
page_is_obsolete (EphyNode *node, GDate *now)
{
	int last_visit;
	GDate date;

	last_visit = ephy_node_get_property_int
		(node, EPHY_NODE_PAGE_PROP_LAST_VISIT);

        g_date_clear (&date, 1);
        g_date_set_time (&date, last_visit);

	return (g_date_days_between (&date, now) >=
		HISTORY_PAGE_OBSOLETE_DAYS);
}

static void
remove_obsolete_pages (EphyHistory *eb)
{
	GPtrArray *children;
	int i;
	GTime now;
	GDate current_date;

	now = time (NULL);
        g_date_clear (&current_date, 1);
        g_date_set_time (&current_date, time (NULL));

	children = ephy_node_get_children (eb->priv->pages);
	ephy_node_thaw (eb->priv->pages);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (page_is_obsolete (kid, &current_date))
		{
			ephy_node_unref (kid);
		}
	}
}

static void
ephy_history_save (EphyHistory *eb)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GPtrArray *children;
	int i;

	LOG ("Saving history")

	/* save nodes to xml */
	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "ephy_history", NULL);
	xmlSetProp (root, "version", EPHY_HISTORY_XML_VERSION);
	xmlDocSetRootElement (doc, root);

	children = ephy_node_get_children (eb->priv->hosts);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);
		if (kid == eb->priv->pages) continue;

		ephy_node_save_to_xml (kid, root);
	}
	ephy_node_thaw (eb->priv->hosts);

	children = ephy_node_get_children (eb->priv->pages);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		ephy_node_save_to_xml (kid, root);
	}
	ephy_node_thaw (eb->priv->pages);

	ephy_file_save_xml (eb->priv->xml_file, doc);
	xmlFreeDoc(doc);
}

static void
hosts_added_cb (EphyNode *node,
	        EphyNode *child,
	        EphyHistory *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->hosts_hash_lock);

	g_hash_table_insert (eb->priv->hosts_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION),
			     child);

	g_static_rw_lock_writer_unlock (eb->priv->hosts_hash_lock);
}

static void
hosts_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint old_index,
		  EphyHistory *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->hosts_hash_lock);

	g_hash_table_remove (eb->priv->hosts_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION));

	g_static_rw_lock_writer_unlock (eb->priv->hosts_hash_lock);
}

static void
pages_added_cb (EphyNode *node,
	        EphyNode *child,
	        EphyHistory *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->pages_hash_lock);

	g_hash_table_insert (eb->priv->pages_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION),
			     child);

	g_static_rw_lock_writer_unlock (eb->priv->pages_hash_lock);
}

static void
pages_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint old_index,
		  EphyHistory *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->pages_hash_lock);

	g_hash_table_remove (eb->priv->pages_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION));

	g_static_rw_lock_writer_unlock (eb->priv->pages_hash_lock);
}

static gboolean
periodic_save_cb (EphyHistory *eh)
{
	remove_obsolete_pages (eh);
	ephy_history_save (eh);

	return TRUE;
}

static gboolean
unref_empty_host (EphyNode *node)
{
	ephy_node_unref (node);

	return FALSE;
}

static void
page_removed_from_host_cb (EphyNode *node,
		           EphyNode *child,
		           guint old_index,
		           EphyHistory *eb)
{
	if (ephy_node_get_n_children (node) == 0)
	{
		g_idle_add ((GSourceFunc)unref_empty_host, node);
	}
}

static void
connect_page_removed_from_host (char *url,
                                EphyNode *node,
                                EphyHistory *eb)
{
	if (node == eb->priv->pages) return;

	ephy_node_signal_connect_object (node,
					 EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) page_removed_from_host_cb,
					 G_OBJECT (eb));
}

static void
ephy_history_init (EphyHistory *eb)
{
	GValue value = { 0, };
	EphyNodeDb *db;

        eb->priv = g_new0 (EphyHistoryPrivate, 1);

	db = ephy_node_db_new ("EphyHistory");
	eb->priv->db = db;

	eb->priv->xml_file = g_build_filename (ephy_dot_dir (),
					       "ephy-history.xml",
					       NULL);

	eb->priv->pages_hash = g_hash_table_new (g_str_hash,
			                          g_str_equal);
	eb->priv->pages_hash_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (eb->priv->pages_hash_lock);

	eb->priv->hosts_hash = g_hash_table_new (g_str_hash,
			                         g_str_equal);
	eb->priv->hosts_hash_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (eb->priv->hosts_hash_lock);

	/* Pages */
	eb->priv->pages = ephy_node_new_with_id (db, PAGES_NODE_ID);
	ephy_node_ref (eb->priv->pages);
	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, _("All"));
	ephy_node_set_property (eb->priv->pages,
			        EPHY_NODE_PAGE_PROP_LOCATION,
			        &value);
	ephy_node_set_property (eb->priv->pages,
			        EPHY_NODE_PAGE_PROP_TITLE,
			        &value);
	g_value_unset (&value);
	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, EPHY_NODE_ALL_PRIORITY);
	ephy_node_set_property (eb->priv->pages,
			        EPHY_NODE_PAGE_PROP_PRIORITY,
			        &value);
	g_value_unset (&value);
	ephy_node_signal_connect_object (eb->priv->pages,
					 EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) pages_added_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->pages,
					 EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) pages_removed_cb,
					 G_OBJECT (eb));

	/* Hosts */
	eb->priv->hosts = ephy_node_new_with_id (db, HOSTS_NODE_ID);
	ephy_node_ref (eb->priv->hosts);
	ephy_node_signal_connect_object (eb->priv->hosts,
					 EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) hosts_added_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->hosts,
					 EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) hosts_removed_cb,
					 G_OBJECT (eb));

	ephy_node_add_child (eb->priv->hosts, eb->priv->pages);

	ephy_history_load (eb);
	ephy_history_emit_data_changed (eb);

	g_hash_table_foreach (eb->priv->hosts_hash,
			      (GHFunc) connect_page_removed_from_host,
			      eb);

	/* setup the periodic history saving callback */
	eb->priv->autosave_timeout =
		g_timeout_add (HISTORY_SAVE_INTERVAL,
		       (GSourceFunc)periodic_save_cb,
		       eb);
}

static void
ephy_history_finalize (GObject *object)
{
        EphyHistory *eb;

	g_return_if_fail (IS_EPHY_HISTORY (object));

	eb = EPHY_HISTORY (object);

        g_return_if_fail (eb->priv != NULL);

	ephy_history_save (eb);

	ephy_node_unref (eb->priv->pages);
	ephy_node_unref (eb->priv->hosts);

	g_object_unref (eb->priv->db);

	g_hash_table_destroy (eb->priv->pages_hash);
	g_static_rw_lock_free (eb->priv->pages_hash_lock);
	g_hash_table_destroy (eb->priv->hosts_hash);
	g_static_rw_lock_free (eb->priv->hosts_hash_lock);

	g_source_remove (eb->priv->autosave_timeout);

        g_free (eb->priv);

	LOG ("Global history finalized");

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyHistory *
ephy_history_new ()
{
	EphyHistory *tab;

	tab = EPHY_HISTORY (g_object_new (EPHY_HISTORY_TYPE, NULL));

	return tab;
}

static void
ephy_history_host_visited (EphyHistory *eh,
			   EphyNode *host,
			   GTime now)
{
	GValue value = { 0, };
	int visits;

	LOG ("Host visited")

	visits = ephy_node_get_property_int
		(host, EPHY_NODE_PAGE_PROP_VISITS);
	if (visits < 0) visits = 0;
	visits++;

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, visits);
	ephy_node_set_property (host, EPHY_NODE_PAGE_PROP_VISITS,
			        &value);
	g_value_unset (&value);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, now);
	ephy_node_set_property (host, EPHY_NODE_PAGE_PROP_LAST_VISIT,
			        &value);
	g_value_unset (&value);
}

static EphyNode *
ephy_history_add_host (EphyHistory *eh, EphyNode *page)
{
	GnomeVFSURI *vfs_uri = NULL;
	EphyNode *host = NULL;
	const char *host_name = NULL;
	GList *host_locations = NULL, *l;
	GValue value = { 0, };
	const char *url;
	const char *scheme = NULL;
	GTime now;

	now = time (NULL);

	url = ephy_node_get_property_string
		(page, EPHY_NODE_PAGE_PROP_LOCATION);

	vfs_uri = gnome_vfs_uri_new (url);

	if (vfs_uri)
	{
		scheme = gnome_vfs_uri_get_scheme (vfs_uri);
		host_name = gnome_vfs_uri_get_host_name (vfs_uri);
	}

	/* Build an host name */
	if (scheme == NULL || host_name == NULL)
	{
		host_name = _("Others");
		host_locations = g_list_append (host_locations,
						g_strdup ("about:blank"));
	}
	else if (strcmp (url, "file") == 0)
	{
		host_name = _("Local files");
		host_locations = g_list_append (host_locations,
						g_strdup ("file:///"));
	}
	else
	{
		char *location;
		char *tmp;

		location = g_strconcat (gnome_vfs_uri_get_scheme (vfs_uri),
					"://", host_name, "/", NULL);
		host_locations = g_list_append (host_locations, location);


		if (g_str_has_prefix (host_name, "www."))
		{
			tmp = g_strdup (g_utf8_offset_to_pointer (host_name, 4));
		}
		else
		{
			tmp = g_strconcat ("www.", host_name, NULL);
		}
		location = g_strconcat (gnome_vfs_uri_get_scheme (vfs_uri),
					"://", tmp, "/", NULL);
		g_free (tmp);
		host_locations = g_list_append (host_locations, location);
	}

	g_return_val_if_fail (host_locations != NULL, NULL);

	g_static_rw_lock_reader_lock (eh->priv->hosts_hash_lock);

	for (l = host_locations; l != NULL; l = l->next)
	{
		host = g_hash_table_lookup (eh->priv->hosts_hash,
					    (char *)l->data);
		if (host) break;
	}
	g_static_rw_lock_reader_unlock (eh->priv->hosts_hash_lock);

	if (!host)
	{
		host = ephy_node_new (eh->priv->db);
		ephy_node_signal_connect_object (host,
						 EPHY_NODE_CHILD_REMOVED,
					         (EphyNodeCallback) page_removed_from_host_cb,
						 G_OBJECT (eh));

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, host_name);
		ephy_node_set_property (host, EPHY_NODE_PAGE_PROP_TITLE,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, (char *)host_locations->data);
		ephy_node_set_property (host, EPHY_NODE_PAGE_PROP_LOCATION,
				        &value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, now);
		ephy_node_set_property (host, EPHY_NODE_PAGE_PROP_FIRST_VISIT,
				        &value);
		g_value_unset (&value);

		ephy_node_add_child (eh->priv->hosts, host);
	}

	ephy_history_host_visited (eh, host, now);

	if (vfs_uri)
	{
		gnome_vfs_uri_unref (vfs_uri);
	}

	g_list_foreach (host_locations, (GFunc)g_free, NULL);
	g_list_free (host_locations);

	return host;
}

static void
ephy_history_visited (EphyHistory *eh, EphyNode *node)
{
	GValue value = { 0, };
	GTime now;
	int visits;
	const char *url;
	int host_id;

	now = time (NULL);

	g_assert (node != NULL);

	url = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_LOCATION);

	visits = ephy_node_get_property_int
		(node, EPHY_NODE_PAGE_PROP_VISITS);
	if (visits < 0) visits = 0;
	visits++;

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, visits);
	ephy_node_set_property (node, EPHY_NODE_PAGE_PROP_VISITS,
			        &value);
	g_value_unset (&value);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, now);
	ephy_node_set_property (node, EPHY_NODE_PAGE_PROP_LAST_VISIT,
			        &value);
	if (visits == 1)
	{
		ephy_node_set_property
			(node, EPHY_NODE_PAGE_PROP_FIRST_VISIT, &value);
	}
	g_value_unset (&value);

	host_id = ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID);
	if (host_id >= 0)
	{
		EphyNode *host;

		host = ephy_node_db_get_node_from_id (eh->priv->db, host_id);
		ephy_history_host_visited (eh, host, now);
	}

	eh->priv->last_page = node;

	g_signal_emit (G_OBJECT (eh), ephy_history_signals[VISITED], 0, url);
	ephy_history_emit_data_changed (eh);
}

int
ephy_history_get_page_visits (EphyHistory *gh,
			      const char *url)
{
	EphyNode *node;
	int visits = 0;

	node = ephy_history_get_page (gh, url);
	if (node)
	{
		visits = ephy_node_get_property_int
			(node, EPHY_NODE_PAGE_PROP_VISITS);
		if (visits < 0) visits = 0;
	}

	return visits;
}

void
ephy_history_add_page (EphyHistory *eb,
		       const char *url)
{
	EphyNode *bm, *node, *host;
	GValue value = { 0, };

	node = ephy_history_get_page (eb, url);
	if (node)
	{
		ephy_history_visited (eb, node);
		return;
	}

	bm = ephy_node_new (eb->priv->db);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, url);
	ephy_node_set_property (bm, EPHY_NODE_PAGE_PROP_LOCATION,
			        &value);
	ephy_node_set_property (bm, EPHY_NODE_PAGE_PROP_TITLE,
			        &value);
	g_value_unset (&value);

	host = ephy_history_add_host (eb, bm);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, ephy_node_get_id (host));
	ephy_node_set_property (bm, EPHY_NODE_PAGE_PROP_HOST_ID,
			        &value);
	g_value_unset (&value);

	ephy_history_visited (eb, bm);

	ephy_node_add_child (host, bm);
	ephy_node_add_child (eb->priv->pages, bm);
}

EphyNode *
ephy_history_get_page (EphyHistory *eb,
		       const char *url)
{
	EphyNode *node;

	g_static_rw_lock_reader_lock (eb->priv->pages_hash_lock);
	node = g_hash_table_lookup (eb->priv->pages_hash, url);
	g_static_rw_lock_reader_unlock (eb->priv->pages_hash_lock);

	return node;
}

gboolean
ephy_history_is_page_visited (EphyHistory *gh,
			      const char *url)
{
	return (ephy_history_get_page (gh, url) != NULL);
}

void
ephy_history_set_page_title (EphyHistory *gh,
			     const char *url,
			     const char *title)
{
	EphyNode *node;
	GValue value = { 0, };

	LOG ("Set page title")

	if (title == NULL || title[0] == '\0') return;

	node = ephy_history_get_page (gh, url);
	if (!node) return;

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, title);
	ephy_node_set_property
		(node, EPHY_NODE_PAGE_PROP_TITLE, &value);
	g_value_unset (&value);
}

void
ephy_history_set_icon (EphyHistory *gh,
		       const char *url,
		       const char *icon)
{
	EphyNode *host;

	LOG ("Set host icon")

	host = g_hash_table_lookup (gh->priv->hosts_hash, url);
	if (host)
	{
		GValue value = { 0, };

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, icon);
		ephy_node_set_property
			(host, EPHY_NODE_PAGE_PROP_ICON, &value);
		g_value_unset (&value);
	}
}

void
ephy_history_clear (EphyHistory *gh)
{
	EphyNode *node;

	while ((node = ephy_node_get_nth_child (gh->priv->pages, 0)) != NULL)
	{
		ephy_node_unref (node);
	}

	ephy_history_save (gh);
}

EphyNode *
ephy_history_get_hosts (EphyHistory *eb)
{
	return eb->priv->hosts;
}

EphyNode *
ephy_history_get_pages (EphyHistory *eb)
{
	return eb->priv->pages;
}

const char *
ephy_history_get_last_page (EphyHistory *gh)
{
	if (gh->priv->last_page == NULL) return NULL;

	return ephy_node_get_property_string
		(gh->priv->last_page, EPHY_NODE_PAGE_PROP_LOCATION);
}
