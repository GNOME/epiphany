/* cellrenderer.h
 * Copyright (C) 2002 Naba Kumar <kh_naba@users.sourceforge.net>
 * modified by Jörgen Scheibengruber <mfcn@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef EPHY_CELL_RENDERER_PROGRESS_H
#define EPHY_CELL_RENDERER_PROGRESS_H

#include <gtk/gtkcellrenderer.h>

G_BEGIN_DECLS

#define EPHY_TYPE_CELL_RENDERER_PROGRESS (ephy_cell_renderer_progress_get_type ())
#define EPHY_CELL_RENDERER_PROGRESS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_CELL_RENDERER_PROGRESS, EphyCellRendererProgress))

typedef struct _EphyCellRendererProgress         EphyCellRendererProgress;
typedef struct _EphyCellRendererProgressClass    EphyCellRendererProgressClass;
typedef struct _EphyCellRendererProgressPrivate  EphyCellRendererProgressPrivate;

enum
{
	EPHY_PROGRESS_CELL_UNKNOWN = -1,
	EPHY_PROGRESS_CELL_FAILED = -2
};

struct _EphyCellRendererProgress
{
	GtkCellRenderer parent_instance;
	EphyCellRendererProgressPrivate *priv;
};

struct _EphyCellRendererProgressClass
{
	GtkCellRendererClass parent_class;
};

GtkType		 ephy_cell_renderer_progress_get_type (void);
GtkCellRenderer* ephy_cell_renderer_progress_new      (void);

G_END_DECLS

#endif
