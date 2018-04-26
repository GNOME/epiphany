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


#include "ephy-completion-model.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-title-widget.h"
#include "ephy-uri-helpers.h"
#include "ephy-widgets-type-builtins.h"

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
  GtkGesture *longpress_gesture;
  char *address;
  gboolean editable;
  gboolean sync_address_is_blocked;
  EphySearchEngineManager *search_engine_manager;
  guint num_search_engines_actions;
};

static void ephy_location_controller_finalize (GObject *object);
static void user_changed_cb (GtkWidget              *widget,
                             EphyLocationController *controller);
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

G_DEFINE_TYPE_WITH_CODE (EphyLocationController, ephy_location_controller, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                NULL))

static gboolean
match_func (GtkEntryCompletion *completion,
            const char         *key,
            GtkTreeIter        *iter,
            gpointer            data)
{
  /* We want every row in the model to show up. */
  return TRUE;
}

static void
entry_drag_data_received_cb (GtkWidget *widget,
                             GdkDragContext *context,
                             gint x, gint y,
                             GtkSelectionData *selection_data,
                             guint info,
                             guint time,
                             EphyLocationController *controller)
{
  GtkEntry *entry;
  GdkAtom url_type;
  GdkAtom text_type;
  const guchar *sel_data;

  sel_data = gtk_selection_data_get_data (selection_data);

  url_type = gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE);
  text_type = gdk_atom_intern (EPHY_DND_TEXT_TYPE, FALSE);

  if (gtk_selection_data_get_length (selection_data) <= 0 || sel_data == NULL)
    return;

  entry = GTK_ENTRY (widget);

  if (gtk_selection_data_get_target (selection_data) == url_type) {
    char **uris;

    uris = g_uri_list_extract_uris ((char *)sel_data);
    if (uris != NULL && uris[0] != NULL && *uris[0] != '\0') {
      gtk_entry_set_text (entry, (char *)uris[0]);
      ephy_link_open (EPHY_LINK (controller),
                      uris[0],
                      NULL,
                      ephy_link_flags_from_current_event ());
    }
    g_strfreev (uris);
  } else if (gtk_selection_data_get_target (selection_data) == text_type) {
    char *address;

    gtk_entry_set_text (entry, (const gchar *)sel_data);
    address = ephy_embed_utils_normalize_or_autosearch_address ((const gchar *)sel_data);
    ephy_link_open (EPHY_LINK (controller),
                    address,
                    NULL,
                    ephy_link_flags_from_current_event ());
    g_free (address);
  }
}

static void
entry_activate_cb (GtkEntry               *entry,
                   EphyLocationController *controller)
{
  const char *content;
  char *address;
  char *effective_address;

  if (controller->sync_address_is_blocked) {
    controller->sync_address_is_blocked = FALSE;
    g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), entry);
  }

  content = gtk_entry_get_text (entry);
  if (content == NULL || content[0] == '\0')
    return;

  address = g_strdup (content);
  effective_address = ephy_embed_utils_normalize_or_autosearch_address (g_strstrip (address));
  g_free (address);
#if 0
  if (!ephy_embed_utils_address_has_web_scheme (effective_address)) {
    /* After normalization there are still some cases that are
     * impossible to tell apart. One example is <URI>:<PORT> and <NON
     * WEB SCHEME>:<DATA>. To fix this, let's do a HEAD request to the
     * effective URI prefxed with http://; if we get OK Status the URI
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
                  ephy_link_flags_from_current_event () | EPHY_LINK_TYPED);

  g_free (effective_address);
}

static void
update_done_cb (EphyHistoryService *service,
                gboolean            success,
                gpointer            result_data,
                gpointer            user_data)
{
  /* FIXME: this hack is needed for the completion entry popup
   * to resize smoothly. See:
   * https://bugzilla.gnome.org/show_bug.cgi?id=671074 */
  gtk_entry_completion_complete (GTK_ENTRY_COMPLETION (user_data));
}

