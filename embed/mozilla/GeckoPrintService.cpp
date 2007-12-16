/*
 *  Copyright © 2006, 2007 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprintunixdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwindow.h>
#include <glade/glade-xml.h>

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIDOMWindowInternal.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-stock-icons.h"

#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"
#include "EphyUtils.h"
#include "GeckoPrintSession.h"

#include "GeckoPrintService.h"

/* Some printing keys */

#define CONF_PRINT_BG_COLORS		"/apps/epiphany/dialogs/print_background_colors"
#define CONF_PRINT_BG_IMAGES		"/apps/epiphany/dialogs/print_background_images"
#define CONF_PRINT_COLOR                "/apps/epiphany/dialogs/print_color"
#define CONF_PRINT_DATE                 "/apps/epiphany/dialogs/print_date"
#define CONF_PRINT_PAGE_NUMBERS         "/apps/epiphany/dialogs/print_page_numbers"
#define CONF_PRINT_PAGE_TITLE           "/apps/epiphany/dialogs/print_page_title"
#define CONF_PRINT_PAGE_URL             "/apps/epiphany/dialogs/print_page_url"

#define LITERAL(s) reinterpret_cast<const nsAString::char_type*>(NS_L(s))

/* From nsIDeviceContext.h */
#define NS_ERROR_GFX_PRINTER_BASE (1) /* adjustable :-) */
#define NS_ERROR_GFX_PRINTER_ACCESS_DENIED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+5)
#define NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND         \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+4)

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

  /* Print settings changes disallowed, just translate the settings */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP)) {
    return PrintUnattended (aParent, aSettings);
  }

  /* Not locked down, show the dialogue */

  nsresult rv;
#if 0
  PRBool haveSelection = PR_FALSE;
  rv = aSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &haveSelection);
  NS_ENSURE_SUCCESS (rv, rv);
#endif

  PRInt16 frameUI = nsIPrintSettings::kFrameEnableAll;
  rv = aSettings->GetHowToEnableFrameUI (&frameUI);
  NS_ENSURE_SUCCESS (rv, rv);

  GtkWidget *parent = EphyUtils::FindGtkParent (aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_INVALID_POINTER);

  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) {
    return rv;
  }

  AutoWindowModalState modalState (aParent);

  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  GladeXML *xml = glade_xml_new (ephy_file ("print.glade"),
				 "print_dialog_custom_tab", NULL);
  if (!xml) {
    return NS_ERROR_FAILURE;
  }

  /* Build the custom tab */
  GtkWidget *custom_tab = glade_xml_get_widget (xml, "custom_tab_container");
  ephy_gui_connect_checkbutton_to_gconf (glade_xml_get_widget (xml, "print_bg_colors_checkbutton"), CONF_PRINT_BG_COLORS);
  ephy_gui_connect_checkbutton_to_gconf (glade_xml_get_widget (xml, "print_bg_images_checkbutton"), CONF_PRINT_BG_IMAGES);
  ephy_gui_connect_checkbutton_to_gconf (glade_xml_get_widget (xml, "print_date_checkbutton"), CONF_PRINT_DATE);
  ephy_gui_connect_checkbutton_to_gconf (glade_xml_get_widget (xml, "print_page_numbers_checkbutton"), CONF_PRINT_PAGE_NUMBERS);
  ephy_gui_connect_checkbutton_to_gconf (glade_xml_get_widget (xml, "print_page_title_checkbutton"), CONF_PRINT_PAGE_TITLE);
  ephy_gui_connect_checkbutton_to_gconf (glade_xml_get_widget (xml, "print_page_url_checkbutton"), CONF_PRINT_PAGE_URL);

  GtkWidget *frame_box = glade_xml_get_widget (xml, "frame_box");
  GtkWidget *print_frames_normal = glade_xml_get_widget (xml, "print_frames_normal");
  GtkWidget *print_frames_selected = glade_xml_get_widget (xml, "print_frames_selected");
  GtkWidget *print_frames_separately = glade_xml_get_widget (xml, "print_frames_separately");

  /* FIXME: store/load from pref */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (print_frames_normal), TRUE);

  if (frameUI == nsIPrintSettings::kFrameEnableAll) {
    /* Allow all frame options */
    gtk_widget_set_sensitive (frame_box, TRUE);
  } else if (frameUI == nsIPrintSettings::kFrameEnableAsIsAndEach) {
    /* Allow all except "selected frame" */
    gtk_widget_set_sensitive (frame_box, TRUE);
    gtk_widget_set_sensitive (print_frames_selected, FALSE);
    /* Preselect this one, since the default above only prints _one page_ ! */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (print_frames_separately), TRUE);
  }

  /* FIXME: this sucks! find some way to do all of this async! */
  GtkWidget *dialog = gtk_print_unix_dialog_new (NULL /* FIXME title */,
		  				 GTK_WINDOW (parent));
  GtkPrintUnixDialog *print_dialog = GTK_PRINT_UNIX_DIALOG (dialog);

  GtkPrintCapabilities capabilities =
	 GtkPrintCapabilities (GTK_PRINT_CAPABILITY_PAGE_SET |
			       GTK_PRINT_CAPABILITY_COPIES   |
			       GTK_PRINT_CAPABILITY_COLLATE  |
			       GTK_PRINT_CAPABILITY_REVERSE  |
			       GTK_PRINT_CAPABILITY_SCALE |
			       GTK_PRINT_CAPABILITY_GENERATE_PS);
