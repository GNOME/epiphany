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

#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gdk/gdk.h>
#include <time.h>

#include "ephy-node.h"

typedef struct
{
	EphyNode *node;
	int id;
	EphyNodeCallback callback;
	EphyNodeSignalType type;
	gpointer data;
	gboolean invalidated;
} EphyNodeSignalData;

typedef struct
{
	EphyNode *node;
	guint index;
} EphyNodeParent;

typedef struct
{
	EphyNode *node;
	guint property_id;
} EphyNodeChange;

struct _EphyNode
{
	int ref_count;

	guint id;

	GPtrArray *properties;

	GHashTable *parents;
	GPtrArray *children;

	GHashTable *signals;
	int signal_id;
	guint emissions;
	guint invalidated_signals;
	guint is_drag_source : 1;
	guint is_drag_dest : 1;

	EphyNodeDb *db;
};

typedef struct
{
	EphyNodeSignalType type;
	va_list valist;
} ENESCData;

static gboolean
int_equal (gconstpointer a,
	   gconstpointer b)
{
	return GPOINTER_TO_INT (a) == GPOINTER_TO_INT (b);
}

static guint
int_hash (gconstpointer a)
{
	return GPOINTER_TO_INT (a);
}

static void
callback (long id, EphyNodeSignalData *data, gpointer *dummy)
{
	ENESCData *user_data;
	va_list valist;

	if (data->invalidated) return;

	user_data = (ENESCData *) dummy;

	G_VA_COPY(valist, user_data->valist);

	if (data->type != user_data->type) return;

	switch (data->type)
	{
		case EPHY_NODE_DESTROY:
		case EPHY_NODE_RESTORED:
			data->callback (data->node, data->data);
		break;

		case EPHY_NODE_CHANGED:
		{
			guint property_id;

			property_id = va_arg (valist, guint);
		
			data->callback (data->node, property_id, data->data);
		}
		break;

		case EPHY_NODE_CHILD_ADDED:
		{
			EphyNode *node;

			node = va_arg (valist, EphyNode *);
		
			data->callback (data->node, node, data->data);
		}
		break;

		case EPHY_NODE_CHILD_CHANGED:
		{
			EphyNode *node;
			guint property_id;

			node = va_arg (valist, EphyNode *);
			property_id = va_arg (valist, guint);
		
			data->callback (data->node, node, property_id, data->data);
		}
		break;

		case EPHY_NODE_CHILD_REMOVED:
		{
			EphyNode *node;
			guint last_index;

			node = va_arg (valist, EphyNode *);
			last_index = va_arg (valist, guint);

			data->callback (data->node, node, last_index, data->data);
		}
		break;

		case EPHY_NODE_CHILDREN_REORDERED:
			data->callback (data->node, va_arg (valist, int *), data->data);
		break;
	}

        va_end(valist);
}

static gboolean
remove_invalidated_signals (long id,
			    EphyNodeSignalData *data,
			    gpointer user_data)
{
	return data->invalidated;
}

static void
ephy_node_emit_signal (EphyNode *node, EphyNodeSignalType type, ...)
{
	ENESCData data;

	++node->emissions;

	va_start (data.valist, type);

	data.type = type;

	g_hash_table_foreach (node->signals,
			      (GHFunc) callback,
			      &data);

	va_end (data.valist);

	if (G_UNLIKELY (--node->emissions == 0 && node->invalidated_signals))
	{
		int removed;

		removed = g_hash_table_foreach_remove
				(node->signals,
				 (GHRFunc) remove_invalidated_signals,
				 NULL);
		g_assert (removed == node->invalidated_signals);

		node->invalidated_signals = 0;
	}
}

