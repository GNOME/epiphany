/*
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef GECKO_FORMSIGNINGDIALOGS_H
#define GECKO_FORMSIGNINGDIALOGS_H

#include <nsIFormSigningDialog.h>

/* 4e42a43e-fbc5-40cc-bcbb-8cbc4e2101eb */
#define GECKO_FORMSIGNINGDIALOGS_CID \
{ 0x4e42a43e, 0xfbc5, 0x40cc, { 0xbc, 0xbb, 0x8c, 0xbc, 0x4e, 0x21, 0x01, 0xeb } }

#define GECKO_FORMSIGNINGDIALOGS_CLASSNAME "Gecko Form Signing Dialogs"

class GeckoFormSigningDialog : public nsIFormSigningDialog
{
  public:
   NS_DECL_ISUPPORTS
   NS_DECL_NSIFORMSIGNINGDIALOG

   GeckoFormSigningDialog();
   ~GeckoFormSigningDialog();
};

#endif /* GECKO_FORMSIGNINGDIALOGS_H */
