/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "EventContext.h"
#include "nsString.h"
#include "nsIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIDOMNSDocument.h"
#include "nsReadableUtils.h"
#include "nsGUIEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include <gdk/gdkkeysyms.h>
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIServiceManager.h"
#include "nsITextToSubURI.h"
#include "nsIDocCharset.h"

#define KEY_CODE 256

EventContext::EventContext ()
{
}

EventContext::~EventContext ()
{
}

nsresult EventContext::Init (EphyBrowser *browser)
{
	mBrowser = browser;
	mDOMDocument = nsnull;

	return NS_OK;
}

nsresult EventContext::GatherTextUnder (nsIDOMNode* aNode, nsString& aResult)
{
	nsAutoString text;
	nsCOMPtr<nsIDOMNode> node;
	aNode->GetFirstChild(getter_AddRefs(node));
	PRUint32 depth = 1;

	while (node && depth)
	{
		nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(node));
		PRUint16 nodeType;

		node->GetNodeType(&nodeType);
		if (charData && nodeType == nsIDOMNode::TEXT_NODE)
		{
			/* Add this text to our collection. */
			text += NS_LITERAL_STRING(" ");
			nsAutoString data;
			charData->GetData(data);
			text += data;
		}
		else
		{
			nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(node));
			if (img)
			{
				nsAutoString altText;
				img->GetAlt(altText);
				if (!altText.IsEmpty())
				{
					text = altText;
					break;
				}
			}
		}

		/* Find the next node to test. */
		PRBool hasChildNodes;
		node->HasChildNodes(&hasChildNodes);
		if (hasChildNodes)
		{
			nsCOMPtr<nsIDOMNode> temp = node;
			temp->GetFirstChild(getter_AddRefs(node));
			depth++;
		}
		else
		{
			nsCOMPtr<nsIDOMNode> nextSibling;
			node->GetNextSibling(getter_AddRefs(nextSibling));
			if (nextSibling)
			{
				node = nextSibling;
			}
			else
			{
				nsCOMPtr<nsIDOMNode> parentNode;
				node->GetParentNode(getter_AddRefs(parentNode));
				if (!parentNode)
				{
					node = nsnull;
				}
				else
				{
					nsCOMPtr<nsIDOMNode> nextSibling;
					parentNode->GetNextSibling(getter_AddRefs(nextSibling));
					node = nextSibling;
					depth--;
				}
			}
		}
	}

	text.CompressWhitespace();
	aResult = text;

	return NS_OK;
}

nsresult EventContext::Unescape (const nsAString &aEscaped, nsAString &aUnescaped)
{
#if MOZILLA_SNAPSHOT >= 11
        if (!aEscaped.Length()) return NS_ERROR_FAILURE;

	NS_ENSURE_TRUE (mBrowser, NS_ERROR_FAILURE);
	NS_ENSURE_TRUE (mBrowser->mWebBrowser, NS_ERROR_FAILURE);

        nsCOMPtr<nsITextToSubURI> escaper
                (do_CreateInstance ("@mozilla.org/intl/texttosuburi;1"));
        NS_ENSURE_TRUE (escaper, NS_ERROR_FAILURE);

        nsCOMPtr<nsIDocCharset> docCharset = do_GetInterface (mBrowser->mWebBrowser);
        NS_ENSURE_TRUE (docCharset, NS_ERROR_FAILURE);

	nsCAutoString encoding;
        char *charset;
        nsresult rv;
        rv = docCharset->GetCharset (&charset);
        NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

        encoding = charset;
        if (charset) nsMemory::Free (charset);

	NS_ConvertUCS2toUTF8 escaped (aEscaped);
        return escaper->UnEscapeNonAsciiURI (encoding, escaped, aUnescaped);
#else
	aUnescaped = aEscaped;
        return NS_OK;
#endif
}

nsresult EventContext::ResolveBaseURL (nsIDocument *doc, const nsAString &relurl, nsACString &url)
{
#if MOZILLA_SNAPSHOT > 13
	nsIURI *base;
	base = doc->GetBaseURI ();
	if (!base) return NS_ERROR_FAILURE;
#elif MOZILLA_SNAPSHOT > 11
	nsIURI *base;
	base = doc->GetBaseURL ();
	if (!base) return NS_ERROR_FAILURE;
#elif MOZILLA_SNAPSHOT > 9 
	nsCOMPtr<nsIURI> base;
	rv = doc->GetBaseURL (getter_AddRefs(base));
	if (NS_FAILED(rv)) return rv;
#else
	nsCOMPtr<nsIURI> base;
	rv = doc->GetBaseURL (*getter_AddRefs(base));
	if (NS_FAILED(rv)) return rv;
#endif

	return base->Resolve (NS_ConvertUCS2toUTF8(relurl), url);
}