static inline void
real_remove_child (EphyNode *node,
		   EphyNode *child,
		   gboolean remove_from_parent,
		   gboolean remove_from_child)
{
	EphyNodeParent *node_info;

	node_info = g_hash_table_lookup (child->parents,
			                 GINT_TO_POINTER (node->id));

	if (remove_from_parent) {
		guint i;
		guint old_index;

		old_index = node_info->index;

		g_ptr_array_remove_index (node->children,
					  node_info->index);

		/* correct indices on kids */
		for (i = node_info->index; i < node->children->len; i++) {
			EphyNode *borked_node;
			EphyNodeParent *borked_node_info;

			borked_node = g_ptr_array_index (node->children, i);


			borked_node_info = g_hash_table_lookup (borked_node->parents,
						                GINT_TO_POINTER (node->id));
			borked_node_info->index--;
		}

		ephy_node_emit_signal (node, EPHY_NODE_CHILD_REMOVED, child, old_index);
	}

	if (remove_from_child) {
		g_hash_table_remove (child->parents,
				     GINT_TO_POINTER (node->id));
	}
}

static void
remove_child (long id,
	      EphyNodeParent *node_info,
	      EphyNode *node)
{
	real_remove_child (node_info->node, node, TRUE, FALSE);
}

static void
signal_object_weak_notify (EphyNodeSignalData *signal_data,
                           GObject *where_the_object_was)
{
	signal_data->data = NULL;
	ephy_node_signal_disconnect (signal_data->node, signal_data->id);
}

static void
destroy_signal_data (EphyNodeSignalData *signal_data)
{
	if (signal_data->data)
	{
		g_object_weak_unref (G_OBJECT (signal_data->data),
				     (GWeakNotify)signal_object_weak_notify,
				     signal_data);
	}
        
        g_slice_free (EphyNodeSignalData, signal_data);
}

static void
node_parent_free (EphyNodeParent *parent)
{
	g_slice_free (EphyNodeParent, parent);
}

static void
ephy_node_destroy (EphyNode *node)
{
	guint i;

	ephy_node_emit_signal (node, EPHY_NODE_DESTROY);

        /* Remove from parents. */
	g_hash_table_foreach (node->parents,
			      (GHFunc) remove_child,
			      node);
	g_hash_table_destroy (node->parents);

        /* Remove children. */
	for (i = 0; i < node->children->len; i++) {
		EphyNode *child;

		child = g_ptr_array_index (node->children, i);

		real_remove_child (node, child, FALSE, TRUE);
	}
	g_ptr_array_free (node->children, TRUE);
        
        /* Remove signals. */
	g_hash_table_destroy (node->signals);

        /* Remove id. */
	_ephy_node_db_remove_id (node->db, node->id);

        /* Remove properties. */
	for (i = 0; i < node->properties->len; i++) {
		GValue *val;

		val = g_ptr_array_index (node->properties, i);

		if (val != NULL) {
			g_value_unset (val);
			g_slice_free (GValue, val);
		}
	}
	g_ptr_array_free (node->properties, TRUE);

	g_slice_free (EphyNode, node);
}

EphyNode *
ephy_node_new (EphyNodeDb *db)
{
	long id;

	g_return_val_if_fail (EPHY_IS_NODE_DB (db), NULL);

	if (ephy_node_db_is_immutable (db)) return NULL;

	id = _ephy_node_db_new_id (db);

	return ephy_node_new_with_id (db, id);
}

EphyNode *
ephy_node_new_with_id (EphyNodeDb *db, guint reserved_id)
{
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_NODE_DB (db), NULL);

	if (ephy_node_db_is_immutable (db)) return NULL;

	node = g_slice_new0 (EphyNode);

	node->ref_count = 1;

	node->id = reserved_id;

	node->db = db;

	node->properties = g_ptr_array_new ();

	node->children = g_ptr_array_new ();

	node->parents = g_hash_table_new_full
          (int_hash, int_equal, NULL, (GDestroyNotify) node_parent_free);

	node->signals = g_hash_table_new_full
          (int_hash, int_equal, NULL,
           (GDestroyNotify)destroy_signal_data);

	node->signal_id = 0;
	node->emissions = 0;
	node->invalidated_signals = 0;
	node->is_drag_source = TRUE;
	node->is_drag_dest = TRUE;

	_ephy_node_db_add_id (db, reserved_id, node);

	return node;
}

/**
 * ephy_node_get_db:
 *
 * Return value: (transfer none):
 **/
EphyNodeDb *
ephy_node_get_db (EphyNode *node)
{
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	
	return node->db;
}

