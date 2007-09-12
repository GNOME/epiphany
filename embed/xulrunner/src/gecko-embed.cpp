/*
 *  Copyright © Christopher Blizzard
 *  Copyright © Ramiro Estrugo
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  ---------------------------------------------------------------------------
 *  Derived from Mozilla.org code, which had the following attributions:
 *
 *  The Original Code is mozilla.org code.
 *
 *  The Initial Developer of the Original Code is
 *  Christopher Blizzard. Portions created by Christopher Blizzard are Copyright © Christopher Blizzard.  All Rights Reserved.
 *  Portions created by the Initial Developer are Copyright © 2001
 *  the Initial Developer. All Rights Reserved.
 *
 *  Contributor(s):
 *    Christopher Blizzard <blizzard@mozilla.org>
 *    Ramiro Estrugo <ramiro@eazel.com>
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */

#include <xpcom-config.h>
#include "config.h"

#include "gecko-embed.h"
#include "gecko-embed-private.h"
#include "gecko-embed-signals.h"
#include "gecko-embed-marshal.h"
#include "gecko-embed-single.h"
#include "gecko-embed-types.h"

#include "gecko-dom-event.h"

#include "GeckoBrowser.h"
#include "EmbedWindow.h"

#ifdef XPCOM_GLUE
#include "nsXPCOMGlue.h"
#endif

// so we can do our get_nsIWebBrowser later...
#include <nsIWebBrowser.h>

#include <stdio.h>

#define GET_OBJECT_CLASS_TYPE(x) G_OBJECT_CLASS_TYPE(x)

class nsIDirectoryServiceProvider;

// class and instance initialization

static void gecko_embed_class_init (GeckoEmbedClass *klass);
static void gecko_embed_init       (GeckoEmbed *embed);

// GtkObject methods

static void gecko_embed_destroy(GtkObject *object);

// GtkWidget methods

static void gecko_embed_realize(GtkWidget *widget);

static void gecko_embed_unrealize(GtkWidget *widget);

static void gecko_embed_size_allocate(GtkWidget *widget, GtkAllocation *allocation);

static void gecko_embed_map(GtkWidget *widget);

static void gecko_embed_unmap(GtkWidget *widget);

#ifdef MOZ_ACCESSIBILITY_ATK
static AtkObject* gecko_embed_get_accessible (GtkWidget *widget);
#endif

static gint handle_child_focus_in(GtkWidget     *aWidget,
		      GdkEventFocus *aGdkFocusEvent,
		      GeckoEmbed   *aEmbed);

static gint handle_child_focus_out(GtkWidget     *aWidget,
		       GdkEventFocus *aGdkFocusEvent,
		       GeckoEmbed   *aEmbed);

static PRInt32 sWidgetCount;

// globals for this type of widget

static GtkBinClass *parent_class;

guint gecko_embed_signals[LAST_EMBED_SIGNAL] = { 0 };

// GtkObject + class-related functions

#define GECKO_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GECKO_TYPE_EMBED, GeckoEmbedPrivate))

struct _GeckoEmbedPrivate
{
  GeckoBrowser *browser;
};

GType
gecko_embed_get_type(void)
{
  static GType type = 0;

  if (!type)
  {
    const GTypeInfo info =
    {
      sizeof (GeckoEmbedClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) gecko_embed_class_init,
      NULL,
      NULL, /* class_data */
      sizeof (GeckoEmbed),
      0, /* n_preallocs */
      (GInstanceInitFunc) gecko_embed_init
    };

    type = g_type_register_static (GTK_TYPE_BIN, "GeckoEmbed",
    				   &info, (GTypeFlags) 0);
  }

  return type;
}


/* GObject methods */

#define GET_BROWSER(x)	(((GeckoEmbed *) x)->priv->browser)

// FIXME split in dispose and finalize
static void
gecko_embed_destroy(GtkObject *object)
{
  GeckoEmbed *embed = GECKO_EMBED (object);
  GeckoBrowser *browser = GET_BROWSER (object);

  if (browser) {

    // Destroy the widget only if it's been Init()ed.
    if(browser->mMozWindowWidget != 0) {
      browser->Destroy();
    }

    delete browser;
    embed->priv->browser = NULL;
  }

  gecko_embed_single_pop_startup();
}

