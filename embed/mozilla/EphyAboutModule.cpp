/*
 *  Copyright (C) 2001 Matt Aubury, Philip Langdale
 *  Copyright (C) 2004 Crispin Flowerday
 *  Copyright (C) 2005 Christian Persch
 *  Copyright (C) 2005 Adam Hooper
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

#include <nsCOMPtr.h>
#include <nsIIOService.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>
#include <nsIChannel.h>
#include <nsIOutputStream.h>
#include <nsIInputStream.h>
#include <nsIStorageStream.h>
#include <nsIInputStreamChannel.h>
#include <nsIScriptSecurityManager.h>
#include <nsNetCID.h>
#include <nsString.h>
#include <nsEscape.h>
#include <nsAutoPtr.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "EphyAboutModule.h"
#include "EphyRedirectChannel.h"
#include "EphyUtils.h"

#include "ephy-debug.h"

#include <string.h>

EphyAboutModule::EphyAboutModule()
{
	LOG ("EphyAboutModule ctor [%p]\n", this);
}

EphyAboutModule::~EphyAboutModule()
{
	LOG ("EphyAboutModule dtor [%p]\n", this);
}

NS_IMPL_ISUPPORTS1 (EphyAboutModule, nsIAboutModule)

/* nsIChannel newChannel (in nsIURI aURI); */
NS_IMETHODIMP
EphyAboutModule::NewChannel(nsIURI *aURI,
			    nsIChannel **_retval)
{
	NS_ENSURE_ARG(aURI);

	nsCAutoString path;
	aURI->GetPath (path);

#ifdef HAVE_GECKO_1_8
	if (strncmp (path.get(), "neterror?", strlen ("neterror?")) == 0)
	{
		return CreateErrorPage (aURI, _retval);
	}
#endif

	if (strncmp (path.get (), "recover?", strlen ("recover?")) == 0)
	{
		return CreateRecoverPage (aURI, _retval);
	}

	if (strcmp (path.get (), "epiphany") == 0)
	{
		return Redirect (nsDependentCString ("file://" SHARE_DIR "/epiphany.xhtml"), _retval);
	}

	return NS_ERROR_ILLEGAL_VALUE;
}

/* private functions */

nsresult
EphyAboutModule::Redirect(const nsACString &aURL,
			  nsIChannel **_retval)
{
	nsresult rv;
	nsCOMPtr<nsIIOService> ioService;
	rv = EphyUtils::GetIOService (getter_AddRefs (ioService));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIChannel> tempChannel;
	rv = ioService->NewChannel(aURL, nsnull, nsnull, getter_AddRefs(tempChannel));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIURI> uri;
	rv = ioService->NewURI(aURL, nsnull, nsnull, getter_AddRefs(uri));
	NS_ENSURE_SUCCESS (rv, rv);

	tempChannel->SetOriginalURI (uri);

	nsCOMPtr<nsIScriptSecurityManager> securityManager = 
			do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIPrincipal> principal;
	rv = securityManager->GetCodebasePrincipal(uri, getter_AddRefs(principal));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = tempChannel->SetOwner(principal);
	NS_ENSURE_SUCCESS (rv, rv);

	NS_ADDREF(*_retval = tempChannel);
	return rv;
}

nsresult
EphyAboutModule::ParseURL(const char *aURL,
			  nsACString &aCode,
			  nsACString &aRawOriginURL,
			  nsACString &aOriginURL,
			  nsACString &aOriginCharset,
			  nsACString &aTitle)
{
	/* The page URL is of the form "about:neterror?e=<errorcode>&u=<URL>&c=<charset>&d=<description>" */
	const char *query = strstr (aURL, "?");
	if (!query) return NS_ERROR_FAILURE;
	
	/* skip the '?' */
	++query;
	
	char **params = g_strsplit (query, "&", -1);
	if (!params) return NS_ERROR_FAILURE;
	
	for (PRUint32 i = 0; params[i] != NULL; ++i)
	{
		char *param = params[i];
	
		if (strlen (param) <= 2) continue;
	
		switch (param[0])
		{
			case 'e':
				aCode.Assign (nsUnescape (param + 2));
				break;
			case 'u':
				aRawOriginURL.Assign (param + 2);
				aOriginURL.Assign (nsUnescape (param + 2));
				break;
			case 'c':
				aOriginCharset.Assign (nsUnescape (param + 2));
				break;
			/* The next one is not used in neterror but recover: */
			case 't':
				aTitle.Assign (nsUnescape (param + 2));
				break;
			case 'd':
				/* we don't need mozilla's description parameter */
			default:
				break;
		}
	}

	g_strfreev (params);

	return NS_OK;
}

