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

#ifndef __GlobalHistory_h
#define __GlobalHistory_h

#include "nsError.h"
#include "nsIGlobalHistory.h"
#include "nsIBrowserHistory.h"
#include "ephy-history.h"

#define EPHY_GLOBALHISTORY_CLASSNAME \
 "Epiphany's Global History Implementation"

#define EPHY_GLOBALHISTORY_CID \
 { 0xbe0c42c1, 0x39d4, 0x4271, { 0xb7, 0x9e, 0xf7, 0xaa, 0x49, 0xeb, 0x6a, 0x15}}

/**
 * class GlobalHistory:
 *
 */
class MozGlobalHistory: public nsIGlobalHistory,
	public nsIBrowserHistory
{
	public:
		MozGlobalHistory ();
		virtual ~MozGlobalHistory();

		NS_DECL_ISUPPORTS
		NS_DECL_NSIGLOBALHISTORY
		NS_DECL_NSIBROWSERHISTORY

	private:
		EphyHistory *mGlobalHistory;
};

#endif
