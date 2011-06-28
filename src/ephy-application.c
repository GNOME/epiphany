/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#include "ephy-application.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-debug.h"
#include "ephy-profile-utils.h"

#include <string.h>

enum {
  PROP_0,
  PROP_PRIVATE_INSTANCE,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

#define EPHY_APPLICATION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_APPLICATION, EphyApplicationPrivate))

struct _EphyApplicationPrivate {
  EphyApplicationStartupContext *startup_context;
  gboolean private_instance;
};

G_DEFINE_TYPE (EphyApplication, ephy_application, GTK_TYPE_APPLICATION);

static void ephy_application_finalize (GObject *object);

/**
 * ephy_application_startup_context_new:
 * @bookmarks_filename: A bookmarks file to import.
 * @session_filename: A session to restore.
 * @bookmark_url: A URL to be added to the bookmarks.
 * @arguments: A %NULL-terminated array of URLs and file URIs to be opened.
 * @user_time: The user time when the EphyApplication startup was invoked.
 *
 * Creates a new startup context. All string parameters, including
 * @arguments, are copied.
 *
 * Returns: a newly allocated #EphyApplicationStartupContext
 **/
EphyApplicationStartupContext *
ephy_application_startup_context_new (EphyStartupFlags startup_flags,
                                      char *bookmarks_filename,
                                      char *session_filename,
                                      char *bookmark_url,
                                      char **arguments,
                                      guint32 user_time)
{
  EphyApplicationStartupContext *ctx = g_slice_new0 (EphyApplicationStartupContext);

  ctx->startup_flags = startup_flags;

  ctx->bookmarks_filename = g_strdup (bookmarks_filename);
  ctx->session_filename = g_strdup (session_filename);
  ctx->bookmark_url = g_strdup (bookmark_url);

  ctx->arguments = g_strdupv (arguments);

  ctx->user_time = user_time;

  return ctx;
}

static void
ephy_application_free_startup_context (EphyApplication *application)
{
  EphyApplicationStartupContext *ctx = application->priv->startup_context;

  g_assert (ctx != NULL);

  g_free (ctx->bookmarks_filename);
  g_free (ctx->session_filename);
  g_free (ctx->bookmark_url);

  g_strfreev (ctx->arguments);

  g_slice_free (EphyApplicationStartupContext, ctx);

  application->priv->startup_context = NULL;
}

static void
queue_commands (EphyApplication *application)
{
  EphyApplicationStartupContext *ctx;
  EphyShell *shell;
  EphySession *session;

  shell = ephy_shell_get_default ();
  g_assert (shell != NULL);
  session = EPHY_SESSION (ephy_shell_get_session (shell));
  g_assert (session != NULL);

  ctx = application->priv->startup_context;

  /* We only get here when starting a new instance, so we first need
     to autoresume! */
  ephy_session_queue_command (EPHY_SESSION (ephy_shell_get_session (shell)),
                              EPHY_SESSION_CMD_RESUME_SESSION,
                              NULL, NULL, ctx->user_time, TRUE);

  if (ctx->startup_flags & EPHY_STARTUP_BOOKMARKS_EDITOR)
    ephy_session_queue_command (session,
                                EPHY_SESSION_CMD_OPEN_BOOKMARKS_EDITOR,
                                NULL, NULL, ctx->user_time, FALSE);

  else if (ctx->session_filename != NULL) {
    ephy_session_queue_command (session,
                                EPHY_SESSION_CMD_LOAD_SESSION,
                                (const char *)ctx->session_filename, NULL,
                                ctx->user_time, FALSE);
  } else if (ctx->arguments != NULL) {
    /* Don't queue any window openings if no extra arguments given, */
    /* since session autoresume will open one for us. */
    GString *options;

    options = g_string_sized_new (64);

    if (ctx->startup_flags & EPHY_STARTUP_NEW_WINDOW) {
      g_string_append (options, "new-window,");
    }
    if (ctx->startup_flags & EPHY_STARTUP_NEW_TAB) {
      g_string_append (options, "new-tab,external,");
    }

    ephy_session_queue_command (session,
                                EPHY_SESSION_CMD_OPEN_URIS,
                                (const char*)options->str,
                                (const char **)ctx->arguments,
                                ctx->user_time, FALSE);
  }
}

static void
ephy_application_startup (GApplication* application)
{
  /* We're not remoting; start our services */
  /* Migrate profile if we are not running a private instance */
  if (ephy_has_private_profile () == FALSE &&
      ephy_profile_utils_get_migration_version () < EPHY_PROFILE_MIGRATION_VERSION) {
    GError *error = NULL;
    char *argv[1] = { "ephy-profile-migrator" };
    char *envp[1] = { "EPHY_LOG_MODULES=ephy-profile" };

    g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH,
                  NULL, NULL, NULL, NULL,
                  NULL, &error);

    if (error) {
      LOG ("Failed to run migrator: %s", error->message);
      g_error_free (error);
    }
  }
}