guint
ephy_node_get_id (EphyNode *node)
{
	long ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), G_MAXUINT);

	ret = node->id;

	return ret;
}

void
ephy_node_ref (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	node->ref_count++;
}

void
ephy_node_unref (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	node->ref_count--;

	if (node->ref_count <= 0) {
		ephy_node_destroy (node);
	}
}

static void
child_changed (guint id,
	       EphyNodeParent *node_info,
	       EphyNodeChange *change)
{
	ephy_node_emit_signal (node_info->node, EPHY_NODE_CHILD_CHANGED,
			       change->node, change->property_id);
}

static inline void
real_set_property (EphyNode *node,
		   guint property_id,
		   GValue *value)
{
	GValue *old;

	if (property_id >= node->properties->len) {
		g_ptr_array_set_size (node->properties, property_id + 1);
	}

	old = g_ptr_array_index (node->properties, property_id);
	if (old != NULL) {
		g_value_unset (old);
		g_slice_free (GValue, old);
	}

	g_ptr_array_index (node->properties, property_id) = value;
}

static inline void
ephy_node_set_property_internal (EphyNode *node,
		        	 guint property_id,
		        	 GValue *value)
{
	EphyNodeChange change;

	real_set_property (node, property_id, value);

	change.node = node;
	change.property_id = property_id;
	g_hash_table_foreach (node->parents,
			      (GHFunc) child_changed,
			      &change);
    
	ephy_node_emit_signal (node, EPHY_NODE_CHANGED, property_id);

}

void
ephy_node_set_property (EphyNode *node,
		        guint property_id,
		        const GValue *value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (value != NULL);

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_VALUE_TYPE (value));
	g_value_copy (value, new);

	ephy_node_set_property_internal (node, property_id, new);
}

/**
 * ephy_node_get_property:
 * @property_id: the identifier for the property
 * @value: (out): the variable to hold the value
 */
gboolean
ephy_node_get_property (EphyNode *node,
		        guint property_id,
		        GValue *value)
{
	GValue *ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	if (property_id >= node->properties->len) {
		return FALSE;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return FALSE;
	}

	g_value_init (value, G_VALUE_TYPE (ret));
	g_value_copy (ret, value);

	return TRUE;
}

void
ephy_node_set_property_string (EphyNode *node,
			       guint property_id,
			       const char *value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_TYPE_STRING);
	g_value_set_string (new, value);

	ephy_node_set_property_internal (node, property_id, new);
}

const char *
ephy_node_get_property_string (EphyNode *node,
			       guint property_id)
{
	GValue *ret;
	const char *retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

	if (property_id >= node->properties->len) {
		return NULL;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return NULL;
	}

	retval = g_value_get_string (ret);

	return retval;
}

void
ephy_node_set_property_boolean (EphyNode *node,
			        guint property_id,
			        gboolean value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_TYPE_BOOLEAN);
	g_value_set_boolean (new, value);

	ephy_node_set_property_internal (node, property_id, new);
}

gboolean
ephy_node_get_property_boolean (EphyNode *node,
			        guint property_id)
{
	GValue *ret;
	gboolean retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);

	if (property_id >= node->properties->len) {
		return FALSE;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return FALSE;
	}

	retval = g_value_get_boolean (ret);

	return retval;
}

void
ephy_node_set_property_long (EphyNode *node,
			     guint property_id,
			     long value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_TYPE_LONG);
	g_value_set_long (new, value);

	ephy_node_set_property_internal (node, property_id, new);
}

long
ephy_node_get_property_long (EphyNode *node,
			     guint property_id)
{
	GValue *ret;
	long retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	if (property_id >= node->properties->len) {
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return -1;
	}

	retval = g_value_get_long (ret);

	return retval;
}

void
ephy_node_set_property_int (EphyNode *node,
			    guint property_id,
			    int value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_TYPE_INT);
	g_value_set_int (new, value);

	ephy_node_set_property_internal (node, property_id, new);
}

int
ephy_node_get_property_int (EphyNode *node,
			    guint property_id)
{
	GValue *ret;
	int retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	if (property_id >= node->properties->len) {
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return -1;
	}

	retval = g_value_get_int (ret);

	return retval;
}

