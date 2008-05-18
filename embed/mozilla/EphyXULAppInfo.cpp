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

#include "mozilla-config.h"
#include "config.h"

#include <nsStringGlue.h>

#include "EphyXULAppInfo.h"

NS_IMPL_ISUPPORTS2 (EphyXULAppInfo, nsIXULRuntime, nsIXULAppInfo)

EphyXULAppInfo::EphyXULAppInfo ()
  : mLogConsoleErrors (PR_TRUE)
{
}

EphyXULAppInfo::~EphyXULAppInfo ()
{
}

/* readonly attribute ACString vendor; */
NS_IMETHODIMP
EphyXULAppInfo::GetVendor(nsACString & aVendor)
{
  aVendor.Assign ("GNOME");
  return NS_OK;
}

/* readonly attribute ACString name; */
NS_IMETHODIMP
EphyXULAppInfo::GetName(nsACString & aName)
{
  aName.Assign ("GNOME Web Browser");
  return NS_OK;
}

/* readonly attribute ACString ID; */
NS_IMETHODIMP
EphyXULAppInfo::GetID(nsACString & aID)
{
  aID.Assign ("{8cbd4d83-3182-4d7e-9889-a8d77bf1f205}");
  return NS_OK;
}

/* readonly attribute ACString version; */
NS_IMETHODIMP
EphyXULAppInfo::GetVersion(nsACString & aVersion)
{
  aVersion.Assign (VERSION);
  return NS_OK;
}

/* readonly attribute ACString appBuildID; */
NS_IMETHODIMP
EphyXULAppInfo::GetAppBuildID(nsACString & aAppBuildID)
{
  aAppBuildID.Assign (EPHY_BUILD_ID);
  return NS_OK;
}

/* readonly attribute ACString platformVersion; */
NS_IMETHODIMP
EphyXULAppInfo::GetPlatformVersion(nsACString & aPlatformVersion)
{
  aPlatformVersion.Assign ("1.9");
  return NS_OK;
}

/* readonly attribute ACString platformBuildID; */
NS_IMETHODIMP
EphyXULAppInfo::GetPlatformBuildID(nsACString & aPlatformBuildID)
{
  aPlatformBuildID.Assign (EPHY_BUILD_ID);
  return NS_OK;
}

/* readonly attribute boolean inSafeMode; */
NS_IMETHODIMP
EphyXULAppInfo::GetInSafeMode(PRBool *aInSafeMode)
{
  *aInSafeMode = PR_FALSE;
  return NS_OK;
}

/* attribute boolean logConsoleErrors; */
NS_IMETHODIMP
EphyXULAppInfo::GetLogConsoleErrors(PRBool *aLogConsoleErrors)
{
  *aLogConsoleErrors = mLogConsoleErrors;
  return NS_OK;
}

NS_IMETHODIMP
EphyXULAppInfo::SetLogConsoleErrors(PRBool aLogConsoleErrors)
{
  mLogConsoleErrors = aLogConsoleErrors;
  return NS_OK;
}

/* readonly attribute AUTF8String OS; */
NS_IMETHODIMP
EphyXULAppInfo::GetOS(nsACString & aOS)
{
  aOS.Assign (EPHY_HOST_OS);
  return NS_OK;
}

/* readonly attribute AUTF8String XPCOMABI; */
NS_IMETHODIMP
EphyXULAppInfo::GetXPCOMABI(nsACString & aXPCOMABI)
{
  aXPCOMABI.Assign (EPHY_HOST_CPU "-gcc3");
  return NS_OK;
}
