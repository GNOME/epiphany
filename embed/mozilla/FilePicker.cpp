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
#include "MozillaPrivate.h"

#include "mozilla-version.h"

#include <nsCOMPtr.h>
#include <nsEmbedString.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsIFileURL.h>
#include <nsILocalFile.h>
#include <nsIPromptService.h>
#include <nsIDOMWindow.h>
#include <nsNetCID.h>

#if !MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
#include <nsIDOMWindowInternal.h>
#endif

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

/* void init (in nsIDOMWindow parent, in AString title, in short mode); */
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
NS_IMETHODIMP GFilePicker::Init(nsIDOMWindow *parent, const nsAString& title, PRInt16 mode)
#else
NS_IMETHODIMP GFilePicker::Init(nsIDOMWindowInternal *parent, const PRUnichar *title, PRInt16 mode)
#endif
{
	LOG ("GFilePicker::Init")

	if (parent)
	{
		GtkWidget *pwin = MozillaFindGtkParent (parent);

		gtk_window_set_transient_for (GTK_WINDOW (mDialog), GTK_WINDOW (pwin));
	}

	nsEmbedCString cTitle;
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
	NS_UTF16ToCString (title, NS_CSTRING_ENCODING_UTF8, cTitle);
#else
	NS_UTF16ToCString (nsEmbedString(title), NS_CSTRING_ENCODING_UTF8, cTitle);
#endif
	gtk_window_set_title (GTK_WINDOW (mDialog), cTitle.get());

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

	LOG ("GFilePicker::AppendFilters mask=%d", filterMask)

	if (filterMask & nsIFilePicker::filterAll)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("All files"),
						      "*", NULL);
	}
	if (filterMask & nsIFilePicker::filterHTML)
	{
		ephy_file_chooser_add_mime_filter (mDialog, _("Web pages"),
						   "text/html",
						   "application/xhtml+xml",
						   "text/xml",
						   NULL);
	}
	if (filterMask & nsIFilePicker::filterText)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("Text files"),
						      "*.txt", "*.text", NULL);
	}
	if (filterMask & nsIFilePicker::filterImages)
	{
		ephy_file_chooser_add_mime_filter (mDialog, _("Images"),
						   "image/png",
						   "image/jpeg",
						   "image/gif",
						   NULL);
	}
	if (filterMask & nsIFilePicker::filterXML)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("XML files"),
						      "*.xml", NULL);
	}
	if (filterMask & nsIFilePicker::filterXUL)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("XUL files"),
						      "*.xul", NULL);
	}

	return NS_OK;
}

/* void appendFilter (in AString title, in AString filter); */
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
NS_IMETHODIMP GFilePicker::AppendFilter(const nsAString& title, const nsAString& filter)
#else
NS_IMETHODIMP GFilePicker::AppendFilter(const PRUnichar *title, const PRUnichar *filter)
#endif
{
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
	if (!filter.Length()) return NS_ERROR_FAILURE;
#else
	if (!filter) return NS_ERROR_FAILURE;
#endif

	nsEmbedCString pattern;
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
	NS_UTF16ToCString (filter, NS_CSTRING_ENCODING_UTF8, pattern);
#else
	NS_UTF16ToCString (nsEmbedString(filter), NS_CSTRING_ENCODING_UTF8, pattern);
#endif

	char **patterns;
	patterns = g_strsplit (pattern.get(), ";", -1);
	if (!patterns) return NS_ERROR_FAILURE;

	GtkFileFilter *filth;
	filth = gtk_file_filter_new ();

	for (int i = 0; patterns[i] != NULL; i++)
	{
		gtk_file_filter_add_pattern (filth, g_strstrip (patterns[i]));
	}

	nsEmbedCString cTitle;
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
	NS_UTF16ToCString (title, NS_CSTRING_ENCODING_UTF8, cTitle);
#else
	NS_UTF16ToCString (nsEmbedString(title), NS_CSTRING_ENCODING_UTF8, cTitle);
#endif

	gtk_file_filter_set_name (filth, cTitle.get());

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (mDialog), filth);

	g_strfreev (patterns);

	return NS_OK;
}

