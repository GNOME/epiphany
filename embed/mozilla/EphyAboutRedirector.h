/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Gagan Saksena (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef EphyAboutRedirector_h__
#define EphyAboutRedirector_h__

#include "nsIAboutModule.h"
#include "nsXPComFactory.h"

class EphyAboutRedirector : public nsIAboutModule
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    EphyAboutRedirector() {}
    virtual ~EphyAboutRedirector() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};
#define EPHY_ABOUT_REDIRECTOR_CID				\
{ /* f5314c66-b6f6-49b0-bfd0-52f69545afb7 */			\
	0xf5314c66,						\
	0xb6f6,							\
	0x49b0,							\
	{0xbf, 0xd0, 0x52, 0xf6, 0x94, 0x45, 0xaf, 0xb7}	\
}

#define EPHY_ABOUT_REDIRECTOR_OPTIONS_CONTRACTID NS_ABOUT_MODULE_CONTRACTID_PREFIX "options"
#define EPHY_ABOUT_REDIRECTOR_EPIPHANY_CONTRACTID NS_ABOUT_MODULE_CONTRACTID_PREFIX "epiphany"
#define EPHY_ABOUT_REDIRECTOR_CLASSNAME "Epiphany's about redirector"

nsresult NS_NewEphyAboutRedirectorFactory(nsIFactory** aFactory);

#endif // EphyAboutRedirector_h__
