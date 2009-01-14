/*
 *  Copyright © 2008 Gustavo Noronha Silva
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_WEB_VIEW_H
#define EPHY_WEB_VIEW_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_VIEW         (ephy_web_view_get_type ())
#define EPHY_WEB_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_WEB_VIEW, EphyWebView))
#define EPHY_WEB_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_WEB_VIEW, EphyWebViewClass))
#define EPHY_IS_WEB_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_WEB_VIEW))
#define EPHY_IS_WEB_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_WEB_VIEW))
#define EPHY_WEB_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_WEB_VIEW, EphyWebViewClass))

typedef struct _EphyWebViewClass  EphyWebViewClass;
typedef struct _EphyWebView    EphyWebView;
typedef struct _EphyWebViewPrivate  EphyWebViewPrivate;

struct _EphyWebView
{
  WebKitWebView parent;

  /*< private >*/
  EphyWebViewPrivate *priv;
};

struct _EphyWebViewClass
{
  WebKitWebViewClass parent_class;
};

GType        ephy_web_view_get_type            (void);

GtkWidget   *ephy_web_view_new                 (void);

void         ephy_web_view_load_request        (EphyWebView *web_view,
                                                WebKitNetworkRequest *request);

G_END_DECLS

#endif