nsresult EventContext::ResolveDocumentURL (nsIDocument *doc, const nsAString &relurl, nsACString &url)
{
#if MOZILLA_SNAPSHOT > 13
	nsIURI *uri;
	uri = doc->GetDocumentURI ();
	if (!uri) return NS_ERROR_FAILURE;
#elif MOZILLA_SNAPSHOT > 11
	nsIURI *uri;
	uri = doc->GetDocumentURL ();
	if (!uri) return NS_ERROR_FAILURE;
#else
	nsCOMPtr<nsIURI> uri;
	rv = doc->GetDocumentURL(getter_AddRefs(uri));
	if (NS_FAILED(rv)) return rv;
#endif

	return uri->Resolve (NS_ConvertUCS2toUTF8(relurl), url);
}

nsresult EventContext::GetEventContext (nsIDOMEventTarget *EventTarget,
					MozillaEmbedEvent *info)
{
	nsresult rv;

	mEmbedEvent = info;

	info->context = EMBED_CONTEXT_DOCUMENT;

	nsCOMPtr<nsIDOMNode> node = do_QueryInterface(EventTarget, &rv);
	if (NS_FAILED(rv) || !node) return NS_ERROR_FAILURE;

        /* Is page xul ? then do not display context menus
	 * FIXME I guess there is an easier way ... */
	/* From philipl: This test needs to be here otherwise we
	 * arrogantly assume we can QI to a HTMLElement, which is
	 * not true for xul content. */ 

	nsCOMPtr<nsIDOMDocument> domDoc;
	rv = node->GetOwnerDocument(getter_AddRefs(domDoc));
	if (NS_FAILED(rv) || !domDoc) return NS_ERROR_FAILURE;

	mDOMDocument = domDoc;

	nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc, &rv);
	if (NS_FAILED(rv) || !doc) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMXULDocument> xul_document = do_QueryInterface(domDoc);
	if (xul_document)
	{
		info->context = EMBED_CONTEXT_NONE;
		return NS_ERROR_FAILURE;
	}

	// Now we know that the page isn't a xul window, we can try and
	// do something useful with it.

	PRUint16 type;
	rv = node->GetNodeType(&type);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(node);
	if ((nsIDOMNode::ELEMENT_NODE == type) && element)
	{
		nsAutoString tag;
		rv = element->GetTagName(tag);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		if (tag.EqualsIgnoreCase("img"))
		{
			info->context |= EMBED_CONTEXT_IMAGE;

			nsAutoString img;
			nsCOMPtr <nsIDOMHTMLImageElement> image = 
						do_QueryInterface(node, &rv);
			if (NS_FAILED(rv) || !image) return NS_ERROR_FAILURE;			

			rv = image->GetSrc (img);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
			SetStringProperty ("image", img);

			rv = image->GetAlt (img);
			if (NS_SUCCEEDED(rv))
			{
				SetStringProperty ("image_alt", img);	
			}

			rv = image->GetLongDesc (img);
			if (NS_SUCCEEDED(rv) && !img.IsEmpty())
			{
				nsCAutoString imglongdesc;
                                rv = ResolveDocumentURL (doc, img, imglongdesc);
                                                                                                                
                                SetStringProperty ("image_long_desc",
                                                   NS_ConvertUTF8toUCS2(imglongdesc));
			}

			int imgwidth, imgheight;
			rv = image->GetWidth (&imgwidth);
			rv = image->GetHeight (&imgheight);
			SetIntProperty ("image_width", imgwidth);
			SetIntProperty ("image_height", imgheight);

			rv = element->GetTitle (img);
                        if (NS_SUCCEEDED(rv))
			{
				SetStringProperty ("image_title",
						   img);
			}
		}
		else if (tag.EqualsIgnoreCase("input"))
		{
			nsCOMPtr<nsIDOMElement> element;
			element = do_QueryInterface (node);
			if (!element) return NS_ERROR_FAILURE;

			NS_NAMED_LITERAL_STRING(attr, "type");
			nsAutoString value;
			element->GetAttribute (attr, value);

			if (value.EqualsIgnoreCase("image"))
			{
				info->context |= EMBED_CONTEXT_IMAGE;
				nsCOMPtr<nsIDOMHTMLInputElement> input;
				input = do_QueryInterface (node);
				if (!input) return NS_ERROR_FAILURE;

				nsAutoString img;
				rv = input->GetSrc (img);
				if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

				nsCAutoString cImg;
				rv = ResolveDocumentURL (doc, img, cImg);
                                if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
				SetStringProperty ("image",
						   NS_ConvertUTF8toUCS2(cImg));
			}
			else if (!value.EqualsIgnoreCase("radio")  &&
				 !value.EqualsIgnoreCase("submit") &&
				 !value.EqualsIgnoreCase("reset")  &&
				 !value.EqualsIgnoreCase("hidden") &&
				 !value.EqualsIgnoreCase("button") &&
				 !value.EqualsIgnoreCase("checkbox"))
			{
				info->context |= EMBED_CONTEXT_INPUT;
			}
		}
		else if (tag.EqualsIgnoreCase("textarea"))
		{
			info->context |= EMBED_CONTEXT_INPUT;
		}
		else if (tag.EqualsIgnoreCase("object"))
		{
			nsCOMPtr<nsIDOMHTMLObjectElement> object;
			object = do_QueryInterface (node);
			if (!element) return NS_ERROR_FAILURE;

			nsAutoString value;
			object->GetType(value);

			// MIME types are always lower case
			if (Substring (value, 0, 6).Equals(NS_LITERAL_STRING("image/")))
			{
				info->context |= EMBED_CONTEXT_IMAGE;
				
				nsAutoString img;
				
				rv = object->GetData (img);
				if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
				
				nsCAutoString cImg;
				rv = ResolveDocumentURL (doc, img, cImg);
                                if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

				SetStringProperty ("image", cImg.get());
			}
			else
			{
				info->context = EMBED_CONTEXT_NONE;
				return NS_OK;
			}
		}
	}

	/* Is page framed ? */
	PRBool framed;
	IsPageFramed (node, &framed);
	SetIntProperty ("framed_page", framed);

	/* Bubble out, looking for items of interest */
	while (node)
	{
		nsCOMPtr <nsIDOMElement> dom_elem = do_QueryInterface(node);
		if (dom_elem)
		{
			NS_NAMED_LITERAL_STRING(nspace, "http://www.w3.org/1999/xlink");
			NS_NAMED_LITERAL_STRING(localname_type, "type");

			nsAutoString value;
			dom_elem->GetAttributeNS (nspace, localname_type, value);

			if (value.EqualsIgnoreCase("simple"))
			{
				info->context |= EMBED_CONTEXT_LINK;
				NS_NAMED_LITERAL_STRING (localname_href, "href");
				dom_elem->GetAttributeNS (nspace, localname_href, value);
				
				SetStringProperty ("link", value);
				CheckLinkScheme (value);
			}
		}

		PRUint16 type;
		rv = node->GetNodeType(&type);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		element = do_QueryInterface(node);
		if ((nsIDOMNode::ELEMENT_NODE == type) && element)
		{
			nsAutoString tag;
			rv = element->GetTagName(tag);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

			/* Link */
			if (tag.EqualsIgnoreCase("a"))
			{
				nsAutoString tmp;
				nsAutoString substr;

				rv = GatherTextUnder (node, tmp);
				if (NS_SUCCEEDED(rv))
                                	SetStringProperty ("linktext", tmp);

				nsCOMPtr <nsIDOMHTMLAnchorElement> anchor =
					do_QueryInterface(node);

				anchor->GetHref (tmp);
				substr.Assign (Substring (tmp, 0, 7));
				if (substr.EqualsIgnoreCase("mailto:"))
				{
					const nsAString &address = Substring(tmp, 7, tmp.Length()-7);

					nsAutoString unescapedHref;
					rv = Unescape (address, unescapedHref);
					if (NS_SUCCEEDED (rv) && unescapedHref.Length())
					{
						SetStringProperty ("email", unescapedHref);
						info->context |= EMBED_CONTEXT_EMAIL_LINK;
					}
				}
				
				if (anchor && !tmp.IsEmpty()) 
				{
					info->context |= EMBED_CONTEXT_LINK;

					SetStringProperty ("link", tmp);
					CheckLinkScheme (tmp);
					rv = anchor->GetHreflang (tmp);
					if (NS_SUCCEEDED(rv))
						SetStringProperty ("link_lang", tmp);
					rv = anchor->GetTarget (tmp);
					if (NS_SUCCEEDED(rv))
						SetStringProperty ("link_target", tmp);
					rv = anchor->GetRel (tmp);
					if (NS_SUCCEEDED(rv))
						SetStringProperty ("link_rel", tmp);
					rv = anchor->GetRev (tmp);
					if (NS_SUCCEEDED(rv))
						SetStringProperty ("link_rev", tmp);
					rv = element->GetTitle (tmp);
		                        if (NS_SUCCEEDED(rv))
						SetStringProperty ("link_title", tmp);
					rv = anchor->GetType (tmp);
					if (NS_SUCCEEDED(rv))
						SetStringProperty ("link_type", tmp);

					if (tmp.EqualsIgnoreCase("text/smartbookmark"))
					{
						SetIntProperty ("link_is_smart", TRUE);
						
						nsCOMPtr<nsIDOMNode> childNode;
						node->GetFirstChild (getter_AddRefs(childNode));
						if (childNode)
						{
							nsCOMPtr <nsIDOMHTMLImageElement> image = 
								do_QueryInterface(childNode, &rv);

							if (image)
							{
								nsAutoString img;
								rv = image->GetSrc (img);
								if (!NS_FAILED(rv))
								{
									SetStringProperty ("image", img);
								}
							}
						}
					}
					else
					{
						SetIntProperty ("link_is_smart", FALSE);
					}
				}
			
			}
			else if (tag.EqualsIgnoreCase("option"))
			{
				info->context = EMBED_CONTEXT_NONE;
				return NS_OK;
			}
			if (tag.EqualsIgnoreCase("area"))
			{
				info->context |= EMBED_CONTEXT_LINK;
				nsCOMPtr <nsIDOMHTMLAreaElement> area =
						do_QueryInterface(node, &rv);
				if (NS_SUCCEEDED(rv) && area)
				{
					nsAutoString href;
					rv = area->GetHref (href);
					if (NS_FAILED(rv))
						return NS_ERROR_FAILURE;
					
					SetStringProperty ("link", href);
					CheckLinkScheme (href);
				}
			}
			else if (tag.EqualsIgnoreCase("textarea") ||
				 tag.EqualsIgnoreCase("input"))
			{
				info->context |= EMBED_CONTEXT_INPUT;
			}

			nsCOMPtr<nsIDOMElement> domelement;
			domelement = do_QueryInterface (node);
			if (!domelement) return NS_ERROR_FAILURE;

			PRBool has_background = PR_FALSE;

			NS_NAMED_LITERAL_STRING(attr, "background");
			nsAutoString value;
			domelement->GetAttribute (attr, value);
				
			if (!value.IsEmpty())
			{
				nsCAutoString bgimg;

				rv = ResolveDocumentURL (doc, value, bgimg);
                                if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

				SetStringProperty ("background_image", bgimg.get());
			}
			else
			{
				nsCOMPtr<nsIDOMHTMLBodyElement> bgelement;
				bgelement = do_QueryInterface (node);
				if (bgelement)
				{
					nsAutoString value;
					bgelement->GetBackground (value);

					if (!value.IsEmpty())
					{
						nsCAutoString bgimg;

						rv = ResolveBaseURL (doc, value, bgimg);
                                                if (NS_FAILED(rv))
                                                        return NS_ERROR_FAILURE;

						SetStringProperty ("background_image",
						   bgimg.get());
						has_background = PR_TRUE;
					}
				}
			}

			if (!has_background)
			{
				nsAutoString cssurl;
				rv = GetCSSBackground (node, cssurl);
				if (NS_SUCCEEDED (rv))
				{
					nsCAutoString bgimg;

                                        rv = ResolveBaseURL (doc, cssurl, bgimg);
                                        if (NS_FAILED (rv))
                                                return NS_ERROR_FAILURE;
					SetStringProperty ("background_image",
						           bgimg.get());
					if (NS_FAILED (rv))
						return NS_ERROR_FAILURE;
				}
			}
		}
		
		nsCOMPtr<nsIDOMNode> parentNode;
		node->GetParentNode (getter_AddRefs(parentNode));
		node = parentNode;
	}
	
	return NS_OK;
}

