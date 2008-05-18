/*
 *  Copyright © 2008 Christian Persch
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
 */

#ifndef EPHY_XUL_APP_INFO_H
#define EPHY_XUL_APP_INFO_H

#include <nsIXULAppInfo.h>
#include <nsIXULRuntime.h>

#include <nsAutoPtr.h>
#include <nsCOMPtr.h>

#define EPHY_XUL_APP_INFO_CLASSNAME	"Epiphany XUL App Info"

/* 3032bcd2-663c-4583-88bf-6f251123f6dd */
#define EPHY_XUL_APP_INFO_CID { 0x3032bcd2, 0x663c, 0x4583, { 0x88, 0xbf, 0x6f, 0x25, 0x11, 0x23, 0xf6, 0xdd } }

class EphyXULAppInfo : public nsIXULAppInfo,
                       public nsIXULRuntime
{
	public:
		EphyXULAppInfo ();
		virtual ~EphyXULAppInfo();

		NS_DECL_ISUPPORTS
                NS_DECL_NSIXULAPPINFO
                NS_DECL_NSIXULRUNTIME

	private:
                PRBool mLogConsoleErrors;
};

#endif /* EPHY_XUL_APP_INFO_H */
