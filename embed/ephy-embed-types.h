/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#ifndef GALEON_EMBED_TYPES_H
#define GALEON_EMBED_TYPES_H

#include "ephy-types.h"

G_BEGIN_DECLS

typedef enum
{
	EMBED_CHROME_NONE = 0,
	EMBED_CHROME_DEFAULT = 1 << 0,
        EMBED_CHROME_MENUBARON = 1 << 1,
        EMBED_CHROME_TOOLBARON = 1 << 2,
	EMBED_CHROME_PERSONALTOOLBARON = 1 << 3,
        EMBED_CHROME_STATUSBARON = 1 << 4,
        EMBED_CHROME_WINDOWRAISED = 1 << 5,
        EMBED_CHROME_WINDOWLOWERED = 1 << 6,
        EMBED_CHROME_CENTERSCREEN = 1 << 7,
	EMBED_CHROME_OPENASDIALOG = 1 << 8,
	EMBED_CHROME_OPENASCHROME = 1 << 9,
	EMBED_CHROME_OPENASPOPUP = 1 << 10,
	EMBED_CHROME_OPENASFULLSCREEN = 1 << 11,
	EMBED_CHROME_PPVIEWTOOLBARON = 1 << 12,
	EMBED_CHROME_SIDEBARON = 1 << 13
} EmbedChromeMask;

G_END_DECLS

#endif
