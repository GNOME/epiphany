/*
 *  Copyright (C) 2001 Philip Langdale
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
 */

#ifndef __FilePicker_h
#define __FilePicker_h

#include "nsIFilePicker.h"
#include "nsError.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include <gtk/gtktogglebutton.h>
#include "ephy-embed-shell.h"

#define G_FILEPICKER_CID			     \
{ /* 3636dc79-0b42-4bad-8a3f-ae15d3671d17 */         \
    0x3636dc79,                                      \
    0x0b42,                                          \
    0x4bad,                                          \
    {0x8a, 0x3f, 0xae, 0x15, 0xd3, 0x67, 0x1d, 0x17} \
}

#define G_FILEPICKER_CONTRACTID "@mozilla.org/filepicker;1"
#define G_FILEPICKER_CLASSNAME "Epiphany's File Picker Implementation"

class nsIFactory;

extern nsresult NS_NewFilePickerFactory(nsIFactory** aFactory);

/* Header file */
class GFilePicker : public nsIFilePicker
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIFILEPICKER
	enum {  returnOK = nsIFilePicker::returnOK,
		returnCancel = nsIFilePicker::returnCancel,
		returnReplace = nsIFilePicker::returnReplace };

	GFilePicker();
	virtual ~GFilePicker();

	/* additional members */
	NS_METHOD InitWithGtkWidget(GtkWidget *aParentWidget, 
				    const char *aTitle, PRInt16 aMode);
	NS_METHOD SanityCheck(PRBool *retIsSane);

  private:
	NS_METHOD HandleFilePickerResult();
	NS_METHOD ValidateFilePickerResult(PRInt16 *retval);

	nsCOMPtr<nsIDOMWindow> mParent;

	nsCString mTitle;
	nsCString mFilter;
	nsCString mDefaultString;

	nsCOMPtr<nsILocalFile> mFile;
	nsCOMPtr<nsILocalFile> mDisplayDirectory;

	PRInt16 mMode;

	GtkWidget *mParentWidget;	
	GtkWidget *mFileSelector;
};

#endif