#ifdef HAVE_GECKO_1_9
  capabilities = GtkPrintCapabilities (capabilities | GTK_PRINT_CAPABILITY_GENERATE_PDF);
#endif
  gtk_print_unix_dialog_set_manual_capabilities	(print_dialog, capabilities);

  gtk_print_unix_dialog_set_page_setup (print_dialog,
					ephy_embed_shell_get_page_setup (shell));
  gtk_print_unix_dialog_set_settings (print_dialog,
				      ephy_embed_shell_get_print_settings (shell));

  /* Remove custom tab from its dummy window and put it in the print dialogue */
  g_object_ref_sink (custom_tab);
  gtk_container_remove (GTK_CONTAINER (custom_tab->parent), custom_tab);
  gtk_print_unix_dialog_add_custom_tab (print_dialog, custom_tab,
					gtk_label_new (_("Options"))); /* FIXME better name! */
  g_object_unref (custom_tab);
  g_object_unref (xml);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

  int response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  GtkPrinter *printer = gtk_print_unix_dialog_get_selected_printer (print_dialog);

  if (response != GTK_RESPONSE_OK || !printer) {
    gtk_widget_destroy (dialog);

    return NS_ERROR_ABORT;
  }

  PRInt16 printFrames = nsIPrintSettings::kNoFrames;
  if (frameUI != nsIPrintSettings::kFrameEnableNone) {
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (print_frames_normal))) {
      printFrames = nsIPrintSettings::kFramesAsIs;
    } else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (print_frames_selected))) {
      printFrames = nsIPrintSettings::kSelectedFrame;
    } if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (print_frames_separately))) {
      printFrames = nsIPrintSettings::kEachFrameSep;
    }
  }

  GtkPageSetup *pageSetup = gtk_print_unix_dialog_get_page_setup (print_dialog); /* no reference owned */
  ephy_embed_shell_set_page_setup (shell, pageSetup);

  GtkPrintSettings *settings = gtk_print_unix_dialog_get_settings (print_dialog);
  ephy_embed_shell_set_print_settings (shell, settings);

  /* We copy the setup and settings so we can modify them to unset
   * options handled by gecko.
   */
  GtkPageSetup *pageSetupCopy = gtk_page_setup_copy (pageSetup);
  pageSetup = pageSetupCopy;

  GtkPrintSettings *settingsCopy = gtk_print_settings_copy (settings);
  g_object_unref (settings);
  settings = settingsCopy;

  rv = session->SetSettings (aSettings, settings, pageSetup, printer);

  /* Now translate the settings to nsIPrintSettings */
  if (NS_SUCCEEDED (rv)) {
    nsCString sourceFile;
    session->GetSourceFile (sourceFile);
    if (!sourceFile.IsEmpty ()) {
      rv = TranslateSettings (settings, pageSetup, printer, sourceFile, printFrames, PR_TRUE, aSettings);
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }

  gtk_widget_destroy (dialog);

  g_object_unref (settings);
  g_object_unref (pageSetup);

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
  
  /* Our print session implements those interfaces */
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
  /* This function is never called from gecko code */
#if 0
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

  AutoWindowModalState modalState (aParent);

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
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
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

/* Private methods */

#if 0
typedef struct
{
  GMainLoop *mainLoop;
  GtkPrinter *mPrinter;
  guint timeout;
  int response;
  guint cancelled : 1;
} FindPrinterData;

static void
FreeFindPrinterData (FindPrinterData *data)
{
  if (data->printer) {
    g_object_unref (data->printer);
  }
}

static void
DialogResponseCallback (GtkWidget *aDialog,
		        int aResponse,
			FindPrinterData *data)
{
  data->response = aResponse;
  g_main_loop_quit (data->mainloop);
}

static void
TimeoutCallback (FindPrinterData *data)
{
  data->cancelled = TRUE;
  g_main_loop_quit (data->mainLoop);
  data->mainLoop = NULL;
  return FALSE;
}

static gboolean
PrinterEnumerateCallback (GtkPrinter *aPrinter,
			  FindPrinterData *data)
{
  if (data->cancelled)
    return TRUE;

  if ((data->printerName &&
       strcmp (data->printerName, gtk_printer_get_name (aPrinter)) == 0) ||
      (!data->printerName &&
       gtk_printer_is_default (aPrinter))) {
    data->printer = g_object_ref (aPrinter);
    return TRUE;
  }

  return FALSE;
}
#endif

nsresult
GeckoPrintService::PrintUnattended (nsIDOMWindow *aParent,
				    nsIPrintSettings *aPrintSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
#if 0
  GtkWidget *parent = EphyUtils::FindGtkParent (aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_INVALID_POINTER);

  PRBool isCalledFromScript = EphyJSUtils::IsCalledFromScript ();

  nsresult rv;
  AutoJSContextStack stack;
  rv = stack.Init ();
  if (NS_FAILED (rv)) {
    return rv;
  }

  AutoWindowModalState modalState (aParent);

  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GtkPrintSettings *settings = ephy_embed_shell_get_print_settings (shell);
  NS_ENSURE_TRUE (settings, NS_ERROR_FAILURE);

  const char *printer = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PRINTER);
#if 0
  if (!printer || !printer[0]) {
    return NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;
  }
#endif
  /* We need to find the printer, so we need to run a mainloop.
   * If called from a script, give the user a way to cancel the print;
   * otherwise we'll just show a generic progress message.
   */
  GtkWidget *dialog;
  if (isCalledFromScript) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
				     GtkDialogFlags (0),
				     GTK_MESSAGE_QUESTION,
				     GTK_BUTTONS_CANCEL,
				     "%s", _("Print this page?"));
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_PRINT,
			   GTK_RESPONSE_ACCEPT);
  } else {
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
				     GtkDialogFlags (0),
				     GTK_MESSAGE_QUESTION,
				     GTK_BUTTONS_CANCEL,
				     "%s", _("Preparing to print"));
  }
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  FindPrinterData *data = g_new0 (PrinterData, 1);
  data->dialog = dialog;

  g_signal_connect (dialog, "response",
		    G_CALLBACK (DialogResponseCallback), data);

  /* Don't run forever */
  data->timeoutId = g_timeout_add (PRINTER_ENUMERATE_TIMEOUT,
				   (GSourceFunc) EnumerateTimoutCallback,
				   data);
  /* Enumerate printers until we find our printer */
  gtk_enumerate_printers ((GtkPrinterFunc) PrinterEnumerateCallback,
			  data,
			  (GDestroyNotify) EnumerateDestroyCallback, FALSE);

  /* Now run the mainloop */
  int response = gtk_dialog_run (GTK_DIALOG (dialog));
  Printer
  gtk_widget_destroy (dialog);

  if (response != GTK_RESPONSE_ACCEPT) {
    return NS_ERROR_ABORT;
  }

    nsCString sourceFile;
    session->GetSourceFile (sourceFile);
    if (!sourceFile.IsEmpty ()) {
      rv = TranslateSettings (settings,
			      ephy_embed_shell_get_page_setup (shell),
			      sourceFile, PR_TRUE, aSettings);
    } else {
      rv = NS_ERROR_FAILURE;
    }

    return rv;
  }
