/*
 *  Copyright (C) 2000, 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persc
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
 *
 *  $Id$
 */

#ifndef EPHY_LANGS_H
#define EPHY_LANGS_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
	char *title;
	char *code;
} EphyFontsLanguageInfo;

const EphyFontsLanguageInfo *ephy_font_languages	 (void);

guint			     ephy_font_n_languages	 (void);

void			     ephy_langs_append_languages (GArray *array);

void			     ephy_langs_sanitise	 (GArray *array);

char			   **ephy_langs_get_languages	 (void);

G_END_DECLS

#endif
