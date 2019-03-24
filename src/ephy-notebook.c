/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
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
#include "ephy-notebook.h"

#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-helpers.h"
#include "ephy-link.h"
#include "ephy-pages-popover.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-tab-label.h"
#include "ephy-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define TAB_WIDTH_N_CHARS 15

#define AFTER_ALL_TABS -1

#define INSANE_NUMBER_OF_URLS 20

#define EPHY_NOTEBOOK_TAB_GROUP_ID "0x42"

struct _EphyNotebook {
  GtkNotebook parent_instance;

  EphyAdaptiveMode adaptive_mode;

  GList *focused_pages;
  guint tabs_vis_notifier_id;

  GMenu *tab_menu;

  guint tabs_allowed : 1;
};

static void ephy_notebook_constructed (GObject *object);
static void ephy_notebook_finalize (GObject *object);
static int  ephy_notebook_insert_page (GtkNotebook *notebook,
                                       GtkWidget   *child,
                                       GtkWidget   *tab_label,
                                       GtkWidget   *menu_label,
                                       int          position);
static void ephy_notebook_remove (GtkContainer *container,
                                  GtkWidget    *tab_widget);
static void ephy_notebook_page_added (GtkNotebook *notebook,
                                      GtkWidget   *child,
                                      guint        page_num);
static void ephy_notebook_page_removed (GtkNotebook *notebook,
                                        GtkWidget   *child,
                                        guint        page_num);
static void ephy_notebook_page_reordered (GtkNotebook *notebook,
                                          GtkWidget   *child,
                                          guint        page_num);

static const GtkTargetEntry url_drag_types [] =
{
  { (char *)"GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, 0 },
  { (char *)EPHY_DND_URI_LIST_TYPE, 0, 0 },
  { (char *)EPHY_DND_URL_TYPE, 0, 1 },
};

enum {
  PROP_0,
  PROP_TABS_ALLOWED,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  TAB_CLOSE_REQUEST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EphyNotebook, ephy_notebook, GTK_TYPE_NOTEBOOK,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
                                                NULL))

static void
ephy_notebook_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EphyNotebook *notebook = EPHY_NOTEBOOK (object);

  switch (prop_id) {
    case PROP_TABS_ALLOWED:
      g_value_set_boolean (value, notebook->tabs_allowed);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_notebook_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EphyNotebook *notebook = EPHY_NOTEBOOK (object);

  switch (prop_id) {
    case PROP_TABS_ALLOWED:
      ephy_notebook_set_tabs_allowed (notebook, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_notebook_class_init (EphyNotebookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

  object_class->constructed = ephy_notebook_constructed;
  object_class->finalize = ephy_notebook_finalize;
  object_class->get_property = ephy_notebook_get_property;
  object_class->set_property = ephy_notebook_set_property;

  container_class->remove = ephy_notebook_remove;

  notebook_class->insert_page = ephy_notebook_insert_page;
  notebook_class->page_added = ephy_notebook_page_added;
  notebook_class->page_removed = ephy_notebook_page_removed;
  notebook_class->page_reordered = ephy_notebook_page_reordered;

  signals[TAB_CLOSE_REQUEST] =
    g_signal_new ("tab-close-request",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET /* Can't use an interface type here */);

  obj_properties[PROP_TABS_ALLOWED] =
    g_param_spec_boolean ("tabs-allowed",
                          NULL,
                          NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static gint
find_tab_num_at_pos (EphyNotebook *notebook, gint abs_x, gint abs_y)
{
  int page_num = 0;
  GtkNotebook *nb = GTK_NOTEBOOK (notebook);
  GtkWidget *page;

  while ((page = gtk_notebook_get_nth_page (nb, page_num))) {
    GtkWidget *tab;
    GtkAllocation allocation;
    gint max_x, max_y;
    gint x_root, y_root;

    tab = gtk_notebook_get_tab_label (nb, page);
    g_assert (tab != NULL);

    if (!gtk_widget_get_mapped (GTK_WIDGET (tab))) {
      page_num++;
      continue;
    }

    gdk_window_get_origin (gtk_widget_get_window (tab),
                           &x_root, &y_root);

    gtk_widget_get_allocation (tab, &allocation);
    max_x = x_root + allocation.x + allocation.width;
    max_y = y_root + allocation.y + allocation.height;

    if (abs_y <= max_y && abs_x <= max_x) {
      return page_num;
    }

    page_num++;
  }
  return AFTER_ALL_TABS;
}

static gboolean
button_press_cb (EphyNotebook   *notebook,
                 GdkEventButton *event,
                 gpointer        data)
{
  int tab_clicked;

  tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);

  if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_MIDDLE) {
    GtkWidget *tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), tab_clicked);
    g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, tab);
    return GDK_EVENT_STOP;
  }

  if (event->type == GDK_BUTTON_PRESS &&
      event->button == GDK_BUTTON_SECONDARY &&
      (event->state & gtk_accelerator_get_default_mod_mask ()) == 0) {
    if (tab_clicked == -1) {
      /* Consume event so that we don't pop up the context
       * menu when the mouse is not over a tab label.
       */
      return GDK_EVENT_STOP;
    }

    /* Switch to the page where the mouse is over, but don't consume the
     * event. */
    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_clicked);
  }

  return GDK_EVENT_PROPAGATE;
}