#ifdef HAVE_GECKO_1_8
nsresult
EphyAboutModule::GetErrorMessage(nsIURI *aURI,
				 const char *aError,
				 char **aPrimary,
				 char **aSecondary,
				 char **aTertiary,
				 char **aLinkIntro)
{
	if (strcmp (aError, "protocolNotFound") == 0)
	{
		nsCAutoString scheme;
		aURI->GetScheme (scheme);

		/* Translators: %s is the name of a protocol, like "http" etc. */
		*aPrimary = g_strdup_printf (_("“%s” protocol is not supported."), scheme.get());
		/* FIXME: get the list of supported protocols from necko */
		*aSecondary = _("Supported protocols are “http”, “https”, “ftp”, “file”, “smb” "
				"and “sftp”.");
	}
	else if (strcmp (aError, "fileNotFound") == 0)
	{
		nsCAutoString path;
		aURI->GetPath (path);

		/* Translators: %s is the path and filename, for example "/home/user/test.html" */
		*aPrimary = g_markup_printf_escaped (_("File “%s” not found."), path.get());
		*aSecondary = _("Check the location of the file and try again.");
	}
	else if (strcmp (aError, "dnsNotFound") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped (_("“%s” could not be found."),
						     host.get());
		*aSecondary = _("Check that you are connected to the internet, and "
				"that the address is correct.");
		*aLinkIntro = _("If this page used to exist, you may find an archived version:");
	}
	else if (strcmp (aError, "connectionFailure") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” refused the connection."),
				 host.get());
		*aSecondary = _("The server may be busy or you may have a "
				"network connection problem. Try again later.");
		*aLinkIntro = _("There may be an old version of the page you wanted:");
	}
	else if (strcmp (aError, "netInterrupt") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” interrupted the connection."),
				 host.get());
		*aSecondary = _("The server may be busy or you may have a "
				"network connection problem. Try again later.");
		*aLinkIntro = _("There may be an old version of the page you wanted:");
	}
	else if (strcmp (aError, "netTimeout") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” is not responding."),
				 host.get());
		*aSecondary = _("The connection was lost because the "
				"server took too long to respond.");
		*aTertiary = _("The server may be busy or you may have a network "
			        "connection problem. Try again later.");
		*aLinkIntro = _("There may be an old version of the page you wanted:");
	}
	else if (strcmp (aError, "malformedURI") == 0)
	{
		*aPrimary = g_strdup (_("Invalid address."));
		*aSecondary = g_strdup (_("The address you entered is not valid."));
	}
	else if (strcmp (aError, "redirectLoop") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” redirected too many times."),
				 host.get());
		*aSecondary = _("The redirection has been stopped for security reasons.");
		*aLinkIntro = _("There may be an old version of the page you wanted:");
	}
	else if (strcmp (aError, "unknownSocketType") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” requires an encrypted connection."),
				 host.get());
		*aSecondary = _("The document could not be loaded because "
				"encryption support is not installed.");
	}
	else if (strcmp (aError, "netReset") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” dropped the connection."),
				 host.get());
		*aSecondary = _("The server dropped the connection "
				 "before any data could be read.");
		*aTertiary = _("The server may be busy or you may have a "
			       "network connection problem. Try again later.");
		*aLinkIntro = _("There may be an old version of the page you wanted:");
	}
	else if (strcmp (aError, "netOffline") == 0)
	{
		*aPrimary = g_strdup (_("Cannot load document in offline mode."));
		*aSecondary = _("This document cannot be viewed in offline "
				"mode. Set Epiphany to “online” "
				"and try again.");
	}
	else if (strcmp (aError, "deniedPortAccess") == 0)
	{
		nsCAutoString host;
		aURI->GetHost (host);

		PRInt32 port = -1;
		aURI->GetPort (&port);

		/* Translators: %s is the hostname, like "www.example.com" */
		*aPrimary = g_markup_printf_escaped
				(_("“%s” denied access to port “%d”."),
				 host.get(), port > 0 ? port : 80);
		*aSecondary = g_strdup (_("The server dropped the connection "
					  "before any data could be read."));
		*aTertiary = _("The server may be busy or you may have a "
			       "network connection problem. Try again later.");
		*aLinkIntro = _("There may be an old version of the page you wanted:");
	}
	else if (strcmp (aError, "proxyResolveFailure") == 0 ||
		 strcmp (aError, "proxyConnectFailure") == 0)
	{
		*aPrimary = g_strdup (_("Could not connect to proxy server."));
		*aSecondary = _("Check your proxy server settings. "
				"If the connection still fails, there may be "
				"a problem with your proxy server or your "
				"network connection.");
	}
	else if (strcmp (aError, "contentEncodingError") == 0)
	{
		*aPrimary = g_strdup (_("Could not display content."));
		*aSecondary = _("The page uses an unsupported or invalid form of compression.");
	}
	else
	{
		return NS_ERROR_ILLEGAL_VALUE;
	}

	return NS_OK;
}

