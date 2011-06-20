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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef __EPHY_APPLICATION_H__
#define __EPHY_APPLICATION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_APPLICATION             (ephy_application_get_type ())
#define EPHY_APPLICATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj),EPHY_TYPE_APPLICATION, EphyApplication))
#define EPHY_APPLICATION_CONST(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_APPLICATION, EphyApplication const))
#define EPHY_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_APPLICATION, EphyApplicationClass))
#define EPHY_IS_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_APPLICATION))
#define EPHY_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_APPLICATION))
#define EPHY_APPLICATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_APPLICATION, EphyApplicationClass))

typedef struct _EphyApplication       EphyApplication;
typedef struct _EphyApplicationPrivate  EphyApplicationPrivate;
typedef struct _EphyApplicationClass  EphyApplicationClass;

extern EphyApplication *application;

struct _EphyApplication {
  GtkApplication parent;

  EphyApplicationPrivate *priv;
};

struct _EphyApplicationClass {
  GtkApplicationClass parent_class;
};

typedef enum {
  EPHY_STARTUP_NEW_TAB          = 1 << 0,
  EPHY_STARTUP_NEW_WINDOW       = 1 << 1,
  EPHY_STARTUP_BOOKMARKS_EDITOR = 1 << 2
} EphyStartupFlags;

typedef struct {
  EphyStartupFlags startup_flags;

  char *bookmarks_filename;
  char *session_filename;
  char *bookmark_url;

  char **arguments;

  guint32 user_time;
} EphyApplicationStartupContext;

GType      ephy_application_get_type (void) G_GNUC_CONST;

EphyApplication *ephy_application_new (void);

void             ephy_application_set_startup_context (EphyApplication *application,
                                                       EphyApplicationStartupContext *ctx);

EphyApplicationStartupContext *
ephy_application_startup_context_new (EphyStartupFlags startup_flags,
                                      char *bookmarks_filename,
                                      char *session_filename,
                                      char *bookmark_url,
                                      char **arguments,
                                      guint32 user_time);
G_END_DECLS

#endif
