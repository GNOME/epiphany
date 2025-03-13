/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Jan-Michael Brummer
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

#include "ephy-privacy-report.h"

struct _EphyPrivacyReport {
  AdwDialog parent_instance;

  GtkWidget *headerbar;
  GtkWidget *navigation_view;
  GtkWidget *stack;
  GtkWidget *website_prefs_page;
  GtkWidget *tracker_prefs_page;
  GtkWidget *website_listbox;
  GtkWidget *tracker_listbox;
  GtkWidget *details_page;
  GtkWidget *details_prefs_page;
  GtkWidget *details_listbox;

  GHashTable *website_table;
  GHashTable *tracker_table;
};

G_DEFINE_FINAL_TYPE (EphyPrivacyReport, ephy_privacy_report, ADW_TYPE_DIALOG)

static void
add_details_domain (gpointer data,
                    gpointer user_data)
{
  EphyPrivacyReport *self = EPHY_PRIVACY_REPORT (user_data);
  GtkWidget *row;

  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), data);
  gtk_list_box_append (GTK_LIST_BOX (self->details_listbox), row);
}

static void
on_website_listbox_activated (GtkListBox    *box,
                              GtkListBoxRow *row,
                              gpointer       user_data)
{
  EphyPrivacyReport *self = EPHY_PRIVACY_REPORT (user_data);
  AdwActionRow *action_row = ADW_ACTION_ROW (row);
  const char *name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (action_row));
  GPtrArray *domains = g_hash_table_lookup (self->website_table, name);

  adw_preferences_page_set_description (ADW_PREFERENCES_PAGE (self->details_prefs_page), _("Blocked trackers on this site"));

  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self->details_page), name);

  gtk_list_box_remove_all (GTK_LIST_BOX (self->details_listbox));
  g_ptr_array_foreach (domains, add_details_domain, self);

  adw_navigation_view_push_by_tag (ADW_NAVIGATION_VIEW (self->navigation_view), "details");
}

static void
on_tracker_listbox_activated (GtkListBox    *box,
                              GtkListBoxRow *row,
                              gpointer       user_data)
{
  EphyPrivacyReport *self = EPHY_PRIVACY_REPORT (user_data);
  AdwActionRow *action_row = ADW_ACTION_ROW (row);
  const char *name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (action_row));
  GPtrArray *domains = g_hash_table_lookup (self->tracker_table, name);

  adw_preferences_page_set_description (ADW_PREFERENCES_PAGE (self->details_prefs_page), _("Sites this tracker was found on"));
  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self->details_page), name);

  gtk_list_box_remove_all (GTK_LIST_BOX (self->details_listbox));
  g_ptr_array_foreach (domains, add_details_domain, self);

  adw_navigation_view_push_by_tag (ADW_NAVIGATION_VIEW (self->navigation_view), "details");
}

static void
ephy_privacy_report_dispose (GObject *object)
{
  EphyPrivacyReport *self = EPHY_PRIVACY_REPORT (object);

  g_clear_pointer (&self->website_table, g_hash_table_unref);
  g_clear_pointer (&self->tracker_table, g_hash_table_unref);

  G_OBJECT_CLASS (ephy_privacy_report_parent_class)->dispose (object);
}

static void
ephy_privacy_report_class_init (EphyPrivacyReportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_privacy_report_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/privacy-report.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, website_listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, tracker_listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, stack);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, website_prefs_page);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, tracker_prefs_page);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, headerbar);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, details_page);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, details_listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyPrivacyReport, details_prefs_page);

  gtk_widget_class_bind_template_callback (widget_class, on_website_listbox_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_tracker_listbox_activated);
}

static void
ephy_privacy_report_init (EphyPrivacyReport *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->website_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_ptr_array_unref);
  self->tracker_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_ptr_array_unref);
}

static void
add_domain_row (gpointer key,
                gpointer value,
                gpointer user_data)
{
  GtkListBox *listbox = GTK_LIST_BOX (user_data);
  GPtrArray *array = value;
  GtkWidget *count;
  GtkWidget *row;
  GtkWidget *image;
  g_autofree char *text = g_strdup_printf ("%d", array->len);

  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), key);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  count = gtk_label_new (text);
  gtk_widget_add_css_class (count, "dim-label");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), count);

  image = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), image);

  gtk_list_box_append (listbox, row);
}

