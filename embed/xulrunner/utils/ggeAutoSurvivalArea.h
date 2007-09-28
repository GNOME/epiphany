/* 
 *  Copyright © 2007 Christian Persch
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GGE_AUTOSURVIVALAREA_H
#define GGE_AUTOSURVIVALAREA_H

#include <nsCOMPtr.h>

class nsIAppStartup;

/**
 * ggeAutoSurvivalArea:
 *
 * A helper class that has to be used whenever a window being open
 * should prevent the application from shutting down. (Only needed for
 * non-DOM windows.)
 * Use on the stack or on the heap.
 */

class ggeAutoSurvivalArea {
  public:
    ggeAutoSurvivalArea ();
    ~ggeAutoSurvivalArea ();
	
  private:
    nsCOMPtr<nsIAppStartup> mAppStartup;
};

#endif /* !GGE_AUTOSURVIVALAREA_H */
