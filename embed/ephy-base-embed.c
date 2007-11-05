/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2007 Christian Persch
 *  Copyright © 2007  Xan Lopez
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

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

#include "ephy-debug.h"
#include "ephy-embed.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-string.h"
#include "ephy-zoom.h"

#include "ephy-base-embed.h"

#define MAX_TITLE_LENGTH        512 /* characters */
#define RELOAD_DELAY            250 /* ms */
#define RELOAD_DELAY_MAX_TICKS  40  /* RELOAD_DELAY * RELOAD_DELAY_MAX_TICKS = 10 s */

struct _EphyBaseEmbedPrivate
{
  EphyEmbedAddressExpire address_expire;
  /* guint address_expire : 2; ? */
  EphyEmbedSecurityLevel security_level;
  /* guint security_level : 3; ? */
  EphyEmbedDocumentType document_type;
  EphyEmbedNavigationFlags nav_flags;
  float zoom;

  /* Flags */
  guint is_blank : 1;
  guint is_loading : 1;
  guint is_setting_zoom : 1;

  gint8 load_percent;
  char *address;
  char *typed_address;
  char *title;
  char *loading_title;
  int cur_requests;
  int total_requests;
  char *status_message;
  char *link_message;
  char *icon_address;
  GdkPixbuf *icon;

  /* File watch */
  GnomeVFSMonitorHandle *monitor;
  guint reload_scheduled_id;
  guint reload_delay_ticks;

  GSList *hidden_popups;
  GSList *shown_popups;
};

#if 0
typedef struct
{
  char *url;
  char *name;
  char *features;
} PopupInfo;
#endif

enum
  {
    PROP_0,
    PROP_ADDRESS,
    PROP_DOCUMENT_TYPE,
    PROP_HIDDEN_POPUP_COUNT,
    PROP_ICON,
    PROP_ICON_ADDRESS,
    PROP_LINK_MESSAGE,
    PROP_LOAD_PROGRESS,
    PROP_LOAD_STATUS,
    PROP_NAVIGATION,
    PROP_POPUPS_ALLOWED,
    PROP_SECURITY,
    PROP_STATUS_MESSAGE,
    PROP_TITLE,
    PROP_TYPED_ADDRESS,
    PROP_ZOOM
  };

#define EPHY_BASE_EMBED_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BASE_EMBED, EphyBaseEmbedPrivate))

static void ephy_base_embed_file_monitor_cancel (EphyBaseEmbed *embed);
static void ephy_base_embed_dispose (GObject *object);
static void ephy_base_embed_finalize (GObject *object);
static void ephy_embed_iface_init (EphyEmbedIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (EphyBaseEmbed, ephy_base_embed, GTK_TYPE_BIN,
                                  G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED,
                                                         ephy_embed_iface_init))

static void
ephy_base_embed_size_request (GtkWidget *widget,
                              GtkRequisition *requisition)
{
  GtkWidget *child;

  GTK_WIDGET_CLASS (ephy_base_embed_parent_class)->size_request (widget, requisition);

  child = GTK_BIN (widget)->child;

  if (child && GTK_WIDGET_VISIBLE (child))
    {
      GtkRequisition child_requisition;
      gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
    }
}

static void
ephy_base_embed_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
  GtkWidget *child;

  widget->allocation = *allocation;

  child = GTK_BIN (widget)->child;
  g_return_if_fail (child != NULL);

  gtk_widget_size_allocate (child, allocation);
}

static void
impl_set_typed_address (EphyEmbed *embed,
                        const char *address,
                        EphyEmbedAddressExpire expire)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (embed)->priv;

  g_free (priv->typed_address);
  priv->typed_address = g_strdup (address);

  if (expire == EPHY_EMBED_ADDRESS_EXPIRE_CURRENT &&
      !priv->is_loading)
    {
      priv->address_expire = EPHY_EMBED_ADDRESS_EXPIRE_NOW;
    }
  else
    {
      priv->address_expire = expire;
    }

  g_object_notify (G_OBJECT (embed), "typed-address");
}

static const char*
impl_get_typed_address (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->typed_address;
}

static const char*
impl_get_loading_title (EphyEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (embed)->priv;

  return priv->loading_title;
}

static gboolean
impl_get_is_blank (EphyEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (embed)->priv;

  return priv->is_blank;
}

static const char*
impl_get_icon_address (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->icon_address;
}

static GdkPixbuf*
impl_get_icon (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->icon;
}

static const char *
impl_get_title (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->title;
}

static EphyEmbedDocumentType
impl_get_document_type (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->document_type;
}

static int
impl_get_load_percent (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->load_percent;
}

static gboolean
impl_get_load_status (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->is_loading;
}

static EphyEmbedNavigationFlags
impl_get_navigation_flags (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->nav_flags;
}

static const char*
impl_get_address (EphyEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (embed)->priv;
  return priv->address ? priv->address : "about:blank";
}

static const char*
impl_get_status_message (EphyEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (embed)->priv;

  if (priv->link_message && priv->link_message[0] != '\0')
    {
      return priv->link_message;
    }
  else if (priv->status_message)
    {
      return priv->status_message;
    }
  else
    {
      return NULL;
    }
}

static const char*
impl_get_link_message (EphyEmbed *embed)
{
  return EPHY_BASE_EMBED (embed)->priv->link_message;
}

