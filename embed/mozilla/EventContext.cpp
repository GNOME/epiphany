/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <gdk/gdkkeysyms.h>

#include <nsStringAPI.h>

#include <nsComponentManagerUtils.h>
#include <nsIDOM3Node.h>
#include <nsIDOMAbstractView.h>
#include <nsIDOMCharacterData.h>
#include <nsIDOMCSSPrimitiveValue.h>
#include <nsIDOMCSSStyleDeclaration.h>
#include <nsIDOMDocument.h>
#include <nsIDOMDocumentView.h>
#include <nsIDOMElementCSSInlineStyle.h>
#include <nsIDOMElement.h>
#include <nsIDOMEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMHTMLAnchorElement.h>
#include <nsIDOMHTMLAreaElement.h>
#include <nsIDOMHTMLBodyElement.h>
#include <nsIDOMHTMLButtonElement.h>
#include <nsIDOMHTMLEmbedElement.h>
#include <nsIDOMHTMLImageElement.h>
#include <nsIDOMHTMLInputElement.h>
#include <nsIDOMHTMLIsIndexElement.h>
#include <nsIDOMHTMLLabelElement.h>
#include <nsIDOMHTMLLegendElement.h>
#include <nsIDOMHTMLMapElement.h>
#include <nsIDOMHTMLObjectElement.h>
#include <nsIDOMHTMLSelectElement.h>
#include <nsIDOMHTMLTextAreaElement.h>
#include <nsIDOMKeyEvent.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMNode.h>
#include <nsIDOMNodeList.h>
#include <nsIDOMNSHTMLDocument.h>
#include <nsIDOMNSUIEvent.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIServiceManager.h>
#include <nsIURI.h>

#ifdef ALLOW_PRIVATE_API
#include <nsIDOMNSEvent.h>
#include <nsIDOMNSHTMLElement.h>
#include <nsIDOMViewCSS.h>
#include <nsIDOMViewCSS.h>
#include <nsIDOMXULDocument.h>
#include <nsITextToSubURI.h>
#endif

#include "ephy-debug.h"

#include "EphyBrowser.h"
#include "EphyUtils.h"

#include "EventContext.h"


#define KEY_CODE 256

EventContext::EventContext ()
{
	LOG ("EventContext ctor [%p]", this);
}

EventContext::~EventContext ()
{
	LOG ("EventContext dtor [%p]", this);
}

nsresult EventContext::Init (EphyBrowser *browser)
{
	mBrowser = browser;
	mDOMDocument = nsnull;

	return NS_OK;
}

nsresult EventContext::GatherTextUnder (nsIDOMNode* aNode, nsAString& aResult)
{
	nsString text;
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
			nsString data;
			charData->GetData(data);
			text += data;
		}
		else
		{
			nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(node));
			if (img)
			{
				nsString altText;
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

/* FIXME: we should resolve against the element's base, not the document's base */
nsresult EventContext::ResolveBaseURL (const nsAString &relurl, nsACString &url)
{
	nsCString cRelURL;
	NS_UTF16ToCString (relurl, NS_CSTRING_ENCODING_UTF8, cRelURL);	

	return mBaseURI->Resolve (cRelURL, url);
}

nsresult EventContext::Unescape (const nsACString &aEscaped, nsACString &aUnescaped)
{
	if (!aEscaped.Length()) return NS_ERROR_FAILURE;

	nsCOMPtr<nsITextToSubURI> escaper
		(do_CreateInstance ("@mozilla.org/intl/texttosuburi;1"));
	NS_ENSURE_TRUE (escaper, NS_ERROR_FAILURE);

	nsresult rv;
	nsCString encoding;
	rv = mBrowser->GetEncoding (encoding);
	NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);

	nsString unescaped;
	rv = escaper->UnEscapeURIForUI (encoding, aEscaped, unescaped);
	NS_ENSURE_TRUE (NS_SUCCEEDED (rv) && unescaped.Length(), NS_ERROR_FAILURE);

	NS_UTF16ToCString (unescaped, NS_CSTRING_ENCODING_UTF8, aUnescaped);

	return NS_OK;
}