static void
user_changed_cb (GtkWidget *widget, EphyLocationController *controller)
{
  const char *address;
  GtkTreeModel *model;
  GtkEntryCompletion *completion;

  address = ephy_title_widget_get_address (EPHY_TITLE_WIDGET (widget));

  LOG ("user_changed_cb, address %s", address);

  completion = gtk_entry_get_completion (GTK_ENTRY (widget));
  model = gtk_entry_completion_get_model (completion);

  ephy_completion_model_update_for_string (EPHY_COMPLETION_MODEL (model), address,
                                           update_done_cb, completion);
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
  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  return ephy_embed_utils_is_no_show_address (address) ? NULL : ephy_uri_decode (address);
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

static gboolean
focus_in_event_cb (GtkWidget              *entry,
                   GdkEventFocus          *event,
                   EphyLocationController *controller)
{
  const char *address;

  /* Never block sync if the location entry is empty, else homepage URL
   * will be missing in homepage mode. */
  address = ephy_title_widget_get_address (controller->title_widget);
  if (!controller->sync_address_is_blocked && address && *address) {
    controller->sync_address_is_blocked = TRUE;
    g_signal_handlers_block_by_func (controller, G_CALLBACK (sync_address), entry);
  }

  return FALSE;
}

static gboolean
focus_out_event_cb (GtkWidget              *entry,
                    GdkEventFocus          *event,
                    EphyLocationController *controller)
{
  if (controller->sync_address_is_blocked) {
    controller->sync_address_is_blocked = FALSE;
    g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), entry);
  }

  return FALSE;
}

static void
switch_page_cb (GtkNotebook            *notebook,
                GtkWidget              *page,
                guint                   page_num,
                EphyLocationController *controller)
{
  if (controller->sync_address_is_blocked == TRUE) {
    controller->sync_address_is_blocked = FALSE;
    g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), controller->title_widget);
  }
}

static void
action_activated_cb (GtkEntryCompletion     *completion,
                     int                     index,
                     EphyLocationController *controller)
{
  GtkWidget *entry;
  char *url = NULL;
  char *content;
  char **engine_names;

  entry = gtk_entry_completion_get_entry (completion);
  content = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  if (content == NULL)
    return;

  engine_names = ephy_search_engine_manager_get_names (controller->search_engine_manager);
  if (index < g_strv_length (engine_names)) {
    url = ephy_search_engine_manager_build_search_address (controller->search_engine_manager,
                                                           engine_names[index],
                                                           content);
    ephy_link_open (EPHY_LINK (controller), url, NULL,
                    ephy_link_flags_from_current_event ());
  }

  g_strfreev (engine_names);
  g_free (content);
  g_free (url);
}

static void
fill_entry_completion_with_actions (GtkEntryCompletion     *completion,
                                    EphyLocationController *controller)
{
  char **engine_names;

  engine_names = ephy_search_engine_manager_get_names (controller->search_engine_manager);

  controller->num_search_engines_actions = 0;

  for (guint i = 0; engine_names[i] != NULL; i++) {
    gtk_entry_completion_insert_action_text (completion, i, engine_names[i]);
    controller->num_search_engines_actions++;
  }

  g_strfreev (engine_names);
}

static void
add_completion_actions (EphyLocationController *controller,
                        EphyLocationEntry      *lentry)
{
  GtkEntryCompletion *completion = gtk_entry_get_completion (GTK_ENTRY (lentry));

  fill_entry_completion_with_actions (completion, controller);
  g_signal_connect (completion, "action_activated",
                    G_CALLBACK (action_activated_cb), controller);
}

static void
search_engines_changed_cb (EphySearchEngineManager *manager,
                           gpointer                 data)
{
  EphyLocationController *controller;
  GtkEntryCompletion *completion;

  controller = EPHY_LOCATION_CONTROLLER (data);
  completion = gtk_entry_get_completion (GTK_ENTRY (controller->title_widget));

  for (guint i = 0; i < controller->num_search_engines_actions; i++)
    gtk_entry_completion_delete_action (completion, 0);

  fill_entry_completion_with_actions (completion, controller);
}

static void
longpress_gesture_cb (GtkGestureLongPress *gesture,
                      gdouble              x,
                      gdouble              y,
                      gpointer             user_data)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (user_data);

  gtk_editable_select_region (GTK_EDITABLE (controller->title_widget), 0, -1);
}

