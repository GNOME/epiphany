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

#include <config.h>
#include <bonobo/bonobo-i18n.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gdk/gdk.h>
#include <time.h>

#include "ephy-node.h"
#include "ephy-string.h"
#include "ephy-thread-helpers.h"

typedef struct
{
	EphyNode *node;
	int id;
	EphyNodeCallback callback;
	EphyNodeSignalType type;
	gpointer data;
} EphyNodeSignalData;

typedef struct
{
	EphyNode *node;
	guint index;
} EphyNodeParent;

struct EphyNode
{
	GStaticRWLock *lock;

	int ref_count;

	gulong id;

	GPtrArray *properties;

	GHashTable *parents;
	GPtrArray *children;

	GHashTable *signals;
	int signal_id;

	EphyNodeDb *db;
};

typedef struct
{
	EphyNodeSignalType type;
	va_list valist;
} ENESCData;

/* evillish hacks to temporarily readlock->writelock and v.v. */
static inline void
write_lock_to_read_lock (EphyNode *node)
{
	g_static_mutex_lock (&node->lock->mutex);
	node->lock->read_counter++;
	g_static_mutex_unlock (&node->lock->mutex);

	g_static_rw_lock_writer_unlock (node->lock);
}

static inline void
read_lock_to_write_lock (EphyNode *node)
{
	g_static_mutex_lock (&node->lock->mutex);
	node->lock->read_counter--;
	g_static_mutex_unlock (&node->lock->mutex);

	g_static_rw_lock_writer_lock (node->lock);
}

static inline void
lock_gdk (void)
{
	if (ephy_thread_helpers_in_main_thread () == FALSE)
		GDK_THREADS_ENTER ();
}

static inline void
unlock_gdk (void)
{
	if (ephy_thread_helpers_in_main_thread () == FALSE)
		GDK_THREADS_LEAVE ();
}

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

	user_data = (ENESCData *) dummy;

	va_copy(valist, user_data->valist);

	if (data->type != user_data->type) return;

	switch (data->type)
	{
		case EPHY_NODE_DESTROY:
		case EPHY_NODE_RESTORED:
			data->callback (data->node, data->data);
		break;

		case EPHY_NODE_CHILD_ADDED:
		case EPHY_NODE_CHILD_CHANGED:
			data->callback (data->node, va_arg (valist, EphyNode *), data->data);
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

static void
ephy_node_emit_signal (EphyNode *node, EphyNodeSignalType type, ...)
{
	ENESCData data;

	va_start (data.valist, type);

	data.type = type;

	g_hash_table_foreach (node->signals,
			      (GHFunc) callback,
			      &data);

	va_end (data.valist);
}

static void
ephy_node_finalize (EphyNode *node)
{
	guint i;

	g_hash_table_destroy (node->signals);
	node->signals = NULL;

	for (i = 0; i < node->properties->len; i++) {
		GValue *val;

		val = g_ptr_array_index (node->properties, i);

		if (val != NULL) {
			g_value_unset (val);
			g_free (val);
		}
	}
	g_ptr_array_free (node->properties, FALSE);

	g_hash_table_destroy (node->parents);

	g_ptr_array_free (node->children, FALSE);

	g_static_rw_lock_free (node->lock);

	g_free (node);
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

			g_static_rw_lock_writer_lock (borked_node->lock);

			borked_node_info = g_hash_table_lookup (borked_node->parents,
						                GINT_TO_POINTER (node->id));
			borked_node_info->index--;

			g_static_rw_lock_writer_unlock (borked_node->lock);
		}

		write_lock_to_read_lock (node);
		write_lock_to_read_lock (child);

		ephy_node_emit_signal (node, EPHY_NODE_CHILD_REMOVED, child, old_index);

		read_lock_to_write_lock (node);
		read_lock_to_write_lock (child);
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
	g_static_rw_lock_writer_lock (node_info->node->lock);

	real_remove_child (node_info->node, node, TRUE, FALSE);

	g_static_rw_lock_writer_unlock (node_info->node->lock);
}

static void
signal_object_weak_notify (EphyNodeSignalData *signal_data,
                           GObject *where_the_object_was)
{
	ephy_node_signal_disconnect (signal_data->node, signal_data->id);
}

static void
unref_signal_objects (long id,
	              EphyNodeSignalData *signal_data,
	              EphyNode *node)
{
	g_object_weak_unref (G_OBJECT (signal_data->data),
			     (GWeakNotify)signal_object_weak_notify,
			     signal_data);
}

