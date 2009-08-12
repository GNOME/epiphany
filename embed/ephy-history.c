/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002, 2003 Marco Pesenti Gritti
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
 */

#include "config.h"

#include "ephy-history.h"
#include "ephy-marshal.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"
#include "ephy-node-db.h"
#include "ephy-node-common.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-string.h"

#include <time.h>
#include <string.h>
#include <glib/gi18n.h>

#define EPHY_HISTORY_XML_ROOT	 (const xmlChar *)"ephy_history"
#define EPHY_HISTORY_XML_VERSION (const xmlChar *)"1.0"

/* how often to save the history, in seconds */
#define HISTORY_SAVE_INTERVAL (5 * 60)

/* if you change this remember to change also the user interface description */
#define HISTORY_PAGE_OBSOLETE_DAYS 10

/* the number of seconds in a day */
#define SECS_PER_DAY (60*60*24)

#define EPHY_HISTORY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_HISTORY, EphyHistoryPrivate))

struct _EphyHistoryPrivate
{
	char *xml_file;
	EphyNodeDb *db;
	EphyNode *hosts;
	EphyNode *pages;
	EphyNode *last_page;
	GHashTable *hosts_hash;
	GHashTable *pages_hash;
	guint autosave_timeout;
	guint update_hosts_idle;
	guint disable_history_notifier_id;
	gboolean dirty;
	gboolean enabled;
};

enum
{
	REDIRECT_FLAG	= 1 << 0,
	TOPLEVEL_FLAG	= 1 << 1
};

enum
{
	PROP_0,
	PROP_ENABLED
};

enum
{
	ADD_PAGE,
	VISITED,
	CLEARED,
	REDIRECT,
	ICON_UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void ephy_history_class_init	(EphyHistoryClass *klass);
static void ephy_history_init		(EphyHistory *history);
static void ephy_history_finalize	(GObject *object);
static gboolean impl_add_page           (EphyHistory *, const char *, gboolean, gboolean);

G_DEFINE_TYPE (EphyHistory, ephy_history, G_TYPE_OBJECT)

static void
ephy_history_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	EphyHistory *history = EPHY_HISTORY (object);

	switch (prop_id)
	{
		case PROP_ENABLED:
			ephy_history_set_enabled (history, g_value_get_boolean (value));
			break;
	}
}

static void
ephy_history_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	EphyHistory *history = EPHY_HISTORY (object);

	switch (prop_id)
	{
		case PROP_ENABLED:
			g_value_set_boolean (value, history->priv->enabled);
			break;
	}
}

static void
ephy_history_class_init (EphyHistoryClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = ephy_history_finalize;
	object_class->get_property = ephy_history_get_property;
	object_class->set_property = ephy_history_set_property;

	klass->add_page = impl_add_page;

	g_object_class_install_property (object_class,
					 PROP_ENABLED,
					 g_param_spec_boolean ("enabled",
							       "Enabled",
							       "Enabled",
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	signals[ADD_PAGE] =
		g_signal_new ("add_page",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyHistoryClass, add_page),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__STRING_BOOLEAN_BOOLEAN,
			      G_TYPE_BOOLEAN,
			      3,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN);

	signals[VISITED] =
                g_signal_new ("visited",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyHistoryClass, visited),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);

	signals[CLEARED] =
                g_signal_new ("cleared",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyHistoryClass, cleared),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);

	signals[REDIRECT] =
                g_signal_new ("redirect",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EphyHistoryClass, redirect),
                              NULL, NULL,
                              ephy_marshal_VOID__STRING_STRING,
                              G_TYPE_NONE,
                              2,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[ICON_UPDATED] =
		g_signal_new ("icon_updated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyHistoryClass, icon_updated),
			      NULL, NULL,
			      ephy_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING, G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (EphyHistoryPrivate));
}

