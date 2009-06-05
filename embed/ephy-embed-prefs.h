/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*  Copyright © 2008 Xan Lopez <xan@gnome.org>
 *  Copyright © 2009 Igalia S.L.
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
 */

#ifndef __EPHY_EMBED_PREFS_H__
#define __EPHY_EMBED_PREFS_H__

#include "ephy-embed.h"

#define CONF_NETWORK_CACHE_SIZE "/apps/epiphany/web/cache_size"
#define CONF_RENDERING_FONT "/apps/epiphany/web/font"
#define CONF_RENDERING_FONT_MIN_SIZE "/apps/epiphany/web/minimum_font_size"
#define CONF_RENDERING_LANGUAGE "/apps/epiphany/web/language"
#define CONF_RENDERING_USE_OWN_COLORS "/apps/epiphany/web/use_own_colors"
#define CONF_RENDERING_USE_OWN_FONTS "/apps/epiphany/web/use_own_fonts"
#define CONF_USER_CSS_ENABLED "/apps/epiphany/web/user_css_enabled"
#define CONF_SECURITY_ALLOW_POPUPS "/apps/epiphany/web/allow_popups"
#define CONF_SECURITY_JAVA_ENABLED "/apps/epiphany/web/java_enabled"
#define CONF_SECURITY_JAVASCRIPT_ENABLED "/apps/epiphany/web/javascript_enabled"
#define CONF_SECURITY_COOKIES_ACCEPT "/apps/epiphany/web/cookie_accept"
#define CONF_LANGUAGE_AUTODETECT_ENCODING "/apps/epiphany/web/autodetect_encoding"
#define CONF_LANGUAGE_DEFAULT_ENCODING "/apps/epiphany/web/default_encoding"
#define CONF_BROWSE_WITH_CARET "/apps/epiphany/web/browse_with_caret"
#define CONF_IMAGE_ANIMATION_MODE "/apps/epiphany/web/image_animation"
#define CONF_IMAGE_LOADING_MODE "/apps/epiphany/web/image_loading"
#define CONF_DISPLAY_SMOOTHSCROLL "/apps/epiphany/web/smooth_scroll"
#define CONF_WEB_INSPECTOR_ENABLED "/apps/epiphany/web/inspector_enabled"
#define CONF_CARET_BROWSING_ENABLED "/apps/epiphany/web/browse_with_caret"

/* These are defined gnome wide now */
#define CONF_NETWORK_PROXY_MODE "/system/proxy/mode"
#define CONF_NETWORK_HTTP_PROXY "/system/http_proxy/host"
#define CONF_NETWORK_SSL_PROXY "/system/proxy/secure_host"
#define CONF_NETWORK_FTP_PROXY "/system/proxy/ftp_host"
#define CONF_NETWORK_SOCKS_PROXY "/system/proxy/socks_host"
#define CONF_NETWORK_HTTP_PROXY_PORT "/system/http_proxy/port"
#define CONF_NETWORK_SSL_PROXY_PORT "/system/proxy/secure_port"
#define CONF_NETWORK_FTP_PROXY_PORT "/system/proxy/ftp_port"
#define CONF_NETWORK_SOCKS_PROXY_PORT "/system/proxy/socks_port"
#define CONF_NETWORK_PROXY_AUTO_URL "/system/proxy/autoconfig_url"
#define CONF_NETWORK_PROXY_IGNORE_HOSTS "/system/http_proxy/ignore_hosts"
#define CONF_DESKTOP_FONT_VAR_SIZE "/desktop/gnome/interface/font_name"
#define CONF_DESKTOP_FONT_FIXED_SIZE "/desktop/gnome/interface/monospace_font_name"

/* DEPRECATED, we migrate them */
#define CONF_RENDERING_FONT_VAR_SIZE_OLD "/apps/epiphany/web/font_var_size"
#define CONF_RENDERING_FONT_FIXED_SIZE_OLD "/apps/epiphany/web/font_fixed_size"
#define CONF_RENDERING_FONT_MIN_SIZE_OLD "/apps/epiphany/web/font_min_size"
#define CONF_RENDERING_FONT_TYPE_OLD "/apps/epiphany/web/default_font_type"

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#define USER_STYLESHEET_FILENAME	"user-stylesheet.css"

G_BEGIN_DECLS

void ephy_embed_prefs_init         (void);
void ephy_embed_prefs_shutdown     (void);
void ephy_embed_prefs_add_embed    (EphyEmbed *embed);

G_END_DECLS

#endif /* __EPHY_EMBED_PREFS_H__ */
