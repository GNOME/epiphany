/* 
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *
 *  $Id$
 */

#include "ephy-node-db.h"

static void ephy_node_db_class_init (EphyNodeDbClass *klass);
static void ephy_node_db_init (EphyNodeDb *node);
static void ephy_node_db_finalize (GObject *object);

/* FIXME I want to find a better way to deal with "root" nodes */
#define RESERVED_IDS 30

enum
{
	PROP_0,
	PROP_NAME
};

struct EphyNodeDbPrivate
{
	char *name;

	GMutex *id_factory_lock;
	long id_factory;

	GStaticRWLock *id_to_node_lock;
	GPtrArray *id_to_node;
};

static GHashTable *ephy_node_databases = NULL;

static GObjectClass *parent_class = NULL;

GType
ephy_node_db_get_type (void)
{
	static GType ephy_node_db_type = 0;

	if (ephy_node_db_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (EphyNodeDbClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_node_db_class_init,
			NULL,
			NULL,
			sizeof (EphyNodeDb),
			0,
			(GInstanceInitFunc) ephy_node_db_init
		};

		ephy_node_db_type = g_type_register_static (G_TYPE_OBJECT,
						       "EphyNodeDb",
						       &our_info, 0);
	}

	return ephy_node_db_type;
}

static void
ephy_node_db_set_name (EphyNodeDb *db, const char *name)
{
	db->priv->name = g_strdup (name);

	if (ephy_node_databases == NULL)
	{
		ephy_node_databases = g_hash_table_new (g_str_hash, g_str_equal);
	}

	g_hash_table_insert (ephy_node_databases, db->priv->name, db);
}

static void
ephy_node_db_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	EphyNodeDb *db;

	db = EPHY_NODE_DB (object);

	switch (prop_id)
	{
		case PROP_NAME:
			g_value_set_string (value, db->priv->name);
			break;
	}
}


static void
ephy_node_db_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	EphyNodeDb *db;

	db = EPHY_NODE_DB (object);

	switch (prop_id)
	{
		case PROP_NAME:
			ephy_node_db_set_name (db, g_value_get_string (value));
			break;
	}
}

static void
ephy_node_db_class_init (EphyNodeDbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_node_db_finalize;
        object_class->set_property = ephy_node_db_set_property;
        object_class->get_property = ephy_node_db_get_property;

	g_object_class_install_property (object_class,
                                         PROP_NAME,
                                         g_param_spec_string  ("name",
                                                               "Name",
                                                               "Name",
                                                               NULL,
                                                               G_PARAM_READWRITE));
}

static void
ephy_node_db_init (EphyNodeDb *db)
{
	db->priv = g_new0 (EphyNodeDbPrivate, 1);

	db->priv->name = NULL;

	/* id to node */
	db->priv->id_to_node = g_ptr_array_new ();

	db->priv->id_to_node_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (db->priv->id_to_node_lock);

	/* id factory */
	db->priv->id_factory = RESERVED_IDS;
	db->priv->id_factory_lock = g_mutex_new ();
}

static void
ephy_node_db_finalize (GObject *object)
{
	EphyNodeDb *db;

	g_return_if_fail (object != NULL);

	db = EPHY_NODE_DB (object);

	g_return_if_fail (db->priv != NULL);

	g_hash_table_remove (ephy_node_databases, db->priv->name);
	if (g_hash_table_size (ephy_node_databases) == 0)
	{
		g_hash_table_destroy (ephy_node_databases);
	}

	g_ptr_array_free (db->priv->id_to_node, FALSE);

	g_static_rw_lock_free (db->priv->id_to_node_lock);

	g_mutex_free (db->priv->id_factory_lock);

	g_free (db->priv->name);

	g_free (db->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyNodeDb *
ephy_node_db_get_by_name (const char *name)
{
	EphyNodeDb *ret;

	ret = g_hash_table_lookup (ephy_node_databases, name);

	return ret;
}

EphyNodeDb *
ephy_node_db_new (const char *name)
{
	EphyNodeDb *db;

	db = EPHY_NODE_DB (g_object_new (EPHY_TYPE_NODE_DB,
					 "name", name,
				         NULL));

	g_return_val_if_fail (db->priv != NULL, NULL);

	return db;
}

static inline EphyNode *
node_from_id_real (EphyNodeDb *db, long id)
{
	EphyNode *ret = NULL;

	if (id < db->priv->id_to_node->len)
		ret = g_ptr_array_index (db->priv->id_to_node, id);;

	return ret;
}

const char *
ephy_node_db_get_name (EphyNodeDb *db)
{
	return db->priv->name;
}

EphyNode *
ephy_node_db_get_node_from_id (EphyNodeDb *db, long id)
{
	EphyNode *ret = NULL;

	g_static_rw_lock_reader_lock (db->priv->id_to_node_lock);

	ret = node_from_id_real (db, id);

	g_static_rw_lock_reader_unlock (db->priv->id_to_node_lock);

	return ret;
}

long
_ephy_node_db_new_id (EphyNodeDb *db)
{
	long ret;

	g_mutex_lock (db->priv->id_factory_lock);

	while (node_from_id_real (db, db->priv->id_factory) != NULL)
	{
		db->priv->id_factory++;
	}

	ret = db->priv->id_factory;

	g_mutex_unlock (db->priv->id_factory_lock);

	return ret;
}

void
_ephy_node_db_add_id (EphyNodeDb *db,
		      long id,
		      EphyNode *node)
{
	g_static_rw_lock_writer_lock (db->priv->id_to_node_lock);

	/* resize array if needed */
	if (id >= db->priv->id_to_node->len)
		g_ptr_array_set_size (db->priv->id_to_node, id + 1);

	g_ptr_array_index (db->priv->id_to_node, id) = node;

	g_static_rw_lock_writer_unlock (db->priv->id_to_node_lock);
}

void
_ephy_node_db_remove_id (EphyNodeDb *db,
			 long id)
{
	g_static_rw_lock_writer_lock (db->priv->id_to_node_lock);

	g_ptr_array_index (db->priv->id_to_node, id) = NULL;

	/* reset id factory so we use the freed node id */
	db->priv->id_factory = RESERVED_IDS;

	g_static_rw_lock_writer_unlock (db->priv->id_to_node_lock);
}