// GtkWidget methods

static void
gecko_embed_realize(GtkWidget *widget)
{
  GeckoEmbed *embed = GECKO_EMBED (widget);
  GeckoEmbedPrivate *priv = embed->priv;
  GeckoBrowser *browser = GET_BROWSER (widget);
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
  GdkWindowAttr attributes;
  int attributes_mask;

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, embed);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  // initialize the window
  nsresult rv;
  rv = browser->Init(embed);
  g_return_if_fail(NS_SUCCEEDED(rv));
  
  PRBool alreadyRealized = PR_FALSE;
  rv = browser->Realize(&alreadyRealized);
  g_return_if_fail(NS_SUCCEEDED(rv));

  // if we're already realized we don't need to hook up to anything below
  if (alreadyRealized)
    return;

  browser->LoadCurrentURI();

  GtkWidget *child_widget = GTK_BIN (widget)->child;
  g_signal_connect_object (child_widget, "focus_in_event",
                           G_CALLBACK (handle_child_focus_in), embed,
                           (GConnectFlags) 0);
  g_signal_connect_object (child_widget, "focus_out_event",
                           G_CALLBACK (handle_child_focus_out), embed,
                           (GConnectFlags) 0);
#if 0
  // connect to the focus out event for the child
  gtk_signal_connect_while_alive(GTK_OBJECT(child_widget),
				 "focus_out_event",
				 GTK_SIGNAL_FUNC(handle_child_focus_out),
				 embed,
				 GTK_OBJECT(child_widget));
  gtk_signal_connect_while_alive(GTK_OBJECT(child_widget),
				 "focus_in_event",
				 GTK_SIGNAL_FUNC(handle_child_focus_in),
				 embed,
				 GTK_OBJECT(child_widget));
#endif
}

static void
gecko_embed_unrealize(GtkWidget *widget)
{
  GeckoEmbed *embed = GECKO_EMBED (widget);
  GeckoEmbedPrivate *priv = embed->priv;
  GeckoBrowser *browser = GET_BROWSER (widget);
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (browser) {
    browser->Unrealize();
  }

  if (GTK_WIDGET_CLASS(parent_class)->unrealize)
    GTK_WIDGET_CLASS(parent_class)->unrealize (widget);
}

static void
gecko_embed_size_allocate (GtkWidget *widget,
                           GtkAllocation *allocation)
{
  GeckoEmbed *embed = GECKO_EMBED (widget);
  GeckoBrowser *browser = GET_BROWSER (widget);
  
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED(widget))
  {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);

    browser->Resize(allocation->width, allocation->height);
  }
}

static void
gecko_embed_map (GtkWidget *widget)
{
  GeckoEmbed *embed = GECKO_EMBED (widget);
  GeckoBrowser *browser = GET_BROWSER (widget);

  GTK_WIDGET_SET_FLAGS(widget, GTK_MAPPED);

  browser->Show();

  gdk_window_show(widget->window);
  
}

static void
gecko_embed_unmap (GtkWidget *widget)
{
  GeckoEmbed *embed = GECKO_EMBED (widget);
  GeckoBrowser *browser = GET_BROWSER (widget);

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);

  gdk_window_hide(widget->window);

  browser->Hide();
}

#ifdef MOZ_ACCESSIBILITY_ATK
static AtkObject*
gecko_embed_get_accessible (GtkWidget *widget)
{
  GeckoEmbed *embed = GECKO_EMBED (widget);
  GeckoBrowser *browser = GET_BROWSER (widget);

  return static_cast<AtkObject *>
                    (browser->GetAtkObjectForCurrentDocument());
}
#endif /* MOZ_ACCESSIBILITY_ATK */

static gint
handle_child_focus_in (GtkWidget     *aWidget,
		       GdkEventFocus *aGdkFocusEvent,
		       GeckoEmbed   *aEmbed)
{
  GeckoBrowser *browser = GET_BROWSER (aEmbed);

  browser->ChildFocusIn();

  return FALSE;
}

