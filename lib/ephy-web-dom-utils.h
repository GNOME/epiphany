/*
 *  Copyright © 2013 Igalia S.L.
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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_WEB_DOM_UTILS_H
#define EPHY_WEB_DOM_UTILS_H

#include <webkitdom/webkitdom.h>
#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>

G_BEGIN_DECLS

gboolean ephy_web_dom_utils_has_modified_forms (WebKitDOMDocument *document);

char * ephy_web_dom_utils_get_application_title (WebKitDOMDocument *document);

gboolean ephy_web_dom_utils_get_best_icon (WebKitDOMDocument *document,
                                           const char        *base_uri,
                                           char             **uri_out,
                                           char             **color_out);

gboolean ephy_web_dom_utils_find_form_auth_elements (WebKitDOMHTMLFormElement *form,
                                                     WebKitDOMNode           **username,
                                                     WebKitDOMNode           **password);

void ephy_web_dom_utils_get_absolute_bottom_for_element (WebKitDOMElement *element,
                                                         double           *x,
                                                         double           *y);

void ephy_web_dom_utils_get_absolute_position_for_element(WebKitDOMElement *element,
                                                          double           *x,
                                                          double           *y);

char *ephy_web_dom_utils_get_selection_as_string (WebKitDOMDOMSelection *selection);
G_END_DECLS

#endif
