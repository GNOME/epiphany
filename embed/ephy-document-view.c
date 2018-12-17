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

#include <glib/gstdio.h>

struct _EphyDocumentView
{
  GtkBox parent_instance;

  GtkWidget *scrolled_window;
  GtkWidget *ev_view;
  EvDocumentModel *model;
  gchar *uri;
};

G_DEFINE_TYPE (EphyDocumentView, ephy_document_view, GTK_TYPE_BOX)

static void
ephy_document_view_finalize (GObject* object)
{
  EphyDocumentView *view = EPHY_DOCUMENT_VIEW (object);

  if (view->uri != NULL) {
    g_unlink (view->uri + 7);
    g_free (view->uri);
  }

  g_clear_object (&view->model);

  G_OBJECT_CLASS (ephy_document_view_parent_class)->finalize (object);
}

static void
ephy_document_view_constructed (GObject* object)
{
  EphyDocumentView *view = EPHY_DOCUMENT_VIEW (object);

  G_OBJECT_CLASS (ephy_document_view_parent_class)->constructed (object);

  view->scrolled_window = gtk_scrolled_window_new (NULL, NULL);

  view->ev_view = ev_view_new ();
  gtk_container_add (GTK_CONTAINER (view->scrolled_window), view->ev_view);

  view->model = ev_document_model_new ();
  ev_view_set_model (EV_VIEW (view->ev_view), view->model);

  GtkWidget *toolbar = gtk_toolbar_new ();
  gtk_box_pack_start (GTK_BOX (view), toolbar, FALSE, TRUE, 0);

  GtkToolItem *save_button = gtk_tool_button_new (NULL, NULL);;
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (save_button), "document-save-symbolic");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), save_button, -1);

  gtk_box_pack_end (GTK_BOX (view), view->scrolled_window, TRUE, TRUE, 0);
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
document_load_job_finished (EvJob *job,
                            EvDocumentModel *model)
{
  if (ev_job_is_failed (job)) {
    /* FIXME: Error reporting */
    g_warning ("Failed to load document");
  } else
    ev_document_model_set_document (model, job->document);
  g_object_unref (job);
}

void
ephy_document_view_load_uri (EphyDocumentView *view,
                             const char *uri)
{
  EvJob *job;

  g_return_if_fail (EPHY_DOCUMENT_VIEW (view));
  g_return_if_fail (uri != NULL);

  if (view->uri != NULL && strcmp (view->uri, uri)) {
    g_unlink (view->uri + 7);
  }

  g_free (view->uri);
  view->uri = g_strdup (uri);
 

  job = ev_job_load_new (uri);
  g_signal_connect (job, "finished",
                    G_CALLBACK (document_load_job_finished),
                    view->model);
  ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_NONE);
}
