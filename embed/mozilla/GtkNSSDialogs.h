/*
 * GtkNSSDialogs.h
 *
 * Copyright (C) 2003 Crispin Flowerday <gnome@flowerday.cx>
 * Available under the terms of the GNU General Public License version 2.
 */ 

#ifndef GTKNSSDIALOGS_H
#define GTKNSSDIALOGS_H 1

#include <nsError.h>
#include "nsIBadCertListener.h"
#include "nsICertificateDialogs.h"

// 7a50a10d-9425-4e12-84b1-5822edacd8ce
#define GTK_NSSDIALOGS_CID	\
 {0x7a50a10d, 0x9425, 0x4e12, {0x84, 0xb1, 0x58, 0x22, 0xed, 0xac, 0xd8, 0xce}}

#define GTK_NSSDIALOGS_CLASSNAME  "Gtk NSS Dialogs"

class GtkNSSDialogs
: public nsIBadCertListener,
  public nsICertificateDialogs
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBADCERTLISTENER
  NS_DECL_NSICERTIFICATEDIALOGS

  GtkNSSDialogs();
  virtual ~GtkNSSDialogs();
};


#endif /* GTKNSSDIALOGS_H */
