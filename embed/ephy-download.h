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

#ifndef EPHY_DOWNLOAD_H
#define EPHY_DOWNLOAD_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOAD		(ephy_download_get_type ())
#define EPHY_DOWNLOAD(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_DOWNLOAD, EphyDownload))
#define EPHY_DOWNLOAD_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_DOWNLOAD, EphyDownloadClass))
#define EPHY_IS_DOWNLOAD(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_DOWNLOAD))
#define EPHY_IS_DOWNLOAD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_DOWNLOAD))
#define EPHY_DOWNLOAD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_DOWNLOAD, EphyDownloadClass))

typedef struct EphyDownload EphyDownload;
typedef struct EphyDownloadClass EphyDownloadClass;
typedef struct EphyDownloadPrivate EphyDownloadPrivate;

typedef enum
{
	EPHY_DOWNLOAD_DOWNLOADING,
	EPHY_DOWNLOAD_PAUSED,
	EPHY_DOWNLOAD_COMPLETED,
	EPHY_DOWNLOAD_FAILED
} EphyDownloadState;

struct EphyDownload
{
        GObject parent;
};

struct EphyDownloadClass
{
        GObjectClass parent_class;

	char * 		  (* get_source)           (EphyDownload *download);
	char * 		  (* get_target)           (EphyDownload *download);
	int    		  (* get_percent)          (EphyDownload *download);
	long   		  (* get_current_progress) (EphyDownload *download);
	long   		  (* get_total_progress)   (EphyDownload *download);
	long   		  (* get_elapsed_time)	   (EphyDownload *download);
	void   		  (* cancel)               (EphyDownload *download);
	void   		  (* pause)                (EphyDownload *download);
	void   		  (* resume)               (EphyDownload *download);
	EphyDownloadState (* get_state)	           (EphyDownload *download);

	/* Signals */
	void              (* changed)              (EphyDownload *download);
};

/* Time is expressed in seconds, file sizes in bytes */

GType              ephy_download_get_type             (void);

EphyDownload      *ephy_download_new                  (void);

char	          *ephy_download_get_name	      (EphyDownload *download);

char	          *ephy_download_get_source	      (EphyDownload *download);

char              *ephy_download_get_target           (EphyDownload *download);

int                ephy_download_get_percent          (EphyDownload *download);

EphyDownloadState  ephy_download_get_state	      (EphyDownload *download);	   

long               ephy_download_get_current_progress (EphyDownload *download);

long               ephy_download_get_total_progress   (EphyDownload *download);

long		   ephy_download_get_elapsed_time     (EphyDownload *download);

long		   ephy_download_get_remaining_time   (EphyDownload *download);

void		   ephy_download_cancel	      	      (EphyDownload *download);

void		   ephy_download_pause		      (EphyDownload *download);

void		   ephy_download_resume		      (EphyDownload *download);

G_END_DECLS

#endif