static void
ephy_notebook_switch_page_cb (GtkNotebook *notebook,
                              GtkWidget   *page,
                              guint        page_num,
                              gpointer     data)
{
  EphyNotebook *nb = EPHY_NOTEBOOK (notebook);
  GtkWidget *child;

  child = gtk_notebook_get_nth_page (notebook, page_num);
  gtk_widget_grab_focus (child);

  /* Remove the old page, we dont want to grow unnecessarily
   * the list */
  if (nb->focused_pages) {
    nb->focused_pages =
      g_list_remove (nb->focused_pages, child);
  }

  nb->focused_pages = g_list_append (nb->focused_pages, child);
}

static void
notebook_drag_data_received_cb (GtkWidget        *widget,
                                GdkDragContext   *context,
                                int               x,
                                int               y,
                                GtkSelectionData *selection_data,
                                guint             info,
                                guint             time,
                                EphyEmbed        *embed)
{
  EphyWindow *window;
  GtkWidget *notebook;
  GdkAtom target;
  const guchar *data;

  target = gtk_selection_data_get_target (selection_data);
  if (target == gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"))
    return;

  g_signal_stop_emission_by_name (widget, "drag-data-received");

  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_ARBITRARY_URL))
    return;

  data = gtk_selection_data_get_data (selection_data);
  if (gtk_selection_data_get_length (selection_data) <= 0 || data == NULL)
    return;

  window = EPHY_WINDOW (gtk_widget_get_toplevel (widget));
  notebook = ephy_window_get_notebook (window);

  if (target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE)) {
    char **split;

    /* URL_TYPE has format: url \n title */
    split = g_strsplit ((const gchar *)data, "\n", 2);
    if (split != NULL && split[0] != NULL && split[0][0] != '\0') {
      ephy_link_open (EPHY_LINK (notebook), split[0], embed,
                      embed ? 0 : EPHY_LINK_NEW_TAB);
    }
    g_strfreev (split);
  } else if (target == gdk_atom_intern (EPHY_DND_URI_LIST_TYPE, FALSE)) {
    char **uris;
    int i;

    uris = gtk_selection_data_get_uris (selection_data);
    if (uris == NULL)
      return;

    for (i = 0; i < INSANE_NUMBER_OF_URLS && uris[i] != NULL; i++) {
      embed = ephy_link_open (EPHY_LINK (notebook), uris[i], embed,
                              (embed && i == 0) ? 0 : EPHY_LINK_NEW_TAB);
    }

    g_strfreev (uris);
  } else {
    char *text;

    text = (char *)gtk_selection_data_get_text (selection_data);
    if (text != NULL) {
      char *address;

      address = ephy_embed_utils_normalize_or_autosearch_address (text);
      ephy_link_open (EPHY_LINK (notebook), address, embed,
                      embed ? 0 : EPHY_LINK_NEW_TAB);
      g_free (address);
      g_free (text);
    }
  }
}

/*
 * update_tabs_visibility: Hide tabs if there is only one tab
 * and the pref is not set or when in application mode.
 */
