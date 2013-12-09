/*
 * Copyright (c) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _EPHY_SEARCH_PROVIDER_H
#define _EPHY_SEARCH_PROVIDER_H

#include "ephy-shell-search-provider-generated.h"

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_PROVIDER (ephy_search_provider_get_type ())

#define EPHY_SEARCH_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_SEARCH_PROVIDER, EphySearchProvider))
#define EPHY_SEARCH_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_SEARCH_PROVIDER, EphySearchProviderClass))
#define EPHY_IS_SEARCH_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_SEARCH_PROVIDER))
#define EPHY_IS_SEARCH_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_SEARCH_PROVIDER))
#define EPHY_SEARCH_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_SEARCH_PROVIDER, EphySearchProviderClass))

typedef struct _EphySearchProvider EphySearchProvider;
typedef struct _EphySearchProviderClass EphySearchProviderClass;

GType ephy_search_provider_get_type (void) G_GNUC_CONST;

EphySearchProvider *ephy_search_provider_new (void);

G_END_DECLS

#endif /* _EPHY_SEARCH_PROVIDER_H */
