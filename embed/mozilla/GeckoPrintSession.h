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

#ifndef GECKO_PRINT_SESSION_H
#define GECKO_PRINT_SESSION_H

#include <nsCOMPtr.h>
#include <nsIPrintSession.h>
#include <nsIWebProgressListener.h>
#include <nsIPrintProgress.h>
#include <nsIPrintProgressParams.h>
#include <nsWeakReference.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkprintjob.h>
#include <gtk/gtkprinter.h>
#include <gtk/gtkprintjob.h>

class nsIPrintSettings;
class nsIDOMWindow;

/* 0940c973-97e7-476f-a612-4ed9473a0b36 */
#define GECKO_PRINT_SESSION_IID \
{ 0x0940c973, 0x97e7, 0x476f, \
  { 0xa6, 0x12, 0x4e, 0xd9, 0x47, 0x3a, 0x0b, 0x36 } }

#define GECKO_PRINT_SESSION_CLASSNAME "Gecko Print Session"

class GeckoPrintSession : public nsIPrintSession,
			  public nsIPrintProgress,
			  public nsIPrintProgressParams,
			  public nsSupportsWeakReference
{
  public:
    GeckoPrintSession();
    virtual ~GeckoPrintSession();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINTSESSION
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIPRINTPROGRESS
    NS_DECL_NSIPRINTPROGRESSPARAMS

    nsresult SetSettings (GtkPrintSettings*, GtkPageSetup*, GtkPrinter*);
    void GetSourceFile (nsACString&);

    static GeckoPrintSession *FromSettings (nsIPrintSettings *);

    void Cancel ();
    void StartPrinting ();
    void JobStatusChanged ();
    void JobDone ();
    void JobError (const char *);

  private:
    GtkPrintSettings *mSettings;
    GtkPageSetup *mPageSetup;
    GtkPrinter *mPrinter;
    GtkPrintJob *mJob;
    GtkWidget *mProgressDialog;
    GtkWidget *mTitleLabel;
    GtkWidget *mProgressBar;
    nsCString mSourceFile;
    nsCString mTitle;
    guint mStartPrintIdleID;
    PRPackedBool mSourceFileIsTemp;
    PRPackedBool mDone;
    PRPackedBool mCancelled;

    void SetProgress (PRInt32, PRInt32);
    void SetProgressText (const char *);
    void LaunchJobOnIdle ();
    void DestroyJob ();
};

#endif /* GECKO_PRINT_SESSION_H */