static void
update_tabs_visibility (EphyNotebook *nb,
                        gboolean      before_inserting)
{
  EphyEmbedShellMode mode;
  gboolean show_tabs = FALSE;
  guint num;
  EphyPrefsUITabsBarVisibilityPolicy policy;

  mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
  num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));

  if (before_inserting)
    num++;

  policy = g_settings_get_enum (EPHY_SETTINGS_UI,
                                EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY);

  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION &&
      nb->adaptive_mode != EPHY_ADAPTIVE_MODE_NARROW &&
      ((policy == EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_MORE_THAN_ONE && num > 1) ||
        policy == EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_ALWAYS))
    show_tabs = TRUE;

  /* Only show the tabs when the "tabs-allowed" property is TRUE. */
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), nb->tabs_allowed && show_tabs);
}

static void
expand_tabs_changed_cb (GSettings    *settings,
                        char         *key,
                        EphyNotebook *nb)
{
  GList *tabs;
  GList *l;
  gboolean expand;

  expand = g_settings_get_boolean (EPHY_SETTINGS_UI,
                                   EPHY_PREFS_UI_EXPAND_TABS_BAR),

  tabs = gtk_container_get_children (GTK_CONTAINER (nb));

  for (l = tabs; l != NULL; l = l->next) {
    gtk_container_child_set (GTK_CONTAINER (nb),
                             l->data,
                             "tab-expand", expand,
                             NULL);
  }

  g_list_free (tabs);
}

static GtkPositionType
ephy_settings_get_tabs_bar_position (void)
{
  EphyPrefsUITabsBarPosition position;
  position = g_settings_get_enum (EPHY_SETTINGS_UI,
                                  EPHY_PREFS_UI_TABS_BAR_POSITION);

  switch (position) {
    case EPHY_PREFS_UI_TABS_BAR_POSITION_TOP:
      return GTK_POS_TOP;
    case EPHY_PREFS_UI_TABS_BAR_POSITION_BOTTOM:
      return GTK_POS_BOTTOM;
    case EPHY_PREFS_UI_TABS_BAR_POSITION_LEFT:
      return GTK_POS_LEFT;
    case EPHY_PREFS_UI_TABS_BAR_POSITION_RIGHT:
      return GTK_POS_RIGHT;
    default:
      g_assert_not_reached ();
  }
}

static void
box_set_halign (GtkWidget       *box,
                GtkPositionType  type)
{
  switch (type) {
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      gtk_widget_set_halign (box, GTK_ALIGN_FILL);
      break;
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
      break;
    default:
      break;
  }
}

static void
position_changed_cb (GSettings    *settings,
                     char         *key,
                     EphyNotebook *nb)
{
  GtkPositionType type = ephy_settings_get_tabs_bar_position ();
  gint pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));
  gint i;

  /* Update halign of all notebook label widgets (sub-box) */
  for (i = 0; i < pages; i++) {
    GtkWidget *child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), i);
    GtkWidget *label_widget = gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb), child);
    GtkWidget *box = gtk_container_get_children (GTK_CONTAINER (label_widget))->data;

    box_set_halign (box, type);
  }

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (nb), type);
}

static void
show_tabs_changed_cb (GSettings    *settings,
                      char         *key,
                      EphyNotebook *nb)
{
  update_tabs_visibility (nb, FALSE);
}

static void
ephy_notebook_init (EphyNotebook *notebook)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);

  gtk_notebook_set_scrollable (gnotebook, TRUE);
  gtk_notebook_set_show_border (gnotebook, FALSE);
  gtk_notebook_set_show_tabs (gnotebook, FALSE);
  gtk_notebook_set_group_name (gnotebook, EPHY_NOTEBOOK_TAB_GROUP_ID);
  gtk_notebook_set_tab_pos (gnotebook, ephy_settings_get_tabs_bar_position ());

  notebook->tabs_allowed = TRUE;

  g_signal_connect (notebook, "button-press-event",
                    (GCallback)button_press_cb, NULL);
  g_signal_connect_after (notebook, "switch-page",
                          G_CALLBACK (ephy_notebook_switch_page_cb),
                          NULL);

  /* Set up drag-and-drop target */
  g_signal_connect (notebook, "drag-data-received",
                    G_CALLBACK (notebook_drag_data_received_cb),
                    NULL);
  gtk_drag_dest_set (widget, 0,
                     url_drag_types, G_N_ELEMENTS (url_drag_types),
                     GDK_ACTION_MOVE | GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (widget);

  g_signal_connect (EPHY_SETTINGS_UI,
                    "changed::" EPHY_PREFS_UI_EXPAND_TABS_BAR,
                    G_CALLBACK (expand_tabs_changed_cb), notebook);
  g_signal_connect (EPHY_SETTINGS_UI,
                    "changed::" EPHY_PREFS_UI_TABS_BAR_POSITION,
                    G_CALLBACK (position_changed_cb), notebook);
  g_signal_connect (EPHY_SETTINGS_UI,
                    "changed::" EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY,
                    G_CALLBACK (show_tabs_changed_cb), notebook);
}