nsresult
EphyAboutModule::CreateErrorPage(nsIURI *aErrorURI,
				 nsIChannel **_retval)
{
        /* First parse the arguments */
	nsresult rv;
        nsCAutoString spec;
	rv = aErrorURI->GetSpec (spec);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv), rv);

	nsCAutoString error, rawurl, url, charset, dummy;
	rv = ParseURL (spec.get (), error, rawurl, url, charset, dummy);
	if (NS_FAILED (rv)) return rv;
	if (error.IsEmpty () || rawurl.IsEmpty () || url.IsEmpty()) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIURI> uri;
	rv = EphyUtils::NewURI(getter_AddRefs (uri), url, charset.get());
	/* FIXME can uri be NULL if the original url was invalid? */
	NS_ENSURE_SUCCESS (rv, rv);

	char *primary = nsnull, *secondary = nsnull, *tertiary = nsnull, *linkintro = nsnull;
	rv = GetErrorMessage (uri, error.get(), &primary, &secondary, &tertiary, &linkintro);

	/* we don't know about this error code.
	 * FIXME: We'd like to forward to mozilla's about:neterror handler,
	 * but I don't know how to. So just redirect to the same page that
	 * mozilla's handler redirects to.
	 */
	if (rv == NS_ERROR_ILLEGAL_VALUE)
	{
		nsCAutoString newurl(spec);

		/* remove "about:neterror" part and insert mozilla's error page url */
		newurl.Cut(0, strlen ("about:neterror"));
		newurl.Insert("chrome://global/content/netError.xhtml", 0);

		return Redirect (newurl, _retval);
	}
	NS_ENSURE_SUCCESS (rv, rv);
	NS_ENSURE_TRUE (primary && secondary, NS_ERROR_FAILURE);

	nsCOMPtr<nsIInputStreamChannel> channel;
	rv = WritePage (aErrorURI, uri, rawurl, primary /* as title */, GTK_STOCK_DIALOG_ERROR, primary, secondary, tertiary, linkintro, getter_AddRefs (channel));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = channel->SetURI (aErrorURI);
	NS_ENSURE_SUCCESS (rv, rv);

	g_free (primary);

	return CallQueryInterface (channel, _retval);
}
#endif /* HAVE_GECKO_1_8 */

