/*
 *  Copyright (C) 2001 Philip Langdale
 *  Copyright (C) 2003 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "FilePicker.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"

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
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "MozillaPrivate.h"

#include "ephy-string.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <glib/gconvert.h>
#include <gtk/gtkfilefilter.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include <glib/gi18n.h>

NS_IMPL_ISUPPORTS1(GFilePicker, nsIFilePicker)

GFilePicker::GFilePicker()
{
	LOG ("GFilePicker ctor (%p)", this)

	mDialog = EPHY_FILE_CHOOSER (g_object_new (EPHY_TYPE_FILE_CHOOSER, NULL));

	g_object_add_weak_pointer (G_OBJECT (mDialog), (gpointer *) &mDialog);

	ephy_file_chooser_set_persist_key (mDialog, CONF_STATE_UPLOAD_DIR);

	mMode = nsIFilePicker::modeOpen;
}

GFilePicker::~GFilePicker()
{
	LOG ("GFilePicker dtor (%p)", this)

	if (mDialog)
	{
		g_object_remove_weak_pointer (G_OBJECT (mDialog), (gpointer *) &mDialog);
		gtk_widget_destroy (GTK_WIDGET (mDialog));
	}
}

/* void init (in nsIDOMWindowInternal parent, in wstring title, in short mode); */
NS_IMETHODIMP GFilePicker::Init(nsIDOMWindowInternal *parent, const PRUnichar *title, PRInt16 mode)
{
	LOG ("GFilePicker::Init")

	nsCOMPtr<nsIDOMWindow> dw = do_QueryInterface (parent);
	if (dw)
	{
		GtkWidget *pwin = MozillaFindGtkParent (dw);

		gtk_window_set_transient_for (GTK_WINDOW (mDialog), GTK_WINDOW (pwin));
	}

	gtk_window_set_title (GTK_WINDOW (mDialog), NS_ConvertUTF16toUTF8 (title).get());

	mMode = mode;

	switch (mode)
	{
		case nsIFilePicker::modeGetFolder:
			gtk_file_chooser_set_action (GTK_FILE_CHOOSER (mDialog),
						     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
			
			gtk_dialog_add_buttons (GTK_DIALOG (mDialog),
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						NULL);
			gtk_dialog_set_default_response (GTK_DIALOG (mDialog), GTK_RESPONSE_ACCEPT);
			break;

		case nsIFilePicker::modeOpenMultiple:
			gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (mDialog), TRUE);
			/* fallthrough */			
		case nsIFilePicker::modeOpen:
			gtk_file_chooser_set_action (GTK_FILE_CHOOSER (mDialog),
						     GTK_FILE_CHOOSER_ACTION_OPEN);

			gtk_dialog_add_buttons (GTK_DIALOG (mDialog),
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						NULL);
			gtk_dialog_set_default_response (GTK_DIALOG (mDialog), GTK_RESPONSE_ACCEPT);

			break;

		case nsIFilePicker::modeSave:
			gtk_file_chooser_set_action (GTK_FILE_CHOOSER (mDialog),
						     GTK_FILE_CHOOSER_ACTION_SAVE);
	
			gtk_dialog_add_buttons (GTK_DIALOG (mDialog),
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						NULL);
			gtk_dialog_set_default_response (GTK_DIALOG (mDialog), GTK_RESPONSE_ACCEPT);
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	return NS_OK;
}

/* void appendFilters (in long filterMask); */
NS_IMETHODIMP GFilePicker::AppendFilters(PRInt32 filterMask)
{
	// http://lxr.mozilla.org/seamonkey/source/xpfe/components/filepicker/res/locale/en-US/filepicker.properties
	// http://lxr.mozilla.org/seamonkey/source/xpfe/components/filepicker/src/nsFilePicker.js line 131 ff

	// FIXME: use filters with mimetypes instead of extensions

	LOG ("GFilePicker::AppendFilters mask=%d", filterMask)

	if (filterMask & nsIFilePicker::filterAll)
	{
		AppendFilter (NS_ConvertUTF8toUTF16 (_("All files")).get(),
			      NS_LITERAL_STRING ("*").get());
	}
	if (filterMask & nsIFilePicker::filterHTML)
	{
		AppendFilter (NS_ConvertUTF8toUTF16 (_("HTML files")).get(),
			      NS_LITERAL_STRING ("*.html; *.htm; *.shtml; *.xhtml").get());
	}
	if (filterMask & nsIFilePicker::filterText)
	{
		AppendFilter (NS_ConvertUTF8toUTF16 (_("Text files")).get(),
			      NS_LITERAL_STRING ("*.txt; *.text").get());
	}
	if (filterMask & nsIFilePicker::filterImages)
	{
		AppendFilter (NS_ConvertUTF8toUTF16 (_("Image files")).get(),
			      NS_LITERAL_STRING ("*.png; *.gif; *.jpeg; *.jpg").get());
	}
	if (filterMask & nsIFilePicker::filterXML)
	{
		AppendFilter (NS_ConvertUTF8toUTF16 (_("XML files")).get(),
			      NS_LITERAL_STRING ("*.xml").get());
	}
	if (filterMask & nsIFilePicker::filterXUL)
	{
		AppendFilter (NS_ConvertUTF8toUTF16 (_("XUL files")).get(),
			      NS_LITERAL_STRING ("*.xul").get());
	}

	return NS_OK;
}

/* void appendFilter (in wstring title, in wstring filter); */
NS_IMETHODIMP GFilePicker::AppendFilter(const PRUnichar *title, const PRUnichar *filter)
{
	LOG ("GFilePicker::AppendFilter title '%s' for '%s'",
	     NS_ConvertUTF16toUTF8 (title).get(),
	     NS_ConvertUTF16toUTF8 (filter).get())

	NS_ConvertUTF16toUTF8 pattern(filter);
	pattern.StripWhitespace();
	if (pattern.IsEmpty()) return NS_ERROR_FAILURE;

	char **patterns = g_strsplit (pattern.get(), ";", -1);

	GtkFileFilter *filth = gtk_file_filter_new ();

	for (int i = 0; patterns[i] != NULL; i++)
	{
		gtk_file_filter_add_pattern (filth, patterns[i]);
	}

	gtk_file_filter_set_name (filth, NS_ConvertUTF16toUTF8(title).get());
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (mDialog), filth);

	g_strfreev (patterns);

	return NS_OK;
}

