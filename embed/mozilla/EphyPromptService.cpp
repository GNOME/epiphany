/*
 *  Copyright © 2005, 2006 Christian Persch
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
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsServiceManagerUtils.h>

#include "ephy-embed-shell.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include "AutoModalDialog.h"
#include "EphyUtils.h"

#ifdef HAVE_GECKO_1_9
#include <nsIStringBundle.h>
#include "nsIChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIProxyInfo.h"
#include "nsNetCID.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIIDNService.h"
#include "nsIAuthInformation.h"
#endif /* HAVE_GECKO_1_9 */

#include "EphyPromptService.h"

#define TIMEOUT			1000 /* ms */
#define TIMEOUT_DATA_KEY	"timeout"

#define MAX_MESSAGE_LENGTH	512
#define MAX_TITLE_LENGTH	256
#define MAX_BUTTON_TEXT_LENGTH	128

enum
{
	RESPONSE_ABORT_SCRIPT = 42
};

class Prompter
{
public:
	Prompter (const char*, nsIDOMWindow*, const PRUnichar*, const PRUnichar*);
	~Prompter();

	void AddStockButton (const char*, int);
	void AddButtonWithFlags (PRInt32, PRUint32, const PRUnichar*, PRUint32);
	void AddButtonsWithFlags (PRUint32, const PRUnichar*, const PRUnichar*, const PRUnichar*);
	void AddCheckbox (const PRUnichar*, PRBool*);
	void GetCheckboxState (PRBool *);
	void AddEntry (const char *, const PRUnichar *, PRBool);
	void GetText (PRUint32, PRUnichar **);
	void AddSelect (PRUint32, const PRUnichar **, PRInt32);
	void GetSelected (PRInt32*);

	PRInt32 Run (PRBool * = nsnull);
	void Show ();

	PRBool IsCalledFromScript ();
	void PerformScriptAbortion ();

	char *ConvertAndTruncateString (const PRUnichar *, PRInt32 = -1);
	char* ConvertAndEscapeButtonText (const PRUnichar *, PRInt32 = -1);

private:
	nsCOMPtr<nsIDOMWindow> mWindow;
	GtkDialog *mDialog;
	GtkWidget *mVBox;
	GtkWidget *mCheck;
	GtkSizeGroup *mSizeGroup;
	GtkWidget *mEntries[2];
	GtkWidget *mCombo;
	PRInt32 mNumButtons;
	PRInt32 mNumEntries;
	PRInt32 mDefaultResponse;
	PRInt32 mUnaffirmativeResponse;
	PRInt32 mResponse;
	PRBool mSuccess;
	PRBool mDelay;
};