nsresult
EphyAboutModule::CreateRecoverPage(nsIURI *aRecoverURI,
				   nsIChannel **_retval)
{
        /* First parse the arguments */
	nsresult rv;
        nsCAutoString spec;
	rv = aRecoverURI->GetSpec (spec);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv), rv);

	nsCAutoString error, rawurl, url, charset, title;
	rv = ParseURL (spec.get (), error, rawurl, url, charset, title);
	if (NS_FAILED (rv)) return rv;
	if (rawurl.IsEmpty () || url.IsEmpty()) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIURI> uri;
	rv = EphyUtils::NewURI(getter_AddRefs (uri), url, charset.get());
	NS_ENSURE_SUCCESS (rv, rv);

	char *secondary = g_strdup_printf
		(_("The page “%s” in this tab was not fully loaded yet when "
		   "the web browser crashed; it could have caused the crash."),
		 url.get());

	nsCOMPtr<nsIInputStreamChannel> ischannel;
	rv = WritePage (aRecoverURI, uri, rawurl, title.get(),
			GTK_STOCK_DIALOG_INFO, title.get() /* as primary */,
			secondary, nsnull, nsnull, getter_AddRefs (ischannel));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = ischannel->SetURI (uri);
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIChannel> channel (do_QueryInterface (ischannel, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	nsRefPtr<EphyRedirectChannel> redirectChannel (new EphyRedirectChannel (channel));
	if (!redirectChannel) return NS_ERROR_OUT_OF_MEMORY;

	g_free (secondary);

	NS_ADDREF(*_retval = redirectChannel);

	return NS_OK;
}

nsresult
EphyAboutModule::WritePage(nsIURI *aOriginalURI,
			   nsIURI *aURI,
			   const nsACString &aRawURL,
			   const char *aTitle,
			   const char *aStockIcon,
			   const char *aPrimary,
			   const char *aSecondary,
			   const char *aTertiary,
			   const char *aLinkIntro,
			   nsIInputStreamChannel **_retval)
{
	nsresult rv;
	nsCOMPtr<nsIStorageStream> storageStream;
	rv = NS_NewStorageStream (16384, (PRUint32) -1, getter_AddRefs (storageStream));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIOutputStream> stream;
	rv = storageStream->GetOutputStream (0, getter_AddRefs (stream));
	NS_ENSURE_SUCCESS (rv, rv);

	char *language = g_strdup (pango_language_to_string (gtk_get_default_language ()));
	g_strdelimit (language, "_-@", '\0');

	Write (stream,
	       "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	       "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
	       "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
	       "<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"");
	Write (stream, language);
	Write (stream,
	       "\" xml:lang=\"");
	Write (stream, language);
	Write (stream,
	       "\">\n"
	       "<head>\n"
		"<title>");
	Write (stream, aTitle);
	/* no favicon for now, it would pollute the favicon cache */
	/* "<link rel=\"icon\" type=\"image/png\" href=\"moz-icon://stock/gtk-dialog-error?size=16\" />\n" */
	Write (stream,
		"</title>\n"
		"<style type=\"text/css\">\n"
		"div#body {\n"
			"top: 12px;\n"
			"right: 12px;\n"
			"bottom: 12px;\n"
			"left: 12px;\n"
			"overflow: auto;\n"

			"background: -moz-dialog url('moz-icon://stock/");
	Write (stream, aStockIcon);
	Write (stream,
			"?size=dialog') no-repeat 12px 12px;\n"
			"color: -moz-dialogtext;\n"
			"font: message-box;\n"
			"border: 1px solid -moz-dialogtext;\n"

			"padding: 12px 12px 12px 72px;\n"
		"}\n"

		"h1 {\n"
			"margin: 0;\n"
			"font-size: 1.2em;\n"
		"}\n"
		"</style>\n"
	       "</head>\n"
	       "<body dir=\"");
	Write (stream,
	       gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL ? "rtl" : "ltr");
	Write (stream,
	       "\">\n"
		"<div id=\"body\">"
		"<h1>");
	Write (stream, aTitle);
	Write (stream,
	       "</h1>\n");
	if (aSecondary)
	{
		Write (stream, "<p>");
		Write (stream, aSecondary);
		if (aTertiary)
		{
			Write (stream, " ");
			Write (stream, aTertiary);
		}
		Write (stream, "</p>\n");
	}

	PRBool isHttp = PR_FALSE, isHttps = PR_FALSE;
	aURI->SchemeIs ("http", &isHttp);
	aURI->SchemeIs ("https", &isHttps);
	if (aLinkIntro && (isHttp || isHttps))
	{
		nsCString raw(aRawURL);

		Write (stream, "<p>");
		Write (stream, aLinkIntro);
		Write (stream, "<ul>\n");
		Write (stream, "<li><a href=\"http://www.google.com/search?q=cache:");
		Write (stream, raw.get());
		Write (stream, "\">");
		/* Translators: The text before the "|" is context to help you decide on
		 * the correct translation. You MUST OMIT it in the translated string. */
		Write (stream, Q_("You may find an old version:|in the Google Cache"));
		Write (stream, "</a></li>\n");

		Write (stream, "<li><a href=\"http://web.archive.org/web/*/");
		Write (stream, raw.get());
		Write (stream, "\">");
		/* Translators: The text before the "|" is context to help you decide on
		 * the correct translation. You MUST OMIT it in the translated string. */
		Write (stream, Q_("You may find an old version:|in the Internet Archive"));
		Write (stream, "</a></li>\n"
			       "</ul>\n"
			       "</p>");
	}

	Write (stream,
		"</div>\n"
	       "</body>\n"
	       "</html>\n");

	g_free (language);
 
	/* finish the rendering */
	nsCOMPtr<nsIInputStream> inputStream;
	rv = storageStream->NewInputStream (0, getter_AddRefs (inputStream));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIInputStreamChannel> channel (do_CreateInstance ("@mozilla.org/network/input-stream-channel;1", &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	rv |= channel->SetOriginalURI (aOriginalURI);
	rv |= channel->SetContentStream (inputStream);
	rv |= channel->SetContentType (NS_LITERAL_CSTRING ("application/xhtml+xml"));
	rv |= channel->SetContentCharset (NS_LITERAL_CSTRING ("utf-8"));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIScriptSecurityManager> securityManager 
		(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIPrincipal> principal;
	rv = securityManager->GetCodebasePrincipal (aOriginalURI, getter_AddRefs (principal));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = channel->SetOwner(principal);
	NS_ENSURE_SUCCESS (rv, rv);

	NS_ADDREF (*_retval = channel);

	return rv;
}

nsresult
EphyAboutModule::Write(nsIOutputStream *aStream,
		       const char *aText)
{
	PRUint32 bytesWritten;
	return aStream->Write (aText, strlen (aText), &bytesWritten);
}
