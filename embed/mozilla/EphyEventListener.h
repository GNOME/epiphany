/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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

#ifndef EPHY_EVENT_LISTENER_H
#define EPHY_EVENT_LISTENER_H

#include "ephy-embed.h"

#include <nsIDOMEventListener.h>

class EphyEventListener : public nsIDOMEventListener
{
public:
	EphyEventListener();
	virtual ~EphyEventListener();

	nsresult Init(EphyEmbed *aOwner);

	NS_DECL_ISUPPORTS

	// nsIDOMEventListener

	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

private:
	EphyEmbed *mOwner;

	nsresult HandleFaviconLink (nsIDOMNode *node);
};

#endif
