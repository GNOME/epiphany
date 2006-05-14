/*
 *  Copyright (C) 2004 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <gtkmozembed.h>
#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsIFile.h>
#include <nsIIOService.h>
#include <nsIPrintSettings.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsIWebBrowserChrome.h>
#include <nsIWindowWatcher.h>
#include <nsServiceManagerUtils.h>
#include <nsStringAPI.h>
#include <nsXPCOM.h>

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-file-helpers.h"
#include "print-dialog.h"

#include "EphyUtils.h"

nsresult
EphyUtils::GetIOService (nsIIOService **ioService)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> mgr; 
	NS_GetServiceManager (getter_AddRefs (mgr));
	if (!mgr) return NS_ERROR_FAILURE;

	rv = mgr->GetServiceByContractID ("@mozilla.org/network/io-service;1",
					  NS_GET_IID (nsIIOService),
					  (void **)ioService);
	return rv;
}

nsresult
EphyUtils::NewURI (nsIURI **result,
		   const nsAString &spec,
		   const char *charset,
		   nsIURI *baseURI)
{
	nsCString cSpec;
	NS_UTF16ToCString (spec, NS_CSTRING_ENCODING_UTF8, cSpec);

	return NewURI (result, cSpec, charset, baseURI);
}

nsresult
EphyUtils::NewURI (nsIURI **result,
		   const nsACString &spec,
		   const char *charset,
		   nsIURI *baseURI)
{
	nsresult rv;
	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	return ioService->NewURI (spec, charset, baseURI, result);
}

nsresult
EphyUtils::NewFileURI (nsIURI **result,
		       nsIFile *spec)
{
	nsresult rv;
	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	return ioService->NewFileURI (spec, result);
}

GtkWidget *
EphyUtils::FindEmbed (nsIDOMWindow *aDOMWindow)
{
	if (!aDOMWindow) return nsnull;

	nsCOMPtr<nsIWindowWatcher> wwatch
		(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	NS_ENSURE_TRUE (wwatch, nsnull);

	/* this DOM window may belong to some inner frame, we need
	 * to get the topmost DOM window to get the embed
	 */
	nsCOMPtr<nsIDOMWindow> topWindow;
	aDOMWindow->GetTop (getter_AddRefs (topWindow));
	if (!topWindow) return nsnull;
	
	nsCOMPtr<nsIWebBrowserChrome> windowChrome;
	wwatch->GetChromeForWindow (topWindow, getter_AddRefs(windowChrome));
	NS_ENSURE_TRUE (windowChrome, nsnull);

	nsCOMPtr<nsIEmbeddingSiteWindow> window (do_QueryInterface(windowChrome));
	NS_ENSURE_TRUE (window, nsnull);

	nsresult rv;
	GtkWidget *mozembed;
	rv = window->GetSiteWindow ((void **)&mozembed);
	NS_ENSURE_SUCCESS (rv, nsnull);

	return mozembed;
}

GtkWidget *
EphyUtils::FindGtkParent (nsIDOMWindow *aDOMWindow)
{
	GtkWidget *embed = FindEmbed (aDOMWindow);
	NS_ENSURE_TRUE (embed, nsnull);

	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (embed));
	if (!GTK_WIDGET_TOPLEVEL (toplevel)) return nsnull;

	return toplevel;
}

