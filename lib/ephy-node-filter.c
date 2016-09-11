/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Olivier Martin <omartin@ifrance.com>
 *            (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ephy-node-filter.h"

static void ephy_node_filter_finalize (GObject *object);
static gboolean ephy_node_filter_expression_evaluate (EphyNodeFilterExpression *expression,
                                                      EphyNode                 *node);

enum {
  CHANGED,
  LAST_SIGNAL
};

struct _EphyNodeFilter {
  GObject parent_instance;

  GPtrArray *levels;
};

struct _EphyNodeFilterExpression {
  EphyNodeFilterExpressionType type;

  union {
    struct {
      EphyNode *a;
      EphyNode *b;
    } node_args;

    struct {
      int prop_id;

      union {
        EphyNode *node;
        char *string;
        int number;
      } second_arg;
    } prop_args;
  } args;
};

static guint ephy_node_filter_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EphyNodeFilter, ephy_node_filter, G_TYPE_OBJECT)

static void
ephy_node_filter_class_init (EphyNodeFilterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_node_filter_finalize;

  ephy_node_filter_signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}

static void
ephy_node_filter_init (EphyNodeFilter *filter)
{
  filter->levels = g_ptr_array_new ();
}

static void
ephy_node_filter_finalize (GObject *object)
{
  EphyNodeFilter *filter = EPHY_NODE_FILTER (object);

  ephy_node_filter_empty (filter);

  g_ptr_array_free (filter->levels, TRUE);

  G_OBJECT_CLASS (ephy_node_filter_parent_class)->finalize (object);
}

EphyNodeFilter *
ephy_node_filter_new (void)
{
  return EPHY_NODE_FILTER (g_object_new (EPHY_TYPE_NODE_FILTER, NULL));
}

void
ephy_node_filter_add_expression (EphyNodeFilter           *filter,
                                 EphyNodeFilterExpression *exp,
                                 int                       level)
{
  while (level >= (int)filter->levels->len)
    g_ptr_array_add (filter->levels, NULL);

  /* FIXME bogosity! This only works because g_list_append (x, data) == x */
  g_ptr_array_index (filter->levels, level) =
    g_list_append (g_ptr_array_index (filter->levels, level), exp);
}

void
ephy_node_filter_empty (EphyNodeFilter *filter)
{
  int i;

  for (i = filter->levels->len - 1; i >= 0; i--) {
    GList *list, *l;

    list = g_ptr_array_index (filter->levels, i);

    for (l = list; l != NULL; l = g_list_next (l)) {
      EphyNodeFilterExpression *exp;

      exp = (EphyNodeFilterExpression *)l->data;

      ephy_node_filter_expression_free (exp);
    }

    g_list_free (list);

    g_ptr_array_remove_index (filter->levels, i);
  }
}

void
ephy_node_filter_done_changing (EphyNodeFilter *filter)
{
  g_signal_emit (G_OBJECT (filter), ephy_node_filter_signals[CHANGED], 0);
}

/*
 * We go through each level evaluating the filter expressions.
 * Every time we get a match we immediately do a break and jump
 * to the next level. We'll return FALSE if we arrive to a level
 * without matches, TRUE otherwise.
 */
gboolean
ephy_node_filter_evaluate (EphyNodeFilter *filter,
                           EphyNode       *node)
{
  guint i;

  for (i = 0; i < filter->levels->len; i++) {
    GList *l, *list;
    gboolean handled;

    handled = FALSE;

    list = g_ptr_array_index (filter->levels, i);

    for (l = list; l != NULL; l = g_list_next (l)) {
      if (ephy_node_filter_expression_evaluate (l->data, node) == TRUE) {
        handled = TRUE;
        break;
      }
    }

    if (list != NULL && handled == FALSE)
      return FALSE;
  }

  return TRUE;
}

EphyNodeFilterExpression *
ephy_node_filter_expression_new (EphyNodeFilterExpressionType type,
                                 ...)
{
  EphyNodeFilterExpression *exp;
  va_list valist;

  va_start (valist, type);

  exp = g_new0 (EphyNodeFilterExpression, 1);

  exp->type = type;

  switch (type) {
    case EPHY_NODE_FILTER_EXPRESSION_NODE_EQUALS:
      exp->args.node_args.a = va_arg (valist, EphyNode *);
      exp->args.node_args.b = va_arg (valist, EphyNode *);
      break;
    case EPHY_NODE_FILTER_EXPRESSION_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT:
    case EPHY_NODE_FILTER_EXPRESSION_HAS_CHILD:
      exp->args.node_args.a = va_arg (valist, EphyNode *);
      break;
    case EPHY_NODE_FILTER_EXPRESSION_NODE_PROP_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_CHILD_PROP_EQUALS:
      exp->args.prop_args.prop_id = va_arg (valist, int);
      exp->args.prop_args.second_arg.node = va_arg (valist, EphyNode *);
      break;
    case EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS:
    case EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_EQUALS:
      exp->args.prop_args.prop_id = va_arg (valist, int);
      exp->args.prop_args.second_arg.string = g_utf8_casefold (va_arg (valist, char *), -1);
      break;
    case EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_CONTAINS:
    case EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_EQUALS:
    {
      char *folded;

      exp->args.prop_args.prop_id = va_arg (valist, int);

      folded = g_utf8_casefold (va_arg (valist, char *), -1);
      exp->args.prop_args.second_arg.string = g_utf8_collate_key (folded, -1);
      g_free (folded);
      break;
    }
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_BIGGER_THAN:
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_LESS_THAN:
      exp->args.prop_args.prop_id = va_arg (valist, int);
      exp->args.prop_args.second_arg.number = va_arg (valist, int);
      break;
    case EPHY_NODE_FILTER_EXPRESSION_ALWAYS_TRUE:
    default:
      break;
  }

  va_end (valist);

  return exp;
}

