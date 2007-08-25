/*
 *  Copyright © 2002 Philip Langdale
 *  Copyright © 2004 Crispin Flowerday
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

#ifndef EPHY_SIDEBAR_H
#define EPHY_SIDEBAR_H

#include <nsISidebar.h>

class nsIComponentManager;
class nsIFile;
struct nsModuleComponentInfo;

#define EPHY_SIDEBAR_CLASSNAME \
 "Epiphany's Sidebar Implementation"

#define EPHY_SIDEBAR_CID \
{  /* {50f13159-f9b9-44b3-b18e-6ee5d85a202a} */      \
    0x50f13159,                                      \
    0xf9b9,                                          \
    0x44b3,                                          \
    {0xb1, 0x8e, 0x6e, 0xe5, 0xd8, 0x5a, 0x20, 0x2a} \
}

class EphySidebar : public nsISidebar
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIDEBAR

    EphySidebar();

    static NS_METHOD Register (nsIComponentManager* aComponentManager,
			       nsIFile* aPath,
			       const char* aRegistryLocation,
			       const char* aComponentType,
			       const nsModuleComponentInfo* aInfo);

    static NS_METHOD Unregister (nsIComponentManager* aComponentManager,
				 nsIFile* aPath,
				 const char* aRegistryLocation,
				 const nsModuleComponentInfo* aInfo);

  private:
    ~EphySidebar();
};

#endif /* ! EPHY_SIDEBAR_H */

