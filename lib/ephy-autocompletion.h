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
 */

#ifndef EPHY_AUTOCOMPLETION_H
#define EPHY_AUTOCOMPLETION_H

#include <glib-object.h>
#include "ephy-autocompletion-source.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyAutocompletion EphyAutocompletion;
typedef struct _EphyAutocompletionClass EphyAutocompletionClass;
typedef struct _EphyAutocompletionPrivate EphyAutocompletionPrivate;
typedef struct _EphyAutocompletionMatch EphyAutocompletionMatch;

/**
 * EphyAutocompletion object
 */

#define EPHY_TYPE_AUTOCOMPLETION		(ephy_autocompletion_get_type())
#define EPHY_AUTOCOMPLETION(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), \
						 EPHY_TYPE_AUTOCOMPLETION,\
						 EphyAutocompletion))
#define EPHY_AUTOCOMPLETION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
						 EPHY_TYPE_AUTOCOMPLETION,\
						 EphyAutocompletionClass))
#define EPHY_IS_AUTOCOMPLETION(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), \
						 EPHY_TYPE_AUTOCOMPLETION))
#define EPHY_IS_AUTOCOMPLETION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
						 EPHY_TYPE_AUTOCOMPLETION))
#define EPHY_AUTOCOMPLETION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
						 EPHY_TYPE_AUTOCOMPLETION,\
						 EphyAutocompletionClass))

struct _EphyAutocompletionClass
{
	GObjectClass parent_class;

	/* signals */
	void	(*sources_changed)	(EphyAutocompletion *ac);
};

/* Remember: fields are public read-only */
struct _EphyAutocompletion
{
	GObject parent_object;

	EphyAutocompletionPrivate *priv;
};

struct _EphyAutocompletionMatch
{
	const char *match;
	const char *title;
	const char *target;
	guint offset;
	gint32 score;
	gboolean is_action;
	gboolean substring;
};

/* this is a set of usual prefixes for web browsing */
#define EPHY_AUTOCOMPLETION_USUAL_WEB_PREFIXES \
	"http://www.", \
	"http://", \
	"https://www.", \
	"https://", \
	"file://", \
	"www."

GType				ephy_autocompletion_get_type		(void);
EphyAutocompletion *		ephy_autocompletion_new			(void);
void				ephy_autocompletion_add_source		(EphyAutocompletion *ac,
									 EphyAutocompletionSource *s);
void				ephy_autocompletion_set_prefixes	(EphyAutocompletion *ac,
									 const gchar **prefixes);
void				ephy_autocompletion_set_key		(EphyAutocompletion *ac,
									 const gchar *key);
const EphyAutocompletionMatch  *ephy_autocompletion_get_matches		(EphyAutocompletion *ac);
const EphyAutocompletionMatch  *ephy_autocompletion_get_matches_sorted_by_score
									(EphyAutocompletion *ac,
									 gboolean *changed);
guint				ephy_autocompletion_get_num_matches	(EphyAutocompletion *ac);
guint				ephy_autocompletion_get_num_action_matches (EphyAutocompletion *ac);

G_END_DECLS

#endif
