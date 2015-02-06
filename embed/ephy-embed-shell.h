/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_EMBED_SHELL_H
#define EPHY_EMBED_SHELL_H

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_SHELL   (ephy_embed_shell_get_type ())
#define EPHY_EMBED_SHELL(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_SHELL, EphyEmbedShell))
#define EPHY_EMBED_SHELL_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellClass))
#define EPHY_IS_EMBED_SHELL(o)    (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_SHELL))
#define EPHY_IS_EMBED_SHELL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_SHELL))
#define EPHY_EMBED_SHELL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellClass))

typedef struct _EphyEmbedShellClass EphyEmbedShellClass;
typedef struct _EphyEmbedShell    EphyEmbedShell;
typedef struct _EphyEmbedShellPrivate EphyEmbedShellPrivate;

typedef enum
{
  EPHY_EMBED_SHELL_MODE_BROWSER,
  EPHY_EMBED_SHELL_MODE_STANDALONE,
  EPHY_EMBED_SHELL_MODE_PRIVATE,
  EPHY_EMBED_SHELL_MODE_INCOGNITO,
  EPHY_EMBED_SHELL_MODE_APPLICATION,
  EPHY_EMBED_SHELL_MODE_TEST,
  EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER
} EphyEmbedShellMode;

#define EPHY_EMBED_SHELL_MODE_HAS_PRIVATE_PROFILE(mode) \
  (mode == EPHY_EMBED_SHELL_MODE_PRIVATE || mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)

struct _EphyEmbedShell
{
  GtkApplication parent;

  /*< private >*/
  EphyEmbedShellPrivate *priv;
};

struct _EphyEmbedShellClass
{
  GtkApplicationClass parent_class;

  void    (* prepare_close)    (EphyEmbedShell *shell);

  void    (* restored_window)  (EphyEmbedShell *shell);
};

GType              ephy_embed_shell_get_type                   (void);
EphyEmbedShell    *ephy_embed_shell_get_default                (void);
WebKitWebContext  *ephy_embed_shell_get_web_context            (EphyEmbedShell   *shell);
GObject           *ephy_embed_shell_get_global_history_service (EphyEmbedShell   *shell);
GObject           *ephy_embed_shell_get_encodings              (EphyEmbedShell   *shell);
void               ephy_embed_shell_prepare_close              (EphyEmbedShell   *shell);
void               ephy_embed_shell_restored_window            (EphyEmbedShell   *shell);
void               ephy_embed_shell_set_page_setup             (EphyEmbedShell   *shell,
                                                                GtkPageSetup     *page_setup);
GtkPageSetup      *ephy_embed_shell_get_page_setup             (EphyEmbedShell   *shell);
void               ephy_embed_shell_set_print_settings         (EphyEmbedShell   *shell,
                                                                GtkPrintSettings *settings);
GtkPrintSettings  *ephy_embed_shell_get_print_settings         (EphyEmbedShell   *shell);
EphyEmbedShellMode ephy_embed_shell_get_mode                   (EphyEmbedShell   *shell);
gboolean           ephy_embed_shell_launch_handler             (EphyEmbedShell   *shell,
                                                                GFile            *file,
                                                                const char       *mime_type,
                                                                guint32           user_time);
void               ephy_embed_shell_clear_cache                (EphyEmbedShell   *shell);
void               ephy_embed_shell_set_thumbanil_path         (EphyEmbedShell   *shell,
                                                                const char       *url,
                                                                time_t            mtime,
                                                                const char       *path);
WebKitUserContentManager *ephy_embed_shell_get_user_content_manager (EphyEmbedShell *shell);

G_END_DECLS

#endif /* !EPHY_EMBED_SHELL_H */
