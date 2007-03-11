/*
 *  Copyright © 2005 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef EPHY_REDIRECT_CHANNEL_H
#define EPHY_REDIRECT_CHANNEL_H

#include <nsCOMPtr.h>
#include <nsIChannel.h>

class EphyWrappedChannel : public nsIChannel
{
	public:
		NS_DECL_ISUPPORTS
		NS_FORWARD_NSIREQUEST (mChannel->)
		NS_FORWARD_NSICHANNEL (mChannel->)

		EphyWrappedChannel (nsIChannel *aChannel) : mChannel (aChannel) { }
		virtual ~EphyWrappedChannel () { }

	protected:
		nsCOMPtr<nsIChannel> mChannel;
};

class EphyRedirectChannel : public EphyWrappedChannel
{
	public:
		EphyRedirectChannel (nsIChannel *aChannel) : EphyWrappedChannel (aChannel) { }
		virtual ~EphyRedirectChannel () { }

		NS_IMETHOD SetLoadFlags (nsLoadFlags aFlags);
};

#endif /* !EPHY_REDIRECT_CHANNEL_H */