void
ephy_node_set_property_double (EphyNode *node,
			       guint property_id,
			       double value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_TYPE_DOUBLE);
	g_value_set_double (new, value);

	ephy_node_set_property_internal (node, property_id, new);
}

double
ephy_node_get_property_double (EphyNode *node,
			       guint property_id)
{
	GValue *ret;
	double retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	if (property_id >= node->properties->len) {
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return -1;
	}

	retval = g_value_get_double (ret);

	return retval;
}

void
ephy_node_set_property_float (EphyNode *node,
			      guint property_id,
			      float value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	new = g_slice_new0 (GValue);
	g_value_init (new, G_TYPE_FLOAT);
	g_value_set_float (new, value);

	ephy_node_set_property_internal (node, property_id, new);
}

float
ephy_node_get_property_float (EphyNode *node,
			      guint property_id)
{
	GValue *ret;
	float retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	if (property_id >= node->properties->len) {
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return -1;
	}

	retval = g_value_get_float (ret);

	return retval;
}

/**
 * ephy_node_get_property_node:
 *
 * Return value: (transfer none):
 **/
EphyNode *
ephy_node_get_property_node (EphyNode *node,
			     guint property_id)
{
	GValue *ret;
	EphyNode *retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

	if (property_id >= node->properties->len) {
		return NULL;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		return NULL;
	}

	retval = g_value_get_pointer (ret);

	return retval;
}

typedef struct
{
	xmlTextWriterPtr writer;
	int ret;
} ForEachData;

static void
write_parent (guint id,
	      EphyNodeParent *node_info,
	      ForEachData* data)
{
	xmlTextWriterPtr writer = data->writer;

	/* there already was an error, do nothing. this works around
	 * the fact that g_hash_table_foreach cannot be cancelled.
	 */
	if (data->ret < 0) return;

	data->ret = xmlTextWriterStartElement (writer, (const xmlChar *)"parent");
	if (data->ret < 0) return;

	data->ret = xmlTextWriterWriteFormatAttribute
			(writer, (const xmlChar *)"id", "%d", node_info->node->id);
	if (data->ret < 0) return;

	data->ret = xmlTextWriterEndElement (writer); /* parent */
	if (data->ret < 0) return;
}

static inline int
safe_write_string (xmlTextWriterPtr writer,
		   const xmlChar *string)
{
	int ret;
	xmlChar *copy, *p;

	if (!string)
		return 0;

	/* http://www.w3.org/TR/REC-xml/#sec-well-formed :
	   Character Range
	   [2]     Char       ::=          #x9 | #xA | #xD | [#x20-#xD7FF] |
	   [#xE000-#xFFFD] | [#x10000-#x10FFFF]
	   any Unicode character, excluding the surrogate blocks, FFFE, and FFFF.
	*/

	copy = xmlStrdup (string);
	for (p = copy; *p; p++)
	{
		xmlChar c = *p;
		if (G_UNLIKELY (c < 0x20 && c != 0xd && c != 0xa && c != 0x9)) {
			*p = 0x20;
		}
	}

	ret = xmlTextWriterWriteString (writer, copy);
	xmlFree (copy);

	return ret;
}

