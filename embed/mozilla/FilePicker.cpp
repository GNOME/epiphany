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

/* Things to be aware of:
 *
 * This filepicker, like the mozilla one, does not make an attempt
 * to verify the validity of the initial directory you pass it.
 * It does check that the user doesn't give it a garbage path
 * during use, but it is the caller's responsibility to give a
 * sensible initial path.
 *
 * At the current moment, we instantiate the filepicker directly
 * in our contenthandler where there is path verification code
 * and else where through our C wrapper, which also does verification.
 * If, at a future date, you need to instantiate filepicker without
 * using the C wrapper, please verify the initial path. See
 * ContentHandler for a way to do this.
 */

#include "ephy-string.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

#include <glib/gconvert.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>

#include "nsIFilePicker.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIPrefService.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsIChannel.h"
#include "nsIFileChannel.h"
#include "nsNetCID.h"
#include "nsILocalFile.h"
#include "nsIPromptService.h"
#include "nsReadableUtils.h"

#include <libgnome/gnome-util.h>

#include "FilePicker.h"
#include "MozillaPrivate.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(GFilePicker, nsIFilePicker)

GFilePicker::GFilePicker(PRBool aShowContentCheck, FileFormat *aFileFormats) :
			mShowContentCheck(aShowContentCheck),
			mSaveContentCheck(NULL),
			mFileFormats(aFileFormats)
{
	NS_INIT_ISUPPORTS();

	/* member initializers and constructor code */
	mFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
	mDisplayDirectory = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
	mDisplayDirectory->InitWithNativePath(nsDependentCString(g_get_home_dir()));
}

GFilePicker::~GFilePicker()
{
	/* destructor code */
}

////////////////////////////////////////////////////////////////////////////////
// begin nsIFilePicker impl
////////////////////////////////////////////////////////////////////////////////

/* void init (in nsIDOMWindowInternal parent, in wstring title, in short mode); */
NS_IMETHODIMP GFilePicker::Init(nsIDOMWindowInternal *aParent, 
				const PRUnichar *aTitle, PRInt16 aMode)
{
	mParent = do_QueryInterface(aParent);
	mParentWidget = MozillaFindGtkParent(mParent);
	mTitle = NS_ConvertUCS2toUTF8(aTitle);
	mMode = aMode;
	
	return NS_OK;
}

/* void appendFilters (in long filterMask); */
NS_IMETHODIMP GFilePicker::AppendFilters(PRInt32 aFilterMask)
{
	//This function cannot be implemented due to the crippled
	//nature of GtkFileSelection, but NS_ERROR_NOT_IMPLEMENTED
	//is interpreted as a terminal error by some callers.
	return NS_OK;
}

/* void appendFilter (in wstring title, in wstring filter); */
NS_IMETHODIMP GFilePicker::AppendFilter(const PRUnichar *aTitle,
					const PRUnichar *aFilter)
{
	//GtkFileSelection is crippled, so we can't provide a short-list
	//of filters to choose from. We provide minimal functionality
	//by using the most recent AppendFilter call as the active filter.
	mFilter = NS_ConvertUCS2toUTF8(aFilter);
	return NS_OK;
}

/* attribute long filterIndex; */
NS_IMETHODIMP GFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
	return NS_OK;
}

/* attribute wstring defaultString; */
NS_IMETHODIMP GFilePicker::GetDefaultString(PRUnichar * *aDefaultString)
{
	guint bytesRead(0), bytesWritten(0);
	gchar *utf8DefaultString = g_filename_to_utf8(mDefaultString.get(), -1,
						      &bytesRead,
						      &bytesWritten, NULL);

	*aDefaultString = ToNewUnicode(NS_ConvertUTF8toUCS2(utf8DefaultString));
	g_free(utf8DefaultString);

	return NS_OK;
}
NS_IMETHODIMP GFilePicker::SetDefaultString(const PRUnichar *aDefaultString)
{
	if (aDefaultString)
	{
		guint bytesRead(0), bytesWritten(0);
		gchar *localeDefaultString =
			g_filename_from_utf8(NS_ConvertUCS2toUTF8(aDefaultString).get(),
					     -1, &bytesRead,
					     &bytesWritten, NULL);
		mDefaultString = localeDefaultString;					     
		g_free(localeDefaultString);
	}
	else
		mDefaultString = "";
	return NS_OK;
}

/* attribute wstring defaultExtension; */
// Again, due to the crippled file selector, we can't really
// do anything here.
NS_IMETHODIMP GFilePicker::GetDefaultExtension(PRUnichar * *aDefaultExtension)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GFilePicker::SetDefaultExtension(const PRUnichar *aDefaultExtension)
{
    return NS_OK;
}