static void
ephy_node_dispose (EphyNode *node)
{
	guint i;

	write_lock_to_read_lock (node);

	ephy_node_emit_signal (node, EPHY_NODE_DESTROY);

	read_lock_to_write_lock (node);

	lock_gdk ();

	/* remove from DAG */
	g_hash_table_foreach (node->parents,
			      (GHFunc) remove_child,
			      node);

	for (i = 0; i < node->children->len; i++) {
		EphyNode *child;

		child = g_ptr_array_index (node->children, i);

		g_static_rw_lock_writer_lock (child->lock);

		real_remove_child (node, child, FALSE, TRUE);

		g_static_rw_lock_writer_unlock (child->lock);
	}

	g_static_rw_lock_writer_unlock (node->lock);

	g_hash_table_foreach (node->signals,
			      (GHFunc) unref_signal_objects,
			      node);

	_ephy_node_db_remove_id (node->db, node->id);

	unlock_gdk ();
}

EphyNode *
ephy_node_new (EphyNodeDb *db)
{
	long id;

	g_return_val_if_fail (EPHY_IS_NODE_DB (db), NULL);

	id = _ephy_node_db_new_id (db);

	return ephy_node_new_with_id (db, id);
}

EphyNode *
ephy_node_new_with_id (EphyNodeDb *db, gulong reserved_id)
{
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_NODE_DB (db), NULL);

	node = g_new0 (EphyNode, 1);

	node->lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (node->lock);

	node->ref_count = 0;

	node->id = reserved_id;

	node->db = db;

	node->properties = g_ptr_array_new ();

	node->children = g_ptr_array_new ();

	node->parents = g_hash_table_new_full (int_hash,
					       int_equal,
					       NULL,
					       g_free);

	node->signals = g_hash_table_new_full (int_hash,
					       int_equal,
					       NULL,
					       g_free);
	node->signal_id = 0;

	_ephy_node_db_add_id (db, reserved_id, node);

	return node;
}

EphyNodeDb *
ephy_node_get_db (EphyNode *node)
{
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	
	return node->db;
}

long
ephy_node_get_id (EphyNode *node)
{
	long ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	g_static_rw_lock_reader_lock (node->lock);

	ret = node->id;

	g_static_rw_lock_reader_unlock (node->lock);

	return ret;
}

void
ephy_node_ref (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_writer_lock (node->lock);

	node->ref_count++;

	g_static_rw_lock_writer_unlock (node->lock);
}

void
ephy_node_unref (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_writer_lock (node->lock);

	node->ref_count--;

	if (node->ref_count <= 0) {
		ephy_node_dispose (node);
		ephy_node_finalize (node);
	} else {
		g_static_rw_lock_writer_unlock (node->lock);
	}
}

void
ephy_node_freeze (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_reader_lock (node->lock);
}

void
ephy_node_thaw (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_reader_unlock (node->lock);
}

static void
child_changed (gulong id,
	       EphyNodeParent *node_info,
	       EphyNode *node)
{
	g_static_rw_lock_reader_lock (node_info->node->lock);

	ephy_node_emit_signal (node_info->node, EPHY_NODE_CHILD_CHANGED, node);

	g_static_rw_lock_reader_unlock (node_info->node->lock);
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
		g_free (old);
	}

	g_ptr_array_index (node->properties, property_id) = value;
}

void
ephy_node_set_property (EphyNode *node,
		        guint property_id,
		        const GValue *value)
{
	GValue *new;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (property_id >= 0);
	g_return_if_fail (value != NULL);

	lock_gdk ();

	g_static_rw_lock_writer_lock (node->lock);

	new = g_new0 (GValue, 1);
	g_value_init (new, G_VALUE_TYPE (value));
	g_value_copy (value, new);

	real_set_property (node, property_id, new);

	write_lock_to_read_lock (node);

	g_hash_table_foreach (node->parents,
			      (GHFunc) child_changed,
			      node);

	g_static_rw_lock_reader_unlock (node->lock);

	unlock_gdk ();
}

gboolean
ephy_node_get_property (EphyNode *node,
		        guint property_id,
		        GValue *value)
{
	GValue *ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);
	g_return_val_if_fail (property_id >= 0, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return FALSE;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return FALSE;
	}

	g_value_init (value, G_VALUE_TYPE (ret));
	g_value_copy (ret, value);

	g_static_rw_lock_reader_unlock (node->lock);

	return TRUE;
}