static void
ephy_notebook_constructed (GObject *object)
{
  EphyNotebook *notebook = EPHY_NOTEBOOK (object);
  GtkWidget *hbox;
  GtkWidget *button;
  EphyPagesPopover *popover;

  G_OBJECT_CLASS (ephy_notebook_parent_class)->constructed (object);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), hbox, GTK_PACK_END);
  gtk_widget_show (hbox);

  button = gtk_menu_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  /* Translators: tooltip for the tab switcher menu button */
  gtk_widget_set_tooltip_text (button, _("View open tabs"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  notebook->tab_menu = g_menu_new ();
  /* Remove this when popover menus become scrollable. */
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), TRUE);

  popover = ephy_pages_popover_new (GTK_WIDGET (button));
  ephy_pages_popover_set_notebook (popover, notebook);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), GTK_WIDGET (popover));
}

static void
ephy_notebook_finalize (GObject *object)
{
  EphyNotebook *notebook = EPHY_NOTEBOOK (object);

  g_signal_handlers_disconnect_by_func (EPHY_SETTINGS_UI,
                                        expand_tabs_changed_cb,
                                        notebook);
  g_signal_handlers_disconnect_by_func (EPHY_SETTINGS_UI,
                                        position_changed_cb,
                                        notebook);
  g_signal_handlers_disconnect_by_func (EPHY_SETTINGS_UI,
                                        show_tabs_changed_cb,
                                        notebook);

  g_list_free (notebook->focused_pages);

  G_OBJECT_CLASS (ephy_notebook_parent_class)->finalize (object);
}

static const char *
get_nth_tab_label_text (GtkNotebook *notebook,
                        int          n)
{
  GtkWidget *page;
  GtkWidget *tab_label;

  g_assert (n >= 0);

  page = gtk_notebook_get_nth_page (notebook, n);
  g_assert (page != NULL);

  tab_label = gtk_notebook_get_tab_label (notebook, page);
  g_assert (EPHY_IS_TAB_LABEL (tab_label));

  return ephy_tab_label_get_text (tab_label);
}

static char *
ellipsize_tab_label (const char *label)
{
  static const int MAX_LENGTH = 50;
  char *substring;
  char *result;

  if (g_utf8_strlen (label, -1) < MAX_LENGTH)
    return g_strdup (label);

  substring = g_utf8_substring (label, 0, MAX_LENGTH);
  result = g_strconcat (substring, "…", NULL);
  g_free (substring);

  return result;
}

static void
ephy_notebook_rebuild_tab_menu (EphyNotebook *notebook)
{
  GMenuItem *item;
  const char *text;
  char *ellipsized_text;
  int num_pages;
  GtkWidget *window;
  GActionGroup *group;
  GAction *action;
  gint current_page;

  g_menu_remove_all (notebook->tab_menu);

  num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

  /* TODO: Add favicon as well. Will have to ditch GMenu. :( */
  for (int i = 0; i < num_pages; i++) {
    text = get_nth_tab_label_text (GTK_NOTEBOOK (notebook), i);
    ellipsized_text = ellipsize_tab_label (text);
    item = g_menu_item_new (ellipsized_text, NULL);
    g_menu_item_set_action_and_target (item, "win.show-tab", "u", (guint)i, NULL);
    g_menu_append_item (notebook->tab_menu, item);
    g_free (ellipsized_text);
    g_object_unref (item);
  }

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  if (current_page < 0)
    return;

  window = gtk_widget_get_toplevel (GTK_WIDGET (notebook));
  group = gtk_widget_get_action_group (window, "win");
  /* Is window being destroyed? */
  if (group == NULL)
    return;

  action = g_action_map_lookup_action (G_ACTION_MAP (group), "show-tab");
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_uint32 ((guint32)current_page));
}

static void
rebuild_tab_menu_cb (EphyEmbed    *embed,
                     GParamSpec   *pspec,
                     EphyNotebook *notebook)
{
  ephy_notebook_rebuild_tab_menu (notebook);
}