/* attribute wstring defaultString; */
NS_IMETHODIMP GFilePicker::GetDefaultString(PRUnichar **aDefaultString)
{
	char *filename, *converted;

	LOG ("GFilePicker::GetDefaultString")

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mDialog));
	if (filename != NULL)
	{
		converted = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

		*aDefaultString = ToNewUnicode (NS_ConvertUTF8toUTF16 (converted));
	
		g_free (filename);
		g_free (converted);
	}

	return NS_OK;
}

NS_IMETHODIMP GFilePicker::SetDefaultString(const PRUnichar *aDefaultString)
{
	LOG ("GFilePicker::SetDefaultString to %s",
	     NS_ConvertUTF16toUTF8 (aDefaultString).get())

	if (aDefaultString)
	{
		/* set_current_name takes UTF-8, not a filename */
		gtk_file_chooser_set_current_name
			(GTK_FILE_CHOOSER (mDialog),
			 NS_ConvertUTF16toUTF8 (aDefaultString).get());
	}

	return NS_OK;
}

/* attribute wstring defaultExtension; */
NS_IMETHODIMP GFilePicker::GetDefaultExtension(PRUnichar **aDefaultExtension)
{
	LOG ("GFilePicker::GetDefaultExtension")

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GFilePicker::SetDefaultExtension(const PRUnichar *aDefaultExtension)
{
	LOG ("GFilePicker::SetDefaultExtension to %s",
	     NS_ConvertUTF16toUTF8(aDefaultExtension).get())

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long filterIndex; */
NS_IMETHODIMP GFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
	LOG ("GFilePicker::GetFilterIndex")

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
	LOG ("GFilePicker::SetFilterIndex index=%d", aFilterIndex)

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILocalFile displayDirectory; */
NS_IMETHODIMP GFilePicker::GetDisplayDirectory(nsILocalFile **aDisplayDirectory)
{
	char *dir;

	LOG ("GFilePicker::GetDisplayDirectory")

	dir = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (mDialog));

	if (dir != NULL)
	{
		nsCOMPtr<nsILocalFile> file = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
		file->InitWithNativePath (nsDependentCString (dir));
		NS_IF_ADDREF (*aDisplayDirectory = file);
	
		g_free (dir);
	}

	return NS_OK;
}

NS_IMETHODIMP GFilePicker::SetDisplayDirectory(nsILocalFile *aDisplayDirectory)
{
	nsCAutoString dir;
	aDisplayDirectory->GetNativePath (dir);

	LOG ("GFilePicker::SetDisplayDirectory to %s", dir.get())

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (mDialog),
					     dir.get());

	return NS_OK;
}