int
ephy_node_write_to_xml(EphyNode *node,
		       xmlTextWriterPtr writer)
{
	xmlChar xml_buf[G_ASCII_DTOSTR_BUF_SIZE];
	guint i;
	int ret;
	ForEachData data;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (writer != NULL, -1);

	/* start writing the node */
	ret = xmlTextWriterStartElement (writer, (const xmlChar *)"node");
	if (ret < 0) goto out;

	/* write node id */
	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"id", "%d", node->id);
	if (ret < 0) goto out;

	/* write node properties */
	for (i = 0; i < node->properties->len; i++)
	{
		GValue *value;

		value = g_ptr_array_index (node->properties, i);

		if (value == NULL) continue;
		if (G_VALUE_TYPE (value) == G_TYPE_STRING &&
		    g_value_get_string (value) == NULL) continue;

		ret = xmlTextWriterStartElement (writer, (const xmlChar *)"property");
		if (ret < 0) break;

		ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"id", "%d", i);
		if (ret < 0) break;

		ret = xmlTextWriterWriteAttribute
			(writer, (const xmlChar *)"value_type", 
			 (const xmlChar *)g_type_name (G_VALUE_TYPE (value)));
		if (ret < 0) break;

		switch (G_VALUE_TYPE (value))
		{
		case G_TYPE_STRING:
			ret = safe_write_string
				(writer, (const xmlChar *)g_value_get_string (value));
			break;
		case G_TYPE_BOOLEAN:
			ret = xmlTextWriterWriteFormatString
				(writer, "%d", g_value_get_boolean (value));
			break;
		case G_TYPE_INT:
			ret = xmlTextWriterWriteFormatString
				(writer, "%d", g_value_get_int (value));
			break;
		case G_TYPE_LONG:
			ret = xmlTextWriterWriteFormatString
				(writer, "%ld", g_value_get_long (value));
			break;
		case G_TYPE_FLOAT:
			g_ascii_dtostr ((gchar *)xml_buf, sizeof (xml_buf), 
					g_value_get_float (value));
			ret = xmlTextWriterWriteString (writer, xml_buf);
			break;
		case G_TYPE_DOUBLE:
			g_ascii_dtostr ((gchar *)xml_buf, sizeof (xml_buf),
					g_value_get_double (value));
			ret = xmlTextWriterWriteString (writer, xml_buf);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		if (ret < 0) break;
	
		ret = xmlTextWriterEndElement (writer); /* property */
		if (ret < 0) break;
	}
	if (ret < 0) goto out;

	/* now write parent node ids */
	data.writer = writer;
	data.ret = 0;

	g_hash_table_foreach (node->parents,
			      (GHFunc) write_parent,
			      &data);
	ret = data.ret;
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* node */
	if (ret < 0) goto out;

out:
	return ret >= 0 ? 0 : -1;
}

static inline void
real_add_child (EphyNode *node,
		EphyNode *child)
{
	EphyNodeParent *node_info;

	if (g_hash_table_lookup (child->parents,
				 GINT_TO_POINTER (node->id)) != NULL) {
		return;
	}

	g_ptr_array_add (node->children, child);

	node_info = g_slice_new0 (EphyNodeParent);
	node_info->node  = node;
	node_info->index = node->children->len - 1;

	g_hash_table_insert (child->parents,
			     GINT_TO_POINTER (node->id),
			     node_info);
}