Prompter::Prompter (const char *aStock,
		    nsIDOMWindow *aParent,
		    const PRUnichar *aTitle,
		    const PRUnichar *aText)
	: mWindow (aParent)
	, mDialog(nsnull)
	, mVBox(nsnull)
	, mCheck(nsnull)
	, mSizeGroup(nsnull)
	, mCombo(nsnull)
	, mNumButtons(0)
	, mNumEntries(0)
	, mDefaultResponse(GTK_RESPONSE_ACCEPT)
	, mUnaffirmativeResponse(0)
	, mResponse(GTK_RESPONSE_CANCEL)
	, mSuccess(PR_FALSE)
	, mDelay(PR_FALSE)
{
	GtkWidget *parent, *hbox, *label, *image;

	g_object_ref (ephy_embed_shell_get_default ());

	mEntries[0] = mEntries[1] = nsnull;

	mDialog = GTK_DIALOG (gtk_dialog_new ());
	g_object_ref_sink (mDialog);

	char *title = NULL;
	if (aTitle)
	{
		title = ConvertAndTruncateString (aTitle, MAX_TITLE_LENGTH);
	}

	gtk_window_set_title (GTK_WINDOW (mDialog), title ? title : "");
	g_free (title);

	gtk_window_set_modal (GTK_WINDOW (mDialog), TRUE);

	parent = EphyUtils::FindGtkParent (aParent);
	if (GTK_IS_WINDOW (parent))
	{
		gtk_window_set_transient_for (GTK_WINDOW (mDialog),
					      GTK_WINDOW (parent));

		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
					     GTK_WINDOW (mDialog));
	}

	gtk_dialog_set_has_separator (mDialog, FALSE);
	gtk_window_set_resizable (GTK_WINDOW (mDialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (mDialog), 5);
	gtk_box_set_spacing (GTK_BOX (mDialog->vbox), 14); /* 2 * 5 + 14 = 24 */
	
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (mDialog)->vbox), hbox);

	image = gtk_image_new_from_stock (aStock, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	mVBox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), mVBox, TRUE, TRUE, 0);

	char *text = NULL;
	if (aText)
	{
		text = ConvertAndTruncateString (aText, MAX_MESSAGE_LENGTH);
	}

	label = gtk_label_new (text ? text : "");
	g_free (text);

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	/* Guard against overlong nonbreakable text (exploit) */
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

	gtk_box_pack_start (GTK_BOX (mVBox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	if (IsCalledFromScript ())
	{
		gtk_dialog_add_button (GTK_DIALOG (mDialog),
				       _("_Abort Script"),
				       RESPONSE_ABORT_SCRIPT);
	}

	gtk_widget_show (image);
	gtk_widget_show (mVBox);
	gtk_widget_show (hbox);
}

Prompter::~Prompter ()
{
	if (mSizeGroup)
	{
		g_object_unref (mSizeGroup);
	}

	gtk_widget_destroy (GTK_WIDGET (mDialog));
	g_object_unref (mDialog);

	g_object_unref (ephy_embed_shell_get_default ());
}

void
Prompter::AddStockButton (const char *aStock,
			  int aResponse)
{
	gtk_dialog_add_button (GTK_DIALOG (mDialog),
			       aStock, aResponse);
	++mNumButtons;
}

void
Prompter::AddButtonWithFlags (PRInt32 aNum,
			      PRUint32 aFlags,
			      const PRUnichar *aText,
			      PRUint32 aDefault)
{
	if (aFlags == 0) return;

	const char *label = NULL;
	char *freeme = NULL;
	gboolean isAffirmative = FALSE;
	switch (aFlags)
	{
		case nsIPromptService::BUTTON_TITLE_OK:
			label = GTK_STOCK_OK;
			isAffirmative = TRUE;
			break;

		case nsIPromptService::BUTTON_TITLE_CANCEL:
			label = GTK_STOCK_CANCEL;
			break;

		case nsIPromptService::BUTTON_TITLE_YES:
			label = GTK_STOCK_YES;
			isAffirmative = TRUE;
			break;

		case nsIPromptService::BUTTON_TITLE_NO:
			label = GTK_STOCK_NO;
			break;

		case nsIPromptService::BUTTON_TITLE_SAVE:
			label = GTK_STOCK_SAVE;
			isAffirmative = TRUE;
			break;

		case nsIPromptService::BUTTON_TITLE_DONT_SAVE:
			label = _("Don't Save");
			break;

		case nsIPromptService::BUTTON_TITLE_REVERT:
			label = GTK_STOCK_REVERT_TO_SAVED;
			break;

		case nsIPromptService::BUTTON_TITLE_IS_STRING:
		default:
			label = freeme = ConvertAndEscapeButtonText (aText, MAX_BUTTON_TEXT_LENGTH);
			/* We can't tell, so assume it's affirmative */
			isAffirmative = TRUE;
			break;
	}

	if (label == NULL) return;

	gtk_dialog_add_button (mDialog, label, aNum);
	++mNumButtons;

	if (isAffirmative && mDelay)
	{
		gtk_dialog_set_response_sensitive (mDialog, aNum, FALSE);
	}

	if (!isAffirmative)
	{
		mUnaffirmativeResponse = aNum;
	}

	if (aDefault)
	{
		mDefaultResponse = aNum;
	}

	g_free (freeme);
}

void
Prompter::AddButtonsWithFlags (PRUint32 aFlags,
			       const PRUnichar *aText0,
			       const PRUnichar *aText1,
			       const PRUnichar *aText2)
{
	mDelay = (aFlags & nsIPromptService::BUTTON_DELAY_ENABLE) != 0;
	mDefaultResponse = -1;

	/* Reverse the order, on the assumption that what we passed is the
	 * 'windows' button order, and we want HIG order.
	 */
	AddButtonWithFlags (2, ((aFlags / nsIPromptService::BUTTON_POS_2) & 0xff), aText2,
			    aFlags & nsIPromptService::BUTTON_POS_2_DEFAULT);
	AddButtonWithFlags (1, ((aFlags / nsIPromptService::BUTTON_POS_1) & 0xff), aText1,
			    aFlags & nsIPromptService::BUTTON_POS_1_DEFAULT);
	AddButtonWithFlags (0, ((aFlags / nsIPromptService::BUTTON_POS_0) & 0xff), aText0,
			    aFlags & nsIPromptService::BUTTON_POS_0_DEFAULT);

	/* If no default was set, use the 'rightmost' unaffirmative response.
	 * This happens with the suite's password manager prompt.
	 */
	if (mDefaultResponse == -1)
	{
		mDefaultResponse = mUnaffirmativeResponse;
	}
}

void
Prompter::AddCheckbox (const PRUnichar *aText,
		       PRBool *aState)
{
	if (!aState || !aText) return;

	char *label = ConvertAndEscapeButtonText (aText, 2 * MAX_BUTTON_TEXT_LENGTH);
	mCheck = gtk_check_button_new_with_mnemonic (label ? label : "");
	g_free (label);

	gtk_label_set_line_wrap (GTK_LABEL (GTK_BIN (mCheck)->child), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mCheck), *aState);
	gtk_box_pack_start (GTK_BOX (mVBox), mCheck, FALSE, FALSE, 0);
	gtk_widget_show (mCheck);
}