static void
ephy_base_embed_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_ICON_ADDRESS:
      ephy_base_embed_set_icon_address (EPHY_BASE_EMBED (object), g_value_get_string (value));
      break;
    case PROP_TYPED_ADDRESS:
      impl_set_typed_address (EPHY_EMBED (object), g_value_get_string (value),
                              EPHY_EMBED_ADDRESS_EXPIRE_NOW);
      break;
    case PROP_POPUPS_ALLOWED:
      //ephy_base_embed_set_popups_allowed (MOZILLA_EMBED (object), g_value_get_boolean (value));
      break;
    case PROP_ADDRESS:
    case PROP_TITLE:
    case PROP_DOCUMENT_TYPE:
    case PROP_HIDDEN_POPUP_COUNT:
    case PROP_ICON:
    case PROP_LOAD_PROGRESS:
    case PROP_LOAD_STATUS:
    case PROP_LINK_MESSAGE:
    case PROP_NAVIGATION:
    case PROP_SECURITY:
    case PROP_STATUS_MESSAGE:
    case PROP_ZOOM:
      /* read only */
      break;
    default:
      break;
    }
}

static void
ephy_base_embed_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (object)->priv;

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_string (value, priv->address);
      break;
    case PROP_DOCUMENT_TYPE:
      g_value_set_enum (value, priv->document_type);
      break;
    case PROP_HIDDEN_POPUP_COUNT:
      g_value_set_int (value, 0);
      //g_value_set_int (value, popup_blocker_n_hidden (embed));
      break;
    case PROP_ICON:
      g_value_set_object (value, priv->icon);
      break;
    case PROP_ICON_ADDRESS:
      g_value_set_string (value, priv->icon_address);
      break;
    case PROP_LINK_MESSAGE:
      g_value_set_string (value, priv->link_message);
      break;
    case PROP_LOAD_PROGRESS:
      g_value_set_int (value, priv->load_percent);
      break;
    case PROP_LOAD_STATUS:
      g_value_set_boolean (value, priv->is_loading);
      break;
    case PROP_NAVIGATION:
      g_value_set_flags (value, priv->nav_flags);
      break;
    case PROP_POPUPS_ALLOWED:
      g_value_set_boolean (value, FALSE);
      //g_value_set_boolean (value, mozilla_embed_get_popups_allowed (embed));
      break;
    case PROP_SECURITY:
      g_value_set_enum (value, priv->security_level);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, priv->status_message);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_TYPED_ADDRESS:
      g_value_set_string (value, priv->typed_address);
      break;
    case PROP_ZOOM:
      g_value_set_float (value, priv->zoom);
      break;
    default:
      break;
    }
}

