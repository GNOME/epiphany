/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
#include "ephy-location-controller.h"

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-suggestion-model.h"
#include "ephy-title-widget.h"
#include "ephy-uri-helpers.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>

/**
 * SECTION:ephy-location-controller
 * @short_description: An #EphyLink implementation
 *
 * #EphyLocationController handles navigation together with an #EphyTitleWidget
 */

struct _EphyLocationController {
  GObject parent_instance;

  EphyWindow *window;
  EphyTitleWidget *title_widget;
  GtkEventController *focus_controller;
  char *address;
  gboolean editable;
  gboolean sync_address_is_blocked;
  EphySearchEngineManager *search_engine_manager;
  GCancellable *suggestion_cancellable;
};

static void ephy_location_controller_finalize (GObject *object);
static void sync_address (EphyLocationController *controller,
                          GParamSpec             *pspec,
                          GtkWidget              *widget);

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_EDITABLE,
  PROP_WINDOW,
  PROP_TITLE_WIDGET,
  LAST_PROP
};
static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyLocationController, ephy_location_controller, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                      NULL))

static gboolean
handle_ephy_tab_uri (EphyLocationController *controller,
                     const char             *content)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (controller->window);
  GtkWidget *tab;
  EphyWebView *webview;
  int window_id;
  int tab_id;
  g_auto (GStrv) split = g_strsplit (content + strlen ("ephy-tab://"), "@", -1);

  if (g_strv_length (split) != 2)
    return FALSE;

  window_id = atoi (split[1]);
  tab_id = atoi (split[0]);
  tab = ephy_tab_view_get_selected_page (tab_view);
  webview = ephy_embed_get_web_view (EPHY_EMBED (tab));

  if (window_id != 0) {
    GApplication *application;
    EphyEmbedShell *shell;
    EphyWindow *window;
    GList *windows;

    shell = ephy_embed_shell_get_default ();
    application = G_APPLICATION (shell);
    windows = gtk_application_get_windows (GTK_APPLICATION (application));

    if ((guint)window_id >= g_list_length (windows))
      return FALSE;

    window = g_list_nth_data (windows, window_id);
    tab_view = ephy_window_get_tab_view (window);

    /* FIXME: this doesn't actually work.
     * https://gitlab.gnome.org/GNOME/epiphany/-/issues/1908
     */
    gtk_window_present (GTK_WINDOW (window));
  }

  if (tab_id >= ephy_tab_view_get_n_pages (tab_view))
    return FALSE;

  ephy_tab_view_select_nth_page (tab_view, tab_id);
  gtk_widget_grab_focus (GTK_WIDGET (webview));

  if (ephy_web_view_is_overview (webview)) {
    if (window_id != 0)
      tab_view = ephy_window_get_tab_view (controller->window);
    ephy_tab_view_close (tab_view, tab);
  }

  return TRUE;
}

static void
entry_activate_cb (EphyLocationEntry      *entry,
                   GdkModifierType         modifiers,
                   EphyLocationController *controller)
{
  const char *content;
  char *address;
  char *effective_address;

  if (controller->sync_address_is_blocked) {
    controller->sync_address_is_blocked = FALSE;
    g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), entry);
  }

  content = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (!content || content[0] == '\0')
    return;

  if (g_str_has_prefix (content, "ephy-tab://") && handle_ephy_tab_uri (controller, content))
    return;

  address = g_strdup (content);
  effective_address = ephy_embed_utils_normalize_or_autosearch_address (g_strstrip (address));
  g_free (address);
#if 0
  if (!ephy_embed_utils_address_has_web_scheme (effective_address)) {
    /* After normalization there are still some cases that are
     * impossible to tell apart. One example is <URI>:<PORT> and <NON
     * WEB SCHEME>:<DATA>. To fix this, let's do a HEAD request to the
     * effective URI prefixed with http://; if we get OK Status the URI
     * exists, and we'll go ahead, otherwise we'll try to launch a
     * proper handler through gtk_show_uri. We only do this in
     * ephy_web_view_load_url, since this case is only relevant for URIs
     * typed in the location entry, which uses this method to do the
     * load. */
    /* TODO: however, this is not really possible, because normalize_or_autosearch_address
     * prepends http:// for anything that doesn't look like a URL.
     */
  }
#endif

  ephy_link_open (EPHY_LINK (controller), effective_address, NULL,
                  ephy_link_flags_from_modifiers (modifiers, FALSE) | EPHY_LINK_TYPED);

  g_free (effective_address);
}


