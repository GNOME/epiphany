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

#ifndef StartHereProtocolHandler_h__
#define StartHereProtocolHandler_h__

#include "nsError.h"

#define G_START_HERE_PROTOCOLHANDLER_CID		\
{ /* a3a7b6e5-7a92-431d-87e6-3bef8e7ada51*/		\
    0xa3a7b6e5,						\
    0x7a92,						\
    0x431d,						\
    {0x87, 0xe6, 0x3b, 0xef, 0x8e, 0x7a, 0xda, 0x51}	\
}
#define G_START_HERE_PROTOCOLHANDLER_CONTRACTID NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "start-here"
#define G_START_HERE_PROTOCOLHANDLER_CLASSNAME "Galeon's start here protocol handler"

class nsIFactory;

extern nsresult NS_NewStartHereHandlerFactory(nsIFactory** aFactory);

#endif // MyportalProtocolHandler_h__
