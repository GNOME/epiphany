/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2004 Crispin Flowerday
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

#include "EventContext.h"
#include "EphyUtils.h"

#include <gdk/gdkkeysyms.h>

#include <nsIInterfaceRequestor.h>
#include <nsIServiceManager.h>
#include <nsEmbedString.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMHTMLInputElement.h>
#include <nsIDOMHTMLObjectElement.h>
#include <nsIDOMHTMLImageElement.h>
#include <nsIDOMElement.h>
#include <nsIURI.h>
#include <nsIDOMCharacterData.h>
#include <nsIDOMHTMLButtonElement.h>
#include <nsIDOMHTMLLabelElement.h>
#include <nsIDOMHTMLLegendElement.h>
#include <nsIDOMHTMLTextAreaElement.h>
#include <nsIDOMElementCSSInlineStyle.h>
#include <nsIDOMCSSStyleDeclaration.h>
#include <nsIDOM3Node.h>
#include <nsIDOMCSSPrimitiveValue.h>
#include <nsIDOMNodeList.h>
#include <nsIDOMDocumentView.h>
#include <nsIDOMAbstractView.h>

#ifdef ALLOW_PRIVATE_API
#include <nsITextToSubURI.h>
#include <nsIDOMXULDocument.h>
#include <nsIDOMNSEvent.h>
#include <nsIDOMNSHTMLElement.h>
#include <nsIDOMViewCSS.h>
#endif

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

nsresult EventContext::GatherTextUnder (nsIDOMNode* aNode, nsAString& aResult)
{
	nsEmbedString text;
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
			text += ' ';
			nsEmbedString data;
			charData->GetData(data);
			text += data;
		}
		else
		{
			nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(node));
			if (img)
			{
				nsEmbedString altText;
				img->GetAlt(altText);
				if (altText.Length())
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
					parentNode->GetNextSibling(getter_AddRefs(nextSibling));
					node = nextSibling;
					depth--;
				}
			}
		}
	}

	/* FIXME we should trim spaces here */

	aResult = text;

	return NS_OK;
}

nsresult EventContext::ResolveBaseURL (const nsAString &relurl, nsACString &url)
{
	nsresult rv;

	nsCOMPtr<nsIDOM3Node> node(do_QueryInterface (mDOMDocument));
	nsEmbedString spec;
	node->GetBaseURI (spec);

	nsCOMPtr<nsIURI> base;
	rv = EphyUtils::NewURI (getter_AddRefs(base), spec);
	if (!base) return NS_ERROR_FAILURE;

	nsEmbedCString cRelURL;
	NS_UTF16ToCString (relurl, NS_CSTRING_ENCODING_UTF8, cRelURL);	

	return base->Resolve (cRelURL, url);
}

nsresult EventContext::Unescape (const nsACString &aEscaped, nsACString &aUnescaped)
{
	if (!aEscaped.Length()) return NS_ERROR_FAILURE;

	nsCOMPtr<nsITextToSubURI> escaper
		(do_CreateInstance ("@mozilla.org/intl/texttosuburi;1"));
	NS_ENSURE_TRUE (escaper, NS_ERROR_FAILURE);

	nsresult rv;
	nsEmbedCString encoding;
	rv = mBrowser->GetEncoding (encoding);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsEmbedString unescaped;
	rv = escaper->UnEscapeNonAsciiURI (encoding, aEscaped, unescaped);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && unescaped.Length(), NS_ERROR_FAILURE);

	NS_UTF16ToCString (unescaped, NS_CSTRING_ENCODING_UTF8, aUnescaped);

	return NS_OK;
}

