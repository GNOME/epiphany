/*
 *  Copyright (C) 2001 Matt Aubury, Philip Langdale
 *  Copyright (C) 2004 Crispin Flowerday
 *  Copyright (C) 2005 Christian Persch
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
 
#ifndef EPHY_ABOUT_MODULE_H
#define EPHY_ABOUT_MODULE_H
    
#include <nsError.h>
#include <nsIAboutModule.h>

/* a9aea13e-21de-4be8-a07e-a05f11658c55 */
#define EPHY_ABOUT_MODULE_CID \
{ 0xa9aea13e, 0x21de, 0x4be8, \
  { 0xa0, 0x7e, 0xa0, 0x5f, 0x11, 0x65, 0x8c, 0x55 } }

#ifdef HAVE_GECKO_1_8
#define EPHY_ABOUT_NETERROR_CONTRACTID	NS_ABOUT_MODULE_CONTRACTID_PREFIX "neterror"
#define EPHY_ABOUT_NETERROR_CLASSNAME	"Epiphany about:neterror module"
#endif

#define EPHY_ABOUT_EPIPHANY_CONTRACTID	NS_ABOUT_MODULE_CONTRACTID_PREFIX "epiphany"
#define EPHY_ABOUT_EPIPHANY_CLASSNAME	"Epiphany about:epiphany module"
#define EPHY_ABOUT_RECOVER_CONTRACTID	NS_ABOUT_MODULE_CONTRACTID_PREFIX "recover"
#define EPHY_ABOUT_RECOVER_CLASSNAME	"Epiphany about:recover module"

class nsIChannel;
class nsIOutputStream;
class nsIInputStreamChannel;
class nsIURI;

class EphyAboutModule : public nsIAboutModule
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIABOUTMODULE

	EphyAboutModule();
	virtual ~EphyAboutModule();

  private:
	nsresult Redirect(const nsACString&, nsIChannel**);
	nsresult ParseURL(const char*, nsACString&, nsACString&, nsACString&, nsACString&, nsACString&);
#ifdef HAVE_GECKO_1_8
	nsresult GetErrorMessage(nsIURI*, const char*, char**, char**, char**, char**, char**, char**);
	nsresult CreateErrorPage(nsIURI*, nsIChannel**);
#endif
	nsresult CreateRecoverPage(nsIURI*, nsIChannel**);
	nsresult WritePage(nsIURI*, nsIURI*, nsIURI*, const nsACString&, const char*, const char*, const char*, const char*, const char*, const char*, nsIChannel**);
	nsresult Write(nsIOutputStream*, const char*);
};

#endif /* EPHY_ABOUT_MODULE_H */
