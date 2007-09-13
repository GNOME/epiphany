/*
 *  Copyright © 2006 Christian Persch
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

#include <xpcom-config.h>
#include "config.h"

#include <unistd.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nsStringAPI.h>

#include <nsIDOMWindow.h>
#include <nsIDOMWindowInternal.h>
#include <nsIPrintSettings.h>

#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"

#include "EphyUtils.h"

#include "GeckoPrintSession.h"

#define MAX_STRING_LENGTH 512

GeckoPrintSession::GeckoPrintSession ()
: mSettings(NULL)
, mPageSetup(NULL)
, mPrinter(NULL)
, mJob(NULL)
, mProgressDialog(NULL)
, mTitleLabel(NULL)
, mProgressBar(NULL)
, mStartPrintIdleID(0)
, mSourceFileIsTemp(PR_FALSE)
, mDone(PR_FALSE)
, mCancelled(PR_FALSE)
{
  LOG ("GeckoPrintSession ctor [%p]", (void*) this);

  /* FIXME: connect to "prepare-close" ? */
  g_object_ref (ephy_embed_shell_get_default ());
}

GeckoPrintSession::~GeckoPrintSession ()
{
  LOG ("GeckoPrintSession dtor [%p]", (void*) this);

  NS_ASSERTION (mStartPrintIdleID == 0, "Impossible");

  if (!mDone && !mCancelled) {
    Cancel ();
  }
  DestroyJob ();

  if (mSettings) {
    g_object_unref (mSettings);
  }
  if (mPageSetup) {
    g_object_unref (mPageSetup);
  }
  if (mPrinter) {
    g_object_unref (mPrinter);
  }
  if (mProgressDialog) {
    gtk_widget_destroy (mProgressDialog);
  }
  if (mSourceFileIsTemp) {
    unlink (mSourceFile.get ());
  }

  g_object_unref (ephy_embed_shell_get_default ());
}

void
GeckoPrintSession::GetSourceFile (nsACString &aSource)
{
  aSource.Assign (mSourceFile);
}

nsresult
GeckoPrintSession::SetSettings (nsIPrintSettings *aPrintSettings,
                                GtkPrintSettings *aSettings,
				GtkPageSetup *aPageSetup,
				GtkPrinter *aPrinter)
{
  NS_ASSERTION (!mPrintSettings && !mSettings && !mPageSetup && !mPrinter, "Already have settings!");

  NS_ENSURE_ARG (aPrintSettings);
  NS_ENSURE_ARG (aSettings);

  mPrintSettings = aPrintSettings;
  mSettings = (GtkPrintSettings *) g_object_ref (aSettings);

  NS_ENSURE_ARG (aPageSetup);
  NS_ENSURE_ARG (aPrinter);

  mPageSetup = (GtkPageSetup *) g_object_ref (aPageSetup);
  mPrinter = (GtkPrinter *) g_object_ref (aPrinter);

#if 0
  /* Compute the source file name */
  if (gtk_print_settings_get_print_to_file (mSettings)) {
    /* FIXME: support gnome-VFS uris here! */
    const char *fileURI = gtk_print_settings_get (aSettings, "export-uri");
    NS_ENSURE_TRUE (fileURI, NS_ERROR_FAILURE);

    char *fileName = g_filename_from_uri (fileURI, NULL, NULL);
    NS_ENSURE_TRUE (fileURI, NS_ERROR_FAILURE);

    mSourceFile.Assign (fileName);
    g_free (fileName);
  } else
#endif
  {
    char *base, *tmpName;

    /* FIXME: use pure glib here (g_mkstemp)! */
    base = g_build_filename (ephy_file_tmp_dir (), "print-XXXXXX", (const char *) NULL);
    tmpName = ephy_file_tmp_filename (base, "ps");
    g_free (base);

    NS_ENSURE_TRUE (tmpName, NS_ERROR_FAILURE);
    mSourceFile.Assign (tmpName);
    g_free (tmpName);

    mSourceFileIsTemp = PR_TRUE;
  }

  return NS_OK;
}

/* static methods */

/* static */ GeckoPrintSession *
GeckoPrintSession::FromSettings (nsIPrintSettings *aSettings)
{
  nsresult rv;
  nsCOMPtr<nsIPrintSession> session;
  rv = aSettings->GetPrintSession (getter_AddRefs (session));
  NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && session, nsnull);

  /* this is ok since the caller holds a ref to the settings which hold a ref to the session */
  nsIPrintSession *sessionPtr = session.get();
  return static_cast<GeckoPrintSession*>(sessionPtr);
}

