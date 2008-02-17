/*
 *  Copyright © 2003 Crispin Flowerday <gnome@flowerday.cx>
 *  Copyright © 2006 Christian Persch
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
 * $Id$
 */ 

#ifndef GTKNSSDIALOGS_H
#define GTKNSSDIALOGS_H 1

#ifdef HAVE_GECKO_1_9
#include <nsIBadCertListener2.h>
#else
#include <nsIBadCertListener.h>
#endif
#include <nsICertificateDialogs.h>
#include <nsITokenPasswordDialogs.h>
#include <nsITokenDialogs.h>
#include <nsIDOMCryptoDialogs.h>

class nsIPK11Token;
class nsIPKCS11Slot;

/* 7a50a10d-9425-4e12-84b1-5822edacd8ce */
#define GTK_NSSDIALOGS_CID	\
 {0x7a50a10d, 0x9425, 0x4e12, {0x84, 0xb1, 0x58, 0x22, 0xed, 0xac, 0xd8, 0xce}}

#define GTK_NSSDIALOGS_CLASSNAME  "Gtk NSS Dialogs"

class GtkNSSDialogs : public nsICertificateDialogs,
#ifndef HAVE_GECKO_1_9
                      public nsIBadCertListener,
#endif
		      public nsITokenPasswordDialogs,
		      public nsITokenDialogs,
		      public nsIDOMCryptoDialogs
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICERTIFICATEDIALOGS
#ifndef HAVE_GECKO_1_9
    NS_DECL_NSIBADCERTLISTENER
#endif
    NS_DECL_NSITOKENPASSWORDDIALOGS
    NS_DECL_NSITOKENDIALOGS
    NS_DECL_NSIDOMCRYPTODIALOGS

    GtkNSSDialogs();
    virtual ~GtkNSSDialogs();

  private:
    nsresult GetTokenAndSlotFromName(const PRUnichar*, nsIPK11Token**, nsIPKCS11Slot**);
};

#endif /* GTKNSSDIALOGS_H */
