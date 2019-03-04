/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2006, 2008 Christian Persch
 *  Copyright © 2012 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-session.h"

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-link.h"
#include "ephy-notebook.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

typedef struct {
  EphyNotebook *notebook;
  gint ref_count;
} NotebookTracker;

typedef struct {
  NotebookTracker *notebook_tracker;
  int position;
  char *url;
  WebKitWebViewSessionState *state;
} ClosedTab;

struct _EphySession {
  GObject parent_instance;

  GQueue *closed_tabs;
  guint save_source_id;
  GCancellable *save_cancellable;
  guint closing : 1;
  guint dont_save : 1;
};

#define SESSION_STATE           "type:session_state"
#define MAX_CLOSED_TABS         10

enum {
  PROP_0,
  PROP_CAN_UNDO_TAB_CLOSED,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static gboolean ephy_session_save_idle_cb (EphySession *session);

G_DEFINE_TYPE (EphySession, ephy_session, G_TYPE_OBJECT)

/* Helper functions */

static GFile *
get_session_file (const char *filename)
{
  GFile *file;
  char *path;

  if (filename == NULL) {
    return NULL;
  }

  if (strcmp (filename, SESSION_STATE) == 0) {
    path = g_build_filename (ephy_profile_dir (),
                             "session_state.xml",
                             NULL);
  } else {
    path = g_strdup (filename);
  }

  file = g_file_new_for_path (path);
  g_free (path);

  return file;
}

static void
session_delete (EphySession *session)
{
  GFile *file;

  file = get_session_file (SESSION_STATE);
  g_file_delete (file, NULL, NULL);
  g_object_unref (file);
}

static void
load_changed_cb (WebKitWebView  *view,
                 WebKitLoadEvent load_event,
                 EphySession    *session)
{
  if (!ephy_web_view_load_failed (EPHY_WEB_VIEW (view)))
    ephy_session_save (session);
}

static void
notebook_tracker_set_notebook (NotebookTracker *tracker,
                               EphyNotebook    *notebook)
{
  if (tracker->notebook == notebook) {
    return;
  }

  if (tracker->notebook) {
    g_object_remove_weak_pointer (G_OBJECT (tracker->notebook), (gpointer *)&tracker->notebook);
  }
  tracker->notebook = notebook;
  if (tracker->notebook) {
    g_object_add_weak_pointer (G_OBJECT (tracker->notebook), (gpointer *)&tracker->notebook);
  }
}

static NotebookTracker *
notebook_tracker_new (EphyNotebook *notebook)
{
  NotebookTracker *tracker = g_new0 (NotebookTracker, 1);

  tracker->ref_count = 1;
  notebook_tracker_set_notebook (tracker, notebook);

  return tracker;
}

static NotebookTracker *
notebook_tracker_ref (NotebookTracker *tracker)
{
  g_atomic_int_inc (&tracker->ref_count);

  return tracker;
}

static void
notebook_tracker_unref (NotebookTracker *tracker)
{
  if (!g_atomic_int_dec_and_test (&tracker->ref_count))
    return;

  notebook_tracker_set_notebook (tracker, NULL);
  g_free (tracker);
}

static EphyNotebook *
closed_tab_get_notebook (ClosedTab *tab)
{
  return tab->notebook_tracker->notebook;
}

static int
compare_func (ClosedTab *iter, EphyNotebook *notebook)
{
  return GTK_NOTEBOOK (closed_tab_get_notebook (iter)) - GTK_NOTEBOOK (notebook);
}

static NotebookTracker *
ephy_session_ref_or_create_notebook_tracker (EphySession  *session,
                                             EphyNotebook *notebook)
{
  GList *item = g_queue_find_custom (session->closed_tabs, notebook, (GCompareFunc)compare_func);
  return item ? notebook_tracker_ref (((ClosedTab *)item->data)->notebook_tracker) : notebook_tracker_new (notebook);
}

static void
closed_tab_free (ClosedTab *tab)
{
  g_free (tab->url);
  notebook_tracker_unref (tab->notebook_tracker);
  webkit_web_view_session_state_unref (tab->state);

  g_free (tab);
}

static ClosedTab *
closed_tab_new (EphyWebView     *web_view,
                int              position,
                NotebookTracker *notebook_tracker)
{
  ClosedTab *tab = g_new0 (ClosedTab, 1);

  tab->url = g_strdup (ephy_web_view_get_address (web_view));
  tab->position = position;
  /* Takes the ownership of the tracker */
  tab->notebook_tracker = notebook_tracker;
  tab->state = webkit_web_view_get_session_state (WEBKIT_WEB_VIEW (web_view));

  return tab;
}

void
ephy_session_undo_close_tab (EphySession *session)
{
  EphyEmbed *embed, *new_tab;
  WebKitWebView *web_view;
  WebKitBackForwardList *bf_list;
  WebKitBackForwardListItem *item;
  ClosedTab *tab;
  EphyWindow *window;
  EphyNotebook *notebook;
  EphyNewTabFlags flags = EPHY_NEW_TAB_JUMP;

  g_assert (EPHY_IS_SESSION (session));

  tab = g_queue_pop_head (session->closed_tabs);
  if (tab == NULL)
    return;

  LOG ("UNDO CLOSE TAB: %s", tab->url);
  notebook = closed_tab_get_notebook (tab);
  if (notebook) {
    if (tab->position > 0) {
      /* Append in the n-th position. */
      embed = EPHY_EMBED (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
                                                     tab->position - 1));
      flags |= EPHY_NEW_TAB_APPEND_AFTER;
    } else {
      /* Just prepend in the first position. */
      embed = NULL;
      flags |= EPHY_NEW_TAB_FIRST;
    }

    window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (notebook)));
    new_tab = ephy_shell_new_tab (ephy_shell_get_default (),
                                  window, embed,
                                  flags);
  } else {
    window = ephy_window_new ();
    new_tab = ephy_shell_new_tab (ephy_shell_get_default (),
                                  window, NULL, flags);
    notebook_tracker_set_notebook (tab->notebook_tracker,
                                   EPHY_NOTEBOOK (ephy_window_get_notebook (window)));
  }

  web_view = WEBKIT_WEB_VIEW (ephy_embed_get_web_view (new_tab));
  webkit_web_view_restore_session_state (web_view, tab->state);
  bf_list = webkit_web_view_get_back_forward_list (web_view);
  item = webkit_back_forward_list_get_current_item (bf_list);
  if (item) {
    webkit_web_view_go_to_back_forward_list_item (web_view, item);
  } else {
    ephy_web_view_load_url (ephy_embed_get_web_view (new_tab), tab->url);
  }

  gtk_widget_grab_focus (GTK_WIDGET (new_tab));
  gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());

  closed_tab_free (tab);

  if (g_queue_is_empty (session->closed_tabs))
    g_object_notify_by_pspec (G_OBJECT (session), obj_properties[PROP_CAN_UNDO_TAB_CLOSED]);
}

