/*
 *  Copyright © Christopher Blizzard
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
 *  ---------------------------------------------------------------------------
 *  Derived from Mozilla.org code, which had the following attributions:
 * 
 *  The Original Code is mozilla.org code.
 *
 *  The Initial Developer of the Original Code is
 *  Christopher Blizzard. Portions created by Christopher Blizzard are Copyright © Christopher Blizzard.  All Rights Reserved.
 *  Portions created by the Initial Developer are Copyright © 2001
 *  the Initial Developer. All Rights Reserved.
 *
 *  Contributor(s):
 *    Christopher Blizzard <blizzard@mozilla.org>
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

#include <nsCOMPtr.h>
#include <nsIDOMMouseEvent.h>

#include "nsIDOMKeyEvent.h"
#include "nsIDOMUIEvent.h"

#include "EmbedEventListener.h"
#include "GeckoBrowser.h"

#include "gecko-embed-signals.h"
#include "gecko-dom-event.h"
#include "gecko-dom-event-internal.h"
#include "gecko-dom-event-private.h"

EmbedEventListener::EmbedEventListener(GeckoBrowser *aOwner)
  : mOwner(aOwner)
{
}

EmbedEventListener::~EmbedEventListener()
{
}

NS_IMPL_ADDREF(EmbedEventListener)
NS_IMPL_RELEASE(EmbedEventListener)
NS_INTERFACE_MAP_BEGIN(EmbedEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUIListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMContextMenuListener)
NS_INTERFACE_MAP_END

inline NS_METHOD
EmbedEventListener::Emit(nsIDOMEvent *aDOMEvent,
			 GeckoEmbedSignals signal,
			 GeckoDOMEventType type)
{
  if (!aDOMEvent)
    return NS_OK;

  // g_print ("Emitting signal '%s'\n", g_signal_name (gecko_embed_signals[signal]));

  /* Check if there are any handlers connected */
  if (!g_signal_has_handler_pending (mOwner->mOwningWidget,
                                     gecko_embed_signals[signal],
				     0, FALSE /* FIXME: correct? */)) {
    return NS_OK;
  }

  GeckoDOMEvent event;
  GECKO_DOM_EVENT_STATIC_INIT (event, aDOMEvent);

  gboolean retval = FALSE;
  g_signal_emit (mOwner->mOwningWidget,
                 gecko_embed_signals[signal], 0,
                 (GeckoDOMEvent*) &event, &retval);
  if (retval) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }

  GECKO_DOM_EVENT_STATIC_DEINIT (event);

  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyDown(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_KEY_DOWN, TYPE_KEY_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::KeyPress(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_KEY_PRESS, TYPE_KEY_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::KeyUp(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_KEY_UP, TYPE_KEY_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::MouseDown(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_MOUSE_DOWN, TYPE_MOUSE_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::MouseUp(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_MOUSE_UP, TYPE_MOUSE_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::MouseClick(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_MOUSE_CLICK, TYPE_MOUSE_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::MouseDblClick(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_MOUSE_DOUBLE_CLICK, TYPE_MOUSE_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::MouseOver(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_MOUSE_OVER, TYPE_MOUSE_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::MouseOut(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_MOUSE_OUT, TYPE_MOUSE_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::FocusIn(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_FOCUS_IN, TYPE_UI_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::FocusOut(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_FOCUS_OUT, TYPE_UI_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::Activate(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_ACTIVATE, TYPE_UI_EVENT);
}

NS_IMETHODIMP
EmbedEventListener::ContextMenu(nsIDOMEvent* aDOMEvent)
{
  return Emit(aDOMEvent, DOM_CONTEXT_MENU, TYPE_MOUSE_EVENT);
}
