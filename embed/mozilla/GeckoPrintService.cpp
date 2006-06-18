/*
 *  Copyright (C) 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkprintunixdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIDOMWindowInternal.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"

#include "AutoJSContextStack.h"
#include "EphyUtils.h"
#include "GeckoPrintSession.h"

#include "GeckoPrintService.h"

#define LITERAL(s) NS_REINTERPRET_CAST(const nsAString::char_type*, NS_L(s))

/* From nsIDeviceContext.h */
#define NS_ERROR_GFX_PRINTER_BASE (1) /* adjustable :-) */
#define NS_ERROR_GFX_PRINTER_ACCESS_DENIED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+5)

NS_IMPL_ISUPPORTS1 (GeckoPrintService,
		    nsIPrintingPromptService)

GeckoPrintService::GeckoPrintService()
{
  LOG ("GeckoPrintService ctor [%p]", this);
}

GeckoPrintService::~GeckoPrintService()
{
  LOG ("GeckoPrintService dtor [%p]", this);
}

/* nsIPrintingPromptService implementation */

/* void showPrintDialog (in nsIDOMWindow parent,
			 in nsIWebBrowserPrint webBrowserPrint,
			 in nsIPrintSettings printSettings); */
NS_IMETHODIMP
GeckoPrintService::ShowPrintDialog (nsIDOMWindow *aParent,
				    nsIWebBrowserPrint *aWebBrowserPrint,
				    nsIPrintSettings *aSettings)
{
  /* Locked down? */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINTING)) {
    return NS_ERROR_GFX_PRINTER_ACCESS_DENIED;
  }

  GeckoPrintSession *session = GeckoPrintSession::FromSettings (aSettings);
  NS_ENSURE_TRUE (session, NS_ERROR_INVALID_POINTER);

  GtkWidget *parent = EphyUtils::FindGtkParent (aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_INVALID_POINTER);

  PRBool isCalledFromScript = EphyJSUtils::IsCalledFromScript ();

  nsresult rv;
  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) {
    return rv;
  }

  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  /* Print settings changes disallowed, just translate the settings */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP)) {
    /* FIXME: we need to call session->SetSettings first, and for that we need a
     * way to get the default printer object!
     */
    g_warning ("Printing with locked print setup doesn't work!");
    return NS_ERROR_FAILURE;

    /* If called from a script, give the user a way to cancel the print! */
    if (isCalledFromScript) {
      GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
		    				  GtkDialogFlags (0),
						  GTK_MESSAGE_QUESTION,
						  GTK_BUTTONS_CANCEL,
						  "%s", _("Print this page?"));
      gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");
      gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_PRINT,
			     GTK_RESPONSE_ACCEPT);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

      int response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response != GTK_RESPONSE_ACCEPT) {
        return NS_ERROR_ABORT;
      }
    }

    nsCString sourceFile;
    session->GetSourceFile (sourceFile);
    if (!sourceFile.IsEmpty ()) {
      rv = TranslateSettings (ephy_embed_shell_get_print_settings (shell),
			      ephy_embed_shell_get_page_setup (shell),
			      sourceFile, PR_TRUE, aSettings);
    } else {
      rv = NS_ERROR_ABORT;
    }

    return rv;
  }

  /* Not locked down, show the dialogue */

#if 0
  PRBool haveSelection = PR_FALSE;
  rv = aSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &haveSelection);
  NS_ENSURE_SUCCESS (rv, rv);

  PRInt16 frameUI = 0;
  rv = aSettings->GetHowToEnableFrameUI (&frameUI);
  NS_ENSURE_SUCCESS (rv, rv);
