/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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
 */

#include "EventContext.h"
#include "nsIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIURI.h"
#include "nsIDOMNSDocument.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
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

#define KEY_CODE 256

EventContext::EventContext ()
{
}

EventContext::~EventContext ()
{
}

nsresult EventContext::Init (EphyWrapper *wrapper)
{
	mWrapper = wrapper;
	mDOMDocument = nsnull;

	return NS_OK;
}

nsresult EventContext::ResolveBaseURL (nsIDocument *doc, const nsAString &relurl, nsACString &url)
{
	nsresult rv;
	nsCOMPtr<nsIURI> base;
#if MOZILLA_SNAPSHOT > 8
	rv = doc->GetBaseURL (getter_AddRefs(base));
#else
	rv = doc->GetBaseURL (*getter_AddRefs(base));
#endif
	if (NS_FAILED(rv)) return rv;

	return base->Resolve (NS_ConvertUCS2toUTF8(relurl), url);
}

nsresult EventContext::ResolveDocumentURL (nsIDocument *doc, const nsAString &relurl, nsACString &url)
{
	nsresult rv;
	nsCOMPtr<nsIURI> uri;
	rv = doc->GetDocumentURL(getter_AddRefs(uri));
	if (NS_FAILED(rv)) return rv;

	return uri->Resolve (NS_ConvertUCS2toUTF8(relurl), url);
}

nsresult EventContext::GetEventContext (nsIDOMEventTarget *EventTarget,
					EphyEmbedEvent *info)
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

		if (tag.Equals(NS_LITERAL_STRING("img"),
			       nsCaseInsensitiveStringComparator()))
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
		else if (tag.Equals(NS_LITERAL_STRING("input"),
				    nsCaseInsensitiveStringComparator()))
		{
			nsCOMPtr<nsIDOMElement> element;
			element = do_QueryInterface (node);
			if (!element) return NS_ERROR_FAILURE;

			NS_NAMED_LITERAL_STRING(attr, "type");
			nsAutoString value;
			element->GetAttribute (attr, value);

			if (value.Equals(NS_LITERAL_STRING("image"),
					 nsCaseInsensitiveStringComparator()))
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
			else if (!value.Equals(NS_LITERAL_STRING("radio"),
					       nsCaseInsensitiveStringComparator()) &&
				 !value.Equals(NS_LITERAL_STRING("submit"),
					       nsCaseInsensitiveStringComparator()) &&
				 !value.Equals(NS_LITERAL_STRING("reset"),
					       nsCaseInsensitiveStringComparator()) &&
				 !value.Equals(NS_LITERAL_STRING("hidden"),
					       nsCaseInsensitiveStringComparator()) &&
				 !value.Equals(NS_LITERAL_STRING("button"),
					       nsCaseInsensitiveStringComparator()) &&
				 !value.Equals(NS_LITERAL_STRING("checkbox"),
					       nsCaseInsensitiveStringComparator()))
			{
				info->context |= EMBED_CONTEXT_INPUT;
			}
		}
		else if (tag.Equals(NS_LITERAL_STRING("textarea"),
				    nsCaseInsensitiveStringComparator()))
		{
			info->context |= EMBED_CONTEXT_INPUT;
		}
		else if (tag.Equals(NS_LITERAL_STRING("object"),
				    nsCaseInsensitiveStringComparator()))
		{
			nsCOMPtr<nsIDOMHTMLObjectElement> object;
			object = do_QueryInterface (node);
			if (!element) return NS_ERROR_FAILURE;

			nsAutoString value;
			object->GetType(value);

			//Forming a substring and confirming it's contents
			//is quicker than doing a Find on the full string
			//and then checking that "image/" is at the beginning
			if (Substring(value, 0, 6).Equals(NS_LITERAL_STRING("image/"),
			    nsCaseInsensitiveStringComparator()))
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

			if (value.Equals(NS_LITERAL_STRING("simple"),
					 nsCaseInsensitiveStringComparator()))
			{
				info->context |= EMBED_CONTEXT_LINK;
				NS_NAMED_LITERAL_STRING (localname_href, "href");
				dom_elem->GetAttributeNS (nspace, localname_href, value);
				
				SetStringProperty ("link", value);
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
			if (tag.Equals(NS_LITERAL_STRING("a"),
				       nsCaseInsensitiveStringComparator()))
			{
				nsCOMPtr <nsIDOMHTMLAnchorElement> anchor =
					do_QueryInterface(node);
				nsAutoString tmp;
				rv = anchor->GetHref (tmp);
				if (NS_FAILED(rv))
					return NS_ERROR_FAILURE;

				if (Substring(tmp, 0, 7).Equals(NS_LITERAL_STRING("mailto:"),
			    	    nsCaseInsensitiveStringComparator()))
				{
					info->context |= EMBED_CONTEXT_EMAIL_LINK;
					const nsAString &address = Substring(tmp, 7, tmp.Length()-7);
					SetStringProperty ("email", address);
				}	
				
				if (anchor && !tmp.IsEmpty()) 
				{
					info->context |= EMBED_CONTEXT_LINK;

					SetStringProperty ("link", tmp);
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

					if (tmp.Equals(NS_LITERAL_STRING("text/smartbookmark"),
						       nsCaseInsensitiveStringComparator()))
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
#ifdef NOT_PORTED
					/* Get the text of the link */
					info->linktext = mozilla_get_link_text (node);
#endif
				}
			
			}
			else if (tag.Equals(NS_LITERAL_STRING("option"),
					    nsCaseInsensitiveStringComparator()))
			{
				info->context = EMBED_CONTEXT_NONE;
				return NS_OK;
			}
			if (tag.Equals(NS_LITERAL_STRING("area"),
				       nsCaseInsensitiveStringComparator()))
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
				}
			}
			else if (tag.Equals(NS_LITERAL_STRING("textarea"),
					    nsCaseInsensitiveStringComparator()) ||
				 tag.Equals(NS_LITERAL_STRING("input"),
					    nsCaseInsensitiveStringComparator()))
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

