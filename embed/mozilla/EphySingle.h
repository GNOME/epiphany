/*
 *  Copyright © 2003 Christian Persch
 *  Copyright © 2003 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_SINGLE_H
#define EPHY_SINGLE_H

#include <nsCOMPtr.h>
#include <nsIObserver.h>
#include <nsIObserverService.h>

#include "ephy-cookie-manager.h"
#include "ephy-embed-single.h"
#include "ephy-permission-manager.h"

class nsICookie;
class nsIPermission;

class EphySingle : public nsIObserver
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIOBSERVER

	EphySingle();
	virtual ~EphySingle();

	nsresult Init (EphyEmbedSingle *aOwner);
	nsresult Detach ();

	PRBool IsOnline() { return mIsOnline; }

protected:
	nsresult EmitCookieNotification (const char *name, nsISupports *aSubject);
	nsresult EmitPermissionNotification (const char *name, nsISupports *aSubject);
	nsresult ExamineResponse (nsISupports *aSubject);	
	nsresult ExamineRequest (nsISupports *aSubject);	
	nsresult ExamineCookies (nsISupports *aSubject);	
	
private:
	nsCOMPtr<nsIObserverService> mObserverService;
	EphyEmbedSingle *mOwner;
	PRBool mIsOnline;
};

EphyCookie	   *mozilla_cookie_to_ephy_cookie	  (nsICookie *cookie);

EphyPermissionInfo *mozilla_permission_to_ephy_permission (nsIPermission *perm);

#endif
