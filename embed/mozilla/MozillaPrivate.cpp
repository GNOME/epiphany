/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "MozillaPrivate.h"

#include <nsIPrintSettingsService.h>
#include <nsIPrintOptions.h>
#include <nsIServiceManager.h>
#include <nsISimpleEnumerator.h>
#include <nsISupportsPrimitives.h>
#include <nsPromiseFlatString.h>

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
