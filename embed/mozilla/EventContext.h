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

#include "EphyBrowser.h"

#include "ephy-embed.h"
#include "mozilla-embed-event.h"

#include <nsIDOMMouseEvent.h>
#include <nsIDOMKeyEvent.h>
#include <nsIDOMEvent.h>
#include <nsIDOMNode.h>
#include <nsIDOMHTMLAnchorElement.h>
#include <nsIDOMHTMLAreaElement.h>
#include <nsIDOMHTMLBodyElement.h>
#include <nsIDOMDocument.h>

class EventContext
{
public:
	EventContext();
	~EventContext();

	nsresult Init              (EphyBrowser *wrapper);
	nsresult GetMouseEventInfo (nsIDOMMouseEvent *event, MozillaEmbedEvent *info);
	nsresult GetKeyEventInfo   (nsIDOMKeyEvent *event, MozillaEmbedEvent *info);
	nsresult GetTargetDocument (nsIDOMDocument **domDoc);

private:
	EphyBrowser *mBrowser;
	MozillaEmbedEvent *mEmbedEvent;
	nsCOMPtr<nsIDOMDocument> mDOMDocument;

	nsresult GatherTextUnder    (nsIDOMNode* aNode, nsAString& aResult);
	nsresult ResolveBaseURL     (const nsAString &relurl, nsACString &url);
	nsresult GetEventContext    (nsIDOMEventTarget *EventTarget,
				     MozillaEmbedEvent *info);
	nsresult GetCSSBackground   (nsIDOMNode *node, nsAString& url);
	nsresult IsPageFramed       (nsIDOMNode *node, PRBool *Framed);
	nsresult CheckLinkScheme    (const nsAString &link);
	nsresult SetIntProperty     (const char *name, int value);
	nsresult SetStringProperty  (const char *name, const char *value);
	nsresult SetStringProperty  (const char *name, const nsAString &value);
};

#endif
