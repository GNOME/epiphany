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

#ifndef __FtpProtocolHandler_h__
#define __FtpProtocolHandler_h__
    
#include "nsError.h"
#include "nsIProtocolHandler.h"
#include "nsCURILoader.h"
#include "BaseProtocolContentHandler.h"

#define G_FTP_PROTOCOL_CID                           \
{ /* 5a48bdf4-a422-4eb4-b073-0fc3bee8e670 */         \
    0x5a48bdf4,                                      \
    0xa422,                                          \
    0x4eb4,                                          \
    {0xb0, 0x73, 0x0f, 0xc3, 0xbe, 0xe8, 0xe6, 0x70} \
}
#define G_FTP_PROTOCOL_CONTRACTID NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp"
#define G_FTP_PROTOCOL_CLASSNAME "Epiphany's FTP Protocol Handler"
#define G_FTP_CONTENT_CONTRACTID NS_CONTENT_HANDLER_CONTRACTID_PREFIX \
				 "application-x-gnome-ftp"
#define G_FTP_CONTENT_CLASSNAME "Epiphany's FTP Content Handler"

#define NS_FTPPROTOCOLHANDLER_CID \
{							\
    0x25029490,						\
    0xf132,						\
    0x11d2,						\
    {0x95, 0x88, 0x0, 0x80, 0x5f, 0x36, 0x9f, 0x95}	\
}
#define NS_FTPPROTOCOLHANDLER_CLASSNAME "The FTP Protocol Handler"

class GFtpProtocolHandler : public GBaseProtocolContentHandler
{
  public:
	NS_DECL_ISUPPORTS
	GFtpProtocolHandler() : GBaseProtocolContentHandler("ftp")
				{NS_INIT_ISUPPORTS();};
	virtual ~GFtpProtocolHandler() {};
	/* additional members */
};

#endif // __FtpProtocolHandler_h__
