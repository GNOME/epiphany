/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-document-view.h"

#include <evince-view.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>

struct _EphyDocumentView
{
  GtkBox parent_instance;

  GtkWidget *scrolled_window;
  GtkWidget *ev_view;
  GtkWidget *current_page;
  GtkWidget *total_page;
  GtkWidget *sizing_mode;

  EvJob *job;
  EvDocumentModel *model;
  SoupURI *uri;
};

G_DEFINE_TYPE (EphyDocumentView, ephy_document_view, GTK_TYPE_BOX)

static void
ephy_document_view_finalize (GObject* object)
{
  EphyDocumentView *view = EPHY_DOCUMENT_VIEW (object);

  if (view->uri != NULL) {
    g_unlink (view->uri->path);
    soup_uri_free (view->uri);
    view->uri = NULL;
  }

  if (view->job != NULL) {
    ev_job_cancel (view->job);
    view->job = NULL;
  }

  g_clear_object (&view->model);

  G_OBJECT_CLASS (ephy_document_view_parent_class)->finalize (object);
}

static void
on_save_button_clicked (GtkButton        *button,
                        EphyDocumentView *view)
{
  GtkFileChooserNative *chooser;
  gint res;
  g_autofree gchar *basename = g_path_get_basename (view->uri->path);

  chooser = gtk_file_chooser_native_new (_("Save File"),
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        NULL,
                                        NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);

  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), basename);

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (chooser));
  if (res == GTK_RESPONSE_ACCEPT) {
    g_autofree gchar *filename;
    GFile *source = g_file_new_for_uri (view->uri->path);
    GFile *dest;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
    dest = g_file_new_for_path (filename);

    g_file_copy (source, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);
  }

  g_object_unref (chooser);
}

static void
ephy_document_view_constructed (GObject *object)
{
  EphyDocumentView *view = EPHY_DOCUMENT_VIEW (object);

  G_OBJECT_CLASS (ephy_document_view_parent_class)->constructed (object);

  view->scrolled_window = gtk_scrolled_window_new (NULL, NULL);

  view->ev_view = ev_view_new ();
  gtk_container_add (GTK_CONTAINER (view->scrolled_window), view->ev_view);
  gtk_box_pack_start (GTK_BOX (view), view->scrolled_window, TRUE, TRUE, 0);

  view->model = ev_document_model_new ();
  ev_view_set_model (EV_VIEW (view->ev_view), view->model);

  GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (view), separator, FALSE, TRUE, 0);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (box, 6);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_box_pack_start (GTK_BOX (view), box, FALSE, TRUE, 0);

  GtkWidget *button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(button_box), GTK_STYLE_CLASS_LINKED);
  gtk_style_context_add_class(gtk_widget_get_style_context(button_box), GTK_STYLE_CLASS_RAISED);
  gtk_box_pack_start (GTK_BOX (box), button_box, FALSE, TRUE, 0);

  view->current_page = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (view->current_page), 2);
  gtk_box_pack_start (GTK_BOX (button_box), view->current_page, FALSE, TRUE, 0);

  view->total_page = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (view->total_page), 5);
  gtk_widget_set_sensitive (view->total_page, FALSE);
  gtk_box_pack_start (GTK_BOX (button_box), view->total_page, FALSE, TRUE, 0);

  GtkWidget *save_button = gtk_button_new_from_icon_name ("document-save-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (save_button, "clicked", G_CALLBACK (on_save_button_clicked), view);
  gtk_box_pack_end (GTK_BOX (box), save_button, FALSE, TRUE, 0);

  view->sizing_mode = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (view->sizing_mode), _("Fit Page"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (view->sizing_mode), _("Fit Width"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (view->sizing_mode), _("Free"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (view->sizing_mode), _("Automatic"));
  gtk_box_pack_end (GTK_BOX (box), view->sizing_mode, FALSE, TRUE, 0);

  //gtk_widget_show_all (view);
}

static void
ephy_document_view_class_init (EphyDocumentViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_document_view_constructed;
  object_class->finalize = ephy_document_view_finalize;
}

static void
ephy_document_view_init (EphyDocumentView *view)
{
}

GtkWidget *
ephy_document_view_new (void)
{
  return g_object_new (EPHY_TYPE_DOCUMENT_VIEW,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       NULL);
}

static void
on_page_changed (EvDocumentModel    *model,
                 gint                old_page,
                 gint                new_page,
                 EphyDocumentView   *view)
{
  g_autofree gchar *page = g_strdup_printf ("%d", new_page + 1);

  gtk_entry_set_text (GTK_ENTRY (view->current_page), page);
}

static void
on_activate (GtkEntry         *entry,
             EphyDocumentView *view)
{
  const gchar *text = gtk_entry_get_text (entry);
  gint page = atoi (text);

  ev_document_model_set_page (view->model, page - 1);
}

static void
on_sizing_mode_changed (GtkComboBox      *widget,
                        EphyDocumentView *view)
{
  gint index = gtk_combo_box_get_active (widget);

  ev_document_model_set_sizing_mode (view->model, index);
}

static void
document_load_job_finished (EvJob            *job,
                            EphyDocumentView *view)
{
  if (ev_job_is_failed (job)) {
    /* FIXME: Error reporting */
    g_warning ("Failed to load document");
    return;
  } else
    ev_document_model_set_document (view->model, job->document);


  EvDocument *ev_document = ev_document_model_get_document (view->model);
  gint n_pages = ev_document_get_n_pages (ev_document);
  g_autofree gchar *total_pages = g_strdup_printf (("of %d"), n_pages);
  g_signal_connect (view->model, "page-changed", G_CALLBACK (on_page_changed), view);
  g_signal_connect (view->current_page, "activate", G_CALLBACK (on_activate), view);
  gtk_entry_set_text (GTK_ENTRY (view->total_page), total_pages);
  gtk_entry_set_text (GTK_ENTRY (view->current_page), "1");
  g_signal_connect (view->sizing_mode, "changed", G_CALLBACK (on_sizing_mode_changed), view);
  gtk_combo_box_set_active (GTK_COMBO_BOX (view->sizing_mode), ev_document_model_get_sizing_mode (view->model));

  g_object_unref (job);
  view->job = NULL;
}

void
ephy_document_view_load_uri (EphyDocumentView *view,
                             const char *uri)
{
  g_return_if_fail (EPHY_DOCUMENT_VIEW (view));
  g_return_if_fail (uri != NULL);

  if (view->uri != NULL) {
    if (strcmp (view->uri->path, uri) != 0)
      g_unlink (view->uri->path);

    soup_uri_free (view->uri);
  }

  view->uri = soup_uri_new (uri);

  view->job = ev_job_load_new (uri);
  g_signal_connect (view->job, "finished",
                    G_CALLBACK (document_load_job_finished),
                    view);
  ev_job_scheduler_push_job (view->job, EV_JOB_PRIORITY_NONE);
}
