/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "ephy-embed.h"
#include "mozilla-embed.h"

#include "MozillaPrivate.h"

#include <nsEmbedString.h>
#include <nsIServiceManagerUtils.h>
#include <nsIWindowWatcher.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsIWebBrowserChrome.h>
#include <gtkmozembed.h>

GtkWidget *MozillaFindEmbed (nsIDOMWindow *aDOMWindow)
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

        nsresult result;
        GtkWidget *mozembed;
        result = window->GetSiteWindow ((void **)&mozembed);
	NS_ENSURE_SUCCESS (result, nsnull);

	return mozembed;
}

GtkWidget *MozillaFindGtkParent (nsIDOMWindow *aDOMWindow)
{
	GtkWidget *embed = MozillaFindEmbed (aDOMWindow);
	NS_ENSURE_TRUE (embed, nsnull);

	return gtk_widget_get_toplevel (GTK_WIDGET (embed));
}

#define MM_TO_INCH(x)		(((double) x) / 25.4)

NS_METHOD MozillaCollatePrintSettings (const EmbedPrintInfo *info,
					  nsIPrintSettings *options)
{
	const static int frame_types[] = {
                nsIPrintSettings::kFramesAsIs,
                nsIPrintSettings::kSelectedFrame,
                nsIPrintSettings::kEachFrameSep
        };


        switch (info->pages)
        {
        case 0:
		options->SetPrintRange (nsIPrintSettings::kRangeAllPages);
                break;
        case 1:
                options->SetPrintRange (nsIPrintSettings::kRangeSpecifiedPageRange);
                options->SetStartPageRange (info->from_page);
                options->SetEndPageRange (info->to_page);
                break;
        case 2:
                options->SetPrintRange (nsIPrintSettings::kRangeSelection);
                break;
        }

        options->SetMarginTop (MM_TO_INCH (info->top_margin));
        options->SetMarginBottom (MM_TO_INCH (info->bottom_margin));
        options->SetMarginLeft (MM_TO_INCH (info->left_margin));
        options->SetMarginRight (MM_TO_INCH (info->right_margin));

	PRUnichar postscript[] = { 'P', 'o', 's', 't', 's', 'c', 'r', 'i',
				   'p', 't', '/', 'd', 'e', 'f', 'a', 'u',
				   'l', 't', '\0' };
        options->SetPrinterName(postscript);

	nsEmbedString tmp;

	NS_CStringToUTF16 (nsEmbedCString(info->header_left_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetHeaderStrLeft (tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->header_center_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetHeaderStrCenter (tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->header_right_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetHeaderStrRight (tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->footer_left_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetFooterStrLeft (tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->footer_center_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetFooterStrCenter(tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->footer_right_string),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetFooterStrRight(tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->file),
			   NS_CSTRING_ENCODING_UTF8, tmp);
        options->SetToFileName (tmp.get());

	NS_CStringToUTF16 (nsEmbedCString(info->printer),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetPrintCommand (tmp.get());

	/**
	 * Work around a mozilla bug where paper size & orientation are ignored
	 * and the specified file is created (containing invalid postscript)
	 * in print preview mode if we set "print to file" to true.
	 * See epiphany bug #119818.
	 */
	if (info->preview)
	{
		options->SetPrintToFile (PR_FALSE);
	}
	else
	{
		options->SetPrintToFile (info->print_to_file);
	}

	/* native paper size formats. Our dialog does not support custom yet */
	options->SetPaperSize (nsIPrintSettings::kPaperSizeNativeData);

	NS_CStringToUTF16 (nsEmbedCString(info->paper),
			   NS_CSTRING_ENCODING_UTF8, tmp);
	options->SetPaperName (tmp.get());

        options->SetPrintInColor (info->print_color);
        options->SetOrientation (info->orientation);
        options->SetPrintFrameType (frame_types[info->frame_type]);

	return NS_OK;
}
