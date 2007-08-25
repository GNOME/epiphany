/* ***** BEGIN LICENSE BLOCK *****
 * Copyright © 2006 Xan Lopez <xan@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * $Id$
 * ***** END LICENSE BLOCK ***** */

#ifndef __gecko_init_internal_h
#define __gecko_init_internal_h

#include <gtk/gtk.h>
#include "nsIDirectoryService.h"

extern gboolean gecko_init_with_params (const char *, const char*, const char*, nsIDirectoryServiceProvider*);

#endif
