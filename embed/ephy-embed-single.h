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
 *
 *  $Id$
 */

#ifndef EPHY_EMBED_SINGLE_H
#define EPHY_EMBED_SINGLE_H

#include "ephy-embed.h"
#include "ephy-history.h"
#include "ephy-langs.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_SINGLE		(ephy_embed_single_get_type ())
#define EPHY_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingle))
#define EPHY_EMBED_SINGLE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleClass))
#define EPHY_IS_EMBED_SINGLE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_IS_EMBED_SINGLE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_EMBED_SINGLE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleClass))

typedef struct EphyEmbedSingleClass EphyEmbedSingleClass;
typedef struct EphyEmbedSingle EphyEmbedSingle;
typedef struct EphyEmbedSinglePrivate EphyEmbedSinglePrivate;

/**
 * FilePickerMode: What mode FilePicker should run in
 */

typedef enum
{
        modeOpen = 0,
        modeSave = 1,
        modeGetFolder =2
} FilePickerMode;

typedef struct
{
        /* description of the file format */
        gchar *description;
        /* tipical sufixes, NULL terminated */
        gchar **extensions;
} FileFormat;

/**
 * Cookie: the type of cookies
 */
typedef struct
{
	gchar *domain;
        gchar *name;
        gchar *value;
        gchar *path;
        gchar *secure;
        gchar *expire;
} CookieInfo;

/**
 * Password: a password manager entry
 */
typedef struct
{
        gchar *host;
        gchar *username;
} PasswordInfo;

/**
 * PasswordType: To distinguish actual passwords from blocked password sites
 */
typedef enum
{
        PASSWORD_PASSWORD,
        PASSWORD_REJECT
} PasswordType;

typedef enum
{
	COOKIES_PERMISSION,
	IMAGES_PERMISSION
} PermissionType;

struct EphyEmbedSingle
{
	GObject parent;
        EphyEmbedSinglePrivate *priv;
};

struct EphyEmbedSingleClass
{
        GObjectClass parent_class;

	/* Signals */
	void		(* new_window)          (EphyEmbedSingle *shell,
					         EphyEmbed **new_embed,
						 EmbedChromeMask chromemask);

	/* Methods */

	gresult         (* clear_cache)         (EphyEmbedSingle *shell);
	gresult         (* set_offline_mode)    (EphyEmbedSingle *shell,
						 gboolean offline);
	gresult         (* load_proxy_autoconf) (EphyEmbedSingle *shell,
					         const char* url);
	gresult         (* show_java_console)   (EphyEmbedSingle *shell);
	gresult         (* show_js_console)     (EphyEmbedSingle *shell);
	gresult		(* get_font_list)	(EphyEmbedSingle *shell,
						 const char *langGroup,
						 GList **fontList);
	gresult         (* list_cookies)        (EphyEmbedSingle *shell,
						 GList **cokies);
	gresult         (* remove_cookies)      (EphyEmbedSingle *shell,
						 GList *cookies);
	gresult         (* list_passwords)      (EphyEmbedSingle *shell,
						 PasswordType type,
						 GList **passwords);
	gresult         (* remove_passwords)    (EphyEmbedSingle *shell,
						 GList *passwords,
						 PasswordType type);
};

GType             ephy_embed_single_get_type            (void);

gresult           ephy_embed_single_clear_cache         (EphyEmbedSingle *shell);

gresult           ephy_embed_single_set_offline_mode    (EphyEmbedSingle *shell,
							 gboolean offline);

gresult           ephy_embed_single_load_proxy_autoconf (EphyEmbedSingle *shell,
							 const char* url);

gresult           ephy_embed_single_get_font_list       (EphyEmbedSingle *shell,
							 const char *langGroup,
							 GList **fontList);

/* Cookies */
gresult           ephy_embed_single_list_cookies        (EphyEmbedSingle *shell,
							 GList **cookies);

gresult           ephy_embed_single_remove_cookies      (EphyEmbedSingle *shell,
							 GList *cookies);

gresult		  ephy_embed_single_free_cookies        (EphyEmbedSingle *shell,
							 GList *cookies);

/* Passwords */
gresult           ephy_embed_single_list_passwords      (EphyEmbedSingle *shell,
							 PasswordType type,
							 GList **passwords);

gresult		  ephy_embed_single_free_passwords      (EphyEmbedSingle *shell,
							 GList *passwords);

gresult           ephy_embed_single_remove_passwords    (EphyEmbedSingle *shell,
							 GList *passwords,
							 PasswordType type);

G_END_DECLS

#endif
