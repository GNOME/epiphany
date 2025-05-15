/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Alexander Mikhaylenko <exalm7659@gmail.com>
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
#include "ephy-tab-view.h"

#include "ephy-desktop-utils.h"
#include "ephy-embed-utils.h"
#include "ephy-link.h"
#include "ephy-settings.h"
#include "ephy-shell.h"

#define MAX_NUMBER_OF_URLS 20

struct _EphyTabView {
  AdwBin parent_instance;

  AdwTabView *tab_view;
  AdwTabBar *tab_bar;
  AdwTabOverview *tab_overview;
  AdwTabPage *current_page;
};

G_DEFINE_FINAL_TYPE (EphyTabView, ephy_tab_view, ADW_TYPE_BIN)

enum {
  PROP_0,
  PROP_N_PAGES,
  PROP_SELECTED_INDEX,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
notify_n_pages_cb (EphyTabView *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_PAGES]);
}

static void
notify_selected_page_cb (EphyTabView *self)
{
  AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);

  if (page)
    adw_tab_page_set_needs_attention (page, FALSE);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_INDEX]);
}

static void
indicator_activated_cb (EphyTabView *self,
                        AdwTabPage  *page)
{
  EphyEmbed *embed = EPHY_EMBED (adw_tab_page_get_child (page));
  EphyWebView *view = ephy_embed_get_web_view (embed);
  gboolean muted = webkit_web_view_get_is_muted (WEBKIT_WEB_VIEW (view));

  webkit_web_view_set_is_muted (WEBKIT_WEB_VIEW (view), !muted);
}

static void
setup_menu_cb (EphyTabView *self,
               AdwTabPage  *page)
{
  self->current_page = page;
}

static AdwTabPage *
get_current_page (EphyTabView *self)
{
  if (self->current_page)
    return self->current_page;

  return adw_tab_view_get_selected_page (self->tab_view);
}

