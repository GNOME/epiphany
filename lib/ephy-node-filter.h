/*
 *  Copyright © 2002 Olivier Martin <omartin@ifrance.com>
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_NODE_FILTER_H
#define EPHY_NODE_FILTER_H

#include <glib-object.h>

#include "ephy-node.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NODE_FILTER         (ephy_node_filter_get_type ())
#define EPHY_NODE_FILTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NODE_FILTER, EphyNodeFilter))
#define EPHY_NODE_FILTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NODE_FILTER, EphyNodeFilterClass))
#define EPHY_IS_NODE_FILTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NODE_FILTER))
#define EPHY_IS_NODE_FILTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NODE_FILTER))
#define EPHY_NODE_FILTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NODE_FILTER, EphyNodeFilterClass))

typedef struct _EphyNodeFilterPrivate EphyNodeFilterPrivate;

typedef struct
{
	GObject parent;

	/*< private >*/
	EphyNodeFilterPrivate *priv;
} EphyNodeFilter;

typedef struct
{
	GObjectClass parent;

	void (*changed) (EphyNodeFilter *filter);
} EphyNodeFilterClass;

typedef enum
{
	EPHY_NODE_FILTER_EXPRESSION_ALWAYS_TRUE,           /* args: none */
	EPHY_NODE_FILTER_EXPRESSION_NODE_EQUALS,           /* args: EphyNode *a, EphyNode *b */
	EPHY_NODE_FILTER_EXPRESSION_EQUALS,                /* args: EphyNode *node */
	EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT,            /* args: EphyNode *parent */
	EPHY_NODE_FILTER_EXPRESSION_HAS_CHILD,             /* args: EphyNode *child */
	EPHY_NODE_FILTER_EXPRESSION_NODE_PROP_EQUALS,      /* args: int prop_id, EphyNode *node */
	EPHY_NODE_FILTER_EXPRESSION_CHILD_PROP_EQUALS,     /* args: int prop_id, EphyNode *node */
	EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,  /* args: int prop_id, const char *string */
	EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_EQUALS,    /* args: int prop_id, const char *string */
	EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_CONTAINS,     /* args: int prop_id, const char *string */
	EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_EQUALS,       /* args: int prop_id, const char *string */
	EPHY_NODE_FILTER_EXPRESSION_INT_PROP_EQUALS,       /* args: int prop_id, int int */
	EPHY_NODE_FILTER_EXPRESSION_INT_PROP_BIGGER_THAN,  /* args: int prop_id, int int */
	EPHY_NODE_FILTER_EXPRESSION_INT_PROP_LESS_THAN     /* args: int prop_id, int int */
} EphyNodeFilterExpressionType;

typedef struct _EphyNodeFilterExpression EphyNodeFilterExpression;

/* The filter starts iterating over all expressions at level 0,
 * if one of them is TRUE it continues to level 1, etc.
 * If it still has TRUE when there are no more expressions at the
 * next level, the result is TRUE. Otherwise, it's FALSE.
 */

GType           ephy_node_filter_get_type       (void);

EphyNodeFilter *ephy_node_filter_new            (void);

void            ephy_node_filter_add_expression (EphyNodeFilter *filter,
					         EphyNodeFilterExpression *expression,
					         int level);

void            ephy_node_filter_empty          (EphyNodeFilter *filter);

void            ephy_node_filter_done_changing  (EphyNodeFilter *filter);

gboolean        ephy_node_filter_evaluate       (EphyNodeFilter *filter,
					         EphyNode *node);

EphyNodeFilterExpression *ephy_node_filter_expression_new  (EphyNodeFilterExpressionType,
						            ...);
/* no need to free unless you didn't add the expression to a filter */
void                      ephy_node_filter_expression_free (EphyNodeFilterExpression *expression);

G_END_DECLS

#endif /* EPHY_NODE_FILTER_H */
