/* 
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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
 *
 */

#include "config.h"

#include "ephy-node-db.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* FIXME I want to find a better way to deal with "root" nodes */
#define RESERVED_IDS 30

enum
{
	PROP_0,
	PROP_NAME,
	PROP_IMMUTABLE,
};

#define EPHY_NODE_DB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NODE_DB, EphyNodeDbPrivate))

struct _EphyNodeDbPrivate
{
	char *name;
	gboolean immutable;

	guint id_factory;

	GPtrArray *id_to_node;
};

static GObjectClass *parent_class = NULL;

static void
ephy_node_db_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	EphyNodeDb *db = EPHY_NODE_DB (object);

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
	EphyNodeDb *db = EPHY_NODE_DB (object);

	switch (prop_id)
	{
		case PROP_NAME:
			db->priv->name = g_value_dup_string (value);
			break;
		case PROP_IMMUTABLE:
			ephy_node_db_set_immutable (db, g_value_get_boolean (value));
			break;
	}
}

static void
ephy_node_db_free_func (EphyNode *node)
{
    if (node)
	    ephy_node_unref (node);
}

static void
ephy_node_db_init (EphyNodeDb *db)
{
	db->priv = EPHY_NODE_DB_GET_PRIVATE (db);

	/* id to node */
	db->priv->id_to_node = g_ptr_array_new_with_free_func ((GDestroyNotify)ephy_node_db_free_func);

	/* id factory */
	db->priv->id_factory = RESERVED_IDS;
}