static void
ephy_tab_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EphyTabView *self = EPHY_TAB_VIEW (object);

  switch (prop_id) {
    case PROP_N_PAGES:
      g_value_set_int (value, ephy_tab_view_get_n_pages (self));
      break;

    case PROP_SELECTED_INDEX:
      g_value_set_int (value, ephy_tab_view_get_selected_index (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_tab_view_class_init (EphyTabViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ephy_tab_view_get_property;

  properties[PROP_N_PAGES] =
    g_param_spec_int ("n-pages",
                      NULL, NULL,
                      0,
                      G_MAXINT,
                      0,
                      (G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY));

  properties[PROP_SELECTED_INDEX] =
    g_param_spec_int ("selected-index",
                      NULL, NULL,
                      0,
                      G_MAXINT,
                      0,
                      (G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_tab_view_init (EphyTabView *self)
{
  self->tab_view = ADW_TAB_VIEW (adw_tab_view_new ());
  /* https://gitlab.gnome.org/GNOME/epiphany/-/issues/2296 */
  adw_tab_view_remove_shortcuts (self->tab_view,
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_HOME | ADW_TAB_VIEW_SHORTCUT_CONTROL_END |
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_HOME | ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_END);
  adw_bin_set_child (ADW_BIN (self), GTK_WIDGET (self->tab_view));

  g_signal_connect_object (self->tab_view,
                           "notify::n-pages",
                           G_CALLBACK (notify_n_pages_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->tab_view,
                           "notify::selected-page",
                           G_CALLBACK (notify_selected_page_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->tab_view,
                           "setup-menu",
                           G_CALLBACK (setup_menu_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->tab_view,
                           "indicator-activated",
                           G_CALLBACK (indicator_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

EphyTabView *
ephy_tab_view_new (void)
{
  return g_object_new (EPHY_TYPE_TAB_VIEW, NULL);
}

void
ephy_tab_view_next (EphyTabView *self)
{
  adw_tab_view_select_next_page (self->tab_view);
}

void
ephy_tab_view_pin (EphyTabView *self)
{
  adw_tab_view_set_page_pinned (self->tab_view, get_current_page (self), TRUE);
}

void
ephy_tab_view_unpin (EphyTabView *self)
{
  adw_tab_view_set_page_pinned (self->tab_view, get_current_page (self), FALSE);
}

void
ephy_tab_view_close (EphyTabView *self,
                     GtkWidget   *widget)
{
  AdwTabPage *page = adw_tab_view_get_page (self->tab_view, widget);

  adw_tab_view_close_page (self->tab_view, page);
}

void
ephy_tab_view_close_selected (EphyTabView *self)
{
  adw_tab_view_close_page (self->tab_view, get_current_page (self));
}

void
ephy_tab_view_close_left (EphyTabView *self)
{
  adw_tab_view_close_pages_before (self->tab_view, get_current_page (self));
}

void
ephy_tab_view_close_right (EphyTabView *self)
{
  adw_tab_view_close_pages_after (self->tab_view, get_current_page (self));
}

void
ephy_tab_view_close_other (EphyTabView *self)
{
  adw_tab_view_close_other_pages (self->tab_view, get_current_page (self));
}

void
ephy_tab_view_close_all (EphyTabView *self)
{
  ephy_tab_view_close_other (self);
  ephy_tab_view_close_selected (self);
}

void
ephy_tab_view_foreach (EphyTabView         *self,
                       EphyTabViewCallback  callback,
                       gpointer             user_data)
{
  int i, n;

  n = adw_tab_view_get_n_pages (self->tab_view);

  for (i = 0; i < n; i++) {
    AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);

    callback (adw_tab_page_get_child (page), user_data);
  }
}

int
ephy_tab_view_get_n_pages (EphyTabView *self)
{
  return adw_tab_view_get_n_pages (self->tab_view);
}

int
ephy_tab_view_get_selected_index (EphyTabView *self)
{
  AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);

  if (!page)
    return -1;

  return adw_tab_view_get_page_position (self->tab_view, page);
}

int
ephy_tab_view_get_page_index (EphyTabView *self,
                              GtkWidget   *widget)
{
  AdwTabPage *page = adw_tab_view_get_page (self->tab_view, widget);

  return adw_tab_view_get_page_position (self->tab_view, page);
}

GtkWidget *
ephy_tab_view_get_nth_page (EphyTabView *self,
                            int          index)
{
  AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, index);

  return adw_tab_page_get_child (page);
}

void
ephy_tab_view_select_nth_page (EphyTabView *self,
                               int          index)
{
  AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, index);

  adw_tab_view_set_selected_page (self->tab_view, page);
}

gboolean
ephy_tab_view_select_page (EphyTabView *self,
                           GtkWidget   *widget)
{
  AdwTabPage *page = adw_tab_view_get_page (self->tab_view, widget);

  if (page)
    adw_tab_view_set_selected_page (self->tab_view, page);

  return !!page;
}

GtkWidget *
ephy_tab_view_get_selected_page (EphyTabView *self)
{
  AdwTabPage *page = adw_tab_view_get_selected_page (self->tab_view);

  if (!page)
    return NULL;

  return adw_tab_page_get_child (page);
}

AdwTabView *
ephy_tab_view_get_tab_view (EphyTabView *self)
{
  return self->tab_view;
}

GList *
ephy_tab_view_get_pages (EphyTabView *self)
{
  GList *list = NULL;
  int i, n;

  n = adw_tab_view_get_n_pages (self->tab_view);

  for (i = 0; i < n; i++) {
    AdwTabPage *page = adw_tab_view_get_nth_page (self->tab_view, i);
    GtkWidget *content = adw_tab_page_get_child (page);

    list = g_list_prepend (list, content);
  }

  return g_list_reverse (list);
}

gboolean
ephy_tab_view_get_is_pinned (EphyTabView *self,
                             GtkWidget   *widget)
{
  AdwTabPage *page = adw_tab_view_get_page (self->tab_view, widget);

  return adw_tab_page_get_pinned (page);
}

static void
update_title_cb (AdwTabPage *page)
{
  EphyEmbed *embed = EPHY_EMBED (adw_tab_page_get_child (page));
  EphyWebView *view = ephy_embed_get_web_view (embed);
  const char *title = ephy_embed_get_title (embed);
  const char *address;

  if (!ephy_embed_has_load_pending (embed) &&
      !adw_tab_page_get_selected (page) &&
      adw_tab_page_get_pinned (page))
    adw_tab_page_set_needs_attention (page, TRUE);

  if (title && strlen (title)) {
    adw_tab_page_set_title (page, title);
    return;
  }

  address = ephy_web_view_get_display_address (view);

  if (ephy_web_view_is_loading (view) &&
      !ephy_embed_utils_is_no_show_address (address))
    adw_tab_page_set_title (page, address);
}

static void
update_icon_cb (AdwTabPage *page)
{
  EphyEmbed *embed = EPHY_EMBED (adw_tab_page_get_child (page));
  EphyWebView *view = ephy_embed_get_web_view (embed);
  GIcon *icon = ephy_web_view_get_icon (view);
  g_autoptr (GIcon) placeholder_icon = NULL;
  const char *uri, *favicon_name;

  if (icon) {
    adw_tab_page_set_icon (page, icon);
    return;
  }

  uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view));
  favicon_name = ephy_get_fallback_favicon_name (uri, EPHY_FAVICON_TYPE_NO_MISSING_PLACEHOLDER);

  if (favicon_name)
    placeholder_icon = g_themed_icon_new (favicon_name);

  adw_tab_page_set_icon (page, placeholder_icon);
}

static void
update_uri_cb (AdwTabPage *page)
{
  EphyEmbed *embed = EPHY_EMBED (adw_tab_page_get_child (page));
  EphyWebView *view = ephy_embed_get_web_view (embed);
  const char *uri;

  update_icon_cb (page);

  uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view));

  adw_tab_page_set_keyword (page, uri);
}

static void
update_indicator_cb (AdwTabPage *page)
{
  EphyEmbed *embed = EPHY_EMBED (adw_tab_page_get_child (page));
  EphyWebView *view = ephy_embed_get_web_view (embed);
  g_autoptr (GIcon) icon = NULL;

  if (webkit_web_view_is_playing_audio (WEBKIT_WEB_VIEW (view))) {
    if (webkit_web_view_get_is_muted (WEBKIT_WEB_VIEW (view)))
      icon = G_ICON (g_themed_icon_new ("ephy-audio-muted-symbolic"));
    else
      icon = G_ICON (g_themed_icon_new ("ephy-audio-playing-symbolic"));
  }

  adw_tab_page_set_indicator_icon (page, icon);
}

gboolean
is_loading_transform_cb (GBinding     *binding,
                         const GValue *from_value,
                         GValue       *to_value,
                         gpointer      user_data)
{
  EphyEmbed *embed = user_data;
  EphyWebView *web_view = ephy_embed_get_web_view (embed);
  const char *address = ephy_web_view_get_address (web_view);

  if (g_str_has_prefix (address, "about:") || g_str_has_prefix (address, "ephy-about:"))
    g_value_set_boolean (to_value, FALSE);
  else
    g_value_set_boolean (to_value, g_value_get_boolean (from_value) && !ephy_embed_has_load_pending (embed));
  return TRUE;
}

int
ephy_tab_view_add_tab (EphyTabView *self,
                       EphyEmbed   *embed,
                       EphyEmbed   *parent,
                       int          position,
                       gboolean     jump_to)
{
  AdwTabPage *page;
  EphyWebView *view;

  if (parent) {
    AdwTabPage *parent_page;

    parent_page = adw_tab_view_get_page (self->tab_view, GTK_WIDGET (parent));
    page = adw_tab_view_add_page (self->tab_view, GTK_WIDGET (embed), parent_page);
  } else if (position < 0) {
    page = adw_tab_view_append (self->tab_view, GTK_WIDGET (embed));
  } else {
    page = adw_tab_view_insert (self->tab_view, GTK_WIDGET (embed), position);
  }

  if (jump_to)
    adw_tab_view_set_selected_page (self->tab_view, page);

  view = ephy_embed_get_web_view (embed);

  adw_tab_page_set_indicator_activatable (page, TRUE);

  g_object_bind_property_full (view, "is-loading", page, "loading", G_BINDING_SYNC_CREATE, is_loading_transform_cb, NULL, embed, NULL);

  g_signal_connect_object (embed, "notify::title",
                           G_CALLBACK (update_title_cb), page,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::display-address",
                           G_CALLBACK (update_title_cb), page,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::icon",
                           G_CALLBACK (update_icon_cb), page,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::uri",
                           G_CALLBACK (update_uri_cb), page,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::is-playing-audio",
                           G_CALLBACK (update_indicator_cb), page,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::is-muted",
                           G_CALLBACK (update_indicator_cb), page,
                           G_CONNECT_SWAPPED);

  update_title_cb (page);
  update_uri_cb (page);
  update_indicator_cb (page);

  return adw_tab_view_get_page_position (self->tab_view, page);
}

GtkWidget *
ephy_tab_view_get_current_page (EphyTabView *self)
{
  AdwTabPage *page = get_current_page (self);

  if (!page)
    return NULL;

  return adw_tab_page_get_child (page);
}

static void
drag_drop_cb (EphyTabView  *self,
              AdwTabPage   *page,
              const GValue *value)
{
  EphyLink *window;
  EphyEmbed *embed;

  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_ARBITRARY_URL))
    return;

  embed = EPHY_EMBED (adw_tab_page_get_child (page));
  window = EPHY_LINK (gtk_widget_get_root (GTK_WIDGET (self)));

  if (G_VALUE_HOLDS (value, G_TYPE_FILE)) {
    GFile *file = g_value_get_object (value);
    g_autofree char *uri = g_file_get_uri (file);

    ephy_link_open (window, uri, embed, embed ? 0 : EPHY_LINK_NEW_TAB);
  } else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)) {
    GdkFileList *file_list = g_value_get_object (value);
    g_autoptr (GSList) files = gdk_file_list_get_files (file_list);
    GSList *l;
    int i = 0;

    for (l = files; l && i < MAX_NUMBER_OF_URLS; l = l->next) {
      GFile *file = l->data;
      g_autofree char *uri = g_file_get_uri (file);

      ephy_link_open (window, uri, embed, (embed && i == 0) ? 0 : EPHY_LINK_NEW_TAB);
      i++;
    }
  } else if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
    const char *text = g_value_get_string (value);
    g_auto (GStrv) split = g_strsplit (text, "\n", MAX_NUMBER_OF_URLS);
    int i;

    for (i = 0; split[i] && *split[i] != '\0'; i++) {
      const char *uri = split[i];
      g_autofree char *normalized =
        ephy_embed_utils_normalize_or_autosearch_address (uri);

      ephy_link_open (window, normalized, embed, (embed && i == 0) ? 0 : EPHY_LINK_NEW_TAB);
    }
  } else {
    g_assert_not_reached ();
  }
}