static void
ephy_location_controller_constructed (GObject *object)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
  EphyHistoryService *history_service;
  EphyBookmarksManager *bookmarks_manager;
  EphyCompletionModel *model;
  GtkWidget *notebook, *widget;

  G_OBJECT_CLASS (ephy_location_controller_parent_class)->constructed (object);

  notebook = ephy_window_get_notebook (controller->window);
  widget = GTK_WIDGET (controller->title_widget);

  g_signal_connect (notebook, "switch-page",
                    G_CALLBACK (switch_page_cb), controller);

  sync_address (controller, NULL, widget);
  g_signal_connect_object (controller, "notify::address",
                           G_CALLBACK (sync_address), widget, 0);

  if (!EPHY_IS_LOCATION_ENTRY (controller->title_widget))
    return;

  controller->longpress_gesture = gtk_gesture_long_press_new (widget);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller->longpress_gesture), TRUE);
  g_signal_connect (controller->longpress_gesture, "pressed", G_CALLBACK (longpress_gesture_cb), controller);

  history_service = ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ());
  bookmarks_manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  model = ephy_completion_model_new (history_service, bookmarks_manager);
  ephy_location_entry_set_completion (EPHY_LOCATION_ENTRY (controller->title_widget),
                                      GTK_TREE_MODEL (model),
                                      EPHY_COMPLETION_TEXT_COL,
                                      EPHY_COMPLETION_ACTION_COL,
                                      EPHY_COMPLETION_KEYWORDS_COL,
                                      EPHY_COMPLETION_RELEVANCE_COL,
                                      EPHY_COMPLETION_URL_COL,
                                      EPHY_COMPLETION_EXTRA_COL,
                                      EPHY_COMPLETION_FAVICON_COL);
  g_object_unref (model);

  ephy_location_entry_set_match_func (EPHY_LOCATION_ENTRY (controller->title_widget),
                                      match_func,
                                      controller->title_widget,
                                      NULL);

  add_completion_actions (controller, EPHY_LOCATION_ENTRY (controller->title_widget));

  g_signal_connect_object (controller->search_engine_manager, "changed",
                           G_CALLBACK (search_engines_changed_cb), controller, 0);

  g_object_bind_property (controller, "editable",
                          controller->title_widget, "editable",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (widget, "drag-data-received",
                           G_CALLBACK (entry_drag_data_received_cb),
                           controller, 0);
  g_signal_connect_object (widget, "activate",
                           G_CALLBACK (entry_activate_cb),
                           controller, 0);
  g_signal_connect_object (widget, "user-changed",
                           G_CALLBACK (user_changed_cb), controller, 0);
  g_signal_connect_object (widget, "get-location",
                           G_CALLBACK (get_location_cb), controller, 0);
  g_signal_connect_object (widget, "get-title",
                           G_CALLBACK (get_title_cb), controller, 0);
  g_signal_connect_object (widget, "focus-in-event",
                           G_CALLBACK (focus_in_event_cb), controller, 0);
  g_signal_connect_object (widget, "focus-out-event",
                           G_CALLBACK (focus_out_event_cb), controller, 0);
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
  GtkWidget *notebook;

  notebook = ephy_window_get_notebook (controller->window);

  if (notebook == NULL ||
      controller->title_widget == NULL) {
    return;
  }

  g_clear_object (&controller->longpress_gesture);

  if (EPHY_IS_LOCATION_ENTRY (controller->title_widget)) {
    g_signal_handlers_disconnect_matched (controller, G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, controller->title_widget);
    g_signal_handlers_disconnect_matched (controller->title_widget, G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, controller);
  }
  g_signal_handlers_disconnect_matched (notebook, G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL, controller);
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
                         "Address",
                         "The address of the current location",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * EphyLocationController:editable:
   *
   * Whether the location bar entry can be edited.
   */
  obj_properties[PROP_EDITABLE] =
    g_param_spec_boolean ("editable",
                          "Editable",
                          "Whether the location bar entry can be edited",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * EphyLocationController:window:
   *
   * The parent window.
   */
  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The parent window",
                         G_TYPE_OBJECT,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  /**
   * EphyLocationController:title-widget:
   *
   * The #EphyLocationController sets the address of the #EphyTitleWidget.
   */
  obj_properties[PROP_TITLE_WIDGET] =
    g_param_spec_object ("title-widget",
                         "Title widget",
                         "The title widget whose address will be managed",
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
}

static void
ephy_location_controller_finalize (GObject *object)
{
  EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);

  g_free (controller->address);

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