/* static functions */

static void
ReleaseSession (GeckoPrintSession *aSession)
{
  NS_RELEASE (aSession);
}

static gboolean
ProgressDeleteCallback (GtkDialog *aDialog)
{
  gtk_dialog_response (aDialog, GTK_RESPONSE_DELETE_EVENT);
  return TRUE;
}

static void
ProgressResponseCallback (GtkDialog *aDialog,
		 	  int aResponse,
			  GeckoPrintSession *aSession)
{
  aSession->Cancel ();
}

static gboolean
StartPrintIdleCallback (GeckoPrintSession *aSession)
{
  aSession->StartPrinting ();

  return FALSE;
}

static void
JobStatusChangedCallback (GtkPrintJob *aJob,
			  GeckoPrintSession *aSession)
{
  aSession->JobStatusChanged ();
}

static void
JobCompletedCallback (GtkPrintJob *aJob,
		      GeckoPrintSession *aSession,
		      GError *aError)
{
  aSession->JobDone ();

  if (aError) {
    aSession->JobError (aError->message);
  }
}

/* Private methods */

void
GeckoPrintSession::SetProgress (PRInt32 aCurrent,
			        PRInt32 aMaximum)
{
  NS_ENSURE_TRUE (mProgressDialog, );

  if (mCancelled) return;

  /* Mozilla is weird */
  if (aCurrent > aMaximum || (aCurrent == 100 && aMaximum == 100)) return;

  double fraction = 0.0;
  if (aMaximum > 0 && aCurrent >= 0) {
    char *text = g_strdup_printf (_("Page %d of %d"), aCurrent, aMaximum);
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR (mProgressBar), text);
    g_free (text);

    fraction = (double) aCurrent / (double) aMaximum;
  }
	
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (mProgressBar), CLAMP (fraction, 0.0, 1.0));
}

void
GeckoPrintSession::SetProgressText (const char *aText)
{
  NS_ENSURE_TRUE (mProgressDialog, );

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (mProgressBar), aText);
}

void
GeckoPrintSession::Cancel ()
{
  SetProcessCanceledByUser (PR_TRUE);

  if (mProgressDialog) {
    gtk_dialog_set_response_sensitive (GTK_DIALOG (mProgressDialog),
				       GTK_RESPONSE_CANCEL, FALSE);

    SetProgress (0, 0);
    SetProgressText (_("Cancelling print")); /* FIXME text! */
  }

  if (mJob) {
    /* FIXME: There's no way to cancel mJob! Bug #339323 */
  }
}

void
GeckoPrintSession::StartPrinting ()
{
  mStartPrintIdleID = 0;

  GError *error = NULL;

#if 0
  /* FIXME: this could also be a print job to a file which was
   * printed to a temp file and now needs to be uploaded to its
   * final location with gnome-vfs.
   */
  if (gtk_print_settings_get_print_to_file (mSettings)) return;
#endif

  NS_ENSURE_TRUE (mSettings && mPageSetup && mPrinter, );

  mJob = gtk_print_job_new (mTitle.get (),
			    mPrinter,
			    mSettings,
			    mPageSetup);
  if (!gtk_print_job_set_source_file (mJob, mSourceFile.get (), &error)) {
    /* FIXME: error dialogue! */
    g_warning ("Couldn't set print job source: %s", error->message);
    g_error_free (error);

    g_object_unref (mJob);
    mJob = NULL;
  
    return;
  }

  g_signal_connect (mJob, "status-changed",
		    G_CALLBACK (JobStatusChangedCallback), this);

  /* Keep us alive until the job is done! */
  NS_ADDREF_THIS ();
  gtk_print_job_send (mJob,
		      (GtkPrintJobCompleteFunc) JobCompletedCallback,
		      this,
		      (GDestroyNotify) ReleaseSession);
}

void
GeckoPrintSession::JobStatusChanged ()
{
  NS_ENSURE_TRUE (mProgressDialog, );
	
  LOG ("print session %p status changed %d\n", this, gtk_print_job_get_status (mJob));

  /* FIXME: are any other status codes relevant info for the user? */
  if (gtk_print_job_get_status (mJob) == GTK_PRINT_STATUS_SENDING_DATA) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (mProgressBar), 0.75);
    /* FIXME text! */
    SetProgressText (_("Spooling..."));
  }
}

