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

#ifndef EPHY_EMBED_SHELL_H
#define EPHY_EMBED_SHELL_H

#include "ephy-embed.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "downloader-view.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct EphyEmbedShellClass EphyEmbedShellClass;

#define EPHY_EMBED_SHELL_TYPE             (ephy_embed_shell_get_type ())
#define EPHY_EMBED_SHELL(obj)             (GTK_CHECK_CAST ((obj), EPHY_EMBED_SHELL_TYPE, EphyEmbedShell))
#define EPHY_EMBED_SHELL_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_EMBED_SHELL_TYPE, EphyEmbedShellClass))
#define IS_EPHY_EMBED_SHELL(obj)          (GTK_CHECK_TYPE ((obj), EPHY_EMBED_SHELL_TYPE))
#define IS_EPHY_EMBED_SHELL_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_EMBED_SHELL))
#define EPHY_EMBED_SHELL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_EMBED_SHELL_TYPE, EphyEmbedShellClass))

typedef struct EphyEmbedShell EphyEmbedShell;
typedef struct EphyEmbedShellPrivate EphyEmbedShellPrivate;

extern EphyEmbedShell *embed_shell;

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
 * BlockedHost: a blocked host
 */
typedef struct
{
        gchar *type;
        gchar *domain;
} PermissionInfo;

/**
 * Cookie: the type of cookies
 */