static gint
sort_domain (GtkListBoxRow *row1,
             GtkListBoxRow *row2,
             gpointer       user_data)
{
  GHashTable *table = user_data;
  AdwActionRow *a1 = ADW_ACTION_ROW (row1);
  AdwActionRow *a2 = ADW_ACTION_ROW (row2);
  const char *name1 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (a1));
  const char *name2 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (a2));
  GPtrArray *array1 = g_hash_table_lookup (table, name1);
  GPtrArray *array2 = g_hash_table_lookup (table, name2);
  int n1 = array1->len;
  int n2 = array2->len;

  if (n1 < n2)
    return 1;

  if (n1 > n2)
    return -1;

  return 0;
}

static void
itp_report_ready (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  WebKitWebsiteDataManager *manager = WEBKIT_WEBSITE_DATA_MANAGER (source_object);
  g_autoptr (EphyWindow) window = EPHY_WINDOW (user_data);
  g_autolist (WebKitITPThirdParty) summary = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *description = NULL;
  EphyPrivacyReport *self = NULL;
  guint length;

  summary = webkit_website_data_manager_get_itp_summary_finish (manager, res, &error);
  if (error) {
    g_warning ("Could not fetch ITP summary: %s", error->message);
    return;
  }

  self = EPHY_PRIVACY_REPORT (g_object_new (EPHY_TYPE_PRIVACY_REPORT, NULL));

  /* Create website and tracker tables */
  for (GList *tp_list = summary; tp_list && tp_list->data; tp_list = tp_list->next) {
    WebKitITPThirdParty *tp = (WebKitITPThirdParty *)(tp_list->data);

    for (GList *fp_list = webkit_itp_third_party_get_first_parties (tp); fp_list && fp_list->data; fp_list = fp_list->next) {
      WebKitITPFirstParty *fp = (WebKitITPFirstParty *)(fp_list->data);

      if (!webkit_itp_first_party_get_website_data_access_allowed (fp)) {
        const char *fp_domain = webkit_itp_first_party_get_domain (fp);
        const char *tp_domain = webkit_itp_third_party_get_domain (tp);
        GPtrArray *websites = NULL;
        GPtrArray *trackers = NULL;

        /* Websites */
        if (g_hash_table_lookup_extended (self->website_table, fp_domain, NULL, (gpointer *)&websites)) {
          g_ptr_array_add (websites, g_strdup (tp_domain));
        } else {
          websites = g_ptr_array_new_with_free_func (g_free);
          g_ptr_array_add (websites, g_strdup (tp_domain));
          g_hash_table_insert (self->website_table, g_strdup (fp_domain), websites);
        }

        /* Tracker */
        if (g_hash_table_lookup_extended (self->tracker_table, tp_domain, NULL, (gpointer *)&trackers)) {
          g_ptr_array_add (trackers, g_strdup (fp_domain));
        } else {
          trackers = g_ptr_array_new_with_free_func (g_free);
          g_ptr_array_add (trackers, g_strdup (fp_domain));
          g_hash_table_insert (self->tracker_table, g_strdup (tp_domain), trackers);
        }
      }
    }
  }

  /* Add tables to widgets */
  g_hash_table_foreach (self->website_table, add_domain_row, self->website_listbox);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->website_listbox), sort_domain, self->website_table, NULL);
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->website_listbox));

  g_hash_table_foreach (self->tracker_table, add_domain_row, self->tracker_listbox);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->tracker_listbox), sort_domain, self->tracker_table, NULL);
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->tracker_listbox));

  length = g_list_length (summary);
  description = g_strdup_printf (ngettext ("GNOME Web prevented %u tracker from following you across websites", "GNOME Web prevented %u trackers from following you across websites", length), length);
  adw_preferences_page_set_description (ADW_PREFERENCES_PAGE (self->website_prefs_page), description);
  adw_preferences_page_set_description (ADW_PREFERENCES_PAGE (self->tracker_prefs_page), description);

  if (!length) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "no-trackers-page");
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (self->headerbar), NULL);
  }

  adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (window));
}

static WebKitWebsiteDataManager *
get_website_data_manager (void)
{
  WebKitNetworkSession *network_session;

  network_session = ephy_embed_shell_get_network_session (ephy_embed_shell_get_default ());
  return webkit_network_session_get_website_data_manager (network_session);
}

void
ephy_privacy_report_show (EphyWindow *window)
{
  webkit_website_data_manager_get_itp_summary (get_website_data_manager (), NULL, itp_report_ready, g_object_ref (window));
}