/* attribute AString defaultString; */
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
NS_IMETHODIMP GFilePicker::GetDefaultString(nsAString& aDefaultString)
#else
NS_IMETHODIMP GFilePicker::GetDefaultString(PRUnichar **aDefaultString)
#endif
{
	char *filename, *converted;

	LOG ("GFilePicker::GetDefaultString")

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mDialog));
	if (filename != NULL)
	{
		converted = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

		nsEmbedString defaultString;
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
		NS_CStringToUTF16 (nsEmbedCString(converted),
				   NS_CSTRING_ENCODING_UTF8, aDefaultString);
#else
		NS_CStringToUTF16 (nsEmbedCString(converted),
				   NS_CSTRING_ENCODING_UTF8, defaultString);
		*aDefaultString = NS_StringCloneData (defaultString);
#endif
	
		g_free (filename);
		g_free (converted);
	}

	return NS_OK;
}

#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
NS_IMETHODIMP GFilePicker::SetDefaultString(const nsAString& aDefaultString)
#else
NS_IMETHODIMP GFilePicker::SetDefaultString(const PRUnichar *aDefaultString)
#endif
{
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
	if (aDefaultString.Length())
#else
	if (aDefaultString)
#endif
	{
		nsEmbedCString defaultString;
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
		NS_UTF16ToCString (aDefaultString, NS_CSTRING_ENCODING_UTF8,
				   defaultString);
#else
		NS_UTF16ToCString (nsEmbedString(aDefaultString),
				   NS_CSTRING_ENCODING_UTF8, defaultString);
#endif
		/* set_current_name takes UTF-8, not a filename */
		gtk_file_chooser_set_current_name
			(GTK_FILE_CHOOSER (mDialog), defaultString.get());
	}

	return NS_OK;
}

/* attribute AString defaultExtension; */
#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
NS_IMETHODIMP GFilePicker::GetDefaultExtension(nsAString& aDefaultExtension)
#else
NS_IMETHODIMP GFilePicker::GetDefaultExtension(PRUnichar **aDefaultExtension)
#endif
{
	LOG ("GFilePicker::GetDefaultExtension")

	return NS_ERROR_NOT_IMPLEMENTED;
}

#if MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)
NS_IMETHODIMP GFilePicker::SetDefaultExtension(const nsAString& aDefaultExtension)
#else
NS_IMETHODIMP GFilePicker::SetDefaultExtension(const PRUnichar *aDefaultExtension)
#endif
{
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
		file->InitWithNativePath (nsEmbedCString (dir));
		NS_IF_ADDREF (*aDisplayDirectory = file);
	
		g_free (dir);
	}

	return NS_OK;
}

NS_IMETHODIMP GFilePicker::SetDisplayDirectory(nsILocalFile *aDisplayDirectory)
{
	nsEmbedCString dir;
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
		file->InitWithNativePath (nsEmbedCString (filename));
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

	int response;
	char *filename = NULL;

	do
	{
		response = gtk_dialog_run (GTK_DIALOG (mDialog));

		g_free (filename);
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mDialog));

		LOG ("GFilePicker::Show response=%d, filename=%s", response, filename)
	}
	while (response == GTK_RESPONSE_ACCEPT &&
	       mMode == nsIFilePicker::modeSave &&
	       !ephy_gui_confirm_overwrite_file (GTK_WIDGET (mDialog), filename));

	gtk_widget_hide (GTK_WIDGET (mDialog));

	if (response == GTK_RESPONSE_ACCEPT && filename != NULL)
	{
		*_retval = nsIFilePicker::returnOK;
	}
	else
	{
		*_retval = nsIFilePicker::returnCancel;
	}

	g_free (filename);

	return NS_OK;
}
