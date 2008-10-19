/*
 *  Copyright © 2001, 2004 Philip Langdale
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

#ifndef EPHY_GLOBAL_HISTORY_H
#define EPHY_GLOBAL_HISTORY_H

#ifdef HAVE_GECKO_1_9
#include <nsToolkitCompsCID.h>
#include <nsIDownloadHistory.h>
#endif
#ifdef HAVE_NSIGLOBALHISTORY3_H
#include <nsIGlobalHistory3.h>
#else
#include <nsIGlobalHistory2.h>
#endif /* HAVE_NSIGLOBALHISTORY3_H */

#include <nsAutoPtr.h>
#include <nsCOMPtr.h>

#include "ephy-history.h"

#include "EphyHistoryListener.h"

#define EPHY_GLOBALHISTORY_CLASSNAME	"Epiphany Global History Implementation"

#ifdef HAVE_GECKO_1_9
/* Just in case anyone gets the service by CID */
#define EPHY_GLOBALHISTORY_CID NS_NAVHISTORYSERVICE_CID
#else
#define EPHY_GLOBALHISTORY_CID					\
{	0xbe0c42c1,						\
	0x39d4,							\
	0x4271,							\
	{ 0xb7, 0x9e, 0xf7, 0xaa, 0x49, 0xeb, 0x6a, 0x15}	\
}
#endif

#ifdef HAVE_NSIGLOBALHISTORY3_H
class MozGlobalHistory: public nsIGlobalHistory3
#else
class MozGlobalHistory: public nsIGlobalHistory2
#endif /* HAVE_NSIGLOBALHISTORY3_H */
#ifdef HAVE_GECKO_1_9
                       , public nsIDownloadHistory
#endif
{
	public:
		MozGlobalHistory ();
		virtual ~MozGlobalHistory();

		NS_DECL_ISUPPORTS
		NS_DECL_NSIGLOBALHISTORY2
#ifdef HAVE_NSIGLOBALHISTORY3_H
		NS_DECL_NSIGLOBALHISTORY3
#endif /* HAVE_NSIGLOBALHISTORY3_H */
#ifdef HAVE_GECKO_1_9
                NS_DECL_NSIDOWNLOADHISTORY
#endif

	private:
		EphyHistory *mGlobalHistory;
		nsRefPtr<EphyHistoryListener> mHistoryListener;
};

#endif /* EPHY_GLOBAL_HISTORY_H */