/* attribute nsILocalFile displayDirectory; */
NS_IMETHODIMP GFilePicker::GetDisplayDirectory(nsILocalFile * *aDisplayDirectory)
{
	NS_IF_ADDREF(*aDisplayDirectory = mDisplayDirectory);
	return NS_OK;
}
NS_IMETHODIMP GFilePicker::SetDisplayDirectory(nsILocalFile * aDisplayDirectory)
{
	mDisplayDirectory = aDisplayDirectory;
	return NS_OK;
}

/* readonly attribute nsILocalFile file; */
NS_IMETHODIMP GFilePicker::GetFile(nsILocalFile * *aFile)
{
	NS_IF_ADDREF(*aFile = mFile);
	return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP GFilePicker::GetFileURL(nsIFileURL * *aFileURL)
{
	nsCOMPtr<nsIFileURL> fileURL = 
		do_CreateInstance(NS_STANDARDURL_CONTRACTID);
	fileURL->SetFile(mFile);
	NS_IF_ADDREF(*aFileURL = fileURL);
	return NS_OK;
}

/* readonly attribute nsISimpleEnumerator files; */
NS_IMETHODIMP GFilePicker::GetFiles(nsISimpleEnumerator * *aFiles)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* short show (); */
NS_IMETHODIMP GFilePicker::Show(PRInt16 *_retval)
{
	mFileSelector = gtk_file_selection_new(mTitle.get());

	nsCAutoString cFileName;
	if(mMode == nsIFilePicker::modeGetFolder)
		cFileName.Assign("");
	else
		cFileName = mDefaultString;

	nsCAutoString cDirName;
	mDisplayDirectory->GetNativePath(cDirName);

	nsCAutoString cFullPath;
	cFullPath.Assign(cDirName + NS_LITERAL_CSTRING("/") + cFileName);
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(mFileSelector),
				 	cFullPath.get());

	if (!mFilter.IsEmpty())
	{
		gtk_file_selection_complete(GTK_FILE_SELECTION(mFileSelector),
					    mFilter.get());
	}

	if (mParentWidget)
		gtk_window_set_transient_for(GTK_WINDOW(mFileSelector),
					     GTK_WINDOW(mParentWidget));

	if (mShowContentCheck)
	{
		GtkWidget *bbox = gtk_hbutton_box_new ();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox),
					  GTK_BUTTONBOX_END);
		gtk_box_set_spacing(GTK_BOX(bbox), 0);
        	gtk_box_pack_end(GTK_BOX(GTK_FILE_SELECTION(mFileSelector)->action_area),
				 bbox, TRUE, TRUE, 0);

		mSaveContentCheck = 
			gtk_check_button_new_with_label(_("Save with content"));

		gtk_box_pack_start(GTK_BOX(bbox), mSaveContentCheck,
				   FALSE, FALSE, 0);

		gtk_widget_show_all(bbox);
	}

	if (mFileFormats)
	{
		mFormatChooser = gtk_option_menu_new();
		GtkMenu *options = GTK_MENU(gtk_menu_new());

		FileFormat *current = mFileFormats;
		while (current->description != NULL)
		{
			/* FIXME: the label should include the extensions too */
			gchar *label = current->description;
			GtkWidget *item = gtk_menu_item_new_with_label(label);
			gtk_widget_show(item);
			gtk_menu_shell_append(GTK_MENU_SHELL(options), item);
			current++;
		}
		gtk_option_menu_set_menu(GTK_OPTION_MENU(mFormatChooser),
					 GTK_WIDGET(options));
		gtk_widget_show(mFormatChooser);
		gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION (mFileSelector)->action_area),
				   mFormatChooser,
				   FALSE, TRUE, 0);
	}
	else
	{
		mFormatChooser = NULL;
	}

	if (mMode == nsIFilePicker::modeGetFolder)
	{
		gtk_widget_set_sensitive(GTK_FILE_SELECTION(mFileSelector)
					 ->file_list, FALSE);
	}

	gtk_window_set_modal(GTK_WINDOW(mFileSelector), TRUE);

	gint retVal = gtk_dialog_run(GTK_DIALOG(mFileSelector));
	
	if (retVal == GTK_RESPONSE_OK)
	{
		HandleFilePickerResult(_retval);
	}
	else
	{
		*_retval = returnCancel;
	}

	gtk_widget_hide(mFileSelector);
	gtk_widget_destroy(mFileSelector);

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// begin local public methods impl
////////////////////////////////////////////////////////////////////////////////

