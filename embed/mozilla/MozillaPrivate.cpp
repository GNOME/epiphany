#include "MozillaPrivate.h"

#include <nsIServiceManagerUtils.h>
#include <nsIWindowWatcher.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsIWebBrowserChrome.h>
#include <gtkmozembed.h>

GtkWidget *MozillaFindGtkParent (nsIDOMWindow *aDOMWindow)
{
        nsresult result;

        nsCOMPtr<nsIWindowWatcher> wwatch
                (do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
        if (!wwatch) return nsnull;

 	nsCOMPtr<nsIDOMWindow> domWindow(aDOMWindow);
	if (!domWindow)
	{
		result = wwatch->GetActiveWindow(getter_AddRefs(domWindow));
		if (NS_FAILED(result) || !domWindow) return nsnull;
	}
	
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

	return gtk_widget_get_toplevel (GTK_WIDGET(mozembed));
}


NS_METHOD MozillaCollatePrintSettings (const EmbedPrintInfo *info,
				       nsIPrintSettings *options)
{
	const static int frame_types[] = {
                nsIPrintSettings::kFramesAsIs,
                nsIPrintSettings::kSelectedFrame,
                nsIPrintSettings::kEachFrameSep
        };
	/* these should match the order of the radiobuttons in the dialog 
	 * and the paper names in the default print provider PS*/
	const static char *PaperSizeNames[] = {
		"Letter","Legal","Executive","A4"
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

        options->SetMarginTop (info->top_margin);
        options->SetMarginBottom (info->bottom_margin);
        options->SetMarginLeft (info->left_margin);
        options->SetMarginRight (info->right_margin);

        options->SetPrinterName(NS_LITERAL_STRING("PostScript/default").get());

        options->SetHeaderStrLeft(NS_ConvertUTF8toUCS2(info->header_left_string).get());

        options->SetHeaderStrCenter(NS_ConvertUTF8toUCS2(info->header_center_string).get());

        options->SetHeaderStrRight(NS_ConvertUTF8toUCS2(info->header_right_string).get());

        options->SetFooterStrLeft(NS_ConvertUTF8toUCS2(info->footer_left_string).get());

        options->SetFooterStrCenter(NS_ConvertUTF8toUCS2(info->footer_center_string).get());

        options->SetFooterStrRight(NS_ConvertUTF8toUCS2(info->footer_right_string).get());

        options->SetToFileName (NS_ConvertUTF8toUCS2(info->file).get());

	options->SetPrintCommand (NS_ConvertUTF8toUCS2(info->printer).get());

       	options->SetPrintToFile (info->print_to_file);

	/* native paper size formats. Our dialog does not support custom yet */
	options->SetPaperSize (nsIPrintSettings::kPaperSizeNativeData);
	int tps = (info->paper >= 0 || info->paper < 4) ? info->paper : 0;
	options->SetPaperName (NS_ConvertUTF8toUCS2(PaperSizeNames[tps]).get());

        options->SetPrintInColor (info->print_color);
        options->SetOrientation (info->orientation);
        options->SetPrintFrameType (frame_types[info->frame_type]);

	return NS_OK;
}