EphyNode *
ephy_node_new_from_xml (EphyNodeDb *db, xmlNodePtr xml_node)
{
	EphyNode *node;
	xmlNodePtr xml_child;
	xmlChar *xml;
	long id;

	g_return_val_if_fail (EPHY_IS_NODE_DB (db), NULL);
	g_return_val_if_fail (xml_node != NULL, NULL);

	if (ephy_node_db_is_immutable (db)) return NULL; 

	xml = xmlGetProp (xml_node, (const xmlChar *)"id");
	if (xml == NULL)
		return NULL;
	id = atol ((const char *)xml);
	xmlFree (xml);

	node = ephy_node_new_with_id (db, id);

	for (xml_child = xml_node->children; xml_child != NULL; xml_child = xml_child->next) {
		if (strcmp ((const char *)xml_child->name, "parent") == 0) {
			EphyNode *parent;
			long parent_id;

			xml = xmlGetProp (xml_child, (const xmlChar *)"id");
			g_assert (xml != NULL);
			parent_id = atol ((const char *)xml);
			xmlFree (xml);

			parent = ephy_node_db_get_node_from_id (db, parent_id);

			if (parent != NULL)
			{
				real_add_child (parent, node);

				ephy_node_emit_signal (parent, EPHY_NODE_CHILD_ADDED, node);
			}
		} else if (strcmp ((const char *)xml_child->name, "property") == 0) {
			GValue *value;
			xmlChar *xmlType, *xmlValue;
			int property_id;

			xml = xmlGetProp (xml_child, (const xmlChar *)"id");
			property_id = atoi ((const char *)xml);
			xmlFree (xml);

			xmlType = xmlGetProp (xml_child, (const xmlChar *)"value_type");
			xmlValue = xmlNodeGetContent (xml_child);

			value = g_slice_new0 (GValue);

			if (xmlStrEqual (xmlType, (const xmlChar *) "gchararray"))
			{
				g_value_init (value, G_TYPE_STRING);
				g_value_set_string (value, (const gchar *)xmlValue);
			}
			else if (xmlStrEqual (xmlType, (const xmlChar *) "gint"))
			{
				g_value_init (value, G_TYPE_INT);
				g_value_set_int (value, atoi ((const char *)xmlValue));
			}
			else if (xmlStrEqual (xmlType, (const xmlChar *) "gboolean"))
			{
				g_value_init (value, G_TYPE_BOOLEAN);
				g_value_set_boolean (value, atoi ((const char *)xmlValue));
			}
			else if (xmlStrEqual (xmlType, (const xmlChar *) "glong"))
			{
				g_value_init (value, G_TYPE_LONG);
				g_value_set_long (value, atol ((const char *)xmlValue));
			}
			else if (xmlStrEqual (xmlType, (const xmlChar *) "gfloat"))
			{
				g_value_init (value, G_TYPE_FLOAT);
				g_value_set_float (value, g_ascii_strtod ((const gchar *)xmlValue, NULL));
			}
			else if (xmlStrEqual (xmlType, (const xmlChar *) "gdouble"))
			{
				g_value_init (value, G_TYPE_DOUBLE);
				g_value_set_double (value, g_ascii_strtod ((const gchar *)xmlValue, NULL));
			}
			else if (xmlStrEqual (xmlType, (const xmlChar *) "gpointer"))
			{
				EphyNode *property_node;

				property_node = ephy_node_db_get_node_from_id (db, atol ((const char *)xmlValue));

				g_value_set_pointer (value, property_node);
				break;
			}
			else
			{
				g_assert_not_reached ();
			}

			real_set_property (node, property_id, value);

			xmlFree (xmlValue);
			xmlFree (xmlType);
		}
	}

	ephy_node_emit_signal (node, EPHY_NODE_RESTORED);

	return node;
}

void
ephy_node_add_child (EphyNode *node,
		     EphyNode *child)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;
	
	real_add_child (node, child);

	ephy_node_emit_signal (node, EPHY_NODE_CHILD_ADDED, child);
}

void
ephy_node_remove_child (EphyNode *node,
		        EphyNode *child)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	if (ephy_node_db_is_immutable (node->db)) return;

	real_remove_child (node, child, TRUE, TRUE);
}

gboolean
ephy_node_has_child (EphyNode *node,
		     EphyNode *child)
{
	gboolean ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);
	
	ret = (g_hash_table_lookup (child->parents,
				    GINT_TO_POINTER (node->id)) != NULL);

	return ret;
}

static int
ephy_node_real_get_child_index (EphyNode *node,
			   EphyNode *child)
{
	EphyNodeParent *node_info;
	int ret;

	node_info = g_hash_table_lookup (child->parents,
					 GINT_TO_POINTER (node->id));

	if (node_info == NULL)
		return -1;

	ret = node_info->index;

	return ret;
}

/**
 * ephy_node_sort_children:
 * @node: an #EphyNode
 * @compare_func: (scope call): function to compare children
 *
 * Sorts the children of @node using @compare_func.
 *
 **/
void
ephy_node_sort_children (EphyNode *node,
			 GCompareFunc compare_func)
{
	GPtrArray *newkids;
	int i, *new_order;

	if (ephy_node_db_is_immutable (node->db)) return;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (compare_func != NULL);

	newkids = g_ptr_array_new ();
	g_ptr_array_set_size (newkids, node->children->len);

	/* dup the array */
	for (i = 0; i < node->children->len; i++)
	{
		g_ptr_array_index (newkids, i) = g_ptr_array_index (node->children, i);
	}

	g_ptr_array_sort (newkids, compare_func);

	new_order = g_new (int, newkids->len);
	memset (new_order, -1, sizeof (int) * newkids->len);

	for (i = 0; i < newkids->len; i++)
	{
		EphyNodeParent *node_info;
		EphyNode *child;

		child = g_ptr_array_index (newkids, i);
		new_order[ephy_node_real_get_child_index (node, child)] = i;
		node_info = g_hash_table_lookup (child->parents,
					         GINT_TO_POINTER (node->id));
		node_info->index = i;
	}

	g_ptr_array_free (node->children, FALSE);
	node->children = newkids;

	ephy_node_emit_signal (node, EPHY_NODE_CHILDREN_REORDERED, new_order);

	g_free (new_order);
}