#endif

  /* FIXME: this sucks! find some way to do all of this async! */
  GtkWidget *dialog = gtk_print_unix_dialog_new (NULL /* FIXME title */,
		  				 GTK_WINDOW (parent));
  GtkPrintUnixDialog *print_dialog = GTK_PRINT_UNIX_DIALOG (dialog);

  gtk_print_unix_dialog_set_manual_capabilities
	(print_dialog,
	 GtkPrintCapabilities (GTK_PRINT_CAPABILITY_PAGE_SET |
			       GTK_PRINT_CAPABILITY_COPIES   |
			       GTK_PRINT_CAPABILITY_COLLATE  |
			       GTK_PRINT_CAPABILITY_REVERSE  |
			       GTK_PRINT_CAPABILITY_SCALE));
  gtk_print_unix_dialog_set_page_setup (print_dialog,
					ephy_embed_shell_get_page_setup (shell));
  gtk_print_unix_dialog_set_settings (print_dialog,
				      ephy_embed_shell_get_print_settings (shell));

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

  int response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  GtkPrinter *printer = gtk_print_unix_dialog_get_selected_printer (print_dialog);

  if (response != GTK_RESPONSE_OK || !printer) {
    gtk_widget_destroy (dialog);

    return NS_ERROR_ABORT;
  }

  GtkPageSetup *pageSetup = gtk_print_unix_dialog_get_page_setup (print_dialog);
  ephy_embed_shell_set_page_setup (shell, pageSetup);

  GtkPrintSettings *settings = gtk_print_unix_dialog_get_settings (print_dialog);
  ephy_embed_shell_set_print_settings (shell, settings);

  /* This adopts the refcount of |settings| */
  rv = session->SetSettings (settings, pageSetup, printer);

  /* Now translate the settings to nsIPrintSettings */
  if (NS_SUCCEEDED (rv)) {
    nsCString sourceFile;
    session->GetSourceFile (sourceFile);
    if (!sourceFile.IsEmpty ()) {
      rv = TranslateSettings (settings, pageSetup, sourceFile, PR_TRUE, aSettings);
    } else {
      rv = NS_ERROR_ABORT;
    }
  }

  gtk_widget_destroy (dialog);

  return rv;
}

/* void showProgress (in nsIDOMWindow parent,
		      in nsIWebBrowserPrint webBrowserPrint,
		      in nsIPrintSettings printSettings,
		      in nsIObserver openDialogObserver,
		      in boolean isForPrinting,
		      out nsIWebProgressListener webProgressListener,
		      out nsIPrintProgressParams printProgressParams,
		      out boolean notifyOnOpen); */
NS_IMETHODIMP
GeckoPrintService::ShowProgress (nsIDOMWindow *aParent,
				 nsIWebBrowserPrint *aWebBrowserPrint,
				 nsIPrintSettings *aPrintSettings,
				 nsIObserver *aOpenDialogObserver,
				 PRBool aIsForPrinting,
				 nsIWebProgressListener **_webProgressListener,
				 nsIPrintProgressParams **_printProgressParams,
				 PRBool *_notifyOnOpen)
{
  /* Print preview */
  if (!aIsForPrinting) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> domWin (do_QueryInterface (aParent, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  nsCOMPtr<nsIPrintSession> session;
  rv = aPrintSettings->GetPrintSession (getter_AddRefs (session));
  NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && session, nsnull);

  nsCOMPtr<nsIPrintProgress> progress (do_QueryInterface (session, &rv));
  NS_ENSURE_SUCCESS (rv, rv);
  
  /* Out print session implements those interfaces */
  rv = CallQueryInterface (session, _webProgressListener);
  rv |= CallQueryInterface (session, _printProgressParams);
  NS_ENSURE_SUCCESS (rv, rv);

  /* Setting this to PR_FALSE will make gecko immediately start printing
   * when we return from this function.
   * If we set this to PR_TRUE, we need to call aOpenDialogObserver::Observe
   * (topic, subject and data don't matter) when we're ready for printing.
   */
  *_notifyOnOpen = PR_FALSE; 

  return progress->OpenProgressDialog (domWin, nsnull, nsnull, aOpenDialogObserver, _notifyOnOpen);
}

/* void showPageSetup (in nsIDOMWindow parent,
		       in nsIPrintSettings printSettings,
		       in nsIObserver aObs); */
NS_IMETHODIMP GeckoPrintService::ShowPageSetup (nsIDOMWindow *aParent,
						nsIPrintSettings *aPrintSettings,
						nsIObserver *aObserver)
{
  /* Locked down? */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINTING) ||
      eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP)) {
    return NS_ERROR_ABORT;
  }

  GtkWidget *parent = EphyUtils::FindGtkParent (aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_INVALID_POINTER);

  AutoJSContextStack stack;
  nsresult rv = stack.Init ();
  if (NS_FAILED (rv)) {
    return rv;
  }

  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GtkPageSetup *new_setup =
    gtk_print_run_page_setup_dialog (GTK_WINDOW (parent),
				     ephy_embed_shell_get_page_setup (shell),
				     ephy_embed_shell_get_print_settings (shell));
  if (new_setup) {
    ephy_embed_shell_set_page_setup (shell, new_setup);
    g_object_unref (new_setup);
  }

  /* FIXME do we need to notify aObserver somehow? */
  return NS_OK;
}

/* void showPrinterProperties (in nsIDOMWindow parent,
			       in wstring printerName,
			       in nsIPrintSettings printSettings); */