nsresult EventContext::GetCSSBackground (nsIDOMNode *node, nsAutoString& url)
{
	nsresult result;

	nsCOMPtr<nsIDOMElementCSSInlineStyle> style;
	style = do_QueryInterface (node);
	if (!style) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
	result = style->GetStyle (getter_AddRefs(decl));
	if (NS_FAILED(result)) return NS_ERROR_FAILURE;

	nsAutoString value;
	NS_NAMED_LITERAL_STRING(prop_bgi, "background-image");
	decl->GetPropertyValue (prop_bgi, value);

	if (value.IsEmpty())
	{
		NS_NAMED_LITERAL_STRING(prop_bg, "background");
		decl->GetPropertyValue (prop_bg, value);
		if (value.IsEmpty())
		{
			NS_NAMED_LITERAL_STRING(prop_bgr, "background-repeat");
			decl->GetPropertyValue (prop_bgr, value);
			if (value.IsEmpty())
				return NS_ERROR_FAILURE;
		}
	}

	PRInt32 start, end;
	nsAutoString cssurl;

	NS_NAMED_LITERAL_STRING(startsub, "url(");
	NS_NAMED_LITERAL_STRING(endsub, ")");

	start = value.Find (startsub) + 4;
	end = value.Find (endsub);

	if (start == -1 || end == -1)
		return NS_ERROR_FAILURE;

	url.Assign(Substring (value, start, end - start));

	return NS_OK;
}

