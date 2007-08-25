/*
 *  Copyright © 2004 Christian Persch
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

#ifndef EPHY_REDIRECT_LISTENER_H
#define EPHY_REDIRECT_LISTENER_H

#include "ephy-history.h"

#include <nsIWebProgressListener.h>
#include <nsWeakReference.h>

/* 6a9533c6-f068-4e63-8225-5feba0b54d6b */
#define EPHY_REDIRECTLISTENER_CID \
{ 0x6a9533c6, 0xf068, 0x4e63, { 0x82, 0x25, 0x5f, 0xeb, 0xa0, 0xb5, 0x4d, 0x6b } }
#define EPHY_REDIRECTLISTENER_CLASSNAME		"Epiphany Redirect Listener Class"

class EphyHistoryListener : public nsIWebProgressListener,
			    public nsSupportsWeakReference
{
	public:
		EphyHistoryListener();
		virtual ~EphyHistoryListener();

		nsresult Init (EphyHistory *aHistory);
	
		NS_DECL_ISUPPORTS
		NS_DECL_NSIWEBPROGRESSLISTENER
	private:
		EphyHistory *mHistory;
};

#endif /* EPHY_REDIRECT_LISTENER_H */
