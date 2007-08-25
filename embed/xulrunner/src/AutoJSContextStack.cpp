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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include <mozilla-config.h>
#include "config.h"

#include "AutoJSContextStack.h"

#include <nsIServiceManager.h>
#include <nsServiceManagerUtils.h>

AutoJSContextStack::~AutoJSContextStack()
{
  if (mStack) {
    JSContext* cx;
    mStack->Pop (&cx);

    NS_ASSERTION(cx == nsnull, "We pushed a null context but popped a non-null context!?");
  }
}

nsresult
AutoJSContextStack::Init()
{
  nsresult rv;
  mStack = do_GetService ("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED (rv))
    return rv;

  return mStack->Push (nsnull);
}