static void
visibility_policy_changed_cb (EphyTabView *self)
{
  EphyEmbedShellMode mode;
  EphyPrefsUITabsBarVisibilityPolicy policy;

  mode = ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

  if (is_desktop_pantheon ())
    policy = EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_ALWAYS;
  else
    policy = g_settings_get_enum (EPHY_SETTINGS_UI,
                                  EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY);

  adw_tab_bar_set_autohide (self->tab_bar,
                            policy != EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_ALWAYS);
  gtk_widget_set_visible (GTK_WIDGET (self->tab_bar),
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION &&
                          policy != EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY_NEVER);
}

static void
expand_changed_cb (EphyTabView *self)
{
  gboolean expand = g_settings_get_boolean (EPHY_SETTINGS_UI,
                                            EPHY_PREFS_UI_EXPAND_TABS_BAR);

  adw_tab_bar_set_expand_tabs (self->tab_bar, expand);
}

static gboolean
is_layout_reversed (void)
{
  GtkSettings *settings;
  g_autofree char *layout = NULL;
  g_auto (GStrv) parts = NULL;

  settings = gtk_settings_get_default ();
  g_object_get (settings, "gtk-decoration-layout", &layout, NULL);

  parts = g_strsplit (layout, ":", 2);

  /* Invalid layout, don't even try */
  if (g_strv_length (parts) < 2)
    return FALSE;

  return !!g_strrstr (parts[0], "close");
}