const char *
ephy_node_get_property_string (EphyNode *node,
			       guint property_id)
{
	GValue *ret;
	const char *retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (property_id >= 0, NULL);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return NULL;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return NULL;
	}

	retval = g_value_get_string (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

gboolean
ephy_node_get_property_boolean (EphyNode *node,
			        guint property_id)
{
	GValue *ret;
	gboolean retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);
	g_return_val_if_fail (property_id >= 0, FALSE);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return FALSE;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return FALSE;
	}

	retval = g_value_get_boolean (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

long
ephy_node_get_property_long (EphyNode *node,
			     guint property_id)
{
	GValue *ret;
	long retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (property_id >= 0, -1);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	retval = g_value_get_long (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

int
ephy_node_get_property_int (EphyNode *node,
			    guint property_id)
{
	GValue *ret;
	int retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (property_id >= 0, -1);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	retval = g_value_get_int (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

double
ephy_node_get_property_double (EphyNode *node,
			       guint property_id)
{
	GValue *ret;
	double retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (property_id >= 0, -1);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	retval = g_value_get_double (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

float
ephy_node_get_property_float (EphyNode *node,
			      guint property_id)
{
	GValue *ret;
	float retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (property_id >= 0, -1);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return -1;
	}

	retval = g_value_get_float (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

EphyNode *
ephy_node_get_property_node (EphyNode *node,
			     guint property_id)
{
	GValue *ret;
	EphyNode *retval;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (property_id >= 0, NULL);

	g_static_rw_lock_reader_lock (node->lock);

	if (property_id >= node->properties->len) {
		g_static_rw_lock_reader_unlock (node->lock);
		return NULL;
	}

	ret = g_ptr_array_index (node->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->lock);
		return NULL;
	}

	retval = g_value_get_pointer (ret);

	g_static_rw_lock_reader_unlock (node->lock);

	return retval;
}

static void
save_parent (gulong id,
	     EphyNodeParent *node_info,
	     xmlNodePtr xml_node)
{
	xmlNodePtr parent_xml_node;
	char *xml;

	parent_xml_node = xmlNewChild (xml_node, NULL, "parent", NULL);

	g_static_rw_lock_reader_lock (node_info->node->lock);

	xml = g_strdup_printf ("%ld", node_info->node->id);
	xmlSetProp (parent_xml_node, "id", xml);
	g_free (xml);

	g_static_rw_lock_reader_unlock (node_info->node->lock);
}

void
ephy_node_save_to_xml (EphyNode *node,
		       xmlNodePtr parent_xml_node)
{
	xmlNodePtr xml_node;
	char *xml;
	guint i;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (parent_xml_node != NULL);

	g_static_rw_lock_reader_lock (node->lock);

	xml_node = xmlNewChild (parent_xml_node, NULL, "node", NULL);

	xml = g_strdup_printf ("%ld", node->id);
	xmlSetProp (xml_node, "id", xml);
	g_free (xml);

	for (i = 0; i < node->properties->len; i++) {
		GValue *value;
		xmlNodePtr value_xml_node;

		value = g_ptr_array_index (node->properties, i);
		if (value == NULL)
			continue;

		value_xml_node = xmlNewChild (xml_node, NULL, "property", NULL);

		xml = g_strdup_printf ("%d", i);
		xmlSetProp (value_xml_node, "id", xml);
		g_free (xml);

		xmlSetProp (value_xml_node, "value_type", g_type_name (G_VALUE_TYPE (value)));

		switch (G_VALUE_TYPE (value))
		{
		case G_TYPE_STRING:
			xml = xmlEncodeEntitiesReentrant (NULL,
							  g_value_get_string (value));
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			break;
		case G_TYPE_BOOLEAN:
			xml = g_strdup_printf ("%d", g_value_get_boolean (value));
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			break;
		case G_TYPE_INT:
			xml = g_strdup_printf ("%d", g_value_get_int (value));
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			break;
		case G_TYPE_LONG:
			xml = g_strdup_printf ("%ld", g_value_get_long (value));
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			break;
		case G_TYPE_FLOAT:
			xml = g_strdup_printf ("%f", g_value_get_float (value));
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			break;
		case G_TYPE_DOUBLE:
			xml = g_strdup_printf ("%f", g_value_get_double (value));
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			break;
		case G_TYPE_POINTER:
		{
			EphyNode *prop_node;

			prop_node = g_value_get_pointer (value);

			g_assert (prop_node != NULL);

			g_static_rw_lock_reader_lock (prop_node->lock);

			xml = g_strdup_printf ("%ld", prop_node->id);
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);

			g_static_rw_lock_reader_unlock (prop_node->lock);
			break;
		}
		default:
			g_assert_not_reached ();
			break;
		}
	}

	g_hash_table_foreach (node->parents,
			      (GHFunc) save_parent,
			      xml_node);

	g_static_rw_lock_reader_unlock (node->lock);
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

	node_info = g_new0 (EphyNodeParent, 1);
	node_info->node  = node;
	node_info->index = node->children->len - 1;

	g_hash_table_insert (child->parents,
			     GINT_TO_POINTER (node->id),
			     node_info);
}

/* this function assumes it's safe to not lock anything while loading,
 * this is at least true for the case where we're loading the library xml file
 * from the main loop */
EphyNode *
ephy_node_new_from_xml (EphyNodeDb *db, xmlNodePtr xml_node)
{
	EphyNode *node;
	xmlNodePtr xml_child;
	char *xml;
	long id;

	g_return_val_if_fail (EPHY_IS_NODE_DB (db), NULL);
	g_return_val_if_fail (xml_node != NULL, NULL);

	xml = xmlGetProp (xml_node, "id");
	if (xml == NULL)
		return NULL;
	id = atol (xml);
	g_free (xml);

	node = ephy_node_new_with_id (db, id);

	for (xml_child = xml_node->children; xml_child != NULL; xml_child = xml_child->next) {
		if (strcmp (xml_child->name, "parent") == 0) {
			EphyNode *parent;
			long parent_id;

			xml = xmlGetProp (xml_child, "id");
			g_assert (xml != NULL);
			parent_id = atol (xml);
			g_free (xml);

			parent = ephy_node_db_get_node_from_id (db, parent_id);

			if (parent != NULL)
			{
				real_add_child (parent, node);

				ephy_node_emit_signal (parent, EPHY_NODE_CHILD_ADDED, node);
			}
		} else if (strcmp (xml_child->name, "property") == 0) {
			GType value_type;
			GValue *value;
			int property_id;

			xml = xmlGetProp (xml_child, "id");
			property_id = atoi (xml);
			g_free (xml);

			xml = xmlGetProp (xml_child, "value_type");
			value_type = g_type_from_name (xml);
			g_free (xml);

			xml = xmlNodeGetContent (xml_child);
			value = g_new0 (GValue, 1);
			g_value_init (value, value_type);

			switch (value_type)
			{
			case G_TYPE_STRING:
				g_value_set_string (value, xml);
				break;
			case G_TYPE_INT:
				g_value_set_int (value, atoi (xml));
				break;
			case G_TYPE_BOOLEAN:
				g_value_set_boolean (value, atoi (xml));
				break;
			case G_TYPE_LONG:
				g_value_set_long (value, atol (xml));
				break;
			case G_TYPE_FLOAT:
				g_value_set_float (value, atof (xml));
				break;
			case G_TYPE_DOUBLE:
				g_value_set_double (value, atof (xml));
				break;
			case G_TYPE_POINTER:
			{
				EphyNode *property_node;

				property_node = ephy_node_db_get_node_from_id (db, atol (xml));

				g_value_set_pointer (value, property_node);
				break;
			}
			default:
				g_assert_not_reached ();
				break;
			}

			real_set_property (node, property_id, value);

			g_free (xml);
		}
	}

	ephy_node_emit_signal (node, EPHY_NODE_RESTORED);

	return node;
}

void
ephy_node_add_child (EphyNode *node,
		     EphyNode *child)
{
	lock_gdk ();

	g_return_if_fail (EPHY_IS_NODE (node));
	
	g_static_rw_lock_writer_lock (node->lock);
	g_static_rw_lock_writer_lock (child->lock);

	real_add_child (node, child);

	write_lock_to_read_lock (node);
	write_lock_to_read_lock (child);

	ephy_node_emit_signal (node, EPHY_NODE_CHILD_ADDED, child);

	g_static_rw_lock_reader_unlock (node->lock);
	g_static_rw_lock_reader_unlock (child->lock);

	unlock_gdk ();
}

void
ephy_node_remove_child (EphyNode *node,
		        EphyNode *child)
{
	lock_gdk ();

	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_writer_lock (node->lock);
	g_static_rw_lock_writer_lock (child->lock);

	real_remove_child (node, child, TRUE, TRUE);

	g_static_rw_lock_writer_unlock (node->lock);
	g_static_rw_lock_writer_unlock (child->lock);

	unlock_gdk ();
}

gboolean
ephy_node_has_child (EphyNode *node,
		     EphyNode *child)
{
	gboolean ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);
	
	g_static_rw_lock_reader_lock (node->lock);
	g_static_rw_lock_reader_lock (child->lock);

	ret = (g_hash_table_lookup (child->parents,
				    GINT_TO_POINTER (node->id)) != NULL);

	g_static_rw_lock_reader_unlock (node->lock);
	g_static_rw_lock_reader_unlock (child->lock);

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

void
ephy_node_sort_children (EphyNode *node,
			 GCompareFunc compare_func)
{
	GPtrArray *newkids;
	int i, *new_order;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (compare_func != NULL);

	lock_gdk ();

	g_static_rw_lock_writer_lock (node->lock);

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

	write_lock_to_read_lock (node);

	ephy_node_emit_signal (node, EPHY_NODE_CHILDREN_REORDERED, new_order);

	g_free (new_order);

	g_static_rw_lock_reader_unlock (node->lock);

	unlock_gdk ();
}

void
ephy_node_reorder_children (EphyNode *node,
			    int *new_order)
{
	GPtrArray *newkids;
	int i;

	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (new_order != NULL);

	lock_gdk ();

	g_static_rw_lock_writer_lock (node->lock);

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

	write_lock_to_read_lock (node);

	ephy_node_emit_signal (node, EPHY_NODE_CHILDREN_REORDERED, new_order);

	g_static_rw_lock_reader_unlock (node->lock);

	unlock_gdk ();
}

GPtrArray *
ephy_node_get_children (EphyNode *node)
{
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

	g_static_rw_lock_reader_lock (node->lock);

	return node->children;
}

int
ephy_node_get_n_children (EphyNode *node)
{
	int ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	g_static_rw_lock_reader_lock (node->lock);

	ret = node->children->len;

	g_static_rw_lock_reader_unlock (node->lock);

	return ret;
}

EphyNode *
ephy_node_get_nth_child (EphyNode *node,
		         guint n)
{
	EphyNode *ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (n >= 0, NULL);

	g_static_rw_lock_reader_lock (node->lock);

	if (n < node->children->len) {
		ret = g_ptr_array_index (node->children, n);
	} else {
		ret = NULL;
	}

	g_static_rw_lock_reader_unlock (node->lock);

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

	g_static_rw_lock_reader_lock (node->lock);
	g_static_rw_lock_reader_lock (child->lock);

	ret = ephy_node_real_get_child_index (node, child);

	g_static_rw_lock_reader_unlock (node->lock);
	g_static_rw_lock_reader_unlock (child->lock);

	return ret;
}

EphyNode *
ephy_node_get_next_child (EphyNode *node,
			  EphyNode *child)
{
	EphyNode *ret;
	guint idx;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (EPHY_IS_NODE (child), NULL);
	
	g_static_rw_lock_reader_lock (node->lock);
	g_static_rw_lock_reader_lock (child->lock);

	idx = get_child_index_real (node, child);

	if ((idx + 1) < node->children->len) {
		ret = g_ptr_array_index (node->children, idx + 1);
	} else {
		ret = NULL;
	}

	g_static_rw_lock_reader_unlock (node->lock);
	g_static_rw_lock_reader_unlock (child->lock);

	return ret;
}

EphyNode *
ephy_node_get_previous_child (EphyNode *node,
			      EphyNode *child)
{
	EphyNode *ret;
	int idx;

	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (EPHY_IS_NODE (child), NULL);
	
	g_static_rw_lock_reader_lock (node->lock);
	g_static_rw_lock_reader_lock (child->lock);

	idx = get_child_index_real (node, child);

	if ((idx - 1) >= 0) {
		ret = g_ptr_array_index (node->children, idx - 1);
	} else {
		ret = NULL;
	}

	g_static_rw_lock_reader_unlock (node->lock);
	g_static_rw_lock_reader_unlock (child->lock);

	return ret;
}

int
ephy_node_signal_connect_object (EphyNode *node,
				 EphyNodeSignalType type,
				 EphyNodeCallback callback,
				 GObject *object)
{
	EphyNodeSignalData *signal_data;
	int ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	
	signal_data = g_new0 (EphyNodeSignalData, 1);
	signal_data->node = node;
	signal_data->id = node->signal_id;
	signal_data->callback = callback;
	signal_data->type = type;
	signal_data->data = object;

	g_hash_table_insert (node->signals,
			     GINT_TO_POINTER (node->signal_id),
			     signal_data);
	g_object_weak_ref (object, (GWeakNotify)signal_object_weak_notify,
			   signal_data);
	ret = node->signal_id;
	node->signal_id++;

	return ret;
}

void
ephy_node_signal_disconnect (EphyNode *node,
			     int signal_id)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_hash_table_remove (node->signals,
			     GINT_TO_POINTER (signal_id));
}

