/*
 *  Copyright © 2003, 2004 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef EPHY_ENCODINGS_H
#define EPHY_ENCODINGS_H

#include <glib-object.h>
#include <glib.h>

#include "ephy-node.h"

G_BEGIN_DECLS

#define EPHY_TYPE_ENCODINGS		(ephy_encodings_get_type ())
#define EPHY_ENCODINGS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_ENCODINGS, EphyEncodings))
#define EPHY_ENCODINGS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), EPHY_TYPE_ENCODINGS, EphyEncodingsClass))
#define EPHY_IS_ENCODINGS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_ENCODINGS))
#define EPHY_IS_ENCODINGS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_ENCODINGS))
#define EPHY_ENCODINGS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_ENCODINGS, EphyEncodingsClass))

typedef struct _EphyEncodings		EphyEncodings;
typedef struct _EphyEncodingsPrivate	EphyEncodingsPrivate;
typedef struct _EphyEncodingsClass	EphyEncodingsClass;

typedef enum
{
	LG_NONE			= 0,
	LG_ARABIC		= 1 << 0,
	LG_BALTIC		= 1 << 1,
	LG_CAUCASIAN		= 1 << 2,
	LG_C_EUROPEAN		= 1 << 3,
	LG_CHINESE_TRAD		= 1 << 4,
	LG_CHINESE_SIMP		= 1 << 5,
	LG_CYRILLIC		= 1 << 6,
	LG_GREEK		= 1 << 7,
	LG_HEBREW		= 1 << 8,
	LG_INDIAN		= 1 << 9,
	LG_JAPANESE		= 1 << 10,
	LG_KOREAN		= 1 << 12,
	LG_NORDIC		= 1 << 13,
	LG_PERSIAN		= 1 << 14,
	LG_SE_EUROPEAN		= 1 << 15,
	LG_THAI			= 1 << 16,
	LG_TURKISH		= 1 << 17,
	LG_UKRAINIAN		= 1 << 18,
	LG_UNICODE		= 1 << 19,
	LG_VIETNAMESE		= 1 << 20,
	LG_WESTERN		= 1 << 21,
	LG_ALL			= 0x3fffff,
}
EphyLanguageGroup;

enum
{
	EPHY_NODE_ENCODING_PROP_TITLE = 1,
	EPHY_NODE_ENCODING_PROP_TITLE_ELIDED = 2,
	EPHY_NODE_ENCODING_PROP_COLLATION_KEY = 3,
	EPHY_NODE_ENCODING_PROP_ENCODING = 4,
	EPHY_NODE_ENCODING_PROP_LANGUAGE_GROUPS = 5,
	EPHY_NODE_ENCODING_PROP_IS_AUTODETECTOR = 6
};

struct _EphyEncodings
{
	GObject parent;

	/*< private >*/
	EphyEncodingsPrivate *priv;
};

struct _EphyEncodingsClass
{
	GObjectClass parent_class;
};

GType		 ephy_encodings_get_type        (void);

EphyEncodings	*ephy_encodings_new             (void);

EphyNode	*ephy_encodings_get_node	(EphyEncodings *encodings,
						 const char *code,
						 gboolean add_if_not_found);

GList		*ephy_encodings_get_encodings	(EphyEncodings *encodings,
						 EphyLanguageGroup group_mask);

EphyNode	*ephy_encodings_get_all		(EphyEncodings *encodings);

EphyNode	*ephy_encodings_get_detectors	(EphyEncodings *encodings);

void		 ephy_encodings_add_recent	(EphyEncodings *encodings,
						 const char *code);

GList		*ephy_encodings_get_recent	(EphyEncodings *encodings);

G_END_DECLS

#endif