void
ephy_node_filter_expression_free (EphyNodeFilterExpression *exp)
{
  switch (exp->type) {
    case EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS:
    case EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_CONTAINS:
    case EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_EQUALS:
      g_free (exp->args.prop_args.second_arg.string);
      break;
    case EPHY_NODE_FILTER_EXPRESSION_ALWAYS_TRUE:
    case EPHY_NODE_FILTER_EXPRESSION_NODE_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT:
    case EPHY_NODE_FILTER_EXPRESSION_HAS_CHILD:
    case EPHY_NODE_FILTER_EXPRESSION_NODE_PROP_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_CHILD_PROP_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_EQUALS:
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_BIGGER_THAN:
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_LESS_THAN:
    default:
      break;
  }

  g_free (exp);
}

static gboolean
ephy_node_filter_expression_evaluate (EphyNodeFilterExpression *exp,
                                      EphyNode                 *node)
{
  switch (exp->type) {
    case EPHY_NODE_FILTER_EXPRESSION_ALWAYS_TRUE:
      return TRUE;
    case EPHY_NODE_FILTER_EXPRESSION_NODE_EQUALS:
      return (exp->args.node_args.a == exp->args.node_args.b);
    case EPHY_NODE_FILTER_EXPRESSION_EQUALS:
      return (exp->args.node_args.a == node);
    case EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT:
      return ephy_node_has_child (exp->args.node_args.a, node);
    case EPHY_NODE_FILTER_EXPRESSION_HAS_CHILD:
      return ephy_node_has_child (node, exp->args.node_args.a);
    case EPHY_NODE_FILTER_EXPRESSION_NODE_PROP_EQUALS:
    {
      EphyNode *prop;

      prop = ephy_node_get_property_node (node,
                                          exp->args.prop_args.prop_id);

      return (prop == exp->args.prop_args.second_arg.node);
    }
    case EPHY_NODE_FILTER_EXPRESSION_CHILD_PROP_EQUALS:
    {
      EphyNode *prop;
      GPtrArray *children;
      guint i;

      children = ephy_node_get_children (node);
      for (i = 0; i < children->len; i++) {
        EphyNode *child;

        child = g_ptr_array_index (children, i);
        prop = ephy_node_get_property_node
                 (child, exp->args.prop_args.prop_id);

        if (prop == exp->args.prop_args.second_arg.node) {
          return TRUE;
        }
      }

      return FALSE;
    }
    case EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS:
    {
      const char *prop;
      char *folded_case;
      gboolean ret;

      prop = ephy_node_get_property_string (node,
                                            exp->args.prop_args.prop_id);
      if (prop == NULL)
        return FALSE;

      folded_case = g_utf8_casefold (prop, -1);
      ret = (strstr (folded_case, exp->args.prop_args.second_arg.string) != NULL);
      g_free (folded_case);

      return ret;
    }
    case EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_EQUALS:
    {
      const char *prop;
      char *folded_case;
      gboolean ret;

      prop = ephy_node_get_property_string (node,
                                            exp->args.prop_args.prop_id);

      if (prop == NULL)
        return FALSE;

      folded_case = g_utf8_casefold (prop, -1);
      ret = (strcmp (folded_case, exp->args.prop_args.second_arg.string) == 0);
      g_free (folded_case);

      return ret;
    }
    case EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_CONTAINS:
    {
      const char *prop;

      prop = ephy_node_get_property_string (node,
                                            exp->args.prop_args.prop_id);

      if (prop == NULL)
        return FALSE;

      return (strstr (prop, exp->args.prop_args.second_arg.string) != NULL);
    }
    case EPHY_NODE_FILTER_EXPRESSION_KEY_PROP_EQUALS:
    {
      const char *prop;

      prop = ephy_node_get_property_string (node,
                                            exp->args.prop_args.prop_id);

      if (prop == NULL)
        return FALSE;

      return (strcmp (prop, exp->args.prop_args.second_arg.string) == 0);
    }
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_EQUALS:
    {
      int prop;

      prop = ephy_node_get_property_int (node,
                                         exp->args.prop_args.prop_id);

      return (prop == exp->args.prop_args.second_arg.number);
    }
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_BIGGER_THAN:
    {
      int prop;

      prop = ephy_node_get_property_int (node,
                                         exp->args.prop_args.prop_id);

      return (prop > exp->args.prop_args.second_arg.number);
    }
    case EPHY_NODE_FILTER_EXPRESSION_INT_PROP_LESS_THAN:
    {
      int prop;

      prop = ephy_node_get_property_int (node,
                                         exp->args.prop_args.prop_id);

      return (prop < exp->args.prop_args.second_arg.number);
    }
    default:
      break;
  }

  return FALSE;
}
