/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"

#include "config.h"

#include "MozillaPrivate.h"

#include <nsIPrintSettingsService.h>
#include <nsIPrintOptions.h>
#include <nsIServiceManager.h>
#include <nsISimpleEnumerator.h>
#include <nsISupportsPrimitives.h>
#include <nsPromiseFlatString.h>

#ifdef HAVE_NSIWALLETSERVICE_H
#include <nsIPrefBranch.h>
#include <nsIDOMWindowInternal.h>
#include <wallet/nsIWalletService.h>
#endif

#include "ephy-debug.h"

/* IMPORTANT. Put only code that use internal mozilla strings (nsAutoString for
 * example) in this file. Note that you cannot use embed strings here,
 * the header inclusions will conflict.
 */

GList *
MozillaPrivate::GetPrinterList ()
{
	GList *printers = NULL;
	nsresult rv = NS_OK;

	nsCOMPtr<nsIPrintSettingsService> pss =
		do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
	NS_ENSURE_SUCCESS(rv, nsnull);

	nsCOMPtr<nsIPrintOptions> po = do_QueryInterface(pss, &rv);
	NS_ENSURE_SUCCESS(rv, nsnull);

	nsCOMPtr<nsISimpleEnumerator> avPrinters;
	rv = po->AvailablePrinters(getter_AddRefs(avPrinters));
	NS_ENSURE_SUCCESS(rv, nsnull);

	PRBool more = PR_FALSE;

	for (avPrinters->HasMoreElements(&more);
	     more == PR_TRUE;
	     avPrinters->HasMoreElements(&more))
	{
		nsCOMPtr<nsISupports> i;
		rv = avPrinters->GetNext(getter_AddRefs(i));
		NS_ENSURE_SUCCESS(rv, nsnull);

		nsCOMPtr<nsISupportsString> printer = do_QueryInterface(i, &rv);
		NS_ENSURE_SUCCESS(rv, nsnull);

		nsAutoString data;
		rv = printer->GetData(data);
		NS_ENSURE_SUCCESS(rv, nsnull);

		printers = g_list_prepend (printers, g_strdup (NS_ConvertUCS2toUTF8 (data).get()));
	}

	return g_list_reverse (printers);
}

#ifdef HAVE_NSIWALLETSERVICE_H

class DummyWindow : public nsIDOMWindowInternal
{
public:
	DummyWindow () { LOG ("DummyWindow ctor"); };
	virtual ~DummyWindow () { LOG ("DummyWindow dtor"); };

	NS_DECL_ISUPPORTS
	NS_FORWARD_SAFE_NSIDOMWINDOW(mFake);
	NS_FORWARD_SAFE_NSIDOMWINDOW2(mFake2);
	NS_FORWARD_SAFE_NSIDOMWINDOWINTERNAL(mFakeInt);
private:
	nsCOMPtr<nsIDOMWindow> mFake;
	nsCOMPtr<nsIDOMWindow2> mFake2;
	nsCOMPtr<nsIDOMWindowInternal> mFakeInt;
};

NS_IMPL_ISUPPORTS3(DummyWindow, nsIDOMWindow, nsIDOMWindow2, nsIDOMWindowInternal)

#endif /* HAVE_NSIWALLETSERVICE_H */

void
MozillaPrivate::SecureWallet (nsIPrefBranch *pref)
{
#ifdef HAVE_NSIWALLETSERVICE_H
	nsresult rv;
	PRBool isEnabled = PR_FALSE;
	rv = pref->GetBoolPref ("wallet.crypto", &isEnabled);
	if (NS_SUCCEEDED (rv) && isEnabled) return;
		
	nsCOMPtr<nsIWalletService> wallet (do_GetService (NS_WALLETSERVICE_CONTRACTID));
	NS_ENSURE_TRUE (wallet, );

	/* We cannot set nsnull as callback data here, since that will crash
	 * in case wallet needs to get the prompter from it (missing null check
	 * in wallet code). Therefore we create a dummy impl which will just
	 * always fail. There is no way to safely set nsnull after we're done,
	 * so we'll just leak our dummy window.
	 */
	DummyWindow *win = new DummyWindow();
	if (!win) return;

	nsCOMPtr<nsIDOMWindowInternal> domWinInt (do_QueryInterface (win));
	NS_ENSURE_TRUE (domWinInt, );

	/* WALLET_InitReencryptCallback doesN'T addref but stores the pointer! */
	NS_ADDREF (win);
	wallet->WALLET_InitReencryptCallback (domWinInt);

	/* Now set the pref. This will encrypt the existing data. */
	pref->SetBoolPref ("wallet.crypto", PR_TRUE);
#endif /* HAVE_NSIWALLETSERVICE_H */
}