NS_IMETHODIMP
GeckoPrintService::ShowPrinterProperties (nsIDOMWindow *aParent,
					  const PRUnichar *aPrinterName,
					  nsIPrintSettings *aPrintSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* Static methods */

/* static */ nsresult
GeckoPrintService::TranslateSettings (GtkPrintSettings *aGtkSettings,
				      GtkPageSetup *aPageSetup,
				      const nsACString &aSourceFile,
				      PRBool aIsForPrinting,
				      nsIPrintSettings *aSettings)
{
  NS_ENSURE_ARG (aGtkSettings);
  NS_ENSURE_ARG (aPageSetup);

  /* Locked down? */
  if (gtk_print_settings_get_print_to_file (aGtkSettings) &&
      eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK)) {
    return NS_ERROR_GFX_PRINTER_ACCESS_DENIED;
  }

  /* Initialisation */
  aSettings->SetIsInitializedFromPrinter (PR_FALSE);
  aSettings->SetIsInitializedFromPrefs (PR_FALSE);
  aSettings->SetPrintSilent (PR_FALSE);
  aSettings->SetShowPrintProgress (PR_TRUE);
  aSettings->SetNumCopies (1);

  /* We always print PS to a file and then hand that off to gtk-print */
  aSettings->SetPrinterName (LITERAL ("PostScript/default"));

  if (aIsForPrinting) {
    aSettings->SetPrintToFile (PR_TRUE);

    nsString sourceFile;
    NS_CStringToUTF16 (aSourceFile,
		       NS_CSTRING_ENCODING_NATIVE_FILESYSTEM,
		       sourceFile);

    aSettings->SetToFileName (sourceFile.get ());
  } else {
    /* Otherwise mozilla will create the file nevertheless and 
     * fail since we haven't set a name!
     */
    aSettings->SetPrintToFile (PR_FALSE);
  }

  /* This is the time between printing each page, in ms.
   * It 'gives the user more time to press cancel' !
   * We don't want any of this nonsense, so set this to a low value,
   * just enough to update the print dialogue.
   */
  aSettings->SetPrintPageDelay (50);

  if (aIsForPrinting) {
    GtkPageSet pageSet = gtk_print_settings_get_page_set (aGtkSettings);
    aSettings->SetPrintOptions (nsIPrintSettings::kPrintEvenPages,
			        pageSet != GTK_PAGE_SET_ODD);
    aSettings->SetPrintOptions (nsIPrintSettings::kPrintEvenPages,
			        pageSet != GTK_PAGE_SET_EVEN);

    aSettings->SetPrintReversed (gtk_print_settings_get_reverse (aGtkSettings));

    GtkPrintPages printPages = gtk_print_settings_get_print_pages (aGtkSettings);
    switch (printPages) {
      case GTK_PRINT_PAGES_RANGES: {
        int numRanges = 0;
        GtkPageRange *pageRanges = gtk_print_settings_get_page_ranges (aGtkSettings, &numRanges);
        if (numRanges > 0) {
          /* FIXME: We can only support one range, ignore more ranges or raise error? */
          aSettings->SetPrintRange (nsIPrintSettings::kRangeSpecifiedPageRange);
          aSettings->SetStartPageRange (pageRanges[0].start);
	  aSettings->SetEndPageRange (pageRanges[1].end);

	  g_free (pageRanges);
        }
        break;
      }
      case GTK_PRINT_PAGES_CURRENT:
        /* not supported, fall through */
      case GTK_PRINT_PAGES_ALL:
        aSettings->SetPrintRange (nsIPrintSettings::kRangeAllPages);
        break;
        /* FIXME: we need some custom ranges here, "Selection" and "Focused Frame" */
    }
  } else {
    aSettings->SetPrintOptions (nsIPrintSettings::kPrintEvenPages, PR_TRUE);
    aSettings->SetPrintOptions (nsIPrintSettings::kPrintEvenPages, PR_TRUE);
    aSettings->SetPrintReversed (PR_FALSE);
    aSettings->SetPrintRange (nsIPrintSettings::kRangeAllPages);
  }

  switch (gtk_print_settings_get_orientation (aGtkSettings)) {
    case GTK_PAGE_ORIENTATION_PORTRAIT:
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT: /* not supported */
      aSettings->SetOrientation (nsIPrintSettings::kPortraitOrientation);
      break;
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE: /* not supported */
      aSettings->SetOrientation (nsIPrintSettings::kLandscapeOrientation);
      break;
  }

  aSettings->SetPrintInColor (gtk_print_settings_get_use_color (aGtkSettings));

  aSettings->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeMillimeters);
  aSettings->SetPaperSize (nsIPrintSettings::kPaperSizeDefined);

  // FIXME for some reason this is always NULL ??
  //  GtkPaperSize *paperSize = gtk_print_settings_get_paper_size (aGtkSettings); 
  GtkPaperSize *paperSize = gtk_page_setup_get_paper_size (aPageSetup);
  if (!paperSize) {
    return NS_ERROR_FAILURE;
  }

  aSettings->SetPaperSizeType (nsIPrintSettings::kPaperSizeDefined);
  aSettings->SetPaperWidth (gtk_paper_size_get_width (paperSize, GTK_UNIT_MM));
  aSettings->SetPaperHeight (gtk_paper_size_get_height (paperSize, GTK_UNIT_MM));

