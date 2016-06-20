/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ephy-debug.h"
#include "ephy-gui.h"
#include "ephy-sync-utils.h"
#include "ephy-sync-window.h"

#include <gtk/gtk.h>
#include <string.h>

struct _EphySyncWindow {
  GtkDialog parent_instance;

  EphySyncService *sync_service;
  GCancellable *cancellable;

  GtkWidget *entry_email;
  GtkWidget *entry_password;
  GtkButton *btn_submit;

  GActionGroup *action_group;
};

G_DEFINE_TYPE (EphySyncWindow, ephy_sync_window, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_SYNC_SERVICE,
  PROP_LAST
};

static GParamSpec *obj_properties[PROP_LAST];

static void
submit_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  const gchar *emailUTF8;
  const gchar *passwordUTF8;
  guint8 *authPW;
  guint8 *unwrapBKey;
  guint8 *sessionToken;
  guint8 *keyFetchToken;
  EphySyncWindow *self = EPHY_SYNC_WINDOW (user_data);

  emailUTF8 = gtk_entry_get_text (GTK_ENTRY (self->entry_email));
LOG ("email: %s", emailUTF8);
  passwordUTF8 = gtk_entry_get_text (GTK_ENTRY (self->entry_password));
LOG ("password: %s", passwordUTF8);

  /* Only for easy testing */
  if (!strlen (emailUTF8) && !strlen (passwordUTF8)) {
    emailUTF8 = g_strdup ("andré@example.org");
    passwordUTF8 = g_strdup ("pässwörd");
  }

  authPW = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  unwrapBKey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  ephy_sync_service_stretch (self->sync_service,
                             emailUTF8,
                             passwordUTF8,
                             authPW,
                             unwrapBKey);
ephy_sync_utils_display_hex ("authPW", authPW, EPHY_SYNC_TOKEN_LENGTH);
ephy_sync_utils_display_hex ("unwrapBKey", unwrapBKey, EPHY_SYNC_TOKEN_LENGTH);

  sessionToken = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  keyFetchToken = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  ephy_sync_service_try_login (self->sync_service,
                               FALSE,
                               emailUTF8,
                               authPW,
                               sessionToken,
                               keyFetchToken);
ephy_sync_utils_display_hex ("sessionToken", sessionToken, EPHY_SYNC_TOKEN_LENGTH);
ephy_sync_utils_display_hex ("keyFetchToken", keyFetchToken, EPHY_SYNC_TOKEN_LENGTH);

  g_free (authPW);
  g_free (unwrapBKey);
  g_free (sessionToken);
  g_free (keyFetchToken);
}

static void
set_sync_service (EphySyncWindow  *self,
                  EphySyncService *sync_service)
{
  if (sync_service == self->sync_service)
    return;

  if (self->sync_service != NULL) {
    // TODO: Disconnect signal handlers, if any
    g_clear_object (&self->sync_service);
  }

  if (sync_service != NULL) {
    self->sync_service = g_object_ref (sync_service);
    // TODO: Connect signal handlers, if any
  }
}

static void
ephy_sync_window_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphySyncWindow *self = EPHY_SYNC_WINDOW (object);

  switch (prop_id) {
    case PROP_SYNC_SERVICE:
      set_sync_service (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_sync_window_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphySyncWindow *self = EPHY_SYNC_WINDOW (object);

  switch (prop_id) {
    case PROP_SYNC_SERVICE:
      g_value_set_object (value, self->sync_service);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GActionGroup *
create_action_group (EphySyncWindow *self)
{
  GSimpleActionGroup *group;

  const GActionEntry entries[] = {
    { "submit_action", submit_action }
  };

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);

  return G_ACTION_GROUP (group);
}

static void
ephy_sync_window_class_init (EphySyncWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

LOG ("%s:%d", __func__, __LINE__);

  object_class->set_property = ephy_sync_window_set_property;
  object_class->get_property = ephy_sync_window_get_property;
  // TODO: Set dispose method

  obj_properties[PROP_SYNC_SERVICE] =
    g_param_spec_object ("sync-service",
                         "Sync service",
                         "Sync Service",
                         EPHY_TYPE_SYNC_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/sync-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySyncWindow, entry_email);
  gtk_widget_class_bind_template_child (widget_class, EphySyncWindow, entry_password);
  gtk_widget_class_bind_template_child (widget_class, EphySyncWindow, btn_submit);
}

static void
ephy_sync_window_init (EphySyncWindow *self)
{
LOG ("%s:%d", __func__, __LINE__);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  ephy_gui_ensure_window_group (GTK_WINDOW (self));

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "sync", self->action_group);
}

GtkWidget *
ephy_sync_window_new (EphySyncService *sync_service)
{
  EphySyncWindow *self;

LOG ("%s:%d", __func__, __LINE__);

  self = g_object_new (EPHY_TYPE_SYNC_WINDOW,
                       "use-header-bar", TRUE,
                       "sync-service", sync_service,
                       NULL);

  return GTK_WIDGET (self);
}
