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

#ifndef _BaseProtocolHandler_h_
#define _BaseProtocolHandler_h_

#include "nsIProtocolHandler.h"

#include "nsString.h"

class GBaseProtocolHandler : public nsIProtocolHandler
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROTOCOLHANDLER

	GBaseProtocolHandler (const char *aScheme);
	virtual ~GBaseProtocolHandler();
	/* additional members */
  protected:
	nsCString mScheme;
};

#endif //_BaseProtocolHandler_h_