nsresult EventContext::GetEventContext (nsIDOMEventTarget *EventTarget,
					MozillaEmbedEvent *info)
{
	nsresult rv;

	const PRUnichar hrefLiteral[] = {'h', 'r', 'e', 'f', '\0'};
	const PRUnichar imgLiteral[] = {'i', 'm', 'g', '\0'};
	const PRUnichar typeLiteral[] = {'t', 'y', 'p', 'e', '\0'};
	const PRUnichar xlinknsLiteral[] = {'h', 't', 't', 'p', ':', '/', '/','w',
				            'w', 'w', '.', 'w', '3', '.', 'o', 'r',
				            'g', '/', '1', '9', '9', '9', '/', 'x',
				            'l', 'i', 'n', 'k', '\0'};
	const PRUnichar bodyLiteral[] = { 'b', 'o', 'd', 'y', '\0' };

	mEmbedEvent = info;

	info->context = EPHY_EMBED_CONTEXT_DOCUMENT;

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

	nsCOMPtr<nsIDOMXULDocument> xul_document = do_QueryInterface(domDoc);
	if (xul_document)
	{
		info->context = EPHY_EMBED_CONTEXT_NONE;
		return NS_ERROR_FAILURE;
	}

	mDOMDocument = domDoc;

	rv = mBrowser->GetEncoding (mCharset);
	NS_ENSURE_SUCCESS (rv, rv);

	/* Get base URI and CSS view */
	nsCOMPtr<nsIDOMDocumentView> docView (do_QueryInterface (domDoc));
	NS_ENSURE_TRUE (docView, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMAbstractView> abstractView;
	docView->GetDefaultView (getter_AddRefs (abstractView));
	NS_ENSURE_TRUE (abstractView, NS_ERROR_FAILURE);
	/* the abstract view is really the DOM window */

	mViewCSS = do_QueryInterface (abstractView);
	NS_ENSURE_TRUE (mViewCSS, NS_ERROR_FAILURE);

	nsCOMPtr<nsIWebNavigation> webNav (do_GetInterface (abstractView, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = webNav->GetCurrentURI (getter_AddRefs (mBaseURI));
	NS_ENSURE_SUCCESS (rv, rv);

	// Now we know that the page isn't a xul window, we can try and
	// do something useful with it.

	PRUint16 type;
	rv = node->GetNodeType(&type);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

	PRBool has_image = PR_FALSE;

	nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(node);
	if ((nsIDOMNode::ELEMENT_NODE == type) && element)
	{
		nsString uTag;
		rv = element->GetLocalName(uTag);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		nsCString tag;
		NS_UTF16ToCString (uTag, NS_CSTRING_ENCODING_UTF8, tag);

		if (g_ascii_strcasecmp (tag.get(), "img") == 0)
		{
			nsString img;
			nsCOMPtr <nsIDOMHTMLImageElement> image = 
						do_QueryInterface(node, &rv);
			if (NS_FAILED(rv) || !image) return NS_ERROR_FAILURE;			

			rv = image->GetSrc (img);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

			SetStringProperty ("image", img);
			info->context |= EPHY_EMBED_CONTEXT_IMAGE;
			has_image = PR_TRUE;
		}
		else if (g_ascii_strcasecmp (tag.get(), "area") == 0)
		{
			nsCOMPtr <nsIDOMHTMLAreaElement> area = 
						do_QueryInterface(node, &rv);
			if (NS_FAILED(rv) || !area) return NS_ERROR_FAILURE;			

			// Parent node is the map itself
			nsCOMPtr<nsIDOMNode> parentNode;
			node->GetParentNode (getter_AddRefs(parentNode));

			nsCOMPtr <nsIDOMHTMLMapElement> map = 
				do_QueryInterface(parentNode, &rv);
			if (NS_FAILED(rv) || !area) return NS_ERROR_FAILURE;			

			nsString mapName;
			rv = map->GetName (mapName);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

			// Now we are searching for all the images with a usemap attribute
			nsCOMPtr<nsIDOMNodeList> imgs;
			rv = mDOMDocument->GetElementsByTagName (nsString(imgLiteral), 	
								 getter_AddRefs (imgs));
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
			
			PRUint32 imgs_count;
			rv = imgs->GetLength (&imgs_count);
			if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

			for (PRUint32 i = 0; i < imgs_count; i++)
			{
				nsCOMPtr<nsIDOMNode> aNode;
				rv = imgs->Item (i, getter_AddRefs (aNode));
				if (NS_FAILED (rv)) continue;

				nsCOMPtr<nsIDOMHTMLImageElement> img = 
						do_QueryInterface(aNode, &rv);
				if (NS_FAILED(rv) || !img) continue;
			
				nsString imgMapName;
				rv = img->GetUseMap (imgMapName);
				if (NS_FAILED (rv)) continue;

				// usemap always starts with #
				imgMapName.Cut (0,1);

				// Check if the current image is attached to the map we are looking for
				if (imgMapName.Equals(mapName))
				{
					nsString imgSrc;
					rv = img->GetSrc (imgSrc);
					if (NS_FAILED(rv)) continue;

					SetStringProperty ("image", imgSrc);
					info->context |= EPHY_EMBED_CONTEXT_IMAGE;
					has_image = PR_TRUE;

					break;
				}
			}
		}
		else if (g_ascii_strcasecmp (tag.get(), "input") == 0)
		{
			CheckInput (node);
		}
		else if (g_ascii_strcasecmp (tag.get(), "textarea") == 0)
		{
			info->context |= EPHY_EMBED_CONTEXT_INPUT;
		}
		else if (g_ascii_strcasecmp (tag.get(), "object") == 0)
		{
			nsCOMPtr<nsIDOMHTMLObjectElement> object;
			object = do_QueryInterface (node);
			if (!element) return NS_ERROR_FAILURE;

			nsString value;
			object->GetType(value);

			nsCString cValue;
			NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);

			// MIME types are always lower case
			if (g_str_has_prefix (cValue.get(), "image/"))
			{
				nsString img;
				
				rv = object->GetData (img);
				if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
				
				nsCString cImg;
				rv = ResolveBaseURL (img, cImg);
                                if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

				SetStringProperty ("image", cImg.get());
				info->context |= EPHY_EMBED_CONTEXT_IMAGE;
				has_image = PR_TRUE;
			}
			else
			{
				info->context = EPHY_EMBED_CONTEXT_NONE;
				return NS_OK;
			}
		}
		else if (g_ascii_strcasecmp (tag.get(), "html") == 0)
		{
			/* Clicked on part of the page without a <body>, so
			 * look for a background image in the body tag */
			nsCOMPtr<nsIDOMNodeList> nodeList;

			rv = mDOMDocument->GetElementsByTagName (nsString(bodyLiteral),
								 getter_AddRefs (nodeList));
			if (NS_SUCCEEDED (rv) && nodeList)
			{
				nsCOMPtr<nsIDOMNode> bodyNode;
				nodeList->Item (0, getter_AddRefs (bodyNode));

				nsString cssurl;
				rv = GetCSSBackground (bodyNode, cssurl);
				if (NS_SUCCEEDED (rv))
				{
					nsCString bgimg;
					rv = ResolveBaseURL (cssurl, bgimg);
					if (NS_FAILED (rv))
						return NS_ERROR_FAILURE;

					SetStringProperty ("image", bgimg.get());
					info->context |= EPHY_EMBED_CONTEXT_IMAGE;
					has_image = PR_TRUE;
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
			nsString value;
			dom_elem->GetAttributeNS (nsString(xlinknsLiteral),
						  nsString(typeLiteral), value);

			nsCString cValue;
			NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);

			if (g_ascii_strcasecmp (cValue.get(), "simple") == 0)
			{
				info->context |= EPHY_EMBED_CONTEXT_LINK;
				dom_elem->GetAttributeNS (nsString(xlinknsLiteral),
							  nsString(hrefLiteral), value);
				
				SetURIProperty (node, "link", value);
				CheckLinkScheme (value);
			}
		}

		rv = node->GetNodeType(&type);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		element = do_QueryInterface(node);
		if ((nsIDOMNode::ELEMENT_NODE == type) && element)
		{
			nsString uTag;
			rv = element->GetLocalName(uTag);
			if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

			nsCString tag;
			NS_UTF16ToCString (uTag, NS_CSTRING_ENCODING_UTF8, tag);

			/* Link */
			if (g_ascii_strcasecmp (tag.get(), "a") == 0)
			{
				nsString tmp;

				rv = GatherTextUnder (node, tmp);
				if (NS_SUCCEEDED(rv))
                                	SetStringProperty ("linktext", tmp);

				nsCOMPtr <nsIDOMHTMLAnchorElement> anchor =
					do_QueryInterface(node);

				nsCString href;
				anchor->GetHref (tmp);
				NS_UTF16ToCString (tmp, NS_CSTRING_ENCODING_UTF8, href);

				if (g_str_has_prefix (href.get(), "mailto:"))
				{
					/* cut "mailto:" */
					href.Cut (0, 7);

					char *str = g_strdup (href.get());
					g_strdelimit (str, "?", '\0');

					nsCString unescapedHref;
					rv = Unescape (nsCString(str), unescapedHref);
					if (NS_SUCCEEDED (rv) && unescapedHref.Length())
					{
						SetStringProperty ("email", unescapedHref.get());
						info->context |= EPHY_EMBED_CONTEXT_EMAIL_LINK;
					}
					g_free (str);
				}
				
				if (anchor && tmp.Length()) 
				{
					info->context |= EPHY_EMBED_CONTEXT_LINK;

					SetURIProperty (node, "link", tmp);
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

					nsCString linkType;
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
								nsString img;
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
				info->context = EPHY_EMBED_CONTEXT_NONE;
				return NS_OK;
			}
			else if (g_ascii_strcasecmp (tag.get(), "area") == 0)
			{
				info->context |= EPHY_EMBED_CONTEXT_LINK;
				nsCOMPtr <nsIDOMHTMLAreaElement> area =
						do_QueryInterface(node, &rv);
				if (NS_SUCCEEDED(rv) && area)
				{
					nsString href;
					rv = area->GetHref (href);
					if (NS_FAILED(rv))
						return NS_ERROR_FAILURE;
					
					SetURIProperty (node, "link", href);
					CheckLinkScheme (href);
				}
			}
			else if (g_ascii_strcasecmp (tag.get(), "input") == 0)
			{
				CheckInput (node);
			}
			else if (g_ascii_strcasecmp (tag.get(), "textarea") == 0)
			{
				info->context |= EPHY_EMBED_CONTEXT_INPUT;
			}

			if (!has_image)
			{
				nsString cssurl;
				rv = GetCSSBackground (node, cssurl);
				if (NS_SUCCEEDED (rv))
				{
					nsCString bgimg;

                                        rv = ResolveBaseURL (cssurl, bgimg);
                                        if (NS_FAILED (rv))
                                                return NS_ERROR_FAILURE;
					SetStringProperty ("image", bgimg.get());
					info->context |= EPHY_EMBED_CONTEXT_IMAGE;
					has_image = PR_TRUE;
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
	if (!mViewCSS) return NS_ERROR_NOT_INITIALIZED;

	nsresult rv;

	const PRUnichar bgimage[] = {'b', 'a', 'c', 'k', 'g', 'r', 'o', 'u', 'n', 'd',
				     '-', 'i', 'm', 'a', 'g', 'e', '\0'};

	nsCOMPtr<nsIDOMElement> element = do_QueryInterface (node);
	NS_ENSURE_TRUE (element, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
	mViewCSS->GetComputedStyle (element, nsString(),
				    getter_AddRefs (decl));
	NS_ENSURE_TRUE (decl, NS_ERROR_FAILURE);

	nsCOMPtr<nsIDOMCSSValue> CSSValue;
	decl->GetPropertyCSSValue (nsString(bgimage),
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

nsresult EventContext::GetTargetCoords (nsIDOMEventTarget *aTarget, PRInt32 *aX, PRInt32 *aY)
{
	/* Calculate the node coordinates relative to the widget origin */
	nsCOMPtr<nsIDOMNSHTMLElement> elem (do_QueryInterface(aTarget));

	PRInt32 x = 0, y = 0;
	while (elem)
	{
		PRInt32 val;
		elem->GetOffsetTop(&val);	y += val;
		elem->GetScrollTop(&val);	y -= val;
		elem->GetOffsetLeft(&val);	x += val;
		elem->GetScrollLeft(&val);	x -= val;

		nsCOMPtr<nsIDOMElement> parent;
		elem->GetOffsetParent (getter_AddRefs (parent));
		elem = do_QueryInterface(parent);
	}

	*aX = x;
	*aY = y;

	return NS_OK;
}

nsresult EventContext::GetMouseEventInfo (nsIDOMMouseEvent *aMouseEvent, MozillaEmbedEvent *info)
{
	/* FIXME: casting 32-bit guint* to PRUint16* below will break on big-endian */
	PRUint16 btn = 1729;
	aMouseEvent->GetButton (&btn);

	switch (btn)
	{
		/* mozilla's button counting is one-off from gtk+'s */
		case 0:
			info->button = 1;
			break;
		case 1:
			info->button = 2;
			break;
		case 2:
			info->button = 3;
			break;

		case (PRUint16) -1:
			/* when the user submits a form with Return, mozilla synthesises
			 * a _mouse_ click event with btn=65535 (-1).
			 */
		default:
			info->button = 0;
			break;
	}

	if (info->button != 0)
	{
		/* OTOH, casting only between (un)signedness is safe */
		aMouseEvent->GetScreenX ((PRInt32*)&info->x);
		aMouseEvent->GetScreenY ((PRInt32*)&info->y);
	}
	else /* this is really a keyboard event */
	{
		nsCOMPtr<nsIDOMEventTarget> eventTarget;
		aMouseEvent->GetTarget (getter_AddRefs (eventTarget));

		GetTargetCoords (eventTarget, (PRInt32*)&info->x, (PRInt32*)&info->y);
	}

	/* be sure we are not clicking on the scroolbars */

	nsCOMPtr<nsIDOMNSEvent> nsEvent = do_QueryInterface(aMouseEvent);
	if (!nsEvent) return NS_ERROR_FAILURE;

#ifdef MOZ_NSIDOMNSEVENT_GETISTRUSTED
	/* make sure the event is trusted */
	PRBool isTrusted = PR_FALSE;
	nsEvent->GetIsTrusted (&isTrusted);
	if (!isTrusted) return NS_ERROR_UNEXPECTED;
#endif /* MOZ_NSIDOMNSEVENT_GETISTRUSTED */

	nsresult rv;
	nsCOMPtr<nsIDOMEventTarget> OriginalTarget;
	rv = nsEvent->GetOriginalTarget(getter_AddRefs(OriginalTarget));
	if (NS_FAILED (rv) || !OriginalTarget) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIDOMNode> OriginalNode = do_QueryInterface(OriginalTarget);
	if (!OriginalNode) return NS_ERROR_FAILURE;

	nsString nodename;
	OriginalNode->GetNodeName(nodename);
	nsCString cNodeName;
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
#ifdef MOZ_NSIDOMNSEVENT_GETISTRUSTED
	/* make sure the event is trusted */
	nsCOMPtr<nsIDOMNSEvent> nsEvent (do_QueryInterface (aKeyEvent));
	NS_ENSURE_TRUE (nsEvent, NS_ERROR_FAILURE);

	PRBool isTrusted = PR_FALSE;
	nsEvent->GetIsTrusted (&isTrusted);
	if (!isTrusted) return NS_ERROR_UNEXPECTED;
#endif /* MOZ_NSIDOMNSEVENT_GETISTRUSTED */


	info->button = 0;

	nsresult rv;
	PRUint32 keyCode;
	rv = aKeyEvent->GetKeyCode(&keyCode);
	if (NS_FAILED(rv)) return rv;
	info->keycode = keyCode;

	nsCOMPtr<nsIDOMEventTarget> target;
	rv = aKeyEvent->GetTarget(getter_AddRefs(target));
	if (NS_FAILED(rv) || !target) return NS_ERROR_FAILURE;

	GetTargetCoords (target, (PRInt32*)&info->x, (PRInt32*)&info->y);

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

	nsString uValue;
	element->GetAttribute (nsString(typeLiteral), uValue);

	nsCString value;
	NS_UTF16ToCString (uValue, NS_CSTRING_ENCODING_UTF8, value);

	if (g_ascii_strcasecmp (value.get(), "image") == 0)
	{
		mEmbedEvent->context |= EPHY_EMBED_CONTEXT_IMAGE;
		nsCOMPtr<nsIDOMHTMLInputElement> input;
		input = do_QueryInterface (aNode);
		if (!input) return NS_ERROR_FAILURE;

		nsresult rv;
		nsString img;
		rv = input->GetSrc (img);
		if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

		nsCString cImg;
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
		mEmbedEvent->context |= EPHY_EMBED_CONTEXT_INPUT;

		if (g_ascii_strcasecmp (value.get(), "password") == 0)
		{
			mEmbedEvent->context |= EPHY_EMBED_CONTEXT_INPUT_PASSWORD;
		}
	}

	return NS_OK;
}

nsresult EventContext::CheckLinkScheme (const nsAString &link)
{
	nsCOMPtr<nsIURI> uri;
	EphyUtils::NewURI (getter_AddRefs (uri), link);
	if (!uri) return NS_ERROR_FAILURE;

	nsresult rv;
	nsCString scheme;
	rv = uri->GetScheme (scheme);
	if (NS_FAILED (rv)) return NS_ERROR_FAILURE;

	if (g_ascii_strcasecmp (scheme.get(), "http") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "https") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "ftp") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "file") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "smb") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "sftp") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "ssh") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "data") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "resource") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "about") == 0 ||
	    g_ascii_strcasecmp (scheme.get(), "gopher") == 0)
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

	char *copy = g_strstrip (g_strdup (value));	
	g_value_take_string (val, copy);

	mozilla_embed_event_set_property (mEmbedEvent, name, val);

	return NS_OK;
}

nsresult EventContext::SetStringProperty (const char *name, const nsAString &value)
{
	nsCString cValue;
	NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);
	return SetStringProperty (name, cValue.get());
}

nsresult EventContext::SetURIProperty (nsIDOMNode *node, const char *name, const nsACString &value)
{
	nsresult rv;
	nsCOMPtr<nsIURI> uri;
	rv = EphyUtils::NewURI (getter_AddRefs (uri), value, mCharset.Length () ? mCharset.get() : nsnull, mBaseURI);
	if (NS_SUCCEEDED (rv) && uri)
	{
		/* Hide password part */
		nsCString user;
		uri->GetUsername (user);
		uri->SetUserPass (user);

		nsCString spec;
		uri->GetSpec (spec);
		rv = SetStringProperty (name, spec.get());
	}
	else
	{
		rv = SetStringProperty (name, nsCString(value).get());
	}

	return rv;
}

nsresult EventContext::SetURIProperty (nsIDOMNode *node, const char *name, const nsAString &value)
{
	nsCString cValue;
	NS_UTF16ToCString (value, NS_CSTRING_ENCODING_UTF8, cValue);
	return SetURIProperty (node, name, cValue);
}

/* static */
PRBool
EventContext::CheckKeyPress (nsIDOMKeyEvent *aEvent)
{
	PRBool retval = PR_FALSE;

	/* make sure the event is trusted */
	nsCOMPtr<nsIDOMNSEvent> nsEvent (do_QueryInterface (aEvent));
	NS_ENSURE_TRUE (nsEvent, retval);
	PRBool isTrusted = PR_FALSE;
	nsEvent->GetIsTrusted (&isTrusted);
	if (!isTrusted) return retval;

	/* check for alt/ctrl */
	PRBool isCtrl = PR_FALSE, isAlt = PR_FALSE;
	aEvent->GetCtrlKey (&isCtrl);
	aEvent->GetAltKey (&isAlt);
	if (isCtrl || isAlt) return retval;

	nsCOMPtr<nsIDOMNSUIEvent> uiEvent (do_QueryInterface (aEvent));
	NS_ENSURE_TRUE (uiEvent, retval);

	/* check for already handled event */
	PRBool isPrevented = PR_FALSE;
	uiEvent->GetPreventDefault (&isPrevented);
	if (isPrevented) return retval;

	/* check for form controls */
	nsresult rv;
	nsCOMPtr<nsIDOMEventTarget> target;
	rv = aEvent->GetTarget (getter_AddRefs (target));
	NS_ENSURE_SUCCESS (rv, retval);

	nsCOMPtr<nsIDOMHTMLInputElement> inputElement (do_QueryInterface (target));
	if (inputElement)
	{
		nsString type;
		inputElement->GetType (type);

		nsCString (cType);
		NS_UTF16ToCString (type, NS_CSTRING_ENCODING_UTF8, cType);

		if (g_ascii_strcasecmp (cType.get(), "text") == 0 ||
		    g_ascii_strcasecmp (cType.get(), "password") == 0 ||
		    g_ascii_strcasecmp (cType.get(), "file") == 0) return retval;
	}

	nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea;
	nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
	nsCOMPtr<nsIDOMHTMLIsIndexElement> indexElement;
	nsCOMPtr<nsIDOMHTMLObjectElement> objectElement;
	nsCOMPtr<nsIDOMHTMLEmbedElement> embedElement;

	if ((textArea = do_QueryInterface (target)) ||
	    (selectElement = do_QueryInterface (target)) ||
	    (indexElement = do_QueryInterface (target)) ||
	    (objectElement = do_QueryInterface (target)) ||
	    (embedElement = do_QueryInterface (target))) return retval;

	/* check for design mode */
	nsCOMPtr<nsIDOMNode> node (do_QueryInterface (target, &rv));
	NS_ENSURE_SUCCESS (rv, PR_FALSE);

	nsCOMPtr<nsIDOMDocument> doc;
	rv = node->GetOwnerDocument (getter_AddRefs (doc));
	NS_ENSURE_SUCCESS (rv, retval);

	nsCOMPtr<nsIDOMXULDocument> xul_document (do_QueryInterface(doc, &rv));
	if (xul_document) return retval;

	nsCOMPtr<nsIDOMNSHTMLDocument> htmlDoc (do_QueryInterface (doc));
	if (htmlDoc)
	{
		nsString uDesign;
		rv = htmlDoc->GetDesignMode (uDesign);
		NS_ENSURE_SUCCESS (rv, retval);

		nsCString design;
		NS_UTF16ToCString (uDesign, NS_CSTRING_ENCODING_UTF8, design);

		retval = g_ascii_strcasecmp (design.get(), "on") != 0;
	}
	else
	{
		retval = PR_TRUE;
	}

	return retval;
}