void
Prompter::GetCheckboxState (PRBool *aState)
{
	if (!aState || !mCheck) return;
		
	*aState = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mCheck));
}

void
Prompter::AddEntry (const char *aLabel,
		    const PRUnichar *aValue,
		    PRBool aIsPassword)
{
	if (!mSizeGroup)
	{
		mSizeGroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	}

	GtkWidget *hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (mVBox), hbox, FALSE, FALSE, 0);

	GtkWidget *label = nsnull;
	if (aLabel)
	{
		label = gtk_label_new_with_mnemonic (aLabel);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		gtk_size_group_add_widget (mSizeGroup, label);
	}
       
	GtkWidget *entry = mEntries[mNumEntries++] = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (entry), !aIsPassword);
	gtk_entry_set_activates_default(GTK_ENTRY (entry), TRUE);

	if (aValue)
	{
		nsCString cValue;
		NS_UTF16ToCString (nsDependentString(aValue),
				   NS_CSTRING_ENCODING_UTF8, cValue);

		gtk_entry_set_text (GTK_ENTRY (entry), cValue.get());
	}

	if (label)
	{
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	}

	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);
}

void
Prompter::GetText (PRUint32 aNum,
		   PRUnichar **aValue)
{
	if (!aValue || !mEntries[aNum]) return;

	const char *text = gtk_entry_get_text (GTK_ENTRY (mEntries[aNum]));
	if (!text) return;

	nsString value;
	NS_CStringToUTF16 (nsDependentCString (text),
			   NS_CSTRING_ENCODING_UTF8, value);

	*aValue = NS_StringCloneData (value);
}

void
Prompter::AddSelect (PRUint32 aCount,
		     const PRUnichar **aList,
		     PRInt32 aDefault)
{
	mCombo = gtk_combo_box_new_text ();

	for (PRUint32 i = 0; i < aCount; i++)
	{
		/* FIXME: use "" instead in this case? */
		if (!aList[i] || !aList[i][0]) continue;

		nsCString cData;
		NS_UTF16ToCString (nsDependentString(aList[i]), NS_CSTRING_ENCODING_UTF8, cData);

		gtk_combo_box_append_text (GTK_COMBO_BOX (mCombo), cData.get());
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (mCombo), aDefault);

	gtk_box_pack_start (GTK_BOX (mVBox), mCombo, FALSE, FALSE, 0);
	gtk_widget_show (mCombo);
}

void
Prompter::GetSelected (PRInt32 *aSelected)
{
	if (!aSelected || !mCombo) return;

	*aSelected = gtk_combo_box_get_active (GTK_COMBO_BOX (mCombo));
}

static gboolean
EnableResponse (GtkDialog *aDialog)
{
	g_object_steal_data (G_OBJECT (aDialog), TIMEOUT_DATA_KEY);

	gtk_dialog_set_response_sensitive (aDialog, 0, TRUE);
	gtk_dialog_set_response_sensitive (aDialog, 1, TRUE);
	gtk_dialog_set_response_sensitive (aDialog, 2, TRUE);

	return FALSE;
}

