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

#include "ephy-embed-popup.h"
#include "ephy-new-bookmark.h"

#include <bonobo/bonobo-ui-component.h>

void popup_cmd_new_window 	  (BonoboUIComponent *uic, 
			    	   EphyEmbedPopup *popup, 
			    	   const char* verbname);

void popup_cmd_new_tab	 	   (BonoboUIComponent *uic, 
			    	    EphyEmbedPopup *popup, 
			    	    const char* verbname);

void popup_cmd_image_in_new_tab    (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);

void popup_cmd_image_in_new_window (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);

void popup_cmd_add_bookmark    	   (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);

void popup_cmd_frame_in_new_tab    (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);

void popup_cmd_frame_in_new_window (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);

void popup_cmd_add_frame_bookmark  (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);

void popup_cmd_view_source  	   (BonoboUIComponent *uic, 
			            EphyEmbedPopup *popup, 
			            const char* verbname);