static void
ephy_session_tab_closed (EphySession  *session,
                         EphyNotebook *notebook,
                         EphyEmbed    *embed,
                         gint          position)
{
  EphyWebView *view;
  WebKitWebView *wk_view;
  ClosedTab *tab;

  view = ephy_embed_get_web_view (embed);
  wk_view = WEBKIT_WEB_VIEW (view);

  if (!webkit_web_view_can_go_back (wk_view) && !webkit_web_view_can_go_forward (wk_view) &&
      (ephy_web_view_get_is_blank (view) || ephy_web_view_is_overview (view))) {
    return;
  }

  if (g_queue_get_length (session->closed_tabs) == MAX_CLOSED_TABS) {
    closed_tab_free (g_queue_pop_tail (session->closed_tabs));
  }

  tab = closed_tab_new (view, position,
                        ephy_session_ref_or_create_notebook_tracker (session, notebook));
  g_queue_push_head (session->closed_tabs, tab);

  if (g_queue_get_length (session->closed_tabs) == 1)
    g_object_notify_by_pspec (G_OBJECT (session), obj_properties[PROP_CAN_UNDO_TAB_CLOSED]);

  LOG ("Added: %s to the list (%d elements)",
       ephy_web_view_get_address (view), g_queue_get_length (session->closed_tabs));
}

gboolean
ephy_session_get_can_undo_tab_closed (EphySession *session)
{
  g_assert (EPHY_IS_SESSION (session));

  return g_queue_is_empty (session->closed_tabs) == FALSE;
}

static void
notebook_page_added_cb (GtkWidget   *notebook,
                        EphyEmbed   *embed,
                        guint        position,
                        EphySession *session)
{
  g_signal_connect (ephy_embed_get_web_view (embed), "load-changed",
                    G_CALLBACK (load_changed_cb), session);
}

static void
notebook_page_removed_cb (GtkWidget   *notebook,
                          EphyEmbed   *embed,
                          guint        position,
                          EphySession *session)
{
  ephy_session_save (session);

  g_signal_handlers_disconnect_by_func
    (ephy_embed_get_web_view (embed), G_CALLBACK (load_changed_cb),
    session);

  ephy_session_tab_closed (session, EPHY_NOTEBOOK (notebook), embed, position);
}

static void
notebook_page_reordered_cb (GtkWidget   *notebook,
                            GtkWidget   *tab,
                            guint        position,
                            EphySession *session)
{
  ephy_session_save (session);
}

static void
notebook_switch_page_cb (GtkNotebook *notebook,
                         GtkWidget   *page,
                         guint        page_num,
                         EphySession *session)
{
  ephy_session_save (session);
}

static void
session_maybe_open_window (EphySession *session,
                           guint32      user_time)
{
  EphyShell *shell = ephy_shell_get_default ();

  /* FIXME: maybe just check for normal windows? */
  if (ephy_shell_get_n_windows (shell) == 0) {
    EphyWindow *window = ephy_window_new ();

    ephy_link_open (EPHY_LINK (window), NULL, NULL, EPHY_LINK_HOME_PAGE);
  }
}

static void
window_added_cb (GtkApplication *application,
                 GtkWindow      *window,
                 EphySession    *session)
{
  GtkWidget *notebook;
  EphyWindow *ephy_window;

  ephy_session_save (session);

  if (!EPHY_IS_WINDOW (window))
    return;

  ephy_window = EPHY_WINDOW (window);

  notebook = ephy_window_get_notebook (ephy_window);
  g_signal_connect (notebook, "page-added",
                    G_CALLBACK (notebook_page_added_cb), session);
  g_signal_connect (notebook, "page-removed",
                    G_CALLBACK (notebook_page_removed_cb), session);
  g_signal_connect (notebook, "page-reordered",
                    G_CALLBACK (notebook_page_reordered_cb), session);
  g_signal_connect_after (notebook, "switch-page",
                          G_CALLBACK (notebook_switch_page_cb), session);

  /* Set unique identifier as role, so that on restore, the WM can
   * place the window on the right workspace
   */

  if (gtk_window_get_role (window) == NULL) {
    /* I guess rand() is unique enough, otherwise we could use
     * time + pid or something
     */
    char *role;

    role = g_strdup_printf ("epiphany-window-%x", rand ());
    gtk_window_set_role (window, role);
    g_free (role);
  }
}

