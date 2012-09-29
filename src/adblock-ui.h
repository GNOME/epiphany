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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ADBLOCK_UI_H
#define ADBLOCK_UI_H

#include "ephy-dialog.h"
#include "uri-tester.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define TYPE_ADBLOCK_UI         (adblock_ui_get_type ())
#define ADBLOCK_UI(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), TYPE_ADBLOCK_UI, AdblockUI))
#define ADBLOCK_UI_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TYPE_ADBLOCK_UI, AdblockUIClass))
#define IS_ADBLOCK_UI(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), TYPE_ADBLOCK_UI))
#define IS_ADBLOCK_UI_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), TYPE_ADBLOCK_UI))
#define ADBLOCK_UI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), TYPE_ADBLOCK_UI, AdblockUIClass))

typedef struct _AdblockUI        AdblockUI;
typedef struct _AdblockUIClass   AdblockUIClass;
typedef struct _AdblockUIPrivate AdblockUIPrivate;

struct _AdblockUI
{
  EphyDialog parent;

  /*< private >*/
  AdblockUIPrivate *priv;
};

struct _AdblockUIClass
{
  EphyDialogClass parent_class;
};

GType      adblock_ui_get_type (void);

void       adblock_ui_register (GTypeModule *module);

AdblockUI *adblock_ui_new      (UriTester *tester);

G_END_DECLS

#endif
