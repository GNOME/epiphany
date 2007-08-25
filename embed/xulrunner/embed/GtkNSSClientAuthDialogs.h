/*
 *  Copyright © 2003 Crispin Flowerday <gnome@flowerday.cx>
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
 *
 *  $Id$
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