static void
window_removed_cb (GtkApplication *application,
                   GtkWindow      *window,
                   EphySession    *session)
{
  ephy_session_save (session);

  /* NOTE: since the window will be destroyed anyway, we don't need to
   * disconnect our signal handlers from its components.
   */
}

/* Class implementation */

static void
ephy_session_init (EphySession *session)
{
  EphyShell *shell;

  LOG ("EphySession initialising");

  session->closed_tabs = g_queue_new ();
  shell = ephy_shell_get_default ();
  g_signal_connect (shell, "window-added",
                    G_CALLBACK (window_added_cb), session);
  g_signal_connect (shell, "window-removed",
                    G_CALLBACK (window_removed_cb), session);
}

static void
ephy_session_dispose (GObject *object)
{
  EphySession *session = EPHY_SESSION (object);

  LOG ("EphySession disposing");

  g_queue_free_full (session->closed_tabs,
                     (GDestroyNotify)closed_tab_free);

  G_OBJECT_CLASS (ephy_session_parent_class)->dispose (object);
}

static void
ephy_session_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  EphySession *session = EPHY_SESSION (object);

  switch (property_id) {
    case PROP_CAN_UNDO_TAB_CLOSED:
      g_value_set_boolean (value,
                           ephy_session_get_can_undo_tab_closed (session));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_session_class_init (EphySessionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = ephy_session_dispose;
  object_class->get_property = ephy_session_get_property;

  obj_properties[PROP_CAN_UNDO_TAB_CLOSED] =
    g_param_spec_boolean ("can-undo-tab-closed",
                          "Can undo tab close",
                          "Session can undo a tab closure",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

/* Implementation */

void
ephy_session_close (EphySession *session)
{
  EphyPrefsRestoreSessionPolicy policy;

  g_assert (EPHY_IS_SESSION (session));

  LOG ("ephy_session_close");

  if (session->save_source_id) {
    /* There's a save pending, cancel it and save the session now since
     * after closing the session the saving is no longer allowed.
     */
    g_source_remove (session->save_source_id);
    session->save_source_id = 0;
  }

  if (session->closing)
    return;

  session->closing = TRUE;

  policy = g_settings_get_enum (EPHY_SETTINGS_MAIN, EPHY_PREFS_RESTORE_SESSION_POLICY);
  if (policy == EPHY_PREFS_RESTORE_SESSION_POLICY_ALWAYS) {
    ephy_session_save_idle_cb (session);
  } else {
    session_delete (session);
  }

  session->dont_save = TRUE;
}

static void
get_window_geometry (GtkWindow    *window,
                     GdkRectangle *rectangle)
{
  gtk_window_get_size (window, &rectangle->width, &rectangle->height);
  gtk_window_get_position (window, &rectangle->x, &rectangle->y);
}

typedef struct {
  char *url;
  char *title;
  gboolean loading;
  gboolean crashed;
  WebKitWebViewSessionState *state;
} SessionTab;

static SessionTab *
session_tab_new (EphyEmbed   *embed,
                 EphySession *session)
{
  SessionTab *session_tab;
  const char *address;
  EphyWebView *web_view = ephy_embed_get_web_view (embed);
  EphyWebViewErrorPage error_page = ephy_web_view_get_error_page (web_view);

  session_tab = g_new (SessionTab, 1);

  address = ephy_web_view_get_address (web_view);
  /* Do not store ephy-about: URIs, they are not valid for loading. */
  if (g_str_has_prefix (address, EPHY_ABOUT_SCHEME)) {
    session_tab->url = g_strconcat ("about", address + EPHY_ABOUT_SCHEME_LEN, NULL);
  } else if (!strcmp (address, "about:blank")) {
    /* EphyWebView address is NULL between load_uri() and WEBKIT_LOAD_STARTED,
     * but WebKitWebView knows the pending API request URL, so use that instead of about:blank.
     */
    session_tab->url = g_strdup (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view)));
  } else {
    session_tab->url = g_strdup (address);
  }

  session_tab->title = g_strdup (ephy_embed_get_title (embed));
  session_tab->loading = (ephy_web_view_is_loading (web_view) &&
                          !ephy_embed_has_load_pending (embed) &&
                          !session->closing);
  session_tab->crashed = (error_page == EPHY_WEB_VIEW_ERROR_PAGE_CRASH ||
                          error_page == EPHY_WEB_VIEW_ERROR_PROCESS_CRASH);
  session_tab->state = webkit_web_view_get_session_state (WEBKIT_WEB_VIEW (web_view));

  return session_tab;
}

static void
session_tab_free (SessionTab *tab)
{
  g_free (tab->url);
  g_free (tab->title);
  g_clear_pointer (&tab->state, webkit_web_view_session_state_unref);

  g_free (tab);
}

typedef struct {
  GdkRectangle geometry;
  char *role;

  GList *tabs;
  gint active_tab;
} SessionWindow;

static SessionWindow *
session_window_new (EphyWindow  *window,
                    EphySession *session)
{
  SessionWindow *session_window;
  GList *tabs, *l;
  GtkNotebook *notebook;

  tabs = ephy_embed_container_get_children (EPHY_EMBED_CONTAINER (window));
  /* Do not save an empty EphyWindow.
   * This only happens when the window was newly opened.
   */
  if (!tabs) {
    return NULL;
  }

  session_window = g_new0 (SessionWindow, 1);
  get_window_geometry (GTK_WINDOW (window), &session_window->geometry);
  session_window->role = g_strdup (gtk_window_get_role (GTK_WINDOW (window)));

  for (l = tabs; l != NULL; l = l->next) {
    SessionTab *tab;

    tab = session_tab_new (EPHY_EMBED (l->data), session);
    session_window->tabs = g_list_prepend (session_window->tabs, tab);
  }
  g_list_free (tabs);
  session_window->tabs = g_list_reverse (session_window->tabs);

  notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
  session_window->active_tab = gtk_notebook_get_current_page (notebook);

  return session_window;
}

static void
session_window_free (SessionWindow *session_window)
{
  g_free (session_window->role);
  g_list_free_full (session_window->tabs, (GDestroyNotify)session_tab_free);

  g_free (session_window);
}

typedef struct {
  EphySession *session;

  GList *windows;
} SaveData;

static SaveData *
save_data_new (EphySession *session)
{
  SaveData *data;
  EphyShell *shell = ephy_shell_get_default ();
  GList *windows, *w;

  data = g_new0 (SaveData, 1);
  data->session = g_object_ref (session);

  windows = gtk_application_get_windows (GTK_APPLICATION (shell));
  for (w = windows; w != NULL; w = w->next) {
    SessionWindow *session_window;

    session_window = session_window_new (EPHY_WINDOW (w->data), session);
    if (session_window)
      data->windows = g_list_prepend (data->windows, session_window);
  }
  data->windows = g_list_reverse (data->windows);

  return data;
}

static void
save_data_free (SaveData *data)
{
  g_list_free_full (data->windows, (GDestroyNotify)session_window_free);

  g_object_unref (data->session);

  g_free (data);
}

static int
write_tab (xmlTextWriterPtr writer,
           SessionTab      *tab)
{
  int ret;

  ret = xmlTextWriterStartElement (writer, (xmlChar *)"embed");
  if (ret < 0)
    return ret;

  ret = xmlTextWriterWriteAttribute (writer, (xmlChar *)"url",
                                     (const xmlChar *)tab->url);
  if (ret < 0)
    return ret;

  ret = xmlTextWriterWriteAttribute (writer, (xmlChar *)"title",
                                     (const xmlChar *)tab->title);
  if (ret < 0)
    return ret;

  if (tab->loading) {
    ret = xmlTextWriterWriteAttribute (writer,
                                       (const xmlChar *)"loading",
                                       (const xmlChar *)"true");
    if (ret < 0)
      return ret;
  }

  if (tab->crashed) {
    ret = xmlTextWriterWriteAttribute (writer,
                                       (const xmlChar *)"crashed",
                                       (const xmlChar *)"true");
    if (ret < 0)
      return ret;
  }

  if (tab->state) {
    GBytes *bytes;

    bytes = webkit_web_view_session_state_serialize (tab->state);
    if (bytes) {
      gchar *base64;
      gconstpointer data;
      gsize data_length;

      data = g_bytes_get_data (bytes, &data_length);
      base64 = g_base64_encode (data, data_length);
      xmlTextWriterWriteAttribute (writer,
                                   (const xmlChar *)"history",
                                   (const xmlChar *)base64);
      g_free (base64);
      g_bytes_unref (bytes);
    }
  }

  ret = xmlTextWriterEndElement (writer);       /* embed */
  return ret;
}

static int
write_window_geometry (xmlTextWriterPtr writer,
                       GdkRectangle    *geometry)
{
  int ret;

  /* set window properties */
  ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"x", "%d",
                                           geometry->x);
  if (ret < 0)
    return ret;

  ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"y", "%d",
                                           geometry->y);
  if (ret < 0)
    return ret;

  ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"width", "%d",
                                           geometry->width);
  if (ret < 0)
    return ret;

  ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"height", "%d",
                                           geometry->height);
  return ret;
}

