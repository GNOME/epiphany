/*
 *  Copyright (C) 2001 Philip Langdale
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include "mozilla-config.h"

#include "config.h"

#include "FilePicker.h"
#include "EphyUtils.h"
#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"

#include <nsCOMPtr.h>
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsIFileURL.h>
#include <nsILocalFile.h>
#include <nsIPromptService.h>
#include <nsIDOMWindow.h>
#include <nsNetCID.h>

#ifndef MOZ_NSIFILEPICKER_NSASTRING_
#include <nsIDOMWindowInternal.h>
#endif

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
: mDialog(nsnull)
, mMode(nsIFilePicker::modeOpen)
{
	LOG ("GFilePicker ctor (%p)", this);
}

GFilePicker::~GFilePicker()
{
	LOG ("GFilePicker dtor (%p)", this);

	if (mDialog)
	{
		g_object_remove_weak_pointer (G_OBJECT (mDialog), (gpointer *) &mDialog);
		gtk_widget_destroy (GTK_WIDGET (mDialog));
	}
}

/* void init (in nsIDOMWindow parent, in AString title, in short mode); */
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
NS_IMETHODIMP GFilePicker::Init(nsIDOMWindow *parent, const nsAString& title, PRInt16 mode)
#else
NS_IMETHODIMP GFilePicker::Init(nsIDOMWindowInternal *parent, const PRUnichar *title, PRInt16 mode)
#endif
{
	LOG ("GFilePicker::Init");

	mParent = do_QueryInterface (parent);

	GtkWidget *gtkparent = EphyUtils::FindGtkParent (parent);
#if defined(MOZ_NSIFILEPICKER_NSASTRING_)
	NS_ENSURE_TRUE (gtkparent, NS_ERROR_FAILURE);
#endif

	nsEmbedCString cTitle;
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
	NS_UTF16ToCString (title, NS_CSTRING_ENCODING_UTF8, cTitle);
#else
	NS_UTF16ToCString (nsEmbedString(title), NS_CSTRING_ENCODING_UTF8, cTitle);
#endif

	mMode = mode;

	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	switch (mode)
	{
		case nsIFilePicker::modeGetFolder:
			action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
			break;
		case nsIFilePicker::modeOpenMultiple:
		case nsIFilePicker::modeOpen:
			action = GTK_FILE_CHOOSER_ACTION_OPEN;
			break;
		case nsIFilePicker::modeSave:
			action = GTK_FILE_CHOOSER_ACTION_SAVE;
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	mDialog = ephy_file_chooser_new (cTitle.get(), gtkparent, action,
					 CONF_STATE_UPLOAD_DIR,
					 EPHY_FILE_FILTER_NONE);

	if (parent)
	{
		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (gtkparent)),
					     GTK_WINDOW (mDialog));
	}

	if (mode == nsIFilePicker::modeOpenMultiple)
	{
		gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (mDialog), TRUE);
	}
	if (mMode == nsIFilePicker::modeSave)
	{
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (mDialog), TRUE);
	}

	g_object_add_weak_pointer (G_OBJECT (mDialog), (gpointer *) &mDialog);

	return NS_OK;
}

/* void appendFilters (in long filterMask); */
NS_IMETHODIMP GFilePicker::AppendFilters(PRInt32 filterMask)
{
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	LOG ("GFilePicker::AppendFilters mask=%d", filterMask);

	// http://lxr.mozilla.org/seamonkey/source/xpfe/components/filepicker/res/locale/en-US/filepicker.properties
	// http://lxr.mozilla.org/seamonkey/source/xpfe/components/filepicker/src/nsFilePicker.js line 131 ff

	if (filterMask & nsIFilePicker::filterAll)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("All files"),
						      "*", (char*) NULL);
	}
	if (filterMask & nsIFilePicker::filterHTML)
	{
		ephy_file_chooser_add_mime_filter (mDialog, _("Web pages"),
						   "text/html",
						   "application/xhtml+xml",
						   "text/xml",
						   (char *) NULL);
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
						   (char *) NULL);
	}
	if (filterMask & nsIFilePicker::filterXML)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("XML files"),
						      "*.xml", (char *) NULL);
	}
	if (filterMask & nsIFilePicker::filterXUL)
	{
		ephy_file_chooser_add_pattern_filter (mDialog, _("XUL files"),
						      "*.xul", (char *) NULL);
	}

	return NS_OK;
}