static void
notify_decoration_layout_cb (EphyTabView *self)
{
  gboolean inverted = is_layout_reversed ();

  if (self->tab_bar)
    adw_tab_bar_set_inverted (self->tab_bar, inverted);

  if (self->tab_overview)
    adw_tab_overview_set_inverted (self->tab_overview, inverted);
}

void
ephy_tab_view_set_tab_bar (EphyTabView *self,
                           AdwTabBar   *tab_bar)
{
  GtkSettings *settings;

  self->tab_bar = tab_bar;

  adw_tab_bar_setup_extra_drop_target (tab_bar, GDK_ACTION_COPY, (GType[3]) {
    G_TYPE_STRING,
    G_TYPE_FILE,
    GDK_TYPE_FILE_LIST,
  }, 3);

  g_signal_connect_object (tab_bar, "extra-drag-drop",
                           G_CALLBACK (drag_drop_cb), self,
                           G_CONNECT_SWAPPED);

  if (is_desktop_pantheon ()) {
    GtkWidget *button;

    adw_tab_bar_set_autohide (tab_bar, FALSE);
    adw_tab_bar_set_expand_tabs (tab_bar, FALSE);

    button = gtk_button_new_from_icon_name ("list-add-symbolic");
    /* Translators: tooltip for the new tab button */
    gtk_widget_set_tooltip_text (button, _("Open a new tab"));
    gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.new-tab");
    gtk_widget_add_css_class (button, "flat");

    adw_tab_bar_set_start_action_widget (tab_bar, button);
  } else {
    g_signal_connect_object (EPHY_SETTINGS_UI,
                             "changed::" EPHY_PREFS_UI_TABS_BAR_VISIBILITY_POLICY,
                             G_CALLBACK (visibility_policy_changed_cb), self,
                             G_CONNECT_SWAPPED);

    g_signal_connect_object (EPHY_SETTINGS_UI,
                             "changed::" EPHY_PREFS_UI_EXPAND_TABS_BAR,
                             G_CALLBACK (expand_changed_cb), self,
                             G_CONNECT_SWAPPED);

    visibility_policy_changed_cb (self);
    expand_changed_cb (self);
  }

  settings = gtk_settings_get_default ();
  g_signal_connect_object (settings, "notify::gtk-decoration-layout",
                           G_CALLBACK (notify_decoration_layout_cb), self,
                           G_CONNECT_SWAPPED);

  visibility_policy_changed_cb (self);
  notify_decoration_layout_cb (self);
}

void
ephy_tab_view_set_tab_overview (EphyTabView    *self,
                                AdwTabOverview *tab_overview)
{
  self->tab_overview = tab_overview;

  adw_tab_overview_setup_extra_drop_target (tab_overview, GDK_ACTION_COPY, (GType[3]) {
    G_TYPE_STRING,
    G_TYPE_FILE,
    GDK_TYPE_FILE_LIST,
  }, 3);

  g_signal_connect_object (tab_overview, "extra-drag-drop",
                           G_CALLBACK (drag_drop_cb), self,
                           G_CONNECT_SWAPPED);

  notify_decoration_layout_cb (self);
}