static int
write_ephy_window (xmlTextWriterPtr writer,
                   SessionWindow   *window)
{
  GList *l;
  int ret;

  ret = xmlTextWriterStartElement (writer, (xmlChar *)"window");
  if (ret < 0)
    return ret;

  ret = write_window_geometry (writer, &window->geometry);
  if (ret < 0)
    return ret;

  ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *)"active-tab", "%d",
                                           window->active_tab);
  if (ret < 0)
    return ret;

  if (window->role != NULL) {
    ret = xmlTextWriterWriteAttribute (writer,
                                       (const xmlChar *)"role",
                                       (const xmlChar *)window->role);
    if (ret < 0)
      return ret;
  }

  for (l = window->tabs; l != NULL; l = l->next) {
    SessionTab *tab = (SessionTab *)l->data;
    ret = write_tab (writer, tab);
    if (ret < 0)
      break;
  }
  if (ret < 0)
    return ret;

  ret = xmlTextWriterEndElement (writer);       /* window */
  return ret;
}

static void
save_session_in_thread_finished_cb (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static gboolean
session_seems_sane (GList *windows)
{
  for (GList *w = windows; w != NULL; w = w->next) {
    for (GList *t = ((SessionWindow *)w->data)->tabs; t != NULL; t = t->next) {
      const char *url = ((SessionTab *)t->data)->url;
      SoupURI *uri;
      gboolean sane = FALSE;

      /* NULL URLs are possible when an invalid URL is opened by JS.
       * E.g. <script>win = window.open("blah", "WIN");</script>
       */
      if (url == NULL)
        continue;

      /* Blank URLs can occur in some situations. Just ignore these, as they
       * are harmless and not an indicator of a corrupted session.
       */
      if (strcmp (url, "") == 0)
        continue;

      /* Ignore fake about "URLs." */
      if (g_str_has_prefix (url, "about:"))
        continue;

      uri = soup_uri_new (url);
      if (uri) {
        if (uri->host != NULL ||
            uri->scheme == SOUP_URI_SCHEME_DATA ||
            uri->scheme == SOUP_URI_SCHEME_FILE)
          sane = TRUE;
        soup_uri_free (uri);
      }

      if (!sane) {
        g_critical ("Refusing to save session due to invalid URL %s", url);
        return FALSE;
      }
    }
  }

  return TRUE;
}

static void
save_session_sync (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  SaveData *data = (SaveData *)g_task_get_task_data (task);
  xmlBufferPtr buffer;
  xmlTextWriterPtr writer;
  GList *w;
  int ret = -1;

  /* If any web view has an insane URL, then something has probably gone wrong
   * inside WebKit. For instance, if the web process is nonfunctional, the UI
   * process could have an invalid URI property. Yes, this would be a WebKit
   * bug, but Epiphany should be robust to such issues. Do not clobber an
   * existing good session file with our new bogus state. Bug #768250. */
  if (!session_seems_sane (data->windows))
    return;

  buffer = xmlBufferCreate ();
  writer = xmlNewTextWriterMemory (buffer, 0);
  if (writer == NULL)
    goto out;

  ret = xmlTextWriterSetIndent (writer, 1);
  if (ret < 0)
    goto out;

  ret = xmlTextWriterSetIndentString (writer, (const xmlChar *)"	 ");
  if (ret < 0)
    goto out;

  START_PROFILER ("Saving session")

  ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
  if (ret < 0)
    goto out;

  /* create and set the root node for the session */
  ret = xmlTextWriterStartElement (writer, (const xmlChar *)"session");
  if (ret < 0)
    goto out;

  /* iterate through all the windows */
  for (w = data->windows; w != NULL && ret >= 0; w = w->next) {
    ret = write_ephy_window (writer, (SessionWindow *)w->data);
  }
  if (ret < 0)
    goto out;

  ret = xmlTextWriterEndElement (writer);       /* session */
  if (ret < 0)
    goto out;

  ret = xmlTextWriterEndDocument (writer);

 out:
  if (writer)
    xmlFreeTextWriter (writer);

  if (ret >= 0 && !g_cancellable_is_cancelled (cancellable)) {
    GError *error = NULL;
    GFile *session_file;

    session_file = get_session_file (SESSION_STATE);

    if (!g_file_replace_contents (session_file,
                                  (const char *)buffer->content,
                                  buffer->use,
                                  NULL, TRUE, 0, NULL,
                                  cancellable, &error)) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_warning ("Error saving session: %s", error->message);
      }
      g_error_free (error);
    }

    g_object_unref (session_file);
  }

  xmlBufferFree (buffer);

  g_task_return_boolean (task, TRUE);

  STOP_PROFILER ("Saving session")
}

