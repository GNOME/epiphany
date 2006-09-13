/*
 * GtkNSSKeyPairDialogs.h
 *
 * Copyright © 2003 Crispin Flowerday <gnome@flowerday.cx>
 * Available under the terms of the GNU General Public License version 2.
 *
 * $Id$
 */ 

#ifndef GTKNSSKEYPAIRDIALOGS_H
#define GTKNSSKEYPAIRDIALOGS_H 1

#include <nsIGenKeypairInfoDlg.h>

// 6a8b1aff-ae8b-4751-982e-4ce5ad544100
#define GTK_NSSKEYPAIRDIALOGS_CID	\
 {0x6a8b1aff, 0xae8b, 0x4751, {0x98, 0x2e, 0x4c, 0xe5, 0xad, 0x54, 0x41, 0x10}}

#define GTK_NSSKEYPAIRDIALOGS_CLASSNAME  "Gtk NSS Key Pair Dialogs"

class GtkNSSKeyPairDialogs
: public nsIGeneratingKeypairInfoDialogs
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGENERATINGKEYPAIRINFODIALOGS

  GtkNSSKeyPairDialogs();
  virtual ~GtkNSSKeyPairDialogs();
};


#endif /* GTKNSSKEYPAIRDIALOGS_H */
