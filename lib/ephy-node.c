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
#include <libgnome/gnome-i18n.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gdk/gdk.h>
#include <time.h>

#include "ephy-node.h"
#include "ephy-string.h"
#include "ephy-thread-helpers.h"

static void ephy_node_class_init (EphyNodeClass *klass);
static void ephy_node_init (EphyNode *node);
static void ephy_node_finalize (GObject *object);
static void ephy_node_dispose (GObject *object);
static void ephy_node_set_object_property (GObject *object,
			                   guint prop_id,
			                   const GValue *value,
			                   GParamSpec *pspec);
static void ephy_node_get_object_property (GObject *object,
			                   guint prop_id,
			                   GValue *value,
			                   GParamSpec *pspec);
static inline void id_factory_set_to (gulong new_factory_pos);
static inline void real_set_property (EphyNode *node,
		                      guint property_id,
		                      GValue *value);
static inline void real_remove_child (EphyNode *node,
		                      EphyNode *child,
			              gboolean remove_from_parent,
			              gboolean remove_from_child);
static inline void real_add_child (EphyNode *node,
		                   EphyNode *child);
static inline void read_lock_to_write_lock (EphyNode *node);
static inline void write_lock_to_read_lock (EphyNode *node);
static inline void lock_gdk (void);
static inline void unlock_gdk (void);
static inline EphyNode *node_from_id_real (gulong id);
static inline int get_child_index_real (EphyNode *node,
		                        EphyNode *child);

typedef struct
{
	EphyNode *node;
	guint index;
} EphyNodeParent;

struct EphyNodePrivate
{
	GStaticRWLock *lock;

	int ref_count;

	gulong id;

	GPtrArray *properties;

	GHashTable *parents;
	GPtrArray *children;
};

enum
{
	PROP_0,
	PROP_ID
};