static gboolean
page_is_obsolete (EphyNode *node, time_t now)
{
	int last_visit;

	last_visit = ephy_node_get_property_int
		(node, EPHY_NODE_PAGE_PROP_LAST_VISIT);
	return now - last_visit >= HISTORY_PAGE_OBSOLETE_DAYS*SECS_PER_DAY;
}

static void
remove_obsolete_pages (EphyHistory *eb)
{
	GPtrArray *children;
	int i;
	time_t now;

	now = time (NULL);

	children = ephy_node_get_children (eb->priv->pages);
	for (i = (int) children->len - 1; i >= 0; i--)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (page_is_obsolete (kid, now))
		{
			ephy_node_unref (kid);
		}
	}
}

static gboolean
save_filter (EphyNode *node,
	     EphyNode *page_node)
{
	return node != page_node;
}

static void
ephy_history_save (EphyHistory *eb)
{
	int ret;

	/* only save if there are changes */
	if (eb->priv->dirty == FALSE && eb->priv->enabled)
	{
		return;
	}

	LOG ("Saving history db");

	ret = ephy_node_db_write_to_xml_safe
		(eb->priv->db, (const xmlChar *)eb->priv->xml_file,
		 EPHY_HISTORY_XML_ROOT,
		 EPHY_HISTORY_XML_VERSION,
		 NULL, /* comment */
		 eb->priv->hosts,
		 (EphyNodeFilterFunc) save_filter, eb->priv->pages,
		 eb->priv->pages, NULL, NULL,
		 NULL);

	if (ret >=0)
	{
		/* save was successful */
		eb->priv->dirty = FALSE;
	}
}

static void
hosts_added_cb (EphyNode *node,
	        EphyNode *child,
	        EphyHistory *eb)
{
	eb->priv->dirty = TRUE;

	g_hash_table_insert (eb->priv->hosts_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION),
			     child);
}

static void
hosts_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint old_index,
		  EphyHistory *eb)
{
	eb->priv->dirty = TRUE;

	g_hash_table_remove (eb->priv->hosts_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION));
}

static void
hosts_changed_cb (EphyNode *node,
		  EphyNode *child,
		  guint property_id,
		  EphyHistory *eb)
{
	eb->priv->dirty = TRUE;
}

static void
pages_added_cb (EphyNode *node,
	        EphyNode *child,
	        EphyHistory *eb)
{
	eb->priv->dirty = TRUE;

	g_hash_table_insert (eb->priv->pages_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION),
			     child);
}

static void
pages_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint old_index,
		  EphyHistory *eb)
{
	eb->priv->dirty = TRUE;

	g_hash_table_remove (eb->priv->pages_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_PAGE_PROP_LOCATION));
}

static void
pages_changed_cb (EphyNode *node,
		  EphyNode *child,
		  guint property_id,
		  EphyHistory *eb)
{
	eb->priv->dirty = TRUE;
}

static gboolean
periodic_save_cb (EphyHistory *eh)
{
	remove_obsolete_pages (eh);
	ephy_history_save (eh);

	return TRUE;
}

static void
update_host_on_child_remove (EphyNode *node)
{
	GPtrArray *children;
	int i, host_last_visit, new_host_last_visit = 0;

	host_last_visit = ephy_node_get_property_int
			(node, EPHY_NODE_PAGE_PROP_LAST_VISIT);

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		int last_visit;

		kid = g_ptr_array_index (children, i);

		last_visit = ephy_node_get_property_int
                        (kid, EPHY_NODE_PAGE_PROP_LAST_VISIT);

		if (last_visit > new_host_last_visit)
		{
			new_host_last_visit = last_visit;
		}
	}

	if (host_last_visit != new_host_last_visit)
	{
		ephy_node_set_property_int (node,
					    EPHY_NODE_PAGE_PROP_LAST_VISIT,
					    new_host_last_visit);
	}
}

