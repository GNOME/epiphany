/* 
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef AUTO_WINDOWMODALSTATE_H
#define AUTO_WINDOWMODALSTATE_H

#include <nsCOMPtr.h>
#include <nsPIDOMWindow.h>

class AutoWindowModalState
{
  public:
    AutoWindowModalState (nsIDOMWindow *);
    ~AutoWindowModalState ();
	
  private:

  nsCOMPtr<nsPIDOMWindow> mWindow;
};

#endif