#ifdef HAVE_GECKO_1_9
  aSettings->SetPaperName (NS_ConvertUTF8toUTF16 (gtk_paper_size_get_name (paperSize)).get ());
#else
{
  /* Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=307404
   * means that we cannot actually use any paper sizes except mozilla's
   * builtin list, and we must refer to them *by name*!
  */
  static const struct {
    const char gtkPaperName[13];
    const char mozPaperName[10];
  } paperTable [] = {
    { GTK_PAPER_NAME_A5, "A5" },
    { GTK_PAPER_NAME_A4, "A4" },
    { GTK_PAPER_NAME_A3, "A3" },
    { GTK_PAPER_NAME_LETTER, "Letter" },
    { GTK_PAPER_NAME_LEGAL, "Legal" },
    { GTK_PAPER_NAME_EXECUTIVE, "Executive" },
  };

  const char *paperName = gtk_paper_size_get_name (paperSize);
	
  PRUint32 i;
  for (i = 0; i < G_N_ELEMENTS (paperTable); i++) {
    if (g_ascii_strcasecmp (paperTable[i].gtkPaperName, paperName) == 0) {
      paperName = paperTable[i].mozPaperName;
      break;
    }
  }
  if (i == G_N_ELEMENTS (paperTable)) {
    /* Not in table, fall back to A4 */
    g_warning ("Unknown paper name '%s', falling back to A4", gtk_paper_size_get_name (paperSize));
    paperName = paperTable[1].mozPaperName;
  }

  aSettings->SetPaperName (NS_ConvertUTF8toUTF16 (paperName).get ());
}
#endif /* !HAVE_GECKO_1_9 */

  // gtk_paper_size_free (paperSize);

  /* Sucky mozilla wants margins in inch! */
  aSettings->SetMarginTop (gtk_page_setup_get_top_margin (aPageSetup, GTK_UNIT_INCH));
  aSettings->SetMarginBottom (gtk_page_setup_get_bottom_margin (aPageSetup, GTK_UNIT_INCH));
  aSettings->SetMarginLeft (gtk_page_setup_get_left_margin (aPageSetup, GTK_UNIT_INCH));
  aSettings->SetMarginRight (gtk_page_setup_get_right_margin (aPageSetup, GTK_UNIT_INCH));

#if 0
  // FIXME !
  aSettings->SetHeaderStrLeft(const PRUnichar * aHeaderStrLeft);
  aSettings->SetHeaderStrCenter(const PRUnichar * aHeaderStrCenter);
  aSettings->SetHeaderStrRight(const PRUnichar * aHeaderStrRight);
  aSettings->SetFooterStrLeft(const PRUnichar * aFooterStrLeft);
  aSettings->SetFooterStrCenter(const PRUnichar * aFooterStrCenter);
  aSettings->SetFooterStrRight(const PRUnichar * aFooterStrRight);
#endif

  /* FIXME I think this is the right default, but this prevents the user
   * from cancelling the print immediately, see the stupid comment in nsPrintEngine:
   *  "DO NOT allow the print job to be cancelled if it is Print FrameAsIs
   *   because it is only printing one page."
   * We work around this by just not sending the job to the printer then.
   */
  aSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs); /* FIXME setting */
  aSettings->SetPrintFrameTypeUsage (nsIPrintSettings::kUseSettingWhenPossible);

  aSettings->SetScaling (gtk_print_settings_get_scale (aGtkSettings) / 100.0);
  aSettings->SetShrinkToFit (PR_FALSE); /* FIXME setting */
    
  aSettings->SetPrintBGColors (PR_FALSE); /* FIXME setting */
  aSettings->SetPrintBGImages (PR_FALSE); /* FIXME setting */

  /* aSettings->SetPlexName (LITERAL ("default")); */
  /* aSettings->SetColorspace (LITERAL ("default")); */
  /* aSettings->SetResolutionName (LITERAL ("default")); */
  /* aSettings->SetDownloadFonts (PR_TRUE); */

  return NS_OK;
}
