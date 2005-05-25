/*
 *  Copyright (C) 2001 Matt Aubury, Philip Langdale
 *  Copyright (C) 2004 Crispin Flowerday
 *  Copyright (C) 2005 Christian Persch
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
 
#ifndef EPHY_PROTOCOL_HANDLER_H
#define EPHY_PROTOCOL_HANDLER_H
    
#include <nsError.h>
#include <nsIAboutModule.h>
#include <nsIProtocolHandler.h>

/* a9aea13e-21de-4be8-a07e-a05f11658c55 */
#define EPHY_PROTOCOL_HANDLER_CID \
{ 0xa9aea13e, 0x21de, 0x4be8, \
  { 0xa0, 0x7e, 0xa0, 0x5f, 0x11, 0x65, 0x8c, 0x55 } }

#define EPHY_PROTOCOL_HANDLER_CONTRACTID	NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "epiphany"
#define EPHY_PROTOCOL_HANDLER_CLASSNAME		"Epiphany Protocol Handler"
#define EPHY_ABOUT_CONTRACTID			NS_ABOUT_MODULE_CONTRACTID_PREFIX "epiphany"
#define EPHY_ABOUT_CLASSNAME			"Epiphany's about:epiphany"

class nsIChannel;
class nsIOutputStream;
class nsIURI;

class EphyProtocolHandler : public nsIProtocolHandler,
			    public nsIAboutModule
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROTOCOLHANDLER

	EphyProtocolHandler (void);
	virtual ~EphyProtocolHandler();

  private:
	nsresult Redirect (const nsACString&, nsIChannel**);
};

#endif /* EPHY_PROTOCOL_HANDLER_H */
