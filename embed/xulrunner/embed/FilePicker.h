/*
 *  Copyright © 2001 Philip Langdale
 *  Copyright © 2003 Christian Persch
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

#ifndef EPHY_FILEPICKER_H
#define EPHY_FILEPICKER_H

#include <nsIFilePicker.h>

class nsIDOMWindow;

#include "ephy-file-chooser.h"

#define G_FILEPICKER_CID			     \
{ /* 3636dc79-0b42-4bad-8a3f-ae15d3671d17 */         \
    0x3636dc79,                                      \
    0x0b42,                                          \
    0x4bad,                                          \
    {0x8a, 0x3f, 0xae, 0x15, 0xd3, 0x67, 0x1d, 0x17} \
}

#define G_FILEPICKER_CONTRACTID	"@mozilla.org/filepicker;1"
#define G_FILEPICKER_CLASSNAME	"Epiphany File Picker Implementation"

class nsIFactory;

extern nsresult NS_NewFilePickerFactory(nsIFactory** aFactory);

class GFilePicker : public nsIFilePicker
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIFILEPICKER
	
	GFilePicker();
	virtual ~GFilePicker();

private:
	EphyFileChooser *mDialog;
	PRInt16 mMode;
	nsString mDefaultString;
	nsCOMPtr<nsIDOMWindow> mParent;
};

#endif
