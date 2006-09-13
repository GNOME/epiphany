/*
 * GtkNSSClientAuthDialogs.h
 *
 * Copyright © 2003 Crispin Flowerday <gnome@flowerday.cx>
 * Available under the terms of the GNU General Public License version 2.
 *
 * $Id$
 */ 

#ifndef GTKNSSCLIENTAUTHDIALOGS_H
#define GTKNSSCLIENTAUTHDIALOGS_H 1

#include <nsIClientAuthDialogs.h>

// 55b3837e-dbde-4c24-9247-f328e3012485
#define GTK_NSSCLIENTAUTHDIALOGS_CID	\
 {0x55b3837e, 0xdbde, 0x4c24, {0x92, 0x47, 0xf3, 0x28, 0xe3, 0x01, 0x24, 0x85}}

#define GTK_NSSCLIENTAUTHDIALOGS_CLASSNAME  "Gtk NSS Client Auth Dialogs"

class GtkNSSClientAuthDialogs
: public nsIClientAuthDialogs
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIENTAUTHDIALOGS

  GtkNSSClientAuthDialogs();
  virtual ~GtkNSSClientAuthDialogs();
};


#endif /* GTKNSSCLIENTAUTHDIALOGS_H */