/* void appendFilter (in AString title, in AString filter); */
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
NS_IMETHODIMP GFilePicker::AppendFilter(const nsAString& title, const nsAString& filter)
#else
NS_IMETHODIMP GFilePicker::AppendFilter(const PRUnichar *title, const PRUnichar *filter)
#endif
{
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	LOG ("GFilePicker::AppendFilter");

#ifdef MOZ_NSIFILEPICKER_NSASTRING_
	if (!filter.Length()) return NS_ERROR_FAILURE;
#else
	if (!filter) return NS_ERROR_FAILURE;
#endif

	nsEmbedCString pattern;
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
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
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
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
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
NS_IMETHODIMP GFilePicker::GetDefaultString(nsAString& aDefaultString)
#else
NS_IMETHODIMP GFilePicker::GetDefaultString(PRUnichar **aDefaultString)
#endif
{
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	LOG ("GFilePicker::GetDefaultString");

#ifdef MOZ_NSIFILEPICKER_NSASTRING_
	aDefaultString = mDefaultString;
#else
	*aDefaultString = NS_StringCloneData (mDefaultString);
#endif

	return NS_OK;
}

#ifdef MOZ_NSIFILEPICKER_NSASTRING_
NS_IMETHODIMP GFilePicker::SetDefaultString(const nsAString& aDefaultString)
#else
NS_IMETHODIMP GFilePicker::SetDefaultString(const PRUnichar *aDefaultString)
#endif
{
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	mDefaultString.Assign (aDefaultString);

	if (mMode == nsIFilePicker::modeSave)
	{
		nsEmbedCString defaultString;
		NS_UTF16ToCString (mDefaultString, NS_CSTRING_ENCODING_UTF8,
				   defaultString);

		LOG ("GFilePicker::SetDefaultString %s", defaultString.get());

		if (!defaultString.Length()) return NS_ERROR_FAILURE;

		/* set_current_name takes UTF-8, not a filename */
		gtk_file_chooser_set_current_name
			(GTK_FILE_CHOOSER (mDialog), defaultString.get());
	}

	return NS_OK;
}

/* attribute AString defaultExtension; */
#ifdef MOZ_NSIFILEPICKER_NSASTRING_
NS_IMETHODIMP GFilePicker::GetDefaultExtension(nsAString& aDefaultExtension)
#else
NS_IMETHODIMP GFilePicker::GetDefaultExtension(PRUnichar **aDefaultExtension)
#endif
{
	LOG ("GFilePicker::GetDefaultExtension");

	return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef MOZ_NSIFILEPICKER_NSASTRING_
NS_IMETHODIMP GFilePicker::SetDefaultExtension(const nsAString& aDefaultExtension)
#else
NS_IMETHODIMP GFilePicker::SetDefaultExtension(const PRUnichar *aDefaultExtension)
#endif
{
	LOG ("GFilePicker::SetDefaultExtension");

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long filterIndex; */
NS_IMETHODIMP GFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
	LOG ("GFilePicker::GetFilterIndex");

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
	LOG ("GFilePicker::SetFilterIndex index=%d", aFilterIndex);

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILocalFile displayDirectory; */
NS_IMETHODIMP GFilePicker::GetDisplayDirectory(nsILocalFile **aDisplayDirectory)
{
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	LOG ("GFilePicker::GetDisplayDirectory");

	char *dir = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (mDialog));

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
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	nsEmbedCString dir;
	aDisplayDirectory->GetNativePath (dir);

	LOG ("GFilePicker::SetDisplayDirectory to %s", dir.get());

	if (mDefaultString.Length() && mMode != nsIFilePicker::modeSave)
	{
		nsEmbedCString defaultString;
		NS_UTF16ToCString (mDefaultString, NS_CSTRING_ENCODING_NATIVE_FILESYSTEM,
				   defaultString);

		char *filename = g_build_filename (dir.get(), defaultString.get(), (char *) NULL);
		LOG ("Setting filename to %s", filename);
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (mDialog), filename);
		g_free (filename);
	}
	else
	{
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (mDialog),
						     dir.get());
	}

	return NS_OK;
}

/* readonly attribute nsILocalFile file; */
NS_IMETHODIMP GFilePicker::GetFile(nsILocalFile **aFile)
{
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	char *filename;

	LOG ("GFilePicker::GetFile");

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
	NS_ENSURE_TRUE (mDialog, NS_ERROR_FAILURE);

	LOG ("GFilePicker::GetFileURL");

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

	LOG ("GFilePicker::GetFiles");

	return NS_ERROR_NOT_IMPLEMENTED;
}

/* short show (); */
NS_IMETHODIMP GFilePicker::Show(PRInt16 *_retval)
{
	nsresult rv;
	AutoJSContextStack stack;
	rv = stack.Init ();
	if (NS_FAILED (rv)) return rv;

	AutoWindowModalState (mParent);
	mParent = nsnull;

	LOG ("GFilePicker::Show");

	gtk_window_set_modal (GTK_WINDOW (mDialog), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (mDialog), FALSE);

	/* If there's just the "ALL" filter, it's no use showing the filters! */
	GSList *filters = gtk_file_chooser_list_filters (GTK_FILE_CHOOSER (mDialog));
	if (g_slist_length (filters) == 1)
	{
		GtkFileFilter *filter = GTK_FILE_FILTER (filters->data);
		const char *name = gtk_file_filter_get_name (filter);

		if (!name || strcmp (name, _("All files")) == 0)
		{
			gtk_file_chooser_remove_filter (GTK_FILE_CHOOSER (mDialog),
							filter);
		}
	}
	g_slist_free (filters);

	gtk_widget_show (GTK_WIDGET (mDialog));

	int response;
	char *filename = NULL;

	do
	{
		response = gtk_dialog_run (GTK_DIALOG (mDialog));

		g_free (filename);
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (mDialog));

		LOG ("GFilePicker::Show response=%d, filename=%s", response, filename);
	}
	while (response == GTK_RESPONSE_ACCEPT &&
	       mMode == nsIFilePicker::modeSave &&
	       !ephy_gui_check_location_writable (GTK_WIDGET (mDialog), filename));

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
