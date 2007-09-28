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

struct JSContext; /* Just so we don't need to include a bunch of JS headers */

#include <nsIDOMDocumentEvent.h>
#include <nsIDOMDocument.h>
#include <nsIDOMEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIJSContextStack.h>
#include <nsPIDOMWindow.h>
#include <nsServiceManagerUtils.h>
#include <nsThreadUtils.h>

#include "ggeAutoModalDialog.h"

ggeAutoModalDialog::ggeAutoModalDialog (nsIDOMWindow *aWindow,
                                        PRBool aNotifyDOM)
  : mWindow (aWindow),
    mPWindow (do_QueryInterface (aWindow))
    mStack (do_GetService ("@mozilla.org/js/xpc/ContextStack;1"))
    mDefaultEnabled (PR_TRUE),
    mContextPushed (PR_FALSE),
    mModalStateSet (PR_FALSE)
{
  /* First we check whether we should show the dialogue at all */
  if (aNotifyDOM) {
    mDefaultEnabled = DispatchEvent ("DOMWillOpenModalDialog"));
    if (!mDefaultEnabled) {
      return;
    }
  }

  if (mStack) {
    mContextPushed = NS_SUCCEEDED (mStack->Push (nsnull));
  }

  if (mWindow) {
    NS_ASSERTION (mPWindow, "Should have a window here!");

    mWindow->EnterModalState ();
    mModalStateSet = PR_TRUE;
  }
}

ggeAutoModalDialog::~ggeAutoModalDialog ()
{
  if (mModalStateSet) {
    NS_ASSERTION (mPWindow, "Should have a window here!");
    mWindow->LeaveModalState ();
  }

  if (mContextPushed) {
    NS_ASSERTION (mStack, "Should have a stack!");
    
    JSContext* cx;
    mStack->Pop (&cx);

    NS_ASSERTION(cx == nsnull, "We pushed a null context but popped a non-null context!?");
  }

}

/* static */ void PR_CALLBACK
ggeAutoModalDialog::ResponseCallback (GtkWidget *aDialog,
                                      int aResponse,
                                      void *aData)
{
  ggeAutoModalDialog *obj = reinterpret_cast<ggeAutoModalDialog*>(aData);

  gtk_widget_hide (aDialog);
  obj->mResponse = aResponse;
  obj->mContinueModalLoop = PR_FALSE;
}

static gboolean PR_CALLBACK
ggeAutoModalDialog::DeleteCallback (GtkWidget *aDialog,
                                    void *aEvent,
                                    void *aData)
{
  gtk_dialog_response (GTK_DIALOG (aDialog), GTK_RESPONSE_DELETE_EVENT);
  return TRUE;
}

int
ggeAutoModalDialog::Run (GtkDialog *aDialog)
{
  NS_ASSERTION (ShouldShow(), "Calling ::Run on a prevented dialogue!");

  /* Do NOT use gtk_dialog_run here, since it blocks the network thread!
   * See https://bugzilla.mozilla.org/show_bug.cgi?id=338225
   */
  
  // Reset popup state while opening a modal dialog, and firing
  // events about the dialog, to prevent the current state from
  // being active the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher (mPWindow, openAbused);
  
  mDialog = g_object_ref_sink (aDialog);
  mResponse = GTK_RESPONSE_DELETE_EVENT;

  gulong responseHandler = g_signal_connect (mDialog, "response",
                                             ResponseCallback,
                                             reinterpret_cast<void*>(this));
  gulong deleteHandler = g_signal_connect (mDialog, "delete-event",
                                           DeleteCallback,
                                           reinterpret_cast<void*>(this));

  
  nsIThread *thread = NS_GetCurrentThread();
  
  mContinueModalLoop = PR_TRUE;
  while (mContinueModalLoop) {
    if (!NS_ProcessNextEvent(thread))
      break;
  }

  g_signal_handler_disconnect (mDialog, responseHandler);
  g_signal_handler_disconnect (mDialog, deleteHandler);
  /* FIXME */
  g_object_unref (mDialog);
  mDialog = NULL;
}

PRBool
ggeAutoModalDialog::DispatchEvent (const char *aEvent)
{
   if (!mWindow) {
    return PR_TRUE;
  }

  nsCOMPtr<nsIDOMDocument> domdoc;
  mWindow->GetDocument (getter_AddRefs (domdoc));

  nsCOMPtr<nsIDOMDocumentEvent> docevent (do_QueryInterface (domdoc));
  nsCOMPtr<nsIDOMEvent> event;

  PRBool defaultActionEnabled = PR_TRUE;

  if (docevent) {
    docevent->CreateEvent(NS_LITERAL_STRING ("Events"), getter_AddRefs (event));

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent (do_QueryInterface (event));
    if (privateEvent) {
      event->InitEvent(NS_ConvertASCIItoUTF16(aEventName), PR_TRUE, PR_TRUE);

      privateEvent->SetTrusted(PR_TRUE);

      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mWindow));

      target->DispatchEvent(event, &defaultActionEnabled);
    }
  }

  return defaultActionEnabled;
}