static void
user_changed_cb (GtkWidget              *widget,
                 const char             *address,
                 EphyLocationController *controller)
{
  GListModel *model;
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  LOG ("user_changed_cb, address %s", address);

  model = ephy_location_entry_get_model (EPHY_LOCATION_ENTRY (controller->title_widget));

  g_cancellable_cancel (controller->suggestion_cancellable);
  g_clear_object (&controller->suggestion_cancellable);
  controller->suggestion_cancellable = g_cancellable_new ();
  ephy_suggestion_model_query_async (EPHY_SUGGESTION_MODEL (model),
                                     address,
                                     TRUE,
                                     mode != EPHY_EMBED_SHELL_MODE_PRIVATE && mode != EPHY_EMBED_SHELL_MODE_INCOGNITO,
                                     controller->suggestion_cancellable,
                                     NULL, NULL);
}

static void
sync_address (EphyLocationController *controller,
              GParamSpec             *pspec,
              GtkWidget              *widget)
{
  LOG ("sync_address %s", controller->address);

  g_signal_handlers_block_by_func (widget, G_CALLBACK (user_changed_cb), controller);
  ephy_title_widget_set_address (controller->title_widget, controller->address);
  g_signal_handlers_unblock_by_func (widget, G_CALLBACK (user_changed_cb), controller);
}

static char *
get_location_cb (EphyLocationEntry      *entry,
                 EphyLocationController *controller)
{
  EphyEmbed *embed;
  const char *address;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (controller->window));
  if (!embed)
    return NULL;

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  return ephy_embed_utils_is_no_show_address (address) ? NULL : g_strdup (address);
}

static char *
get_title_cb (EphyLocationEntry      *entry,
              EphyLocationController *controller)
{
  EphyEmbed *embed;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (controller->window));

  return g_strdup (ephy_embed_get_title (embed));
}

static void
focus_enter_cb (EphyLocationController *controller)
{
  const char *address;

  /* Never block sync if the location entry is empty, else homepage URL
   * will be missing in homepage mode. */
  address = ephy_title_widget_get_address (controller->title_widget);
  if (!controller->sync_address_is_blocked && address && *address) {
    controller->sync_address_is_blocked = TRUE;
    g_signal_handlers_block_by_func (controller, G_CALLBACK (sync_address), controller->title_widget);
  }
}

static void
focus_leave_cb (EphyLocationController *controller)
{
  if (controller->sync_address_is_blocked) {
    controller->sync_address_is_blocked = FALSE;
    g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), controller->title_widget);
  }
}

static void
notify_selected_index_cb (EphyLocationController *controller)
{
  if (controller->sync_address_is_blocked) {
    controller->sync_address_is_blocked = FALSE;
    g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), controller->title_widget);
  }
}

static void
ephy_location_controller_constructed (GObject *object)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
  EphyHistoryService *history_service;
  EphyBookmarksManager *bookmarks_manager;
  EphySuggestionModel *model;
  EphyTabView *tab_view;
  GtkWidget *widget;
  GtkEventController *focus_controller;

  G_OBJECT_CLASS (ephy_location_controller_parent_class)->constructed (object);

  tab_view = ephy_window_get_tab_view (controller->window);
  widget = GTK_WIDGET (controller->title_widget);

  g_signal_connect_object (tab_view, "notify::selected-index",
                           G_CALLBACK (notify_selected_index_cb), controller,
                           G_CONNECT_SWAPPED);

  sync_address (controller, NULL, widget);
  g_signal_connect_object (controller, "notify::address",
                           G_CALLBACK (sync_address), widget, 0);

  if (!EPHY_IS_LOCATION_ENTRY (controller->title_widget))
    return;

  g_signal_connect (controller->title_widget, "user-changed", G_CALLBACK (user_changed_cb), controller);

  history_service = ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ());
  bookmarks_manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  model = ephy_suggestion_model_new (history_service, bookmarks_manager);
  ephy_location_entry_set_model (EPHY_LOCATION_ENTRY (controller->title_widget), G_LIST_MODEL (model));
  g_object_unref (model);

  g_object_bind_property (controller, "editable",
                          widget, "editable",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (widget, "activate",
                           G_CALLBACK (entry_activate_cb),
                           controller, 0);
  g_signal_connect_object (widget, "get-location",
                           G_CALLBACK (get_location_cb), controller, 0);
  g_signal_connect_object (widget, "get-title",
                           G_CALLBACK (get_title_cb), controller, 0);

  focus_controller = gtk_event_controller_focus_new ();
  g_signal_connect_object (focus_controller, "enter",
                           G_CALLBACK (focus_enter_cb), controller, G_CONNECT_SWAPPED);
  g_signal_connect_object (focus_controller, "leave",
                           G_CALLBACK (focus_leave_cb), controller, G_CONNECT_SWAPPED);
  gtk_widget_add_controller (widget, focus_controller);
}