static void
RemoveTimeout (gpointer idptr)
{
	guint timeout = GPOINTER_TO_UINT (idptr);

	g_return_if_fail (timeout != 0);

	g_source_remove (timeout);
}

PRInt32
Prompter::Run (PRBool *aSuccess)
{
#if 0
	AutoEventQueue queue;
	if (NS_FAILED (queue.Init()))
	{
		if (aSuccess)
		{
			*aSuccess = PR_FALSE;
		}
		mSuccess = PR_FALSE;

		return GTK_RESPONSE_CANCEL;
	}
#endif

        AutoModalDialog modalDialog (mWindow, PR_TRUE);
        if (!modalDialog.ShouldShow ())
                return GTK_RESPONSE_CANCEL;

	if (mDelay)
	{
		guint timeout = g_timeout_add (TIMEOUT,
					       (GSourceFunc) EnableResponse,
					       mDialog);
		g_object_set_data_full (G_OBJECT (mDialog), TIMEOUT_DATA_KEY,
					GUINT_TO_POINTER (timeout),
					(GDestroyNotify) RemoveTimeout);
	}

	gtk_dialog_set_default_response (GTK_DIALOG (mDialog), mDefaultResponse);

	GtkWidget *widget = GTK_WIDGET (mDialog);
	gtk_widget_show (widget);
	mResponse = modalDialog.Run (mDialog);
	gtk_widget_hide (widget);

	g_object_set_data (G_OBJECT (mDialog), TIMEOUT_DATA_KEY, NULL);

	mSuccess = (GTK_RESPONSE_ACCEPT == mResponse);
	if (aSuccess)
	{
		*aSuccess = mSuccess;
	}

	if (mResponse == RESPONSE_ABORT_SCRIPT)
	{
		PerformScriptAbortion ();
	}

	return mResponse;
}

static void
DeletePrompter (gpointer aPromptPtr,
	        GObject *aZombie)
{
	Prompter *prompt = static_cast<Prompter*>(aPromptPtr);

	delete prompt;
}

