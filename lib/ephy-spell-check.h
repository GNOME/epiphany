/*
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef EPHY_SPELL_CHECK_H
#define EPHY_SPELL_CHECK_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SPELL_CHECK		(ephy_spell_check_get_type ())
#define EPHY_SPELL_CHECK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SPELL_CHECK, EphySpellCheck))
#define EPHY_SPELL_CHECK_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), EPHY_TYPE_SPELL_CHECK, EphySpellCheckClass))
#define EPHY_IS_SPELL_CHECK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SPELL_CHECK))
#define EPHY_IS_SPELL_CHECK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SPELL_CHECK))
#define EPHY_SPELL_CHECK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SPELL_CHECK, EphySpellCheckClass))

typedef struct _EphySpellCheck		EphySpellCheck;
typedef struct _EphySpellCheckPrivate	EphySpellCheckPrivate;
typedef struct _EphySpellCheckClass	EphySpellCheckClass;

struct _EphySpellCheck
{
	GObject parent_instance;

	/*< private >*/
	EphySpellCheckPrivate *priv;
};

struct _EphySpellCheckClass
{
	GObjectClass parent_class;
};

GType		ephy_spell_check_get_type	(void);

EphySpellCheck *ephy_spell_check_get_default	(void);

gboolean	ephy_spell_check_check_word	(EphySpellCheck *speller,
						 const char *word,
						 gssize len,
						 gboolean *correct);

char	      **ephy_spell_check_get_suggestions	(EphySpellCheck *speller,
							 const char *word,
							 gssize len,
							 gsize *count);

void		ephy_spell_check_free_suggestions	(EphySpellCheck *speller,
							 char **suggestions);

gboolean	ephy_spell_check_set_language	(EphySpellCheck *speller,
						 const char *lang);

char	       *ephy_spell_check_get_language	(EphySpellCheck *speller);

G_END_DECLS

#endif
