/*
 *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Some parts of this file based on the previous 'adblock' extension,
 *  licensed with the GNU General Public License 2 and later versions,
 *  Copyright (C) 2003 Marco Pesenti Gritti, Christian Persch.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "ephy-adblock-extension.h"

#include "adblock-ui.h"
#include "ephy-adblock.h"
#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-extension.h"
#include "ephy-file-helpers.h"
#include "ephy-window.h"
#include "uri-tester.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define EPHY_ADBLOCK_EXTENSION_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ADBLOCK_EXTENSION, EphyAdblockExtensionPrivate))

#define WINDOW_DATA_KEY     "EphyAdblockExtensionWindowData"
#define STATUSBAR_EVBOX_KEY "EphyAdblockExtensionStatusbarEvbox"
#define EXTENSION_KEY       "EphyAdblockExtension"
#define AD_BLOCK_ICON_NAME  "ad-blocked"

typedef struct
{
  EphyAdblockExtension *extension;
  EphyWindow           *window;

  GtkActionGroup       *action_group;
  guint                 ui_id;
} WindowData;

struct EphyAdblockExtensionPrivate
{
  UriTester *tester;
  AdblockUI *ui;
};

static void ephy_adblock_extension_iface_init (EphyExtensionIface *iface);
static void ephy_adblock_adblock_iface_init (EphyAdBlockIface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyAdblockExtension,
                         ephy_adblock_extension,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_ADBLOCK,
                                                ephy_adblock_adblock_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EXTENSION,
                                                ephy_adblock_extension_iface_init))

/* Private functions. */

static void
ephy_adblock_extension_init (EphyAdblockExtension *extension)
{
  LOG ("EphyAdblockExtension initialising");

  extension->priv = EPHY_ADBLOCK_EXTENSION_GET_PRIVATE (extension);
  extension->priv->tester = uri_tester_new ();
}

static void
ephy_adblock_extension_dispose (GObject *object)
{
  EphyAdblockExtension *extension = NULL;

  LOG ("EphyAdblockExtension disposing");

  extension = EPHY_ADBLOCK_EXTENSION (object);
  g_clear_object (&extension->priv->ui);
  g_clear_object (&extension->priv->tester);

  G_OBJECT_CLASS (ephy_adblock_extension_parent_class)->dispose (object);
}

static void
ephy_adblock_extension_finalize (GObject *object)
{
  LOG ("EphyAdblockExtension finalising");

  G_OBJECT_CLASS (ephy_adblock_extension_parent_class)->finalize (object);
}

static void
ephy_adblock_extension_class_init (EphyAdblockExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_adblock_extension_dispose;
  object_class->finalize = ephy_adblock_extension_finalize;

  g_type_class_add_private (object_class, sizeof (EphyAdblockExtensionPrivate));
}

static gboolean
ephy_adblock_impl_should_load (EphyAdBlock *blocker,
                               EphyEmbed *embed,
                               const char *url,
                               AdUriCheckType type)
{
  EphyAdblockExtension *self = NULL;
  EphyWebView* web_view = NULL;
  const char *address = NULL;

  LOG ("ephy_adblock_impl_should_load checking %s", url);

  self = EPHY_ADBLOCK_EXTENSION (blocker);
  g_return_val_if_fail (self != NULL, TRUE);

  web_view = ephy_embed_get_web_view (embed);
  address = ephy_web_view_get_address (web_view);

  return !uri_tester_test_uri (self->priv->tester, url, address, type);
}

static void
ephy_adblock_impl_edit_rule (EphyAdBlock *blocker,
                             const char *url,
                             gboolean allowed)
{
  EphyAdblockExtension *self = NULL;
  EphyAdblockExtensionPrivate *priv = NULL;

  LOG ("ephy_adblock_impl_edit_rule %s with state %d", url, allowed);

  self = EPHY_ADBLOCK_EXTENSION (blocker);
  priv = self->priv;

  if (priv->ui == NULL)
    {
      AdblockUI **ui;

      /*
       * TODO: url and allowed should be passed to the UI,
       * so the user can actually do something with it.
       */
      priv->ui = adblock_ui_new (priv->tester);
      ui = &priv->ui;

      g_object_add_weak_pointer ((gpointer)priv->ui,
                                 (gpointer *) ui);

      ephy_dialog_set_parent (EPHY_DIALOG (priv->ui), NULL);
    }

  ephy_dialog_show (EPHY_DIALOG (priv->ui));
}