static gboolean
update_hosts (EphyHistory *eh)
{
	GPtrArray *children;
	int i;
	GList *empty = NULL;

	children = ephy_node_get_children (eh->priv->hosts);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (kid != eh->priv->pages)
		{
			if (ephy_node_get_n_children (kid) > 0)
			{
				update_host_on_child_remove (kid);
			}
			else
			{
				empty = g_list_prepend (empty, kid);
			}
		}
	}

	g_list_foreach (empty, (GFunc)ephy_node_unref, NULL);
	g_list_free (empty);

	eh->priv->update_hosts_idle = 0;

	return FALSE;
}

static void
page_removed_from_host_cb (EphyNode *node,
		           EphyNode *child,
		           guint old_index,
		           EphyHistory *eb)
{
	if (eb->priv->update_hosts_idle == 0)
	{
		eb->priv->update_hosts_idle = g_idle_add
			((GSourceFunc)update_hosts, eb);
	}
}

static void
remove_pages_from_host_cb (EphyNode *host,
			   EphyHistory *eh)
{
	GPtrArray *children;
	EphyNode *site;
	int i;

	children = ephy_node_get_children (host);

	for (i = (int) children->len - 1; i >= 0; i--)
	{
		site = g_ptr_array_index (children, i);

		ephy_node_unref (site);
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
	ephy_node_signal_connect_object (node,
					 EPHY_NODE_DESTROY,
					 (EphyNodeCallback) remove_pages_from_host_cb,
					 G_OBJECT (eb));
}

static void
disable_history_notifier (GConfClient *client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  EphyHistory *history)
{
	ephy_history_set_enabled
		(history, !eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_HISTORY));
}

static void
ephy_history_init (EphyHistory *eb)
{
	EphyNodeDb *db;
	const char *all = _("All");

        eb->priv = EPHY_HISTORY_GET_PRIVATE (eb);
	eb->priv->update_hosts_idle = 0;
	eb->priv->enabled = TRUE;

	db = ephy_node_db_new (EPHY_NODE_DB_HISTORY);
	eb->priv->db = db;

	eb->priv->xml_file = g_build_filename (ephy_dot_dir (),
					       "ephy-history.xml",
					       NULL);

	eb->priv->pages_hash = g_hash_table_new (g_str_hash,
			                          g_str_equal);
	eb->priv->hosts_hash = g_hash_table_new (g_str_hash,
			                         g_str_equal);

	/* Pages */
	eb->priv->pages = ephy_node_new_with_id (db, PAGES_NODE_ID);

	ephy_node_set_property_string (eb->priv->pages,
				       EPHY_NODE_PAGE_PROP_LOCATION,
				       all);
	ephy_node_set_property_string (eb->priv->pages,
				       EPHY_NODE_PAGE_PROP_TITLE,
				       all);

	ephy_node_set_property_int (eb->priv->pages,
				    EPHY_NODE_PAGE_PROP_PRIORITY,
				    EPHY_NODE_ALL_PRIORITY);
	
	ephy_node_signal_connect_object (eb->priv->pages,
					 EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) pages_added_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->pages,
					 EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) pages_removed_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->pages,
					 EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) pages_changed_cb,
					 G_OBJECT (eb));

	/* Hosts */
	eb->priv->hosts = ephy_node_new_with_id (db, HOSTS_NODE_ID);
	ephy_node_signal_connect_object (eb->priv->hosts,
					 EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) hosts_added_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->hosts,
					 EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) hosts_removed_cb,
					 G_OBJECT (eb));
	ephy_node_signal_connect_object (eb->priv->hosts,
					 EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) hosts_changed_cb,
					 G_OBJECT (eb));

	ephy_node_add_child (eb->priv->hosts, eb->priv->pages);

	ephy_node_db_load_from_file (eb->priv->db, eb->priv->xml_file,
				     EPHY_HISTORY_XML_ROOT,
				     EPHY_HISTORY_XML_VERSION);

	g_hash_table_foreach (eb->priv->hosts_hash,
			      (GHFunc) connect_page_removed_from_host,
			      eb);

	/* mark as clean */
	eb->priv->dirty = FALSE;

	/* setup the periodic history saving callback */
	eb->priv->autosave_timeout =
		g_timeout_add_seconds (HISTORY_SAVE_INTERVAL,
		       (GSourceFunc)periodic_save_cb,
		       eb);

	disable_history_notifier (NULL, 0, NULL, eb);
	eb->priv->disable_history_notifier_id = eel_gconf_notification_add
		(CONF_LOCKDOWN_DISABLE_HISTORY,
		 (GConfClientNotifyFunc) disable_history_notifier, eb);
}