static void
ephy_node_db_finalize (GObject *object)
{
	EphyNodeDb *db = EPHY_NODE_DB (object);

	g_ptr_array_free (db->priv->id_to_node, TRUE);

	g_free (db->priv->name);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * ephy_node_db_new:
 * @name: the name of the new #EphyNodeDb
 *
 * Creates and returns a new #EphyNodeDb, named @name.
 *
 * Return value: the new #EphyNodeDb
 **/
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
node_from_id_real (EphyNodeDb *db, guint id)
{
	EphyNode *ret = NULL;

	if (id < db->priv->id_to_node->len)
		ret = g_ptr_array_index (db->priv->id_to_node, id);

	return ret;
}

/**
 * ephy_node_db_get_name:
 * @db: an #EphyNodeDb
 *
 * Return value: the name of @db
 **/
const char *
ephy_node_db_get_name (EphyNodeDb *db)
{
	return db->priv->name;
}

/**
 * ephy_node_db_is_immutable:
 * @db: an #EphyNodeDb
 *
 * Return value: %TRUE if @db is immutable
 **/
gboolean
ephy_node_db_is_immutable (EphyNodeDb *db)
{
	return db->priv->immutable;
}

/**
 * ephy_node_db_set_immutable:
 * @db: an #EphyNodeDb
 * @immutable: %TRUE to make @db immutable
 *
 * If @immutable is %TRUE, sets @db immutable (read-only). Otherwise, sets @db
 * to be read-write.
 **/
void
ephy_node_db_set_immutable (EphyNodeDb *db, gboolean immutable)
{
	db->priv->immutable = immutable;

	g_object_notify (G_OBJECT (db), "immutable");
}

/**
 * ephy_node_db_get_node_from_id:
 * @db: an #EphyNodeDb
 * @id: an id specifying an #EphyNode in @db
 *
 * Returns the #EphyNode with id @id from @db, or %NULL if no such id exists.
 *
 * Return value: (transfer none): an #EphyNode
 **/
EphyNode *
ephy_node_db_get_node_from_id (EphyNodeDb *db, guint id)
{
	EphyNode *ret = NULL;

	ret = node_from_id_real (db, id);

	return ret;
}

guint
_ephy_node_db_new_id (EphyNodeDb *db)
{
	guint ret;

	while (node_from_id_real (db, db->priv->id_factory) != NULL)
	{
		db->priv->id_factory++;
	}

	ret = db->priv->id_factory;

	return ret;
}

void
_ephy_node_db_add_id (EphyNodeDb *db,
		      guint id,
		      EphyNode *node)
{
	/* resize array if needed */
	if (id >= db->priv->id_to_node->len)
		g_ptr_array_set_size (db->priv->id_to_node, id + 1);

	g_ptr_array_index (db->priv->id_to_node, id) = node;
}

void
_ephy_node_db_remove_id (EphyNodeDb *db,
			 guint id)
{
	g_ptr_array_index (db->priv->id_to_node, id) = NULL;

	/* reset id factory so we use the freed node id */
	db->priv->id_factory = RESERVED_IDS;
}

/**
 * ephy_node_db_load_from_file:
 * @db: a new #EphyNodeDb
 * @xml_file: the filename from which @db will be populated
 * @xml_root: the root element in @xml_file
 * @xml_version: the required version attribute in the @xml_root
 *
 * Populates @db with data from @xml_file. The node database will be populated
 * with everything inside of the @xml_root tag from @xml_file. If @xml_version
 * is different from the version attribute of the @xml_root element, this
 * function will fail.
 *
 * The data will most probably have been stored using
 * ephy_node_db_write_to_xml_safe().
 *
 * Return value: %TRUE if successful
 **/
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

	LOG ("ephy_node_db_load_from_file %s", xml_file);

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

		if (xmlStrEqual (name, (const xmlChar *)"node")
		    && type == XML_READER_TYPE_ELEMENT)
		{
			xmlNodePtr subtree;

			/* grow the subtree and load the node from it */
			subtree = xmlTextReaderExpand (reader);

			if (subtree != NULL)
			{
				ephy_node_new_from_xml (db, subtree);
			}			
				
			skip = TRUE;
		}
		else if (xmlStrEqual (name, xml_root)
			 && type == XML_READER_TYPE_ELEMENT)
		{
			xmlChar *version;

			/* check version info */
			version = xmlTextReaderGetAttribute (reader, (const xmlChar *)"version");
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
				  xmlBuffer *buffer,
				  const xmlChar *root,
				  const xmlChar *version,
				  const xmlChar *comment,
				  EphyNode *first_node,
				  va_list argptr)
{
	xmlTextWriterPtr writer;
	EphyNode *node;
	int ret;

	START_PROFILER ("Saving node db")

	/* FIXME: do we want to turn compression on ? */
	writer = xmlNewTextWriterMemory (buffer, 0);
	if (writer == NULL)
	{
		return -1;
	}

	ret = xmlTextWriterSetIndent (writer, 1);
	if (ret < 0) goto out;

	ret = xmlTextWriterSetIndentString (writer, (const xmlChar *)"  ");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElement (writer, root);
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttribute (writer, (const xmlChar *)"version", version);
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
		EphyNodeFilterFunc filter;
		gpointer user_data;
		int i;

		filter = va_arg (argptr, EphyNodeFilterFunc);
		user_data = va_arg (argptr, gpointer);

		children = ephy_node_get_children (node);
		for (i = 0; i < children->len; i++)
		{
			EphyNode *kid;
		
			kid = g_ptr_array_index (children, i);
		
			if (!filter || filter (kid, user_data))
			{
				ret = ephy_node_write_to_xml (kid, writer);
				if (ret < 0) break;
			}
		}
		if (ret < 0) break;

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

/**
 * ephy_node_db_write_to_xml_safe:
 * @db: an #EphyNodeDb
 * @filename: the XML file in which @db's data will be stored
 * @root: the desired root element in @filename
 * @version: the version attribute to the @root element
 * @comment: a comment to place directly inside the @root element of @filename
 * @node: The first node of data to write
 * @Varargs: number of exceptions, list of their #EphyNodes, and more such
 * 	     sequences, followed by %NULL
 *
 * Writes @db's data to an XML file for storage. The data can be retrieved in
 * the future using ephy_node_db_load_from_file().
 *
 * The function arguments are straightforward until @node, at which point some
 * explanation is necessary.
 *
 * The variable argument list starts at @node, which is an #EphyNode containing
 * data to write to @filename. The next argument is an integer specifying the
 * number of <quote>exception</quote> nodes. After this integer, that number of
 * #EphyNode arguments should be given. Each of these <quote>exception</quote>
 * nodes determines which data out of @node will <emphasis>not</emphasis> be
 * written to @filename.
 *
 * To insert all of an #EphyNode's contents without exception, simply give the
 * integer %0 after @node.
 *
 * The remainder of this function's arguments will be groups of such #EphyNode -
 * integer - (list of #EphyNode<!-- -->s). Finally, the last argument must be
 * %NULL.
 *
 * Return value: %0 on success or a negative number on failure
 **/
int
ephy_node_db_write_to_xml_safe (EphyNodeDb *db,
				const xmlChar *filename,
				const xmlChar *root,
				const xmlChar *version,
				const xmlChar *comment,
				EphyNode *node, ...)
{
	va_list argptr;
	xmlBuffer *buffer;
	GError *error = NULL;
	int ret = 0;

	LOG ("Saving node db to %s", filename);

	va_start (argptr, node);

	buffer = xmlBufferCreate ();
	ret = ephy_node_db_write_to_xml_valist
		(db, buffer, root, version, comment, node, argptr);

	va_end (argptr);

	if (ret < 0)
	{
		g_warning ("Failed to write XML data");
		goto failed;
	}

	if (g_file_set_contents ((const char *)filename, (const char *)buffer->content, buffer->use, &error) == FALSE)
	{
		g_warning ("Error saving EphyNodeDB as XML: %s", error->message);
		g_error_free (error);
		ret = -1;
	}

failed:
	xmlBufferFree (buffer);

	return ret;
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
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					PROP_IMMUTABLE,
					g_param_spec_boolean ("immutable",
							      "Immutable",
							      "Immutable",
							      FALSE,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNodeDbPrivate));
}

GType
ephy_node_db_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
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

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyNodeDb",
					       &our_info, 0);
	}

	return type;
}