static EphySession *
ephy_session_save_idle_started (EphySession *session)
{
  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
  return g_object_ref (session);
}

static void
ephy_session_save_idle_finished (EphySession *session)
{
  g_application_release (G_APPLICATION (ephy_shell_get_default ()));
  g_object_unref (session);
}

static gboolean
ephy_session_save_idle_cb (EphySession *session)
{
  EphyShell *shell = ephy_shell_get_default ();
  SaveData *data;
  GTask *task;

  session->save_source_id = 0;

  if (session->save_cancellable) {
    g_cancellable_cancel (session->save_cancellable);
    g_object_unref (session->save_cancellable);
    session->save_cancellable = NULL;
  }

  LOG ("ephy_sesion_save");

  if (ephy_shell_get_n_windows (shell) == 0) {
    session_delete (session);
    return G_SOURCE_REMOVE;
  }

  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
  session->save_cancellable = g_cancellable_new ();
  data = save_data_new (session);
  task = g_task_new (session, session->save_cancellable,
                     save_session_in_thread_finished_cb, NULL);
  g_task_set_task_data (task, data, (GDestroyNotify)save_data_free);
  g_task_run_in_thread (task, save_session_sync);
  g_object_unref (task);

  return G_SOURCE_REMOVE;
}

void
ephy_session_save (EphySession *session)
{
  EphyPrefsRestoreSessionPolicy policy;

  g_assert (EPHY_IS_SESSION (session));

  if (session->save_source_id) {
    return;
  }

  if (session->dont_save) {
    return;
  }

  policy = g_settings_get_enum (EPHY_SETTINGS_MAIN, EPHY_PREFS_RESTORE_SESSION_POLICY);
  if (policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER) {
    return;
  }

  session->save_source_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE, 1,
                                                        (GSourceFunc)ephy_session_save_idle_cb,
                                                        ephy_session_save_idle_started (session),
                                                        (GDestroyNotify)ephy_session_save_idle_finished);
}

static void
confirm_before_recover (EphyWindow *window, const char *url, const char *title)
{
  EphyEmbed *embed;

  embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
                                   title, NULL,
                                   window, NULL,
                                   EPHY_NEW_TAB_APPEND_LAST,
                                   0);

  ephy_web_view_load_error_page (ephy_embed_get_web_view (embed), url,
                                 EPHY_WEB_VIEW_ERROR_PAGE_CRASH, NULL, NULL);
}

static void
restore_geometry (GtkWindow    *window,
                  GdkRectangle *geometry)
{
  if (geometry->x >= 0 && geometry->y >= 0)
    ephy_window_set_default_position (EPHY_WINDOW (window), geometry->x, geometry->y);

  if (geometry->width > 0 && geometry->height > 0)
    ephy_window_set_default_size (EPHY_WINDOW (window), geometry->width, geometry->height);
}

typedef struct {
  EphySession *session;
  guint32 user_time;

  EphyWindow *window;
  gboolean is_first_window;
  gint active_tab;

  gboolean is_first_tab;
} SessionParserContext;