void
Prompter::Show ()
{
	/* We don't need it anymore */
	mWindow = nsnull;

	gtk_window_set_modal (GTK_WINDOW (mDialog), FALSE);

	g_signal_connect (mDialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	g_object_weak_ref (G_OBJECT (mDialog),
			   (GWeakNotify) DeletePrompter,
			   static_cast<gpointer>(this));

	gtk_widget_show (GTK_WIDGET (mDialog));
}

PRBool
Prompter::IsCalledFromScript()
{
#if 0
	nsCOMPtr<nsIXPConnect> xpconnect (do_GetService (nsIXPConnect::GetCID()));
	NS_ENSURE_TRUE (xpconnect, PR_FALSE);

	nsresult rv;
	nsCOMPtr<nsIXPCNativeCallContext> ncc;
	rv = xpconnect->GetCurrentNativeCallContext (getter_AddRefs (ncc));
	NS_ENSURE_SUCCESS (rv, PR_FALSE);

	if (!ncc) return PR_FALSE;

	JSContext *cx = nsnull;
	rv = ncc->GetJSContext (&cx);
	g_print ("GetJSContext rv=%x, cx=%p\n", rv, cx);

	NS_ENSURE_SUCCESS (rv, PR_FALSE);

	return cx != nsnull;
#endif
	return PR_FALSE;
}

void
Prompter::PerformScriptAbortion()
{
#if 0
	/* FIXME: can we only stop the calling script, not all scripts in the context? */

	nsCOMPtr<nsIXPConnect> xpconnect (do_GetService (nsIXPConnect::GetCID()));
	NS_ENSURE_TRUE (xpconnect, );

	nsresult rv;
	nsCOMPtr<nsIXPCNativeCallContext> ncc;
	rv = xpconnect->GetCurrentNativeCallContext (getter_AddRefs (ncc));
	NS_ENSURE_SUCCESS (rv, );
	NS_ENSURE_TRUE (ncc, );

	JSContext *cx = nsnull;
	rv = ncc->GetJSContext (&cx);
	g_print ("GetJSContext rv=%x, cx=%p\n", rv, cx);
	NS_ENSURE_SUCCESS (rv, );
	NS_ENSURE_TRUE (cx, );

	g_print ("Would now disable scripts\n");
//	MozillaPrivate::SetScriptsEnabled (cx, PR_FALSE, PR_FALSE);
#endif
}

char *
Prompter::ConvertAndTruncateString (const PRUnichar *aText,
				    PRInt32 aMaxLength)
{
	if (aText == nsnull) return NULL;

	/* This depends on the assumption that 
	 * typeof(PRUnichar) == typeof (gunichar2) == uint16,
	 * which should be pretty safe.
	 */
	glong n_read = 0, n_written = 0;
	char *converted = g_utf16_to_utf8 ((gunichar2*) aText, aMaxLength,
					    &n_read, &n_written, NULL);
	/* FIXME loop from the end while !g_unichar_isspace (char)? */
        if (!converted)
                return NULL;

        g_strdelimit (converted, "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037", ' ');
	return converted;
}

char *
Prompter::ConvertAndEscapeButtonText(const PRUnichar *aText,
			   	     PRInt32 aMaxLength)
{
	char *converted = ConvertAndTruncateString (aText, aMaxLength);
	if (converted == NULL) return NULL;

	char *escaped = (char*) g_malloc (strlen (converted) + 1);
	char *q = escaped;
	for (const char *p = converted; *p; ++p, ++q)
	{
		if (*p == '&')
		{
			if (*(p+1) == '&')
			{
				*q = '&';
				++p;
			}
			else
			{
				*q = '_';
			}
		}
		else
		{
			*q = *p;
		}
	}

	/* Null termination */
	*q = '\0';

	g_free (converted);

	return escaped;
}

#ifdef HAVE_GECKO_1_9
NS_IMPL_THREADSAFE_ISUPPORTS3 (EphyPromptService,
                               nsIPromptService,
                               nsIPromptService2,
                               nsINonBlockingAlertService)
#elif defined(HAVE_NSINONBLOCKINGALERTSERVICE_H)
NS_IMPL_ISUPPORTS2 (EphyPromptService,
		    nsIPromptService,
		    nsINonBlockingAlertService)
#else
NS_IMPL_ISUPPORTS1 (EphyPromptService,
		    nsIPromptService)
#endif

EphyPromptService::EphyPromptService()
{
	LOG ("EphyPromptService ctor (%p)", this);
}

EphyPromptService::~EphyPromptService()
{
	LOG ("EphyPromptService dtor (%p)", this);
}

/* nsIPromptService implementation */

/* void alert (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText); */
NS_IMETHODIMP
EphyPromptService::Alert (nsIDOMWindow *aParent,
			  const PRUnichar *aDialogTitle,
			  const PRUnichar *aText)
{
	Prompter prompt (GTK_STOCK_DIALOG_INFO, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.Run ();

	return NS_OK;
}

/* void alertCheck (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, in wstring aCheckMsg, inout boolean aCheckState); */
NS_IMETHODIMP
EphyPromptService::AlertCheck (nsIDOMWindow *aParent,
			       const PRUnichar *aDialogTitle,
			       const PRUnichar *aText,
			       const PRUnichar *aCheckMsg,
			       PRBool *aCheckState)
{
	Prompter prompt (GTK_STOCK_DIALOG_INFO, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.AddCheckbox (aCheckMsg, aCheckState);

	prompt.Run ();
	prompt.GetCheckboxState (aCheckState);

	return NS_OK;
}

/* boolean confirm (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText); */
NS_IMETHODIMP
EphyPromptService::Confirm (nsIDOMWindow *aParent,
			    const PRUnichar *aDialogTitle,
			    const PRUnichar *aText,
			    PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);

	Prompter prompt (GTK_STOCK_DIALOG_QUESTION, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.Run (_retval);

	return NS_OK;
}

/* boolean confirmCheck (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, in wstring aCheckMsg, inout boolean aCheckState); */
NS_IMETHODIMP
EphyPromptService::ConfirmCheck (nsIDOMWindow *aParent,
				 const PRUnichar *aDialogTitle,
				 const PRUnichar *aText,
				 const PRUnichar *aCheckMsg,
				 PRBool *aCheckState,
				 PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);

	Prompter prompt (GTK_STOCK_DIALOG_QUESTION, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.AddCheckbox (aCheckMsg, aCheckState);

	prompt.Run (_retval);
	prompt.GetCheckboxState (aCheckState);

	return NS_OK;
}

/* PRInt32 confirmEx (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, in unsigned long aButtonFlags, in wstring aButton0Title, in wstring aButton1Title, in wstring aButton2Title, in wstring aCheckMsg, inout boolean aCheckState); */
NS_IMETHODIMP
EphyPromptService::ConfirmEx (nsIDOMWindow *aParent,
			      const PRUnichar *aDialogTitle,
			      const PRUnichar *aText,
			      PRUint32 aButtonFlags,
			      const PRUnichar *aButton0Title,
			      const PRUnichar *aButton1Title,
			      const PRUnichar *aButton2Title,
			      const PRUnichar *aCheckMsg,
			      PRBool *aCheckState,
			      PRInt32 *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);

	Prompter prompt (GTK_STOCK_DIALOG_QUESTION, aParent, aDialogTitle, aText);
	prompt.AddButtonsWithFlags (aButtonFlags, aButton0Title,
				    aButton1Title, aButton2Title);
	prompt.AddCheckbox (aCheckMsg, aCheckState);

	*_retval = prompt.Run (nsnull);
	prompt.GetCheckboxState (aCheckState);

	return NS_OK;
}

/* boolean prompt (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, inout wstring aValue, in wstring aCheckMsg, inout boolean aCheckState); */
NS_IMETHODIMP
EphyPromptService::Prompt (nsIDOMWindow *aParent,
			   const PRUnichar *aDialogTitle,
			   const PRUnichar *aText,
			   PRUnichar **aValue,
			   const PRUnichar *aCheckMsg,
			   PRBool *aCheckState,
			   PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);
	NS_ENSURE_ARG_POINTER (aValue);

	Prompter prompt (GTK_STOCK_DIALOG_QUESTION, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.AddEntry (nsnull, *aValue, PR_FALSE);
	prompt.AddCheckbox (aCheckMsg, aCheckState);

	prompt.Run (_retval);
	prompt.GetText (0, aValue);
	prompt.GetCheckboxState (aCheckState);

	return NS_OK;
}

/* boolean promptUsernameAndPassword (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, inout wstring aUsername, inout wstring aPassword, in wstring aCheckMsg, inout boolean aCheckState); */
NS_IMETHODIMP
EphyPromptService::PromptUsernameAndPassword (nsIDOMWindow *aParent,
					      const PRUnichar *aDialogTitle,
					      const PRUnichar *aText,
					      PRUnichar **aUsername,
					      PRUnichar **aPassword,
					      const PRUnichar *aCheckMsg,
					      PRBool *aCheckState,
					      PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);
	NS_ENSURE_ARG_POINTER (aUsername);
	NS_ENSURE_ARG_POINTER (aPassword);

	Prompter prompt (GTK_STOCK_DIALOG_AUTHENTICATION, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.AddEntry (_("_Username:"), *aUsername, PR_FALSE);
	prompt.AddEntry (_("_Password:"), *aPassword, PR_TRUE);
	prompt.AddCheckbox (aCheckMsg, aCheckState);

	prompt.Run (_retval);
	prompt.GetText (0, aUsername);
	prompt.GetText (1, aPassword);
	prompt.GetCheckboxState (aCheckState);

	return NS_OK;
}

/* boolean promptPassword (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, inout wstring aPassword, in wstring aCheckMsg, inout boolean aCheckState); */
NS_IMETHODIMP
EphyPromptService::PromptPassword (nsIDOMWindow *aParent,
				   const PRUnichar *aDialogTitle,
				   const PRUnichar *aText,
				   PRUnichar **aPassword,
				   const PRUnichar *aCheckMsg,
				   PRBool *aCheckState,
				   PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);
	NS_ENSURE_ARG_POINTER (aPassword);

	Prompter prompt (GTK_STOCK_DIALOG_AUTHENTICATION, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.AddEntry (_("_Password:"), *aPassword, PR_TRUE);
	prompt.AddCheckbox (aCheckMsg, aCheckState);

	// FIXME: Add a CAPSLOCK indicator?

	prompt.Run (_retval);
	prompt.GetText (0, aPassword);
	prompt.GetCheckboxState (aCheckState);

	return NS_OK;
}

/* boolean select (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText, in PRUint32 aCount, [array, size_is (aCount)] in wstring aSelectList, out long aOutSelection); */
NS_IMETHODIMP
EphyPromptService::Select (nsIDOMWindow *aParent,
			   const PRUnichar *aDialogTitle,
			   const PRUnichar *aText,
			   PRUint32 aCount,
			   const PRUnichar **aSelectList,
			   PRInt32 *aOutSelection,
			   PRBool *_retval)
{
	NS_ENSURE_ARG_POINTER (_retval);
	NS_ENSURE_ARG_POINTER (aOutSelection);

	Prompter prompt (GTK_STOCK_DIALOG_QUESTION, aParent, aDialogTitle, aText);
	prompt.AddStockButton (GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	prompt.AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt.AddSelect (aCount, aSelectList, *aOutSelection);

	prompt.Run (_retval);
	prompt.GetSelected (aOutSelection);

	return NS_OK;
}

#if HAVE_NSINONBLOCKINGALERTSERVICE_H

/* showNonBlockingAlert (in nsIDOMWindow aParent, in wstring aDialogTitle, in wstring aText); */
NS_IMETHODIMP
EphyPromptService::ShowNonBlockingAlert (nsIDOMWindow *aParent,
					 const PRUnichar *aDialogTitle,
					 const PRUnichar *aText)
{
	Prompter *prompt = new Prompter (GTK_STOCK_DIALOG_INFO, aParent, aDialogTitle, aText);
	if (!prompt) return NS_ERROR_OUT_OF_MEMORY;

	prompt->AddStockButton (GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	prompt->Show ();

	return NS_OK;
}

#endif /* HAVE_NSINONBLOCKINGALERTSERVICE_H */

#ifdef HAVE_GECKO_1_9

static void
NS_GetAuthHostPort(nsIChannel* aChannel, nsIAuthInformation* aAuthInfo,
                   PRBool machineProcessing, nsCString& host, PRInt32* port)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return;

  // Have to distinguish proxy auth and host auth here...
  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);
  if (flags & nsIAuthInformation::AUTH_PROXY) {
    nsCOMPtr<nsIProxiedChannel> proxied(do_QueryInterface(aChannel));
    NS_ASSERTION(proxied, "proxy auth needs nsIProxiedChannel");

    nsCOMPtr<nsIProxyInfo> info;
    proxied->GetProxyInfo(getter_AddRefs(info));
    NS_ASSERTION(info, "proxy auth needs nsIProxyInfo");

    nsCAutoString idnhost;
    info->GetHost(idnhost);
    info->GetPort(port);

    if (machineProcessing) {
      nsCOMPtr<nsIIDNService> idnService =
        do_GetService(NS_IDNSERVICE_CONTRACTID);
      if (idnService) {
        idnService->ConvertUTF8toACE(idnhost, host);
      } else {
        // Not much we can do here...
        host = idnhost;
      }
    } else {
      host = idnhost;
    }
  } else {
    if (machineProcessing) {
      uri->GetAsciiHost(host);
      *port = NS_GetRealPort(uri);
    } else {
      uri->GetHost(host);
      uri->GetPort(port);
    }
  }
}

static nsresult
MakeDialogText(nsIChannel* aChannel, nsIAuthInformation* aAuthInfo,
               nsString& message)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleSvc =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleSvc->CreateBundle("chrome://global/locale/prompts.properties",
                               getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // figure out what message to display...
  nsCAutoString host;
  PRInt32 port;
  NS_GetAuthHostPort(aChannel, aAuthInfo, PR_FALSE, host, &port);

  nsAutoString displayHost;
  CopyUTF8toUTF16(host, displayHost);

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  nsCAutoString scheme;
  uri->GetScheme(scheme);

  nsAutoString username;
  aAuthInfo->GetUsername(username);

  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);
  PRBool proxyAuth = (flags & nsIAuthInformation::AUTH_PROXY) != 0;

  nsAutoString realm;
  aAuthInfo->GetRealm(realm);

  // Append the port if it was specified
  if (port != -1) {
    displayHost.Append(PRUnichar(':'));
    displayHost.AppendInt(port);
  }

  NS_NAMED_LITERAL_STRING(proxyText, "EnterUserPasswordForProxy");
  NS_NAMED_LITERAL_STRING(originText, "EnterUserPasswordForRealm");
  NS_NAMED_LITERAL_STRING(noRealmText, "EnterUserPasswordFor");
  NS_NAMED_LITERAL_STRING(passwordText, "EnterPasswordFor");

  const PRUnichar *text;
  if (proxyAuth) {
    text = proxyText.get();
  } else {
    text = originText.get();

    // prepend "scheme://"
    nsAutoString schemeU; 
    CopyASCIItoUTF16(scheme, schemeU);
    schemeU.AppendLiteral("://");
    displayHost.Insert(schemeU, 0);
  }

  const PRUnichar *strings[] = { realm.get(), displayHost.get() };
  PRUint32 count = NS_ARRAY_LENGTH(strings);

  if (flags & nsIAuthInformation::ONLY_PASSWORD) {
    text = passwordText.get();
    strings[0] = username.get();
  } else if (!proxyAuth && realm.IsEmpty()) {
    text = noRealmText.get();
    count--;
    strings[0] = strings[1];
  }

  rv = bundle->FormatStringFromName(text, strings, count, getter_Copies(message));
  return rv;
}

/* static */ nsresult
EphyPromptService::PromptPasswordAdapter(nsIPromptService* aService,
					 nsIDOMWindow* aParent,
					 nsIChannel* aChannel,
					 PRUint32 aLevel,
					 nsIAuthInformation* aAuthInfo,
					 const PRUnichar* aCheckLabel,
					 PRBool* aCheckValue,
					 PRBool* retval)
{
  // construct the message string
  nsString message;
  MakeDialogText(aChannel, aAuthInfo, message);

  nsAutoString defaultUser, defaultDomain, defaultPass;
  aAuthInfo->GetUsername(defaultUser);
  aAuthInfo->GetDomain(defaultDomain);
  aAuthInfo->GetPassword(defaultPass);

  PRUint32 flags;
  aAuthInfo->GetFlags(&flags);

  if ((flags & nsIAuthInformation::NEED_DOMAIN) && !defaultDomain.IsEmpty()) {
    defaultDomain.Append(PRUnichar('\\'));
    defaultUser.Insert(defaultDomain, 0);
  }

  // NOTE: Allocation failure is not fatal here (just default to empty string
  // if allocation fails)
  PRUnichar *user = ToNewUnicode(defaultUser),
    *pass = ToNewUnicode(defaultPass);
  nsresult rv;
  if (flags & nsIAuthInformation::ONLY_PASSWORD)
    rv = aService->PromptPassword(aParent, nsnull, message.get(),
                                  &pass, aCheckLabel,
                                  aCheckValue, retval);
  else
    rv = aService->PromptUsernameAndPassword(aParent, nsnull, message.get(),
                                             &user, &pass, aCheckLabel,
                                             aCheckValue, retval);

  nsString userStr(user);
  nsString passStr(pass);
  aAuthInfo->SetUsername(userStr);
  aAuthInfo->SetPassword(passStr);

  return rv;
}

/* boolean promptAuth (in nsIDOMWindow aParent, in nsIChannel aChannel, in PRUint32 level, in nsIAuthInformation authInfo, in wstring checkboxLabel, inout boolean checkValue); */
NS_METHOD
EphyPromptService::PromptAuth(nsIDOMWindow *aParent,
			      nsIChannel *aChannel,
			      PRUint32 level,
			      nsIAuthInformation *authInfo,
			      const PRUnichar *checkboxLabel,
			      PRBool *checkValue,
			      PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER (_retval);
  NS_ENSURE_ARG_POINTER (authInfo);

  return EphyPromptService::PromptPasswordAdapter(this,
						  aParent,
						  aChannel,
						  level,
						  authInfo,
						  checkboxLabel,
						  checkValue,
						  _retval);
}

/* nsICancelable asyncPromptAuth (in nsIDOMWindow aParent, in nsIChannel aChannel, in nsIAuthPromptCallback aCallback, in nsISupports aContext, in PRUint32 level, in nsIAuthInformation authInfo, in wstring checkboxLabel, inout boolean checkValue); */
NS_METHOD EphyPromptService::AsyncPromptAuth(nsIDOMWindow *aParent,
					     nsIChannel *aChannel,
					     nsIAuthPromptCallback *aCallback,
					     nsISupports *aContext,
					     PRUint32 level,
					     nsIAuthInformation *authInfo,
					     const PRUnichar *checkboxLabel,
					     PRBool *checkValue,
					     nsICancelable **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#endif /* HAVE_GECKO_1_9 */