void
ephy_node_reorder_children (EphyNode *node,
			    int *new_order)
{
	GPtrArray *newkids;
	int i;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (new_order != NULL);

	if (ephy_node_db_is_immutable (node->db)) return;

	newkids = g_ptr_array_new ();
	g_ptr_array_set_size (newkids, node->children->len);

	for (i = 0; i < node->children->len; i++) {
		EphyNode *child;
		EphyNodeParent *node_info;

		child = g_ptr_array_index (node->children, i);

		g_ptr_array_index (newkids, new_order[i]) = child;

		node_info = g_hash_table_lookup (child->parents,
					         GINT_TO_POINTER (node->id));
		node_info->index = new_order[i];
	}

	g_ptr_array_free (node->children, FALSE);
	node->children = newkids;

	ephy_node_emit_signal (node, EPHY_NODE_CHILDREN_REORDERED, new_order);
}

/**
 * ephy_node_get_children:
 *
 * Return value: (array) (element-type EphyNode) (transfer none):
 **/
GPtrArray *
ephy_node_get_children (EphyNode *node)
{
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

	return node->children;
}

int
ephy_node_get_n_children (EphyNode *node)
{
	int ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	ret = node->children->len;

	return ret;
}

/**
 * ephy_node_get_nth_child:
 *
 * Return value: (transfer none):
 **/
EphyNode *
ephy_node_get_nth_child (EphyNode *node,
		         guint n)
{
	EphyNode *ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

	if (n < node->children->len) {
		ret = g_ptr_array_index (node->children, n);
	} else {
		ret = NULL;
	}

	return ret;
}

static inline int
get_child_index_real (EphyNode *node,
		      EphyNode *child)
{
	EphyNodeParent *node_info;

	node_info = g_hash_table_lookup (child->parents,
					 GINT_TO_POINTER (node->id));

	if (node_info == NULL)
		return -1;

	return node_info->index;
}


int
ephy_node_get_child_index (EphyNode *node,
			   EphyNode *child)
{
	int ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (EPHY_IS_NODE (child), -1);

	ret = ephy_node_real_get_child_index (node, child);

	return ret;
}

/**
 * ephy_node_get_next_child:
 *
 * Return value: (transfer none):
 **/
EphyNode *
ephy_node_get_next_child (EphyNode *node,
			  EphyNode *child)
{
	EphyNode *ret;
	guint idx;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (EPHY_IS_NODE (child), NULL);
	
	idx = get_child_index_real (node, child);

	if ((idx + 1) < node->children->len) {
		ret = g_ptr_array_index (node->children, idx + 1);
	} else {
		ret = NULL;
	}

	return ret;
}

/**
 * ephy_node_get_previous_child:
 *
 * Return value: (transfer none):
 **/
EphyNode *
ephy_node_get_previous_child (EphyNode *node,
			      EphyNode *child)
{
	EphyNode *ret;
	int idx;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (EPHY_IS_NODE (child), NULL);
	
	idx = get_child_index_real (node, child);

	if ((idx - 1) >= 0) {
		ret = g_ptr_array_index (node->children, idx - 1);
	} else {
		ret = NULL;
	}

	return ret;
}

/**
 * ephy_node_signal_connect_object:
 * @node: an #EphyNode
 * @type: signal type
 * @callback: (scope notified): the callback to connect
 * @object: data to pass to @callback
 *
 * Connects a callback function to the @type signal of @node.
 *
 * Returns: an identifier for the connected signal
 **/
