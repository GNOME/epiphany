/* 
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static void ephy_node_db_class_init (EphyNodeDbClass *klass);
static void ephy_node_db_init (EphyNodeDb *node);
static void ephy_node_db_finalize (GObject *object);

/* FIXME I want to find a better way to deal with "root" nodes */
#define RESERVED_IDS 30

enum
{
	PROP_0,
	PROP_NAME,
	PROP_IMMUTABLE,
};

#define EPHY_NODE_DB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NODE_DB, EphyNodeDbPrivate))

struct EphyNodeDbPrivate
{
	char *name;
	gboolean immutable;

	long id_factory;

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
		case PROP_IMMUTABLE:
			g_value_set_boolean (value, ephy_node_db_is_immutable (db));
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
		case PROP_IMMUTABLE:
			ephy_node_db_set_immutable (db, g_value_get_boolean (value));
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

	g_object_class_install_property (object_class,
                                         PROP_IMMUTABLE,
                                         g_param_spec_boolean  ("immutable",
								"Immutable",
								"Immutable",
								FALSE,
								G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EphyNodeDbPrivate));
}

static void
ephy_node_db_init (EphyNodeDb *db)
{
	db->priv = EPHY_NODE_DB_GET_PRIVATE (db);

	db->priv->name = NULL;

	/* id to node */
	db->priv->id_to_node = g_ptr_array_new ();

	/* id factory */
	db->priv->id_factory = RESERVED_IDS;
}

