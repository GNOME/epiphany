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

#include "ephy-window.h"
#include "ephy-embed-utils.h"

#include <bonobo/bonobo-ui-component.h>

void window_cmd_edit_find	(BonoboUIComponent *uic, 
			    	 EphyWindow *window, 
			    	 const char* verbname);

void window_cmd_file_print 	(BonoboUIComponent *uic, 
			   	 EphyWindow *window, 
			   	 const char* verbname);

void window_cmd_go_stop 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_back 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_forward 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_go	 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_up 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_home 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_myportal 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_location 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_go_reload 	(BonoboUIComponent *uic, 
			 	 EphyWindow *window, 
				 const char* verbname);

void window_cmd_new	 	(BonoboUIComponent *uic, 
			    	 EphyWindow *window, 
			    	 const char* verbname);

void window_cmd_new_window 	(BonoboUIComponent *uic, 
			    	 EphyWindow *window, 
			    	 const char* verbname);

void window_cmd_new_tab 	(BonoboUIComponent *uic, 
			    	 EphyWindow *window, 
			    	 const char* verbname);

void window_cmd_bookmarks_add_default 	(BonoboUIComponent *uic, 
					 EphyWindow *window, 
					 const char* verbname);

void window_cmd_bookmarks_edit 	(BonoboUIComponent *uic, 
			    	 EphyWindow *window, 
			    	 const char* verbname);

void window_cmd_file_open 	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_file_save_as 	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_file_send_to 	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_file_close_tab 	  (BonoboUIComponent *uic, 
		      		   EphyWindow *window, 
		      		   const char* verbname);

void window_cmd_file_close_window (BonoboUIComponent *uic, 
		      		   EphyWindow *window, 
		      		   const char* verbname);

void window_cmd_edit_cut 	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_edit_copy 	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_edit_paste 	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_edit_select_all (BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_edit_find_next	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_edit_find_prev	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_view_zoom_in	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_view_zoom_out	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_view_zoom_normal(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_view_page_source(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_tools_history   (BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_tools_pdm	(BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_edit_prefs      (BonoboUIComponent *uic, 
		      		 EphyWindow *window, 
		      		 const char* verbname);

void 
window_cmd_settings_toolbar_editor (BonoboUIComponent *uic, 
				    EphyWindow *window, 
		      		 const char* verbname);

void window_cmd_help_about      (BonoboUIComponent *uic, 
				 EphyWindow *window, 
				 const char* verbname);

void window_cmd_set_charset     (BonoboUIComponent *uic, 
				 EncodingMenuData *data, 
				 const char* verbname);

void window_cmd_tabs_next       (BonoboUIComponent *uic, 
				 EphyWindow *window, 
				 const char* verbname);

void window_cmd_tabs_previous   (BonoboUIComponent *uic, 
				 EphyWindow *window, 
				 const char* verbname);

void window_cmd_tabs_move_left  (BonoboUIComponent *uic, 
				 EphyWindow *window, 
				 const char* verbname);

void window_cmd_tabs_move_right (BonoboUIComponent *uic, 
				 EphyWindow *window, 
				 const char* verbname);

void window_cmd_tabs_detach     (BonoboUIComponent *uic, 
				 EphyWindow *window, 
				 const char* verbname);

void window_cmd_help_manual     (BonoboUIComponent *uic, 
			         char *filename, 
			         const char* verbname);