void
GeckoPrintSession::JobError (const char *aErrorMessage)
{
  LOG ("print job error: %s", aErrorMessage);

  /* FIXME better text */
  GtkWidget *dialog = gtk_message_dialog_new (NULL,
					      GtkDialogFlags (0),
					      GTK_MESSAGE_ERROR,
					      GTK_BUTTONS_OK,
					      _("Print error"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					    "%s", aErrorMessage);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_widget_show (dialog);
}

void
GeckoPrintSession::JobDone ()
{
  NS_ENSURE_TRUE (mProgressDialog, );

  mDone = PR_TRUE;

  gtk_widget_hide (mProgressDialog);

  DestroyJob ();
}

void
GeckoPrintSession::DestroyJob ()
{
  if (!mJob) return;

  g_signal_handlers_disconnect_by_func (mJob, (void*) JobStatusChangedCallback, this);
  g_object_unref (mJob);
  mJob = NULL;
}

void
GeckoPrintSession::LaunchJobOnIdle ()
{
  NS_ASSERTION (!mStartPrintIdleID, "Already started printing!");

  /* Don't send the job to the printer if the user cancelled the print */
  if (mCancelled) return;

  /* Keep us alive until the idle handler runs! */
  NS_ADDREF_THIS ();
  mStartPrintIdleID = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
				       (GSourceFunc) StartPrintIdleCallback,
				       this,
				       (GDestroyNotify) ReleaseSession);
}

/* XPCOM interfaces */

NS_IMPL_THREADSAFE_ISUPPORTS5 (GeckoPrintSession,
			       nsIPrintSession,
			       nsIWebProgressListener,
			       nsIPrintProgress,
			       nsIPrintProgressParams,
			       nsISupportsWeakReference)

/* nsIPrintSession implementation */

/* nsIWebProgressListener implementation */

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP
GeckoPrintSession::OnStateChange (nsIWebProgress *aWebProgress,
				  nsIRequest *aRequest,
				  PRUint32 aStateFlags,
				  nsresult aStatus)
{
  if (NS_SUCCEEDED (aStatus) &&
      aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) {
    if (aStateFlags & nsIWebProgressListener::STATE_START) {
      /* Printing starts now */
      SetProgress (0, 0);
    } else if ((aStateFlags & nsIWebProgressListener::STATE_STOP)) {
      /* Printing done, upload to printer */
      LaunchJobOnIdle ();
    }
  }

  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP
GeckoPrintSession::OnProgressChange (nsIWebProgress *aWebProgress,
				     nsIRequest *aRequest,
				     PRInt32 aCurSelfProgress,
				     PRInt32 aMaxSelfProgress,
				     PRInt32 aCurTotalProgress,
				     PRInt32 aMaxTotalProgress)
{
  SetProgress (aCurTotalProgress, aMaxTotalProgress);

  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI aLocation); */
NS_IMETHODIMP
GeckoPrintSession::OnLocationChange (nsIWebProgress *aWebProgress,
				     nsIRequest *aRequest,
				     nsIURI *aLocation)
{
  NS_ASSERTION (0, "OnLocationChange reached!");
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP
GeckoPrintSession::OnStatusChange (nsIWebProgress *aWebProgress,
				   nsIRequest *aRequest,
				   nsresult aStatus,
				   const PRUnichar *aMessage)
{
  NS_ASSERTION (0, "OnStatusChange reached!");
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aState); */
NS_IMETHODIMP
GeckoPrintSession::OnSecurityChange (nsIWebProgress *aWebProgress,
				     nsIRequest *aRequest,
				     PRUint32 aState)
{
  NS_ASSERTION (0, "OnSecurityChange reached!");
  return NS_OK;
}

/* nsIPrintProgress implementation */

/* void openProgressDialog (in nsIDOMWindowInternal parent, in string dialogURL, in nsISupports parameters, in nsIObserver openDialogObserver, out boolean notifyOnOpen); */
NS_IMETHODIMP
GeckoPrintSession::OpenProgressDialog (nsIDOMWindowInternal *aParent,
				       const char *aDialogURL,
				       nsISupports *aParameters,
				       nsIObserver *aOpenDialogObserver,
				       PRBool *_notifyOnOpen)
{
  NS_ENSURE_STATE (!mProgressDialog);

  nsCOMPtr<nsIDOMWindow> domWindow (do_QueryInterface (aParent));
  GtkWidget *parent = EphyUtils::FindGtkParent (domWindow);

  GtkWidget *vbox, *hbox, *image;

  mProgressDialog = gtk_dialog_new ();
  GtkDialog *dialog = GTK_DIALOG (mProgressDialog);

  gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  g_signal_connect (dialog, "delete-event",
		    G_CALLBACK (ProgressDeleteCallback), NULL);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (ProgressResponseCallback), this);

  /* FIXME do we need transient? initially on top should suffice */
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
				GTK_WINDOW (parent));

  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 14); /* 2 * 5 + 14 = 24 */
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 5);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_container_add (GTK_CONTAINER (dialog->vbox), hbox);

  image = gtk_image_new_from_stock (GTK_STOCK_PRINT, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  mTitleLabel = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (mTitleLabel), TRUE);
  gtk_misc_set_alignment (GTK_MISC (mTitleLabel), 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), mTitleLabel, FALSE, FALSE, 0);

  mProgressBar = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), mProgressBar, FALSE, FALSE, 0);

  gtk_widget_show_all (hbox);
  gtk_window_present (GTK_WINDOW (dialog));

  *_notifyOnOpen = PR_FALSE;

  return NS_OK;
}

