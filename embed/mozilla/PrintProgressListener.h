/*
 *  Copyright (C) 2002 Philip Langdale
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

#ifndef __PrintProgressListener_h_
#define __PrintProgressListener_h_

#include "nsIWebProgressListener.h"

class GPrintListener : public nsIWebProgressListener
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIWEBPROGRESSLISTENER

	GPrintListener();
	GPrintListener(char *filename);
        virtual ~GPrintListener();

private:
	char *mFilename;
};

#endif //__PrintProgressListener_h_
