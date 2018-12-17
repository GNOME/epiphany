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

struct _EphyDocumentView
{
  GtkScrolledWindow parent_instance;

  GtkWidget *ev_view;
  EvDocumentModel *model;
};

G_DEFINE_TYPE (EphyDocumentView, ephy_document_view, GTK_TYPE_SCROLLED_WINDOW)

static void
ephy_document_view_finalize (GObject* object)
{
  EphyDocumentView *view = EPHY_DOCUMENT_VIEW (object);

  g_clear_object (&view->model);

  G_OBJECT_CLASS (ephy_document_view_parent_class)->finalize (object);
}

static void
ephy_document_view_constructed (GObject* object)
{
  EphyDocumentView *view = EPHY_DOCUMENT_VIEW (object);

  G_OBJECT_CLASS (ephy_document_view_parent_class)->constructed (object);

  view->ev_view = ev_view_new ();
  view->model = ev_document_model_new ();
  ev_view_set_model (EV_VIEW (view->ev_view), view->model);

  gtk_container_add (GTK_CONTAINER (view), view->ev_view);
  gtk_widget_show (view->ev_view);
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
                       "hadjustment", NULL,
                       "vadjustment", NULL,
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

  job = ev_job_load_new (uri);
  g_signal_connect (job, "finished",
                    G_CALLBACK (document_load_job_finished),
                    view->model);
  ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_NONE);
}
