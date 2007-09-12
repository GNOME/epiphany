/*
 *  Copyright © Christopher Blizzard
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
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */
 
#include <xpcom-config.h>
#include <config.h>

#include "gecko-init.h"
#include "gecko-embed.h"
#include "gecko-embed-single.h"
#include "gecko-dom-event.h"
#include "gecko-dom-event-internal.h"
#include "gecko-embed-types.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// mozilla specific headers
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMUIEvent.h"

#include <nsStringAPI.h>
#include <nsCOMPtr.h>

#define XPCOM_GLUE
#include <nsXPCOMGlue.h>

typedef struct _TestGeckoBrowser {
  GtkWidget  *topLevelWindow;
  GtkWidget  *topLevelVBox;
  GtkWidget  *menuBar;
  GtkWidget  *fileMenuItem;
  GtkWidget  *fileMenu;
  GtkWidget  *fileOpenNewBrowser;
  GtkWidget  *fileStream;
  GtkWidget  *fileClose;
  GtkWidget  *fileQuit;
  GtkWidget  *toolbarHBox;
  GtkWidget  *toolbar;
  GtkWidget  *backButton;
  GtkWidget  *stopButton;
  GtkWidget  *forwardButton;
  GtkWidget  *reloadButton;
  GtkWidget  *urlEntry;
  GtkWidget  *mozEmbed;
  GtkWidget  *progressAreaHBox;
  GtkWidget  *progressBar;
  GtkWidget  *statusAlign;
  GtkWidget  *statusBar;
  const char *statusMessage;
  int         loadPercent;
  int         bytesLoaded;
  int         maxBytesLoaded;
  char       *tempMessage;
  gboolean menuBarOn;
  gboolean toolBarOn;
  gboolean locationBarOn;
  gboolean statusBarOn;

} TestGeckoBrowser;

// the list of browser windows currently open
GList *browser_list = g_list_alloc();

static TestGeckoBrowser *new_gecko_browser    (guint32 chromeMask);
static void            set_browser_visibility (TestGeckoBrowser *browser,
					       gboolean visibility);

static int num_browsers = 0;

// callbacks from the UI
static void     back_clicked_cb    (GtkButton   *button, 
				    TestGeckoBrowser *browser);
static void     stop_clicked_cb    (GtkButton   *button,
				    TestGeckoBrowser *browser);
static void     forward_clicked_cb (GtkButton   *button,
				    TestGeckoBrowser *browser);
static void     reload_clicked_cb  (GtkButton   *button,
				    TestGeckoBrowser *browser);
static void     url_activate_cb    (GtkEditable *widget, 
				    TestGeckoBrowser *browser);
static void     menu_open_new_cb   (GtkMenuItem *menuitem,
				    TestGeckoBrowser *browser);
static void     menu_stream_cb     (GtkMenuItem *menuitem,
				    TestGeckoBrowser *browser);
static void     menu_close_cb      (GtkMenuItem *menuitem,
				    TestGeckoBrowser *browser);
static void     menu_quit_cb       (GtkMenuItem *menuitem,
				    TestGeckoBrowser *browser);
static gboolean delete_cb          (GtkWidget *widget, GdkEventAny *event,
				    TestGeckoBrowser *browser);
static void     destroy_cb         (GtkWidget *widget,
				    TestGeckoBrowser *browser);

// callbacks from the widget
static void location_changed_cb  (GeckoEmbed *embed, TestGeckoBrowser *browser);
static void title_changed_cb     (GeckoEmbed *embed, TestGeckoBrowser *browser);
static void load_started_cb      (GeckoEmbed *embed, TestGeckoBrowser *browser);
static void load_finished_cb     (GeckoEmbed *embed, TestGeckoBrowser *browser);
static void net_state_change_cb  (GeckoEmbed *embed, gint flags,
				  guint status, TestGeckoBrowser *browser);
static void net_state_change_all_cb (GeckoEmbed *embed, const char *uri,
				     gint flags, guint status,
				     TestGeckoBrowser *browser);
static void progress_change_cb   (GeckoEmbed *embed, gint cur, gint max,
				  TestGeckoBrowser *browser);
static void progress_change_all_cb (GeckoEmbed *embed, const char *uri,
				    gint cur, gint max,
				    TestGeckoBrowser *browser);
static void link_message_cb      (GeckoEmbed *embed, TestGeckoBrowser *browser);
static void js_status_cb         (GeckoEmbed *embed, TestGeckoBrowser *browser);
static void new_window_cb        (GeckoEmbed *embed,
				  GeckoEmbed **retval, guint chromemask,
				  TestGeckoBrowser *browser);
