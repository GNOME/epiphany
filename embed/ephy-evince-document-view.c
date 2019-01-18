/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
 *  Copyright © 2018 Jan-Michael Brummer
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

#include "config.h"
#include "ephy-evince-document-view.h"
#include "ephy-embed-shell.h"

#include <evince-view.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>

struct _EphyEvinceDocumentView {
  GtkBox parent_instance;

  GtkWidget *view;
  GtkWidget *current_page;
  GtkWidget *total_page;
  GtkWidget *sizing_mode;
  GtkWidget *popup;

  EphyEmbed *embed;

  EvJob *job;
  EvDocumentModel *model;
  SoupURI *uri;
};

G_DEFINE_TYPE (EphyEvinceDocumentView, ephy_evince_document_view, GTK_TYPE_BOX)

static void
ephy_evince_document_view_finalize (GObject *object)
{
  EphyEvinceDocumentView *self = EPHY_EVINCE_DOCUMENT_VIEW (object);

  if (self->uri != NULL) {
    g_unlink (self->uri->path);

    g_clear_pointer (&self->uri, soup_uri_free);
  }

  if (self->job != NULL) {
    ev_job_cancel (self->job);
    g_clear_object (&self->job);
  }

  g_clear_object (&self->model);

  G_OBJECT_CLASS (ephy_evince_document_view_parent_class)->finalize (object);
}

static void
on_save_button_clicked (GtkButton              *button,
                        EphyEvinceDocumentView *self)
{
  g_autoptr (GtkFileChooserNative) native = NULL;
  GtkFileChooser *chooser;
  GError *error = NULL;
  gint res;
  g_autofree gchar *basename = g_path_get_basename (self->uri->path);

  native = gtk_file_chooser_native_new (_("Save File"),
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        NULL,
                                        NULL);

  chooser = GTK_FILE_CHOOSER (native);

  gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

  gtk_file_chooser_set_current_name (chooser, basename);

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));
  if (res == GTK_RESPONSE_ACCEPT) {
    g_autofree gchar *filename;
    GFile *source = g_file_new_for_uri (self->uri->path);
    GFile *dest;

    filename = gtk_file_chooser_get_filename (chooser);
    dest = g_file_new_for_path (filename);

    if (!g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)) {
      g_warning ("%s(): Could not copy file: %s\n", __FUNCTION__, error->message);
      g_error_free (error);
    }
  }
}

static void
view_external_link_cb (EvView                 *view,
                       EvLinkAction           *action,
                       EphyEvinceDocumentView *self)
{
  EvLinkActionType type = ev_link_action_get_action_type (action);
  EphyWebView *webview;
  const gchar *url;

  if (type != EV_LINK_ACTION_TYPE_EXTERNAL_URI)
    return;

  url = ev_link_action_get_uri (action);

  webview = ephy_embed_get_web_view (self->embed);
  ephy_web_view_load_url (webview, url);
}

static void
ephy_evince_document_view_constructed (GObject *object)
{
  EphyEvinceDocumentView *self = EPHY_EVINCE_DOCUMENT_VIEW (object);
  GtkWidget *box;
  GtkWidget *entry_box;
  GtkWidget *save_button;
  GtkWidget *scrolled_window;
  GtkWidget *separator;

  G_OBJECT_CLASS (ephy_evince_document_view_parent_class)->constructed (object);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);

  self->view = ev_view_new ();
  g_signal_connect (self->view, "external-link", G_CALLBACK (view_external_link_cb), self);
  gtk_container_add (GTK_CONTAINER (scrolled_window), self->view);
  gtk_box_pack_start (GTK_BOX (self), scrolled_window, TRUE, TRUE, 0);

  self->model = ev_document_model_new ();
  ev_view_set_model (EV_VIEW (self->view), self->model);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (self), separator, FALSE, TRUE, 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_box_pack_start (GTK_BOX (self), box, FALSE, TRUE, 0);

  entry_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (entry_box), GTK_STYLE_CLASS_LINKED);
  gtk_style_context_add_class (gtk_widget_get_style_context (entry_box), GTK_STYLE_CLASS_RAISED);
  gtk_box_pack_start (GTK_BOX (box), entry_box, FALSE, TRUE, 0);

  self->current_page = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (self->current_page), 2);
  gtk_box_pack_start (GTK_BOX (entry_box), self->current_page, FALSE, TRUE, 0);

  self->total_page = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (self->total_page), 5);
  gtk_widget_set_sensitive (self->total_page, FALSE);
  gtk_box_pack_start (GTK_BOX (entry_box), self->total_page, FALSE, TRUE, 0);

  save_button = gtk_button_new_from_icon_name ("document-save-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (save_button, "clicked", G_CALLBACK (on_save_button_clicked), self);
  gtk_box_pack_end (GTK_BOX (box), save_button, FALSE, TRUE, 0);

  self->sizing_mode = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->sizing_mode), _("Fit Page"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->sizing_mode), _("Fit Width"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->sizing_mode), _("Free"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->sizing_mode), _("Automatic"));
  gtk_box_pack_end (GTK_BOX (box), self->sizing_mode, FALSE, TRUE, 0);
}

