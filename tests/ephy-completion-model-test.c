/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2012 Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ephy-completion-model.h"

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-shell.h"

static void
test_ephy_completion_model_create (void)
{
    EphyCompletionModel *model;
    model = ephy_completion_model_new (EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ())),
                                       ephy_shell_get_bookmarks (ephy_shell_get_default ()), TRUE);
    g_assert (model);
    g_object_unref (model);
}

static void
update_empty_cb (EphyHistoryService *service,
                 gboolean success,
                 gpointer result_data,
                 GMainLoop *loop)
{
    GList *results = (GList*)result_data;

    g_assert (success);
    g_assert (results == NULL);

    g_main_loop_quit (loop);
}

static void
test_ephy_completion_model_update_empty (void)
{
    EphyCompletionModel *model;
    GMainLoop *loop = NULL;

    model = ephy_completion_model_new (EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ())),
                                       ephy_shell_get_bookmarks (ephy_shell_get_default ()), TRUE);
    g_assert (model);

    loop = g_main_loop_new (NULL, FALSE);

    ephy_completion_model_update_for_string (model, "hello",
                                             (EphyHistoryJobCallback)update_empty_cb,
                                             loop);

    g_main_loop_run (loop);

    g_object_unref (model);
    g_main_loop_unref (loop);
}

int
main (int argc, char *argv[])
{
  gboolean ret;

  gtk_test_init (&argc, &argv);
  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);

  g_test_add_func ("/src/ephy-completion-model/create",
                   test_ephy_completion_model_create);

  g_test_add_func ("/src/ephy-completion-model/update_empty",
                   test_ephy_completion_model_update_empty);

  ret = g_test_run ();

  return ret;
}