static gint
handle_child_focus_out (GtkWidget     *aWidget,
		        GdkEventFocus *aGdkFocusEvent,
		        GeckoEmbed   *aEmbed)
{
  GeckoBrowser *browser = GET_BROWSER (aEmbed);

  browser->ChildFocusOut();
 
  return FALSE;
}

// Widget methods

void
gecko_embed_load_url (GeckoEmbed *embed,
                      const char *url)
{
  GeckoBrowser *browser;
  
  g_return_if_fail(embed != NULL);
  g_return_if_fail(GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

  browser->SetURI(url);

  // If the widget is realized, load the URI.  If it isn't then we
  // will load it later.
  if (GTK_WIDGET_REALIZED(embed))
    browser->LoadCurrentURI();
}

void
gecko_embed_stop_load (GeckoEmbed *embed)
{
  GeckoBrowser *browser;
  
  g_return_if_fail(GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

  if (browser->mNavigation)
    browser->mNavigation->Stop(nsIWebNavigation::STOP_ALL);
}

gboolean
gecko_embed_can_go_back (GeckoEmbed *embed)
{
  PRBool retval = PR_FALSE;
  GeckoBrowser *browser;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), FALSE);

  browser = GET_BROWSER (embed);

  if (browser->mNavigation)
    browser->mNavigation->GetCanGoBack(&retval);
  return retval;
}

gboolean
gecko_embed_can_go_forward (GeckoEmbed *embed)
{
  PRBool retval = PR_FALSE;
  GeckoBrowser *browser;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), FALSE);

  browser = GET_BROWSER (embed);

  if (browser->mNavigation)
    browser->mNavigation->GetCanGoForward(&retval);
  return retval;
}

void
gecko_embed_go_back (GeckoEmbed *embed)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

  if (browser->mNavigation)
    browser->mNavigation->GoBack();
}

void
gecko_embed_go_forward (GeckoEmbed *embed)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

  if (browser->mNavigation)
    browser->mNavigation->GoForward();
}

void
gecko_embed_render_data (GeckoEmbed *embed, const char *data,
			 guint32 len, const char *base_uri,
			 const char *mime_type)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

#if 0
  browser->OpenStream(base_uri, mime_type);
  browser->AppendToStream(data, len);
  browser->CloseStream();
#endif
}

void
gecko_embed_open_stream (GeckoEmbed *embed, const char *base_uri,
			 const char *mime_type)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  browser = GET_BROWSER (embed);

#if 0
  browser->OpenStream(base_uri, mime_type);
#endif
}

void gecko_embed_append_data (GeckoEmbed *embed, const char *data,
			      guint32 len)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  browser = GET_BROWSER (embed);
#if 0
  browser->AppendToStream(data, len);
#endif
}

void
gecko_embed_close_stream (GeckoEmbed *embed)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  browser = GET_BROWSER (embed);
#if 0
  browser->CloseStream();
#endif
}

char *
gecko_embed_get_link_message (GeckoEmbed *embed)
{
  char *retval = nsnull;
  GeckoBrowser *browser;
  nsEmbedCString tmpCString;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), (char *)NULL);

  browser = GET_BROWSER (embed);

  if (browser->mWindow) {
    NS_UTF16ToCString(browser->mWindow->mLinkMessage,
                      NS_CSTRING_ENCODING_UTF8, tmpCString);
    retval = g_strdup(tmpCString.get());
  }

  return retval;
}

char *
gecko_embed_get_js_status (GeckoEmbed *embed)
{
  char *retval = nsnull;
  GeckoBrowser *browser;
  nsEmbedCString tmpCString;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), (char *)NULL);

  browser = GET_BROWSER (embed);

  if (browser->mWindow) {
    NS_UTF16ToCString(browser->mWindow->mJSStatus,
                      NS_CSTRING_ENCODING_UTF8, tmpCString);
    retval = g_strdup(tmpCString.get());
  }

  return retval;
}