static SessionParserContext *
session_parser_context_new (EphySession *session,
                            guint32      user_time)
{
  SessionParserContext *context;

  context = g_new0 (SessionParserContext, 1);
  context->session = g_object_ref (session);
  context->user_time = user_time;
  context->is_first_window = TRUE;

  return context;
}

static void
session_parser_context_free (SessionParserContext *context)
{
  g_object_unref (context->session);

  g_free (context);
}

static void
session_parse_window (SessionParserContext *context,
                      const gchar         **names,
                      const gchar         **values)
{
  GdkRectangle geometry = { -1, -1, 0, 0 };
  guint i;

  context->window = ephy_window_new ();

  for (i = 0; names[i]; i++) {
    gulong int_value;

    if (strcmp (names[i], "x") == 0) {
      ephy_string_to_int (values[i], &int_value);
      geometry.x = int_value;
    } else if (strcmp (names[i], "y") == 0) {
      ephy_string_to_int (values[i], &int_value);
      geometry.y = int_value;
    } else if (strcmp (names[i], "width") == 0) {
      ephy_string_to_int (values[i], &int_value);
      geometry.width = int_value;
    } else if (strcmp (names[i], "height") == 0) {
      ephy_string_to_int (values[i], &int_value);
      geometry.height = int_value;
    } else if (strcmp (names[i], "role") == 0) {
      gtk_window_set_role (GTK_WINDOW (context->window), values[i]);
    } else if (strcmp (names[i], "active-tab") == 0) {
      ephy_string_to_int (values[i], &int_value);
      context->active_tab = int_value;
    }
  }

  restore_geometry (GTK_WINDOW (context->window), &geometry);
}

static void
session_parse_embed (SessionParserContext *context,
                     const gchar         **names,
                     const gchar         **values)
{
  const char *url = NULL;
  const char *title = NULL;
  const char *history = NULL;
  gboolean was_loading = FALSE;
  gboolean crashed = FALSE;
  gboolean is_blank_page = FALSE;
  guint i;

  for (i = 0; names[i]; i++) {
    if (strcmp (names[i], "url") == 0) {
      url = values[i];
      is_blank_page = (strcmp (url, "about:blank") == 0 ||
                       strcmp (url, "about:overview") == 0);
    } else if (strcmp (names[i], "title") == 0) {
      title = values[i];
    } else if (strcmp (names[i], "loading") == 0) {
      was_loading = strcmp (values[i], "true") == 0;
    } else if (strcmp (names[i], "crashed") == 0) {
      crashed = strcmp (values[i], "true") == 0;
    } else if (strcmp (names[i], "history") == 0) {
      history = values[i];
    }
  }

  /* In the case that crash happens before we receive the URL from the server,
   * this will open an about:blank tab.
   * See http://bugzilla.gnome.org/show_bug.cgi?id=591294
   * Otherwise, if the web was fully loaded, it is reloaded again.
   */
  if ((!was_loading || is_blank_page) && !crashed) {
    EphyNewTabFlags flags;
    EphyEmbedShell *shell;
    EphyEmbedShellMode mode;
    EphyEmbed *embed;
    EphyWebView *web_view;
    gboolean delay_loading = FALSE;
    WebKitWebViewSessionState *state = NULL;

    shell = ephy_embed_shell_get_default ();
    mode = ephy_embed_shell_get_mode (shell);

    if (mode == EPHY_EMBED_SHELL_MODE_BROWSER ||
        mode == EPHY_EMBED_SHELL_MODE_STANDALONE) {
      delay_loading = g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                                              EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS);
    }

    flags = EPHY_NEW_TAB_APPEND_LAST;

    embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
                                     title, NULL,
                                     context->window, NULL, flags,
                                     0);

    web_view = ephy_embed_get_web_view (embed);
    if (history) {
      guchar *data;
      gsize data_length;
      GBytes *history_data;

      data = g_base64_decode (history, &data_length);
      history_data = g_bytes_new_take (data, data_length);
      state = webkit_web_view_session_state_new (history_data);
      g_bytes_unref (history_data);
    }

    if (delay_loading) {
      WebKitURIRequest *request = webkit_uri_request_new (url);

      ephy_embed_set_delayed_load_request (embed, request, state);
      ephy_web_view_set_placeholder (web_view, url, title);
      g_object_unref (request);
    } else {
      WebKitBackForwardList *bf_list;
      WebKitBackForwardListItem *item;

      if (state) {
        webkit_web_view_restore_session_state (WEBKIT_WEB_VIEW (web_view), state);
      }

      bf_list = webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (web_view));
      item = webkit_back_forward_list_get_current_item (bf_list);
      if (item) {
        webkit_web_view_go_to_back_forward_list_item (WEBKIT_WEB_VIEW (web_view), item);
      } else {
        ephy_web_view_load_url (web_view, url);
      }
    }

    if (state) {
      webkit_web_view_session_state_unref (state);
    }
  } else if (url && (was_loading || crashed)) {
    /* This page was loading during a UI process crash
     * (was_loading == TRUE) or a web process crash
     * (crashed == TRUE) and might make Epiphany crash again.
     */
    confirm_before_recover (context->window, url, title);
  }
}

static void
session_start_element (GMarkupParseContext *ctx,
                       const gchar         *element_name,
                       const gchar        **names,
                       const gchar        **values,
                       gpointer             user_data,
                       GError             **error)
{
  SessionParserContext *context = (SessionParserContext *)user_data;

  if (strcmp (element_name, "window") == 0) {
    session_parse_window (context, names, values);
    context->is_first_tab = TRUE;
  } else if (strcmp (element_name, "embed") == 0) {
    session_parse_embed (context, names, values);
  }
}

