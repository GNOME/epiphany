/*
 *  Copyright (C) 2004 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_UTILS_H
#define EPHY_UTILS_H

#include <nsError.h>
#include <gtk/gtkwidget.h>

class nsAString;
class nsACString;
class nsIDOMWindow;
class nsIIOService;
class nsIPrintSettings;
class nsIURI;
class nsIFile;
struct _EmbedPrintInfo;

namespace EphyUtils
{
	nsresult	GetIOService		(nsIIOService **ioService);

	nsresult	NewURI			(nsIURI **result,
						 const nsAString &spec,
						 const char *charset = nsnull,
						 nsIURI *baseURI = nsnull);

	nsresult	NewURI			(nsIURI **result,
						 const nsACString &spec,
						 const char *charset = nsnull,
						 nsIURI *baseURI = nsnull);

	nsresult	NewFileURI		(nsIURI **result,
						 nsIFile *spec);

	GtkWidget      *FindEmbed		(nsIDOMWindow *aDOMWindow);

	GtkWidget      *FindGtkParent		(nsIDOMWindow *aDOMWindow);

	nsresult        CollatePrintSettings	(_EmbedPrintInfo *info,
						 nsIPrintSettings *settings,
						 gboolean preview);
	PRBool          StringEquals            (nsAString &s1, 
						 nsAString &s2);
}

#endif
