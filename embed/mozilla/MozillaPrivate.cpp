#include "MozillaPrivate.h"

#include <nsIServiceManagerUtils.h>
#include <nsIWindowWatcher.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsIWebBrowserChrome.h>
#include <gtkmozembed.h>

#include "ephy-embed.h"
#include "mozilla-embed.h"

GtkWidget *MozillaFindEmbed (nsIDOMWindow *aDOMWindow)
{
        nsresult result;

        nsCOMPtr<nsIWindowWatcher> wwatch
                (do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
        if (!wwatch) return nsnull;

 	nsCOMPtr<nsIDOMWindow> domWindow;
	if (aDOMWindow)
	{
		/* this DOM window may belong to some inner frame, we need
		 * to get the topmost DOM window to get the embed
		 */
		aDOMWindow->GetTop (getter_AddRefs (domWindow));
	}
	else
	{
		/* get the active window */
		wwatch->GetActiveWindow (getter_AddRefs(domWindow));
	}
	NS_ENSURE_TRUE (domWindow, nsnull);
	
        nsCOMPtr<nsIWebBrowserChrome> windowChrome;
        result = wwatch->GetChromeForWindow (domWindow,
                                             getter_AddRefs(windowChrome));
        if (NS_FAILED(result)) return nsnull;

        nsCOMPtr<nsIEmbeddingSiteWindow> window
                        (do_QueryInterface(windowChrome, &result));
        if (NS_FAILED(result)) return nsnull;

        GtkWidget *mozembed;
        result = window->GetSiteWindow ((void **)&mozembed);
        if (NS_FAILED(result)) return nsnull;

	return mozembed;
}

GtkWidget *MozillaFindGtkParent (nsIDOMWindow *aDOMWindow)
{
	GtkWidget *embed = MozillaFindEmbed (aDOMWindow);
	if (!embed) return nsnull;

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

        options->SetPrinterName(NS_LITERAL_STRING("PostScript/default").get());

        options->SetHeaderStrLeft(NS_ConvertUTF8toUCS2(info->header_left_string).get());

        options->SetHeaderStrCenter(NS_ConvertUTF8toUCS2(info->header_center_string).get());

        options->SetHeaderStrRight(NS_ConvertUTF8toUCS2(info->header_right_string).get());

        options->SetFooterStrLeft(NS_ConvertUTF8toUCS2(info->footer_left_string).get());

        options->SetFooterStrCenter(NS_ConvertUTF8toUCS2(info->footer_center_string).get());

        options->SetFooterStrRight(NS_ConvertUTF8toUCS2(info->footer_right_string).get());

        options->SetToFileName (NS_ConvertUTF8toUCS2(info->file).get());

	options->SetPrintCommand (NS_ConvertUTF8toUCS2(info->printer).get());

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
	options->SetPaperName (NS_ConvertUTF8toUCS2(info->paper).get());

        options->SetPrintInColor (info->print_color);
        options->SetOrientation (info->orientation);
        options->SetPrintFrameType (frame_types[info->frame_type]);

	return NS_OK;
}