nsresult EventContext::GetMouseEventInfo (nsIDOMMouseEvent *aMouseEvent, MozillaEmbedEvent *info)
{
	nsresult result;

	/* casting 32-bit guint* to PRUint16* below will break on big-endian */
	PRUint16 btn;
	aMouseEvent->GetButton (&btn);

	switch (btn)
	{
		case 0:
			info->type = EPHY_EMBED_EVENT_MOUSE_BUTTON1;
		break;
		case 1:
			info->type = EPHY_EMBED_EVENT_MOUSE_BUTTON2;
		break;
		case 2:
			info->type = EPHY_EMBED_EVENT_MOUSE_BUTTON3;
		break;

		default:
			g_warning ("Unknown mouse button");
	}

	/* OTOH, casting only between (un)signedness is safe */
	aMouseEvent->GetScreenX ((PRInt32*)&info->x);
	aMouseEvent->GetScreenY ((PRInt32*)&info->y);

	/* be sure we are not clicking on the scroolbars */

	nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(aMouseEvent, &result);
	if (NS_FAILED(result) || !nsEvent) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMEventTarget> OriginalTarget;
	result = nsEvent->GetOriginalTarget(getter_AddRefs(OriginalTarget));
	if (NS_FAILED(result) || !OriginalTarget) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMNode> OriginalNode = do_QueryInterface(OriginalTarget);
	if (!OriginalNode) return NS_ERROR_FAILURE;

	nsAutoString nodename;
	OriginalNode->GetNodeName(nodename);

	if (nodename.EqualsIgnoreCase("xul:scrollbarbutton") ||
	    nodename.EqualsIgnoreCase("xul:thumb")	     ||
	    nodename.EqualsIgnoreCase("xul:vbox")	     ||
	    nodename.EqualsIgnoreCase("xul:spacer")	     ||
	    nodename.EqualsIgnoreCase("xul:slider"))
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMEventTarget> EventTarget;
	result = aMouseEvent->GetTarget(getter_AddRefs(EventTarget));
	if (NS_FAILED(result) || !EventTarget) return NS_ERROR_FAILURE;

	result = GetEventContext (EventTarget, info);
	if (NS_FAILED(result)) return result;

	/* Get the modifier */

	PRBool mod_key;

	info->modifier = 0;

	aMouseEvent->GetAltKey(&mod_key);
	if (mod_key) info->modifier |= GDK_MOD1_MASK;

	aMouseEvent->GetShiftKey(&mod_key);
	if (mod_key) info->modifier |= GDK_SHIFT_MASK;

	aMouseEvent->GetMetaKey(&mod_key);
	if (mod_key) info->modifier |= GDK_MOD2_MASK;
	
	aMouseEvent->GetCtrlKey(&mod_key);
	if (mod_key) info->modifier |= GDK_CONTROL_MASK;

	return NS_OK;
}

