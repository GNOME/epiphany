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

#include "MozDownload.h"
#include "EphyHeaderSniffer.h"
#include "netCore.h"

#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsIStringEnumerator.h"
#include "nsIPrefService.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDownload.h"

const char* const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

EphyHeaderSniffer::EphyHeaderSniffer(nsIWebBrowserPersist* aPersist, MozillaEmbedPersist *aEmbedPersist,
		nsIFile* aFile, nsIURI* aURL, nsIDOMDocument* aDocument, nsIInputStream* aPostData,
                const nsAString& aSuggestedFilename, PRBool aBypassCache)
: mPersist(aPersist)
, mEmbedPersist(aEmbedPersist)
, mTmpFile(aFile)
, mURL(aURL)
, mDocument(aDocument)
, mPostData(aPostData)
, mDefaultFilename(aSuggestedFilename)
, mBypassCache(aBypassCache)
{
}

EphyHeaderSniffer::~EphyHeaderSniffer()
{
}

NS_IMPL_ISUPPORTS1(EphyHeaderSniffer, nsIWebProgressListener)

// Implementation of nsIWebProgressListener
/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
EphyHeaderSniffer::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, 
                                PRUint32 aStatus)
{  
  if (aStateFlags & nsIWebProgressListener::STATE_START)
  {
    nsCOMPtr<nsIWebBrowserPersist> kungFuDeathGrip(mPersist);   // be sure to keep it alive while we save
                                                                // since it owns us as a listener
    nsCOMPtr<nsIWebProgressListener> kungFuSuicideGrip(this);   // and keep ourselves alive
    
    nsresult rv;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
    if (!channel) return rv;
    channel->GetContentType(mContentType);
    
    nsCOMPtr<nsIURI> origURI;
    channel->GetOriginalURI(getter_AddRefs(origURI));
    
    // Get the content-disposition if we're an HTTP channel.
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel)
      httpChannel->GetResponseHeader(nsCAutoString("content-disposition"), mContentDisposition);
    
    mPersist->CancelSave();
    PRBool exists;
    mTmpFile->Exists(&exists);
    if (exists)
        mTmpFile->Remove(PR_FALSE);

    rv = PerformSave(origURI);
    if (NS_FAILED(rv))
    {
      // put up some UI
      
    }
  }
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
EphyHeaderSniffer::OnProgressChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           PRInt32 aCurSelfProgress, 
           PRInt32 aMaxSelfProgress, 
           PRInt32 aCurTotalProgress, 
           PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
EphyHeaderSniffer::OnLocationChange(nsIWebProgress *aWebProgress, 
           nsIRequest *aRequest, 
           nsIURI *location)
{
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
EphyHeaderSniffer::OnStatusChange(nsIWebProgress *aWebProgress, 
               nsIRequest *aRequest, 
               nsresult aStatus, 
               const PRUnichar *aMessage)
{
	return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
EphyHeaderSniffer::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  return NS_OK;
}

nsresult EphyHeaderSniffer::PerformSave(nsIURI* inOriginalURI)
{
	nsresult rv;

	PRBool isHTML = (mDocument && mContentType.Equals("text/html") ||
			 mContentType.Equals("text/xml") ||
			 mContentType.Equals("application/xhtml+xml"));
 
        nsCOMPtr<nsILocalFile> file;
       	rv = NS_NewLocalFile(mDefaultFilename, PR_TRUE, getter_AddRefs(file)); 
        if (NS_FAILED(rv) || !file) return G_FAILED;
  
	nsCOMPtr<nsISupports> sourceData;
	if (isHTML)
		sourceData = do_QueryInterface(mDocument);
	else
		sourceData = do_QueryInterface(mURL);

	return InitiateDownload(sourceData, file, inOriginalURI);
}

// inOriginalURI is always a URI. inSourceData can be an nsIURI or an nsIDOMDocument, depending
// on what we're saving. It's that way for nsIWebBrowserPersist.
nsresult EphyHeaderSniffer::InitiateDownload(nsISupports* inSourceData, nsILocalFile* inDestFile, nsIURI* inOriginalURI)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIWebBrowserPersist> webPersist = do_CreateInstance(persistContractID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIURI> sourceURI = do_QueryInterface(inSourceData);

  PRInt64 timeNow = PR_Now();
  
  nsAutoString fileDisplayName;
  inDestFile->GetLeafName(fileDisplayName);
  
  MozDownload *downloader = new MozDownload ();
  // dlListener attaches to its progress dialog here, which gains ownership
  rv = downloader->InitForEmbed (inOriginalURI, inDestFile, fileDisplayName.get(),
				 nsnull, timeNow, webPersist, mEmbedPersist);
  if (NS_FAILED(rv)) return rv;

  PRInt32 flags = nsIWebBrowserPersist::PERSIST_FLAGS_NO_CONVERSION | 
                  nsIWebBrowserPersist::PERSIST_FLAGS_REPLACE_EXISTING_FILES;
  if (mBypassCache)
    flags |= nsIWebBrowserPersist::PERSIST_FLAGS_BYPASS_CACHE;
  else
    flags |= nsIWebBrowserPersist::PERSIST_FLAGS_FROM_CACHE;

  webPersist->SetPersistFlags(flags);
    
  if (sourceURI)
  {
    rv = webPersist->SaveURI(sourceURI, nsnull, nsnull, mPostData, nsnull, inDestFile);
  }
  else
  {
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(inSourceData, &rv);
    if (!domDoc) return rv;  // should never happen
    
    PRInt32 encodingFlags = 0;
    nsCOMPtr<nsILocalFile> filesFolder;
    
    if (!mContentType.Equals("text/plain")) {
      // Create a local directory in the same dir as our file.  It
      // will hold our associated files.
      filesFolder = do_CreateInstance("@mozilla.org/file/local;1");
      nsAutoString unicodePath;
      inDestFile->GetPath(unicodePath);
      filesFolder->InitWithPath(unicodePath);
      
      nsAutoString leafName;
      filesFolder->GetLeafName(leafName);
      nsAutoString nameMinusExt(leafName);
      PRInt32 index = nameMinusExt.RFind(".");
      if (index >= 0)
          nameMinusExt.Left(nameMinusExt, index);
      nameMinusExt += NS_LITERAL_STRING(" Files"); // XXXdwh needs to be localizable!
      filesFolder->SetLeafName(nameMinusExt);
      PRBool exists = PR_FALSE;
      filesFolder->Exists(&exists);
      if (!exists) {
          rv = filesFolder->Create(nsILocalFile::DIRECTORY_TYPE, 0755);
          if (NS_FAILED(rv))
            return rv;
      }
    }
    else
    {
      encodingFlags |= nsIWebBrowserPersist::ENCODE_FLAGS_FORMATTED |
                        nsIWebBrowserPersist::ENCODE_FLAGS_ABSOLUTE_LINKS |
                        nsIWebBrowserPersist::ENCODE_FLAGS_NOFRAMES_CONTENT;
    }
    rv = webPersist->SaveDocument(domDoc, inDestFile, filesFolder, mContentType.get(), encodingFlags, 80);
  }
  
  return rv;
}