static void
ephy_node_db_finalize (GObject *object)
{
	EphyNodeDb *db = EPHY_NODE_DB (object);

	g_hash_table_remove (ephy_node_databases, db->priv->name);
	if (g_hash_table_size (ephy_node_databases) == 0)
	{
		g_hash_table_destroy (ephy_node_databases);
	}

	g_ptr_array_free (db->priv->id_to_node, TRUE);

	g_free (db->priv->name);

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

gboolean
ephy_node_db_is_immutable (EphyNodeDb *db)
{
	return db->priv->immutable;
}

void
ephy_node_db_set_immutable (EphyNodeDb *db, gboolean immutable)
{
	db->priv->immutable = immutable;

	g_object_notify (G_OBJECT (db), "immutable");
}

EphyNode *
ephy_node_db_get_node_from_id (EphyNodeDb *db, long id)
{
	EphyNode *ret = NULL;

	ret = node_from_id_real (db, id);

	return ret;
}

long
_ephy_node_db_new_id (EphyNodeDb *db)
{
	long ret;

	while (node_from_id_real (db, db->priv->id_factory) != NULL)
	{
		db->priv->id_factory++;
	}

	ret = db->priv->id_factory;

	return ret;
}

void
_ephy_node_db_add_id (EphyNodeDb *db,
		      long id,
		      EphyNode *node)
{
	/* resize array if needed */
	if (id >= db->priv->id_to_node->len)
		g_ptr_array_set_size (db->priv->id_to_node, id + 1);

	g_ptr_array_index (db->priv->id_to_node, id) = node;
}

void
_ephy_node_db_remove_id (EphyNodeDb *db,
			 long id)
{
	g_ptr_array_index (db->priv->id_to_node, id) = NULL;

	/* reset id factory so we use the freed node id */
	db->priv->id_factory = RESERVED_IDS;
}

gboolean
ephy_node_db_load_from_file (EphyNodeDb *db,
			     const char *xml_file,
			     const xmlChar *xml_root,
			     const xmlChar *xml_version)
{
	xmlTextReaderPtr reader;
	gboolean success = TRUE;
	gboolean was_immutable;
	int ret;

	LOG ("ephy_node_db_load_from_file %s", xml_file)

	START_PROFILER ("loading node db")

	if (g_file_test (xml_file, G_FILE_TEST_EXISTS) == FALSE)
	{
		return FALSE;
	}

	reader = xmlNewTextReaderFilename (xml_file);
	if (reader == NULL)
	{
		return FALSE;
	}

	was_immutable = db->priv->immutable;
	db->priv->immutable = FALSE;

	ret = xmlTextReaderRead (reader);
	while (ret == 1)
	{
		const xmlChar *name;
		xmlReaderTypes type;
		gboolean skip = FALSE;

		name = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (xmlStrEqual (name, "node")
		    && type == XML_READER_TYPE_ELEMENT)
		{
			xmlNodePtr subtree;
			EphyNode *node;

			/* grow the subtree and load the node from it */
			subtree = xmlTextReaderExpand (reader);

			node = ephy_node_new_from_xml (db, subtree);
			
			skip = TRUE;
		}
		else if (xmlStrEqual (name, xml_root)
			 && type == XML_READER_TYPE_ELEMENT)
		{
			xmlChar *version;

			/* check version info */
			version = xmlTextReaderGetAttribute (reader, "version");
			if (xmlStrEqual (version, xml_version) == FALSE)
			{
				success = FALSE;
				xmlFree (version);

				break;
			}

			xmlFree (version);
		}

		/* next one, please */
		ret = skip ? xmlTextReaderNext (reader)
			   : xmlTextReaderRead (reader);
	}

	xmlFreeTextReader (reader);

	db->priv->immutable = was_immutable;

	STOP_PROFILER ("loading node db")

	return (success && ret == 0);
}

static int
ephy_node_db_write_to_xml_valist (EphyNodeDb *db,
				  const xmlChar *filename,
				  const xmlChar *root,
				  const xmlChar *version,
				  const xmlChar *comment,
				  EphyNode *first_node,
				  va_list argptr)
{
	xmlTextWriterPtr writer;
	EphyNode *node;
	int ret;

	LOG ("Saving node db to %s", filename)

	START_PROFILER ("Saving node db")

	/* FIXME: do we want to turn compression on ? */
	writer = xmlNewTextWriterFilename (filename, 0);
	if (writer == NULL)
	{
		return -1;
	}

	ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElement (writer, root);
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttribute (writer, "version", version);
	if (ret < 0) goto out;

	if (comment != NULL)
	{
		ret = xmlTextWriterWriteComment (writer, comment);
		if (ret < 0) goto out;
	}

	node = first_node;
	while (node != NULL)
	{
		GPtrArray *children;
		guint n_exceptions, i;
		GSList *exceptions = NULL;

		n_exceptions = va_arg (argptr, guint);
		for (i=0; i < n_exceptions; i++)
		{
			exceptions = g_slist_prepend (exceptions,
						      va_arg (argptr, EphyNode *));
		}

		children = ephy_node_get_children (node);
		for (i=0; i < children->len; i++)
		{
			EphyNode *kid;
		
			kid = g_ptr_array_index (children, i);
		
			if (g_slist_find (exceptions, kid) == NULL)
			{
				ret = ephy_node_write_to_xml (kid, writer);
				if (ret < 0) break;
			}
		}
		if (ret < 0) break;

		g_slist_free (exceptions);

		node = va_arg (argptr, EphyNode *);
	}
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* root */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndDocument (writer);
	if (ret < 0) goto out;

out:
	xmlFreeTextWriter (writer);

	STOP_PROFILER ("Saving node db")

	return ret >= 0 ? 0 : -1;
}

int
ephy_node_db_write_to_xml_safe (EphyNodeDb *db,
				const xmlChar *filename,
				const xmlChar *root,
				const xmlChar *version,
				const xmlChar *comment,
				EphyNode *node, ...)
{
	va_list argptr;
	int ret = 0;
	char *tmp_file;

	tmp_file = g_strconcat (filename, ".tmp", NULL);

	va_start (argptr, node);
 
	ret = ephy_node_db_write_to_xml_valist
		(db, tmp_file, root, version, comment, node, argptr);

	va_end (argptr);

	if (ret < 0)
	{
		g_warning ("Failed to write XML data to %s", tmp_file);
		goto failed;
	}

	if (ephy_file_switch_temp_file (filename, tmp_file) == FALSE)
	{
		ret = -1;
	}

failed:
	g_free (tmp_file);

	return ret;
}