nsresult EventContext::GetEventContext (nsIDOMEventTarget *EventTarget,
					MozillaEmbedEvent *info)
{
	nsresult rv;

	const PRUnichar hrefLiteral[] = {'h', 'r', 'e', 'f', '\0'};
	const PRUnichar typeLiteral[] = {'t', 'y', 'p', 'e', '\0'};
	const PRUnichar xlinknsLiteral[] = {'h', 't', 't', 'p', ':', '/', '/','w',
				            'w', 'w', '.', 'w', '3', '.', 'o', 'r',
				            'g', '/', '1', '9', '9', '9', '/', 'x',
				            'l', 'i', 'n', 'k', '\0'};
	const PRUnichar bodyLiteral[] = { 'b', 'o', 'd', 'y', '\0' };

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

	PRBool has_background = PR_FALSE;

	nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(node);
	if ((nsIDOMNode::ELEMENT_NODE == type) && element)
	{
		nsEmbedString uTag;
		rv = element->GetLocalName(uTag);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		nsEmbedCString tag;
		NS_UTF16ToCString (uTag, NS_CSTRING_ENCODING_UTF8, tag);

		if (g_ascii_strcasecmp (tag.get(), "img") == 0)
		{
			info->context |= EMBED_CONTEXT_IMAGE;

			nsEmbedString img;
			nsCOMPtr <nsIDOMHTMLImageElement> image = 
						do_QueryInterface(node, &rv);
			if (NS_FAILED(rv) || !image) return NS_ERROR_FAILURE;			

			rv = image->GetSrc (img);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
			SetStringProperty ("image", img);
		}
		else if (g_ascii_strcasecmp (tag.get(), "input") == 0)
		{
			CheckInput (node);
		}
		else if (g_ascii_strcasecmp (tag.get(), "textarea") == 0)
		{
			info->context |= EMBED_CONTEXT_INPUT;
		}
		else if (g_ascii_strcasecmp (tag.get(), "object") == 0)
		{
			nsCOMPtr<nsIDOMHTMLObjectElement> object;
			object = do_QueryInterface (node);
			if (!element) return NS_ERROR_FAILURE;

			nsEmbedString value;
			object->GetType(value);

			nsEmbedCString cValue;
			NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);

			// MIME types are always lower case
			if (g_str_has_prefix (cValue.get(), "image/"))
			{
				info->context |= EMBED_CONTEXT_IMAGE;
				
				nsEmbedString img;
				
				rv = object->GetData (img);
				if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
				
				nsEmbedCString cImg;
				rv = ResolveBaseURL (img, cImg);
                                if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

				SetStringProperty ("image", cImg.get());
			}
			else
			{
				info->context = EMBED_CONTEXT_NONE;
				return NS_OK;
			}
		}
		else if (g_ascii_strcasecmp (tag.get(), "html") == 0)
		{
			/* Clicked on part of the page without a <body>, so
			 * look for a background image in the body tag */
			nsCOMPtr<nsIDOMNodeList> nodeList;

			rv = mDOMDocument->GetElementsByTagName (nsEmbedString(bodyLiteral),
								 getter_AddRefs (nodeList));
			if (NS_SUCCEEDED (rv) && nodeList)
			{
				nsCOMPtr<nsIDOMNode> bodyNode;
				nodeList->Item (0, getter_AddRefs (bodyNode));

				nsEmbedString cssurl;
				rv = GetCSSBackground (bodyNode, cssurl);
				if (NS_SUCCEEDED (rv))
				{
					nsEmbedCString bgimg;
					rv = ResolveBaseURL (cssurl, bgimg);
					if (NS_FAILED (rv))
						return NS_ERROR_FAILURE;

					SetStringProperty ("background_image",
							   bgimg.get());

					has_background = PR_TRUE;
				}
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
			nsEmbedString value;
			dom_elem->GetAttributeNS (nsEmbedString(xlinknsLiteral),
						  nsEmbedString(typeLiteral), value);

			nsEmbedCString cValue;
			NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);

			if (g_ascii_strcasecmp (cValue.get(), "simple") == 0)
			{
				info->context |= EMBED_CONTEXT_LINK;
				dom_elem->GetAttributeNS (nsEmbedString(xlinknsLiteral),
							  nsEmbedString(hrefLiteral), value);
				
				SetStringProperty ("link", value);
				CheckLinkScheme (value);
			}
		}

		rv = node->GetNodeType(&type);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		element = do_QueryInterface(node);
		if ((nsIDOMNode::ELEMENT_NODE == type) && element)
		{
			nsEmbedString uTag;
			rv = element->GetLocalName(uTag);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

			nsEmbedCString tag;
			NS_UTF16ToCString (uTag, NS_CSTRING_ENCODING_UTF8, tag);

			/* Link */
			if (g_ascii_strcasecmp (tag.get(), "a") == 0)
			{
				nsEmbedString tmp;

				rv = GatherTextUnder (node, tmp);
				if (NS_SUCCEEDED(rv))
                                	SetStringProperty ("linktext", tmp);

				nsCOMPtr <nsIDOMHTMLAnchorElement> anchor =
					do_QueryInterface(node);

				nsEmbedCString href;
				anchor->GetHref (tmp);
				NS_UTF16ToCString (tmp, NS_CSTRING_ENCODING_UTF8, href);

				if (g_str_has_prefix (href.get(), "mailto:"))
				{
					/* cut "mailto:" */
					href.Cut (0, 7);
					// FIXME: cut any chars after "?"

					nsEmbedCString unescapedHref;
					rv = Unescape (href, unescapedHref);
					if (NS_SUCCEEDED (rv) && unescapedHref.Length())
					{
						SetStringProperty ("email", unescapedHref.get());
						info->context |= EMBED_CONTEXT_EMAIL_LINK;
					}
				}
				
				if (anchor && tmp.Length()) 
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

					nsEmbedCString linkType;
					NS_UTF16ToCString (tmp, NS_CSTRING_ENCODING_UTF8, linkType);

					if (g_ascii_strcasecmp (linkType.get(), "text/smartbookmark") == 0)
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
								nsEmbedString img;
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
			else if (g_ascii_strcasecmp (tag.get(), "option") == 0)
			{
				info->context = EMBED_CONTEXT_NONE;
				return NS_OK;
			}
			if (g_ascii_strcasecmp (tag.get(), "area") == 0)
			{
				info->context |= EMBED_CONTEXT_LINK;
				nsCOMPtr <nsIDOMHTMLAreaElement> area =
						do_QueryInterface(node, &rv);
				if (NS_SUCCEEDED(rv) && area)
				{
					nsEmbedString href;
					rv = area->GetHref (href);
					if (NS_FAILED(rv))
						return NS_ERROR_FAILURE;
					
					SetStringProperty ("link", href);
					CheckLinkScheme (href);
				}
			}
			else if (g_ascii_strcasecmp (tag.get(), "input") == 0)
			{
				CheckInput (node);
			}
			else if (g_ascii_strcasecmp (tag.get(), "textarea") == 0)
			{
				info->context |= EMBED_CONTEXT_INPUT;
			}

			if (!has_background)
			{
				nsEmbedString cssurl;
				rv = GetCSSBackground (node, cssurl);
				if (NS_SUCCEEDED (rv))
				{
					nsEmbedCString bgimg;

                                        rv = ResolveBaseURL (cssurl, bgimg);
                                        if (NS_FAILED (rv))
                                                return NS_ERROR_FAILURE;
					SetStringProperty ("background_image",
						           bgimg.get());
					if (NS_FAILED (rv))
						return NS_ERROR_FAILURE;

					has_background = PR_TRUE;
				}
			}
		}
		
		nsCOMPtr<nsIDOMNode> parentNode;
		node->GetParentNode (getter_AddRefs(parentNode));
		node = parentNode;
	}
	
	return NS_OK;
}

