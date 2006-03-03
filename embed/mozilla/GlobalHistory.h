/*
 *  Copyright (C) 2001, 2004 Philip Langdale
 *  Copyright (C) 2004 Christian Persch
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

#ifndef EPHY_GLOBAL_HISTORY_H
#define EPHY_GLOBAL_HISTORY_H

#include "ephy-history.h"

#ifdef HAVE_NSIGLOBALHISTORY3_H
#include <nsIGlobalHistory3.h>
#else
#include <nsIGlobalHistory2.h>
#endif /* HAVE_NSIGLOBALHISTORY3_H */

#include <nsCOMPtr.h>
#include <nsAutoPtr.h>

#include "EphyHistoryListener.h"

#define EPHY_GLOBALHISTORY_CLASSNAME	"Epiphany Global History Implementation"

#define EPHY_GLOBALHISTORY_CID					\
{	0xbe0c42c1,						\
	0x39d4,							\
	0x4271,							\
	{ 0xb7, 0x9e, 0xf7, 0xaa, 0x49, 0xeb, 0x6a, 0x15}	\
}

#ifdef HAVE_NSIGLOBALHISTORY3_H
class MozGlobalHistory: public nsIGlobalHistory3
#else
class MozGlobalHistory: public nsIGlobalHistory2
#endif /* HAVE_NSIGLOBALHISTORY3_H */
{
	public:
		MozGlobalHistory ();
		virtual ~MozGlobalHistory();

		NS_DECL_ISUPPORTS
		NS_DECL_NSIGLOBALHISTORY2
#ifdef HAVE_NSIGLOBALHISTORY3_H
		NS_DECL_NSIGLOBALHISTORY3
#endif /* HAVE_NSIGLOBALHISTORY3_H */

	private:
		EphyHistory *mGlobalHistory;
		nsRefPtr<EphyHistoryListener> mHistoryListener;
};

#endif /* EPHY_GLOBAL_HISTORY_H */