enum
{
	DESTROYED,
	RESTORED,
	CHILD_ADDED,
	CHILD_CHANGED,
	CHILD_REMOVED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_node_signals[LAST_SIGNAL] = { 0 };

static GMutex *id_factory_lock = NULL;
static long id_factory;

static GStaticRWLock *id_to_node_lock = NULL;
static GPtrArray *id_to_node;

GType
ephy_node_get_type (void)
{
	static GType ephy_node_type = 0;

	if (ephy_node_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (EphyNodeClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_node_class_init,
			NULL,
			NULL,
			sizeof (EphyNode),
			0,
			(GInstanceInitFunc) ephy_node_init
		};

		ephy_node_type = g_type_register_static (G_TYPE_OBJECT,
						       "EphyNode",
						       &our_info, 0);
	}

	return ephy_node_type;
}

static void
ephy_node_class_init (EphyNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_node_finalize;
	object_class->dispose  = ephy_node_dispose;

	object_class->set_property = ephy_node_set_object_property;
	object_class->get_property = ephy_node_get_object_property;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_long ("id",
							    "Node ID",
							    "Node ID",
							    0, G_MAXLONG, 0,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	ephy_node_signals[DESTROYED] =
		g_signal_new ("destroyed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeClass, destroyed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	ephy_node_signals[RESTORED] =
		g_signal_new ("restored",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeClass, restored),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	ephy_node_signals[CHILD_ADDED] =
		g_signal_new ("child_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeClass, child_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_NODE);
	ephy_node_signals[CHILD_CHANGED] =
		g_signal_new ("child_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeClass, child_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_NODE);
	ephy_node_signals[CHILD_REMOVED] =
		g_signal_new ("child_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyNodeClass, child_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      EPHY_TYPE_NODE);
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
ephy_node_init (EphyNode *node)
{
	node->priv = g_new0 (EphyNodePrivate, 1);

	node->priv->lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (node->priv->lock);

	node->priv->ref_count = 0;

	node->priv->id = -1;

	node->priv->properties = g_ptr_array_new ();

	node->priv->parents = g_hash_table_new_full (int_hash,
						     int_equal,
						     NULL,
						     g_free);

	node->priv->children = g_ptr_array_new ();
}

static void
ephy_node_finalize (GObject *object)
{
	EphyNode *node;
	guint i;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_NODE (object));

	node = EPHY_NODE (object);

	g_return_if_fail (node->priv != NULL);

	for (i = 0; i < node->priv->properties->len; i++) {
		GValue *val;

		val = g_ptr_array_index (node->priv->properties, i);

		if (val != NULL) {
			g_value_unset (val);
			g_free (val);
		}
	}
	g_ptr_array_free (node->priv->properties, FALSE);

	g_hash_table_destroy (node->priv->parents);

	g_ptr_array_free (node->priv->children, FALSE);

	g_static_rw_lock_free (node->priv->lock);

	g_free (node->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
remove_child (long id,
	      EphyNodeParent *node_info,
	      EphyNode *node)
{
	g_static_rw_lock_writer_lock (node_info->node->priv->lock);

	real_remove_child (node_info->node, node, TRUE, FALSE);

	g_static_rw_lock_writer_unlock (node_info->node->priv->lock);
}

static void
ephy_node_dispose (GObject *object)
{
	EphyNode *node;
	guint i;

	node = EPHY_NODE (object);

	/* remove from id table */
	g_static_rw_lock_writer_lock (id_to_node_lock);

	g_ptr_array_index (id_to_node, node->priv->id) = NULL;

	g_static_rw_lock_writer_unlock (id_to_node_lock);

	lock_gdk ();

	/* remove from DAG */
	g_hash_table_foreach (node->priv->parents,
			      (GHFunc) remove_child,
			      node);

	for (i = 0; i < node->priv->children->len; i++) {
		EphyNode *child;

		child = g_ptr_array_index (node->priv->children, i);

		g_static_rw_lock_writer_lock (child->priv->lock);

		real_remove_child (node, child, FALSE, TRUE);
		
		g_static_rw_lock_writer_unlock (child->priv->lock);
	}

	g_static_rw_lock_writer_unlock (node->priv->lock);

	g_signal_emit (G_OBJECT (node), ephy_node_signals[DESTROYED], 0);

	unlock_gdk ();

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ephy_node_set_object_property (GObject *object,
		             guint prop_id,
		             const GValue *value,
		             GParamSpec *pspec)
{
	EphyNode *node = EPHY_NODE (object);

	switch (prop_id)
	{
	case PROP_ID:
		node->priv->id = g_value_get_long (value);

		g_static_rw_lock_writer_lock (id_to_node_lock);

		/* resize array if needed */
		if (node->priv->id >= id_to_node->len)
			g_ptr_array_set_size (id_to_node, node->priv->id + 1);

		g_ptr_array_index (id_to_node, node->priv->id) = node;

		g_static_rw_lock_writer_unlock (id_to_node_lock);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_node_get_object_property (GObject *object,
		             guint prop_id,
		             GValue *value,
		             GParamSpec *pspec)
{
	EphyNode *node = EPHY_NODE (object);

	switch (prop_id)
	{
	case PROP_ID:
		g_value_set_long (value, node->priv->id);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

EphyNode *
ephy_node_new (void)
{
	EphyNode *node;

	node = EPHY_NODE (g_object_new (EPHY_TYPE_NODE,
				      "id", ephy_node_new_id (),
				      NULL));

	g_return_val_if_fail (node->priv != NULL, NULL);

	return node;
}

EphyNode *
ephy_node_new_with_id (gulong reserved_id)
{
	EphyNode *node;

	node = EPHY_NODE (g_object_new (EPHY_TYPE_NODE,
				      "id", reserved_id,
				      NULL));

	g_return_val_if_fail (node->priv != NULL, NULL);

	return node;
}

long
ephy_node_get_id (EphyNode *node)
{
	long ret;
	
	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	g_static_rw_lock_reader_lock (node->priv->lock);

	ret = node->priv->id;

	g_static_rw_lock_reader_unlock (node->priv->lock);

	return ret;
}

static inline EphyNode *
node_from_id_real (gulong id)
{
	EphyNode *ret = NULL;
	
	if (id < id_to_node->len)
		ret = g_ptr_array_index (id_to_node, id);;

	return ret;
}

EphyNode *
ephy_node_get_from_id (gulong id)
{
	EphyNode *ret = NULL;

	g_return_val_if_fail (id > 0, NULL);

	g_static_rw_lock_reader_lock (id_to_node_lock);

	ret = node_from_id_real (id);
	
	g_static_rw_lock_reader_unlock (id_to_node_lock);

	return ret;
}

void
ephy_node_ref (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_writer_lock (node->priv->lock);

	node->priv->ref_count++;

	g_static_rw_lock_writer_unlock (node->priv->lock);
}

void
ephy_node_unref (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_writer_lock (node->priv->lock);

	node->priv->ref_count--;

	if (node->priv->ref_count <= 0) {
		g_object_unref (G_OBJECT (node));
	} else {
		g_static_rw_lock_writer_unlock (node->priv->lock);
	}
}

void 
ephy_node_freeze (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));

	g_static_rw_lock_reader_lock (node->priv->lock);
}

void
ephy_node_thaw (EphyNode *node)
{
	g_return_if_fail (EPHY_IS_NODE (node));
	
	g_static_rw_lock_reader_unlock (node->priv->lock);
}

static void
child_changed (gulong id,
	       EphyNodeParent *node_info,
	       EphyNode *node)
{
	g_static_rw_lock_reader_lock (node_info->node->priv->lock);

	g_signal_emit (G_OBJECT (node_info->node), ephy_node_signals[CHILD_CHANGED], 0, node);

	g_static_rw_lock_reader_unlock (node_info->node->priv->lock);
}

static inline void
real_set_property (EphyNode *node,
		   guint property_id,
		   GValue *value)
{
	GValue *old;

	if (property_id >= node->priv->properties->len) {
		g_ptr_array_set_size (node->priv->properties, property_id + 1);
	}

	old = g_ptr_array_index (node->priv->properties, property_id);
	if (old != NULL) {
		g_value_unset (old);
		g_free (old);
	}
	
	g_ptr_array_index (node->priv->properties, property_id) = value;
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

	g_static_rw_lock_writer_lock (node->priv->lock);

	new = g_new0 (GValue, 1);
	g_value_init (new, G_VALUE_TYPE (value));
	g_value_copy (value, new);

	real_set_property (node, property_id, new);

	write_lock_to_read_lock (node);

	g_hash_table_foreach (node->priv->parents,
			      (GHFunc) child_changed,
			      node);

	g_static_rw_lock_reader_unlock (node->priv->lock);
	
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
	
	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return FALSE;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return FALSE;
	}
	
	g_value_init (value, G_VALUE_TYPE (ret));
	g_value_copy (ret, value);

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return NULL;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return NULL;
	}
	
	retval = g_value_get_string (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);
	
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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return FALSE;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return FALSE;
	}
	
	retval = g_value_get_boolean (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}
	
	retval = g_value_get_long (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}
	
	retval = g_value_get_int (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}
	
	retval = g_value_get_double (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return -1;
	}
	
	retval = g_value_get_float (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return NULL;
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return NULL;
	}
	
	retval = g_value_get_pointer (ret);

	g_static_rw_lock_reader_unlock (node->priv->lock);

	return retval;
}

char *
ephy_node_get_property_time (EphyNode *node,
			     guint property_id)
{
	GValue *ret;
	long mtime;
	char *retval;
	
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (property_id >= 0, NULL);

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (property_id >= node->priv->properties->len) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return g_strdup (_("Never"));
	}

	ret = g_ptr_array_index (node->priv->properties, property_id);
	if (ret == NULL) {
		g_static_rw_lock_reader_unlock (node->priv->lock);
		return g_strdup (_("Never"));
	}
	
	mtime = g_value_get_long (ret);

	if (retval >= 0) {
		GDate *now, *file_date;
		guint32 file_date_age;
		const char *format = NULL;

		now = g_date_new ();
		g_date_set_time (now, time (NULL));

		file_date = g_date_new ();
		g_date_set_time (file_date, mtime);

		file_date_age = (g_date_get_julian (now) - g_date_get_julian (file_date));

		g_date_free (file_date);
		g_date_free (now);

		if (file_date_age == 0) {
			format = _("Today at %-H:%M");
		} else if (file_date_age == 1) {
			format = _("Yesterday at %-H:%M");
		} else {
			format = _("%A, %B %-d %Y at %-H:%M");
		}

		retval = ephy_string_time_to_string (file_date, format);
	} else {
		retval = g_strdup (_("Never"));
	}

	g_static_rw_lock_reader_unlock (node->priv->lock);

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

	g_static_rw_lock_reader_lock (node_info->node->priv->lock);

	xml = g_strdup_printf ("%ld", node_info->node->priv->id);
	xmlSetProp (parent_xml_node, "id", xml);
	g_free (xml);
	
	g_static_rw_lock_reader_unlock (node_info->node->priv->lock);
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
	
	g_static_rw_lock_reader_lock (node->priv->lock);

	xml_node = xmlNewChild (parent_xml_node, NULL, "node", NULL);

	xml = g_strdup_printf ("%ld", node->priv->id);
	xmlSetProp (xml_node, "id", xml);
	g_free (xml);

	xmlSetProp (xml_node, "type", G_OBJECT_TYPE_NAME (node));

	for (i = 0; i < node->priv->properties->len; i++) {
		GValue *value;
		xmlNodePtr value_xml_node;

		value = g_ptr_array_index (node->priv->properties, i);
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

			g_static_rw_lock_reader_lock (prop_node->priv->lock);
			
			xml = g_strdup_printf ("%ld", prop_node->priv->id);
			xmlNodeSetContent (value_xml_node, xml);
			g_free (xml);
			
			g_static_rw_lock_reader_unlock (prop_node->priv->lock);
			break;
		}
		default:
			g_assert_not_reached ();
			break;
		}
	}

	g_hash_table_foreach (node->priv->parents,
			      (GHFunc) save_parent,
			      xml_node);
	
	g_static_rw_lock_reader_unlock (node->priv->lock);
}

/* this function assumes it's safe to not lock anything while loading,
 * this is at least true for the case where we're loading the library xml file
 * from the main loop */
EphyNode *
ephy_node_new_from_xml (xmlNodePtr xml_node)
{
	EphyNode *node;
	xmlNodePtr xml_child;
	char *xml;
	long id;
	GType type;
	
	g_return_val_if_fail (xml_node != NULL, NULL);

	xml = xmlGetProp (xml_node, "id");
	if (xml == NULL)
		return NULL;
	id = atol (xml);
	g_free (xml);

	id_factory_set_to (id);

	xml = xmlGetProp (xml_node, "type");
	type = g_type_from_name (xml);
	g_free (xml);

	node = EPHY_NODE (g_object_new (type,
				      "id", id,
				      NULL));

	g_return_val_if_fail (node->priv != NULL, NULL);

	for (xml_child = xml_node->children; xml_child != NULL; xml_child = xml_child->next) {
		if (strcmp (xml_child->name, "parent") == 0) {
			EphyNode *parent;
			long parent_id;

			xml = xmlGetProp (xml_child, "id");
			g_assert (xml != NULL);
			parent_id = atol (xml);
			g_free (xml);

			parent = node_from_id_real (parent_id);

			if (parent != NULL)
			{
				real_add_child (parent, node);
				
				g_signal_emit (G_OBJECT (parent), ephy_node_signals[CHILD_ADDED],
					       0, node);
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

				property_node = node_from_id_real (atol (xml));
				
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

	g_signal_emit (G_OBJECT (node), ephy_node_signals[RESTORED], 0);

	return node;
}

static inline void
real_add_child (EphyNode *node,
		EphyNode *child)
{
	EphyNodeParent *node_info;

	if (g_hash_table_lookup (child->priv->parents,
				 GINT_TO_POINTER (node->priv->id)) != NULL) {
		return;
	}

	g_ptr_array_add (node->priv->children, child);

	node_info = g_new0 (EphyNodeParent, 1);
	node_info->node  = node;
	node_info->index = node->priv->children->len - 1;

	g_hash_table_insert (child->priv->parents,
			     GINT_TO_POINTER (node->priv->id),
			     node_info);
}

void
ephy_node_add_child (EphyNode *node,
		     EphyNode *child)
{
	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (EPHY_IS_NODE (child));

	lock_gdk ();

	g_static_rw_lock_writer_lock (node->priv->lock);
	g_static_rw_lock_writer_lock (child->priv->lock);

	real_add_child (node, child);

	write_lock_to_read_lock (node);
	write_lock_to_read_lock (child);

	g_signal_emit (G_OBJECT (node), ephy_node_signals[CHILD_ADDED], 0, child);

	g_static_rw_lock_reader_unlock (node->priv->lock);
	g_static_rw_lock_reader_unlock (child->priv->lock);
	
	unlock_gdk ();
}

static inline void
real_remove_child (EphyNode *node,
		   EphyNode *child,
		   gboolean remove_from_parent,
		   gboolean remove_from_child)
{
	EphyNodeParent *node_info;
	
	write_lock_to_read_lock (node);
	write_lock_to_read_lock (child);

	g_signal_emit (G_OBJECT (node), ephy_node_signals[CHILD_REMOVED], 0, child);

	read_lock_to_write_lock (node);
	read_lock_to_write_lock (child);

	node_info = g_hash_table_lookup (child->priv->parents,
			                 GINT_TO_POINTER (node->priv->id));

	if (remove_from_parent) {
		guint i;

		g_ptr_array_remove_index (node->priv->children,
					  node_info->index);

		/* correct indices on kids */
		for (i = node_info->index; i < node->priv->children->len; i++) {
			EphyNode *borked_node;
			EphyNodeParent *borked_node_info;
		
			borked_node = g_ptr_array_index (node->priv->children, i);

			g_static_rw_lock_writer_lock (borked_node->priv->lock);

			borked_node_info = g_hash_table_lookup (borked_node->priv->parents,
						                GINT_TO_POINTER (node->priv->id));
			borked_node_info->index--;

			g_static_rw_lock_writer_unlock (borked_node->priv->lock);
		}
	}

	if (remove_from_child) {
		g_hash_table_remove (child->priv->parents,
				     GINT_TO_POINTER (node->priv->id));
	}
}

void
ephy_node_remove_child (EphyNode *node,
		        EphyNode *child)
{
	g_return_if_fail (EPHY_IS_NODE (node));
	g_return_if_fail (EPHY_IS_NODE (child));

	lock_gdk ();

	g_static_rw_lock_writer_lock (node->priv->lock);
	g_static_rw_lock_writer_lock (child->priv->lock);

	real_remove_child (node, child, TRUE, TRUE);

	g_static_rw_lock_writer_unlock (node->priv->lock);
	g_static_rw_lock_writer_unlock (child->priv->lock);

	unlock_gdk ();
}

gboolean
ephy_node_has_child (EphyNode *node,
		     EphyNode *child)
{
	gboolean ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), FALSE);
	g_return_val_if_fail (EPHY_IS_NODE (child), FALSE);

	g_static_rw_lock_reader_lock (node->priv->lock);
	g_static_rw_lock_reader_lock (child->priv->lock);

	ret = (g_hash_table_lookup (child->priv->parents,
				    GINT_TO_POINTER (node->priv->id)) != NULL);
	
	g_static_rw_lock_reader_unlock (node->priv->lock);
	g_static_rw_lock_reader_unlock (child->priv->lock);

	return ret;
}

GPtrArray *
ephy_node_get_children (EphyNode *node)
{
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

	g_static_rw_lock_reader_lock (node->priv->lock);

	return node->priv->children;
}

int
ephy_node_get_n_children (EphyNode *node)
{
	int ret;

	g_return_val_if_fail (EPHY_IS_NODE (node), -1);

	g_static_rw_lock_reader_lock (node->priv->lock);
	
	ret = node->priv->children->len;
	
	g_static_rw_lock_reader_unlock (node->priv->lock);

	return ret;
}

EphyNode *
ephy_node_get_nth_child (EphyNode *node,
		         guint n)
{
	EphyNode *ret;
	
	g_return_val_if_fail (EPHY_IS_NODE (node), NULL);
	g_return_val_if_fail (n >= 0, NULL);

	g_static_rw_lock_reader_lock (node->priv->lock);

	if (n < node->priv->children->len) {
		ret = g_ptr_array_index (node->priv->children, n);
	} else {
		ret = NULL;
	}
	
	g_static_rw_lock_reader_unlock (node->priv->lock);
	
	return ret;
}

static inline int
get_child_index_real (EphyNode *node,
		      EphyNode *child)
{
	EphyNodeParent *node_info;

	node_info = g_hash_table_lookup (child->priv->parents,
					 GINT_TO_POINTER (node->priv->id));

	if (node_info == NULL)
		return -1;

	return node_info->index;
}

int
ephy_node_get_child_index (EphyNode *node,
			   EphyNode *child)
{
	EphyNodeParent *node_info;
	int ret;
	
	g_return_val_if_fail (EPHY_IS_NODE (node), -1);
	g_return_val_if_fail (EPHY_IS_NODE (child), -1);

	g_static_rw_lock_reader_lock (node->priv->lock);
	g_static_rw_lock_reader_lock (child->priv->lock);

	node_info = g_hash_table_lookup (child->priv->parents,
					 GINT_TO_POINTER (node->priv->id));

	if (node_info == NULL)
		return -1;

	ret = node_info->index;
	
	g_static_rw_lock_reader_unlock (node->priv->lock);
	g_static_rw_lock_reader_unlock (child->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);
	g_static_rw_lock_reader_lock (child->priv->lock);

	idx = get_child_index_real (node, child);

	if ((idx + 1) < node->priv->children->len) {
		ret = g_ptr_array_index (node->priv->children, idx + 1);
	} else {
		ret = NULL;
	}

	g_static_rw_lock_reader_unlock (node->priv->lock);
	g_static_rw_lock_reader_unlock (child->priv->lock);

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

	g_static_rw_lock_reader_lock (node->priv->lock);
	g_static_rw_lock_reader_lock (child->priv->lock);

	idx = get_child_index_real (node, child);

	if ((idx - 1) >= 0) {
		ret = g_ptr_array_index (node->priv->children, idx - 1);
	} else {
		ret = NULL;
	}

	g_static_rw_lock_reader_unlock (node->priv->lock);
	g_static_rw_lock_reader_unlock (child->priv->lock);

	return ret;
}

void
ephy_node_system_init (gulong reserved_ids)
{
	/* id to node */
	id_to_node = g_ptr_array_new ();

	id_to_node_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (id_to_node_lock);

	/* id factory */
	id_factory = reserved_ids;
	id_factory_lock = g_mutex_new ();
}

void
ephy_node_system_shutdown (void)
{
	g_ptr_array_free (id_to_node, FALSE);

	g_static_rw_lock_free (id_to_node_lock);

	g_mutex_free (id_factory_lock);
}

long
ephy_node_new_id (void)
{
	long ret;

	g_mutex_lock (id_factory_lock);

	id_factory++;

	ret = id_factory;

	g_mutex_unlock (id_factory_lock);

	return ret;
}

static void
id_factory_set_to (gulong new_factory_pos)
{
	if (new_factory_pos > id_factory)
	{
		id_factory = new_factory_pos + 1;
	}
}

/* evillish hacks to temporarily readlock->writelock and v.v. */
static inline void
write_lock_to_read_lock (EphyNode *node)
{
	g_static_mutex_lock (&node->priv->lock->mutex);
	node->priv->lock->read_counter++;
	g_static_mutex_unlock (&node->priv->lock->mutex);

	g_static_rw_lock_writer_unlock (node->priv->lock);
}

static inline void
read_lock_to_write_lock (EphyNode *node)
{
	g_static_mutex_lock (&node->priv->lock->mutex);
	node->priv->lock->read_counter--;
	g_static_mutex_unlock (&node->priv->lock->mutex);

	g_static_rw_lock_writer_lock (node->priv->lock);
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
