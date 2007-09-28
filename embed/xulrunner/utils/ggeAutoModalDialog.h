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

#ifndef GGE_AUTOMODALDIALOG_H
#define GGE_AUTOMODALDIALOG_H

#include <nsCOMPtr.h>

#include <gtk/gtkwidget.h>

class nsPIDOMWindow;
class nsIJSContextStack;

/**
 * ggeAutoModalDialog:
 * A stack-based helper class for modal GTK+ dialogues.
 * 
 * You MUST check ::ShouldShow() before showing the dialogue!
 */
class ggeAutoModalDialog
{
  public:
    ggeAutoModalDialog (nsIDOMWindow *,
                        PRBool aNotifyDOM = PR_TRUE);
    ~ggeAutoModalDialog ();
	
    PRBool ShouldShow () { return mDefaultEnabled && mContextPushed; }

    void Run (GtkWidget *aDialog);
    int Response () { return mResponse; }

  private: 
    // stack only please
    void *operator new (size_t) CPP_THROW_NEW;

    PRBool DispatchEvent (const char *aEvent);

    static void PR_CALLBACK DialogResponseCallback (GtkWidget*, int, void*);
    static gboolean PR_CALLBACK DialogDeleteCallback (GtkWidget*, void*, void*);

    nsCOMPtr<nsIDOMWindow> mWindow;
    nsCOMPtr<nsPIDOMWindow> mPWindow;
    nsCOMPtr<nsIJSContextStack> mStack;

    GtkWidget *mDialog;
    int mResponse;
    PRPackedBool mDefaultEnabled;
    PRPackedBool mContextPushed;
    PRPackedBool mModalStateSet;
    PRPackedBool mContinueModalLoop;
};

#endif
