/*
 *  Copyright (C) 2001 Philip Langdale
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

#ifndef __IRCProtocolHandler_h__
#define __IRCProtocolHandler_h__
    
#include "nsError.h"
#include "BaseProtocolContentHandler.h"
#include "nsIProtocolHandler.h"
#include "nsCURILoader.h"

#define G_IRC_PROTOCOL_CID			     \
{ /* aabe33d3-7455-4d8f-87e7-43e4541ace4e */         \
    0xaabe33d3,                                      \
    0x7455,                                          \
    0x4d8f,                                          \
    {0x87, 0xe7, 0x43, 0xe4, 0x54, 0x1a, 0xce, 0x4e} \
}
#define G_IRC_PROTOCOL_CONTRACTID NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "irc"
#define G_IRC_PROTOCOL_CLASSNAME "Epiphany's irc Protocol Handler"
#define G_IRC_CONTENT_CONTRACTID NS_CONTENT_HANDLER_CONTRACTID_PREFIX \
				 "application-x-gnome-irc"
#define G_IRC_CONTENT_CLASSNAME "Epiphany's irc Content Handler"

class GIRCProtocolHandler : public GBaseProtocolContentHandler
{
  public:
	NS_DECL_ISUPPORTS
	GIRCProtocolHandler() : GBaseProtocolContentHandler("irc")
				   {NS_INIT_ISUPPORTS();};
	virtual ~GIRCProtocolHandler() {};
	/* additional members */
};

#endif // __IRCProtocolHandler_h__