static void
ephy_history_finalize (GObject *object)
{
        EphyHistory *eb = EPHY_HISTORY (object);

	if (eb->priv->update_hosts_idle)
	{
		g_source_remove (eb->priv->update_hosts_idle);
	}

	ephy_history_save (eb);

	ephy_node_unref (eb->priv->pages);
	ephy_node_unref (eb->priv->hosts);

	g_object_unref (eb->priv->db);

	g_hash_table_destroy (eb->priv->pages_hash);
	g_hash_table_destroy (eb->priv->hosts_hash);

	g_source_remove (eb->priv->autosave_timeout);

	eel_gconf_notification_remove (eb->priv->disable_history_notifier_id);

	g_free (eb->priv->xml_file);

	LOG ("Global history finalized");

	G_OBJECT_CLASS (ephy_history_parent_class)->finalize (object);
}

EphyHistory *
ephy_history_new (void)
{
	return EPHY_HISTORY (g_object_new (EPHY_TYPE_HISTORY, NULL));
}

static void
ephy_history_host_visited (EphyHistory *eh,
			   EphyNode *host,
			   GTime now)
{
	int visits;

	LOG ("Host visited");

	visits = ephy_node_get_property_int
		(host, EPHY_NODE_PAGE_PROP_VISITS);
	if (visits < 0) visits = 0;
	visits++;

	ephy_node_set_property_int (host, EPHY_NODE_PAGE_PROP_VISITS, visits);
	ephy_node_set_property_int (host, EPHY_NODE_PAGE_PROP_LAST_VISIT,
				    now);
}

static EphyNode *
internal_get_host (EphyHistory *eh, const char *url, gboolean create)
{
	EphyNode *host = NULL;
	char *host_name = NULL;
	GList *host_locations = NULL, *l;
	char *scheme = NULL;
	GTime now;

	g_return_val_if_fail (url != NULL, NULL);

	if (eh->priv->enabled == FALSE)
	{
		return NULL;
	}

	now = time (NULL);

	if (url)
	{
		scheme = g_uri_parse_scheme (url);
		host_name = ephy_string_get_host_name (url);
	}

	/* Build an host name */
	if (scheme == NULL || host_name == NULL)
	{
		host_name = g_strdup (_("Others"));
		host_locations = g_list_append (host_locations,
						g_strdup ("about:blank"));
	}
	else if (strcmp (scheme, "file") == 0)
	{
		host_name = g_strdup (_("Local files"));
		host_locations = g_list_append (host_locations,
						g_strdup ("file:///"));
	}
	else
	{
		char *location;
		char *tmp;
		
		if (g_str_equal (scheme, "https"))
		{
			/* If scheme is https, we still fake http. */
			location = g_strconcat ("http://", host_name, "/", NULL);
			host_locations = g_list_append (host_locations, location);
		}

		/* We append the real address */
		location = g_strconcat (scheme,
					"://", host_name, "/", NULL);
		host_locations = g_list_append (host_locations, location);

		/* and also a fake www-modified address if it's http or https. */
		if (g_str_has_prefix (scheme, "http"))
		{
			if (g_str_has_prefix (host_name, "www."))
			{
				tmp = g_strdup (host_name + 4);
			}
			else
			{
				tmp = g_strconcat ("www.", host_name, NULL);
			}
			location = g_strconcat ("http://", tmp, "/", NULL);
			g_free (tmp);
			host_locations = g_list_append (host_locations, location);
		}
	}

	g_return_val_if_fail (host_locations != NULL, NULL);

	for (l = host_locations; l != NULL; l = l->next)
	{
		host = g_hash_table_lookup (eh->priv->hosts_hash,
					    (char *)l->data);
		if (host) break;
	}

	if (!host && create)
	{
		host = ephy_node_new (eh->priv->db);
		ephy_node_signal_connect_object (host,
						 EPHY_NODE_CHILD_REMOVED,
					         (EphyNodeCallback) page_removed_from_host_cb,
						 G_OBJECT (eh));
		ephy_node_signal_connect_object (host,
						 EPHY_NODE_DESTROY,
						 (EphyNodeCallback) remove_pages_from_host_cb,
						 G_OBJECT (eh));
		ephy_node_set_property_string (host,
					       EPHY_NODE_PAGE_PROP_TITLE,
					       host_name);
		ephy_node_set_property_string (host,
					       EPHY_NODE_PAGE_PROP_LOCATION,
					       (char *)host_locations->data);
		ephy_node_set_property_int (host,
					    EPHY_NODE_PAGE_PROP_FIRST_VISIT,
					    now);
		ephy_node_add_child (eh->priv->hosts, host);
	}

	if (host)
	{
		ephy_history_host_visited (eh, host, now);
	}

	g_free (scheme);
	g_free (host_name);

	g_list_foreach (host_locations, (GFunc)g_free, NULL);
	g_list_free (host_locations);

	return host;
}

