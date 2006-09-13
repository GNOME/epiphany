/* 
 * Copied from nsWindowWatcher.cpp.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright © 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Harshal Pradhan <keeda@hotpop.com>
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
 * ***** END LICENSE BLOCK *****
 *
 * $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include "AutoEventQueue.h"

#ifdef HAVE_LAME_APPSHELL
/* for NS_APPSHELL_CID */
#include <nsWidgetsCID.h>
#endif
#include <nsIServiceManager.h>
#include <nsIServiceManagerUtils.h>

#ifdef HAVE_LAME_APPSHELL
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
#endif

AutoEventQueue::AutoEventQueue() : mQueue(nsnull)
{
}

AutoEventQueue::~AutoEventQueue()
{
#ifdef HAVE_LAME_APPSHELL
  if (mAppShell) {
    if (mQueue)
      mAppShell->ListenToEventQueue(mQueue, PR_FALSE);
    mAppShell->Spindown();
    mAppShell = nsnull;
  }
#endif

  if(mQueue)
    mService->PopThreadEventQueue(mQueue);
}

nsresult AutoEventQueue::Init()
{
  if (mQueue)
    return NS_ERROR_ALREADY_INITIALIZED;

  mService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
  if (!mService)
    return NS_ERROR_FAILURE;

  /* push a new queue onto it */
  mService->PushThreadEventQueue(getter_AddRefs(mQueue));
  if (!mQueue)
    return NS_ERROR_FAILURE;

#ifdef HAVE_LAME_APPSHELL
  mAppShell = do_CreateInstance(kAppShellCID);
  if (!mAppShell)
    return NS_ERROR_FAILURE;

  mAppShell->Create(0, nsnull);
  mAppShell->Spinup();

  /* listen to the new queue */
  mAppShell->ListenToEventQueue(mQueue, PR_TRUE);
#endif

  return NS_OK;
}