static void
ephy_application_activate (GApplication *application)
{
  /*
   * We get here on each new instance (remote or not). Queue the
   * commands.
   */
  queue_commands (EPHY_APPLICATION (application));
}

/*
 * We use this enumeration to conveniently fill and read from the
 * dictionary variant that is sent from the remote to the primary
 * instance.
 */
typedef enum {
  CTX_STARTUP_FLAGS,
  CTX_BOOKMARKS_FILENAME,
  CTX_SESSION_FILENAME,
  CTX_BOOKMARK_URL,
  CTX_ARGUMENTS,
  CTX_USER_TIME
}CtxEnum;

static void
ephy_application_add_platform_data (GApplication *application,
                                    GVariantBuilder *builder)
{
  EphyApplication *app;
  EphyApplicationStartupContext *ctx;
  GVariantBuilder *ctx_builder;

  app = EPHY_APPLICATION (application);

  G_APPLICATION_CLASS (ephy_application_parent_class)->add_platform_data (application,
                                                                          builder);

  if (app->priv->startup_context) {
    /*
     * We create an array variant that contains only the elements in
     * ctx that are non-NULL.
     */
    ctx_builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
    ctx = app->priv->startup_context;

    if (ctx->startup_flags) {
      g_variant_builder_add (ctx_builder, "{iv}",
                             CTX_STARTUP_FLAGS,
                             g_variant_new_byte (ctx->startup_flags));
    }
    if (ctx->bookmarks_filename) {
      g_variant_builder_add (ctx_builder, "{iv}",
                             CTX_BOOKMARKS_FILENAME,
                             g_variant_new_string (ctx->bookmarks_filename));
    }
    if (ctx->session_filename) {
      g_variant_builder_add (ctx_builder, "{iv}",
                             CTX_SESSION_FILENAME,
                             g_variant_new_string (ctx->session_filename));
    }
    if (ctx->bookmark_url) {
      g_variant_builder_add (ctx_builder, "{iv}",
                             CTX_BOOKMARK_URL,
                             g_variant_new_string (ctx->bookmark_url));
    }
    if (ctx->arguments) {
      g_variant_builder_add (ctx_builder, "{iv}",
                             CTX_ARGUMENTS,
                             g_variant_new_strv ((const gchar * const *)ctx->arguments, -1));
    }

    g_variant_builder_add (ctx_builder, "{iv}",
                           CTX_USER_TIME,
                           g_variant_new_uint32 (ctx->user_time));

    g_variant_builder_add (builder, "{sv}",
                           "ephy-application-startup-context",
                           g_variant_builder_end (ctx_builder));

    g_variant_builder_unref (ctx_builder);
  }
}