static void
close_button_clicked_cb (GtkWidget *widget, GtkWidget *tab)
{
  GtkWidget *notebook;

  notebook = gtk_widget_get_parent (tab);
  g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, tab);
}

static GtkWidget *
build_tab_label (EphyNotebook *nb, EphyEmbed *embed)
{
  GtkWidget *tab_label;
  EphyWebView *view;

  tab_label = ephy_tab_label_new ();
  g_signal_connect (tab_label, "close-clicked", G_CALLBACK (close_button_clicked_cb), embed);

  /* Set up drag-and-drop target */
  g_signal_connect (tab_label, "drag-data-received",
                    G_CALLBACK (notebook_drag_data_received_cb), embed);
  gtk_drag_dest_set (tab_label, GTK_DEST_DEFAULT_ALL,
                     url_drag_types, G_N_ELEMENTS (url_drag_types),
                     GDK_ACTION_MOVE | GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (tab_label);

  /* Hook the label up to the tab properties */
  view = ephy_embed_get_web_view (embed);

  g_signal_connect_object (embed, "notify::title",
                           G_CALLBACK (rebuild_tab_menu_cb), nb, 0);

  g_object_bind_property (view, "title", tab_label, "label-text", G_BINDING_DEFAULT);
  g_object_bind_property (view, "icon", tab_label, "icon-buf", G_BINDING_DEFAULT);
  g_object_bind_property (view, "is-loading", tab_label, "spinning", G_BINDING_DEFAULT);
  g_object_bind_property (view, "is-playing-audio", tab_label, "audio", G_BINDING_DEFAULT);

  return tab_label;
}

void
ephy_notebook_set_tabs_allowed (EphyNotebook *nb,
                                gboolean      tabs_allowed)
{
  nb->tabs_allowed = tabs_allowed != FALSE;

  update_tabs_visibility (nb, FALSE);

  g_object_notify_by_pspec (G_OBJECT (nb), obj_properties[PROP_TABS_ALLOWED]);
}

static int
ephy_notebook_insert_page (GtkNotebook *gnotebook,
                           GtkWidget   *tab_widget,
                           GtkWidget   *tab_label,
                           GtkWidget   *menu_label,
                           int          position)
{
  EphyNotebook *notebook = EPHY_NOTEBOOK (gnotebook);

  /* Destroy passed-in tab label */
  if (tab_label != NULL) {
    g_object_ref_sink (tab_label);
    g_object_unref (tab_label);
  }

  g_assert (EPHY_IS_EMBED (tab_widget));

  tab_label = build_tab_label (notebook, EPHY_EMBED (tab_widget));

  update_tabs_visibility (notebook, TRUE);

  position = GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->insert_page (gnotebook,
                                                                           tab_widget,
                                                                           tab_label,
                                                                           menu_label,
                                                                           position);

  gtk_notebook_set_tab_reorderable (gnotebook, tab_widget, TRUE);
  gtk_notebook_set_tab_detachable (gnotebook, tab_widget, TRUE);
  gtk_container_child_set (GTK_CONTAINER (gnotebook),
                           GTK_WIDGET (tab_widget),
                           "tab-expand", g_settings_get_boolean (EPHY_SETTINGS_UI,
                                                                 EPHY_PREFS_UI_EXPAND_TABS_BAR),
                           NULL);

  return position;
}

int
ephy_notebook_add_tab (EphyNotebook *notebook,
                       EphyEmbed    *embed,
                       int           position,
                       gboolean      jump_to)
{
  GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);

  g_assert (EPHY_IS_NOTEBOOK (notebook));

  position = gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
                                       GTK_WIDGET (embed),
                                       NULL,
                                       position);

  gtk_container_child_set (GTK_CONTAINER (notebook),
                           GTK_WIDGET (embed),
                           "tab-expand", g_settings_get_boolean (EPHY_SETTINGS_UI,
                                                                 EPHY_PREFS_UI_EXPAND_TABS_BAR),
                           NULL);

  if (jump_to) {
    gtk_notebook_set_current_page (gnotebook, position);
    g_object_set_data (G_OBJECT (embed), "jump_to",
                       GINT_TO_POINTER (jump_to));
  }

  return position;
}