char *
gecko_embed_get_title (GeckoEmbed *embed)
{
  char *retval = nsnull;
  GeckoBrowser *browser;
  nsEmbedCString tmpCString;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), (char *)NULL);

  browser = GET_BROWSER (embed);

  if (browser->mWindow) {
    NS_UTF16ToCString(browser->mWindow->mTitle,
                      NS_CSTRING_ENCODING_UTF8, tmpCString);
    retval = g_strdup(tmpCString.get());
  }

  return retval;
}

char *
gecko_embed_get_location (GeckoEmbed *embed)
{
  char *retval = nsnull;
  GeckoBrowser *browser;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), (char *)NULL);

  browser = GET_BROWSER (embed);
  
  if (browser->mURI.Length()) {
    retval = g_strdup(browser->mURI.get());
  }

  return retval;
}

void
gecko_embed_reload (GeckoEmbed *embed,
                    gint32 flags)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

  PRUint32 reloadFlags = 0;
  
  // map the external API to the internal web navigation API.
  switch (flags) {
  case GECKO_EMBED_FLAG_RELOADNORMAL:
    reloadFlags = 0;
    break;
  case GECKO_EMBED_FLAG_RELOADBYPASSCACHE:
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
    break;
  case GECKO_EMBED_FLAG_RELOADBYPASSPROXY:
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
    break;
  case GECKO_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE:
    reloadFlags = (nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY |
		   nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE);
    break;
  case GECKO_EMBED_FLAG_RELOADCHARSETCHANGE:
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE;
    break;
  default:
    reloadFlags = 0;
    break;
  }

  browser->Reload(reloadFlags);
}

void
gecko_embed_set_chrome_mask (GeckoEmbed *embed,
                             guint32 flags)
{
  GeckoBrowser *browser;

  g_return_if_fail (GECKO_IS_EMBED(embed));

  browser = GET_BROWSER (embed);

  browser->SetChromeMask(flags);
}

guint32
gecko_embed_get_chrome_mask (GeckoEmbed *embed)
{
  GeckoBrowser *browser;

  g_return_val_if_fail (GECKO_IS_EMBED(embed), 0);

  browser = GET_BROWSER (embed);

  return browser->mChromeMask;
}

void
gecko_embed_get_nsIWebBrowser (GeckoEmbed *embed,
                               nsIWebBrowser **retval)
{
  GeckoBrowser *browser;
  *retval = nsnull;

  g_return_if_fail (GECKO_IS_EMBED (embed));

  browser = GET_BROWSER (embed);
  
  if (browser->mWindow)
    browser->mWindow->GetWebBrowser(retval);
}

GeckoBrowser *
gecko_embed_get_GeckoBrowser (GeckoEmbed *embed)
{
  g_return_val_if_fail (GECKO_IS_EMBED (embed), nsnull);

  return GET_BROWSER (embed);
}

static void
gecko_embed_init (GeckoEmbed *embed)
{
  embed->priv = GECKO_EMBED_GET_PRIVATE (embed);

  embed->priv->browser = new GeckoBrowser();
  g_return_if_fail (embed->priv->browser);

  gtk_widget_set_name (GTK_WIDGET (embed), "gecko_embed");

  GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(embed), GTK_NO_WINDOW);
}

static void
gecko_embed_class_init (GeckoEmbedClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass); // FIXME GObject
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  parent_class = (GtkBinClass *) g_type_class_peek_parent (klass);

  object_class->destroy = gecko_embed_destroy;
  
  widget_class->realize = gecko_embed_realize;
  widget_class->unrealize = gecko_embed_unrealize;
  widget_class->size_allocate = gecko_embed_size_allocate;
  widget_class->map = gecko_embed_map;
  widget_class->unmap = gecko_embed_unmap;

#ifdef MOZ_ACCESSIBILITY_ATK
  widget_class->get_accessible = gecko_embed_get_accessible;