/* void closeProgressDialog (in boolean forceClose); */
NS_IMETHODIMP
GeckoPrintSession::CloseProgressDialog (PRBool forceClose)
{
  return NS_OK;
}

/* void registerListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP
GeckoPrintSession::RegisterListener (nsIWebProgressListener *listener)
{
  return NS_OK;
}

/* void unregisterListener (in nsIWebProgressListener listener); */
NS_IMETHODIMP
GeckoPrintSession::UnregisterListener (nsIWebProgressListener *listener)
{
  return NS_OK;
}

/* void doneIniting (); */
NS_IMETHODIMP
GeckoPrintSession::DoneIniting()
{
  return NS_OK;
}

/* nsIPrompt getPrompter (); */
NS_IMETHODIMP
GeckoPrintSession::GetPrompter (nsIPrompt **_retval)
{
  g_return_val_if_reached (NS_ERROR_NOT_IMPLEMENTED);
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean processCanceledByUser; */
NS_IMETHODIMP
GeckoPrintSession::GetProcessCanceledByUser (PRBool *aProcessCanceledByUser)
{
  *aProcessCanceledByUser = mCancelled;
  return NS_OK;
}

NS_IMETHODIMP
GeckoPrintSession::SetProcessCanceledByUser (PRBool aProcessCanceledByUser)
{
  mCancelled = aProcessCanceledByUser;
  if (mPrintSettings) {
    mPrintSettings->SetIsCancelled (aProcessCanceledByUser);
  }

  return NS_OK;
}

/* nsIPrintProgressParams implementation */

/* attribute wstring docTitle; */
NS_IMETHODIMP
GeckoPrintSession::GetDocTitle (PRUnichar * *aDocTitle)
{
  g_return_val_if_reached (NS_ERROR_NOT_IMPLEMENTED);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoPrintSession::SetDocTitle (const PRUnichar * aDocTitle)
{
  NS_ENSURE_STATE (mProgressDialog);

  char *converted = EphyUtils::ConvertUTF16toUTF8 (aDocTitle, MAX_STRING_LENGTH);
  if (converted) {
    mTitle.Assign (converted);

    char *title = g_strdup_printf (_("Printing “%s”"), converted);
    gtk_window_set_title (GTK_WINDOW (mProgressDialog), title);
    gtk_label_set_text (GTK_LABEL (mTitleLabel), title);
    g_free (converted);
    g_free (title);
  }
  return NS_OK;
}

/* attribute wstring docURL; */
NS_IMETHODIMP
GeckoPrintSession::GetDocURL (PRUnichar * *aDocURL)
{
  g_return_val_if_reached (NS_ERROR_NOT_IMPLEMENTED);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoPrintSession::SetDocURL (const PRUnichar * aDocURL)
{
#if 0
  NS_ENSURE_STATE (mJob);

  char *converted = EphyUtils::ConvertUTF16toUTF8 (aDocTitle, MAX_STRING_LENGTH);
  if (converted) {
    g_free (converted);
  }
#endif
  return NS_OK;
}
