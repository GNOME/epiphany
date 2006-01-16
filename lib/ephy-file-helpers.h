/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#ifndef EPHY_FILE_HELPERS_H
#define EPHY_FILE_HELPERS_H

#include <glib.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-ops.h>

G_BEGIN_DECLS

typedef enum
{
	EPHY_MIME_PERMISSION_SAFE	= 1,
	EPHY_MIME_PERMISSION_UNSAFE	= 2,
	EPHY_MIME_PERMISSION_UNKNOWN	= 3
} EphyMimePermission;

typedef struct _EphyFileMonitor EphyFileMonitor;
typedef void (* EphyFileMonitorFunc) (EphyFileMonitor*, const char*, gpointer);
typedef gboolean (* EphyFileMonitorDelayFunc) (EphyFileMonitor*, gpointer);

const char *ephy_file                    (const char *filename);

const char *ephy_dot_dir                 (void);

void        ephy_file_helpers_init       (void);

void        ephy_file_helpers_shutdown   (void);

char       *ephy_file_downloads_dir      (void);

char	   *ephy_file_get_downloads_dir	 (void);

char       *ephy_file_desktop_dir	 (void);

const char *ephy_file_tmp_dir	 	 (void);

char       *ephy_file_tmp_filename	 (const char *base,
					  const char *extension);

gboolean    ephy_ensure_dir_exists       (const char *dir);

GSList     *ephy_file_find               (const char *path,
				          const char *fname,
				          gint maxdepth);

gboolean    ephy_file_switch_temp_file   (const char *filename,
					  const char *filename_temp);

void	    ephy_file_delete_on_exit	 (const char *path);

EphyMimePermission ephy_file_check_mime	 (const char *mime_type);

gboolean    ephy_file_launch_desktop_file (const char *filename,
					   guint32 user_time);

gboolean    ephy_file_launch_application (GnomeVFSMimeApplication *application,
					  const char *parameter,
					  guint32 user_time);

gboolean    ephy_file_launch_handler	 (const char *mime_type,
					  const char *address,
					  guint32 user_time);

EphyFileMonitor *ephy_file_monitor_add	 (const char *uri,
					  GnomeVFSMonitorType monitor_type,
					  guint delay,
					  EphyFileMonitorFunc callback,
					  EphyFileMonitorDelayFunc delay_func,
					  gpointer user_data);

void	   ephy_file_monitor_cancel	 (EphyFileMonitor *monitor);

G_END_DECLS

#endif /* EPHY_FILE_HELPERS_H */