#endif

  GType dom_param_types[1] = { GECKO_TYPE_DOM_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE };

  gecko_embed_signals[DOM_KEY_DOWN] =
    g_signal_newv ("dom-key-down",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_KEY_PRESS] =
    g_signal_newv ("dom-key-press",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_KEY_UP] =
    g_signal_newv ("dom-key-up",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_DOWN] =
    g_signal_newv ("dom-mouse-down",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_UP] =
    g_signal_newv ("dom-mouse-up",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_CLICK] =
    g_signal_newv ("dom-mouse-click",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_DOUBLE_CLICK] =
    g_signal_newv ("dom-mouse-dbl-click",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_OVER] =
    g_signal_newv ("dom-mouse-over",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_OUT] =
    g_signal_newv ("dom-mouse-out",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_OUT] =
    g_signal_newv ("dom-focus-in",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_OUT] =
    g_signal_newv ("dom-focus-out",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_MOUSE_OUT] =
    g_signal_newv ("dom-activate",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[DOM_CONTEXT_MENU] =
    g_signal_newv ("dom-context-menu",
                   GECKO_TYPE_EMBED,
                   (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                   NULL,
                   g_signal_accumulator_true_handled, NULL,
                   gecko_embed_marshal_BOOLEAN__BOXED,
                   G_TYPE_BOOLEAN,
                   G_N_ELEMENTS (dom_param_types),
		   dom_param_types);

  gecko_embed_signals[OPEN_URI] =
    g_signal_new ("open_uri",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, open_uri),
                  g_signal_accumulator_true_handled, NULL,
                  gecko_embed_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN,
                  1,
                  G_TYPE_STRING);

  gecko_embed_signals[NET_START] =
    g_signal_new ("net_start",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, net_start),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[NET_STOP] =
    g_signal_new ("net_stop",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, net_stop),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[NET_STATE] =
    g_signal_new ("net_state",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, net_state),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__INT_UINT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_UINT);

  gecko_embed_signals[NET_STATE_ALL] =
    g_signal_new ("net_state_all",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, net_state_all),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__STRING_INT_UINT,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_STRING,
                  G_TYPE_INT,
                  G_TYPE_UINT); // static scope? to avoid string copy? or G_TYPE_POINTER as 1st?

  gecko_embed_signals[PROGRESS] =
    g_signal_new ("progress",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, progress),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__INT_INT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_INT);

  gecko_embed_signals[PROGRESS_ALL] =
    g_signal_new ("progress_all",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, progress_all),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__STRING_INT_INT,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_STRING,
                  G_TYPE_INT,
                  G_TYPE_INT); // static scope?

  gecko_embed_signals[SECURITY_CHANGE] =
    g_signal_new ("security_change",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, security_change),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__POINTER_UINT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_POINTER,
                  G_TYPE_UINT);

  gecko_embed_signals[STATUS_CHANGE] =
    g_signal_new ("status_change",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, status_change),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__POINTER_INT_POINTER,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_POINTER,
                  G_TYPE_INT,
                  G_TYPE_POINTER);

  gecko_embed_signals[LINK_MESSAGE] =
    g_signal_new ("link_message",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, link_message),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[JS_STATUS] =
    g_signal_new ("js_status",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, js_status_message),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[LOCATION] =
    g_signal_new ("location",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, location),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[TITLE] =
    g_signal_new ("title",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, title),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[VISIBILITY] =
    g_signal_new ("visibility",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, visibility),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  gecko_embed_signals[DESTROY_BROWSER] =
    g_signal_new ("destroy_browser",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, destroy_browser),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gecko_embed_signals[SIZE_TO] =
    g_signal_new ("size_to",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, size_to),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__INT_INT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_INT);

  gecko_embed_signals[NEW_WINDOW] =
    g_signal_new ("new_window",
                  GECKO_TYPE_EMBED,
                  (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST),
                  G_STRUCT_OFFSET (GeckoEmbedClass, new_window),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_OBJECT,
                  G_TYPE_UINT);

  g_type_class_add_private (object_class, sizeof (GeckoEmbedPrivate));
}

GtkWidget *
gecko_embed_new (void)
{
  return GTK_WIDGET (g_object_new (GECKO_TYPE_EMBED, NULL));
}