static void
ephy_adblock_adblock_iface_init (EphyAdBlockIface *iface)
{
  iface->should_load = ephy_adblock_impl_should_load;
  iface->edit_rule = ephy_adblock_impl_edit_rule;
}

static void
ephy_adblock_extension_edit_cb (GtkAction *action, EphyWindow *window)
{
  WindowData *data = NULL;
  EphyAdblockExtensionPrivate *priv = NULL;

  data = g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
  g_return_if_fail (data != NULL);

  priv = data->extension->priv;

  if (priv->ui == NULL)
    {
      AdblockUI **ui;

      priv->ui = adblock_ui_new (priv->tester);
      ui = &priv->ui;

      g_object_add_weak_pointer ((gpointer)priv->ui,
                                 (gpointer *) ui);
    }

  ephy_dialog_set_parent (EPHY_DIALOG (priv->ui), GTK_WIDGET (window));
  ephy_dialog_show (EPHY_DIALOG (priv->ui));
}

static const GtkActionEntry edit_entries[] = {
  { "EphyAdblockExtensionEdit", NULL,
    N_("Ad Blocker"), NULL,
    N_("Configure Ad Blocker filters"),
    G_CALLBACK (ephy_adblock_extension_edit_cb) }
};

static void
impl_attach_window (EphyExtension *ext,
                    EphyWindow *window)
{
  WindowData *data = NULL;
  GtkUIManager *manager = NULL;

  /* Add adblock editor's menu entry. */
  data = g_new (WindowData, 1);
  g_object_set_data_full (G_OBJECT (window),
                          WINDOW_DATA_KEY,
                          data,
                          g_free);

  data->extension = EPHY_ADBLOCK_EXTENSION (ext);
  data->window = window;

  data->action_group = gtk_action_group_new ("EphyAdblockExtension");
  gtk_action_group_set_translation_domain (data->action_group,
                                           GETTEXT_PACKAGE);
  gtk_action_group_add_actions (data->action_group, edit_entries,
                                G_N_ELEMENTS(edit_entries), window);

  manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));

  gtk_ui_manager_insert_action_group (manager, data->action_group, -1);

  /* UI manager references the new action group. */
  g_object_unref (data->action_group);

  data->ui_id = gtk_ui_manager_new_merge_id (manager);

  gtk_ui_manager_add_ui (manager,
                         data->ui_id,
                         "/ui/PagePopup/ExtensionsMenu",
                         "EphyAdblockExtensionEdit",
                         "EphyAdblockExtensionEdit",
                         GTK_UI_MANAGER_MENUITEM,
                         FALSE);

  /* Remember the xtension attached to that window. */
  g_object_set_data (G_OBJECT (window), EXTENSION_KEY, ext);
}

static void
impl_detach_window (EphyExtension *ext,
                    EphyWindow *window)
{
  WindowData *data = NULL;
  GtkUIManager *manager = NULL;

  /* Remove editor UI. */
  data = g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
  g_assert (data != NULL);

  manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));

  gtk_ui_manager_remove_ui (manager, data->ui_id);
  gtk_ui_manager_remove_action_group (manager, data->action_group);

  g_object_set_data (G_OBJECT (window), WINDOW_DATA_KEY, NULL);
}

static void
impl_attach_tab (EphyExtension *ext,
                 EphyWindow *window,
                 EphyEmbed *embed)
{

}

static void
impl_detach_tab (EphyExtension *ext,
                 EphyWindow *window,
                 EphyEmbed *embed)
{

}

static void
ephy_adblock_extension_iface_init (EphyExtensionIface *iface)
{
  iface->attach_window = impl_attach_window;
  iface->detach_window = impl_detach_window;
  iface->attach_tab = impl_attach_tab;
  iface->detach_tab = impl_detach_tab;
}