/* readonly attribute nsILocalFile file; */
NS_IMETHODIMP GFilePicker::GetFile(nsILocalFile **aFile)
{
	char *filename;

	LOG ("GFilePicker::GetFile")

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mDialog));

	if (filename != NULL)
	{
		nsCOMPtr<nsILocalFile> file = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
		file->InitWithNativePath (nsDependentCString (filename));
		NS_IF_ADDREF (*aFile = file);
	
		g_free (filename);
	}

	return NS_OK;
}

/* readonly attribute nsIFileURL fileURL; */
NS_IMETHODIMP GFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
	LOG ("GFilePicker::GetFileURL")

	nsCOMPtr<nsILocalFile> file;
	GetFile (getter_AddRefs(file));
	NS_ENSURE_TRUE (file, NS_ERROR_FAILURE);

	nsCOMPtr<nsIFileURL> fileURL = do_CreateInstance (NS_STANDARDURL_CONTRACTID);
	fileURL->SetFile(file);
	NS_IF_ADDREF(*aFileURL = fileURL);

	return NS_OK;
}

/* readonly attribute nsISimpleEnumerator files; */
NS_IMETHODIMP GFilePicker::GetFiles(nsISimpleEnumerator * *aFiles)
{
	// Not sure if we need to implement it at all, it's used nowhere
	// in mozilla, but I guess a javascript might call it?

	LOG ("GFilePicker::GetFiles")

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* short show (); */
NS_IMETHODIMP GFilePicker::Show(PRInt16 *_retval)
{
	LOG ("GFilePicker::Show")

	gtk_window_set_modal (GTK_WINDOW (mDialog), TRUE);

	gtk_widget_show (GTK_WIDGET (mDialog));

	int response = gtk_dialog_run (GTK_DIALOG (mDialog));

	LOG ("GFilePicker::Show response=%d", response)

	switch (response)
	{
		case GTK_RESPONSE_ACCEPT:
			*_retval = nsIFilePicker::returnOK;
			break;
		default:
			*_retval = nsIFilePicker::returnCancel;
			break;
	}

	gtk_widget_hide (GTK_WIDGET (mDialog));
	
	char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mDialog));

	if (filename == NULL)
	{
		*_retval = nsIFilePicker::returnCancel;
	}
	else if (mMode == nsIFilePicker::modeSave
		 && ephy_gui_confirm_overwrite_file (GTK_WIDGET (mDialog), filename) == FALSE)
	{
		*_retval = nsIFilePicker::returnCancel;
	}

	g_free (filename);

	return NS_OK;
}