static void
ephy_base_embed_class_init (EphyBaseEmbedClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  gobject_class->dispose = ephy_base_embed_dispose;
  gobject_class->finalize = ephy_base_embed_finalize;
  gobject_class->get_property = ephy_base_embed_get_property;
  gobject_class->set_property = ephy_base_embed_set_property;

  widget_class->size_request = ephy_base_embed_size_request;
  widget_class->size_allocate = ephy_base_embed_size_allocate;

  g_object_class_install_property (gobject_class,
                                   PROP_SECURITY,
                                   g_param_spec_enum ("security-level",
                                                      "Security Level",
                                                      "The embed's security level",
                                                      EPHY_TYPE_EMBED_SECURITY_LEVEL,
                                                      EPHY_EMBED_STATE_IS_UNKNOWN,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_DOCUMENT_TYPE,
                                   g_param_spec_enum ("document-type",
                                                      "Document Type",
                                                      "The embed's documen type",
                                                      EPHY_TYPE_EMBED_DOCUMENT_TYPE,
                                                      EPHY_EMBED_DOCUMENT_HTML,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_ZOOM,
                                   g_param_spec_float ("zoom",
                                                       "Zoom",
                                                       "The embed's zoom",
                                                       ZOOM_MINIMAL,
                                                       ZOOM_MAXIMAL,
                                                       1.0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LOAD_PROGRESS,
                                   g_param_spec_int ("load-progress",
                                                     "Load progress",
                                                     "The embed's load progress in percent",
                                                     0,
                                                     100,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LOAD_STATUS,
                                   g_param_spec_boolean ("load-status",
                                                         "Load status",
                                                         "The embed's load status",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_NAVIGATION,
                                   g_param_spec_flags ("navigation",
                                                       "Navigation flags",
                                                       "The embed's navigation flags",
                                                       EPHY_TYPE_EMBED_NAVIGATION_FLAGS,
                                                       0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        "Address",
                                                        "The embed's address",
                                                        "",
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_TYPED_ADDRESS,
                                   g_param_spec_string ("typed-address",
                                                        "Typed Address",
                                                        "The typed address",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The embed's title",
                                                        _("Blank page"),
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_STATUS_MESSAGE,
                                   g_param_spec_string ("status-message",
                                                        "Status Message",
                                                        "The embed's statusbar message",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LINK_MESSAGE,
                                   g_param_spec_string ("link-message",
                                                        "Link Message",
                                                        "The embed's link message",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        "Icon",
                                                        "The embed icon's",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_ADDRESS,
                                   g_param_spec_string ("icon-address",
                                                        "Icon address",
                                                        "The embed icon's address",
                                                        NULL,
                                                        (G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)));
  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN_POPUP_COUNT,
                                   g_param_spec_int ("hidden-popup-count",
                                                     "Number of Blocked Popups",
                                                     "The embed's number of blocked popup windows",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_POPUPS_ALLOWED,
                                   g_param_spec_boolean ("popups-allowed",
                                                         "Popups Allowed",
                                                         "Whether popup windows are to be displayed",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (EphyBaseEmbedPrivate));
}

static void
icon_cache_changed_cb (EphyFaviconCache *cache,
                       const char *address,
                       EphyBaseEmbed *embed)
{
  const char *icon_address;

  g_return_if_fail (address != NULL);

  icon_address = ephy_embed_get_icon_address (EPHY_EMBED (embed));

  /* is this for us? */
  if (icon_address != NULL &&
      strcmp (icon_address, address) == 0)
    {
      ephy_base_embed_load_icon (EPHY_BASE_EMBED (embed));
    }
}

static void
ge_document_type_cb (EphyEmbed *embed,
                     EphyEmbedDocumentType type,
                     EphyBaseEmbed *bembed)
{
  if (bembed->priv->document_type != type)
    {
      bembed->priv->document_type = type;

      g_object_notify (G_OBJECT (embed), "document-type");
    }
}

static gboolean
address_has_web_scheme (const char *address)
{
  gboolean has_web_scheme;

  if (address == NULL) return FALSE;

  has_web_scheme = (g_str_has_prefix (address, "http:") ||
                    g_str_has_prefix (address, "https:") ||
                    g_str_has_prefix (address, "ftp:") ||
                    g_str_has_prefix (address, "file:") ||
                    g_str_has_prefix (address, "data:") ||
                    g_str_has_prefix (address, "about:") ||
                    g_str_has_prefix (address, "gopher:"));

  return has_web_scheme;
}

static void
ge_zoom_change_cb (EphyEmbed *embed,
                   float zoom,
                   EphyBaseEmbed *bembed)
{
  char *address;

  if (bembed->priv->zoom != zoom)
    {
      if (bembed->priv->is_setting_zoom)
        {
          return;
        }

      address = ephy_embed_get_location (embed, TRUE);
      if (address_has_web_scheme (address))
        {
          EphyHistory *history;
          EphyNode *host;
          history = EPHY_HISTORY
            (ephy_embed_shell_get_global_history (embed_shell));
          host = ephy_history_get_host (history, address);

          if (host != NULL)
            {
              ephy_node_set_property_float (host,
                                            EPHY_NODE_HOST_PROP_ZOOM,
                                            zoom);
            }
        }

      g_free (address);

      bembed->priv->zoom = zoom;

      g_object_notify (G_OBJECT (embed), "zoom");
    }
}

static void
ge_favicon_cb (EphyEmbed *membed,
               const char *address,
               EphyBaseEmbed *bembed)
{
  ephy_base_embed_set_icon_address (bembed, address);
}

static void
ephy_base_embed_init (EphyBaseEmbed *self)
{
  EphyBaseEmbedPrivate *priv;
  EphyFaviconCache *cache;

  priv = self->priv = EPHY_BASE_EMBED_GET_PRIVATE (self);

  g_signal_connect_object (self, "ge_document_type",
                           G_CALLBACK (ge_document_type_cb),
                           self, (GConnectFlags) 0);

  g_signal_connect_object (self, "ge_zoom_change",
                           G_CALLBACK (ge_zoom_change_cb),
                           self, (GConnectFlags) 0);

  g_signal_connect_object (self, "ge_favicon",
                           G_CALLBACK (ge_favicon_cb),
                           self, (GConnectFlags)0);

  cache = EPHY_FAVICON_CACHE
    (ephy_embed_shell_get_favicon_cache (embed_shell));
  g_signal_connect_object (G_OBJECT (cache), "changed",
                           G_CALLBACK (icon_cache_changed_cb),
                           self, (GConnectFlags)0);

  priv->document_type = EPHY_EMBED_DOCUMENT_HTML;
  priv->security_level = EPHY_EMBED_STATE_IS_UNKNOWN;
  priv->zoom = 1.0;
  priv->address_expire = EPHY_EMBED_ADDRESS_EXPIRE_NOW;
  priv->is_blank = TRUE;
}

static void
ephy_base_embed_dispose (GObject *object)
{
  ephy_base_embed_file_monitor_cancel (EPHY_BASE_EMBED (object));

  G_OBJECT_CLASS (ephy_base_embed_parent_class)->dispose (object);
}

static void
ephy_base_embed_finalize (GObject *object)
{
  EphyBaseEmbedPrivate *priv = EPHY_BASE_EMBED (object)->priv;

  if (priv->icon != NULL)
    {
      g_object_unref (priv->icon);
      priv->icon = NULL;
    }

#if 0
  popups_manager_reset (embed);
#endif

  g_free (priv->icon_address);
  g_free (priv->status_message);
  g_free (priv->link_message);
  g_free (priv->address);
  g_free (priv->typed_address);
  g_free (priv->title);
  g_free (priv->loading_title);

  G_OBJECT_CLASS (ephy_base_embed_parent_class)->finalize (object);
}

static void
ephy_embed_iface_init (EphyEmbedIface *iface)
{
  iface->get_title = impl_get_title;
  iface->get_address = impl_get_address;
  iface->get_typed_address = impl_get_typed_address;
  iface->set_typed_address = impl_set_typed_address;
  iface->get_loading_title = impl_get_loading_title;
  iface->get_is_blank = impl_get_is_blank;
  iface->get_icon = impl_get_icon;
  iface->get_icon_address = impl_get_icon_address;
  iface->get_document_type = impl_get_document_type;
  iface->get_load_status = impl_get_load_status;
  iface->get_load_percent = impl_get_load_percent;
  iface->get_navigation_flags = impl_get_navigation_flags;
  iface->get_link_message = impl_get_link_message;
  iface->get_status_message = impl_get_status_message;
}

void
ephy_base_embed_set_address (EphyBaseEmbed *embed, char *address)
{
  EphyBaseEmbedPrivate *priv = embed->priv;
  GObject *object = G_OBJECT (embed);

  g_free (priv->address);
  priv->address = address;

  priv->is_blank = address == NULL ||
    strcmp (address, "about:blank") == 0;

  if (priv->is_loading &&
      priv->address_expire == EPHY_EMBED_ADDRESS_EXPIRE_NOW &&
      priv->typed_address != NULL)
    {
      g_free (priv->typed_address);
      priv->typed_address = NULL;

      g_object_notify (object, "typed-address");
    }

  g_object_notify (object, "address");
}

static char *
get_title_from_address (const char *address)
{
  GnomeVFSURI *uri;
  char *title;

  if (address == NULL) return NULL;

  uri = gnome_vfs_uri_new (address);
  if (uri == NULL) return g_strdup (address);

  title = gnome_vfs_uri_to_string (uri,
                                   (GnomeVFSURIHideOptions)
                                   (GNOME_VFS_URI_HIDE_USER_NAME |
                                    GNOME_VFS_URI_HIDE_PASSWORD |
                                    GNOME_VFS_URI_HIDE_HOST_PORT |
                                    GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD |
                                    GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER));
  gnome_vfs_uri_unref (uri);

  return title;
}

void
ephy_base_embed_set_title (EphyBaseEmbed *embed,
                           char *title)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  if (!priv->is_blank && (title == NULL || g_strstrip (title)[0] == '\0'))
    {
      g_free (title);
      title = get_title_from_address (priv->address);

      /* Fallback */
      if (title == NULL || title[0] == '\0')
        {
          g_free (title);
          title = NULL;
          priv->is_blank = TRUE;
        }
    }
  else if (priv->is_blank && title != NULL)
    {
      g_free (title);
      title = NULL;
    }

  g_free (priv->title);
  priv->title = ephy_string_shorten (title, MAX_TITLE_LENGTH);

  g_object_notify (G_OBJECT (embed), "title");
}

static void
ensure_page_info (EphyBaseEmbed *embed, const char *address)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  if ((priv->address == NULL || priv->address[0] == '\0') &&
      priv->address_expire == EPHY_EMBED_ADDRESS_EXPIRE_NOW)
    {
      ephy_base_embed_set_address (embed, g_strdup (address));
    }

  /* FIXME huh?? */
  if (priv->title == NULL || priv->title[0] == '\0')
    {
      ephy_base_embed_set_title (embed, NULL);
    }
}
static void
update_net_state_message (EphyBaseEmbed *embed, const char *uri, EphyEmbedNetState flags)
{
  GnomeVFSURI *vfs_uri = NULL;
  const char *msg = NULL;
  const char *host = NULL;

  if (uri != NULL)
    {
      vfs_uri = gnome_vfs_uri_new (uri);
    }

  if (vfs_uri != NULL)
    {
      host = gnome_vfs_uri_get_host_name (vfs_uri);
    }

  if (host == NULL || host[0] == '\0') goto out;

  /* IS_REQUEST and IS_NETWORK can be both set */
  if (flags & EPHY_EMBED_STATE_IS_REQUEST)
    {
      if (flags & EPHY_EMBED_STATE_REDIRECTING)
        {
          msg = _("Redirecting to “%s”…");
        }
      else if (flags & EPHY_EMBED_STATE_TRANSFERRING)
        {
          msg = _("Transferring data from “%s”…");
        }
      else if (flags & EPHY_EMBED_STATE_NEGOTIATING)
        {
          msg = _("Waiting for authorization from “%s”…");
        }
    }

  if (flags & EPHY_EMBED_STATE_IS_NETWORK)
    {
      if (flags & EPHY_EMBED_STATE_START)
        {
          msg = _("Loading “%s”…");
        }
    }

  if ((flags & EPHY_EMBED_STATE_IS_NETWORK) &&
      (flags & EPHY_EMBED_STATE_STOP))
    {
      g_free (embed->priv->status_message);
      embed->priv->status_message = NULL;
      g_object_notify (G_OBJECT (embed), "status-message");

    }
  else if (msg != NULL)
    {
      g_free (embed->priv->status_message);
      g_free (embed->priv->loading_title);
      embed->priv->status_message = g_strdup_printf (msg, host);
      embed->priv->loading_title = g_strdup_printf (msg, host);
      g_object_notify (G_OBJECT (embed), "status-message");
      g_object_notify (G_OBJECT (embed), "title");
    }

 out:
  if (vfs_uri != NULL)
    {
      gnome_vfs_uri_unref (vfs_uri);
    }
}

static void
update_navigation_flags (EphyBaseEmbed *membed)
{
  EphyBaseEmbedPrivate *priv = membed->priv;
  EphyEmbed *embed = EPHY_EMBED (membed);
  guint flags = 0;

  if (ephy_embed_can_go_up (embed))
    {
      flags |= EPHY_EMBED_NAV_UP;
    }

  if (ephy_embed_can_go_back (embed))
    {
      flags |= EPHY_EMBED_NAV_BACK;
    }

  if (ephy_embed_can_go_forward (embed))
    {
      flags |= EPHY_EMBED_NAV_FORWARD;
    }

  if (priv->nav_flags != (EphyEmbedNavigationFlags)flags)
    {
      priv->nav_flags = (EphyEmbedNavigationFlags)flags;

      g_object_notify (G_OBJECT (embed), "navigation");
    }
}

static int
build_load_percent (int requests_done, int requests_total)
{
  int percent= 0;

  if (requests_total > 0)
    {
      percent = (requests_done * 100) / requests_total;
      percent = CLAMP (percent, 0, 100);
    }

  return percent;
}

void
ephy_base_embed_set_load_percent (EphyBaseEmbed *embed, int percent)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  if (percent != priv->load_percent)
    {
      priv->load_percent = percent;

      g_object_notify (G_OBJECT (embed), "load-progress");
    }
}

static void
build_progress_from_requests (EphyBaseEmbed *embed, EphyEmbedNetState state)
{
  int load_percent;

  if (state & EPHY_EMBED_STATE_IS_REQUEST)
    {
      if (state & EPHY_EMBED_STATE_START)
        {
          embed->priv->total_requests ++;
        }
      else if (state & EPHY_EMBED_STATE_STOP)
        {
          embed->priv->cur_requests ++;
        }

      load_percent = build_load_percent (embed->priv->cur_requests,
                                         embed->priv->total_requests);

      ephy_base_embed_set_load_percent (embed, load_percent);
    }
}

static void
ephy_base_embed_set_load_status (EphyBaseEmbed *embed, gboolean status)
{
  EphyBaseEmbedPrivate *priv = embed->priv;
  guint is_loading;

  is_loading = status != FALSE;

  if (is_loading != priv->is_loading)
    {
      priv->is_loading = is_loading;

      g_object_notify (G_OBJECT (embed), "load-status");
    }
}

void
ephy_base_embed_update_from_net_state (EphyBaseEmbed *embed,
                                       const char *uri,
                                       EphyEmbedNetState state)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  update_net_state_message (embed, uri, state);

  if (state & EPHY_EMBED_STATE_IS_NETWORK)
    {
      if (state & EPHY_EMBED_STATE_START)
        {
          GObject *object = G_OBJECT (embed);

          g_object_freeze_notify (object);

          priv->total_requests = 0;
          priv->cur_requests = 0;

          ephy_base_embed_set_load_percent (embed, 0);
          ephy_base_embed_set_load_status (embed, TRUE);

          ensure_page_info (embed, uri);

          g_object_notify (object, "title");

          g_object_thaw_notify (object);
        }
      else if (state & EPHY_EMBED_STATE_STOP)
        {
          GObject *object = G_OBJECT (embed);

          g_object_freeze_notify (object);

          ephy_base_embed_set_load_percent (embed, 100);
          ephy_base_embed_set_load_status (embed, FALSE);

          g_free (priv->loading_title);
          priv->loading_title = NULL;

          priv->address_expire = EPHY_EMBED_ADDRESS_EXPIRE_NOW;

          g_object_notify (object, "title");

          g_object_thaw_notify (object);
        }

      update_navigation_flags (embed);
    }

  build_progress_from_requests (embed, state);
}

void
ephy_base_embed_set_loading_title (EphyBaseEmbed *embed,
                                   const char *title,
                                   gboolean is_address)
{
  EphyBaseEmbedPrivate *priv = embed->priv;
  char *freeme = NULL;

  g_free (priv->loading_title);
  priv->loading_title = NULL;

  if (is_address)
    {
      title = freeme = get_title_from_address (title);
    }

  if (title != NULL && title[0] != '\0')
    {
      /* translators: %s here is the address of the web page */
      priv->loading_title = g_strdup_printf (_("Loading “%s”…"), title);
    }
  else
    {
      priv->loading_title = g_strdup (_("Loading…"));
    }

  g_free (freeme);
}

static void
ephy_base_embed_file_monitor_cancel (EphyBaseEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  if (priv->monitor != NULL)
    {
      LOG ("Cancelling file monitor");

      gnome_vfs_monitor_cancel (priv->monitor);
      priv->monitor = NULL;
    }

  if (priv->reload_scheduled_id != 0)
    {
      LOG ("Cancelling scheduled reload");

      g_source_remove (priv->reload_scheduled_id);
      priv->reload_scheduled_id = 0;
    }

  priv->reload_delay_ticks = 0;
}

static gboolean
ephy_base_embed_file_monitor_reload_cb (EphyBaseEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  if (priv->reload_delay_ticks > 0)
    {
      priv->reload_delay_ticks--;

      /* Run again */
      return TRUE;
    }

  if (priv->is_loading)
    {
      /* Wait a bit to reload if we're still loading! */
      priv->reload_delay_ticks = RELOAD_DELAY_MAX_TICKS / 2;

      /* Run again */
      return TRUE;
    }

  priv->reload_scheduled_id = 0;

  LOG ("Reloading file '%s'", ephy_embed_get_address (EPHY_EMBED (embed)));

  ephy_embed_reload (EPHY_EMBED (embed), TRUE);

  /* don't run again */
  return FALSE;
}

static void
ephy_base_embed_file_monitor_cb (GnomeVFSMonitorHandle *handle,
                                 const gchar *monitor_uri,
                                 const gchar *info_uri,
                                 GnomeVFSMonitorEventType event_type,
                                 EphyBaseEmbed *embed)
{
  gboolean uri_is_directory;
  gboolean should_reload;
  char* local_path;
  EphyBaseEmbedPrivate *priv = embed->priv;

  LOG ("File '%s' has changed, scheduling reload", monitor_uri);

  local_path = gnome_vfs_get_local_path_from_uri(monitor_uri);
  uri_is_directory = g_file_test(local_path, G_FILE_TEST_IS_DIR);
  g_free(local_path);

  switch (event_type)
    {
      /* These events will always trigger a reload: */
    case GNOME_VFS_MONITOR_EVENT_CHANGED:
    case GNOME_VFS_MONITOR_EVENT_CREATED:
      should_reload = TRUE;
      break;

      /* These events will only trigger a reload for directories: */
    case GNOME_VFS_MONITOR_EVENT_DELETED:
    case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
      should_reload = uri_is_directory;
      break;

      /* These events don't trigger a reload: */
    case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
    case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
    default:
      should_reload = FALSE;
      break;
    }

  if (should_reload) {
    /* We make a lot of assumptions here, but basically we know
     * that we just have to reload, by construction.
     * Delay the reload a little bit so we don't endlessly
     * reload while a file is written.
     */
    if (priv->reload_delay_ticks == 0)
      {
        priv->reload_delay_ticks = 1;
      }
    else
      {
        /* Exponential backoff */
        priv->reload_delay_ticks = MIN (priv->reload_delay_ticks * 2,
                                        RELOAD_DELAY_MAX_TICKS);
      }

    if (priv->reload_scheduled_id == 0)
      {
        priv->reload_scheduled_id =
          g_timeout_add (RELOAD_DELAY,
                         (GSourceFunc) ephy_base_embed_file_monitor_reload_cb, embed);
      }
  }
}

static void
ephy_base_embed_update_file_monitor (EphyBaseEmbed *embed,
                                     const gchar *address)
{
  EphyBaseEmbedPrivate *priv = embed->priv;
  GnomeVFSMonitorHandle *handle = NULL;
  gboolean local;
  char* local_path;
  GnomeVFSMonitorType monitor_type;

  if (priv->monitor != NULL &&
      priv->address != NULL && address != NULL &&
      strcmp (priv->address, address) == 0)

    {
      /* same address, no change needed */
      return;
    }

  ephy_base_embed_file_monitor_cancel (embed);

  local = g_str_has_prefix (address, "file://");
  if (local == FALSE) return;

  local_path = gnome_vfs_get_local_path_from_uri(address);
  monitor_type = g_file_test(local_path, G_FILE_TEST_IS_DIR)
    ? GNOME_VFS_MONITOR_DIRECTORY
    : GNOME_VFS_MONITOR_FILE;
  g_free(local_path);

  if (gnome_vfs_monitor_add (&handle, address,
                             monitor_type,
                             (GnomeVFSMonitorCallback) ephy_base_embed_file_monitor_cb,
                             embed) == GNOME_VFS_OK)
    {
      LOG ("Installed monitor for file '%s'", address);

      priv->monitor = handle;
    }
}

void
ephy_base_embed_location_changed (EphyBaseEmbed *embed,
                                  char *location)
{
  GObject *object = G_OBJECT (embed);

  g_object_freeze_notify (object);

  /* do this up here so we still have the old address around */
  ephy_base_embed_update_file_monitor (embed, location);

  /* Do not expose about:blank to the user, an empty address
     bar will do better */
  if (location == NULL || location[0] == '\0' ||
      strcmp (location, "about:blank") == 0)
    {
      ephy_base_embed_set_address (embed, NULL);
      ephy_base_embed_set_title (embed, NULL);
    }
  else
    {
      char *embed_address;

      /* we do this to get rid of an eventual password in the URL */
      embed_address = ephy_embed_get_location (EPHY_EMBED (embed), TRUE);
      ephy_base_embed_set_address (embed, embed_address);
      ephy_base_embed_set_loading_title (embed, embed_address, TRUE);
    }

  ephy_base_embed_set_link_message (embed, NULL);
  ephy_base_embed_set_icon_address (embed, NULL);
  update_navigation_flags (embed);

  g_object_notify (object, "title");

  g_object_thaw_notify (object);
}

void
ephy_base_embed_set_link_message (EphyBaseEmbed *embed,
                                  char *link_message)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  g_free (priv->link_message);

  priv->link_message = ephy_embed_utils_link_message_parse (link_message);

  g_object_notify (G_OBJECT (embed), "status-message");
  g_object_notify (G_OBJECT (embed), "link-message");
}

void
ephy_base_embed_load_icon (EphyBaseEmbed *embed)
{
  EphyBaseEmbedPrivate *priv = embed->priv;
  EphyEmbedShell *shell;
  EphyFaviconCache *cache;

  if (priv->icon_address == NULL || priv->icon != NULL) return;

  shell = ephy_embed_shell_get_default ();
  cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache (shell));

  /* ephy_favicon_cache_get returns a reference already */
  priv->icon = ephy_favicon_cache_get (cache, priv->icon_address);

  g_object_notify (G_OBJECT (embed), "icon");
}

void
ephy_base_embed_set_icon_address (EphyBaseEmbed *embed,
                                  const char *address)
{
  GObject *object = G_OBJECT (embed);
  EphyBaseEmbedPrivate *priv = embed->priv;
  /*  EphyHistory *history;
      EphyBookmarks *eb;*/

  g_free (priv->icon_address);
  priv->icon_address = g_strdup (address);

  if (priv->icon != NULL)
    {
      g_object_unref (priv->icon);
      priv->icon = NULL;

      g_object_notify (object, "icon");
    }

  if (priv->icon_address)
    {
      /* FIXME: we need to put this somewhere inside src?/
         history = EPHY_HISTORY
         (ephy_embed_shell_get_global_history (embed_shell));
         ephy_history_set_icon (history, priv->address,
         priv->icon_address);

         eb = ephy_shell_get_bookmarks (ephy_shell);
         ephy_bookmarks_set_icon (eb, priv->address,
         priv->icon_address);*/

      ephy_base_embed_load_icon (embed);
    }

  g_object_notify (object, "icon-address");
}

void
ephy_base_embed_set_security_level (EphyBaseEmbed *embed,
                                    EphyEmbedSecurityLevel level)
{
  EphyBaseEmbedPrivate *priv = embed->priv;

  if (priv->security_level != level)
    {
      priv->security_level = level;

      g_object_notify (G_OBJECT (embed), "security-level");
    }
}

void
ephy_base_embed_restore_zoom_level (EphyBaseEmbed *membed,
                                    const char *address)
{
  EphyBaseEmbedPrivate *priv = membed->priv;

  /* restore zoom level */
  if (address_has_web_scheme (address))
    {
      EphyHistory *history;
      EphyNode *host;
      GValue value = { 0, };
      float zoom = 1.0, current_zoom;

      history = EPHY_HISTORY
        (ephy_embed_shell_get_global_history (embed_shell));
      host = ephy_history_get_host (history, address);

      if (host != NULL && ephy_node_get_property
          (host, EPHY_NODE_HOST_PROP_ZOOM, &value))
        {
          zoom = g_value_get_float (&value);
          g_value_unset (&value);
        }

      current_zoom = ephy_embed_get_zoom (EPHY_EMBED (membed));
      if (zoom != current_zoom)
        {
          priv->is_setting_zoom = TRUE;
          ephy_embed_set_zoom (EPHY_EMBED (membed), zoom);
          priv->is_setting_zoom = FALSE;
        }
    }
}

/* Popup stuff if zeroed for now */

#if 0
static void
ephy_tab_content_change_cb (EphyEmbed *embed, const char *address, EphyTab *tab)
{
  popups_manager_reset (tab);
  g_object_notify (G_OBJECT (tab), "popups-allowed");
}

static void
ephy_tab_new_window_cb (EphyEmbed *embed,
                        EphyEmbed *new_embed,
                        EphyTab *tab)
{
  EphyWindow *window;

  g_return_if_fail (new_embed != NULL);

  window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (new_embed)));
  g_return_if_fail (window != NULL);

  popups_manager_add_window (tab, window);
}

static void
ephy_tab_popup_blocked_cb (EphyEmbed *embed,
                           const char *url,
                           const char *name,
                           const char *features,
                           EphyTab *tab)
{
  popups_manager_add (tab, url, name, features);
}

static void
popups_manager_free_info (PopupInfo *popup)
{
  g_free (popup->url);
  g_free (popup->name);
  g_free (popup->features);
  g_free (popup);
}

static void
popups_manager_add (MozillaEmbed *embed,
                    const char *url,
                    const char *name,
                    const char *features)
{
  MozillaEmbedPrivate *priv = embed->priv;
  PopupInfo *popup;

  LOG ("popups_manager_add: embed %p, url %s, features %s",
       embed, url, features);

  popup = g_new0 (PopupInfo, 1);

  popup->url = (url == NULL) ? NULL : g_strdup (url);
  popup->name = g_strdup (name);
  popup->features = g_strdup (features);

  priv->hidden_popups = g_slist_prepend (priv->hidden_popups, popup);

  if (popup_blocker_n_hidden (embed) > MAX_HIDDEN_POPUPS) /* bug #160863 */
    {
      /* Remove the oldest popup */
      GSList *l = embed->priv->hidden_popups;

      while (l->next->next != NULL)
        {
          l = l->next;
        }

      popup = (PopupInfo *) l->next->data;
      popups_manager_free_info (popup);

      l->next = NULL;
    }
  else
    {
      g_object_notify (G_OBJECT (embed), "hidden-popup-count");
    }
}

static gboolean
popups_manager_remove_window (MozillaEmbed *embed,
                              EphyWindow *window)
{
  embed->priv->shown_popups = g_slist_remove (embed->priv->shown_popups,
                                              window);

  return FALSE;
}

static void
disconnect_popup (EphyWindow *window,
                  MozillaEmbed *embed)
{
  g_signal_handlers_disconnect_by_func
    (window, G_CALLBACK (popups_manager_remove_window), embed);
}

static void
popups_manager_add_window (MozillaEmbed *embed,
                           EphyWindow *window)
{
  LOG ("popups_manager_add_window: embed %p, window %p", embed, window);

  embed->priv->shown_popups = g_slist_prepend
    (embed->priv->shown_popups, window);

  g_signal_connect_swapped (window, "destroy",
                            G_CALLBACK (popups_manager_remove_window),
                            embed);
}

static gboolean
mozilla_embed_get_popups_allowed (MozillaEmbed *embed)
{
  EphyPermissionManager *permission_manager;
  EphyPermission response;
  EphyEmbed *embed;
  char *location;
  gboolean allow;

  permission_manager = EPHY_PERMISSION_MANAGER
    (ephy_embed_shell_get_embed_single (embed_shell));
  g_return_val_if_fail (EPHY_IS_PERMISSION_MANAGER (permission_manager),
                        FALSE);

  location = ephy_embed_get_location (embed, TRUE);
  if (location == NULL) return FALSE; /* FALSE, TRUE… same thing */

  response = ephy_permission_manager_test_permission
    (permission_manager, location, EPT_POPUP);

  switch (response)
    {
    case EPHY_PERMISSION_ALLOWED:
      allow = TRUE;
      break;
    case EPHY_PERMISSION_DENIED:
      allow = FALSE;
      break;
    case EPHY_PERMISSION_DEFAULT:
    default:
      allow = eel_gconf_get_boolean
        (CONF_SECURITY_ALLOW_POPUPS);
      break;
    }

  g_free (location);

  LOG ("mozilla_embed_get_popups_allowed: embed %p, allowed: %d", embed, allow);

  return allow;
}

static void
popups_manager_show (PopupInfo *popup,
                     MozillaEmbed *embed)
{
  EphyEmbed *embed;
  EphyEmbedSingle *single;

  /* Only show popup with non NULL url */
  if (popup->url != NULL)
    {
      embed = ephy_embed_get_embed (embed);

      single = EPHY_EMBED_SINGLE
        (ephy_embed_shell_get_embed_single (embed_shell));

      ephy_embed_single_open_window (single, embed, popup->url,
                                     popup->name, popup->features);
    }
  popups_manager_free_info (popup);
}

static void
popups_manager_show_all (MozillaEmbed *embed)
{
  LOG ("popup_blocker_show_all: embed %p", embed);

  g_slist_foreach (embed->priv->hidden_popups,
                   (GFunc) popups_manager_show, embed);
  g_slist_free (embed->priv->hidden_popups);
  embed->priv->hidden_popups = NULL;

  g_object_notify (G_OBJECT (embed), "hidden-popup-count");
}

static char *
popups_manager_new_window_info (EphyWindow *window)
{
  MozillaEmbed *embed;
  EphyEmbedChrome chrome;
  gboolean is_popup;
  char *features;

  g_object_get (window, "chrome", &chrome, "is-popup", &is_popup, NULL);
  g_return_val_if_fail (is_popup, g_strdup (""));

  embed = ephy_window_get_active_embed (window);
  g_return_val_if_fail (embed != NULL, g_strdup (""));

  features = g_strdup_printf
    ("width=%d,height=%d,menubar=%d,status=%d,toolbar=%d",
     embed->priv->width, embed->priv->height,
     (chrome & EPHY_EMBED_CHROME_MENUBAR) > 0,
     (chrome & EPHY_EMBED_CHROME_STATUSBAR) > 0,
     (chrome & EPHY_EMBED_CHROME_TOOLBAR) > 0);

  return features;
}

static void
popups_manager_hide (EphyWindow *window,
                     MozillaEmbed *parent_embed)
{
  EphyEmbed *embed;
  char *location;
  char *features;

  embed = ephy_window_get_active_embed (window);
  g_return_if_fail (EPHY_IS_EMBED (embed));

  location = ephy_embed_get_location (embed, TRUE);
  if (location == NULL) return;

  features = popups_manager_new_window_info (window);

  popups_manager_add (parent_embed, location, "" /* FIXME? maybe _blank? */, features);

  gtk_widget_destroy (GTK_WIDGET (window));

  g_free (location);
  g_free (features);
}

static void
popups_manager_hide_all (MozillaEmbed *embed)
{
  LOG ("popup_blocker_hide_all: embed %p", embed);

  g_slist_foreach (embed->priv->shown_popups,
                   (GFunc) popups_manager_hide, embed);
  g_slist_free (embed->priv->shown_popups);
  embed->priv->shown_popups = NULL;
}

static void
mozilla_embed_set_popups_allowed (MozillaEmbed *embed,
                                  gboolean allowed)
{
  char *location;
  EphyEmbed *embed;
  EphyPermissionManager *manager;
  EphyPermission permission;

  embed = ephy_embed_get_embed (embed);

  location = ephy_embed_get_location (embed, TRUE);
  g_return_if_fail (location != NULL);

  manager = EPHY_PERMISSION_MANAGER
    (ephy_embed_shell_get_embed_single (embed_shell));
  g_return_if_fail (EPHY_IS_PERMISSION_MANAGER (manager));

  permission = allowed ? EPHY_PERMISSION_ALLOWED
    : EPHY_PERMISSION_DENIED;

  ephy_permission_manager_add_permission (manager, location, EPT_POPUP, permission);

  if (allowed)
    {
      popups_manager_show_all (embed);
    }
  else
    {
      popups_manager_hide_all (embed);
    }

  g_free (location);
}

static guint
popup_blocker_n_hidden (MozillaEmbed *embed)
{
  return g_slist_length (embed->priv->hidden_popups);
}

static void
popups_manager_reset (MozillaEmbed *embed)
{
  g_slist_foreach (embed->priv->hidden_popups,
                   (GFunc) popups_manager_free_info, NULL);
  g_slist_free (embed->priv->hidden_popups);
  embed->priv->hidden_popups = NULL;

  g_slist_foreach (embed->priv->shown_popups,
                   (GFunc) disconnect_popup, embed);
  g_slist_free (embed->priv->shown_popups);
  embed->priv->shown_popups = NULL;

  g_object_notify (G_OBJECT (embed), "hidden-popup-count");
}
#endif
