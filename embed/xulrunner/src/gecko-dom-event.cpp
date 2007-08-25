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

#include <mozilla-config.h>
#include "config.h"

#include "gecko-dom-event.h"
#include "gecko-dom-event-internal.h"
#include "gecko-dom-event-private.h"

#include <nsIDOMEvent.h>

/* GType implementation */

GType
gecko_dom_event_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0)) {
    type = g_boxed_type_register_static
             ("GeckoDOMEvent",
             (GBoxedCopyFunc) gecko_dom_event_copy,
             (GBoxedFreeFunc) gecko_dom_event_free);
  }

  return type;
}

/* Public API */

GeckoDOMEvent *
gecko_dom_event_new  (nsIDOMEvent *aEvent)
{
  /* FIXME use slice alloc */
  GeckoDOMEvent *event = g_new (GeckoDOMEvent, 1);
	
  NS_ADDREF (event->mEvent = aEvent);

  return event;
}

GeckoDOMEvent *
gecko_dom_event_copy (GeckoDOMEvent *aEvent)
{
  return gecko_dom_event_new (aEvent->mEvent);
}

void
gecko_dom_event_free (GeckoDOMEvent *aEvent)
{
  NS_RELEASE (aEvent->mEvent);
  /* FIXME slice alloc */
  g_free (aEvent);
}

nsIDOMEvent *
gecko_dom_event_get_I (GeckoDOMEvent *aEvent)
{
  return aEvent->mEvent;
}
