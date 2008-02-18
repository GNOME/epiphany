/* 
 *  Copyright © 2006, 2008 Christian Persch
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
 *  Some code taken from mozilla/embedding/components/windowwatcher/src/nsPromptService.cpp
 *  which was under MPL/LGPL/GPL tri-licence and is here being used under the licence above.
 *  Original notice:
 *
 *   The Original Code is mozilla.org code.
 *
 *   The Initial Developer of the Original Code is
 *   Netscape Communications Corporation.
 *   Portions created by the Initial Developer are Copyright (C) 2001
 *   the Initial Developer. All Rights Reserved.
 */

#include <xpcom-config.h>
#include <config.h>

#ifdef HAVE_GECKO_1_9
#include <nsIDOMDocumentEvent.h>
#include <nsIDOMDocument.h>
#include <nsIDOMEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIJSContextStack.h>
#include <nsIPrivateDOMEvent.h>
#include <nsPIDOMWindow.h>
#include <nsServiceManagerUtils.h>
#include <nsThreadUtils.h>
#endif /* HAVE_GECKO_1_9 */

#include <gtk/gtkdialog.h>

#include "EphyUtils.h"

#include "AutoModalDialog.h"

AutoModalDialog::AutoModalDialog (nsIDOMWindow *aWindow,
                                  PRBool aNotifyDOM)
  : mWindow (aWindow),
    mStack (),
    mModalState (aWindow),
#ifdef HAVE_GECKO_1_9
    mDefaultEnabled (DispatchEvent ("DOMWillOpenModalDialog", aNotifyDOM)),
#endif
    mContextPushed (NS_SUCCEEDED (mStack.Init ()))
{
}

AutoModalDialog::~AutoModalDialog ()
{
#ifdef HAVE_GECKO_1_9
  if (mDefaultEnabled) {
    DispatchEvent ("DOMModalDialogClosed", PR_TRUE);
  }
#endif /* HAVE_GECKO_1_9 */
}

GtkWindow *
AutoModalDialog::GetParent ()
{
  return GTK_WINDOW (EphyUtils::FindGtkParent (mWindow));
}

int
AutoModalDialog::Run (GtkDialog *aDialog)
{
  NS_ASSERTION (ShouldShow(), "Calling ::Run on a prevented dialogue!");

  nsCOMPtr<nsPIDOMWindow> pWindow (do_QueryInterface (mWindow));

  // Reset popup state while opening a modal dialog, and firing
  // events about the dialog, to prevent the current state from
  // being active the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher (pWindow, openAbused);
  
#if 1
  return gtk_dialog_run (aDialog);
#else
  /* Do NOT use gtk_dialog_run here, since it blocks the network thread!
   * See https://bugzilla.mozilla.org/show_bug.cgi?id=338225
   */
  
  g_object_ref_sink (aDialog);
  mResponse = GTK_RESPONSE_DELETE_EVENT;

  gulong responseHandler = g_signal_connect (aDialog, "response",
                                             G_CALLBACK (ResponseCallback),
                                             reinterpret_cast<void*>(this));
  gulong deleteHandler = g_signal_connect (aDialog, "delete-event",
                                           G_CALLBACK (DeleteCallback), NULL);

  gtk_window_present (GTK_WINDOW (aDialog));

  nsCOMPtr<nsIThread> thread (do_GetCurrentThread ());
  NS_ASSERTION (thread, "No UI thread?");
  
  mContinueModalLoop = PR_TRUE;
  while (mContinueModalLoop) {
    if (!NS_ProcessNextEvent (thread))
      break;
  }

  g_signal_handler_disconnect (aDialog, responseHandler);
  g_signal_handler_disconnect (aDialog, deleteHandler);
  g_object_unref (aDialog);

  return mResponse;
#endif
}

#ifdef HAVE_GECKO_1_9

PRBool
AutoModalDialog::DispatchEvent (const char *aEventName,
                                PRBool aDoNotify)
{
  if (!mWindow || !aDoNotify) {
    return PR_TRUE;
  }

  nsCOMPtr<nsIDOMDocument> domdoc;
  mWindow->GetDocument (getter_AddRefs (domdoc));

  nsCOMPtr<nsIDOMDocumentEvent> docevent (do_QueryInterface (domdoc));
  nsCOMPtr<nsIDOMEvent> event;

  PRBool defaultActionEnabled = PR_TRUE;

  if (docevent) {
    docevent->CreateEvent (NS_LITERAL_STRING ("Events"), getter_AddRefs (event));

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent (do_QueryInterface (event));
    if (privateEvent) {
      event->InitEvent (NS_ConvertASCIItoUTF16 (aEventName), PR_TRUE, PR_TRUE);

      privateEvent->SetTrusted(PR_TRUE);

      nsCOMPtr<nsIDOMEventTarget> target (do_QueryInterface (mWindow));

      target->DispatchEvent (event, &defaultActionEnabled);
    }
  }

  return defaultActionEnabled;
}

/* static */ void PR_CALLBACK
AutoModalDialog::ResponseCallback (GtkWidget *aDialog,
                                   int aResponse,
                                   void *aData)
{
  AutoModalDialog *obj = reinterpret_cast<AutoModalDialog*>(aData);

  gtk_widget_hide (aDialog);
  obj->mResponse = aResponse;
  obj->mContinueModalLoop = PR_FALSE;
}

/* static */ gboolean PR_CALLBACK
AutoModalDialog::DeleteCallback (GtkWidget *aDialog,
                                 void *aEvent,
                                 void *aData)
{
  gtk_dialog_response (GTK_DIALOG (aDialog), GTK_RESPONSE_DELETE_EVENT);
  return TRUE;
}

#endif /* HAVE_GECKO_1_9 */
