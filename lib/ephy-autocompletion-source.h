/* 
 *  Copyright (C) 2002  Ricardo Fernándezs Pascual <ric@users.sourceforge.net>
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

#ifndef EPHY_AUTOCOMPLETION_SOUCE_H
#define EPHY_AUTOCOMPLETION_SOUCE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_AUTOCOMPLETION_SOURCE			(ephy_autocompletion_source_get_type ())
#define EPHY_AUTOCOMPLETION_SOURCE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), \
							 EPHY_TYPE_AUTOCOMPLETION_SOURCE, \
							 EphyAutocompletionSource))
#define EPHY_IS_AUTOCOMPLETION_SOURCE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
							 EPHY_TYPE_AUTOCOMPLETION_SOURCE))
#define EPHY_AUTOCOMPLETION_SOURCE_GET_IFACE(obj)	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
							 EPHY_TYPE_AUTOCOMPLETION_SOURCE, \
							 EphyAutocompletionSourceIface))


typedef struct _EphyAutocompletionSource	EphyAutocompletionSource;
typedef struct _EphyAutocompletionSourceIface	EphyAutocompletionSourceIface;
typedef void (* EphyAutocompletionSourceForeachFunc) (EphyAutocompletionSource *source,
						      const char *item,
						      const char *title,
						      const char *target,
						      gboolean is_action,
						      gboolean substring,
						      guint32 score,
						      gpointer data);

struct _EphyAutocompletionSourceIface
{
	GTypeInterface g_iface;

	/* Signals */

	/**
	 * Sources MUST emit this signal when theirs data changes, expecially if the
	 * strings are freed / modified. Otherwise, things will crash.
	 */
	void		(* data_changed)	(EphyAutocompletionSource *source);

	/* Virtual Table */
	void		(* foreach)		(EphyAutocompletionSource *source,
						 const gchar *basic_key,
						 EphyAutocompletionSourceForeachFunc func,
						 gpointer data);
	void		(* set_basic_key)	(EphyAutocompletionSource *source,
						 const gchar *basic_key);
};

GType	ephy_autocompletion_source_get_type		(void);
void	ephy_autocompletion_source_foreach		(EphyAutocompletionSource *source,
							 const gchar *basic_key,
							 EphyAutocompletionSourceForeachFunc func,
							 gpointer data);
void	ephy_autocompletion_source_set_basic_key	(EphyAutocompletionSource *source,
							 const gchar *basic_key);

G_END_DECLS

#endif