static void
ephy_evince_document_view_class_init (EphyEvinceDocumentViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_evince_document_view_constructed;
  object_class->finalize = ephy_evince_document_view_finalize;
}

static void
ephy_evince_document_view_init (EphyEvinceDocumentView *self)
{
}

GtkWidget *
ephy_evince_document_view_new (void)
{
  return g_object_new (EPHY_TYPE_EVINCE_DOCUMENT_VIEW,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       NULL);
}

static void
on_page_changed (EvDocumentModel        *model,
                 gint                    old_page,
                 gint                    new_page,
                 EphyEvinceDocumentView *self)
{
  g_autofree gchar *page = g_strdup_printf ("%d", new_page + 1);

  gtk_entry_set_text (GTK_ENTRY (self->current_page), page);
}

static void
on_current_page_activate (GtkEntry               *entry,
                          EphyEvinceDocumentView *self)
{
  const gchar *text = gtk_entry_get_text (entry);
  gint page = atoi (text);

  ev_document_model_set_page (self->model, page - 1);
}

static void
on_sizing_mode_changed (GtkComboBox            *widget,
                        EphyEvinceDocumentView *self)
{
  gint index = gtk_combo_box_get_active (widget);

  ev_document_model_set_sizing_mode (self->model, index);
}

static void
on_popup_copy_activate (GtkMenuItem            *menuitem,
                        EphyEvinceDocumentView *self)
{
  GtkClipboard *clipboard;
  g_autofree gchar *selected_text = ev_view_get_selected_text (EV_VIEW (self->view));

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (GTK_CLIPBOARD (clipboard), selected_text, strlen (selected_text));
}

static gboolean
on_view_popup_cb (EvView                 *view,
                  GList                  *items,
                  EphyEvinceDocumentView *self)
{
  if (!ev_view_get_has_selection (view))
    return TRUE;

  if (!self->popup) {
    GtkWidget *copy = gtk_menu_item_new_with_label (_("Copy"));
    self->popup = gtk_menu_new ();
    g_signal_connect (copy, "activate", G_CALLBACK (on_popup_copy_activate), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (self->popup), copy);
    gtk_menu_attach_to_widget (GTK_MENU (self->popup), GTK_WIDGET (self), NULL);
    gtk_widget_show_all (self->popup);
  }

  gtk_menu_popup_at_pointer (GTK_MENU (self->popup), NULL);

  return TRUE;
}

static void
document_load_job_finished (EvJob                  *job,
                            EphyEvinceDocumentView *self)
{
  g_autofree gchar *total_pages = NULL;
  gint n_pages;

  if (ev_job_is_failed (job)) {
    g_warning ("Failed to load document: %s", job->error->message);
    g_error_free (job->error);

    return;
  }

  ev_document_model_set_document (self->model, job->document);
  n_pages = ev_document_get_n_pages (job->document);

  /* Translators: Number of x total pages */
  total_pages = g_strdup_printf (_("of %d"), n_pages);
  g_signal_connect (self->model, "page-changed", G_CALLBACK (on_page_changed), self);
  g_signal_connect (self->current_page, "activate", G_CALLBACK (on_current_page_activate), self);
  gtk_entry_set_text (GTK_ENTRY (self->total_page), total_pages);
  gtk_entry_set_text (GTK_ENTRY (self->current_page), "1");
  g_signal_connect (self->sizing_mode, "changed", G_CALLBACK (on_sizing_mode_changed), self);
  gtk_combo_box_set_active (GTK_COMBO_BOX (self->sizing_mode), ev_document_model_get_sizing_mode (self->model));

  g_signal_connect_object (self->view, "popup", G_CALLBACK (on_view_popup_cb), self, 0);
}

void
ephy_evince_document_view_load_uri (EphyEvinceDocumentView *self,
                                    const char             *uri)
{
  g_return_if_fail (EPHY_EVINCE_DOCUMENT_VIEW (self));
  g_return_if_fail (uri != NULL);

  if (self->uri != NULL) {
    if (strcmp (self->uri->path, uri) != 0)
      g_unlink (self->uri->path);

    g_clear_pointer (&self->uri, soup_uri_free);
  }

  if (self->job != NULL) {
    ev_job_cancel (self->job);

    g_clear_object (&self->job);
  }

  self->uri = soup_uri_new (uri);

  self->job = ev_job_load_new (uri);
  g_signal_connect_object (self->job, "finished", G_CALLBACK (document_load_job_finished), self, 0);
  ev_job_scheduler_push_job (self->job, EV_JOB_PRIORITY_NONE);
}

void
ephy_evince_document_set_embed (EphyEvinceDocumentView *self,
                                EphyEmbed              *embed)
{
  self->embed = embed;
}