nsresult EventContext::GetCSSBackground (nsIDOMNode *node, nsAString& url)
{
	nsresult rv;

	const PRUnichar bgimage[] = {'b', 'a', 'c', 'k', 'g', 'r', 'o', 'u', 'n', 'd',
				     '-', 'i', 'm', 'a', 'g', 'e', '\0'};

	nsCOMPtr<nsIDOMElement> element = do_QueryInterface (node);
	NS_ENSURE_TRUE (element, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface (mDOMDocument);
	NS_ENSURE_TRUE (docView, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMAbstractView> abstractView;
	docView->GetDefaultView (getter_AddRefs (abstractView));

	nsCOMPtr<nsIDOMViewCSS> viewCSS = do_QueryInterface (abstractView);
	NS_ENSURE_TRUE (viewCSS, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
	viewCSS->GetComputedStyle (element, nsEmbedString(),
				   getter_AddRefs (decl));
	NS_ENSURE_TRUE (decl, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMCSSValue> CSSValue;
	decl->GetPropertyCSSValue (nsEmbedString(bgimage),
				   getter_AddRefs (CSSValue));

	nsCOMPtr<nsIDOMCSSPrimitiveValue> primitiveValue = 
		do_QueryInterface (CSSValue);
	if (!primitiveValue) return NS_ERROR_FAILURE;
	
	PRUint16 type;
	rv = primitiveValue->GetPrimitiveType (&type);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	if (type != nsIDOMCSSPrimitiveValue::CSS_URI) return NS_ERROR_FAILURE;

	rv = primitiveValue->GetStringValue (url);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	return NS_OK;
}

nsresult EventContext::GetMouseEventInfo (nsIDOMMouseEvent *aMouseEvent, MozillaEmbedEvent *info)
{
	/* FIXME: casting 32-bit guint* to PRUint16* below will break on big-endian */
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

		case (PRUint16) -1:
			/* when the user submits a form with Return, mozilla synthesises
			 * a _mouse_ click event with btn=65535 (-1).
			 */
			info->type = EPHY_EMBED_EVENT_KEY;
			break;

		default:
			g_warning ("Unknown mouse button");
	}

	/* OTOH, casting only between (un)signedness is safe */
	aMouseEvent->GetScreenX ((PRInt32*)&info->x);
	aMouseEvent->GetScreenY ((PRInt32*)&info->y);

	/* be sure we are not clicking on the scroolbars */

	nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(aMouseEvent);
	if (!nsEvent) return NS_ERROR_FAILURE;

	nsresult rv;
	nsCOMPtr<nsIDOMEventTarget> OriginalTarget;
	rv = nsEvent->GetOriginalTarget(getter_AddRefs(OriginalTarget));
	if (NS_FAILED (rv) || !OriginalTarget) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMNode> OriginalNode = do_QueryInterface(OriginalTarget);
	if (!OriginalNode) return NS_ERROR_FAILURE;

	nsEmbedString nodename;
	OriginalNode->GetNodeName(nodename);
	nsEmbedCString cNodeName;
	NS_UTF16ToCString (nodename, NS_CSTRING_ENCODING_UTF8, cNodeName);

	if (g_ascii_strcasecmp (cNodeName.get(), "xul:scrollbarbutton") == 0 ||
	    g_ascii_strcasecmp (cNodeName.get(), "xul:thumb") == 0 ||
	    g_ascii_strcasecmp (cNodeName.get(), "xul:vbox") == 0 ||
	    g_ascii_strcasecmp (cNodeName.get(), "xul:spacer") == 0 ||
	    g_ascii_strcasecmp (cNodeName.get(), "xul:slider") == 0)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMEventTarget> EventTarget;
	rv = aMouseEvent->GetTarget(getter_AddRefs(EventTarget));
	if (NS_FAILED (rv) || !EventTarget) return NS_ERROR_FAILURE;

	rv = GetEventContext (EventTarget, info);
	if (NS_FAILED (rv)) return rv;

	/* Get the modifier */

	PRBool mod_key;

	info->modifier = 0;

	aMouseEvent->GetAltKey(&mod_key);
	if (mod_key) info->modifier |= GDK_MOD1_MASK;

	aMouseEvent->GetShiftKey(&mod_key);
	if (mod_key) info->modifier |= GDK_SHIFT_MASK;

	/* no need to check GetMetaKey, it's always PR_FALSE,
	 * see widget/src/gtk2/nsWindow.cpp:InitMouseEvent
	 */
	
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
	nsresult rv;

	nsCOMPtr<nsIDOMDocument> mainDocument;
	rv = mBrowser->GetDocument (getter_AddRefs(mainDocument));
	if (NS_FAILED (rv) || !mainDocument) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIDOMDocument> nodeDocument;
	rv = node->GetOwnerDocument (getter_AddRefs(nodeDocument));
	if (NS_FAILED (rv) || !nodeDocument) return NS_ERROR_FAILURE;
 
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

nsresult EventContext::CheckInput (nsIDOMNode *aNode)
{
	const PRUnichar typeLiteral[] = { 't', 'y', 'p', 'e', '\0' };

	nsCOMPtr<nsIDOMElement> element;
	element = do_QueryInterface (aNode);
	if (!element) return NS_ERROR_FAILURE;

	nsEmbedString uValue;
	element->GetAttribute (nsEmbedString(typeLiteral), uValue);

	nsEmbedCString value;
	NS_UTF16ToCString (uValue, NS_CSTRING_ENCODING_UTF8, value);

	if (g_ascii_strcasecmp (value.get(), "image") == 0)
	{
		mEmbedEvent->context |= EMBED_CONTEXT_IMAGE;
		nsCOMPtr<nsIDOMHTMLInputElement> input;
		input = do_QueryInterface (aNode);
		if (!input) return NS_ERROR_FAILURE;

		nsresult rv;
		nsEmbedString img;
		rv = input->GetSrc (img);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		nsEmbedCString cImg;
		rv = ResolveBaseURL (img, cImg);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
		SetStringProperty ("image", cImg.get());
	}
	else if (g_ascii_strcasecmp (value.get(), "radio") != 0 &&
		 g_ascii_strcasecmp (value.get(), "submit") != 0 &&
		 g_ascii_strcasecmp (value.get(), "reset") != 0 &&
		 g_ascii_strcasecmp (value.get(), "hidden") != 0 &&
		 g_ascii_strcasecmp (value.get(), "button") != 0 &&
		 g_ascii_strcasecmp (value.get(), "checkbox") != 0)
	{
		mEmbedEvent->context |= EMBED_CONTEXT_INPUT;
	}

	return NS_OK;
}

nsresult EventContext::CheckLinkScheme (const nsAString &link)
{
	nsCOMPtr<nsIURI> uri;
	EphyUtils::NewURI (getter_AddRefs (uri), link);
	if (!uri) return NS_ERROR_FAILURE;

	nsresult rv;
	nsEmbedCString scheme;
	rv = uri->GetScheme (scheme);
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	if (g_ascii_strcasecmp (scheme.get(), "http") ||
	    g_ascii_strcasecmp (scheme.get(), "https") ||
	    g_ascii_strcasecmp (scheme.get(), "ftp") ||
	    g_ascii_strcasecmp (scheme.get(), "file") ||
	    g_ascii_strcasecmp (scheme.get(), "data") ||
	    g_ascii_strcasecmp (scheme.get(), "resource") ||
	    g_ascii_strcasecmp (scheme.get(), "about") ||
	    g_ascii_strcasecmp (scheme.get(), "gopher"))
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
	nsEmbedCString cValue;
	NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);
	return SetStringProperty (name, cValue.get());
}
