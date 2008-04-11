/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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
 *  $Id$
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_EXTENSION_H
#define EPHY_EXTENSION_H

#include "ephy-window.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EXTENSION		(ephy_extension_get_type ())
#define EPHY_EXTENSION(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_EXTENSION, EphyExtension))
#define EPHY_EXTENSION_IFACE(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_EXTENSION, EphyExtensionIface))
#define EPHY_IS_EXTENSION(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_EXTENSION))
#define EPHY_IS_EXTENSION_IFACE(class)	(G_TYPE_CHECK_CLASS_TYPE ((class), EPHY_TYPE_EXTENSION))
#define EPHY_EXTENSION_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_EXTENSION, EphyExtensionIface))

typedef struct _EphyExtension		EphyExtension;
typedef struct _EphyExtensionIface	EphyExtensionIface;
	
struct _EphyExtensionIface
{
	GTypeInterface base_iface;

	void	(* attach_window)	(EphyExtension *extension,
					 EphyWindow *window);
	void	(* detach_window)	(EphyExtension *extension,
					 EphyWindow *window);
	void	(* attach_tab)		(EphyExtension *extension,
					 EphyWindow *window,
					 EphyEmbed *embed);
	void	(* detach_tab)		(EphyExtension *extension,
					 EphyWindow *window,
					 EphyEmbed *embed);
};

GType	ephy_extension_get_type		(void);

void	ephy_extension_attach_window	(EphyExtension *extension,
					 EphyWindow *window);

void	ephy_extension_detach_window	(EphyExtension *extension,
					 EphyWindow *window);

void	ephy_extension_attach_tab	(EphyExtension *extension,
					 EphyWindow *window,
					 EphyEmbed *embed);

void	ephy_extension_detach_tab	(EphyExtension *extension,
					 EphyWindow *window,
					 EphyEmbed *embed);

G_END_DECLS

#endif