nsresult EventContext::GetMouseEventInfo (nsIDOMMouseEvent *aMouseEvent, EphyEmbedEvent *info)
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

	if (nodename.Equals(NS_LITERAL_STRING("xul:scrollbarbutton"),
			    nsCaseInsensitiveStringComparator()) ||
	    nodename.Equals(NS_LITERAL_STRING("xul:thumb"),
			    nsCaseInsensitiveStringComparator()) ||
	    nodename.Equals(NS_LITERAL_STRING("xul:vbox"),
			    nsCaseInsensitiveStringComparator()) ||
	    nodename.Equals(NS_LITERAL_STRING("xul:spacer"),
			    nsCaseInsensitiveStringComparator()) ||
	    nodename.Equals(NS_LITERAL_STRING("xul:slider"),
			    nsCaseInsensitiveStringComparator()))
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
	if (mod_key) info->modifier |= GDK_Meta_L;
	
	aMouseEvent->GetCtrlKey(&mod_key);
	if (mod_key) info->modifier |= GDK_CONTROL_MASK;

	return NS_OK;
}

nsresult EventContext::GetKeyEventInfo (nsIDOMKeyEvent *aKeyEvent, EphyEmbedEvent *info)
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
	if (mod_key) info->modifier |= GDK_Meta_L;
	
	aKeyEvent->GetCtrlKey(&mod_key);
	if (mod_key) info->modifier |= GDK_CONTROL_MASK;

	return NS_OK;
}

nsresult EventContext::IsPageFramed (nsIDOMNode *node, PRBool *Framed)
{
	nsresult result;
	
	nsCOMPtr<nsIDOMDocument> mainDocument;
	result = mWrapper->GetMainDOMDocument (getter_AddRefs(mainDocument));
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

nsresult EventContext::SetIntProperty (const char *name, int value)
{

	GValue *val = g_new0 (GValue, 1);

	g_value_init (val, G_TYPE_INT);
	
	g_value_set_int (val, value);

	ephy_embed_event_set_property (mEmbedEvent, 
			     	       name,
			     	       val);

	return NS_OK;
}

nsresult EventContext::SetStringProperty (const char *name, const char *value)
{
	GValue *val = g_new0 (GValue, 1);

	g_value_init (val, G_TYPE_STRING);
	
	g_value_set_string (val, value);
			 
	ephy_embed_event_set_property (mEmbedEvent, 
			     	       name,
			     	       val);

	return NS_OK;
}

nsresult EventContext::SetStringProperty (const char *name, const nsAString &value)
{
	return SetStringProperty (name, NS_ConvertUCS2toUTF8(value).get());
}