int
ephy_node_signal_connect_object (EphyNode *node,
				 EphyNodeSignalType type,
				 EphyNodeCallback callback,
				 GObject *object)
{
	EphyNodeSignalData *signal_data;
	int ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	/* FIXME: */
	g_return_val_if_fail (node->emissions == 0, -1);

	signal_data = g_slice_new0 (EphyNodeSignalData);
	signal_data->node = node;
	signal_data->id = node->signal_id;
	signal_data->callback = callback;
	signal_data->type = type;
	signal_data->data = object;

	g_hash_table_insert (node->signals,
			     GINT_TO_POINTER (node->signal_id),
			     signal_data);
	if (object)
	{
		g_object_weak_ref (object,
				   (GWeakNotify)signal_object_weak_notify,
				   signal_data);
	}

	ret = node->signal_id;
	node->signal_id++;

	return ret;
}

static gboolean
remove_matching_signal_data (gpointer key,
			     EphyNodeSignalData *signal_data,
			     EphyNodeSignalData *user_data)
{
	return (user_data->data == signal_data->data &&
		user_data->type == signal_data->type &&
		user_data->callback == signal_data->callback);           
}

static void
invalidate_matching_signal_data (gpointer key,
				 EphyNodeSignalData *signal_data,
				 EphyNodeSignalData *user_data)
{
	if (user_data->data == signal_data->data &&
	    user_data->type == signal_data->type &&
	    user_data->callback == signal_data->callback &&
	    !signal_data->invalidated)
	{
		signal_data->invalidated = TRUE;
		++signal_data->node->invalidated_signals;
	}
}

/**
 * ephy_node_signal_disconnect_object:
 * @node: an #EphyNode
 * @type: signal type
 * @callback: (scope notified): the callback to disconnect
 * @object: data passed to @callback when it was connected
 *
 * Disconnects @callback from @type in @node. @callback is identified by the
 * @object previously passed in ephy_node_signal_connect_object.
 *
 * Returns: the number of signal handlers removed
 **/
guint
ephy_node_signal_disconnect_object (EphyNode *node,
                                    EphyNodeSignalType type,
                                    EphyNodeCallback callback,
                                    GObject *object)
{
        EphyNodeSignalData user_data;

	g_return_val_if_fail (EPHY_IS_NODE (node), 0);

	user_data.callback = callback;
	user_data.type = type;
	user_data.data = object;

	if (G_LIKELY (node->emissions == 0))
	{
		return g_hash_table_foreach_remove (node->signals,
						    (GHRFunc) remove_matching_signal_data,
						    &user_data);
	}
	else
	{
		g_hash_table_foreach (node->signals,
				      (GHFunc) invalidate_matching_signal_data,
				      &user_data);
		return 0;
	}
}

void
ephy_node_signal_disconnect (EphyNode *node,
			     int signal_id)
{
	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (signal_id != -1);

	if (G_LIKELY (node->emissions == 0))
	{
		g_hash_table_remove (node->signals,
				     GINT_TO_POINTER (signal_id));
	}
	else
	{
		EphyNodeSignalData *data;

		data = g_hash_table_lookup (node->signals,
					    GINT_TO_POINTER (signal_id));
		g_return_if_fail (data != NULL);
		g_return_if_fail (!data->invalidated);

		data->invalidated = TRUE;
		node->invalidated_signals++;
	}
}

void
ephy_node_set_is_drag_source (EphyNode *node,
			      gboolean allow)
{
	node->is_drag_source = allow != FALSE;
}

gboolean
ephy_node_get_is_drag_source (EphyNode *node)
{
	return node->is_drag_source;
}

void
ephy_node_set_is_drag_dest (EphyNode *node,
			    gboolean allow)
{
	node->is_drag_dest = allow != FALSE;
}

gboolean
ephy_node_get_is_drag_dest (EphyNode *node)
{
	return node->is_drag_dest;
}

GType
ephy_node_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		type = g_boxed_type_register_static ("EphyNode",
						     (GBoxedCopyFunc) ephy_node_ref,
						     (GBoxedFreeFunc) ephy_node_unref);
	}

	return type;
}