nsresult
EphyUtils::CollatePrintSettings (EmbedPrintInfo *info,
				 nsIPrintSettings *options,
				 gboolean preview)
{
	NS_ENSURE_ARG (options);

	/* FIXME: for CUPS printers, print directly instead of to a tmp file? */
	const static PRUnichar pName[] = { 'P', 'o', 's', 't', 'S', 'c', 'r', 'i', 'p', 't', '/', 'd', 'e', 'f', 'a', 'u', 'l', 't', '\0' };
	options->SetPrinterName(nsString(pName).get());

	const static int frame_types[] = {
		nsIPrintSettings::kFramesAsIs,
		nsIPrintSettings::kSelectedFrame,
		nsIPrintSettings::kEachFrameSep
	};

	switch (info->range)
	{
	case GNOME_PRINT_RANGE_CURRENT:
	case GNOME_PRINT_RANGE_SELECTION_UNSENSITIVE:
	case GNOME_PRINT_RANGE_ALL:
		options->SetPrintRange (nsIPrintSettings::kRangeAllPages);
		break;
	case GNOME_PRINT_RANGE_RANGE:
		options->SetPrintRange (nsIPrintSettings::kRangeSpecifiedPageRange);
		options->SetStartPageRange (info->from_page);
		options->SetEndPageRange (info->to_page);
		break;
	case GNOME_PRINT_RANGE_SELECTION:
		options->SetPrintRange (nsIPrintSettings::kRangeSelection);
		break;
	}
	
	const GnomePrintUnit *unit, *inch, *mm;
	double value;

	mm = gnome_print_unit_get_by_abbreviation ((const guchar *) "mm");
	inch = gnome_print_unit_get_by_abbreviation ((const guchar *) "in");
	g_assert (mm != NULL && inch != NULL);

	/* top margin */
	if (gnome_print_config_get_length (info->config,
					   (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_TOP,
					   &value, &unit)
	    && gnome_print_convert_distance (&value, unit, inch))
	{
		options->SetMarginTop (value);
	}

	/* bottom margin */
	if (gnome_print_config_get_length (info->config,
					   (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_BOTTOM,
					   &value, &unit)
	    && gnome_print_convert_distance (&value, unit, inch))
	{
		options->SetMarginBottom (value);
	}

	/* left margin */
	if (gnome_print_config_get_length (info->config,
					   (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_LEFT,
					   &value, &unit)
	    && gnome_print_convert_distance (&value, unit, inch))
	{
		options->SetMarginLeft (value);
	}

	/* right margin */
	if (gnome_print_config_get_length (info->config,
					   (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_RIGHT,
					   &value, &unit)
	    && gnome_print_convert_distance (&value, unit, inch))
	{
		options->SetMarginRight (value);
	}


	nsString tmp;

	NS_CStringToUTF16 (nsCString(info->header_left_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetHeaderStrLeft (tmp.get());

	NS_CStringToUTF16 (nsCString(info->header_center_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetHeaderStrCenter (tmp.get());

	NS_CStringToUTF16 (nsCString(info->header_right_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetHeaderStrRight (tmp.get());

	NS_CStringToUTF16 (nsCString(info->footer_left_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetFooterStrLeft (tmp.get());

	NS_CStringToUTF16 (nsCString(info->footer_center_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetFooterStrCenter(tmp.get());

	NS_CStringToUTF16 (nsCString(info->footer_right_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetFooterStrRight(tmp.get());

	options->SetPrintToFile (PR_FALSE);
	
	if (preview)
	{
		options->SetPrintToFile (PR_FALSE);
	}
	else
	{
		char *base;
		const char *temp_dir;
		
		temp_dir = ephy_file_tmp_dir ();
		base = g_build_filename (temp_dir, "printXXXXXX", NULL);
		info->tempfile = ephy_file_tmp_filename (base, "ps");
		g_free (base);
		
		if (info->tempfile == NULL) return NS_ERROR_FAILURE;
		
		NS_CStringToUTF16 (nsCString(info->tempfile),
				   NS_CSTRING_ENCODING_UTF8, tmp);
		options->SetPrintToFile (PR_TRUE);
		options->SetToFileName (tmp.get());
	}

	/* paper size */
	options->SetPaperSize (nsIPrintSettings::kPaperSizeDefined);
	options->SetPaperSizeUnit (nsIPrintSettings::kPaperSizeMillimeters);

	if (gnome_print_config_get_length (info->config,
					   (const guchar *) GNOME_PRINT_KEY_PAPER_WIDTH,
					   &value, &unit)
	   && gnome_print_convert_distance (&value, unit, mm))
	{
		options->SetPaperWidth (value);	
	}

	if (gnome_print_config_get_length (info->config,
					   (const guchar *) GNOME_PRINT_KEY_PAPER_HEIGHT,
					   &value, &unit)
	   && gnome_print_convert_distance (&value, unit, mm))
	{
		options->SetPaperHeight (value);	
	}

	/* Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=307404
	 * means that we cannot actually use any paper sizes except mozilla's
	 * builtin list, and we must refer to them *by name*!
	 */
#ifndef HAVE_GECKO_1_9
	/* Gnome-Print names some papers differently than what moz understands */
	static const struct
	{
		const char *gppaper;
		const char *mozpaper;
	}
	paper_table [] =
	{
		{ "USLetter", "Letter" },
		{ "USLegal", "Legal" }
	};
#endif /* !HAVE_GECKO_1_9 */

	/* paper name */
	char *string = (char *) gnome_print_config_get (info->config,
						  (const guchar *) GNOME_PRINT_KEY_PAPER_SIZE);
	const char *paper = string;

#ifndef HAVE_GECKO_1_9
	for (PRUint32 i = 0; i < G_N_ELEMENTS (paper_table); i++)
	{
		if (string != NULL &&
		    g_ascii_strcasecmp (paper_table[i].gppaper, string) == 0)
		{
			paper = paper_table[i].mozpaper;
			break;
		}
	}
#endif /* !HAVE_GECKO_1_9 */

	NS_CStringToUTF16 (nsCString(paper),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetPaperName (tmp.get());
	g_free (string);

	/* paper orientation */
	string = (char *) gnome_print_config_get (info->config,
						  (const guchar *) GNOME_PRINT_KEY_ORIENTATION);
	if (string == NULL) string = g_strdup ("R0");

	if (strncmp (string, "R90", 3) == 0 || strncmp (string, "R270", 4) == 0)
	{
		options->SetOrientation (nsIPrintSettings::kLandscapeOrientation);
	}
	else
	{
		options->SetOrientation (nsIPrintSettings::kPortraitOrientation);
	}
	g_free (string);

	options->SetPrintInColor (info->print_color);
	options->SetPrintFrameType (frame_types[info->frame_type]);

	return NS_OK;
}
