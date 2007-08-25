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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef GECKO_DOM_EVENT_H
#define GECKO_DOM_EVENT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GECKO_TYPE_DOM_EVENT (gecko_dom_event_get_type ())

typedef struct _GeckoDOMEvent GeckoDOMEvent;

GType           gecko_dom_event_get_type (void);

GeckoDOMEvent * gecko_dom_event_copy     (GeckoDOMEvent *);

void            gecko_dom_event_free     (GeckoDOMEvent *);

G_END_DECLS

#endif