static void
ephy_location_controller_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      ephy_location_controller_set_address (controller, g_value_get_string (value));
      break;
    case PROP_EDITABLE:
      controller->editable = g_value_get_boolean (value);
      break;
    case PROP_WINDOW:
      controller->window = EPHY_WINDOW (g_value_get_object (value));
      break;
    case PROP_TITLE_WIDGET:
      controller->title_widget = EPHY_TITLE_WIDGET (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_location_controller_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, ephy_location_controller_get_address (controller));
      break;
    case PROP_EDITABLE:
      g_value_set_boolean (value, controller->editable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_location_controller_dispose (GObject *object)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);

  if (!controller->title_widget)
    return;

  if (EPHY_IS_LOCATION_ENTRY (controller->title_widget)) {
    g_signal_handlers_disconnect_matched (controller, G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, controller->title_widget);
    g_signal_handlers_disconnect_matched (controller->title_widget, G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, controller);

    gtk_widget_remove_controller (GTK_WIDGET (controller->title_widget),
                                  controller->focus_controller);
    controller->focus_controller = NULL;
  }
  controller->title_widget = NULL;

  G_OBJECT_CLASS (ephy_location_controller_parent_class)->dispose (object);
}

static void
ephy_location_controller_class_init (EphyLocationControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = ephy_location_controller_finalize;
  object_class->dispose = ephy_location_controller_dispose;
  object_class->constructed = ephy_location_controller_constructed;
  object_class->get_property = ephy_location_controller_get_property;
  object_class->set_property = ephy_location_controller_set_property;

  /**
   * EphyLocationController:address:
   *
   * The address of the current location.
   */
  obj_properties[PROP_ADDRESS] =
    g_param_spec_string ("address",
                         NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * EphyLocationController:editable:
   *
   * Whether the location bar entry can be edited.
   */
  obj_properties[PROP_EDITABLE] =
    g_param_spec_boolean ("editable",
                          NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * EphyLocationController:window:
   *
   * The parent window.
   */
  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  /**
   * EphyLocationController:title-widget:
   *
   * The #EphyLocationController sets the address of the #EphyTitleWidget.
   */
  obj_properties[PROP_TITLE_WIDGET] =
    g_param_spec_object ("title-widget",
                         NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_location_controller_init (EphyLocationController *controller)
{
  EphyEmbedShell *shell;

  controller->address = g_strdup ("");
  controller->editable = TRUE;
  controller->sync_address_is_blocked = FALSE;
  shell = ephy_embed_shell_get_default ();
  controller->search_engine_manager = ephy_embed_shell_get_search_engine_manager (shell);
  controller->suggestion_cancellable = g_cancellable_new ();
}

static void
ephy_location_controller_finalize (GObject *object)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);

  g_free (controller->address);
  g_cancellable_cancel (controller->suggestion_cancellable);
  g_clear_object (&controller->suggestion_cancellable);

  G_OBJECT_CLASS (ephy_location_controller_parent_class)->finalize (object);
}

/**
 * ephy_location_controller_get_address:
 * @controller: an #EphyLocationController
 *
 * Retrieves the currently loaded address.
 *
 * Returns: the current address
 **/
const char *
ephy_location_controller_get_address (EphyLocationController *controller)
{
  g_assert (EPHY_IS_LOCATION_CONTROLLER (controller));

  return controller->address;
}

/**
 * ephy_location_controller_set_address:
 * @controller: an #EphyLocationController
 * @address: new address
 *
 * Sets @address as the address of @controller.
 **/
void
ephy_location_controller_set_address (EphyLocationController *controller,
                                      const char             *address)
{
  g_assert (EPHY_IS_LOCATION_CONTROLLER (controller));

  LOG ("set_address %s", address);

  g_free (controller->address);
  controller->address = g_strdup (address);

  g_object_notify_by_pspec (G_OBJECT (controller), obj_properties[PROP_ADDRESS]);
}
