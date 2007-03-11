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

#ifndef AUTO_JSCONTEXTSTACK_H
#define AUTO_JSCONTEXTSTACK_H

struct JSContext;

#include <nsCOMPtr.h>
#include <nsIJSContextStack.h>

class AutoJSContextStack
{
	public:
		AutoJSContextStack () { }
		~AutoJSContextStack ();
	
		nsresult Init ();
	
	private:
		nsCOMPtr<nsIJSContextStack> mStack;
};

#endif
