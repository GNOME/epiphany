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

#ifndef GECKO_PRINT_SERVICE_H
#define GECKO_PRINT_SERVICE_H

#include <gtk/gtkpagesetup.h>
#include <gtk/gtkprintsettings.h>
#include <gtk/gtkprinter.h>

#include <nsIPrintingPromptService.h>

class nsIPrintSettings;

/* 6a71ff30-7f4d-4d91-b71a-d5c9764b34be */
#define GECKO_PRINT_SERVICE_IID \
{ 0x6a71ff30, 0x7f4d, 0x4d91, \
  { 0xb7, 0x1a, 0xd5, 0xc9, 0x76, 0x4b, 0x34, 0xbe } }

#define GECKO_PRINT_SERVICE_CLASSNAME "Gecko Print Service"

class GeckoPrintService : public nsIPrintingPromptService
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPRINTINGPROMPTSERVICE

	GeckoPrintService();
	virtual ~GeckoPrintService();

	static nsresult TranslateSettings (GtkPrintSettings*, GtkPageSetup *, GtkPrinter *, const nsACString&, PRInt16, PRBool, nsIPrintSettings*);

private:
	nsresult PrintUnattended (nsIDOMWindow *, nsIPrintSettings *);
};

#endif /* GECKO_PRINT_SERVICE_H */