NS_METHOD GFilePicker::InitWithGtkWidget (GtkWidget *aParentWidget, 
					  const char *aTitle, PRInt16 aMode)
{
	mParentWidget = aParentWidget;

	mTitle = nsDependentCString(aTitle);

	mMode = mMode;

	mFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);

	return NS_OK;
}

NS_METHOD GFilePicker::SanityCheck (PRBool *retIsSane)
{
	*retIsSane = PR_TRUE;

	PRBool dirExists, fileExists = PR_TRUE;

	if (mDisplayDirectory)
	{
		mDisplayDirectory->Exists (&dirExists);
	}
	else
	{
		dirExists = PR_FALSE;
	}

	if (mMode == nsIFilePicker::modeOpen)
	{
		mFile->Exists (&fileExists);
	}
	
	if (!dirExists || !fileExists)
	{
		GtkWidget *errorDialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("The specified path does not exist."));

		if (mParentWidget)
			gtk_window_set_transient_for(GTK_WINDOW(errorDialog),
						     GTK_WINDOW(mFileSelector));

		gtk_window_set_modal (GTK_WINDOW(errorDialog), TRUE);
		gtk_dialog_run (GTK_DIALOG(errorDialog));
		*retIsSane = PR_FALSE;
		return NS_OK;
	}

	PRBool correctType;
	char *errorText;
	if (mMode == nsIFilePicker::modeGetFolder)
	{
		mDisplayDirectory->IsDirectory (&correctType);
		errorText = g_strdup (_("A file was selected when a "
					"folder was expected."));
	}
	else
	{
		mFile->IsFile (&correctType);
		errorText = g_strdup (_("A folder was selected when a "
				        "file was expected."));
	}
	
	if(!correctType)
	{
		GtkWidget *errorDialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				errorText);

		if (mParentWidget)
			gtk_window_set_transient_for(GTK_WINDOW(errorDialog),
						     GTK_WINDOW(mFileSelector));

		gtk_window_set_modal (GTK_WINDOW(errorDialog), TRUE);
		gtk_dialog_run (GTK_DIALOG(errorDialog));
		*retIsSane = PR_FALSE;
	}
	g_free (errorText);

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// begin local private methods impl
////////////////////////////////////////////////////////////////////////////////

NS_METHOD GFilePicker::HandleFilePickerResult(PRInt16 *retval)
{
	*retval = returnCancel;
	nsresult rv;

	const char *fileName = gtk_file_selection_get_filename(GTK_FILE_SELECTION(mFileSelector));

	if (!fileName || strlen(fileName) == 0) return NS_ERROR_FAILURE;

	if (mMode == nsIFilePicker::modeSave)
	{
		if (!ephy_gui_confirm_overwrite_file (mFileSelector,
					              fileName))
		{
			return NS_OK;
		}
	}

	const nsACString &cFileName = nsDependentCString(fileName);
	mFile->InitWithNativePath(cFileName);

	if (mMode == nsIFilePicker::modeGetFolder)
	{
		mDisplayDirectory->InitWithNativePath(cFileName);
		mDefaultString = "";
	}
	else
	{
		nsCOMPtr<nsIFile> directory;
		mFile->GetParent(getter_AddRefs(directory));
		mDisplayDirectory = do_QueryInterface(directory);
		mFile->GetNativeLeafName(mDefaultString);
	}

	PRBool passesSanityCheck;
	rv = SanityCheck(&passesSanityCheck);
	if (NS_SUCCEEDED(rv) && !passesSanityCheck) return NS_ERROR_FAILURE;

	if (mFormatChooser)
	{
		GtkWidget *menu = gtk_option_menu_get_menu 
			(GTK_OPTION_MENU(mFormatChooser));
		GtkWidget *selected = gtk_menu_get_active (GTK_MENU(menu));

		gint i(0);
		for (GList *iterator = GTK_MENU_SHELL(menu)->children ;
		     iterator ; iterator = iterator->next, i++)
		{
			if (iterator->data == selected) 
			{
				mSelectedFileFormat = i;
				break;
			}
		}
	}

	if (GTK_IS_TOGGLE_BUTTON(mSaveContentCheck))
		*retval = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mSaveContentCheck)) ?
			  returnOKSaveContent : returnOK;
	else
		*retval = returnOK;

	return NS_OK;
}

//------------------------------------------------------------------------------

NS_DEF_FACTORY (GFilePicker, GFilePicker);

/**
 * NS_NewFilePickerFactory:
 */ 
nsresult NS_NewFilePickerFactory(nsIFactory** aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
	*aFactory = nsnull;

	nsGFilePickerFactory *result = new nsGFilePickerFactory;
	if (result == NULL)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
    
	NS_ADDREF(result);
	*aFactory = result;

	return NS_OK;
}
