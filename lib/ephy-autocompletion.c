/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 *
 */

#include <string.h>
#include <stdlib.h>

#include "ephy-autocompletion.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

/**
 * Private data
 */

typedef enum {
	GAS_NEEDS_REFINE,
	GAS_NEEDS_FULL_UPDATE,
	GAS_UPDATED
} EphyAutocompletionStatus;

typedef struct {
	EphyAutocompletionMatch *array;
	guint num_matches;
	guint num_action_matches;
	guint array_size;
} ACMatchArray;

#define ACMA_BASE_SIZE 10240

struct _EphyAutocompletionPrivate {
	GSList *sources;

	guint nkeys;
	gchar **keys;
	guint *key_lengths;
	gchar **prefixes;
	guint *prefix_lengths;

	ACMatchArray matches;
	EphyAutocompletionStatus status;
	gboolean sorted;
	gboolean changed;

	gboolean sort_alpha;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_autocompletion_class_init			(EphyAutocompletionClass *klass);
static void		ephy_autocompletion_init			(EphyAutocompletion *e);
static void		ephy_autocompletion_finalize_impl		(GObject *o);
static void		ephy_autocompletion_reset			(EphyAutocompletion *ac);
static void		ephy_autocompletion_update_matches		(EphyAutocompletion *ac);
static void		ephy_autocompletion_update_matches_full		(EphyAutocompletion *ac);
static gboolean		ephy_autocompletion_sort_by_score		(EphyAutocompletion *ac);
static void		ephy_autocompletion_data_changed_cb		(EphyAutocompletionSource *s,
									 EphyAutocompletion *ac);

static void		acma_init					(ACMatchArray *a);
static void		acma_destroy					(ACMatchArray *a);
static inline void	acma_append					(ACMatchArray *a,
									 EphyAutocompletionMatch *m,
									 gboolean action);
static void		acma_grow					(ACMatchArray *a);


static gpointer g_object_class;

/**
 * Signals enums and ids
 */
enum EphyAutocompletionSignalsEnum {
	EPHY_AUTOCOMPLETION_SOURCES_CHANGED,
	EPHY_AUTOCOMPLETION_LAST_SIGNAL
};
static gint EphyAutocompletionSignals[EPHY_AUTOCOMPLETION_LAST_SIGNAL];

GType
ephy_autocompletion_get_type (void)
{
	static GType ephy_autocompletion_type = 0;

	if (ephy_autocompletion_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyAutocompletionClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_autocompletion_class_init,
			NULL,
			NULL,
			sizeof (EphyAutocompletion),
			0,
			(GInstanceInitFunc) ephy_autocompletion_init
		};

		ephy_autocompletion_type = g_type_register_static (G_TYPE_OBJECT,
							           "EphyAutocompletion",
							           &our_info, 0);
	}

	return ephy_autocompletion_type;
}