typedef struct
{
        PermissionInfo base;
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

typedef struct
{
	const char *name;
	const char *title;
} CharsetInfo;

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

typedef enum
{
	DISK_CACHE = 2,
	MEMORY_CACHE = 1
} CacheType;

typedef enum
{
	CACHE_CLEAR_CAP = 1 << 0,
	OFFLINE_CAP = 1 << 1,
	PROXY_AUTOCONF_CAP = 1 << 2,
	JAVA_CONSOLE_CAP = 1 << 3,
	JS_CONSOLE_CAP = 1 << 4,
	CHARSETS_CAP = 1 << 5,
	PERMISSIONS_CAP = 1 << 6,
	COOKIES_CAP = 1 << 7,
	PASSWORDS_CAP = 1 << 8,
	FILEPICKER_CAP = 1 << 9
} EmbedShellCapabilities;

struct EphyEmbedShell
{
	GObject parent;
        EphyEmbedShellPrivate *priv;
};

struct EphyEmbedShellClass
{
        GObjectClass parent_class;

	void		(* new_window)          (EphyEmbedShell *shell,
					         EphyEmbed **new_embed,
						 EmbedChromeMask chromemask);

	/* Methods */

	void		(* get_capabilities)    (EphyEmbedShell *shell,
						 EmbedShellCapabilities *caps);
	EphyHistory   * (* get_global_history)  (EphyEmbedShell *shell);
	DownloaderView* (* get_downloader_view) (EphyEmbedShell *shell);
	gresult         (* clear_cache)         (EphyEmbedShell *shell,
						 CacheType type);
	gresult         (* set_offline_mode)    (EphyEmbedShell *shell,
						 gboolean offline);
	gresult         (* load_proxy_autoconf) (EphyEmbedShell *shell,
					         const char* url);
	gresult         (* show_java_console)   (EphyEmbedShell *shell);
	gresult         (* show_js_console)     (EphyEmbedShell *shell);
	gresult		(* get_charset_groups)  (EphyEmbedShell *shell,
						 GList **groups);
	gresult         (* get_charset_titles)  (EphyEmbedShell *shell,
						 const char *group,
						 GList **charsets);
	gresult		(* get_font_list)	(EphyEmbedShell *shell,
						 const char *langGroup,
						 const char *fontType,
						 GList **fontList,
						 char **default_font);
	gresult         (* set_permission)      (EphyEmbedShell *shell,
						 const char *url,
					         PermissionType type,
					         gboolean allow);
	gresult         (* list_permissions)    (EphyEmbedShell *shell,
						 PermissionType type,
						 GList **permissions);
	gresult         (* remove_permissions)  (EphyEmbedShell *shell,
						 PermissionType type,
						 GList *permissions);
	gresult         (* list_cookies)        (EphyEmbedShell *shell,
						 GList **cokies);
	gresult         (* remove_cookies)      (EphyEmbedShell *shell,
						 GList *cookies);
	gresult         (* list_passwords)      (EphyEmbedShell *shell,
						 PasswordType type,
						 GList **passwords);
	gresult         (* remove_passwords)    (EphyEmbedShell *shell,
						 GList *passwords,
						 PasswordType type);
	gresult         (* show_file_picker)    (EphyEmbedShell *shell,
						 GtkWidget *parentWidget,
						 const char* title,
						 const char* directory,
						 const char* file,
						 FilePickerMode mode,
						 char **ret_fullpath,
						 gboolean *ret_save_content,
						 FileFormat *file_formats,
						 gint *ret_file_format);
};

GType             ephy_embed_shell_get_type            (void);

EphyEmbedShell   *ephy_embed_shell_new                 (const char *type);

EphyFaviconCache *ephy_embed_shell_get_favicon_cache   (EphyEmbedShell *ges);

void              ephy_embed_shell_add_embed           (EphyEmbedShell *ges,
							EphyEmbed *embed);

void              ephy_embed_shell_remove_embed        (EphyEmbedShell *ges,
							EphyEmbed *embed);

EphyEmbed        *ephy_embed_shell_get_active_embed    (EphyEmbedShell *ges);

GList            *ephy_embed_shell_get_embeds          (EphyEmbedShell *ges);

const char      **ephy_embed_shell_get_supported       (void);

void              ephy_embed_shell_get_capabilities    (EphyEmbedShell *shell,
							EmbedShellCapabilities *caps);

EphyHistory      *ephy_embed_shell_get_global_history  (EphyEmbedShell *shell);

DownloaderView   *ephy_embed_shell_get_downloader_view (EphyEmbedShell *shell);

gresult           ephy_embed_shell_clear_cache         (EphyEmbedShell *shell,
							CacheType type);

gresult           ephy_embed_shell_set_offline_mode    (EphyEmbedShell *shell,
							gboolean offline);

gresult           ephy_embed_shell_load_proxy_autoconf (EphyEmbedShell *shell,
							const char* url);

/* Charsets */
gresult		  ephy_embed_shell_get_charset_groups  (EphyEmbedShell *shell,
							GList **groups);

gresult           ephy_embed_shell_get_charset_titles  (EphyEmbedShell *shell,
							const char *group,
							GList **charsets);

gresult           ephy_embed_shell_get_font_list       (EphyEmbedShell *shell,
							const char *langGroup,
							const char *fontType,
							GList **fontList,
							char **default_font);

/* Permissions */
gresult           ephy_embed_shell_set_permission      (EphyEmbedShell *shell,
							const char *url,
					                PermissionType type,
							gboolean allow);

gresult           ephy_embed_shell_list_permissions    (EphyEmbedShell *shell,
							PermissionType type,
							GList **permissions);

gresult		  ephy_embed_shell_free_permissions    (EphyEmbedShell *shell,
							GList *permissions);

gresult           ephy_embed_shell_remove_permissions  (EphyEmbedShell *shell,
							PermissionType type,
							GList *permissions);

/* Cookies */
gresult           ephy_embed_shell_list_cookies        (EphyEmbedShell *shell,
							GList **cookies);

gresult           ephy_embed_shell_remove_cookies      (EphyEmbedShell *shell,
							GList *cookies);

gresult		  ephy_embed_shell_free_cookies        (EphyEmbedShell *shell,
							GList *cookies);

/* Passwords */
gresult           ephy_embed_shell_list_passwords      (EphyEmbedShell *shell,
							PasswordType type,
							GList **passwords);

gresult		  ephy_embed_shell_free_passwords      (EphyEmbedShell *shell,
							GList *passwords);

gresult           ephy_embed_shell_remove_passwords    (EphyEmbedShell *shell,
							GList *passwords,
							PasswordType type);

gresult           ephy_embed_shell_show_file_picker    (EphyEmbedShell *shell,
							GtkWidget *parentWidget,
							const char *title,
							const char *directory,
							const char *file,
							FilePickerMode mode,
							char **ret_fullpath,
							gboolean *ret_save_content,
							FileFormat *file_formats,
							int *ret_file_format);

G_END_DECLS

#endif
