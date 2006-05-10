/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
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

#ifndef EVENT_CONTEXT_H
#define EVENT_CONTEXT_H

#include "ephy-embed.h"
#include "mozilla-embed-event.h"

class EphyBrowser;
class nsIDOMDocument;
class nsIDOMEvent;
class nsIDOMEventTarget;
class nsIDOMHTMLAnchorElement;
class nsIDOMHTMLAreaElement;
class nsIDOMHTMLBodyElement;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIDOMNode;
class nsIDOMViewCSS;
class nsIURI;

class EventContext
{
public:
	EventContext();
	~EventContext();

	nsresult Init              (EphyBrowser *wrapper);
	nsresult GetMouseEventInfo (nsIDOMMouseEvent *event, MozillaEmbedEvent *info);
	nsresult GetKeyEventInfo   (nsIDOMKeyEvent *event, MozillaEmbedEvent *info);
	nsresult GetTargetDocument (nsIDOMDocument **domDoc);

	static PRBool CheckKeyPress (nsIDOMKeyEvent *aEvent);

private:
	EphyBrowser *mBrowser;
	MozillaEmbedEvent *mEmbedEvent;
	nsCOMPtr<nsIDOMDocument> mDOMDocument;
	nsCOMPtr<nsIDOMViewCSS> mViewCSS;
	nsCOMPtr<nsIURI> mBaseURI;
	nsCString mCharset;

	nsresult GetTargetCoords    (nsIDOMEventTarget *aTarget, PRInt32 *aX, PRInt32 *aY);
	nsresult GatherTextUnder    (nsIDOMNode* aNode, nsAString& aResult);
	nsresult ResolveBaseURL     (const nsAString &relurl, nsACString &url);
	nsresult Unescape 	    (const nsACString &aEscaped, nsACString &aUnescaped);
	nsresult GetEventContext    (nsIDOMEventTarget *EventTarget,
				     MozillaEmbedEvent *info);
	nsresult GetCSSBackground   (nsIDOMNode *node, nsAString& url);
	nsresult IsPageFramed       (nsIDOMNode *node, PRBool *Framed);
	nsresult CheckInput	    (nsIDOMNode *node);
	nsresult CheckLinkScheme    (const nsAString &link);
	nsresult SetIntProperty     (const char *name, int value);
	nsresult SetStringProperty  (const char *name, const char *value);
	nsresult SetStringProperty  (const char *name, const nsAString &value);
	nsresult SetURIProperty     (nsIDOMNode *node, const char *name, const nsAString &value);
	nsresult SetURIProperty     (nsIDOMNode *node, const char *name, const nsACString &value);
};

#endif
