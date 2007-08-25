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

#ifndef __EmbedEventListener_h
#define __EmbedEventListener_h

#include <nsIDOMKeyListener.h>
#include <nsIDOMMouseListener.h>
#include <nsIDOMUIListener.h>
#include <nsIDOMContextMenuListener.h>

#include <gecko-embed-signals.h>

class GeckoBrowser;

class EmbedEventListener : public nsIDOMKeyListener,
                           public nsIDOMMouseListener,
                           public nsIDOMUIListener,
                           public nsIDOMContextMenuListener
{
 public:

  EmbedEventListener(GeckoBrowser *aOwner);
  virtual ~EmbedEventListener();

  NS_DECL_ISUPPORTS

  // nsIDOMEventListener

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  // nsIDOMKeyListener
  
  NS_IMETHOD KeyDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aDOMEvent);

  // nsIDOMMouseListener

  NS_IMETHOD MouseDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aDOMEvent);

  // nsIDOMUIListener

  NS_IMETHOD Activate(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD FocusIn(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD FocusOut(nsIDOMEvent* aDOMEvent);

  // nsIDOMContextMenuListener
  NS_IMETHOD ContextMenu(nsIDOMEvent *aDOMEvent);

 private:
  GeckoBrowser *mOwner;

  enum GeckoDOMEventType {
    TYPE_KEY_EVENT,
    TYPE_MOUSE_EVENT,
    TYPE_UI_EVENT
  };

  NS_METHOD Emit(nsIDOMEvent *aDOMEvent,
		 GeckoEmbedSignals signal,
		 GeckoDOMEventType);
};

#endif /* __EmbedEventListener_h */
