/*
 *  Copyright © 2000 Nate Case
 *  Copyright © 2000-2004 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef MOZILLA_NOTIFIERS_H
#define MOZILLA_NOTIFIERS_H

#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf.h>

G_BEGIN_DECLS

typedef gboolean (* PrefValueTransformFunc)	(GConfEntry*, GValue*, gpointer);

gboolean mozilla_notifier_transform_bool	(GConfEntry*, GValue*, gpointer);

gboolean mozilla_notifier_transform_bool_invert	(GConfEntry*, GValue*, gpointer);

gboolean mozilla_notifier_transform_int		(GConfEntry*, GValue*, gpointer);

gboolean mozilla_notifier_transform_string	(GConfEntry*, GValue*, gpointer);

guint	 mozilla_notifier_add		(const char *gconf_key,
					 const char *mozilla_pref,
					 PrefValueTransformFunc func,
					 gpointer user_data);

void	 mozilla_notifier_remove	(guint id);

gboolean mozilla_pref_set		(const char *pref,
					 const GValue *value);

gboolean mozilla_notifiers_init		(void);

void	 mozilla_notifiers_shutdown	(void);

G_END_DECLS

#endif