static void visibility_cb        (GeckoEmbed *embed, 
				  gboolean visibility,
				  TestGeckoBrowser *browser);
static void destroy_brsr_cb      (GeckoEmbed *embed, TestGeckoBrowser *browser);
static gint open_uri_cb          (GeckoEmbed *embed, const char *uri,
				  TestGeckoBrowser *browser);
static void size_to_cb           (GeckoEmbed *embed, gint width,
				  gint height, TestGeckoBrowser *browser);
static gboolean dom_key_down_cb      (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_key_press_cb     (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_key_up_cb        (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_mouse_down_cb    (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_mouse_up_cb      (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_mouse_click_cb   (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_mouse_dbl_click_cb (GeckoEmbed *embed, 
				  GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_mouse_over_cb    (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_mouse_out_cb     (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_activate_cb      (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_focus_in_cb      (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_focus_out_cb     (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);
static gboolean dom_context_menu_cb     (GeckoEmbed *, GeckoDOMEvent *event,
				  TestGeckoBrowser *browser);

// callbacks from the singleton object
static void new_window_orphan_cb (GeckoEmbedSingle *embed,
				  GeckoEmbed **retval, guint chromemask,
				  gpointer data);

// some utility functions
static void update_status_bar_text  (TestGeckoBrowser *browser);
static void update_temp_message     (TestGeckoBrowser *browser,
				     const char *message);
static void update_nav_buttons      (TestGeckoBrowser *browser);

int
main(int argc, char **argv)
{
  //g_setenv ("MOZILLA_FIVE_HOME", GECKO_HOME, FALSE);

  gtk_init(&argc, &argv);

  static const GREVersionRange greVersion = {
    "1.9a", PR_TRUE,
    "2", PR_TRUE
  };

  char xpcomPath[PATH_MAX];

  nsresult rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0,
                                             xpcomPath, sizeof(xpcomPath));
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Couldn't find a compatible GRE.\n");
    return 1;
  }

  rv = XPCOMGlueStartup(xpcomPath);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Couldn't start XPCOM.");
    return 1;
  }

  char *lastSlash = strrchr(xpcomPath, '/');
  if (lastSlash)
    *lastSlash = '\0';

  
  char *home_path;
  char *full_path;
  home_path = getenv("HOME");
  if (!home_path) {
    fprintf(stderr, "Failed to get HOME\n");
    exit(1);
  }

  
  full_path = g_strdup_printf("%s/%s", home_path, ".TestGtkEmbed");
  
  if (!gecko_init_with_profile (xpcomPath, full_path, "TestGtkEmbed")) {
    fprintf(stderr, "Failed gecko_init_with_profile\n");
    exit(1);
  }

  // get the singleton object and hook up to its new window callback
  // so we can create orphaned windows.

  GeckoEmbedSingle *single;

  single = gecko_embed_single_get();
  if (!single) {
    fprintf(stderr, "Failed to get singleton embed object!\n");
    exit(1);
  }

  g_signal_connect (single, "new-window-orphan",
		    G_CALLBACK (new_window_orphan_cb), NULL);


  TestGeckoBrowser *browser = new_gecko_browser(GECKO_EMBED_FLAG_DEFAULTCHROME);

  // set our minimum size
  gtk_widget_set_usize(browser->mozEmbed, 400, 400);

  set_browser_visibility(browser, TRUE);

  if (argc > 1)
    gecko_embed_load_url(GECKO_EMBED(browser->mozEmbed), argv[1]);

  gtk_main();

  gecko_shutdown ();
}

static TestGeckoBrowser *
new_gecko_browser(guint32 chromeMask)
{
  guint32         actualChromeMask = chromeMask;
  TestGeckoBrowser *browser = 0;

  num_browsers++;

  browser = g_new0(TestGeckoBrowser, 1);

  browser_list = g_list_prepend(browser_list, browser);

  browser->menuBarOn = FALSE;
  browser->toolBarOn = FALSE;
  browser->locationBarOn = FALSE;
  browser->statusBarOn = FALSE;

  g_print("new_gecko_browser\n");

//   if (chromeMask == GECKO_EMBED_FLAG_DEFAULTCHROME)
//     actualChromeMask = GECKO_EMBED_FLAG_ALLCHROME;

  /* FIXMEchpe find out WHY the chrome masks we get from gecko as so weird */
  actualChromeMask = 0xffffffff;

  if (actualChromeMask & GECKO_EMBED_FLAG_MENUBARON)
  {
    browser->menuBarOn = TRUE;
    g_print("\tmenu bar\n");
  }
  if (actualChromeMask & GECKO_EMBED_FLAG_TOOLBARON)
  {
    browser->toolBarOn = TRUE;
    g_print("\ttool bar\n");
  }
  if (actualChromeMask & GECKO_EMBED_FLAG_LOCATIONBARON)
  {
    browser->locationBarOn = TRUE;
    g_print("\tlocation bar\n");
  }
  if (actualChromeMask & GECKO_EMBED_FLAG_STATUSBARON)
  {
    browser->statusBarOn = TRUE;
    g_print("\tstatus bar\n");
  }

  // create our new toplevel window
  browser->topLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // new vbox
  browser->topLevelVBox = gtk_vbox_new(FALSE, 0);
  // add it to the toplevel window
  gtk_container_add(GTK_CONTAINER(browser->topLevelWindow),
		    browser->topLevelVBox);
  // create our menu bar
  browser->menuBar = gtk_menu_bar_new();
  // create the file menu
  browser->fileMenuItem = gtk_menu_item_new_with_label("File");
  browser->fileMenu = gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(browser->fileMenuItem),
			     browser->fileMenu);

  browser->fileOpenNewBrowser = 
    gtk_menu_item_new_with_label("Open New Browser");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileOpenNewBrowser);
  
  browser->fileStream =
    gtk_menu_item_new_with_label("Test Stream");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileStream);

  browser->fileClose =
    gtk_menu_item_new_with_label("Close");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileClose);

  browser->fileQuit =
    gtk_menu_item_new_with_label("Quit");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileQuit);
  
  // append it
  gtk_menu_bar_append(GTK_MENU_BAR(browser->menuBar), browser->fileMenuItem);

  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox),
		     browser->menuBar,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // create the hbox that will contain the toolbar and the url text entry bar
  browser->toolbarHBox = gtk_hbox_new(FALSE, 0);
  // add that hbox to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), 
		     browser->toolbarHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // new horiz toolbar with buttons + icons
  browser->toolbar = gtk_toolbar_new();
  gtk_toolbar_set_orientation(GTK_TOOLBAR(browser->toolbar),
			      GTK_ORIENTATION_HORIZONTAL);
  gtk_toolbar_set_style(GTK_TOOLBAR(browser->toolbar),
			GTK_TOOLBAR_BOTH);

  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->toolbarHBox), browser->toolbar,
		   FALSE, // expand
		   FALSE, // fill
		   0);    // padding
  // new back button
  browser->backButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Back",
			    "Go Back",
			    "Go Back",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(back_clicked_cb),
			    browser);
  // new stop button
  browser->stopButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Stop",
			    "Stop",
			    "Stop",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(stop_clicked_cb),
			    browser);
  // new forward button
  browser->forwardButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Forward",
			    "Forward",
			    "Forward",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(forward_clicked_cb),
			    browser);
  // new reload button
  browser->reloadButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Reload",
			    "Reload",
			    "Reload",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(reload_clicked_cb),
			    browser);
  // create the url text entry
  browser->urlEntry = gtk_entry_new();
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->toolbarHBox), browser->urlEntry,
		     TRUE, // expand
		     TRUE, // fill
		     0);    // padding
  // create our new gtk moz embed widget
  browser->mozEmbed = gecko_embed_new();
  // add it to the toplevel vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->mozEmbed,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding
  // create the new hbox for the progress area
  browser->progressAreaHBox = gtk_hbox_new(FALSE, 0);
  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->progressAreaHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);   // padding
  // create our new progress bar
  browser->progressBar = gtk_progress_bar_new();
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->progressAreaHBox), browser->progressBar,
		     FALSE, // expand
		     FALSE, // fill
		     0); // padding
  
  // create our status area and the alignment object that will keep it
  // from expanding
  browser->statusAlign = gtk_alignment_new(0, 0, 1, 1);
  gtk_widget_set_usize(browser->statusAlign, 1, -1);
  // create the status bar
  browser->statusBar = gtk_statusbar_new();
  gtk_container_add(GTK_CONTAINER(browser->statusAlign), browser->statusBar);
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->progressAreaHBox), browser->statusAlign,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding
  // by default none of the buttons are marked as sensitive.
  gtk_widget_set_sensitive(browser->backButton, FALSE);
  gtk_widget_set_sensitive(browser->stopButton, FALSE);
  gtk_widget_set_sensitive(browser->forwardButton, FALSE);
  gtk_widget_set_sensitive(browser->reloadButton, FALSE);
  
  // catch the destruction of the toplevel window
  gtk_signal_connect(GTK_OBJECT(browser->topLevelWindow), "delete_event",
		     GTK_SIGNAL_FUNC(delete_cb), browser);

  // hook up the activate signal to the right callback
  gtk_signal_connect(GTK_OBJECT(browser->urlEntry), "activate",
		     GTK_SIGNAL_FUNC(url_activate_cb), browser);

  // hook up to the open new browser activation
  gtk_signal_connect(GTK_OBJECT(browser->fileOpenNewBrowser), "activate",
		     GTK_SIGNAL_FUNC(menu_open_new_cb), browser);
  // hook up to the stream test
  gtk_signal_connect(GTK_OBJECT(browser->fileStream), "activate",
		     GTK_SIGNAL_FUNC(menu_stream_cb), browser);
  // close this window
  gtk_signal_connect(GTK_OBJECT(browser->fileClose), "activate",
		     GTK_SIGNAL_FUNC(menu_close_cb), browser);
  // quit the application
  gtk_signal_connect(GTK_OBJECT(browser->fileQuit), "activate",
		     GTK_SIGNAL_FUNC(menu_quit_cb), browser);

  // hook up the location change to update the urlEntry
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "location",
		     GTK_SIGNAL_FUNC(location_changed_cb), browser);
  // hook up the title change to update the window title
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "title",
		     GTK_SIGNAL_FUNC(title_changed_cb), browser);
  // hook up the start and stop signals
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_start",
		     GTK_SIGNAL_FUNC(load_started_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_stop",
		     GTK_SIGNAL_FUNC(load_finished_cb), browser);
  // hook up to the change in network status
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state",
		     GTK_SIGNAL_FUNC(net_state_change_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state_all",
		     GTK_SIGNAL_FUNC(net_state_change_all_cb), browser);
  // hookup to changes in progress
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress",
		     GTK_SIGNAL_FUNC(progress_change_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress_all",
		     GTK_SIGNAL_FUNC(progress_change_all_cb), browser);
  // hookup to changes in over-link message
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "link_message",
		     GTK_SIGNAL_FUNC(link_message_cb), browser);
  // hookup to changes in js status message
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "js_status",
		     GTK_SIGNAL_FUNC(js_status_cb), browser);
  // hookup to see whenever a new window is requested
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "new_window",
		     GTK_SIGNAL_FUNC(new_window_cb), browser);
  // hookup to any requested visibility changes
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "visibility",
		     GTK_SIGNAL_FUNC(visibility_cb), browser);
  // hookup to the signal that says that the browser requested to be
  // destroyed
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "destroy_browser",
		     GTK_SIGNAL_FUNC(destroy_brsr_cb), browser);
  // hookup to the signal that is called when someone clicks on a link
  // to load a new uri
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "open_uri",
		     GTK_SIGNAL_FUNC(open_uri_cb), browser);
  // this signal is emitted when there's a request to change the
  // containing browser window to a certain height, like with width
  // and height args for a window.open in javascript
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "size_to",
		     GTK_SIGNAL_FUNC(size_to_cb), browser);
  // key event signals
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_down",
		     GTK_SIGNAL_FUNC(dom_key_down_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_press",
		     GTK_SIGNAL_FUNC(dom_key_press_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_up",
		     GTK_SIGNAL_FUNC(dom_key_up_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_down",
		     GTK_SIGNAL_FUNC(dom_mouse_down_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_up",
		     GTK_SIGNAL_FUNC(dom_mouse_up_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_click",
		     GTK_SIGNAL_FUNC(dom_mouse_click_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_dbl_click",
		     GTK_SIGNAL_FUNC(dom_mouse_dbl_click_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_over",
		     GTK_SIGNAL_FUNC(dom_mouse_over_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_out",
		     GTK_SIGNAL_FUNC(dom_mouse_out_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_activate",
		     GTK_SIGNAL_FUNC(dom_activate_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_focus_in",
		     GTK_SIGNAL_FUNC(dom_focus_in_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_focus_out",
		     GTK_SIGNAL_FUNC(dom_focus_out_cb), browser);
  g_signal_connect(browser->mozEmbed, "dom-context-menu",
		     GTK_SIGNAL_FUNC(dom_context_menu_cb), browser);
  // hookup to when the window is destroyed
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "destroy",
		     GTK_SIGNAL_FUNC(destroy_cb), browser);
  
  // set the chrome type so it's stored in the object
  gecko_embed_set_chrome_mask(GECKO_EMBED(browser->mozEmbed),
				actualChromeMask);

  gtk_widget_show_all (browser->topLevelVBox);

  return browser;
}

void
set_browser_visibility (TestGeckoBrowser *browser, gboolean visibility)
{
  if (!visibility)
  {
    gtk_widget_hide(browser->topLevelWindow);
    return;
  }

  if (browser->menuBarOn)
    gtk_widget_show_all(browser->menuBar);
  else
    gtk_widget_hide_all(browser->menuBar);

  // since they are on the same line here...
  if (browser->toolBarOn || browser->locationBarOn)
    gtk_widget_show_all(browser->toolbarHBox);
  else 
    gtk_widget_hide_all(browser->toolbarHBox);

  if (browser->statusBarOn)
    gtk_widget_show_all(browser->progressAreaHBox);
  else
    gtk_widget_hide_all(browser->progressAreaHBox);

  gtk_widget_show(browser->mozEmbed);
  gtk_widget_show(browser->topLevelVBox);
  gtk_widget_show(browser->topLevelWindow);
}

void
back_clicked_cb (GtkButton *button, TestGeckoBrowser *browser)
{
  gecko_embed_go_back(GECKO_EMBED(browser->mozEmbed));
}

void
stop_clicked_cb (GtkButton *button, TestGeckoBrowser *browser)
{
  g_print("stop_clicked_cb\n");
  gecko_embed_stop_load(GECKO_EMBED(browser->mozEmbed));
}

void
forward_clicked_cb (GtkButton *button, TestGeckoBrowser *browser)
{
  g_print("forward_clicked_cb\n");
  gecko_embed_go_forward(GECKO_EMBED(browser->mozEmbed));
}

void
reload_clicked_cb  (GtkButton *button, TestGeckoBrowser *browser)
{
  g_print("reload_clicked_cb\n");
  GdkModifierType state = (GdkModifierType)0;
  gint x, y;
  gdk_window_get_pointer(NULL, &x, &y, &state);
  
  gecko_embed_reload(GECKO_EMBED(browser->mozEmbed),
		       (state & GDK_SHIFT_MASK) ?
		       GECKO_EMBED_FLAG_RELOADBYPASSCACHE : 
		       GECKO_EMBED_FLAG_RELOADNORMAL);
}

void 
stream_clicked_cb  (GtkButton   *button, TestGeckoBrowser *browser)
{
  const char *data;
  const char *data2;
  data = "<html>Hi";
  data2 = " there</html>\n";
  g_print("stream_clicked_cb\n");
  gecko_embed_open_stream(GECKO_EMBED(browser->mozEmbed),
			    "file://", "text/html");
  gecko_embed_append_data(GECKO_EMBED(browser->mozEmbed),
			    data, strlen(data));
  gecko_embed_append_data(GECKO_EMBED(browser->mozEmbed),
			    data2, strlen(data2));
  gecko_embed_close_stream(GECKO_EMBED(browser->mozEmbed));
}

void
url_activate_cb    (GtkEditable *widget, TestGeckoBrowser *browser)
{
  gchar *text = gtk_editable_get_chars(widget, 0, -1);
  g_print("loading url %s\n", text);
  gecko_embed_load_url(GECKO_EMBED(browser->mozEmbed), text);
  g_free(text);
}

void
menu_open_new_cb   (GtkMenuItem *menuitem, TestGeckoBrowser *browser)
{
  g_print("opening new browser.\n");
  TestGeckoBrowser *newBrowser = 
    new_gecko_browser(GECKO_EMBED_FLAG_DEFAULTCHROME);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  set_browser_visibility(newBrowser, TRUE);
}

void
menu_stream_cb     (GtkMenuItem *menuitem, TestGeckoBrowser *browser)
{
  g_print("menu_stream_cb\n");
  const char *data;
  const char *data2;
  data = "<html>Hi";
  data2 = " there</html>\n";
  g_print("stream_clicked_cb\n");
  gecko_embed_open_stream(GECKO_EMBED(browser->mozEmbed),
			    "file://", "text/html");
  gecko_embed_append_data(GECKO_EMBED(browser->mozEmbed),
			    data, strlen(data));
  gecko_embed_append_data(GECKO_EMBED(browser->mozEmbed),
			    data2, strlen(data2));
  gecko_embed_close_stream(GECKO_EMBED(browser->mozEmbed));
}

void
menu_close_cb (GtkMenuItem *menuitem, TestGeckoBrowser *browser)
{
  gtk_widget_destroy(browser->topLevelWindow);
}

void
menu_quit_cb (GtkMenuItem *menuitem, TestGeckoBrowser *browser)
{
  TestGeckoBrowser *tmpBrowser;
  GList *tmp_list = browser_list;
  tmpBrowser = (TestGeckoBrowser *)tmp_list->data;
  while (tmpBrowser) {
    tmp_list = tmp_list->next;
    gtk_widget_destroy(tmpBrowser->topLevelWindow);
    tmpBrowser = (TestGeckoBrowser *)tmp_list->data;
  }
}

gboolean
delete_cb(GtkWidget *widget, GdkEventAny *event, TestGeckoBrowser *browser)
{
  g_print("delete_cb\n");
  gtk_widget_destroy(widget);
  return TRUE;
}

void
destroy_cb         (GtkWidget *widget, TestGeckoBrowser *browser)
{
  GList *tmp_list;
  g_print("destroy_cb\n");
  num_browsers--;
  tmp_list = g_list_find(browser_list, browser);
  browser_list = g_list_remove_link(browser_list, tmp_list);
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (num_browsers == 0)
    gtk_main_quit();
}

void
location_changed_cb (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
  char *newLocation;
  int   newPosition = 0;
  g_print("location_changed_cb\n");
  newLocation = gecko_embed_get_location(embed);
  if (newLocation)
  {
    gtk_editable_delete_text(GTK_EDITABLE(browser->urlEntry), 0, -1);
    gtk_editable_insert_text(GTK_EDITABLE(browser->urlEntry),
			     newLocation, strlen(newLocation), &newPosition);
    g_free(newLocation);
  }
  else
    g_print("failed to get location!\n");
  // always make sure to clear the tempMessage.  it might have been
  // set from the link before a click and we wouldn't have gotten the
  // callback to unset it.
  update_temp_message(browser, 0);
  // update the nav buttons on a location change
  update_nav_buttons(browser);
}

void
title_changed_cb    (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
  char *newTitle;
  g_print("title_changed_cb\n");
  newTitle = gecko_embed_get_title(embed);
  if (newTitle)
  {
    gtk_window_set_title(GTK_WINDOW(browser->topLevelWindow), newTitle);
    g_free(newTitle);
  }
  
}

void
load_started_cb     (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
  g_print("load_started_cb\n");
  gtk_widget_set_sensitive(browser->stopButton, TRUE);
  gtk_widget_set_sensitive(browser->reloadButton, FALSE);
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  update_status_bar_text(browser);
}

void
load_finished_cb    (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
  g_print("load_finished_cb\n");
  gtk_widget_set_sensitive(browser->stopButton, FALSE);
  gtk_widget_set_sensitive(browser->reloadButton, TRUE);
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  update_status_bar_text(browser);
  gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), 0);
}


void
net_state_change_cb (GeckoEmbed *embed, gint flags, guint status,
		     TestGeckoBrowser *browser)
{
  g_print("net_state_change_cb %d\n", flags);
  if (flags & GECKO_EMBED_FLAG_IS_REQUEST) {
    if (flags & GECKO_EMBED_FLAG_REDIRECTING)
    browser->statusMessage = "Redirecting to site...";
    else if (flags & GECKO_EMBED_FLAG_TRANSFERRING)
    browser->statusMessage = "Transferring data from site...";
    else if (flags & GECKO_EMBED_FLAG_NEGOTIATING)
    browser->statusMessage = "Waiting for authorization...";
  }

  if (status == GECKO_EMBED_STATUS_FAILED_DNS)
    browser->statusMessage = "Site not found.";
  else if (status == GECKO_EMBED_STATUS_FAILED_CONNECT)
    browser->statusMessage = "Failed to connect to site.";
  else if (status == GECKO_EMBED_STATUS_FAILED_TIMEOUT)
    browser->statusMessage = "Failed due to connection timeout.";
  else if (status == GECKO_EMBED_STATUS_FAILED_USERCANCELED)
    browser->statusMessage = "User canceled connecting to site.";

  if (flags & GECKO_EMBED_FLAG_IS_DOCUMENT) {
    if (flags & GECKO_EMBED_FLAG_START)
      browser->statusMessage = "Loading site...";
    else if (flags & GECKO_EMBED_FLAG_STOP)
      browser->statusMessage = "Done.";
  }

  update_status_bar_text(browser);
  
}

void net_state_change_all_cb (GeckoEmbed *embed, const char *uri,
				     gint flags, guint status,
				     TestGeckoBrowser *browser)
{
  //  g_print("net_state_change_all_cb %s %d %d\n", uri, flags, status);
}

void progress_change_cb   (GeckoEmbed *embed, gint cur, gint max,
			   TestGeckoBrowser *browser)
{
  g_print("progress_change_cb cur %d max %d\n", cur, max);

  // avoid those pesky divide by zero errors
  if (max < 1)
  {
    gtk_progress_set_activity_mode(GTK_PROGRESS(browser->progressBar), FALSE);
    browser->loadPercent = 0;
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = 0;
    update_status_bar_text(browser);
  }
  else
  {
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = max;
    if (cur > max)
      browser->loadPercent = 100;
    else
      browser->loadPercent = (cur * 100) / max;
    update_status_bar_text(browser);
    gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), browser->loadPercent / 100.0);
  }
  
}

void progress_change_all_cb (GeckoEmbed *embed, const char *uri,
			     gint cur, gint max,
			     TestGeckoBrowser *browser)
{
  //g_print("progress_change_all_cb %s cur %d max %d\n", uri, cur, max);
}

void
link_message_cb      (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
  char *message;
  g_print("link_message_cb\n");
  message = gecko_embed_get_link_message(embed);
  if (!message || !*message)
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
js_status_cb (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
 char *message;
  g_print("js_status_cb\n");
  message = gecko_embed_get_js_status(embed);
  if (!message || !*message)
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
new_window_cb (GeckoEmbed *embed, GeckoEmbed **newEmbed, guint chromemask, TestGeckoBrowser *browser)
{
  g_print("new_window_cb\n");
  g_print("embed is %p chromemask is %d\n", (void *)embed, chromemask);
  TestGeckoBrowser *newBrowser = new_gecko_browser(chromemask);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  *newEmbed = GECKO_EMBED(newBrowser->mozEmbed);
  g_print("new browser is %p\n", (void *)*newEmbed);
}

void
visibility_cb (GeckoEmbed *embed, gboolean visibility, TestGeckoBrowser *browser)
{
  g_print("visibility_cb %d\n", visibility);
  set_browser_visibility(browser, visibility);
}

void
destroy_brsr_cb      (GeckoEmbed *embed, TestGeckoBrowser *browser)
{
  g_print("destroy_brsr_cb\n");
  gtk_widget_destroy(browser->topLevelWindow);
}

gint
open_uri_cb          (GeckoEmbed *embed, const char *uri, TestGeckoBrowser *browser)
{
  g_print("open_uri_cb %s\n", uri);

  // interrupt this test load
  if (!strcmp(uri, "http://people.redhat.com/blizzard/monkeys.txt"))
    return TRUE;
  // don't interrupt anything
  return FALSE;
}

void
size_to_cb (GeckoEmbed *embed, gint width, gint height,
	    TestGeckoBrowser *browser)
{
  g_print("*** size_to_cb %d %d\n", width, height);
  gtk_widget_set_usize(browser->mozEmbed, width, height);
}

gboolean dom_key_down_cb      (GeckoEmbed *embed,
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  nsIDOMEvent *domevent = gecko_dom_event_get_I (gecko_event);
  nsCOMPtr<nsIDOMKeyEvent> event (do_QueryInterface (domevent));

  PRUint32 keyCode = 0;
  //  g_print("dom_key_down_cb\n");
  event->GetKeyCode(&keyCode);
  // g_print("key code is %d\n", keyCode);
  return NS_OK;
}

gboolean dom_key_press_cb     (GeckoEmbed *embed,
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  nsIDOMEvent *domevent = gecko_dom_event_get_I (gecko_event);
  nsCOMPtr<nsIDOMKeyEvent> event (do_QueryInterface (domevent));

  PRUint32 keyCode = 0;
  // g_print("dom_key_press_cb\n");
  event->GetCharCode(&keyCode);
  // g_print("char code is %d\n", keyCode);
  return NS_OK;
}

gboolean dom_key_up_cb        (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  nsIDOMEvent *domevent = gecko_dom_event_get_I (gecko_event);
  nsCOMPtr<nsIDOMKeyEvent> event (do_QueryInterface (domevent));

  PRUint32 keyCode = 0;
  // g_print("dom_key_up_cb\n");
  event->GetKeyCode(&keyCode);
  // g_print("key code is %d\n", keyCode);
  return NS_OK;
}

gboolean dom_mouse_down_cb    (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //  g_print("dom_mouse_down_cb\n");
  return NS_OK;
 }

gboolean dom_mouse_up_cb      (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //  g_print("dom_mouse_up_cb\n");
  return NS_OK;
}

gboolean dom_mouse_click_cb   (GeckoEmbed *embed,
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  nsIDOMEvent *domevent = gecko_dom_event_get_I (gecko_event);
  nsCOMPtr<nsIDOMMouseEvent> event (do_QueryInterface (domevent));

  //  g_print("dom_mouse_click_cb\n");
  PRUint16 button;
  event->GetButton(&button);
  printf("button was %d\n", button);
  return NS_OK;
}

gboolean dom_mouse_dbl_click_cb (GeckoEmbed *embed, 
			     GeckoDOMEvent *gecko_event,
			     TestGeckoBrowser *browser)
{
  //  g_print("dom_mouse_dbl_click_cb\n");
  return NS_OK;
}

gboolean dom_mouse_over_cb    (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //g_print("dom_mouse_over_cb\n");
  return NS_OK;
}

gboolean dom_mouse_out_cb     (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //g_print("dom_mouse_out_cb\n");
  return NS_OK;
}

gboolean dom_activate_cb      (GeckoEmbed *embed,
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //g_print("dom_activate_cb\n");
  return NS_OK;
}

gboolean dom_focus_in_cb      (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //g_print("dom_focus_in_cb\n");
  return NS_OK;
}

gboolean dom_focus_out_cb     (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  //g_print("dom_focus_out_cb\n");
  return NS_OK;
}

gboolean dom_context_menu_cb     (GeckoEmbed *embed, 
			   GeckoDOMEvent *gecko_event,
			   TestGeckoBrowser *browser)
{
  g_print("dom_context_menu_cb\n");
  return NS_OK;
}

void new_window_orphan_cb (GeckoEmbedSingle *embed,
			   GeckoEmbed **retval, guint chromemask,
			   gpointer data)
{
  g_print("new_window_orphan_cb\n");
  g_print("chromemask is %d\n", chromemask);
  TestGeckoBrowser *newBrowser = new_gecko_browser(chromemask);
  *retval = GECKO_EMBED(newBrowser->mozEmbed);
  g_print("new browser is %p\n", (void *)*retval);
}

// utility functions

void
update_status_bar_text(TestGeckoBrowser *browser)
{
  gchar message[256];
  
  gtk_statusbar_pop(GTK_STATUSBAR(browser->statusBar), 1);
  if (browser->tempMessage)
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusBar), 1, browser->tempMessage);
  else
  {
    if (browser->loadPercent)
    {
      g_snprintf(message, 255, "%s (%d%% complete, %d bytes of %d loaded)", browser->statusMessage, browser->loadPercent, browser->bytesLoaded, browser->maxBytesLoaded);
    }
    else if (browser->bytesLoaded)
    {
      g_snprintf(message, 255, "%s (%d bytes loaded)", browser->statusMessage, browser->bytesLoaded);
    }
    else if (browser->statusMessage == NULL)
    {
      g_snprintf(message, 255, " ");
    }
    else
    {
      g_snprintf(message, 255, "%s", browser->statusMessage);
    }
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusBar), 1, message);
  }
}

void
update_temp_message(TestGeckoBrowser *browser, const char *message)
{
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (message)
    browser->tempMessage = g_strdup(message);
  else
    browser->tempMessage = 0;
  // now that we've updated the temp message, redraw the status bar
  update_status_bar_text(browser);
}


void
update_nav_buttons      (TestGeckoBrowser *browser)
{
  gboolean can_go_back;
  gboolean can_go_forward;
  can_go_back = gecko_embed_can_go_back(GECKO_EMBED(browser->mozEmbed));
  can_go_forward = gecko_embed_can_go_forward(GECKO_EMBED(browser->mozEmbed));
  if (can_go_back)
    gtk_widget_set_sensitive(browser->backButton, TRUE);
  else
    gtk_widget_set_sensitive(browser->backButton, FALSE);
  if (can_go_forward)
    gtk_widget_set_sensitive(browser->forwardButton, TRUE);
  else
    gtk_widget_set_sensitive(browser->forwardButton, FALSE);
 }
