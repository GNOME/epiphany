/*
 *  Copyright © 2005, 2006, 2008 Christian Persch
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
 */

#include "mozilla-config.h"
#include "config.h"

#include <glib/gi18n.h>

#include <nsStringAPI.h>

#include "EphyUtils.h"

#include "ephy-debug.h"
#include "ephy-embed.h"

#include "EphyLoginPrompter.h"

EphyLoginPrompter::EphyLoginPrompter ()
{
        LOG ("EphyLoginPrompter ctor (%p)", this);
}

EphyLoginPrompter::~EphyLoginPrompter()
{
}

NS_IMPL_ISUPPORTS1 (EphyLoginPrompter,
		    nsILoginManagerPrompter)

/* void init (in nsIDOMWindow aWindow); */
NS_IMETHODIMP EphyLoginPrompter::Init(nsIDOMWindow *aWindow)
{
  mWindow = aWindow;

  /* Ensure it's one of ours */
  GtkWidget *embed = EphyUtils::FindGtkParent (aWindow);
  NS_ENSURE_TRUE (embed, NS_ERROR_FAILURE);

  return NS_OK;
}

/* void promptToSavePassword (in nsILoginInfo aLogin); */
NS_IMETHODIMP EphyLoginPrompter::PromptToSavePassword(nsILoginInfo *aLogin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void promptToChangePassword (in nsILoginInfo aOldLogin, in nsILoginInfo aNewLogin); */
NS_IMETHODIMP EphyLoginPrompter::PromptToChangePassword(nsILoginInfo *aOldLogin, nsILoginInfo *aNewLogin)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void promptToChangePasswordWithUsernames ([array, size_is (count)] in nsILoginInfo logins, in PRUint32 count, in nsILoginInfo aNewLogin); */
NS_IMETHODIMP EphyLoginPrompter::PromptToChangePasswordWithUsernames(nsILoginInfo **logins, PRUint32 count, nsILoginInfo *aNewLogin)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