static void
ephy_autocompletion_class_init (EphyAutocompletionClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_autocompletion_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	EphyAutocompletionSignals[EPHY_AUTOCOMPLETION_SOURCES_CHANGED] = g_signal_new (
		"sources-changed", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyAutocompletionClass, sources_changed),
		NULL, NULL,
		ephy_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
ephy_autocompletion_init (EphyAutocompletion *ac)
{
	EphyAutocompletionPrivate *p = g_new0 (EphyAutocompletionPrivate, 1);
	ac->priv = p;
	p->sources = NULL;
	acma_init (&p->matches);
	p->status = GAS_NEEDS_FULL_UPDATE;

	p->nkeys = 1;

	p->keys = g_new0 (gchar *, 2);
	p->keys[0] = g_strdup ("");
	p->key_lengths = g_new0 (guint, 2);
	p->key_lengths[0] = 0;

	p->prefixes = g_new0 (gchar *, 2);
	p->prefixes[0] = g_strdup ("");
	p->prefix_lengths = g_new0 (guint, 2);
	p->prefix_lengths[0] = 0;

	p->sort_alpha = TRUE;
}

static void
ephy_autocompletion_finalize_impl (GObject *o)
{
	EphyAutocompletion *ac = EPHY_AUTOCOMPLETION (o);
	EphyAutocompletionPrivate *p = ac->priv;
	GSList *li;

	ephy_autocompletion_reset (ac);

	for (li = p->sources; li; li = li->next)
	{
		g_signal_handlers_disconnect_by_func (li->data,
						      ephy_autocompletion_data_changed_cb, ac);
		g_object_unref (li->data);
	}

	g_slist_free (p->sources);

	g_strfreev (p->keys);
	g_free (p->key_lengths);
	g_strfreev (p->prefixes);
	g_free (p->prefix_lengths);

	g_free (p);

	G_OBJECT_CLASS (g_object_class)->finalize (o);

}

static void
ephy_autocompletion_reset (EphyAutocompletion *ac)
{
	EphyAutocompletionPrivate *p = ac->priv;

	START_PROFILER ("Resetting autocompletion")

	p->status = GAS_NEEDS_FULL_UPDATE;

	STOP_PROFILER ("Resetting autocompletion")
}

EphyAutocompletion *
ephy_autocompletion_new (void)
{
	EphyAutocompletion *ret = g_object_new (EPHY_TYPE_AUTOCOMPLETION, NULL);
	return ret;
}
void
ephy_autocompletion_add_source (EphyAutocompletion *ac,
				EphyAutocompletionSource *s)
{
	EphyAutocompletionPrivate *p = ac->priv;
	g_object_ref (G_OBJECT (s));
	p->sources = g_slist_prepend (p->sources, s);
	ephy_autocompletion_reset (ac);
	g_signal_connect (s, "data-changed", G_CALLBACK (ephy_autocompletion_data_changed_cb), ac);

	g_signal_emit (ac, EphyAutocompletionSignals[EPHY_AUTOCOMPLETION_SOURCES_CHANGED], 0);
}

void
ephy_autocompletion_set_key (EphyAutocompletion *ac,
			     const gchar *key)
{
	EphyAutocompletionPrivate *p = ac->priv;
	guint i;
	guint keylen = strlen (key);

	if (strcmp (key, p->keys[0]))
	{
		GSList *li;
		for (li = p->sources; li; li = li->next)
		{
			ephy_autocompletion_source_set_basic_key
				(EPHY_AUTOCOMPLETION_SOURCE (li->data), key);
		}
	}

	LOG ("Set key %s, old key %s", key, p->keys[0])

	if (keylen >= p->key_lengths[0]
	    && !strncmp (p->keys[0], key, p->key_lengths[0]))
	{
		if (!strcmp (key, p->keys[0]))
		{
			return;
		}
		if (p->status != GAS_NEEDS_FULL_UPDATE)
		{
			p->status = GAS_NEEDS_REFINE;
		}
	}
	else
	{
		p->status = GAS_NEEDS_FULL_UPDATE;
	}

	for (i = 0; p->prefixes[i]; ++i)
	{
		g_free (p->keys[i]);
		p->keys[i] = g_strconcat (p->prefixes[i], key, NULL);
		p->key_lengths[i] = keylen + p->prefix_lengths[i];
	}

}

const EphyAutocompletionMatch *
ephy_autocompletion_get_matches (EphyAutocompletion *ac)
{
	ephy_autocompletion_update_matches (ac);
	return ac->priv->matches.array;
}

const EphyAutocompletionMatch *
ephy_autocompletion_get_matches_sorted_by_score (EphyAutocompletion *ac,
						 gboolean *changed)
{
	*changed = ephy_autocompletion_sort_by_score (ac);
	return ac->priv->matches.array;
}

guint
ephy_autocompletion_get_num_matches (EphyAutocompletion *ac)
{
	ephy_autocompletion_update_matches (ac);

	return ac->priv->matches.num_matches;
}

guint
ephy_autocompletion_get_num_action_matches (EphyAutocompletion *ac)
{
	return ac->priv->matches.num_matches -
		ac->priv->matches.num_action_matches;
}

static void
ephy_autocompletion_refine_matches (EphyAutocompletion *ac)
{
	EphyAutocompletionPrivate *p = ac->priv;
	ACMatchArray oldmatches = p->matches;
	ACMatchArray newmatches;
	guint i;
	gchar *key0 = p->keys[0];
	guint key0l = p->key_lengths[0];

	START_PROFILER ("Refine matches")

	acma_init (&newmatches);

	p->changed = FALSE;

	for (i = 0; i < oldmatches.num_matches; i++)
	{
		EphyAutocompletionMatch *mi = &oldmatches.array[i];

		if (mi->is_action ||
		    (mi->match && mi->substring && g_strrstr (mi->match, p->keys[0])) ||
		    (mi->title && mi->substring && g_strrstr (mi->title, p->keys[0])) ||
		    !strncmp (key0, mi->title + mi->offset, key0l))
		{
			acma_append (&newmatches, mi, mi->is_action);
		}
		else
		{
			p->changed = TRUE;
		}
	}

	acma_destroy (&oldmatches);
	p->matches = newmatches;

	STOP_PROFILER ("Refine matches")
}

static void
ephy_autocompletion_update_matches (EphyAutocompletion *ac)
{
	EphyAutocompletionPrivate *p = ac->priv;
	if (p->status == GAS_UPDATED)
	{
		return;
	}
	if (p->status == GAS_NEEDS_FULL_UPDATE)
	{
		ephy_autocompletion_update_matches_full (ac);
	}
	if (p->status == GAS_NEEDS_REFINE)
	{
		ephy_autocompletion_refine_matches (ac);
	}

	p->status = GAS_UPDATED;
}

static void
ephy_autocompletion_update_matches_full_item (EphyAutocompletionSource *source,
					      const char *item,
					      const char *title,
					      const char *target,
					      gboolean is_action,
					      gboolean substring,
					      guint32 score,
					      EphyAutocompletionPrivate *p)
{
	if (substring)
	{
		if ((item && g_strrstr (item, p->keys[0])) ||
		    (title && g_strrstr (title, p->keys[0])))
		{
			EphyAutocompletionMatch m;
			m.match = item;
			m.title = title;
			m.target = target;
			m.is_action = is_action;
			m.substring = substring;
			m.offset = 0;
			m.score = score;
			acma_append (&p->matches, &m, is_action);
		}
	}
	else if (is_action)
	{
		EphyAutocompletionMatch m;
		m.match = item;
		m.title = title;
		m.target = target;
		m.is_action = is_action;
		m.substring = substring;
		m.offset = 0;
		m.score = score;
		acma_append (&p->matches, &m, is_action);
	}
	else
	{
		guint i;
		for (i = 0; p->keys[i]; ++i)
		{
			if (!strncmp (item, p->keys[i], p->key_lengths[i]))
			{
				EphyAutocompletionMatch m;
				m.match = item;
				m.title = title;
				m.target = target;
				m.is_action = is_action;
				m.substring = substring;
				m.offset = p->prefix_lengths[i];
				m.score = score;
				acma_append (&p->matches, &m, is_action);
			}
		}
	}
}

static void
ephy_autocompletion_update_matches_full (EphyAutocompletion *ac)
{
	EphyAutocompletionPrivate *p = ac->priv;
	GSList *li;

	START_PROFILER ("Update full")

	acma_destroy (&p->matches);

	for (li = p->sources; li; li = li->next)
	{
		EphyAutocompletionSource *s = EPHY_AUTOCOMPLETION_SOURCE (li->data);
		g_assert (s);
		ephy_autocompletion_source_foreach (s, p->keys[0],
						    (EphyAutocompletionSourceForeachFunc)
						    ephy_autocompletion_update_matches_full_item,
						    p);
	}

	STOP_PROFILER ("Update full")

	p->sorted = FALSE;
	p->changed = TRUE;

	LOG ("AC: %d matches, fully updated", p->matches.num_matches);
}

static gint
ephy_autocompletion_compare_scores (EphyAutocompletionMatch *a, EphyAutocompletionMatch *b)
{
	/* higher scores first */
	return b->score - a->score;
}

static gint
ephy_autocompletion_compare_scores_and_alpha (EphyAutocompletionMatch *a, EphyAutocompletionMatch *b)
{
	if (a->score == b->score)
	{
		return strcmp (a->title, b->title);
	}
	else
	{
		/* higher scores first */
		return b->score - a->score;
	}
}

static gboolean
ephy_autocompletion_sort_by_score (EphyAutocompletion *ac)
{
	EphyAutocompletionPrivate *p = ac->priv;

	gint (*comparer) (EphyAutocompletionMatch *m1, EphyAutocompletionMatch *m2);

	if (p->sort_alpha)
	{
		comparer = ephy_autocompletion_compare_scores_and_alpha;
	}
	else
	{
		comparer = ephy_autocompletion_compare_scores;
	}

	ephy_autocompletion_update_matches (ac);
	if (p->changed == FALSE) return FALSE;

	START_PROFILER ("Sorting")

	if (p->matches.num_matches > 0)
	{
		qsort (p->matches.array, p->matches.num_matches,
		       sizeof (EphyAutocompletionMatch),
		       (void *) comparer);
	}

	p->sorted = TRUE;

	STOP_PROFILER ("Sorting")

	return TRUE;
}

static void
ephy_autocompletion_data_changed_cb (EphyAutocompletionSource *s,
				     EphyAutocompletion *ac)
{
	LOG ("Data changed, resetting");
	ephy_autocompletion_reset (ac);

	g_signal_emit (ac, EphyAutocompletionSignals[EPHY_AUTOCOMPLETION_SOURCES_CHANGED], 0);
}

void
ephy_autocompletion_set_prefixes (EphyAutocompletion *ac,
				  const gchar **prefixes)
{
	EphyAutocompletionPrivate *p = ac->priv;
	guint nprefixes = 0;
	gchar *oldkey = g_strdup (p->keys[0]);
	guint i;

	/* count prefixes */
	while (prefixes[nprefixes])
	{
		++nprefixes;
	}

	nprefixes--;

	g_strfreev (p->keys);
	g_free (p->key_lengths);
	g_strfreev (p->prefixes);
	g_free (p->prefix_lengths);

	p->prefixes = g_new0 (gchar *, nprefixes + 2);
	p->prefix_lengths = g_new0 (guint, nprefixes + 2);
	p->keys = g_new0 (gchar *, nprefixes + 2);
	p->key_lengths = g_new0 (guint, nprefixes + 2);

	p->prefixes[0] = g_strdup ("");
	p->prefix_lengths[0] = 0;
	p->keys[0] = oldkey;
	p->key_lengths[0] = strlen (p->keys[0]);

	for (i = 0; i < nprefixes; ++i)
	{
		p->prefixes[i + 1] = g_strdup (prefixes[i]);
		p->prefix_lengths[i + 1] = strlen (prefixes[i]);
		p->keys[i + 1] = g_strconcat (p->prefixes[i + 1], p->keys[0], NULL);
		p->key_lengths[i + 1] = p->prefix_lengths[i + 1] + p->key_lengths[0];
	}

	p->nkeys = nprefixes;
}


/* ACMatchArray */

static void
acma_init (ACMatchArray *a)
{
	a->array = NULL;
	a->array_size = 0;
	a->num_matches = 0;
}

/**
 * Does not free the struct itself, only its contents
 */
static void
acma_destroy (ACMatchArray *a)
{
	g_free (a->array);
	a->array = NULL;
	a->array_size = 0;
	a->num_matches = 0;
	a->num_action_matches = 0;
}

static inline void
acma_append (ACMatchArray *a,
	     EphyAutocompletionMatch *m,
	     gboolean action)
{
	if (a->array_size == a->num_matches)
	{
		acma_grow (a);
	}

	a->array[a->num_matches] = *m;
	a->num_matches++;
	if (action) a->num_action_matches++;
}

static void
acma_grow (ACMatchArray *a)
{
	gint new_size;
	EphyAutocompletionMatch *new_array;

	new_size = a->array_size + ACMA_BASE_SIZE;
	new_array = g_renew (EphyAutocompletionMatch, a->array, new_size);

	a->array_size = new_size;
	a->array = new_array;
}