static void
ephy_application_before_emit (GApplication *application,
                              GVariant *platform_data)
{
  GVariantIter iter, ctx_iter;
  const char *key;
  CtxEnum ctx_key;
  GVariant *value, *ctx_value;
  EphyApplicationStartupContext *ctx = NULL;

  EphyApplication *app = EPHY_APPLICATION (application);

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value)) {
    if (strcmp (key, "ephy-application-startup-context") == 0) {
      ctx = g_slice_new0 (EphyApplicationStartupContext);

      /*
       * Iterate over the startup context variant and fill the members
       * that were wired. Everything else is just NULL.
       */
      g_variant_iter_init (&ctx_iter, value);
      while (g_variant_iter_loop (&ctx_iter, "{iv}", &ctx_key, &ctx_value)) {
        switch (ctx_key) {
        case CTX_STARTUP_FLAGS:
          ctx->startup_flags = g_variant_get_byte (ctx_value);
          break;
        case CTX_BOOKMARKS_FILENAME:
          ctx->bookmarks_filename = g_variant_dup_string (ctx_value, NULL);
          break;
        case CTX_SESSION_FILENAME:
          ctx->session_filename = g_variant_dup_string (ctx_value, NULL);
          break;
        case CTX_BOOKMARK_URL:
          ctx->bookmark_url = g_variant_dup_string (ctx_value, NULL);
          break;
        case CTX_ARGUMENTS:
          ctx->arguments = g_variant_dup_strv (ctx_value, NULL);
          break;
        case CTX_USER_TIME:
          ctx->user_time = g_variant_get_uint32 (ctx_value);
          break;
        default:
          g_assert_not_reached ();
          break;
        }
      }
    }
  }

  if (app->priv->startup_context)
    ephy_application_free_startup_context (app);
  app->priv->startup_context = ctx;

  G_APPLICATION_CLASS (ephy_application_parent_class)->before_emit (application,
                                                                    platform_data);
}

static void
ephy_application_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  EphyApplication *application = EPHY_APPLICATION (object);

  switch (prop_id) {
  case PROP_PRIVATE_INSTANCE:
    application->priv->private_instance = g_value_get_boolean (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_application_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  EphyApplication *application = EPHY_APPLICATION (object);

  switch (prop_id) {
  case PROP_PRIVATE_INSTANCE:
    g_value_set_boolean (value, application->priv->private_instance);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_application_class_init (EphyApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS(class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->finalize = ephy_application_finalize;
  object_class->set_property = ephy_application_set_property;
  object_class->get_property = ephy_application_get_property;

  application_class->startup = ephy_application_startup;
  application_class->activate = ephy_application_activate;
  application_class->before_emit = ephy_application_before_emit;
  application_class->add_platform_data = ephy_application_add_platform_data;

  object_properties[PROP_PRIVATE_INSTANCE] =
    g_param_spec_boolean ("private-instance",
                          "Private instance",
                          "Whether this Epiphany instance is private.",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);

  g_type_class_add_private (class, sizeof (EphyApplicationPrivate));
}

static void
ephy_application_init (EphyApplication *application)
{
  application->priv = EPHY_APPLICATION_GET_PRIVATE(application);
  application->priv->startup_context = NULL;
}

static void
ephy_application_finalize (GObject *object)
{
  EphyApplication *application = EPHY_APPLICATION (object);

  if (application->priv->startup_context)
    ephy_application_free_startup_context (application);

  G_OBJECT_CLASS (ephy_application_parent_class)->finalize (object);
}

EphyApplication *
ephy_application_new (gboolean private_instance)
{
  GApplicationFlags flags = G_APPLICATION_FLAGS_NONE;

  if (private_instance)
    flags |= G_APPLICATION_NON_UNIQUE;

  return g_object_new (EPHY_TYPE_APPLICATION,
                       "application-id", "org.gnome.Epiphany",
                       "flags", flags,
                       "private-instance", private_instance,
                       NULL);
}

/**
 * ephy_application_set_startup_context:
 * @application: A #EphyApplication
 * @ctx: (transfer full): a #EphyApplicationStartupContext
 *
 * Sets the startup context to be used during activation of a new instance.
 * See ephy_application_set_startup_new().
 **/
void
ephy_application_set_startup_context (EphyApplication *application,
                                      EphyApplicationStartupContext *ctx)
{
  g_return_if_fail (EPHY_IS_APPLICATION (application));

  if (application->priv->startup_context)
    ephy_application_free_startup_context (application);

  application->priv->startup_context = ctx;
}