EphyNode *
ephy_history_get_host (EphyHistory *eh, const char *url)
{
	return internal_get_host (eh, url, FALSE);
}

static EphyNode *
ephy_history_add_host (EphyHistory *eh, EphyNode *page)
{
	const char *url;

	url = ephy_node_get_property_string
		(page, EPHY_NODE_PAGE_PROP_LOCATION);

	return internal_get_host (eh, url, TRUE);
}

static void
ephy_history_visited (EphyHistory *eh, EphyNode *node)
{
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

	ephy_node_set_property_int (node, EPHY_NODE_PAGE_PROP_VISITS, visits);
	ephy_node_set_property_int (node, EPHY_NODE_PAGE_PROP_LAST_VISIT,
				    now);
	if (visits == 1)
	{
		ephy_node_set_property_int
			(node, EPHY_NODE_PAGE_PROP_FIRST_VISIT, now);
	}

	host_id = ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID);
	if (host_id >= 0)
	{
		EphyNode *host;

		host = ephy_node_db_get_node_from_id (eh->priv->db, host_id);
		ephy_history_host_visited (eh, host, now);
	}

	eh->priv->last_page = node;

	g_signal_emit (G_OBJECT (eh), signals[VISITED], 0, url);
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
ephy_history_add_page (EphyHistory *eh,
		       const char *url,
		       gboolean redirect,
		       gboolean toplevel)
{
	gboolean result = FALSE;

	g_signal_emit (eh, signals[ADD_PAGE], 0, url, redirect, toplevel, &result);
}

static gboolean
impl_add_page (EphyHistory *eb,
	       const char *url,
	       gboolean redirect,
	       gboolean toplevel)
{
	EphyNode *bm, *node, *host;
	gulong flags = 0;

	if (eb->priv->enabled == FALSE)
	{
		return FALSE;
	}

	node = ephy_history_get_page (eb, url);
	if (node)
	{
		ephy_history_visited (eb, node);
		return TRUE;
	}

	bm = ephy_node_new (eb->priv->db);

	ephy_node_set_property_string (bm, EPHY_NODE_PAGE_PROP_LOCATION, url);
	ephy_node_set_property_string (bm, EPHY_NODE_PAGE_PROP_TITLE, url);

	if (redirect) flags |= REDIRECT_FLAG;
	if (toplevel) flags |= TOPLEVEL_FLAG;

	/* EphyNode SUCKS! */
	ephy_node_set_property_long (bm, EPHY_NODE_PAGE_PROP_EXTRA_FLAGS,
				     flags);

	host = ephy_history_add_host (eb, bm);

	ephy_node_set_property_int (bm, EPHY_NODE_PAGE_PROP_HOST_ID,
				    ephy_node_get_id (host));

	ephy_history_visited (eb, bm);

	ephy_node_add_child (host, bm);
	ephy_node_add_child (eb->priv->pages, bm);

	return TRUE;
}

