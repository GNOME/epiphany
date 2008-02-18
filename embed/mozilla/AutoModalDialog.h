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
 */

#ifndef AUTOMODALDIALOG_H
#define AUTOMODALDIALOG_H

#include <nsCOMPtr.h>

#include <gtk/gtkwidget.h>

#include "AutoJSContextStack.h"
#include "AutoWindowModalState.h"

/**
 * AutoModalDialog:
 * A stack-based helper class for modal GTK+ dialogues.
 * 
 * You MUST check ::ShouldShow() before showing the dialogue!
 */
class AutoModalDialog
{
  public:
    AutoModalDialog (nsIDOMWindow *, PRBool);
    ~AutoModalDialog ();

    GtkWindow *GetParent ();
 
    PRBool ShouldShow () { return mDefaultEnabled && mContextPushed; }

    int Run (GtkDialog *aDialog);

  private: 
    // stack only please
    void *operator new (size_t) CPP_THROW_NEW;

    nsCOMPtr<nsIDOMWindow> mWindow;

    AutoJSContextStack mStack;
    AutoWindowModalState mModalState;

#ifdef HAVE_GECKO_1_9
    static void PR_CALLBACK ResponseCallback (GtkWidget*, int, void*);
    static gboolean PR_CALLBACK DeleteCallback (GtkWidget*, void*, void*);

    PRBool DispatchEvent (const char*, PRBool);

    int mResponse;
    PRPackedBool mContinueModalLoop;
#endif

    PRPackedBool mDefaultEnabled;
    PRPackedBool mContextPushed;
};

#endif