nsresult EventContext::GetKeyEventInfo (nsIDOMKeyEvent *aKeyEvent, MozillaEmbedEvent *info)
{
	nsresult rv;

	info->type = EPHY_EMBED_EVENT_KEY;

	PRUint32 keyCode;
	rv = aKeyEvent->GetKeyCode(&keyCode);
	if (NS_FAILED(rv)) return rv;
	info->keycode = keyCode;

	nsCOMPtr<nsIDOMEventTarget> target;
	rv = aKeyEvent->GetTarget(getter_AddRefs(target));
	if (NS_FAILED(rv) || !target) return NS_ERROR_FAILURE;

	/* Calculate the node coordinates relative to the widget origin */
	nsCOMPtr<nsIDOMNSHTMLElement> elem = do_QueryInterface(target, &rv);
	if (NS_FAILED(rv)) return rv;

	PRInt32 x = 0, y = 0;
	while (elem)
	{
		PRInt32 val;
		elem->GetOffsetTop(&val);	y += val;
		elem->GetScrollTop(&val);	y -= val;
		elem->GetOffsetLeft(&val);	x += val;
		elem->GetScrollLeft(&val);	x -= val;

		nsCOMPtr<nsIDOMElement> parent;
		elem->GetOffsetParent(getter_AddRefs(parent));
		elem = do_QueryInterface(parent, &rv);
	}
	info->x = x;
	info->y = y;

	/* Context */
	rv = GetEventContext (target, info);
	if (NS_FAILED(rv)) return rv;

	/* Get the modifier */

	PRBool mod_key;

	info->modifier = 0;

	aKeyEvent->GetAltKey(&mod_key);
	if (mod_key) info->modifier |= GDK_MOD1_MASK;

	aKeyEvent->GetShiftKey(&mod_key);
	if (mod_key) info->modifier |= GDK_SHIFT_MASK;

	aKeyEvent->GetMetaKey(&mod_key);
	if (mod_key) info->modifier |= GDK_MOD2_MASK;
	
	aKeyEvent->GetCtrlKey(&mod_key);
	if (mod_key) info->modifier |= GDK_CONTROL_MASK;

	return NS_OK;
}