EphyNode *
ephy_history_get_page (EphyHistory *eb,
		       const char *url)
{
	EphyNode *node;

	node = g_hash_table_lookup (eb->priv->pages_hash, url);

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

	LOG ("Set page title");

	if (title == NULL || title[0] == '\0') return;

	node = ephy_history_get_page (gh, url);
	if (node == NULL) return;

	ephy_node_set_property_string (node, EPHY_NODE_PAGE_PROP_TITLE,
				       title);
}

const char*
ephy_history_get_icon (EphyHistory *gh,
		       const char *url)
{
	EphyNode *node, *host;
	int host_id;

	node = ephy_history_get_page (gh, url);
	if (node == NULL) return NULL;
	
	host_id = ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID);
	g_return_val_if_fail (host_id >= 0, NULL);

	host = ephy_node_db_get_node_from_id (gh->priv->db, host_id);
	g_return_val_if_fail (host != NULL, NULL);

	return ephy_node_get_property_string (host, EPHY_NODE_PAGE_PROP_ICON);
}	
	
		       
void
ephy_history_set_icon (EphyHistory *gh,
		       const char *url,
		       const char *icon)
{
	EphyNode *node, *host;
	int host_id;

	node = ephy_history_get_page (gh, url);
	if (node == NULL) return;
	
	host_id = ephy_node_get_property_int (node, EPHY_NODE_PAGE_PROP_HOST_ID);
	g_return_if_fail (host_id >= 0);

	host = ephy_node_db_get_node_from_id (gh->priv->db, host_id);
	if (host)
	{
		ephy_node_set_property_string (host, EPHY_NODE_PAGE_PROP_ICON,
					       icon);
	}

	g_signal_emit (gh, signals[ICON_UPDATED], 0, url, icon);
}

void
ephy_history_clear (EphyHistory *gh)
{
	EphyNode *node;

	LOG ("clearing history");

	ephy_node_db_set_immutable (gh->priv->db, FALSE);

	while ((node = ephy_node_get_nth_child (gh->priv->pages, 0)) != NULL)
	{
		ephy_node_unref (node);
	}
	ephy_history_save (gh);

	ephy_node_db_set_immutable (gh->priv->db, !gh->priv->enabled);

	g_signal_emit (gh, signals[CLEARED], 0);
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

gboolean
ephy_history_is_enabled (EphyHistory *history)
{
	g_return_val_if_fail (EPHY_IS_HISTORY (history), FALSE);

	return history->priv->enabled;
}

void
ephy_history_set_enabled (EphyHistory *history,
			  gboolean enabled)
{
	int ret;

	ret = 1;

	LOG ("ephy_history_set_enabled %d", enabled);

	/* Write history only when disabling it, not when reenabling it */
	if (!enabled && history->priv->dirty)
	{
		ret = ephy_node_db_write_to_xml_safe
			(history->priv->db, (const xmlChar *)history->priv->xml_file,
			 EPHY_HISTORY_XML_ROOT,
			 EPHY_HISTORY_XML_VERSION,
			 NULL, /* comment */
			 history->priv->hosts,
			 (EphyNodeFilterFunc) save_filter, history->priv->pages,
			 history->priv->pages, NULL, NULL,
			 NULL);
	}

	if (ret >=0)
	{
		/* save was successful */
		history->priv->dirty = FALSE;
	}

	history->priv->enabled = enabled;

	ephy_node_db_set_immutable (history->priv->db, !enabled);
}

