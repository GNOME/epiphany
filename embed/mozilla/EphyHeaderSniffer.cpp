/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt  <hyatt@netscape.com>
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "MozillaPrivate.h"

#include "MozDownload.h"
#include "EphyHeaderSniffer.h"
#include "netCore.h"

#include "nsReadableUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsIStringEnumerator.h"
#include "nsIPrefService.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDownload.h"

#include <bonobo/bonobo-i18n.h>

EphyHeaderSniffer::EphyHeaderSniffer (nsIWebBrowserPersist* aPersist, MozillaEmbedPersist *aEmbedPersist,
		nsIFile* aFile, nsIURI* aURL, nsIDOMDocument* aDocument, nsIInputStream* aPostData,
                PRBool aBypassCache)
: mPersist(aPersist)
, mEmbedPersist(aEmbedPersist)
, mTmpFile(aFile)
, mURL(aURL)
, mDocument(aDocument)
, mPostData(aPostData)
{
	mPrompt = do_GetService("@mozilla.org/embedcomp/prompt-service;1");
}

EphyHeaderSniffer::~EphyHeaderSniffer()
{
}

NS_IMPL_ISUPPORTS2(EphyHeaderSniffer, nsIWebProgressListener, nsIAuthPrompt)

NS_IMETHODIMP 
EphyHeaderSniffer::OnStateChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, 
                                  PRUint32 aStatus)
{  
	if (aStateFlags & nsIWebProgressListener::STATE_START)
	{
		/* be sure to keep it alive while we save since it owns
		   us as a listener and keep ourselves alive */
		nsCOMPtr<nsIWebBrowserPersist> kungFuDeathGrip(mPersist);
		nsCOMPtr<nsIWebProgressListener> kungFuSuicideGrip(this);
    
		nsresult rv;
		nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
		if (!channel) return rv;
		channel->GetContentType(mContentType);
    
		nsCOMPtr<nsIURI> origURI;
		channel->GetOriginalURI(getter_AddRefs(origURI));
    
		nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
		if (httpChannel)
		{
			httpChannel->GetResponseHeader(nsCAutoString("content-disposition"),
						       mContentDisposition);
		}
    
		mPersist->CancelSave();

		PRBool exists;
		mTmpFile->Exists(&exists);
		if (exists)
		{
			mTmpFile->Remove(PR_FALSE);
		}

		rv = PerformSave(origURI);
		if (NS_FAILED(rv))
		{
			/* FIXME put up some UI */
      
		}
	}

	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnProgressChange (nsIWebProgress *aWebProgress, 
                                     nsIRequest *aRequest, 
                                     PRInt32 aCurSelfProgress, 
                                     PRInt32 aMaxSelfProgress, 
                                     PRInt32 aCurTotalProgress, 
                                     PRInt32 aMaxTotalProgress)
{
	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnLocationChange (nsIWebProgress *aWebProgress, 
				     nsIRequest *aRequest, 
				     nsIURI *location)
{
	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnStatusChange (nsIWebProgress *aWebProgress, 
				   nsIRequest *aRequest, 
				   nsresult aStatus, 
				   const PRUnichar *aMessage)
{
	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnSecurityChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
	return NS_OK;
}

nsresult EphyHeaderSniffer::PerformSave (nsIURI* inOriginalURI)
{
	nsresult rv;
	EmbedPersistFlags flags;
	PRBool askDownloadDest;

	ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (mEmbedPersist), &flags);
	askDownloadDest = flags & EMBED_PERSIST_ASK_DESTINATION;
 
	nsAutoString defaultFileName;

	if (defaultFileName.IsEmpty() && !mContentDisposition.IsEmpty())
	{
		/* 1 Use the HTTP header suggestion. */
		PRInt32 index = mContentDisposition.Find("filename=");
		if (index >= 0)
		{
			/* Take the substring following the prefix. */
			index += 9;
			nsCAutoString filename;
			mContentDisposition.Right(filename, mContentDisposition.Length() - index);
			defaultFileName = NS_ConvertUTF8toUCS2(filename);
		}
	}
    
	if (defaultFileName.IsEmpty())
	{
		/* 2 For file URLs, use the file name. */

		nsCOMPtr<nsIURL> url(do_QueryInterface(mURL));
		if (url)
		{
			nsCAutoString fileNameCString;
			url->GetFileName(fileNameCString);
			defaultFileName = NS_ConvertUTF8toUCS2(fileNameCString);
		}
	}
    
	if (defaultFileName.IsEmpty() && mDocument)
	{
		/* 3 Use the title of the document. */

		nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
		if (htmlDoc)
		{
			htmlDoc->GetTitle(defaultFileName);
		}
	}
    
	if (defaultFileName.IsEmpty() && mURL)
	{
		/* 4 Use the host. */
		nsCAutoString hostName;
		mURL->GetHost(hostName);
		defaultFileName = NS_ConvertUTF8toUCS2(hostName);
	}
    
	/* 5 One last case to handle about:blank and other untitled pages. */
	if (defaultFileName.IsEmpty())
	{
		defaultFileName.AssignWithConversion(_("Untitled"));
	}
        
	/* Validate the file name to ensure legality. */
	for (PRUint32 i = 0; i < defaultFileName.Length(); i++)
	{
		if (defaultFileName[i] == ':' || defaultFileName[i] == '/')
		{
			defaultFileName.SetCharAt(i, PRUnichar(' '));
		}
	}

	if (askDownloadDest)
	{
		/* FIXME show the file selector here */
	}
	else
	{
		/* FIXME build path from download dir */
	}
             
        nsCOMPtr<nsILocalFile> destFile;
       	rv = NS_NewLocalFile(defaultFileName, PR_TRUE, getter_AddRefs(destFile)); 
        if (NS_FAILED(rv) || !destFile) return G_FAILED;

	return InitiateMozillaDownload (mDocument, mURL, destFile,
					mContentType.get(), inOriginalURI, mEmbedPersist,
					mBypassCache, mPostData);
}

NS_IMETHODIMP EphyHeaderSniffer::Prompt (const PRUnichar *dialogTitle, const PRUnichar *text,
				         const PRUnichar *passwordRealm, PRUint32 savePassword,
				         const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
	if (defaultText) *result = ToNewUnicode(nsDependentString(defaultText));

	return mPrompt->Prompt (nsnull, dialogTitle, text, result,
				nsnull, nsnull, _retval);
}                                                                                                                            

NS_IMETHODIMP EphyHeaderSniffer::PromptUsernameAndPassword (const PRUnichar *dialogTitle, const PRUnichar *text,
						            const PRUnichar *passwordRealm, PRUint32 savePassword,
						            PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
	*_retval = savePassword;

	return mPrompt->PromptUsernameAndPassword (nsnull, dialogTitle, text, user, pwd,
						   nsnull, nsnull, _retval);
}

NS_IMETHODIMP EphyHeaderSniffer::PromptPassword (const PRUnichar *dialogTitle, const PRUnichar *text,
					         const PRUnichar *passwordRealm, PRUint32 savePassword,
					         PRUnichar **pwd, PRBool *_retval)
{
	*_retval = savePassword;

	return mPrompt->PromptPassword (nsnull, dialogTitle, text, pwd,
					nsnull, nsnull, _retval);
}
