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
 *
 *  $Id$
 */

/* We are registering protocol handlers for news, mailto and irc
   because mozilla load all modules on startup, so if you have these
   components installed it will try to use them as protocol handlers.
   Our implementation just forward to external protocol service.
   Ftp is a special case, it can be handled by mozilla but we
   want to use an external client when one is configured in GNOME
   handlers.
   IMPORTANT: there is no need to register handlers for other protocols
   here. Once they are configured in GNOME, they will just work.
*/

#ifndef EXTERNAL_PROTOCOL_HANDLERS
#define EXTERNAL_PROTOCOL_HANDLERS

#include "nsError.h"
#include "nsIContentHandler.h"
#include "nsIProtocolHandler.h"
#include "nsCURILoader.h"
#include "nsString.h"

class GBaseProtocolHandler : public nsIProtocolHandler
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROTOCOLHANDLER

	GBaseProtocolHandler (const char *aScheme);
	virtual ~GBaseProtocolHandler();
  protected:
	nsCString mScheme;
};

class GBaseProtocolContentHandler : public GBaseProtocolHandler,
				    public nsIContentHandler
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSICONTENTHANDLER

	NS_IMETHODIMP NewChannel(nsIURI *aURI, nsIChannel **_retval);

	GBaseProtocolContentHandler (const char *aScheme);
	virtual ~GBaseProtocolContentHandler();
  protected:
	nsCString mMimeType;
};

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

	GIRCProtocolHandler() : GBaseProtocolContentHandler("irc") {};
	virtual ~GIRCProtocolHandler() {};
};

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

class GFtpProtocolHandler : public GBaseProtocolContentHandler
{
  public:
	NS_DECL_ISUPPORTS
	GFtpProtocolHandler() : GBaseProtocolContentHandler("ftp") {};
	virtual ~GFtpProtocolHandler() {};
};

#define G_MAILTO_PROTOCOL_CID                        \
{ /* aabe33d3-7455-4d8f-87e7-43e4541ace4d */         \
    0xaabe33d3,                                      \
    0x7455,                                          \
    0x4d8f,                                          \
    {0x87, 0xe7, 0x43, 0xe4, 0x54, 0x1a, 0xce, 0x4d} \
}
#define G_MAILTO_PROTOCOL_CONTRACTID NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "mailto"
#define G_MAILTO_PROTOCOL_CLASSNAME "Epiphany's mailto Protocol Handler"
#define G_MAILTO_CONTENT_CONTRACTID NS_CONTENT_HANDLER_CONTRACTID_PREFIX \
				 "application-x-gnome-mailto"
#define G_MAILTO_CONTENT_CLASSNAME "Epiphany's mailto Content Handler"

class GMailtoProtocolHandler : public GBaseProtocolContentHandler
{
  public:
	NS_DECL_ISUPPORTS
	GMailtoProtocolHandler() : GBaseProtocolContentHandler("mailto") {};
	virtual ~GMailtoProtocolHandler() {};
  private:
};

#define G_NEWS_PROTOCOL_CID                        \
{ /* 3583f8f9-abc7-4ff0-84cb-0eefcaec99f8 */         \
    0x3583f8f9,                                      \
    0xabc7,                                          \
    0x4ff0,                                          \
    {0x84, 0xcb, 0x0e, 0xef, 0xca, 0xec, 0x99, 0xf8} \
}
#define G_NEWS_PROTOCOL_CONTRACTID NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "news"
#define G_NEWS_PROTOCOL_CLASSNAME "Epiphany's news Protocol Handler"
#define G_NEWS_CONTENT_CONTRACTID NS_CONTENT_HANDLER_CONTRACTID_PREFIX \
				 "application-x-gnome-news"
#define G_NEWS_CONTENT_CLASSNAME "Epiphany's news Content Handler"

class GNewsProtocolHandler : public GBaseProtocolContentHandler
{
  public:
	NS_DECL_ISUPPORTS

	GNewsProtocolHandler() : GBaseProtocolContentHandler("news") {};
	virtual ~GNewsProtocolHandler() {};
  private:
};

#endif /* EXTERNAL_PROTOCOL_HANDLERS */