static void
session_end_element (GMarkupParseContext *ctx,
                     const gchar         *element_name,
                     gpointer             user_data,
                     GError             **error)
{
  SessionParserContext *context = (SessionParserContext *)user_data;

  if (strcmp (element_name, "window") == 0) {
    GtkWidget *notebook;
    EphyEmbedShell *shell = ephy_embed_shell_get_default ();

    notebook = ephy_window_get_notebook (context->window);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), context->active_tab);

    if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_TEST) {
      EphyEmbed *active_child;

      active_child = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (context->window));
      gtk_widget_grab_focus (GTK_WIDGET (active_child));
      gtk_widget_show (GTK_WIDGET (context->window));
    }

    ephy_embed_shell_restored_window (shell);

    context->window = NULL;
    context->active_tab = 0;
    context->is_first_window = FALSE;
  } else if (strcmp (element_name, "embed") == 0) {
    context->is_first_tab = FALSE;
  }
}

static const GMarkupParser session_parser = {
  session_start_element,
  session_end_element,
  NULL,
  NULL,
  NULL
};

typedef struct {
  EphyShell *shell;
  GMarkupParseContext *parser;
  char buffer[1024];
} LoadFromStreamAsyncData;

static LoadFromStreamAsyncData *
load_from_stream_async_data_new (GMarkupParseContext *parser)
{
  LoadFromStreamAsyncData *data;

  data = g_new (LoadFromStreamAsyncData, 1);
  data->shell = g_object_ref (ephy_shell_get_default ());
  data->parser = parser;

  return data;
}

static void
load_from_stream_async_data_free (LoadFromStreamAsyncData *data)
{
  g_object_unref (data->shell);
  g_markup_parse_context_free (data->parser);

  g_free (data);
}

