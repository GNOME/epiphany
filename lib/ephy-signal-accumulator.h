/*
 *  Copyright © 2004 Christian Persch
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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_SIGNAL_ACCUMULATORS_H
#define EPHY_SIGNAL_ACCUMULATORS_H

#include <glib-object.h>

G_BEGIN_DECLS

gboolean ephy_signal_accumulator_object (GSignalInvocationHint *ihint,
					 GValue *return_accu,
					 const GValue *handler_return,
					 gpointer accu_data);

gboolean ephy_signal_accumulator_string	(GSignalInvocationHint *ihint,
					 GValue *return_accu,
					 const GValue *handler_return,
					 gpointer accu_data);

G_END_DECLS

#endif /* EPHY_SIGNAL_ACCUMULATORS_H */