static void
smart_tab_switching_on_closure (EphyNotebook *notebook,
                                GtkWidget    *tab)
{
  gboolean jump_to;

  jump_to = GPOINTER_TO_INT (g_object_get_data
                               (G_OBJECT (tab), "jump_to"));

  if (!jump_to || !notebook->focused_pages) {
    gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
  } else {
    GList *l;
    GtkWidget *child;
    int page_num;

    /* activate the last focused tab */
    l = g_list_last (notebook->focused_pages);
    child = GTK_WIDGET (l->data);
    page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook),
                                      child);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                   page_num);
  }
}

static void
ephy_notebook_remove (GtkContainer *container,
                      GtkWidget    *tab_widget)
{
  GtkNotebook *gnotebook = GTK_NOTEBOOK (container);
  EphyNotebook *notebook = EPHY_NOTEBOOK (container);
  int position, curr;

  if (!EPHY_IS_EMBED (tab_widget))
    return;

  /* Remove the page from the focused pages list */
  notebook->focused_pages = g_list_remove (notebook->focused_pages, tab_widget);

  position = gtk_notebook_page_num (gnotebook, tab_widget);
  curr = gtk_notebook_get_current_page (gnotebook);

  if (position == curr) {
    smart_tab_switching_on_closure (notebook, tab_widget);
  }

  GTK_CONTAINER_CLASS (ephy_notebook_parent_class)->remove (container, tab_widget);

  update_tabs_visibility (notebook, FALSE);
}

static void
ephy_notebook_page_added (GtkNotebook *notebook,
                          GtkWidget   *child,
                          guint        page_num)
{
  if (GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->page_added != NULL)
    GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->page_added (notebook, child, page_num);

  ephy_notebook_rebuild_tab_menu (EPHY_NOTEBOOK (notebook));
}

static void
ephy_notebook_page_removed (GtkNotebook *notebook,
                            GtkWidget   *child,
                            guint        page_num)
{
  if (GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->page_removed != NULL)
    GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->page_removed (notebook, child, page_num);

  ephy_notebook_rebuild_tab_menu (EPHY_NOTEBOOK (notebook));
}

static void ephy_notebook_page_reordered (GtkNotebook *notebook,
                                          GtkWidget   *child,
                                          guint        page_num)
{
  if (GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->page_reordered != NULL)
    GTK_NOTEBOOK_CLASS (ephy_notebook_parent_class)->page_reordered (notebook, child, page_num);

  ephy_notebook_rebuild_tab_menu (EPHY_NOTEBOOK (notebook));
}

/**
 * ephy_notebook_next_page:
 * @notebook: an #EphyNotebook
 *
 * Advances to the next page in the @notebook. Note that unlike
 * gtk_notebook_next_page() this method will wrap around if
 * #GtkSettings:gtk-keynav-wrap-around is set.
 **/
void
ephy_notebook_next_page (EphyNotebook *notebook)
{
  gint current_page, n_pages;

  g_assert (EPHY_IS_NOTEBOOK (notebook));

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

  if (current_page < n_pages - 1)
    gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
  else {
    gboolean wrap_around;

    g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
                  "gtk-keynav-wrap-around", &wrap_around,
                  NULL);

    if (wrap_around)
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
  }
}

/**
 * ephy_notebook_prev_page:
 * @notebook: an #EphyNotebook
 *
 * Advances to the previous page in the @notebook. Note that unlike
 * gtk_notebook_next_page() this method will wrap around if
 * #GtkSettings:gtk-keynav-wrap-around is set.
 **/
void
ephy_notebook_prev_page (EphyNotebook *notebook)
{
  gint current_page;

  g_assert (EPHY_IS_NOTEBOOK (notebook));

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

  if (current_page > 0)
    gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
  else {
    gboolean wrap_around;

    g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
                  "gtk-keynav-wrap-around", &wrap_around,
                  NULL);

    if (wrap_around)
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), -1);
  }
}

GMenu *
ephy_notebook_get_pages_menu (EphyNotebook *notebook)
{
  g_assert (EPHY_IS_NOTEBOOK (notebook));

  return notebook->tab_menu;
}

void
ephy_notebook_set_adaptive_mode (EphyNotebook     *notebook,
                                 EphyAdaptiveMode  adaptive_mode)
{
  g_assert (EPHY_IS_NOTEBOOK (notebook));

  notebook->adaptive_mode = adaptive_mode;
  update_tabs_visibility (notebook, FALSE);
}
