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

#ifndef __ExternalProtocolService_h__
#define __ExternalProtocolService_h__

#include "nsError.h"
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

class GExternalProtocolService : public nsIExternalProtocolService
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIEXTERNALPROTOCOLSERVICE

	GExternalProtocolService();
	virtual ~GExternalProtocolService();
	/* additional members */
};

#define G_EXTERNALPROTOCOLSERVICE_CID                \
{ /* d2a2f743-f126-4f1f-8921-d4e50490f112 */         \
    0xd2a2f743,                                      \
    0xf126,                                          \
    0x4f1f,                                          \
    {0x89, 0x21, 0xd4, 0xe5, 0x04, 0x90, 0xf1, 0x12} \
}
#define G_EXTERNALPROTOCOLSERVICE_CLASSNAME "Galeon's ExternalProtocolService"

class nsIFactory;

extern nsresult NS_NewExternalProtocolServiceFactory(nsIFactory** aFactory);

#endif // __ExternalProtocolService_h__
