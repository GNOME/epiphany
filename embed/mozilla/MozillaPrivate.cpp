/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
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

#include "mozilla-config.h"
#include "config.h"

#define MOZILLA_INTERNAL_API 1
#include <nsString.h>

#include "ephy-debug.h"

#include "MozillaPrivate.h"

/* IMPORTANT. Put only code that use internal mozilla strings (nsAutoString for
 * example) in this file. Note that you cannot use embed strings here,
 * the header inclusions will conflict.
 */