static void
load_stream_complete (GTask *task)
{
  EphySession *session;

  g_task_return_boolean (task, TRUE);

  session = EPHY_SESSION (g_task_get_source_object (task));
  session->dont_save = FALSE;

  ephy_session_save (session);

  g_object_unref (task);

  g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
load_stream_complete_error (GTask  *task,
                            GError *error)
{
  EphySession *session;
  LoadFromStreamAsyncData *data;
  SessionParserContext *context;

  g_task_return_error (task, error);

  session = EPHY_SESSION (g_task_get_source_object (task));
  session->dont_save = FALSE;
  /* If the session fails to load for whatever reason,
   * delete the file and open an empty window.
   */
  session_delete (session);

  data = g_task_get_task_data (task);
  context = (SessionParserContext *)g_markup_parse_context_get_user_data (data->parser);
  session_maybe_open_window (session, context->user_time);

  g_object_unref (task);

  g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
load_stream_read_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (object);
  GTask *task = G_TASK (user_data);
  LoadFromStreamAsyncData *data;
  gssize bytes_read;
  GError *error = NULL;

  bytes_read = g_input_stream_read_finish (stream, result, &error);
  if (bytes_read < 0) {
    load_stream_complete_error (task, error);

    return;
  }

  data = g_task_get_task_data (task);
  if (bytes_read == 0) {
    if (!g_markup_parse_context_end_parse (data->parser, &error)) {
      load_stream_complete_error (task, error);
    } else {
      load_stream_complete (task);
    }

    return;
  }

  if (!g_markup_parse_context_parse (data->parser, data->buffer, bytes_read, &error)) {
    load_stream_complete_error (task, error);

    return;
  }

  g_input_stream_read_async (stream, data->buffer, sizeof (data->buffer),
                             g_task_get_priority (task),
                             g_task_get_cancellable (task),
                             load_stream_read_cb, task);
}

/**
 * ephy_session_load_from_stream:
 * @session: an #EphySession
 * @stream: a #GInputStream to read the session data from
 * @user_time: a user time, or 0
 * @cancellable: (allow-none): optional #GCancellable object, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *    request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously loads the session reading the session data from @stream,
 * restoring windows and their state.
 *
 * When the operation is finished, @callback will be called. You can
 * then call ephy_session_load_from_stream_finish() to get the result of
 * the operation.
 **/
void
ephy_session_load_from_stream (EphySession        *session,
                               GInputStream       *stream,
                               guint32             user_time,
                               GCancellable       *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer            user_data)
{
  GTask *task;
  SessionParserContext *context;
  GMarkupParseContext *parser;
  LoadFromStreamAsyncData *data;

  g_assert (EPHY_IS_SESSION (session));
  g_assert (G_IS_INPUT_STREAM (stream));

  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

  session->dont_save = TRUE;

  task = g_task_new (session, cancellable, callback, user_data);
  /* Use a priority lower than drawing events (HIGH_IDLE + 20) to make sure
   * the main window is shown as soon as possible at startup
   */
  g_task_set_priority (task, G_PRIORITY_HIGH_IDLE + 30);

  context = session_parser_context_new (session, user_time);
  parser = g_markup_parse_context_new (&session_parser, 0, context, (GDestroyNotify)session_parser_context_free);
  data = load_from_stream_async_data_new (parser);
  g_task_set_task_data (task, data, (GDestroyNotify)load_from_stream_async_data_free);

  g_input_stream_read_async (stream, data->buffer, sizeof (data->buffer),
                             g_task_get_priority (task), cancellable,
                             load_stream_read_cb, task);
}

/**
 * ephy_session_load_from_stream_finish:
 * @session: an #EphySession
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes an async session load operation started with
 * ephy_session_load_from_stream().
 *
 * Returns: %TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load_from_stream_finish (EphySession  *session,
                                      GAsyncResult *result,
                                      GError      **error)
{
  g_assert (g_task_is_valid (result, session));

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
  guint32 user_time;
} LoadAsyncData;

static LoadAsyncData *
load_async_data_new (guint32 user_time)
{
  LoadAsyncData *data;

  data = g_new (LoadAsyncData, 1);
  data->user_time = user_time;

  return data;
}

static void
load_async_data_free (LoadAsyncData *data)
{
  g_free (data);
}

static void
load_from_stream_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  EphySession *session = EPHY_SESSION (object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (!ephy_session_load_from_stream_finish (session, result, &error)) {
    g_task_return_error (task, error);
  } else {
    g_task_return_boolean (task, TRUE);
  }

  g_object_unref (task);
}

static void
session_read_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GFileInputStream *stream;
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  stream = g_file_read_finish (G_FILE (object), result, &error);
  if (stream) {
    EphySession *session;
    LoadAsyncData *data;

    session = EPHY_SESSION (g_task_get_source_object (task));
    data = g_task_get_task_data (task);
    ephy_session_load_from_stream (session, G_INPUT_STREAM (stream), data->user_time,
                                   g_task_get_cancellable (task), load_from_stream_cb, task);
    g_object_unref (stream);
  } else {
    g_task_return_error (task, error);
    g_object_unref (task);
  }

  g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

/**
 * ephy_session_load:
 * @session: an #EphySession
 * @filename: the path of the source file
 * @user_time: a user time, or 0
 * @cancellable: (allow-none): optional #GCancellable object, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *    request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously loads the session reading the session data from @filename,
 * restoring windows and their state.
 *
 * When the operation is finished, @callback will be called. You can
 * then call ephy_session_load_finish() to get the result of
 * the operation.
 **/
void
ephy_session_load (EphySession        *session,
                   const char         *filename,
                   guint32             user_time,
                   GCancellable       *cancellable,
                   GAsyncReadyCallback callback,
                   gpointer            user_data)
{
  GFile *save_to_file;
  GTask *task;
  LoadAsyncData *data;

  g_assert (EPHY_IS_SESSION (session));
  g_assert (filename);

  LOG ("ephy_sesion_load %s", filename);

  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

  task = g_task_new (session, cancellable, callback, user_data);
  /* Use a priority lower than drawing events (HIGH_IDLE + 20) to make sure
   * the main window is shown as soon as possible at startup
   */
  g_task_set_priority (task, G_PRIORITY_HIGH_IDLE + 30);

  save_to_file = get_session_file (filename);
  data = load_async_data_new (user_time);
  g_task_set_task_data (task, data, (GDestroyNotify)load_async_data_free);
  g_file_read_async (save_to_file, g_task_get_priority (task), cancellable, session_read_cb, task);
  g_object_unref (save_to_file);
}

/**
 * ephy_session_load_finish:
 * @session: an #EphySession
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes an async session load operation started with
 * ephy_session_load().
 *
 * Returns: %TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load_finish (EphySession  *session,
                          GAsyncResult *result,
                          GError      **error)
{
  g_assert (g_task_is_valid (result, session));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
session_state_file_exists (EphySession *session)
{
  GFile *saved_session_file;
  char *saved_session_file_path;
  gboolean retval;

  saved_session_file = get_session_file (SESSION_STATE);
  saved_session_file_path = g_file_get_path (saved_session_file);
  g_object_unref (saved_session_file);
  retval = g_file_test (saved_session_file_path, G_FILE_TEST_EXISTS);
  g_free (saved_session_file_path);

  return retval;
}

static void
session_resumed_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  EphySession *session = EPHY_SESSION (object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (!ephy_session_load_finish (session, result, &error)) {
    g_task_return_error (task, error);
  } else {
    g_task_return_boolean (task, TRUE);
  }

  g_object_unref (task);
}

void
ephy_session_resume (EphySession        *session,
                     guint32             user_time,
                     GCancellable       *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer            user_data)
{
  GTask *task;
  gboolean has_session_state;
  EphyPrefsRestoreSessionPolicy policy;
  EphyShell *shell;

  LOG ("ephy_session_resume");

  task = g_task_new (session, cancellable, callback, user_data);

  has_session_state = session_state_file_exists (session);

  policy = g_settings_get_enum (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_RESTORE_SESSION_POLICY);

  shell = ephy_shell_get_default ();

  if (has_session_state == FALSE ||
      policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER) {
    /* If we are auto-resuming, and we never want to
     * restore the session, clobber the session state
     * file. */
    if (policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER)
      session_delete (session);

    session_maybe_open_window (session, user_time);
  } else if (ephy_shell_get_n_windows (shell) == 0) {
    ephy_session_load (session, SESSION_STATE, user_time, cancellable,
                       session_resumed_cb, task);
    return;
  }

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

gboolean
ephy_session_resume_finish (EphySession  *session,
                            GAsyncResult *result,
                            GError      **error)
{
  g_assert (g_task_is_valid (result, session));

  return g_task_propagate_boolean (G_TASK (result), error);
}


void
ephy_session_clear (EphySession *session)
{
  EphyShell *shell;
  GList *windows, *p;

  g_assert (EPHY_IS_SESSION (session));

  shell = ephy_shell_get_default ();
  windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (shell)));
  for (p = windows; p; p = p->next)
    gtk_widget_destroy (GTK_WIDGET (p->data));
  g_list_free (windows);
  g_queue_foreach (session->closed_tabs,
                   (GFunc)closed_tab_free, NULL);
  g_queue_clear (session->closed_tabs);

  ephy_session_save (session);
}