nsresult EventContext::IsPageFramed (nsIDOMNode *node, PRBool *Framed)
{
	nsresult result;

	nsCOMPtr<nsIDOMDocument> mainDocument;
	result = mBrowser->GetDocument (getter_AddRefs(mainDocument));
	if (NS_FAILED(result) || !mainDocument) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIDOMDocument> nodeDocument;
	result = node->GetOwnerDocument (getter_AddRefs(nodeDocument));
	if (NS_FAILED(result) || !nodeDocument) return NS_ERROR_FAILURE;
 
	*Framed = (mainDocument != nodeDocument);

        return NS_OK;
}

nsresult EventContext::GetTargetDocument (nsIDOMDocument **domDoc)
{
	if (!mDOMDocument) return NS_ERROR_FAILURE;

	*domDoc = mDOMDocument.get();

	NS_IF_ADDREF(*domDoc);

	return NS_OK;
}

nsresult EventContext::CheckLinkScheme (const nsAString &link)
{
	nsCOMPtr<nsIURI> uri;
	NS_NewURI (getter_AddRefs (uri), link);
	if (!uri) return NS_ERROR_FAILURE;

	nsresult rv;
	nsCAutoString scheme;
	rv = uri->GetScheme (scheme);
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	if (scheme.EqualsIgnoreCase ("http")	 ||
	    scheme.EqualsIgnoreCase ("https")	 ||
	    scheme.EqualsIgnoreCase ("ftp")	 ||
	    scheme.EqualsIgnoreCase ("file")	 ||
	    scheme.EqualsIgnoreCase ("data")	 ||
	    scheme.EqualsIgnoreCase ("resource") ||
	    scheme.EqualsIgnoreCase ("about")	 ||
	    scheme.EqualsIgnoreCase ("gopher"))
	{
		SetIntProperty ("link-has-web-scheme", TRUE);
	}

	return NS_OK;
}

nsresult EventContext::SetIntProperty (const char *name, int value)
{

	GValue *val = g_new0 (GValue, 1);

	g_value_init (val, G_TYPE_INT);
	
	g_value_set_int (val, value);

	mozilla_embed_event_set_property (mEmbedEvent, name, val);

	return NS_OK;
}

nsresult EventContext::SetStringProperty (const char *name, const char *value)
{
	GValue *val = g_new0 (GValue, 1);

	g_value_init (val, G_TYPE_STRING);
	
	g_value_set_string (val, value);
			 
	mozilla_embed_event_set_property (mEmbedEvent, name, val);

	return NS_OK;
}

nsresult EventContext::SetStringProperty (const char *name, const nsAString &value)
{
	return SetStringProperty (name, NS_ConvertUCS2toUTF8(value).get());
}
