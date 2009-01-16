/*
 *  Copyright © 2004 Peter Harvey <pah06@uow.edu.au>
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

#include "ephy-nodes-cover.h"

/* Count the number of node entries which are children of parent. */
gint
ephy_nodes_count_covered (EphyNode *parent, const GPtrArray *children)
{
	guint i, len = 0;
	EphyNode *child;
	
	for(i = 0; i < children->len; i++)
	{
		child = g_ptr_array_index (children, i);
		if (ephy_node_has_child (parent, child))
		{
			len++;
		}
	}
	return len;
}

/* Removes from the array of children those which are children of the given parent. */
gint
ephy_nodes_remove_covered (EphyNode *parent, GPtrArray *children)
{
	guint i, len = children->len;
	EphyNode *child;
	
	for(i = 0; i < children->len; i++)
	{
		child = g_ptr_array_index (children, i);
		if (ephy_node_has_child (parent, child))
		{
			g_ptr_array_remove_index_fast (children, i);
			i--;
		}
	}
	return len - children->len;
}

/* Removes from the array of children those which are children of the given parent. */
gint
ephy_nodes_remove_not_covered (EphyNode *parent, GPtrArray *children)
{
	guint i, len = children->len;
	EphyNode *child;
	
	for(i = 0; i < children->len; i++)
	{
		child = g_ptr_array_index (children, i);
		if (!ephy_node_has_child (parent, child))
		{
			g_ptr_array_remove_index_fast (children, i);
			i--;
		}
	}
	return len - children->len;
}

/* Returns the subset of children which are childs of the given parent.
 * Stores the result in the given _covered array if non-null. */
GPtrArray *
ephy_nodes_get_covered (EphyNode *parent, const GPtrArray *children, GPtrArray *_covered)
{
	GPtrArray *covered = _covered?_covered:g_ptr_array_sized_new (children->len);
	EphyNode *child;
	guint i;

	covered->len = 0;
	for (i = 0; i < children->len; i++)
	{
		child = g_ptr_array_index (children, i);
		if (ephy_node_has_child (parent, child))
		{
			g_ptr_array_add (covered, child);
		}
	}
	
	return covered;
}

/* Returns true if the parent covers all the children. */
gboolean
ephy_nodes_covered (EphyNode *parent, const GPtrArray *children)
{
	EphyNode *child;
	guint i;

	for (i = 0; i < children->len; i++)
	{
		child = g_ptr_array_index (children, i);
		if (!ephy_node_has_child (parent, child))
		{
			return FALSE;
		}
	}
	
	return TRUE;
}

static gint
compare_chosen (const guint *a, const guint *b, guint *count_c)
{
	return (count_c[*b] - count_c[*a]);
}

/* Returns the subset of parents which provide a covering of children.
 * Arguments other than parents and children arguments are only used if non-null.
 * Uses the _covering array to store the subset of parents.
 * Uses the _uncovered array to store those children which couldn't be covered.
 * Uses the _sizes array to store the number of children covered by each parent. */
GPtrArray *
ephy_nodes_get_covering (const GPtrArray *parents, const GPtrArray *children,
			 GPtrArray *_covering, GPtrArray *_uncovered, GArray *_sizes)
{
	GPtrArray *uncovered = _uncovered?_uncovered:g_ptr_array_sized_new (children->len);
	GPtrArray *covering = _covering?_covering:g_ptr_array_sized_new (parents->len);
	GArray *chosen = g_array_sized_new (FALSE, FALSE, sizeof(guint), parents->len);
	GArray *sizes = _sizes;

	/* Create arrays to store the number of children each parent has which
	 * are currently not covered, and the number of children it has total. */
	guint *count_u = g_malloc (sizeof(guint) * parents->len);
	guint *count_c = g_malloc (sizeof(guint) * parents->len);
	
	EphyNode *parent;
	guint i, p;

	/* Empty all the returning arrays. */
	uncovered->len = 0;
	covering->len = 0;
	if (sizes) sizes->len = 0;
	
	/* Initialise the array of uncovered bookmarks. */
	for (i = 0; i < children->len; i++)
	{
		g_ptr_array_add (uncovered, g_ptr_array_index (children, i));
	}
	
	/* Initialise the count_u and count_c arrays.
	 * NB: count_u[0] is set to 0 if the parent node
	   covers the entire set of children. */
	for (i = 0, p = 0; i < parents->len; i++)
	{
		parent = g_ptr_array_index (parents, i);
		count_c[i] = ephy_nodes_count_covered (parent, children);
		count_u[i] = (count_c[i]<children->len) ? count_c[i] : 0;
		if (count_u[i] > count_u[p]) p = i;
	}
	
	/* While there are more suitable topics... */
	while (p < parents->len && count_u[p])
	{
		/* Update the arrays of uncovered bookmarks and covering topics. */
		parent = g_ptr_array_index (parents, p);
		ephy_nodes_remove_covered (parent, uncovered);
		g_array_append_val (chosen, p);
		
		/* Find the next most suitable topic. */
		count_u[p] = 0;
		for (i = 0; i < parents->len; i++)
		{
			/* Lazy update the count_u[i] array. */
			if (count_u[i] > count_u[p] || (count_u[i] == count_u[p] && count_c[i] < count_c[p]))
			{
				parent = g_ptr_array_index (parents, i);
				count_u[i] = ephy_nodes_count_covered (parent, uncovered);
			}

			if (count_u[i] > count_u[p] || (count_u[i] == count_u[p] && count_c[i] < count_c[p]))
			{
				p = i;
			}
		}
	}

	g_array_sort_with_data (chosen, (GCompareDataFunc)compare_chosen, count_c);
	
	for (i = 0; i < chosen->len; i++)
	{
		p = g_array_index (chosen, guint, i);
		g_ptr_array_add (covering, g_ptr_array_index (parents, p));
		if (sizes) g_array_append_val (sizes, count_c[p]);
	}

	if (_uncovered != uncovered) g_ptr_array_free (uncovered, TRUE);
	g_array_free (chosen, TRUE);
	g_free (count_u);
	g_free (count_c);
	
	return covering;
}