#endif /* if 0 */
}

/* Static methods */

/* static */ nsresult
GeckoPrintService::TranslateSettings (GtkPrintSettings *aGtkSettings,
				      GtkPageSetup *aPageSetup,
				      GtkPrinter *aPrinter,
				      const nsACString &aSourceFile,
                                      PRInt16 aPrintFrames,
				      PRBool aIsForPrinting,
				      nsIPrintSettings *aSettings)
{
  NS_ENSURE_ARG (aPrinter);
  NS_ENSURE_ARG (aGtkSettings);
  NS_ENSURE_ARG (aPageSetup);

#if 0
  /* Locked down? */
  if (gtk_print_settings_get_print_to_file (aGtkSettings) &&
      eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK)) {
    return NS_ERROR_GFX_PRINTER_ACCESS_DENIED;
  }
#endif

  GtkPrintCapabilities capabilities = gtk_printer_get_capabilities (aPrinter);

  /* Initialisation */
  aSettings->SetIsInitializedFromPrinter (PR_FALSE); /* FIXME: PR_TRUE? */
  aSettings->SetIsInitializedFromPrefs (PR_FALSE); /* FIXME: PR_TRUE? */
  aSettings->SetPrintSilent (PR_FALSE);
  aSettings->SetShowPrintProgress (PR_TRUE);

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
#ifdef HAVE_GECKO_1_9
    NS_ENSURE_TRUE (aPrinter, NS_ERROR_FAILURE);

    const char *format = gtk_print_settings_get (aGtkSettings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
    if (!format)
      format = "ps";

    if (strcmp (format, "pdf") == 0 &&
	gtk_printer_accepts_pdf (aPrinter)) {
      aSettings->SetOutputFormat (nsIPrintSettings::kOutputFormatPDF);
    } else if (strcmp (format, "ps") == 0 &&
	       gtk_printer_accepts_ps (aPrinter)) {
      aSettings->SetOutputFormat (nsIPrintSettings::kOutputFormatPS);
    } else {
      g_warning ("Output format '%s' specified, but printer '%s' does not support it!",
		 format, gtk_printer_get_name (aPrinter));
      return NS_ERROR_FAILURE;
    }
#endif
	  
    int n_copies = gtk_print_settings_get_n_copies (aGtkSettings);
    if (n_copies <= 0)
      return NS_ERROR_FAILURE;
    if (capabilities & GTK_PRINT_CAPABILITY_COPIES) {
      aSettings->SetNumCopies (1);
    } else {
      /* We have to copy them ourself */
      aSettings->SetNumCopies (n_copies);
      gtk_print_settings_set_n_copies (aGtkSettings, 1);
    }

    gboolean reverse = gtk_print_settings_get_reverse (aGtkSettings);
    if (capabilities & GTK_PRINT_CAPABILITY_REVERSE) {
      aSettings->SetPrintReversed (PR_FALSE);
    } else {
      aSettings->SetPrintReversed (reverse);
      gtk_print_settings_set_reverse (aGtkSettings, FALSE);
    }

    GtkPageSet pageSet = gtk_print_settings_get_page_set (aGtkSettings);
    aSettings->SetPrintOptions (nsIPrintSettings::kPrintEvenPages,
			        pageSet != GTK_PAGE_SET_ODD);
    aSettings->SetPrintOptions (nsIPrintSettings::kPrintEvenPages,
			        pageSet != GTK_PAGE_SET_EVEN);

    GtkPrintPages printPages = gtk_print_settings_get_print_pages (aGtkSettings);
    switch (printPages) {
      case GTK_PRINT_PAGES_RANGES: {
        int numRanges = 0;
        GtkPageRange *pageRanges = gtk_print_settings_get_page_ranges (aGtkSettings, &numRanges);
        if (numRanges > 0) {
          /* FIXME: We can only support one range, ignore more ranges or raise error? */
          aSettings->SetPrintRange (nsIPrintSettings::kRangeSpecifiedPageRange);
	  /* Gecko page numbers start at 1, while gtk page numbers start at 0 */
          aSettings->SetStartPageRange (pageRanges[0].start + 1);
	  aSettings->SetEndPageRange (pageRanges[0].end + 1);

	  g_free (pageRanges);
          break;
        }
	/* Fall-through to PAGES_ALL */
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

  /* And clear those in the settings, so the printer doesn't try to apply them too */
  gtk_print_settings_set_print_pages (aGtkSettings, GTK_PRINT_PAGES_ALL);
  gtk_print_settings_set_page_ranges (aGtkSettings, NULL, 0);
  gtk_print_settings_set_page_set (aGtkSettings, GTK_PAGE_SET_ALL);

  /* We must use the page setup here instead of the print settings, see gtk bug #485685 */
  switch (gtk_page_setup_get_orientation (aPageSetup)) {
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

  /* Sucky mozilla wants margins in inch! */
  aSettings->SetMarginTop (gtk_page_setup_get_top_margin (aPageSetup, GTK_UNIT_INCH));
  aSettings->SetMarginBottom (gtk_page_setup_get_bottom_margin (aPageSetup, GTK_UNIT_INCH));
  aSettings->SetMarginLeft (gtk_page_setup_get_left_margin (aPageSetup, GTK_UNIT_INCH));
  aSettings->SetMarginRight (gtk_page_setup_get_right_margin (aPageSetup, GTK_UNIT_INCH));

  aSettings->SetHeaderStrLeft (eel_gconf_get_boolean (CONF_PRINT_PAGE_TITLE) ? LITERAL ("&T") : LITERAL (""));
  aSettings->SetHeaderStrCenter (LITERAL (""));
  aSettings->SetHeaderStrRight (eel_gconf_get_boolean (CONF_PRINT_PAGE_URL) ? LITERAL ("&U") : LITERAL (""));
  aSettings->SetFooterStrLeft (eel_gconf_get_boolean (CONF_PRINT_PAGE_NUMBERS) ? LITERAL ("&PT") : LITERAL (""));
  aSettings->SetFooterStrCenter (LITERAL (""));
  aSettings->SetFooterStrRight (eel_gconf_get_boolean (CONF_PRINT_DATE) ? LITERAL ("&D") : LITERAL (""));

  aSettings->SetPrintFrameType (aPrintFrames);
  aSettings->SetPrintFrameTypeUsage (nsIPrintSettings::kUseSettingWhenPossible);

  /* FIXME: only if GTK_PRINT_CAPABILITY_SCALE is not set? */
  aSettings->SetScaling (gtk_print_settings_get_scale (aGtkSettings) / 100.0);
  gtk_print_settings_set_scale (aGtkSettings, 1.0);

  aSettings->SetShrinkToFit (PR_FALSE); /* FIXME setting */
    
  aSettings->SetPrintBGColors (eel_gconf_get_boolean (CONF_PRINT_BG_COLORS) != FALSE);
  aSettings->SetPrintBGImages (eel_gconf_get_boolean (CONF_PRINT_BG_IMAGES) != FALSE);

  /* aSettings->SetPlexName (LITERAL ("default")); */
  /* aSettings->SetColorspace (LITERAL ("default")); */
  /* aSettings->SetResolutionName (LITERAL ("default")); */
  /* aSettings->SetDownloadFonts (PR_TRUE); */

  /* Unset those setting that we can handle, so they don't get applied
   * again for the print job.
   */
  /* gtk_print_settings_set_collate (aGtkSettings, FALSE); not yet */
  /* FIXME: Unset the orientation for the print job? */
  /* gtk_page_setup_set_orientation (aPageSetup, GTK_PAGE_ORIENTATION_PORTRAIT); */
  /* FIXME: unset output format -> "ps" ? */

  return NS_OK;
}
